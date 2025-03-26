#pragma once

#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace xcc {

template <typename K, typename V>
class OrderedMap {
  std::unordered_map<K, V> data;
  std::vector<K> order;

public:
  OrderedMap() = default;
  virtual ~OrderedMap() = default;

  OrderedMap(const OrderedMap& rhs) {
    data = rhs.data;
    order = rhs.order;
  }

  OrderedMap(const std::initializer_list<std::pair<K, V>>& il) {
    for (auto& item : il) {
      append(item.first, item.second);
    }
  }

  V& operator [] (const K& key) {
    if (has(key)) {
      return data[key];
    }

    order.push_back(key);
    return data[key];
  }

  const V& operator [] (const K& key) const {
    if (has(key)) {
      return data.at(key);
    }

    throw std::runtime_error("OrderedMap: key error");
  }

  V& operator [] (const size_t index) {
    if (index < order.size()) {
      return data[order[index]];
    }

    throw std::runtime_error("OrderedMap: index error");
  }

  const V& operator [] (const size_t index) const {
    if (index < order.size()) {
      return data[order[index]];
    }

    throw std::runtime_error("OrderedMap: index error");
  }

  bool has(const K& key) const {
    return data.find(key) != data.end();
  }

  bool empty() const {
    return order.empty();
  }

  size_t size() const {
    return data.size();
  }

  void append(const K& key, V value) {
    (*this)[key] = value;
  }

  void remove(const K& key) {
    if (has(key)) {
      data.erase(key);
      order.erase(std::find(order.begin(), order.end(), key));
    }
  }

  size_t get_order(const K& key) {
    if (has(key)) {
      return order.find(key) - order.begin();
    }

    return -1U;
  }

  std::vector<K>& keys() {
    return order;
  }

  std::vector<V> values() {
    std::vector<V> result;

    for (auto& k : keys()) {
      result.push_back(data[k]);
    }

    return result;
  }

  K& front() {
    if (!order.empty()) {
      return data[order[0]];
    }

    throw std::runtime_error("OrderedMap: empty");
  }

  K& back() {
    if (!order.empty()) {
      return order[order.size()-1];
    }

    throw std::runtime_error("OrderedMap: empty");
  }

  const K& front() const {
    if (!order.empty()) {
      return order[0];
    }

    throw std::runtime_error("OrderedMap: empty");
  }

  const K& back() const {
    if (!order.empty()) {
      return order[order.size()-1];
    }

    throw std::runtime_error("OrderedMap: empty");
  }

  typename std::vector<K>::iterator begin() {
    return order.begin();
  }

  typename std::vector<K>::iterator end() {
    return order.end();
  }

  typename std::vector<K>::const_iterator begin() const {
    return order.begin();
  }

  typename std::vector<K>::const_iterator end() const {
    return order.end();
  }
};

}
