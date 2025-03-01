#pragma once

#include "closer.h"
#include "signal.h"
#include "unix_sig.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

enum class fd_events {
  read = POLLIN,
  write = POLLOUT,
  read_write = POLLIN | POLLOUT
};

using fd_slot = slot<int, struct fd_slot_tag>;

// slots for handling file descriptor events
struct fd_slots {
  fd_slot readable;
  fd_slot writable;
  fd_slot error;
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
  closer schedule(callback cb) { return schedule(0ns, cb); }

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
  closer schedule(std::chrono::nanoseconds fire_in, callback cb) {
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
  closer connect_to_unix_signal(unix::sig s, unix::signal::slot_type sl) {
    unix::signal& sig = register_or_get_unix_signal(s);
    return sig.connect(sl);
  }

  // register a handler to the given fd (this will cause the loop to poll the
  // file descriptor). to stop polling the descriptor simply close the handle.
  closer add_fd(int fd, fd_slots slots, fd_events events) {
    auto c = closer([=, this] { remove_fd(fd); });
    if (_processing_fds) {
      _pending_poll_additions.emplace_back(
          pending_poll_addition{._fd = fd, ._slots = slots, ._events = events});
      return c;
    }
    push_back_fd(fd, slots, events);
    return c;
  }

  // change the desired events of the descriptor
  void update_fd(int fd, fd_events events) {
    auto it =
        std::find_if(_fd_handlers.begin(), _fd_handlers.end(),
                     [=](const fd_handler& fdh) { return fdh._fd == fd; });

    if (it == _fd_handlers.end()) {
      // might be pending
      auto pending_it = std::find_if(
          _pending_poll_additions.begin(), _pending_poll_additions.end(),
          [=](const pending_poll_addition& ppa) { return ppa._fd == fd; });
      if (pending_it != _pending_poll_additions.end()) {
        pending_it->_events = events;
      }
      return;
    }

    auto idx = std::distance(_fd_handlers.begin(), it);
    struct pollfd& pfd = _pollfds.at(idx);
    pfd.events = static_cast<uint8_t>(events);
    it->_events = events;
  }

  void remove_fd(int fd) {
    auto it =
        std::find_if(_fd_handlers.begin(), _fd_handlers.end(),
                     [=](const fd_handler& fdh) { return fdh._fd == fd; });

    if (it == _fd_handlers.end()) {
      // might be pending
      auto pending_it = std::find_if(
          _pending_poll_additions.begin(), _pending_poll_additions.end(),
          [=](const pending_poll_addition& ppa) { return ppa._fd == fd; });
      if (pending_it != _pending_poll_additions.end()) {
        _pending_poll_additions.erase(pending_it);
      }
      return;
    }
    if (_processing_fds) {
      // we can't remove right now so let's mark for cleanup
      auto& handler = *it;
      if (!handler._active) _pending_poll_removals.insert(fd);

      handler._active = false;
      return;
    }

    // we can simply remove
    auto idx = std::distance(_fd_handlers.begin(), it);
    _fd_handlers.erase(it);
    _pollfds.erase(_pollfds.begin() + idx);
  }

  // when setting to 0, expect 100% cpu usage
  void set_poll_interval(clock::duration d) { _poll_interval = d; }

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

  struct pending_poll_addition {
    int _fd = -1;
    fd_slots _slots;
    fd_events _events;
  };

  struct fd_handler {
    int _fd = -1;
    fd_slots _slots;
    fd_events _events;
    bool _active = false;

    bool want_read() const { return static_cast<int>(_events) & POLLIN; }
    bool want_write() const { return static_cast<int>(_events) & POLLOUT; }

    void readable(int fd) {
      if (_slots.readable) _slots.readable(fd);
    }

    void writable(int fd) {
      if (_slots.writable) _slots.writable(fd);
    }

    void error(int fd) {
      if (_slots.error) _slots.error(fd);
    }
  };

  void do_cycle() {
    if (!work_to_do()) {
      _stopping = true;
      return;
    }

    auto now = clock::now();
    bool want_write =
        std::any_of(_fd_handlers.begin(), _fd_handlers.end(),
                    [](const fd_handler& fdh) { return fdh.want_write(); });

    auto sleep_until = want_write ? now : _next_poll_time;
    if (!_callback_contexts.empty()) {
      auto next_cb_time = _callback_contexts.back()._fire_at;
      if (next_cb_time < sleep_until) sleep_until = next_cb_time;
    }

    if (sleep_until > now) {
      std::this_thread::sleep_until(sleep_until);
      now = clock::now();
    }
    _next_poll_time = now + _poll_interval;

    int poll_res = ::poll(_pollfds.data(), _pollfds.size(), 0);
    if (poll_res < 0) {
      if (errno == EINTR) return;
      throw std::runtime_error("hula::loop => poll failed");
    }

    if (poll_res > 0) {
      _processing_fds = true;
      for (auto i = 0; i < _pollfds.size(); ++i) {
        auto pfd = _pollfds.at(i);
        auto& handler = _fd_handlers.at(i);
        if (handler._active && handler.want_read() && (pfd.revents & POLLIN)) {
          handler.readable(pfd.fd);
        }

        if (handler._active && handler.want_write() &&
            (pfd.revents & POLLOUT)) {
          handler.writable(pfd.fd);
        }

        if (handler._active && (pfd.revents & (POLLERR | POLLHUP))) {
          handler.error(pfd.fd);
        }
      }
      _processing_fds = false;
    }

    now = clock::now();

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

    handle_fd_changes();
  }

  void handle_fd_changes() {
    if (!_pending_poll_removals.empty()) {
      std::erase_if(_pollfds, [&](const struct pollfd& pfd) {
        return _pending_poll_removals.contains(pfd.fd);
      });
      std::erase_if(_fd_handlers, [&](const fd_handler& fdh) {
        return _pending_poll_removals.contains(fdh._fd);
      });
    }
    _pending_poll_removals.clear();
    for (const pending_poll_addition& ppa : _pending_poll_additions) {
      push_back_fd(ppa._fd, ppa._slots, ppa._events);
    }
    _pending_poll_additions.clear();
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

  void push_back_fd(int fd, fd_slots slots, fd_events events) {
    struct pollfd pfd{fd, static_cast<uint8_t>(events), 0};
    _pollfds.push_back(pfd);
    _fd_handlers.push_back(fd_handler{
        ._fd = fd,
        ._slots = slots,
        ._events = events,
        ._active = true,
    });
  }

  bool _stopping = false;
  clock::duration _poll_interval{100us};
  clock::time_point _next_poll_time{};

  bool _processing_fds = false;
  std::vector<struct pollfd> _pollfds;
  std::vector<pending_poll_addition> _pending_poll_additions;
  std::unordered_set<int> _pending_poll_removals;
  std::vector<fd_handler> _fd_handlers;
  uint64_t _next_callback_id = 1;
  // next callback to fire is at the end of the vector
  std::vector<callback_context> _callback_contexts;

  std::unordered_map<unix::sig, signal_handler> _signal_handlers;
  std::deque<unix::sig> _queued_unix_signals;

  friend struct test::loop_test;
  friend struct test::fake_clock_loop_test;
};

}  // namespace hula
