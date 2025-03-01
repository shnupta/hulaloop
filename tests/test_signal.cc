#include <catch2/catch_test_macros.hpp>

#include <hulaloop/signal.h>

namespace hula::test {

TEST_CASE("signal void signal no slots", "[signal]")
{
	signal<> s;
	s();
}

TEST_CASE("signal void signal one slot", "[signal]")
{
	int val = 1;

	signal<> s;
	auto c = s.connect([&] { val = 2; });

	s();

	REQUIRE(val == 2);
}

TEST_CASE("signal void signal slot disconnects before call", "[signal]")
{
	int val = 1;
	signal<> s;
	auto c = s.connect([&] { val = 2;});

	c.close();

	s();
	REQUIRE(val == 1);
}

TEST_CASE("signal void signal connect during call", "[signal]")
{
	signal<> s;
	bool once = false;
	int val = 1;
	closer inner;

	auto c = s.connect([&] {
			if (!once)
			{
				inner = s.connect([&] { val = 2; });
				once = true;
			}
		});

	s();
	REQUIRE(val == 1);

	s();
	REQUIRE(val == 2);
}

TEST_CASE("signal void signal disconnect during call", "[signal]")
{
	int val = 1;
	signal<> s;
	closer c;
	c = s.connect([&] { val++; c.close(); });

	s();
	REQUIRE(val == 2);

	s();
	REQUIRE(val == 2);
}

TEST_CASE("signal param signal", "[signal]")
{
	int val = 1;
	signal<int> s;

	auto c = s.connect([&](int v) { val = v; });

	s(12);
	REQUIRE(val == 12);
}

TEST_CASE("signal by ref signal", "[signal]")
{
	std::string str;
	signal<std::string&> s;

	auto c = s.connect([](std::string& ref) { ref = "blah"; });

	s(str);
	REQUIRE(str == "blah");
}

}
