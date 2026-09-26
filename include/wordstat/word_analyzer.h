// wordstat: a word-frequency CLI tool.
//
// This header is INTENTIONALLY sub-optimal. It exists as a teaching
// example: an AI coding agent (Claude Code, Codex, Copilot, Gemini)
// is meant to read this, find the algorithmic and style issues, and
// produce an improved version with benchmarks proving the
// improvement.
//
// Known issues (do not fix here; this is the "before" state):
//   - TopWords is O(n^2) via repeated linear scans for the max.
//   - Tokenize rebuilds a string one character at a time.
//   - CountWords uses a vector with linear search instead of a
//     hash map, making it O(n^2) overall.

#ifndef WORDSTAT_WORD_ANALYZER_H_
#define WORDSTAT_WORD_ANALYZER_H_

#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace wordstat {

// A single word and its occurrence count.
struct WordCount {
  std::string word;
  int count = 0;
};

// Splits text into lowercase alphabetic words.
std::vector<std::string> Tokenize(const std::string& text);

// Counts word frequencies using linear-search lookup.
std::vector<WordCount> CountWords(const std::vector<std::string>& words);

// Returns the top-n counts by frequency, descending.
std::vector<WordCount> TopWords(const std::vector<WordCount>& counts, int n);

// Runs the full analysis pipeline on a text blob.
std::vector<WordCount> Analyze(const std::string& text, int top_n);

// Parsed command-line arguments.
struct ParsedArgs {
  std::string file_path;
  int top_n = 10;
};

// Parses CLI arguments in the form: FILE [-n|--top N].
//
// Returns std::nullopt and sets `error` when arguments are missing or
// malformed, mirroring the exception-based error paths in the
// Python/C#/Java reference implementations without requiring
// exceptions here.
std::optional<ParsedArgs> ParseArgs(const std::vector<std::string>& args,
                                    std::string* error);

// Runs the CLI logic. Returns the process exit code (0 on success, 1
// on error). `out` and `err` are injectable for testability, exactly
// like the `run(args, out, err)` pattern used in the Java and C#
// reference implementations.
int Run(const std::vector<std::string>& args, std::ostream* out,
        std::ostream* err);

}  // namespace wordstat

#endif  // WORDSTAT_WORD_ANALYZER_H_
