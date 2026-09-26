#include "wordstat/word_analyzer.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace wordstat {
namespace {

TEST(TokenizeTest, BasicTextReturnsLowercaseWords) {
  std::vector<std::string> expected = {"hello", "world", "hello", "again"};
  EXPECT_EQ(Tokenize("Hello, world! Hello again."), expected);
}

TEST(TokenizeTest, EmptyStringReturnsEmptyList) {
  EXPECT_TRUE(Tokenize("").empty());
}

TEST(TokenizeTest, NoTrailingDelimiterReturnsWord) {
  std::vector<std::string> expected = {"abc"};
  EXPECT_EQ(Tokenize("abc"), expected);
}

TEST(TokenizeTest, OnlyDelimitersReturnsEmptyList) {
  EXPECT_TRUE(Tokenize("!!! ,,, ...").empty());
}

TEST(CountWordsTest, BasicCountsCorrectly) {
  std::vector<std::string> words = {"a", "b", "a", "c", "b", "a"};
  std::vector<WordCount> counts = CountWords(words);
  int count_a = 0;
  int count_b = 0;
  int count_c = 0;
  for (const WordCount& entry : counts) {
    if (entry.word == "a") {
      count_a = entry.count;
    } else if (entry.word == "b") {
      count_b = entry.count;
    } else if (entry.word == "c") {
      count_c = entry.count;
    }
  }
  EXPECT_EQ(count_a, 3);
  EXPECT_EQ(count_b, 2);
  EXPECT_EQ(count_c, 1);
}

TEST(CountWordsTest, EmptyReturnsEmptyList) {
  EXPECT_TRUE(CountWords({}).empty());
}

TEST(TopWordsTest, BasicReturnsDescendingByCount) {
  std::vector<WordCount> counts = {{"a", 3}, {"b", 5}, {"c", 1}};
  std::vector<WordCount> top = TopWords(counts, 2);
  ASSERT_EQ(top.size(), 2U);
  EXPECT_EQ(top[0].word, "b");
  EXPECT_EQ(top[1].word, "a");
}

TEST(TopWordsTest, CountLargerThanListReturnsAll) {
  std::vector<WordCount> counts = {{"a", 1}};
  std::vector<WordCount> top = TopWords(counts, 5);
  EXPECT_EQ(top.size(), 1U);
}

TEST(TopWordsTest, CountZeroReturnsEmpty) {
  std::vector<WordCount> counts = {{"a", 1}};
  EXPECT_TRUE(TopWords(counts, 0).empty());
}

TEST(TopWordsTest, DoesNotMutateInput) {
  std::vector<WordCount> counts = {{"a", 1}, {"b", 2}};
  TopWords(counts, 1);
  ASSERT_EQ(counts.size(), 2U);
  EXPECT_EQ(counts[0].word, "a");
}

TEST(AnalyzeTest, EndToEndReturnsExpectedTopWords) {
  std::string text = "the cat sat on the mat the cat ran";
  std::vector<WordCount> result = Analyze(text, 2);
  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].word, "the");
  EXPECT_EQ(result[0].count, 3);
  EXPECT_EQ(result[1].word, "cat");
  EXPECT_EQ(result[1].count, 2);
}

TEST(ParseArgsTest, DefaultsUsesTopTen) {
  std::string error;
  std::optional<ParsedArgs> parsed = ParseArgs({"file.txt"}, &error);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->file_path, "file.txt");
  EXPECT_EQ(parsed->top_n, 10);
}

TEST(ParseArgsTest, CustomTopUsesGivenValue) {
  std::string error;
  std::optional<ParsedArgs> parsed = ParseArgs({"file.txt", "-n", "3"}, &error);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->top_n, 3);
}

TEST(ParseArgsTest, LongOptionUsesGivenValue) {
  std::string error;
  std::optional<ParsedArgs> parsed =
      ParseArgs({"file.txt", "--top", "7"}, &error);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->top_n, 7);
}

TEST(ParseArgsTest, MissingValueForTopFails) {
  std::string error;
  std::optional<ParsedArgs> parsed = ParseArgs({"file.txt", "-n"}, &error);
  EXPECT_FALSE(parsed.has_value());
  EXPECT_NE(error.find("missing value"), std::string::npos);
}

TEST(ParseArgsTest, InvalidIntegerForTopFails) {
  std::string error;
  std::optional<ParsedArgs> parsed =
      ParseArgs({"file.txt", "-n", "abc"}, &error);
  EXPECT_FALSE(parsed.has_value());
  EXPECT_NE(error.find("invalid integer"), std::string::npos);
}

TEST(ParseArgsTest, TrailingGarbageAfterDigitsFails) {
  std::string error;
  std::optional<ParsedArgs> parsed =
      ParseArgs({"file.txt", "-n", "3x"}, &error);
  EXPECT_FALSE(parsed.has_value());
  EXPECT_NE(error.find("invalid integer"), std::string::npos);
}

TEST(ParseArgsTest, UnexpectedExtraArgumentFails) {
  std::string error;
  std::optional<ParsedArgs> parsed = ParseArgs({"file.txt", "extra"}, &error);
  EXPECT_FALSE(parsed.has_value());
  EXPECT_NE(error.find("unexpected argument"), std::string::npos);
}

TEST(ParseArgsTest, MissingFileFails) {
  std::string error;
  std::optional<ParsedArgs> parsed = ParseArgs({}, &error);
  EXPECT_FALSE(parsed.has_value());
  EXPECT_NE(error.find("missing required argument"), std::string::npos);
}

class TempFile {
 public:
  explicit TempFile(const std::string& contents) {
    std::filesystem::path dir = std::filesystem::temp_directory_path();
    path_ = (dir /
             ("wordstat_test_" +
              std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".txt"))
                .string();
    std::ofstream file(path_);
    file << contents;
  }

  ~TempFile() { std::remove(path_.c_str()); }

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

TEST(RunTest, SuccessPrintsTopWords) {
  TempFile file("dog dog cat");
  std::ostringstream out;
  std::ostringstream err;

  int exit_code = wordstat::Run({file.path(), "-n", "2"}, &out, &err);

  EXPECT_EQ(exit_code, 0);
  EXPECT_NE(out.str().find("dog\t2"), std::string::npos);
  EXPECT_NE(out.str().find("cat\t1"), std::string::npos);
}

TEST(RunTest, FileNotFoundReturnsOneAndPrintsError) {
  std::ostringstream out;
  std::ostringstream err;

  int exit_code =
      wordstat::Run({"/nonexistent/does_not_exist.txt"}, &out, &err);

  EXPECT_EQ(exit_code, 1);
  EXPECT_NE(err.str().find("error: file not found"), std::string::npos);
}

TEST(RunTest, BadArgumentsReturnsOneAndPrintsError) {
  std::ostringstream out;
  std::ostringstream err;

  int exit_code = wordstat::Run({}, &out, &err);

  EXPECT_EQ(exit_code, 1);
  EXPECT_NE(err.str().find("error:"), std::string::npos);
}

}  // namespace
}  // namespace wordstat
