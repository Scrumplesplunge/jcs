#include "buffer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <format>
#include <stdexcept>
#include <string>

namespace jcs {
namespace {

class Handle {
 public:
  Handle() = default;
  explicit Handle(HANDLE x) : value_(x) {}
  ~Handle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }

  // Not copyable.
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;

  HANDLE get() const { return value_; }

 private:
  HANDLE value_ = INVALID_HANDLE_VALUE;
};

}  // namespace

FileReadBuffer::FileReadBuffer(std::string_view path) {
  const std::string null_terminated_path(path);
  const Handle file(CreateFile(
      null_terminated_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.get() == INVALID_HANDLE_VALUE) {
    throw std::runtime_error(std::format("Cannot open file {}", path));
  }

  const DWORD size = GetFileSize(file.get(), nullptr);
  if (size == INVALID_FILE_SIZE) {
    throw std::runtime_error(std::format("Can't get size of file {}", path));
  }

  const Handle map(CreateFileMapping(
      file.get(), nullptr, PAGE_READONLY, 0, size, nullptr));
  if (!map.get()) {
    throw std::runtime_error(std::format("Failed to map {}", path));
  }

  SYSTEM_INFO info;
  GetNativeSystemInfo(&info);

  const LPVOID data = MapViewOfFile(map.get(), FILE_MAP_READ, 0, 0, size);
  if (data == nullptr) {
    throw std::runtime_error(std::format("Failed to map {}", path));
  }

  data_ = std::span<const char>(reinterpret_cast<const char*>(data), size);
}

FileReadBuffer::~FileReadBuffer() {
  if (data_.data()) UnmapViewOfFile(data_.data());
}

FileReadBuffer::FileReadBuffer(FileReadBuffer&& other)
    : data_(std::exchange(other.data_, {})) {}

FileReadBuffer& FileReadBuffer::operator=(FileReadBuffer&& other) {
  if (data_.data()) UnmapViewOfFile(data_.data());
  data_ = std::exchange(other.data_, {});
  return *this;
}

}  // namespace jcs
