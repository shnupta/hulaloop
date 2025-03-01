#include <hulaloop/closer.h>

#include <catch2/catch_test_macros.hpp>

namespace hula::test {

TEST_CASE("closer basic construct then close", "[closer]") {
  int val = 1;
  closer c([&] { val = 2; });

  c.close();
  REQUIRE(val == 2);
}

TEST_CASE("closer only closes once", "[closer]") {
  int val = 1;
  closer c([&] { val++; });

  c.close();
  c.close();
  REQUIRE(val == 2);
}

TEST_CASE("closer closes on destruction", "[closer]") {
  int val = 1;
  {
    closer c([&] { val++; });
  }
  REQUIRE(val == 2);
}

TEST_CASE("closer move constructing does not close new", "[closer]") {
  int val = 1;
  closer c1([&] { val++; });

  closer c2(std::move(c1));

  REQUIRE(val == 1);
}

TEST_CASE("closer move constructed closes correctly", "[closer]") {
  int val = 1;
  closer c1([&] { val++; });

  closer c2(std::move(c1));
  c2.close();

  REQUIRE(val == 2);
}

TEST_CASE("closer move assigning closes original", "[closer]") {
  int val = 1;
  closer c1([&] { val = 3; });
  closer c2([&] { val = 99; });

  c1 = std::move(c2);
  REQUIRE(val == 3);

  c2.close();
  REQUIRE(val == 3);

  c1.close();
  REQUIRE(val == 99);
}

}  // namespace hula::test
