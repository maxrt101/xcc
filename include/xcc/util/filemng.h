#pragma once

#include <unordered_map>
#include <climits>
#include <string>
#include <memory>

namespace xcc
{

/** File ID */
using FileId = size_t;

/** Represents a functionality, that has no resident source */
constexpr FileId BuiltInFileId = ULONG_MAX;

/**
 * Represents File
 *
 * Contains id, path, contents and a map of line numbers to line info
 */
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

  /** Converts absolute offset from file contents start into line number */
  size_t lineByOffset(size_t offset);

private:
  void process();

  friend class FileManager;
};

/**
 * Static global manager of files
 */
class FileManager {
public:
  FileManager() = delete;
  ~FileManager() = default;

  /**
   * Get id of file with provided path, or read the file and return newly assigned id
   */
  static FileId load(std::string path);

  /**
   * Find file by path, returns 0 if file isn't loaded
   */
  static FileId lookup(std::string path);

  /**
   * Get File instance by id, returns nullptr if file isn't loaded
   */
  static std::shared_ptr<File> get(FileId id);

  /**
   * Get file contents, of load from path and return loaded contents
   */
  static std::string getOrLoad(std::string path);

  /**
   * Remove file instance by id
   *
   * @warning Use with care, and only if given file won't be referenced again
   */
  static void purge(FileId id);

  /**
   * Create a virtual file with provided contents
   */
  static FileId createVirtual(std::string path, const std::string& contents);

private:
  static std::unordered_map<FileId, std::shared_ptr<File>> files;
  static FileId id;
};

}
