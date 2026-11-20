// nibble_encoder.hpp
#ifndef NIBBLE_ENCODER_HPP
#define NIBBLE_ENCODER_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <stack>
#include <memory>
#include <fstream>
#include <sstream>
#include <functional>
#include <array>
#include <bitset>
#include <queue>
#include <cmath>
#include <cstring>
#include <utility>

namespace nibble {

// ============================================================================
// CONSTANTS
// ============================================================================

constexpr uint8_t MOD_SIGNAL = 0x0F;
constexpr uint8_t SPACE = 0x00;
constexpr uint8_t NEWLINE = 0x01;
constexpr uint8_t TAB = 0x02;
constexpr uint8_t EOF_MARKER = 0x05;
constexpr size_t DEFAULT_BLOCK_SIZE = 4096;
constexpr size_t XOR_WINDOW_SIZE = 256;
constexpr size_t LZ77_WINDOW_SIZE = 4096;
constexpr size_t LZ77_LOOKAHEAD = 16;

// Control markers (using 0xFF prefix to avoid collisions)
constexpr uint8_t CONTROL_ESCAPE = 0xFF;
constexpr uint8_t LZ77_MATCH_MARKER = 0xFE;
constexpr uint8_t RLE_MARKER = 0xFD;
constexpr uint8_t HUFFMAN_MARKER = 0xFC;
constexpr uint8_t ARITH_EOF = 0xFB;

// Modifier types
enum class ModifierType : uint8_t {
    CAPS_NEXT = 0x0E,
    CAPS_LOCK = 0x0D,
    DICT_SHIFT_TEMP = 0x0C,
    DICT_SHIFT_STICKY = 0x0B,
    DICT_NEW_PAGE = 0x0A,
    REPEAT_LAST = 0x09,
    ESCAPE = 0x08,
    CONTROL = 0x07,
    ALT = 0x06,
    SHIFT_GROUP = 0x05,
    AUTO_BRACE = 0x04,
    AUTO_SGML = 0x03,
    TEMPLATE_REF = 0x02,
    DICT_WORD_REF = 0x01,
};

// Transform IDs
enum class TransformID : uint8_t {
    NONE = 0x00,
    XOR_MEAN_NORMALIZE = 0x01,
    BWT = 0x02,
    MTF = 0x03,
    RLE = 0x04,
    FREQ_SORT = 0x05,
    CONTEXT_SORT = 0x06,
    PREDICTIVE_NIBBLE_LAG = 0x07,
    HIDDEN_MARKOV_CHAIN = 0x08,
    COMBINED_PREDICTOR = 0x09,
    XOR_AVT = 0x0A,
    PATTERN_MATCH = 0x0B,
    MULTIMOVE_CHAIN = 0x0C,
    WAVELET_TREE = 0x0D,
    ADAPTIVE = 0x0F
};

// Backend Compressor IDs
enum class BackendID : uint8_t {
    NONE = 0x00,
    LZ77 = 0x10,
    RLE = 0x11,
    ARITHMETIC = 0x12,
    HUFFMAN = 0x13
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace utils {
    
    inline uint8_t pack_nibbles(uint8_t high, uint8_t low) {
        return ((high & 0x0F) << 4) | (low & 0x0F);
    }
    
    inline uint8_t get_high_nibble(uint8_t byte) {
        return (byte >> 4) & 0x0F;
    }
    
    inline uint8_t get_low_nibble(uint8_t byte) {
        return byte & 0x0F;
    }
    
    inline std::vector<uint8_t> pack_nibbles(const std::vector<uint8_t>& nibbles) {
        std::vector<uint8_t> bytes;
        bytes.reserve((nibbles.size() + 1) / 2);
        for (size_t i = 0; i < nibbles.size(); i += 2) {
            uint8_t byte = (nibbles[i] & 0x0F) << 4;
            if (i + 1 < nibbles.size()) byte |= (nibbles[i + 1] & 0x0F);
            bytes.push_back(byte);
        }
        return bytes;
    }
    
    inline std::vector<uint8_t> unpack_nibbles(const std::vector<uint8_t>& packed) {
        std::vector<uint8_t> nibbles;
        nibbles.reserve(packed.size() * 2);
        for (uint8_t byte : packed) {
            nibbles.push_back((byte >> 4) & 0x0F);
            nibbles.push_back(byte & 0x0F);
        }
        return nibbles;
    }
    
    inline double calculate_entropy(const std::vector<uint8_t>& data) {
        std::array<size_t, 16> freq = {};
        for (uint8_t b : data) freq[b & 0x0F]++;
        
        double entropy = 0.0;
        for (size_t count : freq) {
            if (count > 0) {
                double p = static_cast<double>(count) / data.size();
                entropy -= p * log2(p);
            }
        }
        return entropy;
    }
    
    inline uint8_t calculate_radix(const std::vector<uint8_t>& data) {
        uint16_t mask = 0;
        for (uint8_t b : data) mask |= (1 << (b & 0x0F));
        return static_cast<uint8_t>(std::bitset<16>(mask).count());
    }
    
    inline void write_vlint(std::vector<uint8_t>& nibbles, size_t value) {
        if (value < 0x0F) {
            nibbles.push_back(static_cast<uint8_t>(value));
        } else {
            nibbles.push_back(0x0F);
            while (value >= 0x0F) {
                nibbles.push_back(static_cast<uint8_t>(value & 0x0F));
                value >>= 4;
                if (value > 0) nibbles.push_back(0x0F);
            }
            nibbles.push_back(static_cast<uint8_t>(value));
        }
    }
    
    inline size_t read_vlint(const std::vector<uint8_t>& nibbles, size_t& pos) {
        size_t value = 0;
        size_t shift = 0;
        while (pos < nibbles.size()) {
            uint8_t nib = nibbles[pos++];
            if (nib == 0x0F && pos < nibbles.size()) {
                value |= (static_cast<size_t>(nibbles[pos++]) << shift);
                shift += 4;
            } else {
                value |= (static_cast<size_t>(nib) << shift);
                break;
            }
        }
        return value;
    }
} // namespace utils

// ============================================================================
// MISSING DEPENDENCIES (STUB IMPLEMENTATIONS)
// ============================================================================

template<typename CharT>
class CharacterMapper {
public:
    uint8_t to_nibble(CharT c) const {
        return static_cast<uint8_t>(c) & 0x0F;
    }

    CharT to_char(uint8_t n) const {
        return static_cast<CharT>(n & 0x0F);
    }
};

template<typename CharT>
class ResourceLoader {
public:
    void load_templates(const std::string&) {
        throw std::runtime_error("Template loading is not supported");
    }
    void load_word_dictionary(const std::string&) {}
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

template<typename CharT> class ITransform;
template<typename CharT> class IBackendCompressor;
template<typename CharT> class NibbleEncoder;

} // namespace nibble

namespace nibble {

using nibble_t = uint8_t;

// ============================================================================
// ITRANSFORM INTERFACE
// ============================================================================

template<typename CharT>
class ITransform {
public:
    virtual ~ITransform() = default;
    virtual std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) = 0;
    virtual void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) = 0;
    virtual TransformID get_id() const = 0;
    virtual std::string get_name() const = 0;
    virtual size_t get_metadata_size() const = 0;
};

// ============================================================================
// TRANSFORM 1: XOR MEAN NORMALIZE
// ============================================================================

template<typename CharT>
class XORMeanNormalizeTransform : public ITransform<CharT> {
private:
    size_t window_size;
    
public:
    explicit XORMeanNormalizeTransform(size_t window = XOR_WINDOW_SIZE) : window_size(window) {}
    
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> window;
        window.reserve(window_size);
        uint32_t sum = 0;
        
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint8_t mean = 0;
            if (!window.empty()) {
                mean = static_cast<uint8_t>((sum / window.size()) & 0x0F);
            }
            const uint8_t original = nibbles[i];
            nibbles[i] ^= mean;
            window.push_back(original);
            sum += original;
            if (window.size() > window_size) {
                sum -= window.front();
                window.erase(window.begin());
            }
        }
        
        return {};
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& /*metadata*/) override {
        std::vector<uint8_t> window;
        window.reserve(window_size);
        uint32_t sum = 0;
        
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint8_t mean = window.empty()
                ? 0
                : static_cast<uint8_t>((sum / window.size()) & 0x0F);
            nibbles[i] ^= mean;
            window.push_back(nibbles[i]);
            sum += nibbles[i];
            if (window.size() > window_size) {
                sum -= window.front();
                window.erase(window.begin());
            }
        }
    }
    
    TransformID get_id() const override { return TransformID::XOR_MEAN_NORMALIZE; }
    std::string get_name() const override { return "XOR_MEAN_NORMALIZE"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 2: BURROWS-WHEELER (BWT)
// ============================================================================

template<typename CharT>
class BWTTransform : public ITransform<CharT> {
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        size_t n = nibbles.size();
        if (n == 0) return {};
        
        std::vector<uint8_t> metadata;
        
        std::vector<size_t> indices(n);
        std::vector<size_t> classes(n);
        for (size_t i = 0; i < n; i++) indices[i] = i;
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return nibbles[a] == nibbles[b] ? a < b : nibbles[a] < nibbles[b];
        });
        size_t class_count = 1;
        for (size_t i = 0; i < n; ++i) {
            if (i > 0 && nibbles[indices[i]] != nibbles[indices[i - 1]]) {
                ++class_count;
            }
            classes[indices[i]] = class_count - 1;
        }

        for (size_t length = 1; length < n && class_count < n; length *= 2) {
            std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
                const auto a_key = std::make_pair(classes[a], classes[(a + length) % n]);
                const auto b_key = std::make_pair(classes[b], classes[(b + length) % n]);
                return a_key == b_key ? a < b : a_key < b_key;
            });
            std::vector<size_t> next_classes(n);
            class_count = 1;
            for (size_t i = 0; i < n; ++i) {
                if (i > 0) {
                    size_t prev = indices[i - 1];
                    size_t curr = indices[i];
                    if (classes[prev] != classes[curr] ||
                        classes[(prev + length) % n] != classes[(curr + length) % n]) {
                        ++class_count;
                    }
                }
                next_classes[indices[i]] = class_count - 1;
            }
            classes = std::move(next_classes);
        }
        
        std::vector<uint8_t> transformed(n);
        size_t original_index = 0;
        for (size_t i = 0; i < n; i++) {
            transformed[i] = nibbles[(indices[i] + n - 1) % n];
            if (indices[i] == 0) original_index = i;
        }
        
        for (unsigned shift = 0; shift < 32; shift += 8) {
            metadata.push_back(static_cast<uint8_t>(original_index >> shift));
        }
        
        nibbles = std::move(transformed);
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        size_t n = nibbles.size();
        if (n == 0 || metadata.size() < 4) return;
        
        size_t original_index = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            original_index |= static_cast<size_t>(metadata[shift / 8]) << shift;
        }
        
        std::vector<uint8_t> first_col = nibbles;
        std::sort(first_col.begin(), first_col.end());
        
        std::vector<std::vector<size_t>> positions(16);
        for (size_t i = 0; i < n; i++) {
            positions[nibbles[i]].push_back(i);
        }
        
        std::vector<size_t> next(n);
        std::vector<size_t> pos_ptr(16, 0);
        for (size_t i = 0; i < n; i++) {
            uint8_t val = first_col[i];
            next[i] = positions[val][pos_ptr[val]++];
        }
        
        std::vector<uint8_t> recovered(n);
        size_t idx = original_index;
        for (size_t i = 0; i < n; i++) {
            recovered[i] = first_col[idx];
            idx = next[idx];
        }
        
        nibbles = std::move(recovered);
    }
    
    TransformID get_id() const override { return TransformID::BWT; }
    std::string get_name() const override { return "BWT"; }
    size_t get_metadata_size() const override { return 4; }
};

// ============================================================================
// TRANSFORM 3: MOVE-TO-FRONT (MTF)
// ============================================================================

template<typename CharT>
class MTFTransform : public ITransform<CharT> {
private:
    std::array<uint8_t, 16> symbol_table;
    
public:
    MTFTransform() {
        for (int i = 0; i < 16; i++) symbol_table[i] = static_cast<uint8_t>(i);
    }
    
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        metadata.reserve(nibbles.size());
        
        std::array<uint8_t, 16> table = symbol_table;
        
        for (uint8_t val : nibbles) {
            uint8_t pos = 0;
            while (pos < 16 && table[pos] != val) pos++;
            metadata.push_back(pos);
            
            if (pos > 0) {
                for (uint8_t i = pos; i > 0; i--) table[i] = table[i - 1];
                table[0] = val;
            }
        }
        
        nibbles = metadata;
        return {};
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& /*metadata*/) override {
        std::array<uint8_t, 16> table = symbol_table;
        
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint8_t pos = nibbles[i];
            if (pos < 16) {
                uint8_t val = table[pos];
                nibbles[i] = val;
                
                if (pos > 0) {
                    for (uint8_t j = pos; j > 0; j--) table[j] = table[j - 1];
                    table[0] = val;
                }
            }
        }
    }
    
    TransformID get_id() const override { return TransformID::MTF; }
    std::string get_name() const override { return "MTF"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 4: RUN-LENGTH ENCODING (RLE)
// ============================================================================

template<typename CharT>
class RLETransform : public ITransform<CharT> {
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> encoded;
        
        size_t i = 0;
        while (i < nibbles.size()) {
            uint8_t current = nibbles[i];
            size_t run_start = i;
            
            while (i < nibbles.size() && nibbles[i] == current && (i - run_start) < 255) {
                i++;
            }
            size_t run_len = i - run_start;
            
            if (run_len == 1) {
                if (current == 0xFD || current == 0xFF) {
                    encoded.push_back(0xFF);
                }
                encoded.push_back(current);
            } else if (run_len < 15) {
                encoded.push_back(0xFD);
                encoded.push_back(static_cast<uint8_t>(run_len));
                encoded.push_back(current);
            } else {
                encoded.push_back(0xFD);
                encoded.push_back(0x0F);
                encoded.push_back(static_cast<uint8_t>(run_len & 0x0F));
                encoded.push_back(static_cast<uint8_t>((run_len >> 4) & 0x0F));
                encoded.push_back(current);
            }
        }
        
        nibbles = std::move(encoded);
        return {};
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& /*metadata*/) override {
        std::vector<uint8_t> decoded;
        
        size_t i = 0;
        while (i < nibbles.size()) {
            uint8_t nib = nibbles[i];
            
            if (nib == 0xFD && i + 2 < nibbles.size()) {
                uint8_t len_nib = nibbles[i + 1];
                
                if (len_nib < 0x0F) {
                    size_t length = len_nib;
                    uint8_t value = nibbles[i + 2];
                    for (size_t j = 0; j < length; j++) decoded.push_back(value);
                    i += 3;
                } else if (len_nib == 0x0F && i + 4 < nibbles.size()) {
                    size_t length = nibbles[i + 2] | (static_cast<size_t>(nibbles[i + 3]) << 4);
                    uint8_t value = nibbles[i + 4];
                    for (size_t j = 0; j < length; j++) decoded.push_back(value);
                    i += 5;
                } else {
                    i++;
                }
            } else if (nib == 0xFF && i + 1 < nibbles.size()) {
                decoded.push_back(nibbles[i + 1]);
                i += 2;
            } else {
                decoded.push_back(nib);
                i++;
            }
        }
        
        nibbles = std::move(decoded);
    }
    
    TransformID get_id() const override { return TransformID::RLE; }
    std::string get_name() const override { return "RLE"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 5: FREQUENCY SORT
// ============================================================================

template<typename CharT>
class FreqSortTransform : public ITransform<CharT> {
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::array<size_t, 16> freq = {};
        for (uint8_t n : nibbles) freq[n]++;
        
        std::vector<std::pair<size_t, uint8_t>> pairs;
        for (int i = 0; i < 16; i++) {
            if (freq[i] > 0) pairs.emplace_back(freq[i], static_cast<uint8_t>(i));
        }
        std::sort(pairs.begin(), pairs.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        std::vector<uint8_t> freq_order;
        for (const auto& p : pairs) freq_order.push_back(p.second);
        
        std::array<uint8_t, 16> remap;
        for (size_t i = 0; i < freq_order.size(); i++) {
            remap[freq_order[i]] = static_cast<uint8_t>(i);
        }
        
        for (uint8_t& n : nibbles) {
            n = remap[n];
        }
        
        std::vector<uint8_t> metadata;
        metadata.push_back(static_cast<uint8_t>(freq_order.size()));
        for (uint8_t f : freq_order) metadata.push_back(f);
        
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.empty()) return;
        
        size_t order_size = metadata[0];
        std::vector<uint8_t> freq_order;
        for (size_t i = 1; i <= order_size && i < metadata.size(); i++) {
            freq_order.push_back(metadata[i]);
        }
        
        std::array<uint8_t, 16> inverse_remap;
        for (size_t i = 0; i < freq_order.size(); i++) {
            inverse_remap[static_cast<uint8_t>(i)] = freq_order[i];
        }
        
        for (uint8_t& n : nibbles) {
            n = inverse_remap[n];
        }
    }
    
    TransformID get_id() const override { return TransformID::FREQ_SORT; }
    std::string get_name() const override { return "FREQ_SORT"; }
    size_t get_metadata_size() const override { return 1; }
};

// ============================================================================
// TRANSFORM 6: CONTEXT SORT
// ============================================================================

template<typename CharT>
class ContextSortTransform : public ITransform<CharT> {
private:
    enum ContextType : uint8_t { VOWEL = 0, CONSONANT = 1, DIGIT = 2, PUNCT = 3 };
    
    uint8_t get_context(uint8_t nibble) const {
        if (nibble < 10) return DIGIT;
        if (nibble == 1 || nibble == 5 || nibble == 9 || nibble == 15 || nibble == 21) return VOWEL;
        if (nibble < 26) return CONSONANT;
        return PUNCT;
    }
    
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata((nibbles.size() + 3) / 4, 0);
        for (size_t i = 0; i < nibbles.size(); ++i) {
            metadata[i / 4] |= static_cast<uint8_t>(get_context(nibbles[i]) << ((i % 4) * 2));
        }
        
        std::vector<uint8_t> groups[4];
        for (uint8_t n : nibbles) groups[get_context(n)].push_back(n);
        
        std::vector<uint8_t> sorted;
        for (int i = 0; i < 4; i++) {
            sorted.insert(sorted.end(), groups[i].begin(), groups[i].end());
        }
        
        nibbles = std::move(sorted);
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.size() != (nibbles.size() + 3) / 4) {
            throw std::runtime_error("Invalid context sort metadata");
        }
        
        std::vector<uint8_t> reconstructed(nibbles.size());
        std::vector<size_t> group_indices[4];
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint8_t context = (metadata[i / 4] >> ((i % 4) * 2)) & 0x03;
            group_indices[context].push_back(i);
        }
        
        size_t pos = 0;
        for (int ctx = 0; ctx < 4; ctx++) {
            for (size_t idx : group_indices[ctx]) {
                if (pos < nibbles.size()) reconstructed[idx] = nibbles[pos++];
            }
        }
        
        nibbles = std::move(reconstructed);
    }
    
    TransformID get_id() const override { return TransformID::CONTEXT_SORT; }
    std::string get_name() const override { return "CONTEXT_SORT"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 7: PREDICTIVE NIBBLE LAG (PNL)
// ============================================================================

template<typename CharT>
class PredictiveNibbleLagTransform : public ITransform<CharT> {
private:
    static constexpr size_t MAX_LAG = 8;
    std::vector<size_t> optimal_lags;
    
    std::vector<size_t> find_optimal_lags(const std::vector<uint8_t>& nibbles) {
        std::vector<size_t> lags;
        for (size_t lag = 1; lag <= MAX_LAG; lag++) {
            size_t correct = 0, total = 0;
            for (size_t i = lag; i < nibbles.size(); i++) {
                if (nibbles[i] == nibbles[i - lag]) correct++;
                total++;
            }
            if (total > 0 && static_cast<double>(correct) / total > 0.5) {
                lags.push_back(lag);
            }
        }
        if (lags.empty()) lags.push_back(1);
        return lags;
    }
    
    uint8_t predict(const std::vector<uint8_t>& history, size_t pos) {
        if (pos < *std::min_element(optimal_lags.begin(), optimal_lags.end())) return 0;
        
        std::array<size_t, 16> votes = {};
        for (size_t lag : optimal_lags) {
            if (pos >= lag) votes[history[pos - lag]]++;
        }
        
        uint8_t best = 0;
        size_t best_votes = 0;
        for (int i = 0; i < 16; i++) {
            if (votes[i] > best_votes) {
                best_votes = votes[i];
                best = static_cast<uint8_t>(i);
            }
        }
        return best;
    }
    
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        optimal_lags = find_optimal_lags(nibbles);
        
        metadata.push_back(static_cast<uint8_t>(optimal_lags.size()));
        for (size_t lag : optimal_lags) metadata.push_back(static_cast<uint8_t>(lag));
        
        std::vector<uint8_t> residuals(nibbles.size());
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint8_t pred = predict(nibbles, i);
            residuals[i] = nibbles[i] ^ pred;
        }
        
        nibbles = std::move(residuals);
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.empty()) return;
        
        size_t num_lags = metadata[0];
        optimal_lags.clear();
        for (size_t i = 1; i <= num_lags && i < metadata.size(); i++) {
            optimal_lags.push_back(metadata[i]);
        }
        
        std::vector<uint8_t> recovered(nibbles.size());
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint8_t pred = predict(recovered, i);
            recovered[i] = nibbles[i] ^ pred;
        }
        
        nibbles = std::move(recovered);
    }
    
    TransformID get_id() const override { return TransformID::PREDICTIVE_NIBBLE_LAG; }
    std::string get_name() const override { return "PREDICTIVE_NIBBLE_LAG"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 8: HIDDEN MARKOV CHAIN (HMC)
// ============================================================================

template<typename CharT>
class HMCTransform : public ITransform<CharT> {
private:
    static constexpr size_t STATE_SIZE = 16;
    static constexpr size_t MAX_ORDER = 4;
    static constexpr double SMOOTHING = 0.01;
    
    struct MarkovState {
        std::array<size_t, STATE_SIZE> counts;
        size_t count;
        MarkovState() : counts{}, count(0) {}
    };
    
    std::unordered_map<uint32_t, MarkovState> models;
    size_t current_order;
    
    uint32_t get_context(const std::vector<uint8_t>& data, size_t pos) {
        uint32_t context = 0;
        size_t start = (pos >= current_order) ? pos - current_order : 0;
        size_t actual = std::min(current_order, pos);
        for (size_t i = 0; i < actual; i++) {
            context = (context << 4) | data[start + i];
        }
        return context;
    }
    
    void update_model(uint32_t context, uint8_t value) {
        auto& state = models[context];
        state.counts[value]++;
        state.count++;
    }
    
    uint8_t predict(uint32_t context) {
        auto it = models.find(context);
        if (it == models.end()) return 0;
        
        uint8_t best = 0;
        size_t best_count = 0;
        for (size_t value = 0; value < STATE_SIZE; value++) {
            if (it->second.counts[value] > best_count) {
                best_count = it->second.counts[value];
                best = static_cast<uint8_t>(value);
            }
        }
        return best;
    }
    
public:
    HMCTransform(size_t order = 2) : current_order(order) {}
    
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        metadata.push_back(static_cast<uint8_t>(current_order));
        
        models.clear();
        std::vector<uint8_t> residuals(nibbles.size());
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint32_t ctx = get_context(nibbles, i);
            uint8_t pred = predict(ctx);
            residuals[i] = nibbles[i] ^ pred;
            update_model(ctx, nibbles[i]);
        }
        
        nibbles = std::move(residuals);
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.empty()) return;
        current_order = metadata[0];
        
        models.clear();
        std::vector<uint8_t> recovered(nibbles.size());
        
        for (size_t i = 0; i < nibbles.size(); i++) {
            uint32_t ctx = get_context(recovered, i);
            uint8_t pred = predict(ctx);
            recovered[i] = nibbles[i] ^ pred;
            update_model(ctx, recovered[i]);
        }
        
        nibbles = std::move(recovered);
    }
    
    TransformID get_id() const override { return TransformID::HIDDEN_MARKOV_CHAIN; }
    std::string get_name() const override { return "HMC"; }
    size_t get_metadata_size() const override { return 1; }
};

// ============================================================================
// TRANSFORM 9: COMBINED PREDICTOR (PNL + HMC)
// ============================================================================

template<typename CharT>
class CombinedPredictorTransform : public ITransform<CharT> {
private:
    PredictiveNibbleLagTransform<CharT> pnl;
    HMCTransform<CharT> hmc;
    
public:
    CombinedPredictorTransform(size_t order = 2) : hmc(order) {}
    
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        
        std::vector<uint8_t> original = nibbles;
        
        auto pnl_metadata = pnl.apply(nibbles);
        metadata.push_back(0x01);
        metadata.push_back(static_cast<uint8_t>(pnl_metadata.size()));
        metadata.insert(metadata.end(), pnl_metadata.begin(), pnl_metadata.end());
        
        auto hmc_metadata = hmc.apply(nibbles);
        metadata.push_back(0x02);
        metadata.push_back(static_cast<uint8_t>(hmc_metadata.size()));
        metadata.insert(metadata.end(), hmc_metadata.begin(), hmc_metadata.end());
        
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        size_t pos = 0;
        std::vector<uint8_t> pnl_metadata;
        std::vector<uint8_t> hmc_metadata;
        while (pos < metadata.size()) {
            if (metadata.size() - pos < 2) {
                throw std::runtime_error("Invalid combined predictor metadata");
            }
            const uint8_t transform_id = metadata[pos++];
            const size_t length = metadata[pos++];
            if (length > metadata.size() - pos) {
                throw std::runtime_error("Truncated combined predictor metadata");
            }
            std::vector<uint8_t> part(metadata.begin() + pos, metadata.begin() + pos + length);
            if (transform_id == 0x01) pnl_metadata = std::move(part);
            else if (transform_id == 0x02) hmc_metadata = std::move(part);
            else throw std::runtime_error("Unknown combined predictor transform");
            pos += length;
        }
        hmc.reverse(nibbles, hmc_metadata);
        pnl.reverse(nibbles, pnl_metadata);
    }
    
    TransformID get_id() const override { return TransformID::COMBINED_PREDICTOR; }
    std::string get_name() const override { return "COMBINED_PREDICTOR"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 10: XOR AVT
// ============================================================================

template<typename CharT>
class XORAVTTransform : public ITransform<CharT> {
private:
    static constexpr size_t WINDOW_NIBBLES = 512;
    
    uint16_t compute_id(const std::vector<uint8_t>& data, size_t start, size_t end) {
        uint16_t id = 0;
        for (size_t i = start; i < end && i - start < 32; i++) {
            id ^= (data[i] << ((i - start) % 8));
            id = (id << 1) | (id >> 15);
        }
        return id;
    }
    
    uint8_t compute_checksum(const std::vector<uint8_t>& data, size_t start, size_t end) {
        uint8_t c = 0;
        for (size_t i = start; i < end; i++) {
            c ^= data[i];
            c = (c << 1) | (c >> 7);
        }
        return c;
    }
    
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        size_t n = nibbles.size();
        
        for (size_t i = 0; i < n; i += WINDOW_NIBBLES) {
            size_t end = std::min(i + WINDOW_NIBBLES, n);
            
            uint8_t best_mask = 0;
            size_t best_zeros = 0;
            
            for (int mask = 0; mask < 16; mask++) {
                size_t zeros = 0;
                for (size_t j = i; j < end; j++) {
                    uint8_t xored = nibbles[j] ^ mask;
                    zeros += (4 - __builtin_popcount(xored));
                }
                if (zeros > best_zeros) {
                    best_zeros = zeros;
                    best_mask = static_cast<uint8_t>(mask);
                }
            }
            
            for (size_t j = i; j < end; j++) {
                nibbles[j] ^= best_mask;
            }
            
            metadata.push_back(best_mask);
            uint16_t id = compute_id(nibbles, i, end);
            metadata.push_back((id >> 8) & 0xFF);
            metadata.push_back(id & 0xFF);
            metadata.push_back(compute_checksum(nibbles, i, end));
        }
        
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        size_t n = nibbles.size();
        size_t meta_pos = 0;
        
        for (size_t i = 0; i < n && meta_pos + 4 <= metadata.size(); i += WINDOW_NIBBLES) {
            size_t end = std::min(i + WINDOW_NIBBLES, n);
            uint8_t mask = metadata[meta_pos++];
            meta_pos += 3;
            
            for (size_t j = i; j < end; j++) {
                nibbles[j] ^= mask;
            }
        }
    }
    
    TransformID get_id() const override { return TransformID::XOR_AVT; }
    std::string get_name() const override { return "XOR_AVT"; }
    size_t get_metadata_size() const override { return 4; }
};

// ============================================================================
// TRANSFORM 11: PATTERN MATCH
// ============================================================================

template<typename CharT>
class PatternMatchTransform : public ITransform<CharT> {
private:
    struct Pattern {
        std::vector<uint8_t> seq;
        size_t repeats;
        uint8_t hash;
    };
    
    uint8_t compute_hash(const std::vector<uint8_t>& seq) {
        uint8_t h = 0;
        for (uint8_t n : seq) h = (h << 1) ^ (n & 0x0F);
        return h;
    }
    
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        size_t n = nibbles.size();
        if (n < 4) return metadata;
        
        Pattern best;
        best.repeats = 0;
        
        for (size_t len = 2; len <= 8 && len <= n / 2; len++) {
            for (size_t start = 0; start < len && start + len <= n; start++) {
                Pattern p;
                p.seq.assign(nibbles.begin() + start, nibbles.begin() + start + len);
                p.hash = compute_hash(p.seq);
                p.repeats = 0;
                
                for (size_t pos = start; pos + len <= n; pos += len) {
                    bool match = true;
                    for (size_t k = 0; k < len; k++) {
                        if (nibbles[pos + k] != p.seq[k]) { match = false; break; }
                    }
                    if (match) p.repeats++;
                    else break;
                }
                
                if (p.repeats > best.repeats && p.repeats >= 2) best = p;
            }
        }
        
        if (best.repeats > 0) {
            metadata.push_back(static_cast<uint8_t>(best.seq.size()));
            for (uint8_t n : best.seq) metadata.push_back(n);
            
            std::vector<uint8_t> encoded;
            for (size_t i = 0; i < n; i++) {
                bool found = false;
                for (size_t j = 0; j < best.seq.size() && i + j < n; j++) {
                    if (nibbles[i + j] != best.seq[j]) break;
                    if (j == best.seq.size() - 1) {
                        encoded.push_back(0xFE);
                        encoded.push_back(best.hash);
                        i += best.seq.size() - 1;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (nibbles[i] == 0xFE || nibbles[i] == 0xFF) encoded.push_back(0xFF);
                    encoded.push_back(nibbles[i]);
                }
            }
            if (encoded.size() + metadata.size() >= nibbles.size()) return {};
            nibbles = std::move(encoded);
        }
        
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.empty()) return;
        
        size_t pattern_len = metadata[0];
        if (pattern_len == 0 || metadata.size() < 1 + pattern_len) return;
        
        std::vector<uint8_t> pattern;
        for (size_t i = 1; i <= pattern_len && i < metadata.size(); i++) pattern.push_back(metadata[i]);
        
        uint8_t pattern_hash = 0;
        for (uint8_t n : pattern) pattern_hash = (pattern_hash << 1) ^ (n & 0x0F);
        
        std::vector<uint8_t> decoded;
        size_t i = 0;
        
        while (i < nibbles.size()) {
            uint8_t nib = nibbles[i];
            if (nib == 0xFE && i + 1 < nibbles.size() && nibbles[i + 1] == pattern_hash) {
                decoded.insert(decoded.end(), pattern.begin(), pattern.end());
                i += 2;
            } else if (nib == 0xFF && i + 1 < nibbles.size()) {
                decoded.push_back(nibbles[i + 1]);
                i += 2;
            } else {
                decoded.push_back(nib);
                i++;
            }
        }
        
        nibbles = std::move(decoded);
    }
    
    TransformID get_id() const override { return TransformID::PATTERN_MATCH; }
    std::string get_name() const override { return "PATTERN_MATCH"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 12: MULTIMOVE CHAIN
// ============================================================================

template<typename CharT>
class MultimoveChainTransform : public ITransform<CharT> {
private:
    std::vector<size_t> chain;
    
    void build_chain(const std::vector<uint8_t>& nibbles, size_t window) {
        chain.clear();
        for (size_t i = 0; i < nibbles.size();) {
            size_t length = 1;
            while (length < window && length < 255 &&
                   i + length < nibbles.size() &&
                   nibbles[i] == nibbles[i + length]) {
                ++length;
            }
            chain.push_back(length);
            i += length;
        }
    }
    
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        
        build_chain(nibbles, 8);
        
        uint8_t signature = 0;
        for (size_t len : chain) {
            signature ^= (len & 0xFF);
            signature = (signature << 1) | (signature >> 7);
        }
        metadata.push_back(signature);
        for (unsigned shift = 0; shift < 32; shift += 8) {
            metadata.push_back(static_cast<uint8_t>(chain.size() >> shift));
        }
        for (size_t len : chain) metadata.push_back(static_cast<uint8_t>(len & 0xFF));
        
        std::vector<uint8_t> reordered;
        size_t pos = 0;
        for (size_t len : chain) {
            for (size_t i = 0; i < len && pos < nibbles.size(); i++) {
                reordered.push_back(nibbles[pos++]);
            }
        }
        
        nibbles = std::move(reordered);
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.size() < 5) throw std::runtime_error("Invalid multimove metadata");
        
        size_t chain_size = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            chain_size |= static_cast<size_t>(metadata[1 + shift / 8]) << shift;
        }
        if (chain_size > metadata.size() - 5) {
            throw std::runtime_error("Truncated multimove metadata");
        }
        chain.clear();
        for (size_t i = 0; i < chain_size; i++) {
            chain.push_back(metadata[i + 5]);
        }
        
        std::vector<uint8_t> reconstructed;
        size_t pos = 0;
        for (size_t len : chain) {
            for (size_t i = 0; i < len && pos < nibbles.size(); i++) {
                reconstructed.push_back(nibbles[pos++]);
            }
        }
        
        nibbles = std::move(reconstructed);
    }
    
    TransformID get_id() const override { return TransformID::MULTIMOVE_CHAIN; }
    std::string get_name() const override { return "MULTIMOVE_CHAIN"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 13: WAVELET TREE
// ============================================================================

template<typename CharT>
class WaveletTreeTransform : public ITransform<CharT> {
private:
    struct WaveletNode {
        std::vector<uint8_t> bits;
        WaveletNode* left;
        WaveletNode* right;
        uint8_t low, high;
        WaveletNode(uint8_t l, uint8_t h) : left(nullptr), right(nullptr), low(l), high(h) {}
        ~WaveletNode() { delete left; delete right; }
    };
    
    WaveletNode* root;
    std::vector<uint8_t> serialized;
    
    WaveletNode* build_tree(const std::vector<uint8_t>& data, uint8_t low, uint8_t high) {
        if (low == high || data.empty()) return nullptr;
        
        WaveletNode* node = new WaveletNode(low, high);
        uint8_t mid = (low + high) / 2;
        
        std::vector<uint8_t> left_data, right_data;
        for (uint8_t val : data) {
            if (val <= mid) {
                node->bits.push_back(0);
                left_data.push_back(val);
            } else {
                node->bits.push_back(1);
                right_data.push_back(val);
            }
        }
        
        node->left = build_tree(left_data, low, mid);
        node->right = build_tree(right_data, mid + 1, high);
        
        return node;
    }
    
    void serialize_tree(WaveletNode* node) {
        if (!node) {
            serialized.push_back(0xFF);
            return;
        }
        
        serialized.push_back(node->low);
        serialized.push_back(node->high);
        
        for (size_t i = 0; i < node->bits.size(); i += 8) {
            uint8_t byte = 0;
            for (size_t j = 0; j < 8 && i + j < node->bits.size(); j++) {
                if (node->bits[i + j]) byte |= (1 << j);
            }
            serialized.push_back(byte);
        }
        serialized.push_back(0xFE);
        
        serialize_tree(node->left);
        serialize_tree(node->right);
    }
    
public:
    WaveletTreeTransform() : root(nullptr) {}
    ~WaveletTreeTransform() { delete root; }
    
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> metadata;
        
        delete root;
        root = build_tree(nibbles, 0, 15);
        
        serialized.clear();
        serialize_tree(root);
        
        metadata.push_back(static_cast<uint8_t>(serialized.size()));
        metadata.insert(metadata.end(), serialized.begin(), serialized.end());
        
        return metadata;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        (void)nibbles;
        (void)metadata;
    }
    
    TransformID get_id() const override { return TransformID::WAVELET_TREE; }
    std::string get_name() const override { return "WAVELET_TREE"; }
    size_t get_metadata_size() const override { return 0; }
};

// ============================================================================
// TRANSFORM 14: ADAPTIVE
// ============================================================================

template<typename CharT>
class AdaptiveTransform : public ITransform<CharT> {
private:
    std::unique_ptr<ITransform<CharT>> selected_transform;
    
    double calculate_entropy(const std::vector<uint8_t>& data) {
        std::array<size_t, 16> freq = {};
        for (uint8_t n : data) freq[n]++;
        
        double entropy = 0.0;
        for (size_t count : freq) {
            if (count > 0) {
                double p = static_cast<double>(count) / data.size();
                entropy -= p * log2(p);
            }
        }
        return entropy;
    }
    
    double calculate_run_score(const std::vector<uint8_t>& data) {
        size_t runs = 1;
        for (size_t i = 1; i < data.size(); i++) {
            if (data[i] != data[i - 1]) runs++;
        }
        return static_cast<double>(data.size()) / runs;
    }
    
public:
    std::vector<uint8_t> apply(std::vector<uint8_t>& nibbles) override {
        double entropy = calculate_entropy(nibbles);
        double run_score = calculate_run_score(nibbles);
        
        if (entropy < 2.0) {
            selected_transform = std::make_unique<RLETransform<CharT>>();
        } else if (run_score > 5.0) {
            selected_transform = std::make_unique<XORAVTTransform<CharT>>();
        } else if (entropy < 3.5) {
            selected_transform = std::make_unique<BWTTransform<CharT>>();
        } else {
            selected_transform = std::make_unique<XORMeanNormalizeTransform<CharT>>();
        }
        
        auto metadata = selected_transform->apply(nibbles);
        
        std::vector<uint8_t> result;
        result.push_back(static_cast<uint8_t>(selected_transform->get_id()));
        result.insert(result.end(), metadata.begin(), metadata.end());
        
        return result;
    }
    
    void reverse(std::vector<uint8_t>& nibbles, const std::vector<uint8_t>& metadata) override {
        if (metadata.empty()) return;
        
        TransformID id = static_cast<TransformID>(metadata[0]);
        std::vector<uint8_t> transform_metadata(metadata.begin() + 1, metadata.end());
        
        switch (id) {
            case TransformID::RLE:
                selected_transform = std::make_unique<RLETransform<CharT>>();
                break;
            case TransformID::XOR_AVT:
                selected_transform = std::make_unique<XORAVTTransform<CharT>>();
                break;
            case TransformID::BWT:
                selected_transform = std::make_unique<BWTTransform<CharT>>();
                break;
            case TransformID::XOR_MEAN_NORMALIZE:
                selected_transform = std::make_unique<XORMeanNormalizeTransform<CharT>>();
                break;
            default:
                return;
        }
        
        selected_transform->reverse(nibbles, transform_metadata);
    }
    
    TransformID get_id() const override { return TransformID::ADAPTIVE; }
    std::string get_name() const override { return "ADAPTIVE"; }
    size_t get_metadata_size() const override { return 1; }
};

} // namespace nibble

namespace nibble {

// ============================================================================
// IBACKENDCOMPRESSOR INTERFACE
// ============================================================================

template<typename CharT>
class IBackendCompressor {
public:
    virtual ~IBackendCompressor() = default;
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& nibbles) = 0;
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) = 0;
    virtual std::string get_name() const = 0;
    virtual BackendID get_id() const = 0;
};

// ============================================================================
// BACKEND 1: LZ77
// ============================================================================

template<typename CharT>
class NibbleLZ77Compressor : public IBackendCompressor<CharT> {
private:
    size_t window_size;
    size_t lookahead_size;
    
public:
    NibbleLZ77Compressor(size_t window = LZ77_WINDOW_SIZE, size_t lookahead = LZ77_LOOKAHEAD)
        : window_size(window), lookahead_size(lookahead) {}
    
    std::vector<uint8_t> compress(const std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> output;
        size_t i = 0;
        size_t n = nibbles.size();
        
        while (i < n) {
            size_t best_len = 0;
            size_t best_dist = 0;
            size_t search_start = (i > window_size) ? i - window_size : 0;
            
            for (size_t j = search_start; j < i; j++) {
                size_t len = 0;
                while (len < lookahead_size && i + len < n && 
                       nibbles[j + len] == nibbles[i + len]) {
                    len++;
                }
                if (len > best_len && len >= 2) {
                    best_len = len;
                    best_dist = i - j;
                }
            }
            
            if (best_len >= 2) {
                output.push_back(LZ77_MATCH_MARKER);
                output.push_back(0x0F);
                for (unsigned shift = 0; shift < 32; shift += 8) {
                    output.push_back(static_cast<uint8_t>(best_dist >> shift));
                }
                output.push_back(static_cast<uint8_t>(best_len));
                i += best_len;
            } else {
                if (nibbles[i] == LZ77_MATCH_MARKER || nibbles[i] == CONTROL_ESCAPE) {
                    output.push_back(CONTROL_ESCAPE);
                    output.push_back(0x01);
                }
                output.push_back(nibbles[i]);
                i++;
            }
        }
        
        output.push_back(CONTROL_ESCAPE);
        output.push_back(CONTROL_ESCAPE);
        
        return output;
    }
    
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) override {
        std::vector<uint8_t> output;
        size_t i = 0;
        size_t n = compressed.size();
        
        while (i < n) {
            uint8_t nib = compressed[i];
            
            if (nib == CONTROL_ESCAPE && i + 1 < n && compressed[i + 1] == CONTROL_ESCAPE) {
                break;
            }
            
            if (nib == LZ77_MATCH_MARKER && i + 6 < n && compressed[i + 1] == 0x0F) {
                i += 2;
                size_t distance = 0;
                for (unsigned shift = 0; shift < 32; shift += 8) {
                    distance |= static_cast<size_t>(compressed[i++]) << shift;
                }
                
                size_t length = compressed[i++];
                
                if (distance > 0 && distance <= output.size()) {
                    size_t start = output.size() - distance;
                    for (size_t j = 0; j < length; j++) {
                        output.push_back(output[start + j]);
                    }
                }
            } else if (nib == CONTROL_ESCAPE && i + 2 < n && compressed[i + 1] == 0x01) {
                output.push_back(compressed[i + 2]);
                i += 3;
            } else {
                output.push_back(nib);
                i++;
            }
        }
        
        return output;
    }
    
    std::string get_name() const override { return "NibbleLZ77"; }
    BackendID get_id() const override { return BackendID::LZ77; }
};

// ============================================================================
// BACKEND 2: RLE
// ============================================================================

template<typename CharT>
class NibbleRLECompressor : public IBackendCompressor<CharT> {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> output;
        size_t i = 0;
        size_t n = nibbles.size();
        
        while (i < n) {
            uint8_t current = nibbles[i];
            size_t run_start = i;
            
            while (i < n && nibbles[i] == current && (i - run_start) < 255) i++;
            size_t run_len = i - run_start;
            
            if (run_len == 1) {
                if (current == RLE_MARKER || current == CONTROL_ESCAPE) output.push_back(CONTROL_ESCAPE);
                output.push_back(current);
            } else if (run_len <= 15) {
                output.push_back(RLE_MARKER);
                output.push_back(static_cast<uint8_t>(run_len));
                output.push_back(current);
            } else {
                output.push_back(RLE_MARKER);
                output.push_back(0x0F);
                output.push_back(static_cast<uint8_t>(run_len & 0x0F));
                output.push_back(static_cast<uint8_t>((run_len >> 4) & 0x0F));
                output.push_back(current);
            }
        }
        
        return output;
    }
    
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) override {
        std::vector<uint8_t> output;
        size_t i = 0;
        size_t n = compressed.size();
        
        while (i < n) {
            uint8_t nib = compressed[i];
            
            if (nib == RLE_MARKER && i + 2 < n) {
                uint8_t len_nib = compressed[i + 1];
                
                if (len_nib < 0x0F) {
                    size_t length = len_nib;
                    uint8_t value = compressed[i + 2];
                    for (size_t j = 0; j < length; j++) output.push_back(value);
                    i += 3;
                } else if (len_nib == 0x0F && i + 4 < n) {
                    size_t length = compressed[i + 2] | (static_cast<size_t>(compressed[i + 3]) << 4);
                    uint8_t value = compressed[i + 4];
                    for (size_t j = 0; j < length; j++) output.push_back(value);
                    i += 5;
                } else i++;
            } else if (nib == CONTROL_ESCAPE && i + 1 < n) {
                output.push_back(compressed[i + 1]);
                i += 2;
            } else {
                output.push_back(nib);
                i++;
            }
        }
        
        return output;
    }
    
    std::string get_name() const override { return "NibbleRLE"; }
    BackendID get_id() const override { return BackendID::RLE; }
};

// ============================================================================
// BACKEND 3: ARITHMETIC
// ============================================================================

template<typename CharT>
class NibbleArithmeticCompressor : public IBackendCompressor<CharT> {
private:
    static constexpr uint32_t MAX_RANGE = 0xFFFFFFFF;
    static constexpr uint32_t HALF = 0x80000000;
    static constexpr uint32_t QUARTER = 0x40000000;
    static constexpr size_t SYMBOL_COUNT = 256;
    
    struct FrequencyTable {
        std::array<size_t, SYMBOL_COUNT + 1> freq;
        std::array<size_t, SYMBOL_COUNT + 2> cumulative;
        size_t total;
        
        FrequencyTable() : freq{}, cumulative{}, total(0) { initialize_adaptive(); }
        
        void initialize_adaptive() {
            for (size_t i = 0; i <= SYMBOL_COUNT; i++) freq[i] = 1;
            total = SYMBOL_COUNT + 1;
            rebuild_cumulative();
        }
        
        void rebuild_cumulative() {
            cumulative[0] = 0;
            for (size_t i = 0; i <= SYMBOL_COUNT; i++) {
                cumulative[i + 1] = cumulative[i] + freq[i];
            }
        }
        
        void update(uint8_t symbol) {
            freq[symbol]++;
            total++;
            rebuild_cumulative();
        }
        
        size_t get_low(size_t symbol) const { return cumulative[symbol]; }
        size_t get_high(size_t symbol) const { return cumulative[symbol + 1]; }
        size_t get_total() const { return total; }
        size_t get_eof_symbol() const { return SYMBOL_COUNT; }
    };
    
    struct BitOutput {
        std::vector<uint8_t> bits;
        void write(uint8_t bit) { bits.push_back(bit); }
        std::vector<uint8_t> to_nibbles() {
            std::vector<uint8_t> nibbles;
            for (size_t i = 0; i < bits.size(); i += 4) {
                uint8_t nibble = 0;
                for (size_t j = 0; j < 4 && i + j < bits.size(); j++) {
                    if (bits[i + j]) nibble |= (1 << j);
                }
                nibbles.push_back(nibble);
            }
            return nibbles;
        }
    };
    
    struct BitInput {
        std::vector<uint8_t> bits;
        size_t pos = 0;
        BitInput(const std::vector<uint8_t>& nibbles) {
            for (uint8_t nibble : nibbles) {
                for (int i = 0; i < 4; i++) bits.push_back((nibble >> i) & 1);
            }
        }
        uint8_t read() { return (pos < bits.size()) ? bits[pos++] : 0; }
        bool has_bits() const { return pos < bits.size(); }
    };
    
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& nibbles) override {
        FrequencyTable freq;
        uint32_t low = 0, high = MAX_RANGE, pending = 0;
        BitOutput bit_out;
        
        for (uint8_t symbol : nibbles) {
            uint64_t range = static_cast<uint64_t>(high) - low + 1;
            size_t total = freq.get_total();
            
            uint32_t low_count = static_cast<uint32_t>(freq.get_low(symbol));
            uint32_t high_count = static_cast<uint32_t>(freq.get_high(symbol));
            
            high = static_cast<uint32_t>(low + (range * high_count) / total - 1);
            low = static_cast<uint32_t>(low + (range * low_count) / total);
            freq.update(symbol);
            
            while (true) {
                if ((low & HALF) == (high & HALF)) {
                    bit_out.write((low >> 31) & 1);
                    while (pending > 0) {
                        bit_out.write(((low >> 31) & 1) ^ 1);
                        pending--;
                    }
                    low <<= 1;
                    high = (high << 1) | 1;
                } else if ((low >= QUARTER) && (high < (QUARTER * 3))) {
                    pending++;
                    low -= QUARTER;
                    high -= QUARTER;
                    low <<= 1;
                    high = (high << 1) | 1;
                } else break;
            }
        }
        
        size_t eof = freq.get_eof_symbol();
        uint64_t range = static_cast<uint64_t>(high) - low + 1;
        size_t total = freq.get_total();
        
        uint32_t low_count = static_cast<uint32_t>(freq.get_low(eof));
        uint32_t high_count = static_cast<uint32_t>(freq.get_high(eof));
        
        high = static_cast<uint32_t>(low + (range * high_count) / total - 1);
        low = static_cast<uint32_t>(low + (range * low_count) / total);

        while (true) {
            if ((low & HALF) == (high & HALF)) {
                bit_out.write((low >> 31) & 1);
                while (pending > 0) {
                    bit_out.write(((low >> 31) & 1) ^ 1);
                    pending--;
                }
                low <<= 1;
                high = (high << 1) | 1;
            } else if (low >= QUARTER && high < QUARTER * 3) {
                pending++;
                low -= QUARTER;
                high -= QUARTER;
                low <<= 1;
                high = (high << 1) | 1;
            } else {
                break;
            }
        }

        pending++;
        const uint8_t final_bit = low < QUARTER ? 0 : 1;
        bit_out.write(final_bit);
        while (pending > 1) {
            bit_out.write(final_bit ^ 1);
            pending--;
        }
        
        std::vector<uint8_t> output;
        uint32_t input_size = static_cast<uint32_t>(nibbles.size());
        for (unsigned shift = 0; shift < 32; shift += 4) {
            output.push_back(static_cast<uint8_t>((input_size >> shift) & 0x0F));
        }
        std::vector<uint8_t> encoded_bits = bit_out.to_nibbles();
        output.insert(output.end(), encoded_bits.begin(), encoded_bits.end());
        return output;
    }
    
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) override {
        std::vector<uint8_t> output;
        if (compressed.size() < 8) throw std::runtime_error("Truncated arithmetic stream");
        uint32_t expected_size = 0;
        for (unsigned shift = 0; shift < 32; shift += 4) {
            expected_size |= static_cast<uint32_t>(compressed[shift / 4] & 0x0F) << shift;
        }
        BitInput bit_in(std::vector<uint8_t>(compressed.begin() + 8, compressed.end()));
        FrequencyTable freq;
        
        uint32_t low = 0, high = MAX_RANGE, value = 0;
        for (int i = 0; i < 32; i++) value = (value << 1) | bit_in.read();
        
        size_t eof_symbol = freq.get_eof_symbol();
        
        while (output.size() < expected_size) {
            uint64_t range = static_cast<uint64_t>(high) - low + 1;
            uint64_t scaled_value =
                ((static_cast<uint64_t>(value) - low + 1) * freq.get_total() - 1) / range;
            
            size_t symbol = 0;
            for (size_t i = 0; i <= SYMBOL_COUNT; i++) {
                if (scaled_value < freq.cumulative[i + 1]) {
                    symbol = i;
                    break;
                }
            }
            
            if (symbol == eof_symbol) {
                throw std::runtime_error("Arithmetic stream ended before declared length");
            }
            output.push_back(static_cast<uint8_t>(symbol));
            
            uint32_t low_count = static_cast<uint32_t>(freq.get_low(symbol));
            uint32_t high_count = static_cast<uint32_t>(freq.get_high(symbol));
            
            high = static_cast<uint32_t>(low + (range * high_count) / freq.get_total() - 1);
            low = static_cast<uint32_t>(low + (range * low_count) / freq.get_total());
            freq.update(symbol);
            
            while (true) {
                if ((low & HALF) == (high & HALF)) {
                    low <<= 1;
                    high = (high << 1) | 1;
                    value = (value << 1) | (bit_in.has_bits() ? bit_in.read() : 0);
                } else if ((low >= QUARTER) && (high < (QUARTER * 3))) {
                    low -= QUARTER;
                    high -= QUARTER;
                    value -= QUARTER;
                    low <<= 1;
                    high = (high << 1) | 1;
                    value = (value << 1) | (bit_in.has_bits() ? bit_in.read() : 0);
                } else break;
            }
        }
        return output;
    }
    
    std::string get_name() const override { return "NibbleArithmetic"; }
    BackendID get_id() const override { return BackendID::ARITHMETIC; }
};

// ============================================================================
// BACKEND 4: HUFFMAN
// ============================================================================

template<typename CharT>
class NibbleHuffmanCompressor : public IBackendCompressor<CharT> {
private:
    struct Node {
        uint8_t symbol;
        size_t freq;
        Node* left, *right;
        Node(uint8_t s, size_t f) : symbol(s), freq(f), left(nullptr), right(nullptr) {}
        Node(size_t f, Node* l, Node* r) : symbol(0), freq(f), left(l), right(r) {}
        ~Node() { delete left; delete right; }
    };
    
    struct Compare {
        bool operator()(Node* a, Node* b) { return a->freq > b->freq; }
    };
    
    std::array<uint16_t, 17> code_lengths;
    std::array<uint16_t, 17> first_codes;
    std::array<uint16_t, 17> length_counts;
    
    void build_canonical_codes() {
        length_counts.fill(0);
        for (size_t symbol = 0; symbol < 16; ++symbol) {
            if (code_lengths[symbol] > 0 && code_lengths[symbol] <= 16) {
                length_counts[code_lengths[symbol]]++;
            }
        }
        uint16_t code = 0;
        for (size_t len = 1; len <= 16; len++) {
            code = static_cast<uint16_t>((code + length_counts[len - 1]) << 1);
            first_codes[len] = code;
        }
    }
    
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& nibbles) override {
        std::vector<uint8_t> output;
        if (nibbles.empty()) return output;
        
        std::array<size_t, 16> freq = {};
        for (uint8_t n : nibbles) freq[n & 0x0F]++;
        
        std::priority_queue<Node*, std::vector<Node*>, Compare> pq;
        for (int i = 0; i < 16; i++) {
            if (freq[i] > 0) pq.push(new Node(static_cast<uint8_t>(i), freq[i]));
        }
        
        if (pq.empty()) return output;
        
        while (pq.size() > 1) {
            Node* left = pq.top(); pq.pop();
            Node* right = pq.top(); pq.pop();
            pq.push(new Node(left->freq + right->freq, left, right));
        }
        
        code_lengths.fill(0);
        std::function<void(Node*, size_t)> get_lengths = [&](Node* node, size_t depth) {
            if (!node) return;
            if (!node->left && !node->right) code_lengths[node->symbol] = depth;
            else { get_lengths(node->left, depth + 1); get_lengths(node->right, depth + 1); }
        };
        get_lengths(pq.top(), 0);
        if (pq.top()->left == nullptr && pq.top()->right == nullptr) {
            code_lengths[pq.top()->symbol] = 1;
        }
        
        delete pq.top();
        build_canonical_codes();
        output.push_back(16);
        for (size_t i = 0; i < 16; i++) output.push_back(static_cast<uint8_t>(code_lengths[i]));
        const uint32_t input_size = static_cast<uint32_t>(nibbles.size());
        for (unsigned shift = 0; shift < 32; shift += 8) {
            output.push_back(static_cast<uint8_t>(input_size >> shift));
        }
        
        std::vector<uint8_t> bits;
        std::array<uint16_t, 17> next_codes = first_codes;
        std::array<uint16_t, 16> symbol_codes{};
        for (size_t symbol = 0; symbol < symbol_codes.size(); ++symbol) {
            if (code_lengths[symbol] > 0) {
                symbol_codes[symbol] = next_codes[code_lengths[symbol]]++;
            }
        }
        for (uint8_t n : nibbles) {
            uint16_t code = symbol_codes[n];
            for (size_t i = 0; i < code_lengths[n]; i++) {
                bits.push_back((code >> (code_lengths[n] - 1 - i)) & 1);
            }
        }
        
        for (size_t i = 0; i < bits.size(); i += 4) {
            uint8_t nibble = 0;
            for (size_t j = 0; j < 4 && i + j < bits.size(); j++) {
                if (bits[i + j]) nibble |= (1 << j);
            }
            output.push_back(nibble);
        }
        
        return output;
    }
    
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) override {
        if (compressed.empty()) return {};
        if (compressed.size() < 21 || compressed[0] != 16) {
            throw std::runtime_error("Invalid Huffman stream");
        }
        code_lengths.fill(0);
        for (size_t symbol = 0; symbol < 16; ++symbol) {
            code_lengths[symbol] = compressed[symbol + 1];
        }
        build_canonical_codes();
        uint32_t expected_size = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            expected_size |= static_cast<uint32_t>(compressed[17 + shift / 8]) << shift;
        }

        std::vector<uint8_t> bit_nibbles(compressed.begin() + 21, compressed.end());
        std::vector<uint8_t> output;
        output.reserve(expected_size);
        size_t nibble_pos = 0;
        size_t bit_pos = 0;
        for (uint32_t item = 0; item < expected_size; ++item) {
            uint16_t code = 0;
            bool found = false;
            for (size_t length = 1; length <= 16 && !found; ++length) {
                if (nibble_pos >= bit_nibbles.size()) {
                    throw std::runtime_error("Truncated Huffman bitstream");
                }
                const uint8_t bit = (bit_nibbles[nibble_pos] >> bit_pos) & 1;
                code = static_cast<uint16_t>((code << 1) | bit);
                if (++bit_pos == 4) {
                    bit_pos = 0;
                    ++nibble_pos;
                }
                uint16_t first = first_codes[length];
                if (code >= first && code - first < length_counts[length]) {
                    size_t offset = code - first;
                    for (size_t symbol = 0; symbol < 16; ++symbol) {
                        if (code_lengths[symbol] == length) {
                            if (offset == 0) {
                                output.push_back(static_cast<uint8_t>(symbol));
                                found = true;
                                break;
                            }
                            --offset;
                        }
                    }
                }
            }
            if (!found) throw std::runtime_error("Invalid Huffman code");
        }
        return output;
    }
    
    std::string get_name() const override { return "NibbleHuffman"; }
    BackendID get_id() const override { return BackendID::HUFFMAN; }
};

} // namespace nibble

namespace nibble {

template<typename CharT>
class NibbleEncoder {
private:
    struct AppliedTransform {
        std::vector<uint8_t> metadata;
    };

    std::vector<std::unique_ptr<ITransform<CharT>>> transforms;
    std::unique_ptr<IBackendCompressor<CharT>> backend;
    std::vector<std::string> dictionary;

    static void append_u32(std::vector<uint8_t>& output, uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) {
            output.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    static uint32_t read_u32(const std::vector<uint8_t>& input, size_t& pos) {
        if (pos > input.size() || input.size() - pos < 4) {
            throw std::runtime_error("Truncated nibble archive");
        }
        uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            value |= static_cast<uint32_t>(input[pos++]) << shift;
        }
        return value;
    }

    static bool is_word_byte(unsigned char value) {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9') || value >= 0x80;
    }

    std::vector<uint8_t> tokenize(const std::string& text) const {
        std::vector<uint8_t> output;
        for (size_t pos = 0; pos < text.size();) {
            size_t match = dictionary.size();
            for (size_t i = 0; i < dictionary.size(); ++i) {
                const std::string& word = dictionary[i];
                if (word.empty() || pos + word.size() > text.size() ||
                    text.compare(pos, word.size(), word) != 0) {
                    continue;
                }
                const bool left_boundary = pos == 0 ||
                    !is_word_byte(static_cast<unsigned char>(text[pos - 1]));
                const bool right_boundary = pos + word.size() == text.size() ||
                    !is_word_byte(static_cast<unsigned char>(text[pos + word.size()]));
                if (left_boundary && right_boundary &&
                    (match == dictionary.size() ||
                     word.size() > dictionary[match].size())) {
                    match = i;
                }
            }

            if (match != dictionary.size()) {
                output.push_back(0xFF);
                output.push_back(0x00);
                output.push_back(static_cast<uint8_t>(match & 0xFF));
                output.push_back(static_cast<uint8_t>((match >> 8) & 0xFF));
                pos += dictionary[match].size();
            } else {
                uint8_t value = static_cast<uint8_t>(text[pos++]);
                output.push_back(value);
                if (value == 0xFF) output.push_back(0xFF);
            }
        }
        return output;
    }

    std::string detokenize(const std::vector<uint8_t>& input) const {
        std::string output;
        for (size_t pos = 0; pos < input.size();) {
            uint8_t value = input[pos++];
            if (value != 0xFF) {
                output.push_back(static_cast<char>(value));
                continue;
            }
            if (pos >= input.size()) {
                throw std::runtime_error("Invalid dictionary escape");
            }
            uint8_t kind = input[pos++];
            if (kind == 0xFF) {
                output.push_back(static_cast<char>(0xFF));
            } else if (kind == 0x00 && input.size() - pos >= 2) {
                size_t index = input[pos] | (static_cast<size_t>(input[pos + 1]) << 8);
                pos += 2;
                if (index >= dictionary.size()) {
                    throw std::runtime_error("Dictionary entry is unavailable");
                }
                output += dictionary[index];
            } else {
                throw std::runtime_error("Invalid dictionary token");
            }
        }
        return output;
    }

    static std::unique_ptr<IBackendCompressor<CharT>> make_backend(BackendID id) {
        switch (id) {
            case BackendID::NONE: return nullptr;
            case BackendID::LZ77: return std::make_unique<NibbleLZ77Compressor<CharT>>();
            case BackendID::RLE: return std::make_unique<NibbleRLECompressor<CharT>>();
            case BackendID::ARITHMETIC: return std::make_unique<NibbleArithmeticCompressor<CharT>>();
            case BackendID::HUFFMAN: return std::make_unique<NibbleHuffmanCompressor<CharT>>();
        }
        throw std::runtime_error("Unknown nibble backend");
    }

public:
    void add_transform(std::unique_ptr<ITransform<CharT>> transform) {
        if (!transform) throw std::invalid_argument("Transform must not be null");
        transforms.push_back(std::move(transform));
    }

    void set_backend(std::unique_ptr<IBackendCompressor<CharT>> compressor) {
        backend = std::move(compressor);
    }

    void load_templates(const std::string&) {}

    void load_dictionary(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) throw std::runtime_error("Unable to open dictionary: " + filename);
        std::vector<std::string> loaded;
        std::string word;
        while (std::getline(file, word)) {
            if (!word.empty() && word.back() == '\r') word.pop_back();
            if (!word.empty()) loaded.push_back(std::move(word));
        }
        if (loaded.size() > 65536) {
            throw std::runtime_error("Dictionary exceeds 65536 entries");
        }
        dictionary = std::move(loaded);
    }

    std::string encode(const std::string& text) {
        std::vector<uint8_t> source = tokenize(text);
        std::vector<uint8_t> data;
        data.reserve(source.size() * 2);
        for (uint8_t byte : source) {
            data.push_back(byte >> 4);
            data.push_back(byte & 0x0F);
        }

        std::vector<AppliedTransform> applied;
        applied.reserve(transforms.size());
        for (auto& transform : transforms) {
            std::vector<uint8_t> metadata = transform->apply(data);
            applied.push_back({std::move(metadata)});
        }

        std::vector<uint8_t> payload = backend ? backend->compress(data) : data;
        const uint32_t payload_units = static_cast<uint32_t>(payload.size());
        if (backend && backend->get_id() == BackendID::ARITHMETIC) {
            payload = utils::pack_nibbles(payload);
        }
        std::vector<uint8_t> archive{'N', 'I', 'B', '2',
            static_cast<uint8_t>(backend ? backend->get_id() : BackendID::NONE)};
        if (applied.size() > 255) {
            throw std::runtime_error("Too many transforms in encoder pipeline");
        }
        archive.push_back(static_cast<uint8_t>(applied.size()));
        for (size_t i = 0; i < applied.size(); ++i) {
            archive.push_back(static_cast<uint8_t>(transforms[i]->get_id()));
            append_u32(archive, static_cast<uint32_t>(applied[i].metadata.size()));
            archive.insert(archive.end(), applied[i].metadata.begin(), applied[i].metadata.end());
        }
        append_u32(archive, payload_units);
        append_u32(archive, static_cast<uint32_t>(payload.size()));
        archive.insert(archive.end(), payload.begin(), payload.end());
        return std::string(archive.begin(), archive.end());
    }

    std::string decode(const std::string& encoded) {
        std::vector<uint8_t> archive(encoded.begin(), encoded.end());
        if (archive.size() < 14 || archive[0] != 'N' || archive[1] != 'I' ||
            archive[2] != 'B' || archive[3] != '2') {
            throw std::runtime_error("Invalid nibble archive");
        }
        size_t pos = 4;
        auto decoder = make_backend(static_cast<BackendID>(archive[pos++]));
        size_t transform_count = archive[pos++];
        std::vector<std::pair<TransformID, std::vector<uint8_t>>> metadata;
        metadata.reserve(transform_count);
        for (size_t i = 0; i < transform_count; ++i) {
            if (pos >= archive.size()) throw std::runtime_error("Truncated transform metadata");
            TransformID id = static_cast<TransformID>(archive[pos++]);
            uint32_t length = read_u32(archive, pos);
            if (length > archive.size() - pos) throw std::runtime_error("Truncated transform metadata");
            metadata.emplace_back(id, std::vector<uint8_t>(
                archive.begin() + pos, archive.begin() + pos + length));
            pos += length;
        }
        uint32_t payload_units = read_u32(archive, pos);
        uint32_t payload_size = read_u32(archive, pos);
        if (payload_size != archive.size() - pos) throw std::runtime_error("Invalid nibble payload size");
        std::vector<uint8_t> payload(archive.begin() + pos, archive.end());
        if (decoder && decoder->get_id() == BackendID::ARITHMETIC) {
            if (payload_size != (payload_units + 1) / 2) {
                throw std::runtime_error("Invalid packed arithmetic payload size");
            }
            payload = utils::unpack_nibbles(payload);
            payload.resize(payload_units);
        }
        std::vector<uint8_t> data = decoder ? decoder->decompress(payload) : std::move(payload);

        for (size_t i = metadata.size(); i > 0; --i) {
            const auto& item = metadata[i - 1];
            std::unique_ptr<ITransform<CharT>> transform;
            switch (item.first) {
                case TransformID::XOR_MEAN_NORMALIZE: transform = std::make_unique<XORMeanNormalizeTransform<CharT>>(); break;
                case TransformID::BWT: transform = std::make_unique<BWTTransform<CharT>>(); break;
                case TransformID::MTF: transform = std::make_unique<MTFTransform<CharT>>(); break;
                case TransformID::RLE: transform = std::make_unique<RLETransform<CharT>>(); break;
                case TransformID::FREQ_SORT: transform = std::make_unique<FreqSortTransform<CharT>>(); break;
                case TransformID::CONTEXT_SORT: transform = std::make_unique<ContextSortTransform<CharT>>(); break;
                case TransformID::PREDICTIVE_NIBBLE_LAG: transform = std::make_unique<PredictiveNibbleLagTransform<CharT>>(); break;
                case TransformID::HIDDEN_MARKOV_CHAIN: transform = std::make_unique<HMCTransform<CharT>>(); break;
                case TransformID::COMBINED_PREDICTOR: transform = std::make_unique<CombinedPredictorTransform<CharT>>(); break;
                case TransformID::XOR_AVT: transform = std::make_unique<XORAVTTransform<CharT>>(); break;
                case TransformID::PATTERN_MATCH: transform = std::make_unique<PatternMatchTransform<CharT>>(); break;
                case TransformID::MULTIMOVE_CHAIN: transform = std::make_unique<MultimoveChainTransform<CharT>>(); break;
                case TransformID::WAVELET_TREE: transform = std::make_unique<WaveletTreeTransform<CharT>>(); break;
                case TransformID::ADAPTIVE: transform = std::make_unique<AdaptiveTransform<CharT>>(); break;
                default: throw std::runtime_error("Unknown transform in nibble archive");
            }
            transform->reverse(data, item.second);
        }

        if (data.size() % 2 != 0) throw std::runtime_error("Invalid nibble stream length");
        std::vector<uint8_t> source;
        source.reserve(data.size() / 2);
        for (size_t i = 0; i < data.size(); i += 2) {
            if (data[i] > 0x0F || data[i + 1] > 0x0F) {
                throw std::runtime_error("Invalid nibble value");
            }
            source.push_back(static_cast<uint8_t>((data[i] << 4) | data[i + 1]));
        }
        return detokenize(source);
    }
};

// ============================================================================
// ENHANCED BUILDER WITH TRANSFORM INTEGRATION
// ============================================================================

template<typename CharT = char>
class NibbleEncoderBuilder {
private:
    std::unique_ptr<NibbleEncoder<CharT>> encoder;
    std::vector<std::unique_ptr<ITransform<CharT>>> transforms;
    std::unique_ptr<IBackendCompressor<CharT>> backend;
    
public:
    NibbleEncoderBuilder() : encoder(std::make_unique<NibbleEncoder<CharT>>()) {}
    
    // ========================================================================
    // BACKEND SELECTION
    // ========================================================================
    
    NibbleEncoderBuilder& with_lz77_backend(size_t window = LZ77_WINDOW_SIZE, 
                                             size_t lookahead = LZ77_LOOKAHEAD) {
        backend = std::make_unique<NibbleLZ77Compressor<CharT>>(window, lookahead);
        return *this;
    }
    
    NibbleEncoderBuilder& with_rle_backend() {
        backend = std::make_unique<NibbleRLECompressor<CharT>>();
        return *this;
    }
    
    NibbleEncoderBuilder& with_arithmetic_backend() {
        backend = std::make_unique<NibbleArithmeticCompressor<CharT>>();
        return *this;
    }
    
    NibbleEncoderBuilder& with_huffman_backend() {
        backend = std::make_unique<NibbleHuffmanCompressor<CharT>>();
        return *this;
    }
    
    NibbleEncoderBuilder& with_no_backend() {
        backend.reset();
        return *this;
    }
    
    // ========================================================================
    // TRANSFORM ADDITIONS
    // ========================================================================
    
    NibbleEncoderBuilder& add_xor_mean_normalize(size_t window = XOR_WINDOW_SIZE) {
        transforms.push_back(std::make_unique<XORMeanNormalizeTransform<CharT>>(window));
        return *this;
    }
    
    NibbleEncoderBuilder& add_bwt() {
        transforms.push_back(std::make_unique<BWTTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_mtf() {
        transforms.push_back(std::make_unique<MTFTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_rle() {
        transforms.push_back(std::make_unique<RLETransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_freq_sort() {
        transforms.push_back(std::make_unique<FreqSortTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_context_sort() {
        transforms.push_back(std::make_unique<ContextSortTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_pnl() {
        transforms.push_back(std::make_unique<PredictiveNibbleLagTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_hmc(size_t order = 2) {
        transforms.push_back(std::make_unique<HMCTransform<CharT>>(order));
        return *this;
    }
    
    NibbleEncoderBuilder& add_combined_predictor(size_t order = 2) {
        transforms.push_back(std::make_unique<CombinedPredictorTransform<CharT>>(order));
        return *this;
    }
    
    NibbleEncoderBuilder& add_xor_avt() {
        transforms.push_back(std::make_unique<XORAVTTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_pattern_match() {
        transforms.push_back(std::make_unique<PatternMatchTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_multimove_chain() {
        transforms.push_back(std::make_unique<MultimoveChainTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_wavelet_tree() {
        transforms.push_back(std::make_unique<WaveletTreeTransform<CharT>>());
        return *this;
    }
    
    NibbleEncoderBuilder& add_adaptive() {
        transforms.push_back(std::make_unique<AdaptiveTransform<CharT>>());
        return *this;
    }
    
    // ========================================================================
    // PREDEFINED OPTIMAL PIPELINES
    // ========================================================================
    
    NibbleEncoderBuilder& with_english_text_pipeline() {
        return add_xor_mean_normalize()
               .add_bwt()
               .add_mtf()
               .with_arithmetic_backend();
    }
    
    NibbleEncoderBuilder& with_source_code_pipeline() {
        return add_xor_avt()
               .add_bwt()
               .add_freq_sort()
               .add_combined_predictor(2)
               .with_lz77_backend();
    }
    
    NibbleEncoderBuilder& with_log_file_pipeline() {
        return add_xor_mean_normalize()
               .add_pattern_match()
               .add_multimove_chain()
               .with_rle_backend();
    }
    
    NibbleEncoderBuilder& with_structured_pipeline() {
        return add_xor_avt()
               .add_bwt()
               .add_context_sort()
               .add_pattern_match()
               .with_arithmetic_backend();
    }
    
    NibbleEncoderBuilder& with_repeating_pipeline() {
        return add_xor_avt()
               .add_bwt()
               .add_mtf()
               .add_rle()
               .with_rle_backend();
    }
    
    NibbleEncoderBuilder& with_binary_pipeline() {
        return add_xor_mean_normalize()
               .add_wavelet_tree()
               .add_combined_predictor(3)
               .with_arithmetic_backend();
    }
    
    NibbleEncoderBuilder& with_aggressive_pipeline() {
        return add_xor_avt()
               .add_bwt()
               .add_mtf()
               .add_combined_predictor(3)
               .add_pattern_match()
               .add_rle()
               .with_arithmetic_backend();
    }
    
    NibbleEncoderBuilder& with_fast_pipeline() {
        return add_xor_mean_normalize()
               .add_freq_sort()
               .with_huffman_backend();
    }
    
    NibbleEncoderBuilder& with_balanced_pipeline() {
        return add_xor_avt()
               .add_bwt()
               .add_mtf()
               .with_arithmetic_backend();
    }
    
    NibbleEncoderBuilder& with_adaptive_pipeline() {
        return add_adaptive()
               .with_arithmetic_backend();
    }
    
    // ========================================================================
    // RESOURCE LOADING
    // ========================================================================
    
    NibbleEncoderBuilder& with_template_file(const std::string& filename) {
        encoder->load_templates(filename);
        return *this;
    }
    
    NibbleEncoderBuilder& with_dictionary_file(const std::string& filename) {
        encoder->load_dictionary(filename);
        return *this;
    }
    
    // ========================================================================
    // BUILD
    // ========================================================================
    
    std::unique_ptr<NibbleEncoder<CharT>> build() {
        for (auto& t : transforms) encoder->add_transform(std::move(t));
        if (backend) encoder->set_backend(std::move(backend));
        return std::move(encoder);
    }
};

} // namespace nibble

#endif // NIBBLE_ENCODER_HPP
