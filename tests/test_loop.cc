#include "fakes.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

namespace hula::test {

TEST_CASE_METHOD(loop_test, "loop post called once", "[loop]") {
  int val = 1;

  _loop.post([&] { val++; });
  cycle();
  cycle();
  REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop post inside callback", "[loop]") {
  int val = 1;

  _loop.post([&] { _loop.post([&] { val++; }); });
  cycle();
  REQUIRE(val == 1);
  cycle();
  REQUIRE(val == 2);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop post with interval", "[loop]") {
  int val = 1;

  _loop.post(1s, [&] { val++; });
  cycle();
  REQUIRE(val == 1);

  fake_clock::advance(2s);
  cycle();
  REQUIRE(val == 2);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop posts called in order", "[loop]") {
  int val = 1;

  _loop.post(1s, [&] { val = 100; });
  _loop.post(10ms, [&] { val = 23; });
  cycle();
  REQUIRE(val == 1);

  fake_clock::advance(11ms);
  cycle();
  REQUIRE(val == 23);

  fake_clock::advance(1s);
  cycle();
  REQUIRE(val == 100);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop posts longer time added later",
                 "[loop]") {
  int val = 1;

  _loop.post(10ms, [&] { val = 23; });
  _loop.post(1s, [&] { val = 100; });

  cycle();
  REQUIRE(val == 1);

  fake_clock::advance(11ms);
  cycle();
  REQUIRE(val == 23);

  fake_clock::advance(1s);
  cycle();
  REQUIRE(val == 100);
}

TEST_CASE_METHOD(loop_test, "loop cancel posted callback", "[loop]") {
  int val = 1;

  auto id = _loop.post([&] { val++; });
  _loop.cancel_callback(id);
  cycle();
  REQUIRE(val == 1);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop cancel inside posted callback",
                 "[loop]") {
  int val = 1;

  auto id = _loop.post(1s, [&] { val++; });
  _loop.post([&] {
    val++;
    _loop.cancel_callback(id);
  });

  cycle();
  REQUIRE(val == 2);  // second callback happens

  fake_clock::advance(2s);
  cycle();
  REQUIRE(val == 2);  // first callback was cancelled earlier, value stays same
}

TEST_CASE_METHOD(loop_test, "loop schedule called once", "[loop]") {
  int val = 1;

  auto c = _loop.schedule([&] { val++; });
  cycle();
  cycle();
  REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop schedule inside callback", "[loop]") {
  int val = 1;

  closer inner;
  auto c = _loop.schedule([&] { inner = _loop.schedule([&] { val++; }); });
  cycle();
  REQUIRE(val == 1);
  cycle();
  REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop signal handler", "[loop]") {
  int val = 1;

  auto c = _loop.connect_to_unix_signal(unix::sig::sigusr1, [&] { val = 2; });

  std::raise(SIGUSR1);
  REQUIRE(val == 1);

  cycle();
  REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop signal handler closed", "[loop]") {
  int val = 1;

  auto c = _loop.connect_to_unix_signal(unix::sig::sigusr1, [&] { val = 2; });
  c.close();

  std::raise(SIGUSR1);
  REQUIRE(val == 1);

  cycle();
  REQUIRE(val == 1);
}

TEST_CASE_METHOD(loop_test, "loop signal handler closed during cycle",
                 "[loop]") {
  int val = 1;

  auto c = _loop.connect_to_unix_signal(unix::sig::sigusr1, [&] { val = 2; });
  auto cb_closer = _loop.schedule([&] {
    std::raise(SIGUSR1);
    c.close();
    val = 99;
  });

  cycle();
  REQUIRE(val == 99);
}

TEST_CASE_METHOD(loop_test, "loop real run with callback", "[loop]") {
  _loop.post([&] { _loop.stop(); });
  _loop.run();
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop fd signals fired", "[loop]") {
  int val = 1;

  fd_pair p{};
  p.reader_slots().readable = [&](int) { val = 2; };

  auto c = _loop.add_fd(p.reader_fd(), p.reader_slots(), fd_events::read);

  SECTION("no data available, no read signal") {
    cycle();
    REQUIRE(val == 1);
  }

  fake_clock::advance(1s);
  auto* msg = "hello";
  ::write(p.writer_fd(), msg, strlen(msg));

  SECTION("data available to read, signal fired") {
    cycle();
    REQUIRE(val == 2);
  }

  SECTION("fd wants write") {
    ssize_t bytes_written = 0;
    p.writer_slots().writable = [&](int) {
      bytes_written = ::write(p.writer_fd(), msg, strlen(msg));
    };

    auto wc = _loop.add_fd(p.writer_fd(), p.writer_slots(), fd_events::write);

    cycle();
    REQUIRE(bytes_written == strlen(msg));
  }
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop fd removed during processing",
                 "[loop]") {
  fd_pair p{};

  int rval = 0;
  int wval = 0;

  p.reader_slots().readable = [&](int) { rval++; };
  p.writer_slots().writable = [&](int) {
    wval++;
    _loop.remove_fd(p.reader_fd());
  };
  auto rc = _loop.add_fd(p.reader_fd(), p.reader_slots(), fd_events::read);
  auto wc = _loop.add_fd(p.writer_fd(), p.writer_slots(), fd_events::write);

  auto* msg = "hello";
  auto msglen = strlen(msg);
  ::write(p.writer_fd(), msg, msglen);

  // given writer is added second, we should see rval increment only once
  // despite data being available to read
  cycle();
  REQUIRE(rval == 1);
  REQUIRE(wval == 1);

  ::write(p.writer_fd(), msg, msglen);
  cycle();
  REQUIRE(rval == 1);
  REQUIRE(wval == 2);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop fd pending addition", "[loop]") {
  fd_pair p{};

  int rval = 0;
  int wval = 0;

  closer rc;

  p.reader_slots().readable = [&](int) {
    rval++;
    _loop.remove_fd(p.reader_fd());
  };
  p.writer_slots().writable = [&](int) {
    wval++;

    if (!rc)
      rc = _loop.add_fd(p.reader_fd(), p.reader_slots(), fd_events::read);
  };

  auto wc = _loop.add_fd(p.writer_fd(), p.writer_slots(), fd_events::write);

  cycle();
  REQUIRE(rval == 0);
  REQUIRE(wval == 1);  // reader fd has been added

  cycle();
  REQUIRE(rval == 0);  // still nothing to read
  REQUIRE(wval == 2);

  auto* msg = "hello";
  auto msglen = strlen(msg);
  ::write(p.writer_fd(), msg, msglen);

  cycle();
  REQUIRE(rval == 1);  // reader fd removed during loop
  REQUIRE(wval == 3);

  ::write(p.writer_fd(), msg, msglen);
  cycle();
  REQUIRE(rval == 1);  // no longer part of fd processing
  REQUIRE(wval == 4);  // technically adds the fd again
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop fd gets error events", "[loop]") {
  fd_pair p{};

  int wval = 0;

  bool err = false;

  closer rc;

  p.writer_slots().writable = [&](int) { wval++; };
  p.writer_slots().error = [&](int) { err = true; };

  auto wc =
      _loop.add_fd(p.writer_fd(), p.writer_slots(), fd_events::read_write);

  auto* msg = "hello";
  auto msglen = strlen(msg);
  ::write(p.writer_fd(), msg, msglen);

  cycle();
  REQUIRE(wval == 1);
  REQUIRE(!err);

  p.reader_close();
  cycle();
  REQUIRE(err);
}

}  // namespace hula::test
