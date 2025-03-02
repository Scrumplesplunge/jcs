#include "index.hpp"

#include "serial.hpp"

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
  jcs::Index index(".index");
  if (arg != "--interactive") {
    index.Search(arg);
    return 0;
  }
  while (true) {
    std::print("> ");
    std::string term;
    std::getline(std::cin, term);
    index.Search(term);
  }
  return 0;
}
