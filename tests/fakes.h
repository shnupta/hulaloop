#pragma once

#include <hulaloop/loop.h>

#include <chrono>

#include <unistd.h>

namespace hula::test {

using namespace std::chrono_literals;

struct fake_clock {
  using underlying_clock = std::chrono::steady_clock;

  using rep = underlying_clock::rep;
  using period = underlying_clock::period;
  using duration = underlying_clock::duration;
  using time_point = underlying_clock::time_point;

  static inline constexpr time_point k_default = time_point{} + 1s;
  static inline time_point current = k_default;

  static constexpr bool is_steady = underlying_clock::is_steady;

  static time_point now() { return current; }

  static void advance(duration d) { current += d; }

  static void reset() { current = k_default; }
};

struct loop_test {
  loop<> _loop;

  void cycle() { _loop.do_cycle(); }
};

struct fake_clock_loop_test {
  loop<fake_clock> _loop;

  fake_clock_loop_test() { fake_clock::reset(); }

  void cycle() {
    _loop._stopping = false;
    _loop.do_cycle();
  }
};

// read write pair of fds from pipe()
class fd_pair {
 public:
  explicit fd_pair() {
    int res = pipe(_fds);
    if (res != 0) {
      throw std::runtime_error("pipe failed to setup fd_pair");
    }
  }

  ~fd_pair() {
    reader_close();
    writer_close();
  }

  int reader_fd() const { return _fds[0]; }
  fd_slots& reader_slots() { return _slots[0]; }
  void reader_close() { close(reader_fd()); }

  int writer_fd() const { return _fds[1]; }
  fd_slots& writer_slots() { return _slots[1]; }
  void writer_close() { close(writer_fd()); }

 private:
  int _fds[2];
  fd_slots _slots[2];
};

}  // namespace hula::test
