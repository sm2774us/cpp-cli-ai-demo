#include "wordstat/benchmark.h"

#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace wordstat {
namespace {

TEST(IndexToWordTest, ProducesDistinctAlphabeticWords) {
  EXPECT_EQ(IndexToWord(0), "a");
  EXPECT_EQ(IndexToWord(25), "z");
  EXPECT_EQ(IndexToWord(26), "aa");
  EXPECT_EQ(IndexToWord(27), "ab");
}

TEST(IndexToWordTest, AllValuesAreDistinct) {
  std::set<std::string> seen;
  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(seen.insert(IndexToWord(i)).second) << "duplicate at " << i;
  }
}

TEST(MakeCorpusTest, ProducesRequestedWordCount) {
  std::string corpus = MakeCorpus(50, 10, 1U);
  int spaces = 0;
  for (char ch : corpus) {
    if (ch == ' ') {
      ++spaces;
    }
  }
  EXPECT_EQ(spaces, 49);
}

TEST(MakeCorpusTest, IsDeterministicForSameSeed) {
  EXPECT_EQ(MakeCorpus(20, 5, 7U), MakeCorpus(20, 5, 7U));
}

TEST(RunBenchmarkTest, ReturnsNonNegativeElapsedSeconds) {
  EXPECT_GE(RunBenchmark(100, 20, 5), 0.0);
}

TEST(ParseBenchmarkArgsTest, ParsesBothFlags) {
  int words = 0;
  int vocab = 0;
  ParseBenchmarkArgs({"--words", "50", "--vocab", "10"}, &words, &vocab);
  EXPECT_EQ(words, 50);
  EXPECT_EQ(vocab, 10);
}

TEST(ParseBenchmarkArgsTest, NoArgsKeepsDefaults) {
  int words = 20000;
  int vocab = 15000;
  ParseBenchmarkArgs({}, &words, &vocab);
  EXPECT_EQ(words, 20000);
  EXPECT_EQ(vocab, 15000);
}

TEST(ParseBenchmarkArgsTest, IgnoresUnrecognizedFlag) {
  int words = 20000;
  int vocab = 15000;
  ParseBenchmarkArgs({"--unknown"}, &words, &vocab);
  EXPECT_EQ(words, 20000);
  EXPECT_EQ(vocab, 15000);
}

// This is the exact branch missed in the Java reference implementation
// (Benchmark.main's `args[i].equals("--words") && i + 1 < args.length`)
// that broke its 100% coverage gate after the JaCoCo upgrade -- a
// recognized flag with no trailing value. Covered here from the start.
TEST(ParseBenchmarkArgsTest, WordsFlagWithNoTrailingValueIsIgnored) {
  int words = 20000;
  int vocab = 15000;
  ParseBenchmarkArgs({"--words"}, &words, &vocab);
  EXPECT_EQ(words, 20000);
  EXPECT_EQ(vocab, 15000);
}

TEST(ParseBenchmarkArgsTest, VocabFlagWithNoTrailingValueIsIgnored) {
  int words = 20000;
  int vocab = 15000;
  ParseBenchmarkArgs({"--vocab"}, &words, &vocab);
  EXPECT_EQ(words, 20000);
  EXPECT_EQ(vocab, 15000);
}

}  // namespace
}  // namespace wordstat
