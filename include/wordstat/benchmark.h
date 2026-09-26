// Benchmark harness declarations. See benchmark_main.cc for the
// tokenize-safe corpus generator rationale.
#ifndef WORDSTAT_BENCHMARK_H_
#define WORDSTAT_BENCHMARK_H_

#include <string>
#include <vector>

namespace wordstat {

// Converts a non-negative index to a unique lowercase-letter word.
std::string IndexToWord(int index);

// Builds a synthetic corpus with a genuinely distinct vocabulary.
std::string MakeCorpus(int word_count, int vocab_size, unsigned int seed);

// Times a single Analyze() call. Returns elapsed wall-clock seconds.
double RunBenchmark(int word_count, int vocab_size, int top_n);

// Parses "--words N --vocab N" style arguments, updating *words and
// *vocab in place. Unrecognized or value-less trailing flags are
// silently ignored (defaults are kept), matching the Python/C#/Java
// reference implementations' benchmark CLI behavior.
void ParseBenchmarkArgs(const std::vector<std::string>& args, int* words,
                        int* vocab);

}  // namespace wordstat

#endif  // WORDSTAT_BENCHMARK_H_
