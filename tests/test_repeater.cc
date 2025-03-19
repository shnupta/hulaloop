#include "fakes.h"

#include <hulaloop/repeater.h>

#include <catch2/catch_test_macros.hpp>

namespace hula::test {

TEST_CASE_METHOD(fake_clock_loop_test, "repeater basic", "[repeater]") {
  int val = 0;
  auto run_for = [&](fake_clock::duration d, fake_clock::duration step = 1ms) {
    auto end = fake_clock::now() + d;
    while (fake_clock::now() < end) {
      cycle();
      fake_clock::advance(step);
    }
  };

  repeater<fake_clock> r(_loop, 1s, [&] { val++; });
  run_for(2s);
  REQUIRE(val == 0);  // repeater not started yet

  r.start();
  run_for(500ms);
  REQUIRE(val == 0);  // not enough time advanced

  run_for(501ms);
  REQUIRE(val == 1);  // fired!

  run_for(1s);
  REQUIRE(val == 2);  // and again!
}

TEST_CASE_METHOD(fake_clock_loop_test, "repeater stop inside slot",
                 "[repeater]") {
  int val = 0;
  auto run_for = [&](fake_clock::duration d, fake_clock::duration step = 1ms) {
    auto end = fake_clock::now() + d;
    while (fake_clock::now() < end) {
      cycle();
      fake_clock::advance(step);
    }
  };

  repeater<fake_clock> r(_loop);
  r.set_interval(10ms);
  r.set_slot([&] {
    ++val;
    r.stop();
  });
  r.start();

  run_for(1s);

  REQUIRE(val == 1);
}

}  // namespace hula::test
