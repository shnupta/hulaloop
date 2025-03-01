#pragma once

#include "loop.h"

namespace hula {

// utility class which automatically adds an fd to the loop
// and removes it on destruction.
class scoped_fd {
 public:
  explicit scoped_fd(loop<>& l, int fd, fd_slots slots, fd_events events)
      : _loop(l), _fd(fd) {
    _closer = _loop.add_fd(fd, slots, events);
  }

  void update(fd_events events) { _loop.update_fd(_fd, events); }

  void remove() { _closer.close(); }

 private:
  loop<>& _loop;
  int _fd = -1;
  closer _closer;
};

}  // namespace hula
