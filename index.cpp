#include "index.hpp"

#include "serial.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>
#include <set>

namespace jcs {
namespace {

namespace fs = std::filesystem;

using Clock = std::chrono::steady_clock;
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""s;

constexpr int kMaxMatchesInFile = 5;
constexpr int kMaxMatchedFiles = 5;

class Indexer {
 public:
  void IndexFile(const fs::path& file) {
    std::ifstream stream(file);
    const std::string contents(std::istreambuf_iterator<char>(stream), {});
    if (!stream.good()) return;  // Skip bad files.

    std::vector<bool> seen(kNumSnippets);
    for (auto snippet : std::ranges::views::slide(contents, 3)) {
      seen[Hash(std::string_view(snippet))] = true;
    }

    std::unique_lock lock(mutex_);
    const Index::FileID file_id = (Index::FileID)files_.size();
    files_.emplace_back(fs::canonical(file).string());

    for (int id = 0; id < kNumSnippets; id++) {
      if (seen[id]) snippets_[id].push_back(file_id);
    }
  }

  void IndexAll() {
    const std::vector<fs::path> files = DiscoverFiles();
    std::atomic_int done = 0, next = 0;

    const auto worker = [&] {
      while (true) {
        const int i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= files.size()) break;
        IndexFile(files[i]);
        done.fetch_add(1, std::memory_order_relaxed);
      }
    };

    std::vector<std::jthread> workers;
    for (int i = 0; i < 8; i++) workers.emplace_back(worker);

    while (true) {
      const int current = done.load(std::memory_order_relaxed);
      if (current == files.size()) break;
      std::println(
          "{:7d}/{} {}", current, files.size(), files[current].string());
      std::this_thread::sleep_for(1s);
    }
    std::println("{:7d}/{} done.", files.size(), files.size());
  }

  Index Build() && { return Index(std::move(files_), std::move(snippets_)); }

 private:
  static std::vector<fs::path> DiscoverFiles() {
    const std::set<fs::path> allowed = {".cc", ".h", ".cpp"};
    std::vector<fs::path> files;
    for (const fs::path& path : fs::recursive_directory_iterator(
             ".", fs::directory_options::skip_permission_denied)) {
      if (allowed.contains(path.extension())) files.push_back(path);
    }
    return files;
  }

  mutable std::mutex mutex_;
  std::vector<std::string> files_;
  std::vector<std::vector<Index::FileID>> snippets_
      = std::vector<std::vector<Index::FileID>>(kNumSnippets);
};

}  // namespace

int Hash(std::string_view term) noexcept {
  std::uint32_t hash = 0xdeadbeef;
  for (char c : term) hash = hash * 109 + c;
  return hash % kNumSnippets;
}

Index::Index(std::string_view path) { Load(path); }

Index::Index(std::vector<std::string> files,
             std::vector<std::vector<FileID>> snippets) noexcept
    : files_(std::move(files)),
      snippets_(std::move(snippets)) {}

void Index::Load(std::string_view path) {
  const auto start = Clock::now();
  std::ifstream in(std::string(path), std::ios::binary);
  in.exceptions(std::ostream::failbit | std::ostream::badbit);
  files_ = Serial<std::string[]>::Read(in);
  snippets_.clear();
  snippets_.resize(kNumSnippets);
  for (int i = 0; i < kNumSnippets; i++) {
    snippets_[i] = Serial<std::uint32_t[]>::Read(in);
  }
  const auto end = Clock::now();
  std::println("Loaded in {}ms", (end - start) / 1ms);
}

void Index::Save(std::string_view path) const {
  const auto start = Clock::now();
  std::ofstream out(std::string(path), std::ios::binary);
  out.exceptions(std::ostream::failbit | std::ostream::badbit);
  Serial<std::string[]>::Write(out, files_);
  std::size_t refs = 0;
  for (int i = 0; i < kNumSnippets; i++) {
    Serial<std::uint32_t[]>::Write(out, snippets_[i]);
    refs += snippets_[i].size();
  }
  const auto end = Clock::now();
  std::println("Saved {} refs in {}ms", refs, (end - start) / 1ms);
}

void Index::Search(std::string_view term) const noexcept {
  int matched_files = 0;
  int matched_lines = 0;
  for (FileID file : Candidates(term)) {
    std::ifstream stream(files_[file]);
    const std::string contents(std::istreambuf_iterator<char>(stream), {});
    if (!stream.good()) {
      std::println(stderr, "Failed to re-open candidate file {}", files_[file]);
      continue;
    }
    int matches_in_file = 0;
    int line_number = 0;
    for (auto line : std::ranges::views::split(contents, '\n')) {
      line_number++;
      const std::string_view line_contents(line);
      if (!line_contents.contains(term)) continue;
      if (matches_in_file == 0) matched_files++;
      matches_in_file++;
      matched_lines++;
      if (matches_in_file == 1 && matched_files == kMaxMatchedFiles + 1) {
        std::println("...");
      } else if (matched_files <= kMaxMatchedFiles) {
        if (matches_in_file == 1) std::println("{}", files_[file]);
        if (matches_in_file == kMaxMatchesInFile + 1) {
          std::println("          ...");
        } else if (matches_in_file <= kMaxMatchesInFile) {
          std::println("  {:4d}  {}", line_number, line_contents);
        }
      }
    }
  }
  std::println("{} matched lines across {} files.", matched_lines, matched_files);
}

std::vector<Index::FileID> Index::Candidates(
    std::string_view term) const noexcept {
  if (term.size() < 3) {
    std::println(stderr, "Minimum search length is 3 characters.");
    return {};
  }
  bool first = true;
  std::vector<FileID> candidates;
  for (auto quad : std::ranges::views::slide(term, 3)) {
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

Index Build() {
  auto indexer = std::make_unique<Indexer>();
  indexer->IndexAll();
  return std::move(*indexer).Build();
}

}  // namespace jcs
