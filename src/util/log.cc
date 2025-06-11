#include "xcc/util/log.h"
#include "xcc/exceptions.h"
#include <fstream>

#define LOG_LEVEL_CASE(__name, __color)           \
  case Level::__name:                             \
    log_level_string = #__name;                   \
    log_level_color = Color::__color;             \
    break;

using namespace xcc::util;

static std::unordered_map<std::string, log::Logger*> * loggers = nullptr;

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

std::string log::Logger::createLogHeader(Level level) {
  std::string log_level_string;
  std::string log_level_color;

  switch (level) {
    LOG_LEVEL_CASE(FATAL, RED_RED);
    LOG_LEVEL_CASE(ERROR, RED);
    LOG_LEVEL_CASE(WARN, YELLOW);
    LOG_LEVEL_CASE(INFO, CYAN);
    LOG_LEVEL_CASE(DEBUG, BLUE);

    case Level::NONE:
      break;

    default:
      log_level_string = "<?>";
      break;
  }

  if (!hasFlag(Flag::DISABLE_COLOR)) {
    if (level != Level::NONE) {
      return std::format(
        "[{}{}{}][{}{}{}]: ",
            log_level_color, log_level_string, Color::RESET,
            Color::MAGENTA, name, Color::RESET);
    } else {
      return std::format(
        "[{}{}{}]: ",
            Color::MAGENTA, name, Color::RESET);
    }
  } else {
    if (level != Level::NONE) {
      return std::format("[{}][{}]: ", log_level_string, name);
    } else {
      return std::format("[{}]: ", name);
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
