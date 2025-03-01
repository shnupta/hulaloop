#include "fakes.h"

#include <hulaloop/scoped_fd.h>

#include <catch2/catch_test_macros.hpp>

namespace hula::test {

TEST_CASE_METHOD(loop_test, "scoped_fd basic", "[scoped_fd]") {
  int val = 0;
  fd_pair p{};

  auto run_for = [&](loop<>::clock::duration d) {
    _loop.post(d, [&] { _loop.stop(); });
    _loop.run();
  };

  char buf[128];
  const auto len = sizeof(buf);

  p.reader_slots().readable = [&](int) {
    val++;
    ::read(p.reader_fd(), buf, len);
  };

  auto* msg = "hello";
  auto msglen = strlen(msg);
  ::write(p.writer_fd(), msg, msglen);

  {
    scoped_fd sfd(_loop, p.reader_fd(), p.reader_slots(), fd_events::read);
    run_for(10ms);
    REQUIRE(val == 1);
  }

  run_for(10ms);
  REQUIRE(val == 1);
}

}  // namespace hula::test
