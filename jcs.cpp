#include "serial.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <print>
#include <set>
#include <mutex>
#include <thread>

namespace jcs {
namespace {

namespace fs = std::filesystem;
using clock = std::chrono::steady_clock;
using std::chrono_literals::operator""s;
using std::chrono_literals::operator""ms;

using FileID = std::uint32_t;
constexpr int kNumSnippets = 1 << 16;  // Matches the range of Hash().

class Database {
 public:
  void Save(const fs::path& index_file) const {
    std::unique_lock lock(mutex_);
    const auto start = clock::now();
    std::ofstream out(index_file, std::ios::binary);
    out.exceptions(std::ostream::failbit | std::ostream::badbit);
    Serial<std::string[]>::Write(out, files_);
    std::size_t refs = 0;
    for (int i = 0; i < kNumSnippets; i++) {
      Serial<std::uint32_t[]>::Write(out, snippets_[i]);
      refs += snippets_[i].size();
    }
    const auto end = clock::now();
    std::println("Saved {} refs in {}ms", refs, (end - start) / 1ms);
  }

  void Load(const fs::path& index_file) {
    std::unique_lock lock(mutex_);
    const auto start = clock::now();
    std::ifstream in(index_file, std::ios::binary);
    in.exceptions(std::ostream::failbit | std::ostream::badbit);
    files_ = Serial<std::string[]>::Read(in);
    snippets_.clear();
    snippets_.resize(kNumSnippets);
    for (int i = 0; i < kNumSnippets; i++) {
      snippets_[i] = Serial<std::uint32_t[]>::Read(in);
    }
    const auto end = clock::now();
    std::println("Loaded in {}ms", (end - start) / 1ms);
  }

  void IndexAll(const std::span<const fs::path> paths) {
    std::atomic_int next = 0;

    const auto worker = [&] {
      while (true) {
        const int i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= paths.size()) break;
        Index(paths[i]);
      }
    };

    std::vector<std::jthread> workers;
    for (int i = 0; i < 32; i++) workers.emplace_back(worker);

    while (true) {
      const int current = next.load(std::memory_order_relaxed);
      if (current >= paths.size()) break;
      std::println("{}... ({}/{})",
                   paths[current].string(), current, paths.size());
      std::this_thread::sleep_for(1s);
    }
  }

  bool Index(const fs::path& file) {
    std::ifstream stream(file);
    const std::string contents(std::istreambuf_iterator<char>(stream), {});
    if (!stream.good()) return false;

    std::vector<bool> seen(kNumSnippets);
    for (auto quad : std::ranges::views::slide(contents, 4)) {
      seen[Hash(std::string_view(quad))] = true;
    }

    std::unique_lock lock(mutex_);
    const FileID file_id = (FileID)files_.size();
    files_.emplace_back(fs::canonical(file).string());

    for (int id = 0; id < kNumSnippets; id++) {
      if (seen[id]) snippets_[id].push_back(file_id);
    }

    return true;
  }

  void Search(std::string_view term) {
    std::unique_lock lock(mutex_);
    int matched_files = 0;
    int matched_lines = 0;
    for (FileID file : Candidates(term)) {
      std::ifstream stream(files_[file]);
      const std::string contents(std::istreambuf_iterator<char>(stream), {});
      if (!stream.good()) {
        std::cerr << "Failed to re-open candidate file "
                  << files_[file] << '\n';
        continue;
      }
      int matches = 0;
      int line_number = 0;
      for (auto line : std::ranges::views::split(contents, '\n')) {
        line_number++;
        const std::string_view line_contents(line);
        const auto i = line_contents.find(term);
        if (i == line_contents.npos) continue;
        matches++;
        std::println("{}:{}:{}: {}",
                     files_[file], line_number, i + 1, line_contents);
      }
      if (matches) {
        matched_lines += matches;
        matched_files++;
      }
    }
    std::println("{} matched lines across {} files.", matched_lines, matched_files);
  }

 private:
  static int Hash(std::string_view term) {
    std::uint32_t hash = 0xdeadbeef;
    for (char c : term) hash = hash * 109 + c;
    return hash % kNumSnippets;
  }

  std::vector<FileID> Candidates(std::string_view term) {
    if (term.size() < 4) {
      std::cerr << "Minimum search term length is 4 characters.\n";
      return {};
    }
    bool first = true;
    std::vector<FileID> candidates;
    for (auto quad : std::ranges::views::slide(term, 4)) {
      const int id = Hash(std::string_view(quad));
      if (first) {
        first = false;
        candidates = snippets_[id];
      } else {
        int i = 0, j = 0;
        const int n = (int)candidates.size();
        for (FileID f : snippets_[id]) {
          if (i == n) break;
          while (i < n && candidates[i] < f) i++;
          if (i < n && candidates[i] == f) candidates[j++] = candidates[i++];
        }
        candidates.resize(j);
      }
    }
    std::println("{} candidate files.", candidates.size());
    return candidates;
  }

  mutable std::mutex mutex_;
  std::vector<std::vector<FileID>> snippets_ =
      std::vector<std::vector<FileID>>(kNumSnippets);
  std::vector<std::string> files_;
};

void BuildIndex() {
  Database database;
  const std::set<fs::path> allowed = {".cc", ".h", ".cpp"};
  std::vector<fs::path> files;
  std::println("Discovering files...");
  for (const fs::path& path : fs::recursive_directory_iterator(
           ".", fs::directory_options::skip_permission_denied)) {
    if (allowed.contains(path.extension())) files.push_back(path);
  }
  std::println("Will index {} files...", files.size());
  database.IndexAll(files);
  database.Save(".index");
}

}  // namespace
}  // namespace jcs

int main(int argc, char* argv[]) {
  if (argc == 2 && argv[1] == std::string_view("--index")) {
    jcs::BuildIndex();
    return 0;
  }
  jcs::Database database;
  database.Load(".index");
  while (true) {
    std::print("> ");
    std::string term;
    std::getline(std::cin, term);
    database.Search(term);
  }
  return 0;
}
