# nibble_encoder

`nibble_encoder` is a header-only C++17 library for encoding and decoding
UTF-8 text with optional dictionary tokenization, reversible nibble transforms,
and compression backends. It preserves input bytes exactly; it does not
normalize Unicode or validate UTF-8. Archives are binary data and may contain
null bytes. This is a compression format, not encryption.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nibble_encoder_tests
```

The test executable prints measured archive sizes and ratios for five
languages, multiple sample sizes, and the predefined pipelines.

## Quick start

```cpp
#include "nib.hpp"

#include <string>

int main() {
    auto encoder = nibble::NibbleEncoderBuilder<>()
        .with_english_text_pipeline()
        .build();

    const std::string text = "The café is open. ¡La cafetería está abierta!";
    const std::string archive = encoder->encode(text);
    const std::string restored = encoder->decode(archive);
    return restored == text ? 0 : 1;
}
```

`encode` returns a binary archive in a `std::string`; store and transmit it as
bytes, not as line-oriented text. Decoding requires the complete archive.

### Dictionary

Dictionary files contain one exact UTF-8 word per line. Configure the same
dictionary contents in the same order for encoding and decoding:

```cpp
auto encoder = nibble::NibbleEncoderBuilder<>()
    .with_dictionary_file("words.txt")
    .with_english_text_pipeline()
    .build();
```

Entries are case-sensitive and matched at word boundaries. Dictionary
references are not embedded in the archive; a decoder without the matching
dictionary cannot resolve them.

## Compression options

Choose a backend with one builder method:

| Method | Backend |
|---|---|
| `with_arithmetic_backend()` | Adaptive range coding |
| `with_huffman_backend()` | Canonical Huffman coding |
| `with_lz77_backend(window, lookahead)` | Sliding-window matches |
| `with_rle_backend()` | Run-length encoding |
| `with_no_backend()` | No backend compression |

Transforms run in the order added; decoding reverses them in reverse order.
Available methods include `add_xor_mean_normalize`, `add_bwt`, `add_mtf`,
`add_rle`, `add_freq_sort`, `add_context_sort`, `add_pnl`, `add_hmc`,
`add_combined_predictor`, `add_xor_avt`, `add_pattern_match`,
`add_multimove_chain`, `add_wavelet_tree`, and `add_adaptive`.

Convenience pipelines are `with_english_text_pipeline`,
`with_source_code_pipeline`, `with_log_file_pipeline`, `with_structured_pipeline`,
`with_repeating_pipeline`, `with_binary_pipeline`, `with_aggressive_pipeline`,
`with_fast_pipeline`, `with_balanced_pipeline`, and `with_adaptive_pipeline`.
Results depend on the input; benchmark the data and pipeline you plan to use.
The English pipeline uses XOR mean normalization, BWT, MTF, and arithmetic
coding. It avoids per-symbol context metadata while retaining effective
compression for the tested text samples.

## Compression statistics

The table compares the previous English pipeline with the current one. The
ratio is **complete archive bytes / original UTF-8 input bytes**; lower is
better. “Saved” is the reduction in bytes compared with the input. The test
dictionary and corpus are defined in `tests/nibble_encoder_tests.cpp`.

| Language | Sample | Previous archive/input | Current archive/input | Current bytes saved |
|---|---:|---:|---:|---:|
| English | medium | 97.3% | 31.3% | 68.7% |
| Spanish | medium | 75.1% | 21.2% | 78.8% |
| French | medium | 89.5% | 25.4% | 74.6% |
| German | medium | 93.2% | 27.3% | 72.7% |
| Portuguese | medium | 90.3% | 28.4% | 71.6% |
| English | large | 51.9% | 2.5% | 97.5% |
| Spanish | large | 43.2% | 1.7% | 98.3% |
| French | large | 48.4% | 2.0% | 98.0% |
| German | large | 51.0% | 2.1% | 97.9% |
| Portuguese | large | 46.9% | 2.2% | 97.8% |

Medium samples repeat a short paragraph 12 times; large samples repeat it 250
times. The current pipeline's archive framing is 33 bytes for these samples
(29 fixed bytes plus 4 bytes of BWT metadata), compared with hundreds or
thousands of bytes of metadata in the previous pipeline. Short samples are
still larger than their source because fixed archive framing and backend
overhead dominate. Repetitive test data is not representative of every corpus;
random, high-entropy, and very short inputs may expand.

An archive contains a six-byte signature/backend/transform prefix, each
transform ID and its metadata, two four-byte size fields, and the compressed
payload. Accordingly, payload/input alone is not the complete compression
ratio. The `NIB2` archive is binary and should be handled as bytes.

## Limitations

- The decoder needs the same dictionary in the same order.
- UTF-8 is preserved byte-for-byte; there is no normalization or
  language-aware casing.
- Transforms and backends can expand small or high-entropy inputs.
- `WaveletTreeTransform` is experimental and currently stores tree metadata
  without changing the input values.
- Template-file loading is unsupported.
- `CharacterMapper` is a 4-bit helper, not a Unicode mapper;
  `ResourceLoader` is a legacy placeholder.

Copyright (c) 2026 Anthony Matarazzo
