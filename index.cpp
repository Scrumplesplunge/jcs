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
    FileReadBuffer buffer(file.string());
    const std::string_view contents = buffer.Contents();

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
        try {
          IndexFile(files[i]);
        } catch (std::exception&) {}
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

  void Save(std::string_view path) const {
    // Variable-length data.
    std::string data;
    // filename_offsets[i] is the offset of files[i] in data.
    std::vector<std::uint64_t> filename_offsets;
    // snippets_offsets[i] is the offset of snippets[i] in data.
    std::vector<std::uint64_t> snippets_offsets;
    {
      Writer writer(data);
      for (std::string_view file : files_) {
        filename_offsets.push_back(data.size());
        writer.WriteVarUint64(file.size());
        writer.Write(file);
      }
      for (std::span<const Index::FileID> list : snippets_) {
        snippets_offsets.push_back(data.size());
        writer.WriteVarUint64(list.size());
        for (Index::FileID file : list) writer.WriteVarUint64(file);
      }
    }
    std::string tables;
    Writer writer(tables);
    for (std::uint64_t offset : snippets_offsets) writer.WriteUint64(offset);
    writer.WriteUint64(std::uint32_t(filename_offsets.size()));
    for (std::uint64_t offset : filename_offsets) writer.WriteUint64(offset);
    std::ofstream out{std::string(path), std::ios::binary};
    out.exceptions(std::ostream::failbit | std::ostream::badbit);
    out.write(tables.data(), tables.size());
    out.write(data.data(), data.size());
  }

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
  std::array<std::vector<Index::FileID>, kNumSnippets> snippets_;
};

}  // namespace

int Hash(std::string_view term) noexcept {
  std::uint32_t hash = 0xdeadbeef;
  for (char c : term) hash = hash * 109 + c;
  return hash % kNumSnippets;
}

Index::Index(std::string_view path) { Load(path); }

void Index::Load(std::string_view path) {
  buffer_ = FileReadBuffer(path);
  const std::span<const char> contents = buffer_.Contents();
  const char* p = contents.data();
  snippets_ = std::span<const std::uint64_t>(
      reinterpret_cast<const std::uint64_t*>(p), kNumSnippets);
  p += std::as_bytes(snippets_).size();
  std::uint64_t num_files;
  p = ReadUint64(p, num_files);
  files_ = std::span<const std::uint64_t>(
      reinterpret_cast<const std::uint64_t*>(p), num_files);
  p += std::as_bytes(files_).size();
  data_ = contents.subspan(p - contents.data());
}

void Index::Search(std::string_view term) const noexcept {
  int matched_files = 0;
  int matched_lines = 0;
  for (FileID file : Candidates(term)) {
    FileReadBuffer buffer;
    try {
      buffer = FileReadBuffer(GetFileName(file));
    } catch (std::exception&) {}
    int matches_in_file = 0;
    int line_number = 0;
    for (auto line : std::ranges::views::split(buffer.Contents(), '\n')) {
      line_number++;
      const std::string_view line_contents(line);
      if (!line_contents.contains(term)) continue;
      if (matches_in_file == 0) matched_files++;
      matches_in_file++;
      matched_lines++;
      if (matches_in_file == 1 && matched_files == kMaxMatchedFiles + 1) {
        std::println("...");
      } else if (matched_files <= kMaxMatchedFiles) {
        if (matches_in_file == 1) std::println("{}", GetFileName(file));
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
      candidates.assign_range(GetSnippets(id));
    } else {
      int i = 0, j = 0;
      const int n = (int)candidates.size();
      for (FileID f : GetSnippets(id)) {
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

std::string_view Index::GetFileName(FileID id) const {
  const char* p = data_.data() + files_[id];
  std::uint64_t length;
  p = ReadVarUint64(p, length);
  return std::string_view(p, length);
}

std::generator<Index::FileID> Index::GetSnippets(int id) const {
  const char* p = data_.data() + snippets_[id];
  std::uint64_t length;
  p = ReadVarUint64(p, length);
  for (std::uint64_t i = 0; i < length; i++) {
    std::uint64_t value;
    p = ReadVarUint64(p, value);
    co_yield Index::FileID(value);
  }
}

void Build(std::string_view path) {
  auto indexer = std::make_unique<Indexer>();
  indexer->IndexAll();
  indexer->Save(path);
}

}  // namespace jcs
