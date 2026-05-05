#include "xcc/util/filemng.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"

using namespace xcc;

static auto logger = util::log::Logger("FILEMNG");

std::unordered_map<FileId, std::shared_ptr<File>> FileManager::files;
FileId FileManager::id = 1;

size_t File::lineByOffset(size_t offset) {
  for (auto& [line, info] : lines) {
    if (offset >= info.offset && offset <= info.offset + info.length) {
      return line;
    }
  }

  return 0;
}

FileId FileManager::load(const std::string& path) {
  if (auto id = lookup(path)) {
    return id;
  }

  FileId id = FileManager::id++;

  auto file = std::make_shared<File>(id, path, fs::readFile(path));

  size_t start  = 0;
  size_t offset = 0;
  size_t line   = 1;

  while (offset < file->contents.size()) {
    if (file->contents[offset] == '\n') {
      if (file->contents[start] == '\n') {
        start += 1;
      }

      file->lines[line] = File::LineInfo(line, start, offset - start);

      line   += 1;
      start   = offset;
    }

    offset++;
  }

  files[id] = file;

  return id;
}

FileId FileManager::lookup(const std::string& path) {
  for (auto& [id, file] : files) {
    if (path == file->path) {
      return id;
    }
  }

  return 0;
}

std::shared_ptr<File> FileManager::get(FileId id) {
  if (files.contains(id)) {
    return files[id];
  }

  return {nullptr};
}

std::string FileManager::getOrLoad(const std::string& path) {
  if (auto id = lookup(path)) {
    return files[id]->contents;
  }

  return files[load(path)]->contents;
}
