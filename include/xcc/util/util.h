#pragma once

#include <vector>
#include <string>

/**
 * Shorthand for checking if a value is contained by a vector
 *
 * @code{.c}
 *   std::vector<std::string> values;
 *   if (XCC_VECTOR_CONTAINS(values, "abc")) {
 *     // "abc" is in values
 *   }
 * @endcode
 *
 * @param __vec     Vector to find __element in
 * @param __element Element to find in __vec
 */
#define XCC_VECTOR_CONTAINS(__vec, __element) \
  (std::find(__vec.begin(), __vec.end(), __element) != __vec.end())

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
#define XCC_IF_VECTOR_CONTAINS(__vec, __element, __it)        \
  auto __it = std::find_if(__vec.begin(), __vec.end(),        \
    [&](const auto& x) { return x == __element; });           \
  if (__it != __vec.end())

/**
 * Shorthand for finding a value in a vector of std::pair by
 * first element of the pair
 *
 * @code{.c}
 *   std::vector<std::pair<int, std::string>> values;
 *   XCC_IF_VECTOR_CONTAINS_BY_PAIR0(values, 42, v) {
 *     // v is an iterator to std::pair here
 *   }
 * @endcode
 *
 * @param __vec     Vector to find __element in
 * @param __element Element to find in __vec
 * @param __it      Variable name for iterator
 */
#define XCC_IF_VECTOR_CONTAINS_BY_PAIR0(__vec, __element, __it)   \
  auto __it = std::find_if(__vec.begin(), __vec.end(),            \
    [&](const auto& x) { return x.first == __element; });         \
  if (__it != __vec.end())

/**
 * Shorthand for finding a value in a vector of std::pair by
 * second element of the pair
 *
 * @code{.c}
 *   std::vector<std::pair<int, std::string>> values;
 *   XCC_IF_VECTOR_CONTAINS_BY_PAIR1(values, "abc", v) {
 *     // v is an iterator to std::pair here
 *   }
 * @endcode
 *
 * @param __vec     Vector to find __element in
 * @param __element Element to find in __vec
 * @param __it      Variable name for iterator
 */
#define XCC_IF_VECTOR_CONTAINS_BY_PAIR1(__vec, __element, __it)   \
  auto __it = std::find_if(__vec.begin(), __vec.end(),            \
    [&](const auto& x) { return x.second == __element; });        \
  if (__it != __vec.end())


namespace xcc::util {

/**
 * Checks if `value` is one of the `args`
 *
 * @tparam T     `value` type
 * @tparam Args  `args` pack type. Each individual element should be comparable to `T`
 * @param  value Value to check if present in `args`
 * @param  args  Pack to check if `value` is present in
 * @return @c true if `value` is in `args`
 */
template <typename T, typename... Args>
bool oneOf(T&& value, Args&&... args) {
  return ((value == args) || ...);
}

/**
 * Merge two vectors together
 *
 * @tparam T Vector value type
 * @param  first Vector to merge into
 * @param  second Vector to merge
 * @return first + second
 */
template <typename T>
std::vector<T> mergeVectors(std::vector<T> first, const std::vector<T>& second) {
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

/**
 * Return a vector, that is a subvector of `vec` with range [start; end]
 *
 * @tparam T Vector value type
 * @param  vec Vector to get subvector from
 * @param  start Start index
 * @param  end End index (if 0 - `start` is set to 0 and `end` is set to `start`)
 * @return `vec[start; end]`
 */
template <typename T>
std::vector<T> sliceVector(const std::vector<T>& vec, ssize_t start, ssize_t end = 0) {
  if (!end) {
    end = start;
    start = 0;
  }

  if (start < 0) {
    start = (ssize_t)vec.size() + start;
  }

  if (end < 0) {
    end = (ssize_t)vec.size() + end;
  }

  std::vector<T> res;

  for (ssize_t i = start; i < end; ++i) {
    res.push_back(vec[i]);
  }

  return res;
}

/**
 * Extract pair<K, V>::first from a vector<pair<K, V>>
 *
 * @tparam K Type of first pair element
 * @tparam V Type of second pair element
 * @param vec Vector of pairs
 * @return Vector of K
 */
template <typename K, typename V>
std::vector<K> pairVectorExtractFirst(const std::vector<std::pair<K, V>>& vec) {
  std::vector<K> res;

  for (auto& [first, _] : vec) {
    res.push_back(first);
  }

  return res;
}

}
