#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cxxopts.hpp"
#include "libsais64.h"
#include "lzf.hpp"
#include "lzhb-decode.hpp"
#include "segtree.hpp"

static inline uint32_t _e() { return 0; }
static inline uint32_t _max(uint32_t a, uint32_t b) { return std::max(a, b); }

// Extract Phrase
std::string extractStringFromPhrase(const PhraseC& phrase,
                                    const std::string& text) {
  return text.substr(phrase.pos, phrase.len);
}

int compareSuffixWithPattern(const std::string& text, std::int64_t suffixPos,
                             const std::string& pattern) {
  const std::int64_t n = static_cast<std::int64_t>(text.size());
  const std::int64_t m = static_cast<std::int64_t>(pattern.size());
  if (suffixPos < 0 || suffixPos > n) {
    // out of range - treat as smaller than any non-empty pattern (or you may
    // throw)
    return -1;
  }

  // number of bytes to compare
  std::int64_t common = std::min(m, n - suffixPos);
  if (common > 0) {
    // compare bytes safely as unsigned char
    const unsigned char* a =
        reinterpret_cast<const unsigned char*>(text.data() + suffixPos);
    const unsigned char* b =
        reinterpret_cast<const unsigned char*>(pattern.data());
    int cmp = std::memcmp(a, b, static_cast<std::size_t>(common));
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
  }

  // if all compared bytes equal, decide by lengths:
  if (m == 0) return 0;  // empty pattern matches anywhere
  if (n - suffixPos >= m) {
    // suffix has at least m bytes and first m bytes equal -> suffix starts with
    // pattern
    return 0;
  }
  // suffix exhausted before pattern -> suffix < pattern
  return -1;
}

// Binary search for the first occurrence of pattern in text
int64_t findFirstMatch(const std::string& text, const std::vector<int64_t>& SA,
                       const std::string& pattern) {
  int64_t left = 0, right = static_cast<int64_t>(SA.size()) - 1;
  int64_t first = -1;

  while (left <= right) {
    int64_t mid = (left + right) / 2;
    int cmp = compareSuffixWithPattern(text, SA[mid], pattern);

    if (cmp >= 0) {
      if (cmp == 0) first = mid;
      right = mid - 1;
    } else {
      left = mid + 1;
    }
  }
  return first;
}

// Binary search for the last occurrence of pattern in text
int64_t findLastMatch(const std::string& text, const std::vector<int64_t>& SA,
                      const std::string& pattern) {
  int64_t left = 0, right = static_cast<int64_t>(SA.size()) - 1;
  int64_t last = -1;

  while (left <= right) {
    int64_t mid = (left + right) / 2;
    int cmp = compareSuffixWithPattern(text, SA[mid], pattern);

    if (cmp <= 0) {
      if (cmp == 0) last = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  return last;
}

// Double Binary Search to first find the range of occurrences
std::pair<int64_t, int64_t> findFirstAndLastOccurrence(
    const PhraseC& phrase, const std::string& text,
    const std::vector<int64_t>& SA) {
  std::string pattern = extractStringFromPhrase(phrase, text);

  int64_t L = findFirstMatch(text, SA, pattern);
  if (L == -1) return std::make_pair(-1, -1);

  int64_t R = findLastMatch(text, SA, pattern);
  if (R == -1) return std::make_pair(-1, -1);
  return std::make_pair(L, R);
}

std::vector<int> heightAnalysis(const std::vector<PhraseC>& phrases) {
  std::vector<int> heights;

  for (uint32_t i = 0; i < phrases.size(); i++) {
    const auto& p = phrases[i];

    // Literal
    if (p.len == 1) {
      heights.push_back(0);
      continue;
    }

    auto phrase_start = heights.size();
    for (uint32_t j = 0; j < p.len - 1; j++) {
      if (p.pos + j >= phrase_start) {
        heights.push_back(heights[p.pos + j]);  // Self-Reference
      } else
        heights.push_back(heights[p.pos + j] + 1);
    }
    // appended literal
    heights.push_back(0);
  }
  return heights;
}

std::vector<PhraseC> lzmaxocc(std::vector<PhraseC>& phrases) {
  // Phrase Decoding
  std::string text = decodePhrasesToString(phrases);
  atcoder::segtree<uint32_t, _max, _e> h(text.size());
  auto heights = heightAnalysis(phrases);
  for (size_t i = 0; i < text.size(); ++i) {
    h.set(i, heights[i]);
  }
  // Suffix Array construction
  const uint8_t* T = reinterpret_cast<const uint8_t*>(text.data());
  size_t n = text.size();
  std::vector<int64_t> SA(n);
  int64_t ret = libsais64(T, SA.data(), n, 0, nullptr);

  if (ret != 0) {
    std::cerr << "Error constructing suffix array." << std::endl;
    return {};
  }
  // Find occurence with minimal max height.
  for (auto& phrase : phrases) {
    auto [L, R] = findFirstAndLastOccurrence(phrase, text, SA);
    if (L == -1 || R == -1) {
      continue;  // No occurrence found
    }
    uint32_t minHeight = UINT32_MAX;
    uint32_t bestPos = phrase.pos;
    // For every occurrence, check the max height & find the minimal one
    for (auto occIdx = L; occIdx <= R; ++occIdx) {
      uint32_t occPos = static_cast<uint32_t>(SA[occIdx]);
      if (occPos >= phrase.pos) continue;  // Only consider previous occurrences
      uint32_t curHeight = h.prod(occPos, occPos + phrase.len - 1);
      if (curHeight < minHeight) {
        minHeight = curHeight;
        bestPos = occPos;
      }
    }
    if (bestPos == phrase.pos) continue;  // No change
    // Update new heights in segment tree (Does the whole height array need an
    // update?)
    for (uint32_t j = 0; j < phrase.len - 1; j++) {
      uint32_t pos = bestPos + j;
      if (pos >= phrases.size()) break;
      uint32_t newHeight = (j + 1 < phrase.len - 1) ? heights[pos] : 0;
      h.set(pos, newHeight);
    }
    phrase.pos = bestPos;
  }
  return phrases;
}

int main(int argc, char* argv[]) {
  cxxopts::Options options("lz-maxocc",
                           "LZ-MAXOCC Compression Enhancement Tool");
  options.add_options()("i,input", "Input LZHB compressed file",
                        cxxopts::value<std::string>())(
      "o,output", "Output LZHB compressed file",
      cxxopts::value<std::string>()->default_value(
          "output/lzmaxocc_output.lzcp"));
  auto result = options.parse(argc, argv);

  std::string inputFile = result["input"].as<std::string>();
  std::string outputFile = result["output"].as<std::string>();

  std::cout << "LZ-MAXOCC Compression Enhancement Tool" << std::endl;
  auto inputPhrases = decodeToPhraseC(inputFile);
  std::string inputText = decodePhrasesToString(inputPhrases);
  std::cout << "Read " << inputPhrases.size() << " phrases from input."
            << std::endl;

  // Height Analysis Before

  auto oldHeights = heightAnalysis(inputPhrases);
  double averageHeightBefore = 0;
  uint64_t maxHeightBefore = 0;
  for (uint64_t h : oldHeights) {
    averageHeightBefore += h;
    if (h > maxHeightBefore) {
      maxHeightBefore = h;
    }
  }
  averageHeightBefore /= oldHeights.size();
  double varianceHeightBefore = 0;
  for (uint64_t h : oldHeights) {
    varianceHeightBefore +=
        (h - averageHeightBefore) * (h - averageHeightBefore);
  }
  varianceHeightBefore /= oldHeights.size();
  std::cout << "Average height before: " << averageHeightBefore << std::endl;
  std::cout << "Max height before: " << maxHeightBefore << std::endl;
  std::cout << "Variance height before: " << varianceHeightBefore << std::endl;

  // Timing
  auto start = std::chrono::high_resolution_clock::now();
  auto outputPhrases = lzmaxocc(inputPhrases);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "LZ-MAXOCC processing time: " << elapsed.count() << " seconds."
            << std::endl;

  std::cout << "Generated " << outputPhrases.size() << " output phrases."
            << std::endl;

  std::string outputText = decodePhrasesToString(outputPhrases);
  if (inputText != outputText) {
    std::cerr << "Error: Decoded texts do not match!" << std::endl;
    return 1;
  }
  std::cout << "Success: Decoded texts match!" << std::endl;
  encodePhraseC(outputPhrases, outputFile, true, true);
  auto newHeights = heightAnalysis(outputPhrases);
  double averageHeightAfter = 0;
  uint64_t maxHeightAfter = 0;
  for (uint64_t h : newHeights) {
    averageHeightAfter += h;
    if (h > maxHeightAfter) {
      maxHeightAfter = h;
    }
  }
  averageHeightAfter /= newHeights.size();
  double varianceHeightAfter = 0;
  for (uint64_t h : newHeights) {
    varianceHeightAfter += (h - averageHeightAfter) * (h - averageHeightAfter);
  }
  varianceHeightAfter /= newHeights.size();
  std::cout << "Average height after: " << averageHeightAfter << std::endl;
  std::cout << "Max height after: " << maxHeightAfter << std::endl;
  std::cout << "Variance height after: " << varianceHeightAfter << std::endl;

  // Save to CSV
  {
    if (!std::filesystem::exists("./lz-maxocc_results.csv")) {
      std::fstream resultcsv("./lz-maxocc_results.csv",
                             std::ios::out | std::ios::app);
      resultcsv << "Timestamp,"
                << "Algorithm,"
                << "Input File,"
                << "Number of Phrases,"
                << "Average Height Before,"
                << "Max Height Before,"
                << "Variance Height Before,"
                << "Average Height After,"
                << "Max Height After,"
                << "Variance Height After,"
                << "Elapsed Time (s)" << std::endl;
      resultcsv.close();
    }

    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&now_time);  // Has newline at end
    timestamp.erase(std::remove(timestamp.begin(), timestamp.end(), '\n'),
                    timestamp.end());
    std::fstream resultcsv("./lz-maxocc_results.csv",
                           std::ios::out | std::ios::app);
    resultcsv << timestamp << "," << "LZ-MAXOCC" << "," << inputFile << ","
              << inputPhrases.size() << "," << averageHeightBefore << ","
              << maxHeightBefore << "," << varianceHeightBefore << ","
              << averageHeightAfter << "," << maxHeightAfter << ","
              << varianceHeightAfter << "," << elapsed.count() << std::endl;
    resultcsv.close();
  }
  return 0;
}