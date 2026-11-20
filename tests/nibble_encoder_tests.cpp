#include "nib.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename Transform>
void check_transform(const std::string& name, const std::vector<uint8_t>& input) {
    Transform transform;
    std::vector<uint8_t> encoded = input;
    std::vector<uint8_t> metadata = transform.apply(encoded);
    transform.reverse(encoded, metadata);
    require(encoded == input, name + " transform failed to round-trip");
}

template<typename Backend>
void check_backend(const std::string& name, const std::vector<uint8_t>& input) {
    Backend backend;
    std::vector<uint8_t> encoded = backend.compress(input);
    std::vector<uint8_t> decoded = backend.decompress(encoded);
    if (decoded != input) {
        size_t mismatch = 0;
        while (mismatch < decoded.size() && mismatch < input.size() &&
               decoded[mismatch] == input[mismatch]) ++mismatch;
        const std::string expected = mismatch < input.size()
            ? std::to_string(input[mismatch])
            : "missing";
        throw std::runtime_error(name + " backend failed to round-trip at " +
            std::to_string(mismatch) + " (input " + std::to_string(input.size()) +
            ", output " + std::to_string(decoded.size()) + ", expected " + expected +
            ", got " +
            (mismatch < decoded.size() ? std::to_string(decoded[mismatch]) : "missing") + ")");
    }
}

struct ArchiveSizes {
    size_t payload_bytes;
    size_t framing_bytes;
    size_t transform_metadata_bytes;
};

uint32_t read_u32(const std::string& archive, size_t& pos) {
    require(pos <= archive.size() && archive.size() - pos >= 4,
            "Truncated archive while measuring framing");
    uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        value |= static_cast<uint32_t>(
            static_cast<unsigned char>(archive[pos++])) << shift;
    }
    return value;
}

ArchiveSizes measure_archive(const std::string& archive) {
    require(archive.size() >= 14 && archive.compare(0, 4, "NIB2") == 0,
            "Invalid archive while measuring framing");
    size_t pos = 6;
    const size_t transform_count = static_cast<unsigned char>(archive[5]);
    size_t metadata_bytes = 0;
    for (size_t i = 0; i < transform_count; ++i) {
        require(pos < archive.size(), "Truncated archive transform list");
        ++pos;
        const uint32_t metadata_size = read_u32(archive, pos);
        require(metadata_size <= archive.size() - pos,
                "Truncated archive transform metadata");
        metadata_bytes += metadata_size;
        pos += metadata_size;
    }
    (void)read_u32(archive, pos);
    const uint32_t payload_bytes = read_u32(archive, pos);
    require(payload_bytes == archive.size() - pos,
            "Payload length does not match archive framing");
    require(pos == 14 + transform_count * 5 + metadata_bytes,
            "Fixed archive header and transform framing were counted incorrectly");
    return {payload_bytes, pos, metadata_bytes};
}

void test_pipeline_chains(const std::string& dictionary, const std::string& input) {
    using Encoder = std::unique_ptr<nibble::NibbleEncoder<char>>;
    std::vector<std::pair<std::string, Encoder>> chains;
    chains.emplace_back("english", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_english_text_pipeline().build());
    chains.emplace_back("source", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_source_code_pipeline().build());
    chains.emplace_back("log", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_log_file_pipeline().build());
    chains.emplace_back("structured", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_structured_pipeline().build());
    chains.emplace_back("repeating", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_repeating_pipeline().build());
    chains.emplace_back("binary", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_binary_pipeline().build());
    chains.emplace_back("aggressive", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_aggressive_pipeline().build());
    chains.emplace_back("fast", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_fast_pipeline().build());
    chains.emplace_back("balanced", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_balanced_pipeline().build());
    chains.emplace_back("adaptive", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary).with_adaptive_pipeline().build());
    chains.emplace_back("custom-rle", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary)
        .add_xor_mean_normalize().add_mtf().add_rle().with_rle_backend().build());
    chains.emplace_back("custom-huffman", nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary)
        .add_xor_mean_normalize().add_freq_sort().with_huffman_backend().build());

    std::string repeated;
    for (size_t i = 0; i < 3; ++i) repeated += input;
    const std::vector<std::pair<std::string, std::string>> chain_inputs{
        {"mixed-language", input},
        {"short-diacritics", "¡Hola! Ça va? Grüße aus München, olá coração."},
        {"repeated-mixed", repeated}
    };

    std::cout << "\nPipeline chain coverage (mixed, short, and repeated cases)\n";
    for (auto& chain : chains) {
        std::string mixed_archive;
        for (const auto& test_case : chain_inputs) {
            const std::string archive = chain.second->encode(test_case.second);
            require(chain.second->decode(archive) == test_case.second,
                    chain.first + " pipeline failed " + test_case.first + " round-trip");
            const ArchiveSizes sizes = measure_archive(archive);
            require(sizes.framing_bytes + sizes.payload_bytes == archive.size(),
                    chain.first + " pipeline framing was miscounted");
            if (test_case.first == "mixed-language") mixed_archive = archive;
        }
        std::cout << "  " << std::setw(12) << std::left << chain.first
                  << std::right << " input " << input.size() << " B, archive "
                  << mixed_archive.size() << " B ("
                  << std::fixed << std::setprecision(1)
                  << static_cast<double>(mixed_archive.size()) / input.size() * 100.0
                  << "% of input)\n";
    }
}

void test_transforms() {
    const std::vector<uint8_t> empty;
    check_transform<nibble::XORMeanNormalizeTransform<char>>("empty XOR mean", empty);
    check_transform<nibble::BWTTransform<char>>("empty BWT", empty);
    check_transform<nibble::MTFTransform<char>>("empty MTF", empty);
    check_transform<nibble::RLETransform<char>>("empty RLE", empty);
    check_transform<nibble::FreqSortTransform<char>>("empty frequency sort", empty);
    check_transform<nibble::ContextSortTransform<char>>("empty context sort", empty);
    check_transform<nibble::PredictiveNibbleLagTransform<char>>("empty predictive lag", empty);
    check_transform<nibble::HMCTransform<char>>("empty HMC", empty);
    check_transform<nibble::CombinedPredictorTransform<char>>("empty combined predictor", empty);
    check_transform<nibble::XORAVTTransform<char>>("empty XOR AVT", empty);
    check_transform<nibble::PatternMatchTransform<char>>("empty pattern match", empty);
    check_transform<nibble::MultimoveChainTransform<char>>("empty multimove chain", empty);
    check_transform<nibble::WaveletTreeTransform<char>>("empty wavelet tree", empty);
    check_transform<nibble::AdaptiveTransform<char>>("empty adaptive", empty);

    std::vector<uint8_t> input;
    for (size_t i = 0; i < 320; ++i) {
        input.push_back(static_cast<uint8_t>((i * 7 + i / 3) & 0x0F));
    }

    check_transform<nibble::XORMeanNormalizeTransform<char>>("XOR mean", input);
    check_transform<nibble::BWTTransform<char>>("BWT", input);
    check_transform<nibble::MTFTransform<char>>("MTF", input);
    check_transform<nibble::RLETransform<char>>("RLE", input);
    check_transform<nibble::FreqSortTransform<char>>("frequency sort", input);
    check_transform<nibble::ContextSortTransform<char>>("context sort", input);
    check_transform<nibble::PredictiveNibbleLagTransform<char>>("predictive lag", input);
    check_transform<nibble::HMCTransform<char>>("HMC", input);
    check_transform<nibble::CombinedPredictorTransform<char>>("combined predictor", input);
    check_transform<nibble::XORAVTTransform<char>>("XOR AVT", input);
    check_transform<nibble::PatternMatchTransform<char>>("pattern match", input);
    check_transform<nibble::MultimoveChainTransform<char>>("multimove chain", input);
    check_transform<nibble::WaveletTreeTransform<char>>("wavelet tree", input);
    check_transform<nibble::AdaptiveTransform<char>>("adaptive", input);

    for (size_t run : {size_t(14), size_t(15), size_t(16), size_t(255), size_t(256)}) {
        check_transform<nibble::RLETransform<char>>(
            "RLE run length " + std::to_string(run), std::vector<uint8_t>(run, 7));
    }

    std::vector<uint8_t> large_bwt(70000);
    for (size_t i = 0; i < large_bwt.size(); ++i) {
        large_bwt[i] = static_cast<uint8_t>((i * 13 + i / 7) & 0x0F);
    }
    check_transform<nibble::BWTTransform<char>>("large BWT", large_bwt);
}

void test_backends() {
    std::vector<uint8_t> input;
    input.reserve(1024);
    for (size_t i = 0; i < 1024; ++i) {
        input.push_back(static_cast<uint8_t>((i / 9 + i % 5) & 0x0F));
    }
    check_backend<nibble::NibbleLZ77Compressor<char>>("LZ77", input);
    check_backend<nibble::NibbleRLECompressor<char>>("RLE", input);
    for (size_t length = 1; length <= 64; ++length) {
        std::vector<uint8_t> prefix(input.begin(), input.begin() + length);
        check_backend<nibble::NibbleArithmeticCompressor<char>>(
            "arithmetic length " + std::to_string(length), prefix);
    }
    check_backend<nibble::NibbleArithmeticCompressor<char>>("arithmetic", input);
    check_backend<nibble::NibbleHuffmanCompressor<char>>("Huffman", input);
    check_backend<nibble::NibbleLZ77Compressor<char>>(
        "LZ77 marker escaping", {0xFE, 0xFF, 0x00, 0x01, 0xFE, 0xFF});
    for (const std::vector<uint8_t>& edge : {
             std::vector<uint8_t>{},
             std::vector<uint8_t>{0},
             std::vector<uint8_t>(400, 15)}) {
        check_backend<nibble::NibbleLZ77Compressor<char>>("LZ77 edge case", edge);
        check_backend<nibble::NibbleRLECompressor<char>>("RLE edge case", edge);
        check_backend<nibble::NibbleArithmeticCompressor<char>>("arithmetic edge case", edge);
        check_backend<nibble::NibbleHuffmanCompressor<char>>("Huffman edge case", edge);
    }
    std::vector<uint8_t> byte_values(256);
    for (size_t i = 0; i < byte_values.size(); ++i) byte_values[i] = static_cast<uint8_t>(i);
    check_backend<nibble::NibbleArithmeticCompressor<char>>("arithmetic byte symbols", byte_values);
}

void test_text_encoder() {
    const std::filesystem::path dictionary =
        std::filesystem::temp_directory_path() / "nibble_encoder_test_dictionary.txt";
    {
        std::ofstream file(dictionary);
        file << "the\nquick\nbrown\nfox\nvisits\ncafé\n"
                "la\nhistoria\nde\nuna\nciudad\ncuando\nlos\nviajeros\n"
                "comparten\nideas\nvisitan\ny\nleen\nrelatos\nespañol\n"
                "le\nrenard\nbrun\net\nles\nvoyageurs\npartagent\nleurs\nidées\n"
                "das\nschnelle\nbraune\nund\nreisende\nteilen\nihre\n"
                "o\nraposo\nviaja\npela\ncidade\ncompartilham\n";
    }

    const std::string english_paragraph =
        "The quick brown fox visits the old café, while curious travelers "
        "read stories and share ideas. ";
    const std::string spanish_paragraph =
        "La historia de una ciudad cambia cuando los viajeros comparten ideas, "
        "visitan el café y leen relatos en español. ";
    const std::string french_paragraph =
        "Le renard brun traverse la ville, et les voyageurs partagent leurs idées "
        "au café en lisant une histoire. ";
    const std::string german_paragraph =
        "Das schnelle braune Pferd zieht durch die Stadt, und Reisende teilen "
        "ihre Gedanken bei Kaffee und Geschichten. ";
    const std::string portuguese_paragraph =
        "O raposo viaja pela cidade, e os viajantes compartilham histórias, "
        "ideias e café durante a manhã. ";
    const auto repeat = [](const std::string& paragraph, size_t count) {
        std::string result;
        result.reserve(paragraph.size() * count);
        for (size_t i = 0; i < count; ++i) result += paragraph;
        return result;
    };

    struct Sample {
        const char* language;
        const char* scale;
        std::string text;
    };
    const std::vector<Sample> samples{
        {"EN", "short", "A short café — déjà vu!"},
        {"ES", "short", "¡Hola, señor! ¿Cómo estás? Acción, pingüino y café."},
        {"FR", "short", "Bonjour, déjà vu ! Voilà l'été; où est le garçon?"},
        {"DE", "short", "Grüße, Straße, Mädchen, schön und größer."},
        {"PT", "short", "Olá, senhor! Ação, coração, pão e café."},
        {"EN", "medium", repeat(english_paragraph, 12)},
        {"ES", "medium", repeat(spanish_paragraph, 12)},
        {"FR", "medium", repeat(french_paragraph, 12)},
        {"DE", "medium", repeat(german_paragraph, 12)},
        {"PT", "medium", repeat(portuguese_paragraph, 12)},
        {"EN", "large", repeat(english_paragraph, 250)},
        {"ES", "large", repeat(spanish_paragraph, 250)},
        {"FR", "large", repeat(french_paragraph, 250)},
        {"DE", "large", repeat(german_paragraph, 250)},
        {"PT", "large", repeat(portuguese_paragraph, 250)}
    };
    const std::string empty;

    auto encoder = nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary.string())
        .with_english_text_pipeline()
        .build();

    std::cout << "\nCompression by language and size (archive = payload + framing)\n"
              << std::fixed << std::setprecision(1);
    for (const Sample& sample : samples) {
        const std::string encoded = encoder->encode(sample.text);
        require(encoder->decode(encoded) == sample.text,
                std::string(sample.language) + " UTF-8 sample failed to round-trip");
        const ArchiveSizes sizes = measure_archive(encoded);
        require(sizes.framing_bytes + sizes.payload_bytes == encoded.size(),
                "Framing and payload do not account for the complete archive");
        require(sizes.transform_metadata_bytes <= sizes.framing_bytes,
                "Transform metadata exceeds archive framing");
        const double payload_ratio =
            static_cast<double>(sizes.payload_bytes) / sample.text.size();
        const double archive_ratio =
            static_cast<double>(encoded.size()) / sample.text.size();
        if (sample.scale == "large") {
            require(archive_ratio < 0.10,
                    std::string(sample.language) +
                        " large repeated sample exceeded 10% of its input size");
        }
        std::cout << sample.language << ' ' << std::setw(6) << sample.scale
                  << ": input " << std::setw(6) << sample.text.size()
                  << " B, payload " << std::setw(6) << sizes.payload_bytes
                  << " B, framing " << std::setw(5) << sizes.framing_bytes
                  << " B (fixed " << (sizes.framing_bytes - sizes.transform_metadata_bytes)
                  << " + metadata " << sizes.transform_metadata_bytes
                  << "), archive " << std::setw(6) << encoded.size()
                  << " B; payload/input " << std::setw(6) << payload_ratio * 100.0
                  << "%, archive/input " << std::setw(6) << archive_ratio * 100.0
                  << "%\n";
    }
    test_pipeline_chains(dictionary.string(),
        repeat(english_paragraph + spanish_paragraph + french_paragraph +
               german_paragraph + portuguese_paragraph, 4));
    const std::string empty_archive = encoder->encode(empty);
    require(encoder->decode(empty_archive).empty(), "Empty text failed to round-trip");

    auto raw_encoder = nibble::NibbleEncoderBuilder<>()
        .with_dictionary_file(dictionary.string())
        .build();
    require(raw_encoder->decode(raw_encoder->encode(samples.front().text)) ==
                samples.front().text,
            "Uncompressed UTF-8 text failed to round-trip");
    std::filesystem::remove(dictionary);
}

} // namespace

int main() {
    try {
        test_transforms();
        test_backends();
        test_text_encoder();
        std::cout << "All nibble encoder tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
