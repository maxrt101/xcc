#include "xcc/util/log.h"
#include "xcc/util/string.h"
#include "xcc/exceptions.h"
#include <fstream>

#define LOG_LINE_BUFFER_SIZE 256

#define LOG_LEVEL_CASE(_name, _color)       \
  case Level::_name:                        \
    index += snprintf(buffer,               \
                    sizeof(buffer) - index, \
                    "[%s" #_name,           \
                    Color::_color);         \
    break

#define LOG_OUTPUT_STRING(_level, _fmt, _out) \
  va_list args;                               \
  va_start(args, _fmt);                       \
  output(_level, _fmt, args);                 \
  va_end(args)


using namespace xcc::util;

static std::unordered_map<std::string, log::Logger*> * loggers = NULL;

std::shared_ptr<log::outputs::OutputStdout> log::outputs::OutputStdout::instance;

log::outputs::OutputStdout::OutputStdout()
    : OutputBase() {}

void log::outputs::OutputStdout::output(const std::string& message) {
  printf("%s", message.c_str());
}

std::shared_ptr<log::outputs::OutputStdout> log::outputs::OutputStdout::get() {
  if (!instance) {
    instance = std::make_shared<OutputStdout>();
  }

  return instance;
}

std::unordered_map<std::string, std::shared_ptr<log::outputs::OutputFile>> log::outputs::OutputFile::instances;

log::outputs::OutputFile::OutputFile(std::string filename)
  : OutputBase(), filename(std::move(filename)) {}

void log::outputs::OutputFile::output(const std::string& message) {
  std::fstream file {filename, file.out};

  if (file.is_open()) {
    file << message;
    file.close();
  }
}

std::shared_ptr<log::outputs::OutputFile> log::outputs::OutputFile::get(std::string filename) {
  if (instances.find(filename) == instances.end()) {
    instances[filename] = std::make_unique<OutputFile>(filename);
  }

  return instances[filename];
}

log::Logger::Logger(std::string name, uint32_t flags, const std::initializer_list<std::shared_ptr<outputs::OutputBase>>& outputs)
  : level(Level::NONE), name(std::move(name)), enabled(true), flags((uint32_t) flags)
{
  for (auto out = outputs.begin(); out != outputs.end(); out++) {
    this->outputs.push_back(*out);
  }

  registerModule(this);
}

void log::Logger::setLogLevel(Level level) {
  this->level = level;
}

void log::Logger::setEnable(bool enabled) {
  this->enabled = enabled;
}

bool log::Logger::isEnabled() {
  return enabled;
}

void log::Logger::setFlag(uint32_t flag) {
  flags |= flag;
}

void log::Logger::clearFlag(uint32_t flag) {
  flags &= ~flag;
}

bool log::Logger::hasFlag(uint32_t flag) {
  return flags & flag;
}

std::string log::Logger::getName() {
  return name;
}

void log::Logger::print(std::string fmt, ...) {
  char buffer[LOG_LINE_BUFFER_SIZE];

  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt.c_str(), args);
  va_end(args);

  std::vector<std::string> msg_parts;

  if (hasFlag(Flag::SPLIT_ON_NEWLINE)) {
    msg_parts = strsplit({buffer}, "\n");
  } else {
    msg_parts.emplace_back(buffer);
  }

  for (auto& msg : msg_parts) {
    for (auto & out : outputs) {
      out->output(msg + (hasFlag(Flag::SPLIT_ON_NEWLINE) ? "\n" : ""));
    }
  }
}

void log::Logger::debug(std::string fmt, ...) {
  LOG_OUTPUT_STRING(Level::DEBUG, fmt, msg);
}

void log::Logger::info(std::string fmt, ...) {
  LOG_OUTPUT_STRING(Level::INFO, fmt, msg);
}

void log::Logger::warn(std::string fmt, ...) {
  LOG_OUTPUT_STRING(Level::WARN, fmt, msg);
}

void log::Logger::error(std::string fmt, ...) {
  LOG_OUTPUT_STRING(Level::ERROR, fmt, msg);
}

void log::Logger::fatal(std::string fmt, ...) {
  LOG_OUTPUT_STRING(Level::FATAL, fmt, msg);
}

std::string log::Logger::createLogString(Level level, const std::string& fmt, va_list args) {
  char buffer[LOG_LINE_BUFFER_SIZE];
  size_t index = 0;

  if (hasFlag(Flag::ENABLE_COLOR)) {
    switch (level) {
      LOG_LEVEL_CASE(FATAL, RED_RED);
      LOG_LEVEL_CASE(ERROR, RED);
      LOG_LEVEL_CASE(WARN, YELLOW);
      LOG_LEVEL_CASE(INFO, CYAN);
      LOG_LEVEL_CASE(DEBUG, BLUE);

      case Level::NONE:
        break;

      default:
        index += snprintf(buffer, sizeof(buffer) - index, "<?>");
      break;
    }
  } else {
    index += snprintf(buffer, sizeof(buffer) - index, "[");
  }

  if (level != Level::NONE) {
    if (hasFlag(Flag::ENABLE_COLOR)) {
      index += snprintf(buffer + index, sizeof(buffer) - index, "%s]", Color::RESET);
    } else {
      index += snprintf(buffer, sizeof(buffer) - index, "]");
    }
  }

  if (hasFlag(Flag::ENABLE_COLOR)) {
    index += snprintf(buffer + index, sizeof(buffer) - index, "[%s%s%s]: ", Color::MAGENTA, name.c_str(), Color::RESET);
  } else {
    index += snprintf(buffer + index, sizeof(buffer) - index, "[%s]: ", name.c_str());
  }

  index += vsnprintf(buffer + index, sizeof(buffer) - index, fmt.c_str(), args);

  if (level != Level::NONE) {
    index += snprintf(buffer + index, sizeof(buffer) - index, "\n");
  }

  buffer[index] = '\0';

  return {buffer};
}

void log::Logger::output(Level level, const std::string& fmt, va_list args) {
  if (!isEnabled()) {
    return;
  }

  // TODO: this->level < level check

  auto log_string = createLogString(level, fmt, args);

  std::vector<std::string> msg_parts;

  if (hasFlag(Flag::SPLIT_ON_NEWLINE)) {
    msg_parts = strsplit({log_string}, "\n");
  } else {
    msg_parts.emplace_back(log_string);
  }

  for (auto& msg : msg_parts) {
    for (auto & out : outputs) {
      out->output(msg + (hasFlag(Flag::SPLIT_ON_NEWLINE) ? "\n" : ""));
    }
  }
}

void log::registerModule(Logger * logger) {
  assertThrow(
      logger,
      std::runtime_error("registerModule got NULL pointer as logger")
  );

  if (!loggers) {
    loggers = new std::unordered_map<std::string, log::Logger*>();
  }

  (*loggers)[logger->getName()] = logger;
}

void log::enableModule(const std::string& name, bool enable) {
  assertThrow(
      loggers,
      std::runtime_error("enableModule is called before global logger init")
  );

  if (loggers->find(name) == loggers->end()) {
    throw std::runtime_error(name + " - logger not found");
  }

  (*loggers)[name]->setEnable(enable);
}

void log::cleanup() {
  delete loggers;
}
