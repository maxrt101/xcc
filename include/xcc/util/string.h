#pragma once

#include <string>
#include <vector>

namespace xcc::util {
/**
 * Splits string into a vector of parts using delimiter
 *
 * @param str String to split
 * @param delimiter Delimiter to split by (' ' by default)
 * @return Vector of string parts, split by delimiter
 */
std::vector<std::string> strsplit(const std::string& str, const std::string& delimiter = " ");

/**
 * Joins multiple string into one using delimiter
 *
 * @param parts Strings to join
 * @param delimiter Delimiter to put inbetween parts
 * @return Joined string
 */
std::string strjoin(const std::vector<std::string>& parts, const std::string& delimiter = ", ");

/**
 * Replaces occurrences of `from` with `to` in-place in `str`
 *
 * @param str Source string
 * @param from String to replace
 * @param to String to replace with
 */
void strreplace(std::string& str, const std::string& from, const std::string& to);

/**
 * Adds/Removes escape sequences
 *
 * @param str Source string
 * @param add If true - will escape escape sequences
 *            if false - will replace with actual ascii special characters
 */
std::string strescseq(const std::string& str, bool add = true);

}
