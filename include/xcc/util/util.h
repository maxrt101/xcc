#pragma once

/**
 * Shorthand for finding a value in a vector
 *
 * @code{.c}
 *   std::vector<std::string> values;
 *   XCC_FIND_IN_VECTOR(values, "abc", v) {
 *     // v is an iterator to std::string here
 *   }
 * @endcode
 *
 * @param __vec     Vector to find __element in
 * @param __element Element to find in __vec
 * @param __it      Variable name for iterator
 */
#define XCC_FIND_IN_VECTOR(__vec, __element, __it)            \
  auto __it = std::find_if(__vec.begin(), __vec.end(),        \
    [&](const auto& x) { return x == __element; });           \
  if (__it != __vec.end())

/**
 * Shorthand for finding a value in a vector of std::pair by
 * first element of the pair
 *
 * @code{.c}
 *   std::vector<std::pair<int, std::string>> values;
 *   XCC_FIND_IN_VECTOR(values, 42, v) {
 *     // v is an iterator to std::pair here
 *   }
 * @endcode
 *
 * @param __vec     Vector to find __element in
 * @param __element Element to find in __vec
 * @param __it      Variable name for iterator
 */
#define XCC_FIND_IN_VECTOR_BY_PAIR0(__vec, __element, __it)   \
  auto __it = std::find_if(__vec.begin(), __vec.end(),        \
    [&](const auto& x) { return x.first == __element; });     \
  if (__it != __vec.end())

/**
 * Shorthand for finding a value in a vector of std::pair by
 * second element of the pair
 *
 * @code{.c}
 *   std::vector<std::pair<int, std::string>> values;
 *   XCC_FIND_IN_VECTOR(values, "abc", v) {
 *     // v is an iterator to std::pair here
 *   }
 * @endcode
 *
 * @param __vec     Vector to find __element in
 * @param __element Element to find in __vec
 * @param __it      Variable name for iterator
 */
#define XCC_FIND_IN_VECTOR_BY_PAIR1(__vec, __element, __it)   \
  auto __it = std::find_if(__vec.begin(), __vec.end(),        \
    [&](const auto& x) { return x.second == __element; });    \
  if (__it != __vec.end())


namespace xcc {

/* Checks of value is in any of args */
template <typename T, typename... Args>
bool oneOf(T&& value, Args&&... args) {
  return ((value == args) || ...);
}

}
