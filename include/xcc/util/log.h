#pragma once

#include <cstdint>
#include <cstdarg>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

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

  /** Outputs */
  std::vector<std::shared_ptr<outputs::OutputBase>> outputs;

public:
  /**
   * Logger constructor
   *
   * @param name Logger (module) name
   * @param outputs Outputs list (by default only stdout output is configured)
   */
  explicit Logger(std::string name,
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
   * Return logger (module) name
   */
  std::string getName();

  /**
   * Raw print without additional formatting
   *
   * @param fmt Format string
   * @param ... vargs for fmt
   */
  void print(std::string fmt, ...);

  /**
   * Prints with DEBUG log level
   *
   * @param fmt Format string
   * @param ... vargs for fmt
   */
  void debug(std::string fmt, ...);

  /**
   * Prints with INFO log level
   *
   * @param fmt Format string
   * @param ... vargs for fmt
   */
  void info(std::string fmt, ...);

  /**
   * Prints with WARN log level
   *
   * @param fmt Format string
   * @param ... vargs for fmt
   */
  void warn(std::string fmt, ...);

  /**
   * Prints with ERROR log level
   *
   * @param fmt Format string
   * @param ... vargs for fmt
   */
  void error(std::string fmt, ...);

  /**
   * Prints with FATAL log level
   *
   * @param fmt Format string
   * @param ... vargs for fmt
   */
  void fatal(std::string fmt, ...);

private:
  /**
   * Generates log string with log level, logger name and fmt+vargs passed by the user
   *
   * @param level Log level
   * @param fmt Format string
   * @param args vargs for fmt
   * @return Complete log string
   */
  std::string createLogString(Level level, const std::string& fmt, va_list args);

  /**
   * Puts log entry into every output configured
   *
   * @param level Log level
   * @param fmt Format string
   * @param args vargs for fmt
   */
  void output(Level level, const std::string& fmt, va_list args);
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
