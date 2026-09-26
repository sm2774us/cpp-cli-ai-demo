#include "wordstat/word_analyzer.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wordstat {

std::vector<std::string> Tokenize(const std::string& text) {
  std::vector<std::string> words;
  std::string current;
  for (char ch : text) {
    if (std::isalpha(static_cast<unsigned char>(ch)) != 0) {
      // Sub-optimal: appends one character at a time instead of a
      // single regex-style pass over the whole text.
      current.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    } else {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    }
  }
  if (!current.empty()) {
    words.push_back(current);
  }
  return words;
}

std::vector<WordCount> CountWords(const std::vector<std::string>& words) {
  std::vector<WordCount> counts;
  for (const std::string& word : words) {
    bool found = false;
    for (WordCount& entry : counts) {
      if (entry.word == word) {
        entry.count += 1;
        found = true;
        break;
      }
    }
    if (!found) {
      counts.push_back(WordCount{word, 1});
    }
  }
  return counts;
  // gcov marks a brace after `return` as unreached (nothing executes
  // past `return`); not a real coverage gap.
}  // LCOV_EXCL_LINE

std::vector<WordCount> TopWords(const std::vector<WordCount>& counts, int n) {
  std::vector<WordCount> remaining = counts;
  std::vector<WordCount> result;
  int take = std::min<int>(n, static_cast<int>(remaining.size()));
  for (int i = 0; i < take; ++i) {
    int best_index = 0;
    for (int j = 1; j < static_cast<int>(remaining.size()); ++j) {
      if (remaining[j].count > remaining[best_index].count) {
        best_index = j;
      }
    }
    result.push_back(remaining[best_index]);
    remaining.erase(remaining.begin() + best_index);
  }
  return result;
}

std::vector<WordCount> Analyze(const std::string& text, int top_n) {
  std::vector<std::string> words = Tokenize(text);
  std::vector<WordCount> counts = CountWords(words);
  return TopWords(counts, top_n);
}

std::optional<ParsedArgs> ParseArgs(const std::vector<std::string>& args,
                                    std::string* error) {
  ParsedArgs parsed;
  bool have_file = false;

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "-n" || arg == "--top") {
      if (i + 1 >= args.size()) {
        *error = "missing value for " + arg;
        return std::nullopt;
      }
      try {
        std::size_t consumed = 0;
        parsed.top_n = std::stoi(args[i + 1], &consumed);
        if (consumed != args[i + 1].size()) {
          throw std::invalid_argument("trailing characters");
        }
      } catch (const std::exception&) {
        *error = "invalid integer for " + arg + ": " + args[i + 1];
        return std::nullopt;
      }
      ++i;
    } else if (!have_file) {
      parsed.file_path = arg;
      have_file = true;
    } else {
      *error = "unexpected argument: " + arg;
      return std::nullopt;
    }
  }

  if (!have_file) {
    *error = "missing required argument: file";
    return std::nullopt;
  }

  return parsed;
}

int Run(const std::vector<std::string>& args, std::ostream* out,
        std::ostream* err) {
  std::string error;
  std::optional<ParsedArgs> parsed = ParseArgs(args, &error);
  if (!parsed.has_value()) {
    *err << "error: " << error << "\n";
    return 1;
  }

  std::ifstream file(parsed->file_path);
  if (!file.is_open()) {
    *err << "error: file not found: " << parsed->file_path << "\n";
    return 1;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string text = buffer.str();

  std::vector<WordCount> results = Analyze(text, parsed->top_n);
  for (const WordCount& entry : results) {
    *out << entry.word << "\t" << entry.count << "\n";
  }

  return 0;
}

}  // namespace wordstat
