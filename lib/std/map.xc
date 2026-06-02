#
# Generic Map (key-value dictionary) struct
#
mod std::map;

use core;

const MIN_SIZE:        usize = 32;
const GROWTH_FACTOR:   f32   = 2.0;
const MAX_LOAD_FACTOR: f32   = 0.75;

#
# Hash type
#
type Hash = u64;

#
# Helper macro to generate direct hasher functions
#
macro generate_direct_hasher(__type) {
  fn cat!(hash_, __type) (value: __type) -> Hash {
    return value as Hash;
  }
}

# Generate direct hashers for built-in integer types
generate_direct_hasher!(i8)
generate_direct_hasher!(u8)
generate_direct_hasher!(i16)
generate_direct_hasher!(u16)
generate_direct_hasher!(i32)
generate_direct_hasher!(u32)
generate_direct_hasher!(i64)
generate_direct_hasher!(u64)

#
# Special case for c-strings
#
fn hash_i8_ptr(str: i8*) -> Hash {
  var hash: Hash = 5381;

  while (*str) {

    hash = (hash << 5) + hash + (*str);

    str += 1;
  }

  return hash;
}

struct Node<K, V, A = core::alloc::Allocator> {
  key:  K;
  data: V;
  next: Node*;

  fn new(key: K, data: V) -> Node {
    Node { key, data, next: 0x0 }
  }

  fn create(key: K, data: V) -> Node* {
    var node = A::alloc(sizeof!(Node)) as Node*;

    node->key  = key;
    node->data = data;
    node->next = 0x0;

    node
  }

  fn add(self, node: Node*) {
    if (self->next == 0x0) {
      self->next = node;
      return;
    }

    var tmp = self->next;

    while (tmp->next) {
      tmp = tmp->next;
    }

    tmp->next = node;
  }
}

#
#
#
struct Map<K, V, A = core::alloc::Allocator> {
  buckets: Vector<Node<K, V, A>*>;
  size:    usize;
  cap:     usize;

  fn new() -> Map {
    var map = Map { buckets: Vector::new::<Node<K, V, A>*>(), size: 0, cap: MIN_SIZE };
    map.__recreate_buckets();
    map
  }

  [drop]
  fn drop(self) {
    self->buckets.drop();
  }


  fn add(self, key: K, data: V) {
    self->__add_node(Node::create::<K, V, A>(key, data), true);
  }

  fn get(self, key: K) -> V {
    var hasher = cat!(hash_, mangle!(ty!(K)));
    var hash   = hasher(key);
    var index  = hash % self->cap;

    var node = self->buckets.get(index);

    if (node->key == key) {
      return node->data;
    }

    var tmp = node->next;

    while (tmp) {
      if (tmp->key == key) {
        return tmp->data;
      }

      tmp = tmp->next;
    }

    panic!("No such element");
  }

  fn get_or(self, key: K, default: V) -> V {
    # TODO: Lookup happens 2 times
    if (self->contains(key)) {
      return self->get(key);
    }

    return default;
  }

  fn contains(self, key: K) -> bool {
    var hasher = cat!(hash_, mangle!(ty!(K)));
    var hash   = hasher(key);
    var index  = hash % self->cap;

    var node = self->buckets.get(index);

    if (node->key == key) {
      return true;
    }

    var tmp = node->next;

    while (tmp) {
      if (tmp->key == key) {
        return true;
      }

      tmp = tmp->next;
    }

    return false;
  }

  fn remove(self, key: K) {
    if (self->cap == 0) {
      return;
    }

    var hasher = cat!(hash_, mangle!(ty!(K)));
    var hash   = hasher(key);
    var index  = hash % self->cap;

    var node = self->buckets.get(index);

    if (node == 0x0) {
      return;
    }

    if (node->key == key) {
      self->buckets.set(index, node->next);
      self->size -= 1;
      A::free(node);
    } else {
      while (node->next) {
        if (node->next->key == key) {
          var to_delete = node->next;
          node->next = node->next->next;
          self->size -= 1;
          A::free(to_delete);
          return;
        }

        node = node->next;
      }
    }
  }

  fn __load_factor(self) -> f32 {
    if (self->cap == 0) {
      return 0.0;
    }

    self->size as f32 / self->cap as f32
  }

  fn __add_node(self, node: Node<K, V, A>*, check: bool) {
    if (check) {
      if (self->buckets.length() == 0) {
        self->__recreate_buckets();
      }

      if (self->__load_factor() > MAX_LOAD_FACTOR) {
        self->cap = ((self->cap as f32) * GROWTH_FACTOR) as usize;
        self->__recreate_buckets();
      }
    }

    if (node == 0x0) {
      return;
    }

    var hasher = cat!(hash_, mangle!(ty!(K)));
    var hash   = hasher(node->key);
    var index  = hash % self->cap;

    var current = self->buckets.get(index);

    if (current != 0x0) {
      var found = false;
      var tmp = current;

      while (tmp && !found) {
        if (tmp->key == node->key) {
          tmp->data = node->data;
          A::free(node);
          found = true;
        } else {
          tmp = tmp->next;
        }
      }

      if (!found) {
        current->add(node);
        self->size += 1;
      }
    } else {
      self->buckets.set(index, node);
      self->size += 1;
    }
  }

  fn __recreate_buckets(self) {
    if (self->buckets.length() == 0) {
      if (!self->cap) {
        self->cap = MIN_SIZE;
      }
      self->buckets = Vector::fill::<Node<K, V, A>*>(self->cap, 0x0);

      return;
    }

    var buckets = self->buckets;
    self->buckets = Vector::fill::<Node<K, V, A>*>(self->cap, 0x0);

    for (var i = 0; i < buckets.length(); i += 1) {
      var node = buckets.get(i);
      if (node != 0x0) {
        self->__add_node(Node::create::<K, V, A>(node->key, node->data), false);

        var tmp = node->next;

        while (tmp) {
          self->__add_node(Node::create::<K, V, A>(tmp->key, tmp->data), false);
          var to_delete = tmp;
          tmp = tmp->next;
          A::free(to_delete);
        }

        A::free(node);
      }
    }

    buckets.clear();
  }
}
