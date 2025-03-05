#include "index.hpp"

#include <filesystem>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(stderr, "Usage: jcs [--index|--interactive|<search>]");
    return 1;
  }
  const std::string_view arg = argv[1];
  if (arg == std::string_view("--index")) {
    jcs::Build(".index");
    return 0;
  }
  // Search for an index file.
  namespace fs = std::filesystem;
  fs::path directory = fs::current_path();
  const fs::path root = directory.root_path();
  while (!fs::exists(directory / ".index")) {
    if (directory == root) {
      std::println(stderr, "No index found. Generate one with `jcs --index`.");
      return 1;
    }
    directory = directory.parent_path();
  }
  jcs::Index index((directory / ".index").string());
  if (arg != "--interactive") {
    for (jcs::Index::SearchResult result : index.Search(arg)) {
      std::println("{}:{}:{}: {}",
                   result.file_name, result.line, result.column,
                   result.line_contents);
    }
    return 0;
  }
  constexpr int kMaxFileMatches = 5;
  constexpr int kMaxFiles = 5;
  while (true) {
    std::print("> ");
    std::string term;
    std::getline(std::cin, term);
    int num_files = 0;
    int num_file_matches = 0;
    int num_matches = 0;
    std::string_view previous_file;
    for (jcs::Index::SearchResult result : index.Search(term)) {
      num_matches++;
      if (result.file_name != previous_file) {
        if (num_files < kMaxFiles) {
          std::println("{}", result.file_name);
        } else if (num_files == kMaxFiles) {
          std::println("...");
        }
        previous_file = result.file_name;
        num_files++;
        num_file_matches = 1;
      }
      if (num_files < kMaxFiles) {
        if (num_file_matches < kMaxFileMatches) {
          std::println("  {:4d}  {}", result.line, result.line_contents);
        } else if (num_file_matches == kMaxFileMatches) {
          std::println("  ...");
        }
      }
      num_file_matches++;
    }
    std::println("{} matches across {} files.", num_matches, num_files);
  }
  return 0;
}
