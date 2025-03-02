#ifndef SERIAL_HPP_
#define SERIAL_HPP_

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace jcs {

template <typename T>
struct Serial;

template <typename T>
concept Serializable = requires (
    std::istream& in, std::ostream& out, const T& x) {
  { Serial<T>::Read(in) } -> std::convertible_to<T>;
  { Serial<T>::Write(out, x) };
};

template <>
struct Serial<std::uint32_t> {
  static std::uint32_t Read(std::istream& in);
  static void Write(std::ostream& out, std::uint32_t x);
};

template <>
struct Serial<std::string> {
  static std::string Read(std::istream& in);
  static void Write(std::ostream& out, std::string_view s);
};

template <Serializable T>
struct Serial<T[]> {
  static std::vector<T> Read(std::istream& in) {
    const auto length = Serial<std::uint32_t>::Read(in);
    std::vector<T> values;
    values.reserve(length);
    for (std::uint32_t i = 0; i < length; i++) {
      values.push_back(Serial<T>::Read(in));
    }
    return values;
  }

  static void Write(std::ostream& out, std::span<const T> values) {
    Serial<std::uint32_t>::Write(out, (std::uint32_t)values.size());
    for (const T& value : values) Serial<T>::Write(out, value);
  }
};

}  // namespace jcs

#endif  // SERIAL_HPP_
