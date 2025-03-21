#pragma once

#include "loop.h"
#include "signal.h"

namespace hula {

// calls the registered slot at fixed intervals
template <class Clock = std::chrono::steady_clock>
class repeater {
 public:
  using clock = Clock;

  explicit repeater(loop<clock>& loop, clock::duration interval, slot<> slot)
      : _loop(loop), _interval(interval), _slot(slot) {}

  explicit repeater(loop<clock>& loop) : _loop(loop) {}

  void set_interval(clock::duration interval) { _interval = interval; }

  void set_slot(slot<> slot) { _slot = slot; }

  void start() {
    assert(_slot);
    schedule();
  }

  void stop() { _closer.close(); }

 private:
  void fire() {
    schedule();
    _slot();
  }

  void schedule() {
    _closer = _loop.schedule(_interval, [this] { fire(); });
  }

  loop<clock>& _loop;
  clock::duration _interval;
  slot<> _slot;
  closer _closer;
};

}  // namespace hula
