#include "query.hpp"

namespace jcs {

Query::Query(std::string_view text) : term_(text) {}

std::generator<std::string_view> Query::Trigrams() const {
  for (auto trigram : std::ranges::views::slide(term_, 3)) {
    co_yield std::string_view(trigram);
  }
}

std::generator<Query::Match> Query::Search(std::string_view text) const {
  int line = 1, column = 1;
  std::size_t i = 0;
  while (!text.empty()) {
    const auto j = text.find(term_, i);
    if (j == text.npos) break;
    for (char c : text.substr(i, j - i)) {
      if (c == '\n') {
        line++;
        column = 1;
      } else {
        column++;
      }
    }
    co_yield Match{
        .line = line, .column = column, .start = j, .end = j + term_.size()};
    i = j + 1;  // Permit overlapping matches.
  }
}

}  // namespace jcs
