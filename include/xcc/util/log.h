#pragma once

#include <cstdint>
#include <cstdarg>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace xcc::util::log {

namespace outputs {

class OutputBase {
public:
  OutputBase() = default;
  virtual ~OutputBase() = default;

  virtual void output(const std::string& message) = 0;
};

class OutputStdout : public OutputBase {
private:
  static std::shared_ptr<OutputStdout> instance;

public:
  OutputStdout();
  ~OutputStdout() = default;

  void output(const std::string& message) override;

  static std::shared_ptr<OutputStdout> get();
};

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

enum class Level : uint8_t {
  NONE = 0,
  DEBUG,
  INFO,
  WARN,
  ERROR,
  FATAL,
};

class Logger {
private:
  Level level;
  std::string name;
  bool enabled;
  std::vector<std::shared_ptr<outputs::OutputBase>> outputs;

public:
  explicit Logger(std::string name,
                  const std::initializer_list<std::shared_ptr<outputs::OutputBase>>& outputs
                    = {outputs::OutputStdout::get()});

  virtual ~Logger() = default;

  void setLogLevel(Level level);
  void setEnable(bool enabled);
  bool isEnabled();
  std::string getName();

  void print(std::string fmt, ...);
  void debug(std::string fmt, ...);
  void info(std::string fmt, ...);
  void warn(std::string fmt, ...);
  void error(std::string fmt, ...);
  void fatal(std::string fmt, ...);

private:
  std::string createLogString(Level level, const std::string& fmt, va_list args);
  void output(Level level, const std::string& fmt, va_list args);
};

void registerModule(Logger * logger);
void enableModule(const std::string& name, bool enable);

void cleanup();

}
