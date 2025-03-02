#ifndef INDEX_HPP_
#define INDEX_HPP_

#include "buffer.hpp"

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
  explicit Index(std::vector<std::string> files,
                 std::vector<std::vector<FileID>> snippets) noexcept;

  void Load(std::string_view path);
  void Save(std::string_view path) const;

  // Not copyable.
  Index(const Index&) = delete;
  Index& operator=(const Index&) = delete;

  // Not movable.
  Index(Index&&) = delete;
  Index& operator=(Index&&) = delete;

  // Search for a term.
  void Search(std::string_view term) const noexcept;

 private:
  std::vector<FileID> Candidates(std::string_view term) const noexcept;

  std::vector<std::vector<FileID>> snippets_ =
      std::vector<std::vector<FileID>>(kNumSnippets);
  std::vector<std::string> files_;
};

Index Build();

}  // namespace jcs

#endif  // INDEX_HPP_
