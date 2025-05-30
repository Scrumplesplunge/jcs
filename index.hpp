#ifndef INDEX_HPP_
#define INDEX_HPP_

#include "platform/memory_mapped_file.hpp"

#include <generator>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>

namespace jcs {

inline constexpr int kNumSnippets = 1 << 16;

int Hash(std::string_view snippet) noexcept;

class Index {
 public:
  using FileID = std::uint32_t;

  Index() = default;
  explicit Index(std::string_view path);

  void Load(std::string_view path);

  // Not copyable.
  Index(const Index&) = delete;
  Index& operator=(const Index&) = delete;

  // Not movable.
  Index(Index&&) = delete;
  Index& operator=(Index&&) = delete;

  // Search for a term.
  struct SearchResult {
    std::string_view file_name;
    int line, column;
    std::string_view line_contents;
  };

  std::generator<SearchResult> Search(std::string_view term) const noexcept;

 private:
  std::vector<FileID> Candidates(std::string_view term) const noexcept;

  std::string_view GetFileName(FileID id) const;
  std::generator<FileID> GetSnippets(int id) const;

  MemoryMappedFile buffer_;
  std::span<const std::uint64_t> snippets_;
  std::span<const std::uint64_t> files_;
  std::span<const char> data_;
};

void Build(std::string_view path);

}  // namespace jcs

#endif  // INDEX_HPP_
