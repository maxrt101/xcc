#pragma once

#include <initializer_list>
#include <unordered_map>
#include <string>

namespace xcc {

template <typename T>
class PrefixTree {
  struct Node {
    char prefix = '\0';
    T value;
    size_t depth = 0;
    std::unordered_map<char, Node> children;
  };

  std::unordered_map<char, Node> roots;
  const T null_value;

public:
  struct FindResult {
    T value;
    size_t depth;
  };

public:
  explicit PrefixTree(T null_value) : null_value(null_value) {}
  ~PrefixTree() = default;

  PrefixTree(T null_value, const std::initializer_list<std::pair<std::string, T>>& il) : null_value(null_value)  {
    for (auto& element : il) {
      append(element.first, element.second);
    }
  }

  void append(const std::string& prefix, T value) {
    if (roots.find(prefix[0]) == roots.end()) {
      roots[prefix[0]] = Node {
        prefix[0],
        prefix.size() == 1 ? value : null_value,
        1
      };
    }

    if (prefix.size() > 1) {
      append(roots[prefix[0]], prefix, 1, value);
    } else {
      roots[prefix[0]].value = value;
    }
  }

  FindResult find(const std::string& prefix, size_t start_index = 0) const {
    if (roots.find(prefix[start_index]) != roots.end()) {
      if (roots.at(prefix[start_index]).children.empty()) {
        return {roots.at(prefix[start_index]).value, 1};
      } else {
        FindResult result = find(roots.at(prefix[start_index]), prefix, start_index + 1);
        if (result.value == null_value) {
          return {roots.at(prefix[start_index]).value, 1};
        } else {
          return result;
        }
      }
    }

    return {null_value, 0};
  }

  void print() {
    for (auto& [prefix, node] : roots) {
      print(node, std::string("") + prefix);
    }
  }

private:
  void append(Node& node, const std::string& prefix, size_t index, T value) {
    if (node.children.find(prefix[index]) == node.children.end()) {
      node.children[prefix[index]] = Node {prefix[index], null_value, node.depth + 1};

      if (index >= prefix.size() - 1) {
        node.children[prefix[index]].value = value;
        return;
      }
    } else if (index == prefix.size()) {
      node.children[prefix[index]].value = value;
      return;
    }

    append(node.children[prefix[index]], prefix, index + 1, value);
  }

  FindResult find(const Node& node, const std::string& prefix, size_t index) const {
    if (node.children.empty()) {
      return {node.value, node.depth};
    } else {
      if (node.children.find(prefix[index]) != node.children.end()) {
        return find(node.children.at(prefix[index]), prefix, index + 1);
      }
    }

    return {null_value, node.depth};
  }

  void print(Node& node, std::string prefix) {
    for (auto& [child_prefix, child] : node.children) {
      print(child, prefix + child_prefix);
    }

    printf("%s\n", prefix.c_str());
  }
};

} /* namespace xcc */