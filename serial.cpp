#include "serial.hpp"

#include <bit>

namespace jcs {

void Writer::WriteUint8(std::uint8_t x) {
  data_.push_back(x);
}

void Writer::WriteUint16(std::uint16_t x) {
  char out[2];
  out[0] = std::uint8_t(x);
  out[1] = std::uint8_t(x >> 8);
  data_.append_range(out);
}

void Writer::WriteUint32(std::uint32_t x) {
  char out[4];
  out[0] = std::uint8_t(x);
  out[1] = std::uint8_t(x >> 8);
  out[2] = std::uint8_t(x >> 16);
  out[3] = std::uint8_t(x >> 24);
  data_.append_range(out);
}

void Writer::WriteUint64(std::uint64_t x) {
  char out[8];
  out[0] = std::uint8_t(x);
  out[1] = std::uint8_t(x >> 8);
  out[2] = std::uint8_t(x >> 16);
  out[3] = std::uint8_t(x >> 24);
  out[4] = std::uint8_t(x >> 32);
  out[5] = std::uint8_t(x >> 40);
  out[6] = std::uint8_t(x >> 48);
  out[7] = std::uint8_t(x >> 56);
  data_.append_range(out);
}

void Writer::WriteVarUint64(std::uint64_t x) {
  if (x < 0x80) {
    WriteUint8(std::uint8_t(x << 1));
  } else if (x < 0x40'00) {
    WriteUint16(std::uint16_t(x << 2 | 1));
  } else if (x < 0x20'00'00) {
    x = x << 3 | 3;
    WriteUint16(std::uint16_t(x));
    WriteUint8(std::uint8_t(x >> 16));
  } else if (x < 0x10'00'00'00) {
    WriteUint32(std::uint32_t(x << 4 | 7));
  } else if (x < 0x8'00'00'00'00) {
    x = x << 5 | 15;
    WriteUint32(std::uint32_t(x));
    WriteUint8(std::uint8_t(x >> 32));
  } else if (x < 0x4'00'00'00'00'00) {
    x = x << 6 | 31;
    WriteUint32(std::uint32_t(x));
    WriteUint16(std::uint16_t(x >> 32));
  } else if (x < 0x2'00'00'00'00'00'00) {
    x = x << 7 | 63;
    WriteUint32(std::uint32_t(x));
    WriteUint16(std::uint16_t(x >> 32));
    WriteUint8(std::uint8_t(x >> 48));
  } else if (x < 0x1'00'00'00'00'00'00'00) {
    x = x << 8 | 127;
    WriteUint64(x);
  } else {
    WriteUint8(255);
    WriteUint64(x);
  }
}

void Writer::Write(std::string_view bytes) {
  data_ += bytes;
}

const char* ReadUint64(const char* in, std::uint64_t& x) {
  x = std::uint64_t(std::uint8_t(in[0])) |
      std::uint64_t(std::uint8_t(in[1])) << 8 |
      std::uint64_t(std::uint8_t(in[2])) << 16 |
      std::uint64_t(std::uint8_t(in[3])) << 24 |
      std::uint64_t(std::uint8_t(in[4])) << 32 |
      std::uint64_t(std::uint8_t(in[5])) << 40 |
      std::uint64_t(std::uint8_t(in[6])) << 48 |
      std::uint64_t(std::uint8_t(in[7])) << 56;
  return in + 8;
}

const char* ReadVarUint64(const char* in, std::uint64_t& x) {
  switch (std::countr_one(std::uint8_t(in[0]))) {
    case 0:
      x = std::uint8_t(in[0]) >> 1;
      return in + 1;
    case 1:
      x = std::uint64_t(std::uint8_t(in[0])) |
          std::uint64_t(std::uint8_t(in[1])) << 8;
      x >>= 2;
      return in + 2;
    case 2:
      x = std::uint64_t(std::uint8_t(in[0])) |
          std::uint64_t(std::uint8_t(in[1])) << 8 |
          std::uint64_t(std::uint8_t(in[2])) << 16;
      x >>= 3;
      return in + 3;
    case 3:
      x = std::uint64_t(std::uint8_t(in[0])) |
          std::uint64_t(std::uint8_t(in[1])) << 8 |
          std::uint64_t(std::uint8_t(in[2])) << 16 |
          std::uint64_t(std::uint8_t(in[3])) << 24;
      x >>= 4;
      return in + 4;
    case 4:
      x = std::uint64_t(std::uint8_t(in[0])) |
          std::uint64_t(std::uint8_t(in[1])) << 8 |
          std::uint64_t(std::uint8_t(in[2])) << 16 |
          std::uint64_t(std::uint8_t(in[3])) << 24 |
          std::uint64_t(std::uint8_t(in[4])) << 32;
      x >>= 5;
      return in + 5;
    case 5:
      x = std::uint64_t(std::uint8_t(in[0])) |
          std::uint64_t(std::uint8_t(in[1])) << 8 |
          std::uint64_t(std::uint8_t(in[2])) << 16 |
          std::uint64_t(std::uint8_t(in[3])) << 24 |
          std::uint64_t(std::uint8_t(in[4])) << 32 |
          std::uint64_t(std::uint8_t(in[5])) << 40;
      x >>= 6;
      return in + 6;
    case 6:
      x = std::uint64_t(std::uint8_t(in[0])) |
          std::uint64_t(std::uint8_t(in[1])) << 8 |
          std::uint64_t(std::uint8_t(in[2])) << 16 |
          std::uint64_t(std::uint8_t(in[3])) << 24 |
          std::uint64_t(std::uint8_t(in[4])) << 32 |
          std::uint64_t(std::uint8_t(in[5])) << 40 |
          std::uint64_t(std::uint8_t(in[6])) << 48;
      x >>= 7;
      return in + 7;
    case 7:
      in = ReadUint64(in, x);
      x >>= 8;
      return in;
    case 8:
      return ReadUint64(in + 1, x);
    default:
      throw std::logic_error("impossible");
  }
}

}  // namespace jcs
