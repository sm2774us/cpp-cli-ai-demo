// CLI launcher.
//
// Deliberately kept to a single delegating call: wordstat::Run holds
// all real logic and is fully unit tested via injectable streams;
// this file only wires it to the real process argv/stdout/stderr.

#include <iostream>
#include <string>
#include <vector>

#include "wordstat/word_analyzer.h"

// LCOV_EXCL_START
// Trivial wiring only: converts argv to a vector and delegates to
// Run(), which is fully unit tested via injectable streams. Excluded
// from the coverage gate because gtest cannot link a second `main`
// symbol into the test binary to exercise this one directly.
int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  return wordstat::Run(args, &std::cout, &std::cerr);
}
// LCOV_EXCL_STOP
