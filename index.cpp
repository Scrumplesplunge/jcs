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

using SnippetTable = std::array<std::vector<Index::FileID>, kNumSnippets>;

struct IndexBatch {
  void IndexFile(Index::FileID file_id, std::string_view path) {
    try {
      const auto start = Clock::now();
      const FileReadBuffer buffer(path);
      const auto open = Clock::now();
      std::vector<bool> seen(kNumSnippets);
      for (auto snippet : std::ranges::views::slide(buffer.Contents(), 3)) {
        seen[Hash(std::string_view(snippet))] = true;
      }
      for (int id = 0; id < kNumSnippets; id++) {
        if (seen[id]) snippets[id].push_back(file_id);
      }
      const auto done = Clock::now();
      open_time += open - start;
      index_time += done - open;
    } catch (std::exception&) {}  // Ignore I/O issues for files, skip them.
  }

  SnippetTable snippets;
  std::chrono::nanoseconds open_time = {};
  std::chrono::nanoseconds index_time = {};
};

std::unique_ptr<SnippetTable> MergeBatches(
    std::span<const IndexBatch> batches) {
  auto result = std::make_unique<SnippetTable>();
  const auto start = Clock::now();
  for (int i = 0; i < kNumSnippets; i++) {
    std::vector<Index::FileID>& out = (*result)[i];
    for (const IndexBatch& batch : batches) {
      out.append_range(batch.snippets[i]);
    }
    std::ranges::sort(out);
  }
  const auto end = Clock::now();
  std::chrono::nanoseconds open_time = {};
  std::chrono::nanoseconds index_time = {};
  std::chrono::nanoseconds merge_time = end - start;
  for (const IndexBatch& batch : batches) {
    open_time += batch.open_time;
    index_time += batch.index_time;
  }
  std::println("opening: {}", open_time);
  std::println("indexing: {}", index_time);
  std::println("merging: {}", merge_time);
  return result;
}

class Indexer {
 public:
  void IndexAll() {
    files_ = DiscoverFiles();
    // Use multiple threads to index the files. Threads create separate indices
    // which are merged at the end.
    std::atomic_int done = 0, next = 0;
    constexpr int kNumWorkers = 8;
    std::vector<IndexBatch> batches(kNumWorkers);
    std::vector<std::jthread> workers(kNumWorkers);
    for (int i = 0; i < kNumWorkers; i++) {
      auto& batch = batches[i];
      workers[i] = std::jthread([this, &batch, &done, &next] {
        while (true) {
          const Index::FileID file_id =
              next.fetch_add(1, std::memory_order_relaxed);
          if (file_id >= files_.size()) break;
          batch.IndexFile(file_id, files_[file_id]);
          done.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }
    while (true) {
      const int current = done.load(std::memory_order_relaxed);
      if (current == files_.size()) break;
      std::println("{:7d}/{} {}", current, files_.size(), files_[current]);
      std::this_thread::sleep_for(1s);
    }
    std::println("{0:7d}/{0} done.", files_.size());
    for (std::jthread& worker : workers) worker.join();
    snippets_ = MergeBatches(batches);
  }

  void Save(std::string_view path) const {
    const auto start = Clock::now();
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
      for (std::span<const Index::FileID> list : *snippets_) {
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
    const auto end = Clock::now();
    std::println("saving: {}", end - start);
  }

 private:
  static std::vector<std::string> DiscoverFiles() {
    const auto start = Clock::now();
    const std::set<fs::path> allowed = {".cc", ".h", ".cpp", ".hpp"};
    std::vector<std::string> files;
    for (const fs::path& path : fs::recursive_directory_iterator(
             fs::current_path(),
             fs::directory_options::skip_permission_denied)) {
      if (allowed.contains(path.extension())) files.push_back(path.string());
    }
    std::ranges::sort(files);
    const auto end = Clock::now();
    std::println("discovering: {}", end - start);
    return files;
  }

  std::vector<std::string> files_;
  std::unique_ptr<SnippetTable> snippets_;
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

std::generator<Index::SearchResult> Index::Search(
    std::string_view term) const noexcept {
  for (FileID file : Candidates(term)) {
    FileReadBuffer buffer;
    try {
      buffer = FileReadBuffer(GetFileName(file));
    } catch (std::exception&) {}
    int line_number = 0;
    for (auto line : std::ranges::views::split(buffer.Contents(), '\n')) {
      line_number++;
      std::string_view line_contents(line);
      if (!line_contents.empty() && line_contents.back() == '\r') {
        line_contents.remove_suffix(1);
      }
      const auto column = line_contents.find(term);
      if (column == line_contents.npos) continue;
      co_yield {.file_name = GetFileName(file),
                .line = line_number,
                .column = int(column + 1),
                .line_contents = line_contents};
    }
  }
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
