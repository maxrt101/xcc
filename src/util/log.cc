#include "xcc/util/log.h"
#include "xcc/exceptions.h"
#include <fstream>

/**
 * Helper for setting log level string and color in a log level switch-case
 */
#define LOG_LEVEL_CASE(__name, __color)           \
  case Level::__name:                             \
    log_level_string = #__name;                   \
    log_level_color = Color::__color;             \
    break;

using namespace xcc;

/**
 * Global static map of loggers
 *
 * TODO: Why not make it a plain value instead of a pointer?
 * TODO: Why not save loggers as a shared_ptr instead of raw pointer?
 */
static std::unordered_map<std::string, std::shared_ptr<log::Logger>> * loggers = nullptr;

/**
 * Global static list of outputs
 */
static std::vector<std::shared_ptr<log::outputs::OutputBase>> outputs;

std::vector<std::shared_ptr<log::outputs::OutputBase>>& log::outputs::get() {
  return ::outputs;
}

std::shared_ptr<log::outputs::OutputStdout> log::outputs::OutputStdout::instance;

log::outputs::OutputStdout::OutputStdout() : OutputBase() {}

void log::outputs::OutputStdout::output(const std::string& message) {
  printf("%s", message.c_str());
}

std::shared_ptr<log::outputs::OutputStdout> log::outputs::OutputStdout::get() {
  if (!instance) {
    instance = std::make_shared<OutputStdout>();
  }

  return instance;
}

std::shared_ptr<log::outputs::OutputStderr> log::outputs::OutputStderr::instance;

log::outputs::OutputStderr::OutputStderr() : OutputBase() {}

void log::outputs::OutputStderr::output(const std::string& message) {
  fprintf(stderr, "%s", message.c_str());
}

std::shared_ptr<log::outputs::OutputStderr> log::outputs::OutputStderr::get() {
  if (!instance) {
    instance = std::make_shared<OutputStderr>();
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

log::Logger::Logger(std::string name, uint32_t flags)
  : level(Level::NONE), name(std::move(name)), enabled(false), flags((uint32_t) flags) {}

log::Logger& log::Logger::get(std::string name, uint32_t flags) {
  if (!loggers) {
    loggers = new std::unordered_map<std::string, std::shared_ptr<Logger>>();
  }

  if (!loggers->contains(name)) {
    registerModule(std::make_shared<Logger>(name, flags));
  }

  Logger& ref = *(*loggers)[name];

  return *(*loggers)[name];
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
    }
    return std::format(
      "[{}{}{}]: ",
          Color::MAGENTA, name, Color::RESET);
  }

  if (level != Level::NONE) {
    return std::format("[{}][{}]: ", log_level_string, name);
  }

  return std::format("[{}]: ", name);
}

void log::registerOutput(std::shared_ptr<outputs::OutputBase> output) {
  ::outputs.push_back(output);
}

void log::registerModule(std::shared_ptr<Logger> logger) {
  assertThrow(
      logger.get(),
      std::runtime_error("registerModule got NULL pointer as logger")
  );

  if (!loggers) {
    loggers = new std::unordered_map<std::string, std::shared_ptr<Logger>>();
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

std::vector<std::string> log::getModuleNames() {
  assertThrow(
      loggers,
      std::runtime_error("getModuleNames is called before global logger init")
  );

  std::vector<std::string> res;

  for (auto& [name, _] : *loggers) {
    res.push_back(name);
  }

  return res;
}

void log::cleanup() {
  delete loggers;
}
