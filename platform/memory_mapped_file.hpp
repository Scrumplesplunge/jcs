#pragma once

#include <string_view>

namespace jcs {

class MemoryMappedFile {
 public:
  MemoryMappedFile() = default;
  explicit MemoryMappedFile(std::string_view path);
  ~MemoryMappedFile();

  MemoryMappedFile(MemoryMappedFile&&) noexcept;
  MemoryMappedFile& operator=(MemoryMappedFile&&) noexcept;

  std::string_view Contents() const { return data_; }

 private:
  std::string_view data_;
};

}  // namespace jcs
