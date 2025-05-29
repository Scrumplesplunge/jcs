#include "memory_mapped_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace jcs {
namespace {

class Handle {
 public:
  Handle() = default;
  explicit Handle(int fd) : value_(fd) {}
  ~Handle() { if (value_ >= 0) close(value_); }

  // Not copyable.
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;

  int get() const { return value_; }

 private:
  int value_ = -1;
};

}  // namespace

MemoryMappedFile::MemoryMappedFile(std::string_view path) {
  const std::string null_terminated_path(path);
  const Handle file(open(null_terminated_path.c_str(), O_RDONLY));
  if (file.get() < 0) {
    throw std::runtime_error(std::format("Cannot open file {}", path));
  }

  struct stat info;
  if (fstat(file.get(), &info) < 0) {
    throw std::runtime_error(std::format("Cannot stat file {}", path));
  }

  const char* data = (const char*)mmap(nullptr, info.st_size, PROT_READ,
                                       MAP_SHARED, file.get(), 0);
  if (data == (caddr_t)-1) {
    throw std::runtime_error(std::format("Cannot mmap file {}", path));
  }

  data_ = std::string_view(data, info.st_size);
}

MemoryMappedFile::~MemoryMappedFile() {
  if (data_.data()) munmap(const_cast<char*>(data_.data()), data_.size());
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : data_(std::exchange(other.data_, {})) {}

MemoryMappedFile& MemoryMappedFile::operator=(
    MemoryMappedFile&& other) noexcept {
  if (data_.data()) munmap(const_cast<char*>(data_.data()), data_.size());
  data_ = std::exchange(other.data_, {});
  return *this;
}

}  // namespace jcs
