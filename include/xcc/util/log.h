#pragma once

#include <cstdint>
#include <string>
#include <format>
#include <vector>
#include <unordered_map>
#include "xcc/util/string.h"

// #ifndef XCC_LOG_DEFAULT_FLAGS
// #define XCC_LOG_DEFAULT_FLAGS xcc::util::log::Flag::ENABLE_COLOR | xcc::util::log::Flag::ADD_NEWLINE
// #endif

namespace xcc::util::log {

namespace outputs {

/**
 * Base output class
 *
 * Used to create custom outputs for log data
 */
class OutputBase {
public:
  OutputBase() = default;
  virtual ~OutputBase() = default;

  virtual void output(const std::string& message) = 0;
};

/**
 * Output implementation that redirects logs to stdout
 *
 * Singleton
 */
class OutputStdout : public OutputBase {
private:
  static std::shared_ptr<OutputStdout> instance;

public:
  OutputStdout();
  ~OutputStdout() = default;

  void output(const std::string& message) override;

  static std::shared_ptr<OutputStdout> get();
};

/**
 * Output implementation that redirects logs to a file
 *
 * Has an instance pool by filename
 */
class OutputFile : public OutputBase {
private:
  static std::unordered_map<std::string, std::shared_ptr<OutputFile>> instances;

private:
  std::string filename;

public:
  OutputFile(std::string filename);
  ~OutputFile() = default;

  void output(const std::string& message) override;

  static std::shared_ptr<OutputFile> get(std::string filename);
};

}

/**
 * Logger flags. Basically feature toggles
 */
struct Flag {
  static constexpr uint32_t NONE             = 0;
  static constexpr uint32_t DISABLE_COLOR    = 1 << 0;
  static constexpr uint32_t SPLIT_ON_NEWLINE = 1 << 1;
  static constexpr uint32_t DONT_ADD_NEWLINE = 1 << 2;
};

/**
 * ANSI Color Codes
 */
namespace Color {
  constexpr char RESET[]   = "\u001b[0m";
  constexpr char BLACK[]   = "\u001b[30m";
  constexpr char RED[]     = "\u001b[31m";
  constexpr char RED_RED[] = "\u001b[41m";
  constexpr char GREEN[]   = "\u001b[32m";
  constexpr char YELLOW[]  = "\u001b[33m";
  constexpr char BLUE[]    = "\u001b[34m";
  constexpr char MAGENTA[] = "\u001b[35m";
  constexpr char CYAN[]    = "\u001b[36m";
  constexpr char WHITE[]   = "\u001b[37m";
}

/**
 * Log Level
 */
enum class Level : uint8_t {
  NONE = 0,
  DEBUG,
  INFO,
  WARN,
  ERROR,
  FATAL,
};

/**
 * Logger context
 * Every module should use its own logger instance
 *
 * Example:
 * @code{.c}
 *   static auto logger = xcc::util::log::Logger("MAIN");
 *   ...
 *   logger.debug("Logger test");
 *   logger.info("Logger test");
 *   logger.warn("Logger test");
 *   logger.error("Logger test");
 *   logger.fatal("Logger test");
 *   ...
 *   logger.print("Logger test\n");
 * @encode
 */
class Logger {
private:
  /** Maximal configured logger leg level */
  Level level;

  /** Logger name */
  std::string name;

  /** Is enabled */
  bool enabled;

  /** Flags */
  uint32_t flags = Flag::NONE;

  /** Outputs */
  std::vector<std::shared_ptr<outputs::OutputBase>> outputs;

public:
  /**
   * Logger constructor
   *
   * @param name Logger (module) name
   * @param outputs Outputs list (by default only stdout output is configured)
   */
  explicit Logger(std::string name, uint32_t flags = Flag::NONE,
                  const std::initializer_list<std::shared_ptr<outputs::OutputBase>>& outputs
                    = {outputs::OutputStdout::get()});

  virtual ~Logger() = default;

  /**
   * Maximal log level
   *
   * @param level Log level
   */
  void setLogLevel(Level level);

  /**
   * Enables/Disables logger completely
   *
   * @param enabled true to enable, false to disable
   */
  void setEnable(bool enabled);

  /**
   * Returns true if logger is enabled
   */
  bool isEnabled();

  /**
   * Sets flag
   */
  void setFlag(uint32_t flag);

  /**
   * Clears flag
   */
  void clearFlag(uint32_t flag);

  /**
   * Return value of a flag
   */
  bool hasFlag(uint32_t flag);

  /**
   * Return logger (module) name
   */
  std::string getName();

  /**
   * Raw print without additional formatting
   *
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void print(const std::format_string<Args...> fmt, Args&&... args) {
    auto string = std::format(fmt, std::forward<Args>(args)...);

    std::vector<std::string> msg_parts;

    if (hasFlag(Flag::SPLIT_ON_NEWLINE)) {
      msg_parts = strsplit(string, "\n");
    } else {
      msg_parts.push_back(string);
    }

    for (auto& msg : msg_parts) {
      for (auto & out : outputs) {
        out->output(msg + (hasFlag(Flag::SPLIT_ON_NEWLINE) ? (hasFlag(Flag::DONT_ADD_NEWLINE) ? "" : "\n") : ""));
      }
    }
  }

  /**
   * Prints with DEBUG log level
   *
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void debug(const std::format_string<Args...> fmt, Args&&... args) {
    output(Level::DEBUG, fmt, std::forward<Args>(args)...);
  }

  /**
   * Prints with INFO log level
   *
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void info(const std::format_string<Args...> fmt, Args&&... args) {
    output(Level::INFO, fmt, std::forward<Args>(args)...);
  }

  /**
   * Prints with WARN log level
   *
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void warn(const std::format_string<Args...> fmt, Args&&... args) {
    output(Level::WARN, fmt, std::forward<Args>(args)...);
  }

  /**
   * Prints with ERROR log level
   *
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void error(const std::format_string<Args...> fmt, Args&&... args) {
    output(Level::ERROR, fmt, std::forward<Args>(args)...);
  }

  /**
   * Prints with FATAL log level
   *
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void fatal(const std::format_string<Args...> fmt, Args&&... args) {
    output(Level::FATAL, fmt, std::forward<Args>(args)...);
  }

private:
  /**
   * Generates log header with log level & logger name
   *
   * @param level Log level
   * @return Log header (level + module)
   */
  std::string createLogHeader(Level level);

  /**
   * Puts log entry into every output configured
   *
   * @param level Log level
   * @param fmt Format string
   * @param args vargs for fmt
   */
  template <typename... Args>
  void output(Level level, const std::format_string<Args...> fmt, Args&&... args) {
    if (!isEnabled()) {
      return;
    }

    // TODO: this->level < level check

    auto header = createLogHeader(level);
    auto log_string = std::format(fmt, std::forward<Args>(args)...);

    std::vector<std::string> msg_parts;

    if (hasFlag(Flag::SPLIT_ON_NEWLINE)) {
      msg_parts = strsplit({log_string}, "\n");
    } else {
      msg_parts.push_back(log_string);
    }

    for (auto& msg : msg_parts) {
      for (auto & out : outputs) {
        out->output(header + msg + (hasFlag(Flag::DONT_ADD_NEWLINE) ? "" : "\n"));
      }
    }
  }
};

/**
 * Registers logger in global logger list
 *
 * @param logger Logger instance
 */
void registerModule(Logger * logger);

/**
 * Enables/Disables logger by name
 *
 * @param name Logger name
 * @param enable enable/disable
 */
void enableModule(const std::string& name, bool enable);

/**
 * Cleans up all registered loggers
 */
void cleanup();

}
