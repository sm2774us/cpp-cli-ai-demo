#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "wordstat/benchmark.h"

// LCOV_EXCL_START
// Trivial wiring only: converts argv to a vector and prints the
// result. All real logic lives in ParseBenchmarkArgs/RunBenchmark
// above, which are fully unit tested; this wrapper is excluded from
// the coverage gate the same way main.cc's launcher is, since gtest
// cannot link a second `main` symbol into the test binary to exercise
// this one directly.
int main(int argc, char** argv) {
  int words = 20000;
  int vocab = 15000;

  std::vector<std::string> args(argv + 1, argv + argc);
  wordstat::ParseBenchmarkArgs(args, &words, &vocab);

  double elapsed = wordstat::RunBenchmark(words, vocab, 10);
  std::cout << "words=" << words << " vocab=" << vocab
            << " elapsed_seconds=" << std::fixed << std::setprecision(4)
            << elapsed << "\n";
  return 0;
}
// LCOV_EXCL_STOP
