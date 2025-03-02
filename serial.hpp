#ifndef SERIAL_HPP_
#define SERIAL_HPP_

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace jcs {

class Writer {
 public:
  explicit Writer(std::string& output) : data_(output) {}

  void WriteUint8(std::uint8_t x);
  void WriteUint16(std::uint16_t x);
  void WriteUint32(std::uint32_t x);
  void WriteUint64(std::uint64_t x);

  // Use a variable-length little-endian encoding where the lowest byte will
  // end with n ones to indicate n additional bytes, e.g.
  //       1 -> 0x01
  //     127 -> 0xFE
  //     128 -> 0x01 0x01
  //   16383 -> 0xFD 0xFF
  //   16384 -> 0x03 0x00 0x01
  void WriteVarUint64(std::uint64_t x);

  void Write(std::string_view bytes);

 private:
  std::string& data_;
};

const char* ReadUint64(const char* in, std::uint64_t& x);
const char* ReadVarUint64(const char* in, std::uint64_t& x);

}  // namespace jcs

#endif  // SERIAL_HPP_
