
#include "lzhb-decode.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "lzf.hpp"

uint8_t getBit(const uint8_t& block, uint8_t pos) { return (block >> pos) & 1; }

void decodeBlock(std::vector<uint8_t>& bin_blocks,
                 std::vector<uint32_t>& val_blocks, uint32_t bitsize,
                 uint32_t tail) {
  uint32_t val = 0;
  uint32_t buff = 0;
  for (size_t i = 0; i < bin_blocks.size(); i++) {
    if (i != bin_blocks.size() - 1) {
      for (size_t j = 8; j > 0; j--) {
        val |= (getBit(bin_blocks[i], j - 1) << (bitsize - buff - 1));
        buff++;
        if (buff == bitsize) {
          val_blocks.push_back(val);
          buff = 0;
          val = 0;
        }
      }
    } else {
      for (size_t j = 8; j > 8 - tail; j--) {
        val |= (getBit(bin_blocks[i], j - 1) << (bitsize - buff - 1));
        buff++;
        if (buff == bitsize) {
          val_blocks.push_back(val);
          buff = 0;
          val = 0;
        }
      }
    }
  }
}

// The position parameter indicates if the length or the end position of the
// phrase should be saved in len. Depending on the generation method we need to
// use either one or the other.
std::vector<PhraseC> decodeToPhraseC(const std::string& filename,
                                     bool position) {
  std::vector<PhraseC> output;

  std::ifstream fs(filename, std::ifstream::binary);

  uint32_t phrase_size = 1;
  uint32_t bitsize_lp = 1;
  uint32_t bitsize_src = 1;

  fs.read((char*)&phrase_size, sizeof(uint32_t));
  fs.read((char*)&bitsize_lp, sizeof(uint32_t));
  fs.read((char*)&bitsize_src, sizeof(uint32_t));

  size_t tail_lp = 1 + ((phrase_size * bitsize_lp - 1) % 8);
  size_t tail_src = 1 + ((phrase_size * bitsize_src - 1) % 8);
  size_t size_lp = 1 + (phrase_size * bitsize_lp - 1) / 8;
  size_t size_src = 1 + (phrase_size * bitsize_src - 1) / 8;

  std::vector<uint8_t> bin_lp(size_lp);
  std::vector<uint8_t> bin_src(size_src);
  std::vector<char> phrase_c(phrase_size);
  fs.read((char*)&bin_lp[0], bin_lp.size());
  fs.read((char*)&bin_src[0], bin_src.size());
  fs.read((char*)&phrase_c[0], phrase_c.size());
  fs.close();

  std::vector<uint32_t> phrase_lp;
  std::vector<uint32_t> phrase_src;
  decodeBlock(bin_lp, phrase_lp, bitsize_lp, tail_lp);
  decodeBlock(bin_src, phrase_src, bitsize_src, tail_src);

  if (position) {
    for (size_t i = 0; i < phrase_lp.size(); i++) {
      if (i == 0) {
        output.push_back(
            PhraseC{.len = 1, .pos = phrase_src[i], .nextChar = phrase_c[i]});
      } else {
        if (phrase_lp[i] - phrase_lp[i - 1] == 1) {
          output.push_back(
              PhraseC{.len = 1, .pos = phrase_src[i], .nextChar = phrase_c[i]});
        } else {
          output.push_back(PhraseC{.len = phrase_lp[i] - phrase_lp[i - 1],
                                   .pos = phrase_src[i],
                                   .nextChar = phrase_c[i]});
        }
      }
    }
  } else {
    for (size_t i = 0; i < phrase_lp.size(); i++) {
      output.push_back(PhraseC{
          .len = phrase_lp[i], .pos = phrase_src[i], .nextChar = phrase_c[i]});
    }
  }
  return output;
}

void setBit(uint8_t& block, uint8_t bit, uint8_t pos) {
  block |= (bit << pos);
}

void encodeBlock(uint32_t bitsize, std::vector<uint8_t>& bin_blocks,
                            std::vector<uint32_t>& val_blocks) {
  uint32_t buff = 0;
  uint8_t block = 0;
  for (uint32_t i = 0; i < val_blocks.size(); i++) {
    for (uint32_t j = bitsize; j > 0; j--) {
      setBit(block, (val_blocks[i] >> (j - 1)) & 1, 7 - buff);
      buff++;
      if (buff == 8) {
        buff = 0;
        bin_blocks.push_back(block);
        block = 0;
      }
    }
  }
  if (buff != 0) {
    bin_blocks.push_back(block);
  }
}

uint32_t encodePhraseC(std::vector<PhraseC>& phrase,
                              const std::string& filename, bool output,
                              bool position) {
  std::vector<uint32_t> phrase_lp;
  std::vector<uint32_t> phrase_src;
  std::vector<char> phrase_c;
  uint32_t pos = 0;
  uint32_t max_lp = 1;
  uint32_t max_src = 1;

  for (size_t i = 0; i < phrase.size(); i++) {
    if (position) {
      pos += std::max((uint32_t)1, phrase[i].len);
      phrase_lp.push_back(pos);
    } else {
      phrase_lp.push_back(phrase[i].len);
    }
    phrase_src.push_back(phrase[i].pos);
    phrase_c.push_back(phrase[i].nextChar);
    max_lp = std::max(max_lp, phrase_lp[i]);
    max_src = std::max(max_src, phrase_src[i]);
  }
  uint32_t bitsize_lp = (uint32_t)log2((double)max_lp) + 1;
  uint32_t bitsize_src = (uint32_t)log2((double)max_src) + 1;

  std::vector<uint8_t> bin_lp;
  std::vector<uint8_t> bin_src;
  encodeBlock(bitsize_lp, bin_lp, phrase_lp);
  encodeBlock(bitsize_src, bin_src, phrase_src);

  uint32_t phrase_size = phrase.size();
  if (output) {
    std::string binfile = filename;
    if (position) {
      binfile += ".lzcp";
    } else {
      binfile += ".lzc";
    }
    std::ofstream fs(binfile, std::ofstream::binary);
    fs.write((char*)&phrase_size, sizeof(uint32_t));
    fs.write((char*)&bitsize_lp, sizeof(uint32_t));
    fs.write((char*)&bitsize_src, sizeof(uint32_t));
    fs.write((char*)&bin_lp[0], bin_lp.size());
    fs.write((char*)&bin_src[0], bin_src.size());
    fs.write((char*)&phrase_c[0], phrase.size());
    fs.close();
  }
  return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) +
         bin_lp.size() + bin_src.size() + phrase.size();
}

// ---------------------------------------------------------------------------

std::string decodePhrasesToString(const std::vector<PhraseC>& phrases,
                                  bool position) {
  if (!position) {
    std::string output;
    for (const auto& phrase : phrases) {
      uint32_t cuPos = output.length();
      for (uint32_t i = 0; i < phrase.len - 1 - cuPos; i++) {
        output.push_back(output[phrase.pos + i]);
      }
      output.push_back(phrase.nextChar);
    }
    return output;
  } else {
    std::string output;
    for (const auto& phrase : phrases) {
      for (uint32_t i = 0; i < phrase.len - 1; i++) {
        output.push_back(output[phrase.pos + i]);
      }
      output.push_back(phrase.nextChar);
    }
    return output;
  }
}

// The predecessor table holds the cumulative lengths of phrases. I.e the last
// position of a phrase i is stored at predecessortable[i] (which is the
// position of the appended char).
std::vector<uint32_t> buildPredecessorTable(
    const std::vector<PhraseC>& phrases) {
  std::vector<uint32_t> predecessortable;
  predecessortable.reserve(phrases.size());
  for (uint32_t i = 0; i < phrases.size(); ++i) {
    uint32_t prev = (predecessortable.size() > 0) ? predecessortable.back() : 0;
    predecessortable.push_back(phrases[i].len + prev);
  }
  return predecessortable;
}

// When position = false on generation. The contents of the predecessor table
// are "embedded" in the PhraseC len fields.
uint32_t binSearchPredecessor(const std::vector<PhraseC>& phrases,
                              uint32_t position) {
  if (phrases.empty()) return UINT32_MAX;
  // Iterarive binary search. I did it! Yippie!
  uint32_t low = 0;
  uint32_t high = phrases.size();
  uint32_t result = UINT32_MAX;
  while (low < high) {
    uint32_t mid = low + (high - low) / 2;
    if (phrases[mid].len <= position) {
      // this phrase ends before 'position' and is a candidate.
      result =
          position == phrases[mid].len
              ? mid
              : mid +
                    1;  // If exact match, return this index. Else out position
                        // is included in next phrase. Quirks of lzhb format.
      low = mid + 1;    // move right to find possibly later predecessor.
    } else {
      high = mid;  // move left to earlier phrases.
    }
  }

  return result;
}

// When position = true on generation. We need to calculate the predecessor
// table beforehand.
uint32_t binSearchPredecessorT(const std::vector<PhraseC>& phrases,
                               const std::vector<uint32_t>& predecessortable,
                               uint32_t position) {
  if (phrases.empty()) return UINT32_MAX;
  uint32_t low = 0;
  uint32_t high = predecessortable.size();
  uint32_t result = UINT32_MAX;
  while (low < high) {
    uint32_t mid = low + (high - low) / 2;
    if (predecessortable[mid] <= position) {
      result =
          position == predecessortable[mid]
              ? mid
              : mid + 1;// If exact match, return this index. Else our character
                        // is included in next phrase. Quirks of lzhb format.
      low = mid + 1;
    } else {
      high = mid;  // move left to earlier phrases.
    }
  }
  return result;
}

char getPositionFromPhrases(const std::vector<PhraseC>& phrases,
                            uint32_t position, int* height) {
  uint32_t idx = binSearchPredecessor(phrases, position);
  if (idx == UINT32_MAX) return 0;  // not found, return NUL as a fallback

  const PhraseC& predecessor = phrases[idx];
  uint32_t prevEnd = (idx == 0) ? 0 : phrases[idx - 1].len;

  // We found the char when the position is exactly the phrase's final position.
  if (position == predecessor.len) {
    return predecessor.nextChar;
  }

  // Otherwise the character lies between the lastPos of our predecessor and the
  // previous phrase's end. The new Position has the same offset from the source
  // position as from the beginning of the copied region.
  if (position > prevEnd && position < predecessor.len) {
    uint32_t offset = position - prevEnd;
    uint32_t newPos = predecessor.pos + offset;
    if (height) (*height)++;
    return getPositionFromPhrases(phrases, newPos, height);
  }
  return 0;
}

char getPositionFromPhrasesT(const std::vector<PhraseC>& phrases,
                             const std::vector<uint32_t>& predecessortable,
                             uint32_t position, int* height) {
  uint32_t idx = binSearchPredecessorT(phrases, predecessortable, position);
  if (idx == UINT32_MAX) return 0;

  const PhraseC& predecessor = phrases[idx];
  uint32_t prevEnd =
      (idx == 0) ? 0 : predecessortable[idx - 1];  // cumulative length

  // We found the char when the position is exactly the phrase's final position.
  if (position == predecessortable[idx]) {
    return predecessor.nextChar;
  }

  // Otherwise the character lies between the lastPos of our predecessor and the
  // previous phrase's end. The new Position has the same offset from the source
  // position as from the beginning of the copied region.
  if (position > prevEnd && position < predecessortable[idx]) {
    uint32_t offset = position - prevEnd;
    uint32_t newPos = predecessor.pos + offset;
    if (height) (*height)++;
    return getPositionFromPhrasesT(phrases, predecessortable, newPos, height);
  }
  return 0;
}

void printPhrase(const PhraseC& phrase) {
  std::cout << "(" << phrase.len << "," << phrase.pos << "," << phrase.nextChar
            << ")" << std::endl;
}
