#ifndef QUERY_HPP_
#define QUERY_HPP_

#include <cstddef>
#include <expected>
#include <generator>
#include <string>
#include <string_view>

namespace jcs {

class Query {
 public:
  struct Match {
    int line, column;
    std::size_t start;
    std::size_t end;
  };

  static std::expected<Query, std::string> Compile(std::string_view text);

  // Generates a sequence of trigrams which must be present in any file which
  // contains a match.
  std::generator<std::string_view> Trigrams() const;

  // Generates an in-order sequence of matches in `text`.
  std::generator<Match> Search(std::string_view text) const;

 private:
  explicit Query(std::string_view text);

  std::string term_;
};

}  // namespace jcs

#endif  // QUERY_HPP_
