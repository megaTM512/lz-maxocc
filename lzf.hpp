#ifndef LZFACTORIZATION
#define LZFACTORIZATION

#include <vector>
#include<cstdint>

struct Phrase{
  uint32_t len;
  uint32_t pos; // char when len <= 1
};

struct PhraseC{ //
  uint32_t len;
  uint32_t pos;
  char nextChar;
};

class lzf{
  public:
    std::vector<Phrase> phrases;
    std::vector<int> predecessors;
};

#endif