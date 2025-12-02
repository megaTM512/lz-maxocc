#ifndef LZHB_DECODE_HPP
#define LZHB_DECODE_HPP

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "lzf.hpp"

uint8_t getBit(const uint8_t& block, uint8_t pos);
void decodeBlock(std::vector<uint8_t>& bin_blocks,
                 std::vector<uint32_t>& val_blocks, uint32_t bitsize,
                 uint32_t tail);
std::vector<PhraseC> decodeToPhraseC(const std::string& filename,
                                     bool position = true);
std::string decodePhrasesToString(const std::vector<PhraseC>& phrases,
                                  bool position = true);
void setBit(uint8_t& block, uint8_t bit, uint8_t pos);

void encodeBlock(uint32_t bitsize, std::vector<uint8_t>& bin_blocks,
                 std::vector<uint32_t>& val_blocks);
uint32_t encodePhraseC(std::vector<PhraseC>& phrase,
                       const std::string& filename, bool output, bool position);
std::vector<uint32_t> buildPredecessorTable(
    const std::vector<PhraseC>& phrases);
uint32_t binSearchPredecessor(const std::vector<PhraseC>& phrases,
                              uint32_t position);
uint32_t binSearchPredecessorT(const std::vector<PhraseC>& phrases,
                               const std::vector<uint32_t>& predecessortable,
                               uint32_t position);
char getPositionFromPhrases(const std::vector<PhraseC>& phrases,
                            uint32_t position, int* height = nullptr);
char getPositionFromPhrasesT(const std::vector<PhraseC>& phrases,
                             const std::vector<uint32_t>& predecessortable,
                             uint32_t position, int* height = nullptr);
std::vector<uint32_t> buildPredecessorTable(
    const std::vector<PhraseC>& phrases);
void printPhrase(const PhraseC& phrase);

#endif  // LZHB_DECODE_HPP