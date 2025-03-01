#pragma once

#include "closer.h"
#include "signal.h"
#include "unix_sig.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/poll.h>

using namespace std::chrono_literals;

namespace hula {

namespace test {
struct loop_test;
struct fake_clock_loop_test;
}  // namespace test

// generic callback
using callback = std::function<void()>;

enum class fd_events { read, write, read_write };

using fd_signal = signal<int, struct fd_signal_tag>;

// function which handles a given signal event
// slots for handling file descriptor events
struct fd_slots {
  fd_signal::slot readable;
  fd_signal::slot writable;
  fd_signal::slot error;
};

// basic event loop which handles polling file descriptors,
// registering callbacks and other timing related functionality.
template <typename Clock = std::chrono::steady_clock>
class loop {
 public:
  using clock = Clock;

  enum class result { success = 0, failure = 1 };

  result run() {
    _stopping = false;

    while (!_stopping) {
      do_cycle();
    }

    return result::success;
  }

  void stop() { _stopping = true; }

  // immediately called in the next cycle.
  // callback can be manually removed by calling cancel_callback(id).
  uint64_t post(callback cb) { return post(0ns, cb); }

  // immediately called in the next cycle.
  // close the handle to cancel the callback.
  [[nodiscard]] closer schedule(callback cb) { return schedule(0ns, cb); }

  // called after fire_in duration has passed
  // callback can be manually removed by calling cancel_callback(id).
  uint64_t post(std::chrono::nanoseconds fire_in, callback cb) {
    auto id = _next_callback_id++;
    auto fire_at = clock::now() + fire_in;

    auto it = _callback_contexts.rbegin();
    while (it != _callback_contexts.rend() && fire_at > it->_fire_at) it++;

    _callback_contexts.insert(
        it.base(), callback_context{._id = id, ._fire_at = fire_at, ._cb = cb});

    return id;
  }

  // called after fire_in duration has passed
  // close the handle to cancel the callback.
  [[nodiscard]] closer schedule(std::chrono::nanoseconds fire_in, callback cb) {
    auto id = post(fire_in, cb);
    return closer([=, this] { cancel_callback(id); });
  }

  // cancels the callback with the given id if it exists.
  // safe to call with a non-existing callback id.
  void cancel_callback(uint64_t id) {
    auto it = std::find_if(
        _callback_contexts.begin(), _callback_contexts.end(),
        [=](const callback_context& ctx) { return ctx._id == id; });
    if (it == _callback_contexts.end()) return;

    _callback_contexts.erase(it);
  }

  // register a handler to a specific signal (e.g. kill, winsize change)
  [[nodiscard]] closer connect_to_unix_signal(unix::sig s,
                                              unix::signal::slot sl) {
    unix::signal& sig = register_or_get_unix_signal(s);
    return sig.connect(sl);
  }

  // register a handler to the given fd (this will cause the loop to poll the
  // file descriptor). to stop polling the descriptor simply close the handle.
  [[nodiscard]] closer add_fd(int fd, fd_slots slots, fd_events events) {
    // TODO
    return closer();
  }

  // change the desired events of the descriptor
  void update_fd(int fd, fd_events events) {
    // TODO
  }

  void remove_fd(int fd) {
    // TODO
  }

 private:
  struct callback_context {
    uint64_t _id{};
    clock::time_point _fire_at;
    callback _cb;

    void operator()() { _cb(); }
  };

  struct signal_handler {
    closer _closer;
    unix::signal _signal;
  };

  void do_cycle() {
    if (!work_to_do()) {
      _stopping = true;
      return;
    }

    auto now = clock::now();

    // todo: fd polling and slow polling
    // later add timer

    while (!_callback_contexts.empty() &&
           _callback_contexts.back()._fire_at <= now && !_stopping) {
      auto cb = std::move(_callback_contexts.back());
      _callback_contexts.pop_back();
      cb();
    }

    while (!_queued_unix_signals.empty() && !_stopping) {
      auto unix_s = _queued_unix_signals.front();
      _queued_unix_signals.pop_front();
      auto& sig = get_unix_signal(unix_s);
      sig();
    }
  }

  bool work_to_do() const noexcept {
    return !(_pollfds.empty() && _callback_contexts.empty() &&
             _queued_unix_signals.empty());
  }

  unix::signal& get_unix_signal(unix::sig s) {
    auto it = _signal_handlers.find(s);
    if (it == _signal_handlers.end()) {
      throw std::runtime_error("unexpected get for signal handler: " +
                               std::to_string(static_cast<int>(s)));
    }
    return it->second._signal;
  }

  unix::signal& register_or_get_unix_signal(unix::sig s) {
    auto [it, inserted] = _signal_handlers.try_emplace(s, signal_handler{});
    auto& handler = it->second;
    if (inserted) {
      handler._closer = unix::signal_registry::connect(
          s, [=, this] { _queued_unix_signals.push_back(s); });
    }
    return handler._signal;
  }

  bool _stopping = false;
  clock::time_point _next_poll_time{};

  std::vector<struct pollfd> _pollfds;
  std::vector<fd_slots> _fd_slots;
  uint64_t _next_callback_id = 1;
  // next callback to fire is at the end of the vector
  std::vector<callback_context> _callback_contexts;

  std::unordered_map<unix::sig, signal_handler> _signal_handlers;
  std::deque<unix::sig> _queued_unix_signals;

  friend struct test::loop_test;
  friend struct test::fake_clock_loop_test;
};

}  // namespace hula
