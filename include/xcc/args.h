#pragma once

#include <string>
#include <vector>

namespace xcc::args {

/**
 * Contains all command-line arguments that could be parsed
 */
struct Arguments {
  bool                     help;
  bool                     version;
  bool                     verbose;
  bool                     compile_only;
  bool                     run;
  std::string              target;
  std::string              machine;
  std::string              output;
  std::vector<std::string> files;

  Arguments();
};

/**
 * Parses command line arguments
 *
 * @param argc Argument count (can be passed as-is from main())
 * @param argv Argument vector (can be passed as-is from main())
 * @return Parsed arguments
 */
Arguments parse(int argc, char ** argv);

} /* namespace xcc::args */
