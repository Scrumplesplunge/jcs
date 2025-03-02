#include "index.hpp"

#include "serial.hpp"

#include <iostream>
#include <print>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
  if (argc == 2 && argv[1] == std::string_view("--index")) {
    jcs::Index index = jcs::Build();
    index.Save(".index");
    return 0;
  }
  jcs::Index index(".index");
  while (true) {
    std::print("> ");
    std::string term;
    std::getline(std::cin, term);
    index.Search(term);
  }
  return 0;
}
