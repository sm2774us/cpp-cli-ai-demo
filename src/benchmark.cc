// Benchmarks wordstat::Analyze on a generated corpus.
//
// IMPORTANT: Tokenize() keeps only alphabetic characters, so a
// vocabulary like "word0".."word9999" would collapse into a single
// token ("word") after tokenization -- silently defeating --vocab
// entirely. This generator instead builds distinct alphabetic-only
// words (bijective base-26, like spreadsheet column names: a, b, ...,
// z, aa, ab, ...), so the requested vocabulary size is the actual
// vocabulary size. This is the exact bug found and fixed in the
// Python reference implementation's benchmark.py, carried forward
// here from the start.
//
// Usage: ./benchmark --words 20000 --vocab 15000

#include "wordstat/benchmark.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "wordstat/word_analyzer.h"

namespace wordstat {

std::string IndexToWord(int index) {
  std::string letters;
  int n = index + 1;
  while (n > 0) {
    n -= 1;
    letters.push_back(static_cast<char>('a' + (n % 26)));
    n /= 26;
  }
  std::reverse(letters.begin(), letters.end());
  return letters;
  // gcov marks a brace after `return` as unreached (nothing executes
  // past `return`); not a real coverage gap.
}  // LCOV_EXCL_LINE

std::string MakeCorpus(int word_count, int vocab_size, unsigned int seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(0, vocab_size - 1);

  std::vector<std::string> vocab;
  vocab.reserve(vocab_size);
  for (int i = 0; i < vocab_size; ++i) {
    vocab.push_back(IndexToWord(i));
  }

  std::ostringstream out;
  for (int i = 0; i < word_count; ++i) {
    if (i > 0) {
      out << ' ';
    }
    out << vocab[dist(rng)];
  }
  return out.str();
}

double RunBenchmark(int word_count, int vocab_size, int top_n) {
  std::string text = MakeCorpus(word_count, vocab_size, 42U);
  auto start = std::chrono::steady_clock::now();
  Analyze(text, top_n);
  auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(end - start).count();
}

void ParseBenchmarkArgs(const std::vector<std::string>& args, int* words,
                        int* vocab) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--words" && i + 1 < args.size()) {
      *words = std::atoi(args[i + 1].c_str());
      ++i;
    } else if (args[i] == "--vocab" && i + 1 < args.size()) {
      *vocab = std::atoi(args[i + 1].c_str());
      ++i;
    }
  }
}

}  // namespace wordstat
