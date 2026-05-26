#include "xcc/util/filemng.h"
#include "xcc/util/fs.h"
#include "xcc/util/log.h"
#include <filesystem>

using namespace xcc;

static auto& logger = log::Logger::get("FILEMNG");

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

void File::process() {
  size_t start  = 0;
  size_t offset = 0;
  size_t line   = 1;

  while (offset < contents.size()) {
    if (contents[offset] == '\n') {
      if (contents[start] == '\n') {
        start += 1;
      }

      lines[line] = LineInfo(line, start, offset - start);

      line   += 1;
      start   = offset;
    }

    offset++;
  }

  lines[line] = LineInfo(line, start, offset - start);
}

FileId FileManager::load(std::string path) {
  path = std::filesystem::path(path).string();

  if (auto id = lookup(path)) {
    return id;
  }

  FileId id = FileManager::id++;

  auto file = std::make_shared<File>(id, path, fs::readFile(path));

  file->process();

  files[id] = file;

  return id;
}

FileId FileManager::lookup(std::string path) {
  path = std::filesystem::path(path).string();

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

std::string FileManager::getOrLoad(std::string path) {
  path = std::filesystem::path(path).string();

  if (auto id = lookup(path)) {
    return files[id]->contents;
  }

  return files[load(path)]->contents;
}

void FileManager::purge(FileId id) {
  files.erase(id);
}

FileId FileManager::createVirtual(std::string path, const std::string& contents) {
  path = std::filesystem::path(path).string();

  FileId id = FileManager::id++;

  auto file = std::make_shared<File>(id, path, contents);

  file->process();

  files[id] = file;

  return id;
}
