#ifndef BUFFER_HPP_
#define BUFFER_HPP_

#include <span>
#include <string_view>

namespace jcs {

class FileReadBuffer {
 public:
  FileReadBuffer() = default;
  explicit FileReadBuffer(std::string_view path);
  ~FileReadBuffer();

  // Not copyable.
  FileReadBuffer(const FileReadBuffer&) = delete;
  FileReadBuffer& operator=(const FileReadBuffer&) = delete;

  // Movable.
  FileReadBuffer(FileReadBuffer&&);
  FileReadBuffer& operator=(FileReadBuffer&&);

  std::string_view Contents() const noexcept { return std::string_view(data_); }

 private:
  std::span<const char> data_;
};

}  // namespace jcs

#endif  // BUFFER_HPP_
