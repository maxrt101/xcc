#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace xcc::str {

struct BaseDetermineResult {
  int         base;
  std::string value;
};

/**
 * Check if `str` contains `sub`
 *
 * @param str String to check for `sub`
 * @param sub Substring to look for
 * @return @c true if `str` contains `sub`
 */
bool contains(const std::string& str, const std::string& sub);

/**
 * Check if `str` contains any of the `substrings`
 *
 * @tparam Args       All should be std::string
 * @param  str        String to check for `substrings`
 * @param  substrings Substrings to look for in `str`
 * @return @c true if `str` contains at least one of `substrings`
 */
template <typename... Args>
bool contains(const std::string& str, Args&&... substrings) {
  return ((contains(str, substrings)) || ...);
}

/**
 * Splits string into a vector of parts using delimiter
 *
 * @param str String to split
 * @param delimiter Delimiter to split by (' ' by default)
 * @return Vector of string parts, split by delimiter
 */
std::vector<std::string> split(const std::string& str, const std::string& delimiter = " ");

/**
 * Joins multiple string into one using delimiter
 *
 * @param parts     Strings to join
 * @param delimiter Delimiter to put inbetween parts
 * @param omit_last If true - will not append extra delimiter at the end
 * @return Joined string
 */
std::string join(const std::vector<std::string>& parts, const std::string& delimiter = ", ", bool omit_last = true);

/**
 * Replaces occurrences of `from` with `to` in-place in `str`
 *
 * @param str Source string
 * @param from String to replace
 * @param to String to replace with
 */
void replace(std::string& str, const std::string& from, const std::string& to);

/**
 * Adds/Removes escape sequences
 *
 * @param str Source string
 * @param add If true - will escape escape sequences
 *            if false - will replace with actual ascii special characters
 */
std::string escseq(const std::string& str, bool add = true);

/**
 * Compile-time djb2 string hash function
 *
 * @param s String
 * @return Hash
 */
constexpr uint64_t hash(const char * s) {
  uint64_t hash = 5381;
  int c;

  while ((c = *s++)) {
    hash = (hash << 5) + hash + c;
  }

  return hash;
}

/**
 * Convert a number to string and add an ordinal suffix
 *
 * 1 -> "1st"
 * 2 -> "2nd"
 * 3 -> "3rd"
 * 4 -> "4th"
 * ...
 *
 * @param num Number to convert
 * @return stringified number with ordinal suffix
 */
std::string toStringWithOrdinalSuffix(int num);

/**
 * Determine base of number literal, returning base & value without prefix
 */
BaseDetermineResult determineBase(const std::string& value);

}
