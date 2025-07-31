#include "index.hpp"

#include <filesystem>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;

struct Options {
  enum class Mode {
    kInfo,         // Enabled by `--info`. Expects no args.
    kIndex,        // Enabled by `--index`. Expects no args.
    kUpdate,       // Enabled by `--update`. Expects no args.
    kInteractive,  // Enabled by `--interactive` (or nothing). Expects no args.
    kSearch,       // Enabled by no options and a single argument.
  };
  Mode mode;
  std::span<char*> args;
};

Options ParseOptions(int argc, char* argv[]) {
  std::optional<Options::Mode> mode;
  bool ignore = false;
  int num_args = 1;
  auto set_mode = [&](Options::Mode m) {
    if (mode) {
      std::println(stderr, "Multiple mode selectors given.");
      std::exit(1);
    }
    mode = m;
  };
  for (int i = 1; i < argc; i++) {
    const std::string_view arg = argv[i];
    if (ignore || !arg.starts_with("--")) argv[num_args++] = argv[i];
    if (arg == "--") {
      ignore = true;
    } else if (arg == "--info") {
      set_mode(Options::Mode::kInfo);
    } else if (arg == "--index") {
      set_mode(Options::Mode::kIndex);
    } else if (arg == "--update") {
      set_mode(Options::Mode::kUpdate);
    } else if (arg == "--interactive") {
      set_mode(Options::Mode::kInteractive);
    }
  }
  const auto args = std::span<char*>(argv, num_args).subspan(1);
  if (mode) {
    std::size_t expected_args;
    switch (*mode) {
      case Options::Mode::kInfo:
      case Options::Mode::kIndex:
      case Options::Mode::kUpdate:
      case Options::Mode::kInteractive:
        expected_args = 0;
        break;
      case Options::Mode::kSearch:
        expected_args = 1;
        break;
    }
    if (args.size() != expected_args) {
      std::println(stderr, "Got {} arguments when {} were expected.",
                   args.size(), expected_args);
      std::exit(1);
    }
  } else {
    switch (args.size()) {
      case 0:
        mode = Options::Mode::kInteractive;
        break;
      case 1:
        mode = Options::Mode::kSearch;
        break;
      default:
        std::println(stderr, "Multiple arguments provided.");
        std::exit(1);
    }
  }
  return Options{.mode = *mode, .args = args};
}

std::optional<fs::path> FindIndex() {
  fs::path directory = fs::current_path();
  const fs::path root = directory.root_path();
  while (!fs::exists(directory / ".index")) {
    if (directory == root) return std::nullopt;
    directory = directory.parent_path();
  }
  return directory / ".index";
}

jcs::Index LoadIndex() {
  std::optional<fs::path> index = FindIndex();
  if (!index) {
    std::println(stderr, "No .index found. Run `jcs --index` to generate one.");
    std::exit(1);
  }
  return jcs::Index(index->string());
}

int RunInteractive() {
  const jcs::Index index = LoadIndex();
  constexpr int kMaxFileMatches = 5;
  constexpr int kMaxFiles = 5;
  while (true) {
    std::print("> ");
    std::string query;
    std::getline(std::cin, query);
    if (std::cin.eof()) {
      std::cout << '\n';
      return 0;
    }
    int num_files = 0;
    int num_file_matches = 0;
    int num_matches = 0;
    std::string_view previous_file;
    for (jcs::Index::SearchResult result : index.Search(query)) {
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
}

int Search(std::string_view query) {
  const jcs::Index index = LoadIndex();
  for (jcs::Index::SearchResult result : index.Search(query)) {
    std::println("{}:{}:{}: {}",
                 result.file_name, result.line, result.column,
                 result.line_contents);
  }
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  const Options options = ParseOptions(argc, argv);
  switch (options.mode) {
    case Options::Mode::kInfo:
      if (std::optional<fs::path> index_path = FindIndex()) {
        std::println("Using {}", index_path->string());
        return 0;
      } else {
        std::println("No .index file found.");
        return 1;
      }
    case Options::Mode::kIndex:
      jcs::Build(".index");
      return 0;
    case Options::Mode::kUpdate:
      if (std::optional<fs::path> index = FindIndex(); index.has_value()) {
        fs::current_path(index->parent_path());
      }
      jcs::Build(".index");
      return 0;
    case Options::Mode::kInteractive:
      return RunInteractive();
    case Options::Mode::kSearch:
      return Search(options.args[0]);
  }
}
