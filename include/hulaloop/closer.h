#pragma once

#include <functional>

namespace hula {

// once closed, will not fire again.
// closes on destruction.
class closer {
 public:
  using callback = std::function<void()>;

  closer() = default;

  explicit closer(callback cb) : _cb(cb) {}

  ~closer() { close(); }
  // non-copyable
  closer(const closer&) = delete;
  closer& operator=(const closer&) = delete;
  // movable
  closer(closer&& o) {
    close();
    _cb = o._cb;
    o._cb = nullptr;
  }
  closer& operator=(closer&& o) {
    close();
    _cb = o._cb;
    o._cb = nullptr;

    return *this;
  }

  operator bool() const { return _cb != nullptr; }

  void close() {
    if (_cb) _cb();
    _cb = nullptr;
  }

 private:
  callback _cb = nullptr;
};

}  // namespace hula
