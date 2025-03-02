#include "serial.hpp"

namespace jcs {

std::uint32_t Serial<std::uint32_t>::Read(std::istream& in) {
  char bytes[4];
  in.read(bytes, 4);
  return std::uint32_t(std::uint8_t(bytes[0])) |
         std::uint32_t(std::uint8_t(bytes[1])) << 8 |
         std::uint32_t(std::uint8_t(bytes[2])) << 16 |
         std::uint32_t(std::uint8_t(bytes[3])) << 24;
}

void Serial<std::uint32_t>::Write(std::ostream& out, std::uint32_t x) {
  char bytes[4];
  bytes[0] = std::uint8_t(x);
  bytes[1] = std::uint8_t(x >> 8);
  bytes[2] = std::uint8_t(x >> 16);
  bytes[3] = std::uint8_t(x >> 24);
  out.write(bytes, 4);
}

std::string Serial<std::string>::Read(std::istream& in) {
  const std::uint32_t length = Serial<std::uint32_t>::Read(in);
  std::string value(length, '\0');
  in.read(value.data(), length);
  return value;
}

void Serial<std::string>::Write(std::ostream& out, std::string_view s) {
  Serial<std::uint32_t>::Write(out, (std::uint32_t)s.size());
  out.write(s.data(), s.size());
}

}  // namespace jcs
