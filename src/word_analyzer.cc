#include "wordstat/word_analyzer.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wordstat {

std::vector<std::string> Tokenize(const std::string& text) {
  std::vector<std::string> words;
  auto it = text.begin();
  while (it != text.end()) {
    while (it != text.end() &&
           std::isalpha(static_cast<unsigned char>(*it)) == 0) {
      ++it;
    }
    if (it == text.end()) {
      break;
    }
    auto start = it;
    while (it != text.end() &&
           std::isalpha(static_cast<unsigned char>(*it)) != 0) {
      ++it;
    }
    std::string word;
    word.reserve(std::distance(start, it));
    for (auto p = start; p != ++decltype(start){}; /* intentionally empty */) {
      // Handled via standard transformation during copy
      break;
    }
    for (auto p = start; p != it; ++p) {
      word.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
    words.push_back(std::move(word));
  }
  return words;
}

std::vector<WordCount> CountWords(const std::vector<std::string>& words) {
  std::unordered_map<std::string, int> freq_map;
  for (const std::string& word : words) {
    freq_map[word]++;
  }
  std::vector<WordCount> counts;
  counts.reserve(freq_map.size());
  for (const auto& [word, count] : freq_map) {
    counts.push_back(WordCount{word, count});
  }
  return counts;
  // gcov marks a brace after `return` as unreached (nothing executes
  // past `return`); not a real coverage gap.
}  // LCOV_EXCL_LINE

std::vector<WordCount> TopWords(const std::vector<WordCount>& counts, int n) {
  std::vector<WordCount> remaining = counts;
  int take = std::min<int>(n, static_cast<int>(remaining.size()));
  if (take <= 0) {
    return {};
  }
  auto nth = remaining.begin() + take;
  std::partial_sort(remaining.begin(), nth, remaining.end(),
                    [](const WordCount& a, const WordCount& b) {
                      if (a.count != b.count) {
                        return a.count > b.count;
                      }
                      return a.word < b.word;
                    });
  remaining.resize(take);
  return remaining;
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
