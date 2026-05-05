#pragma once

#include <unordered_map>
#include <string>
#include <memory>

namespace xcc
{
using FileId = size_t;

constexpr FileId BuildInFileId = SIZE_T_MAX;

class File {
public:
  struct LineInfo {
    size_t num;
    size_t offset;
    size_t length;
  };

  FileId                               id;
  std::string                          path;
  std::string                          contents;
  std::unordered_map<size_t, LineInfo> lines;

  size_t lineByOffset(size_t offset);
};

class FileManager {
public:
  FileManager() = delete;
  ~FileManager() = default;

  static FileId load(const std::string& path);
  static FileId lookup(const std::string& path);

  static std::shared_ptr<File> get(FileId id);

  static std::string getOrLoad(const std::string& path);

private:
  static std::unordered_map<FileId, std::shared_ptr<File>> files;
  static FileId id;
};

}
