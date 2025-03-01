#include <catch2/catch_test_macros.hpp>

#include <hulaloop/loop.h>

namespace hula::test {

struct fake_clock
{
	using underlying_clock = std::chrono::steady_clock;

	using rep = underlying_clock::rep;
	using period = underlying_clock::period;
	using duration = underlying_clock::duration;
	using time_point = underlying_clock::time_point;

	static inline time_point current{};

	static constexpr bool is_steady = underlying_clock::is_steady;

	static time_point now()
	{
		return current;
	}

	static void advance(duration d)
	{
		current += d;
	}

	static void reset()
	{
		current = {};
	}
};


struct loop_test
{
	loop<> _loop;

	void cycle()
	{
		_loop.do_cycle();
	}
};

struct fake_clock_loop_test
{
	loop<fake_clock> _loop;

	fake_clock_loop_test()
	{
		fake_clock::reset();
	}

	void cycle()
	{
		_loop.do_cycle();
	}
};

TEST_CASE_METHOD(loop_test, "loop post called once", "[loop]")
{
	int val = 1;

	_loop.post([&] { val++; });
	cycle();
	cycle();
	REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop post inside callback", "[loop]")
{
	int val = 1;

	_loop.post([&] { _loop.post([&] { val++; }); });
	cycle();
	REQUIRE(val == 1);
	cycle();
	REQUIRE(val == 2);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop post with interval", "[loop]")
{
	int val = 1;

	_loop.post(1s, [&] { val++; });
	cycle();
	REQUIRE(val == 1);

	fake_clock::advance(2s);
	cycle();
	REQUIRE(val == 2);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop posts called in order", "[loop]")
{
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

TEST_CASE_METHOD(fake_clock_loop_test, "loop posts longer time added later", "[loop]")
{
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

TEST_CASE_METHOD(loop_test, "loop cancel posted callback", "[loop]")
{
	int val = 1;

	auto id = _loop.post([&] { val++; });
	_loop.cancel_callback(id);
	cycle();
	REQUIRE(val == 1);
}

TEST_CASE_METHOD(fake_clock_loop_test, "loop cancel inside posted callback", "[loop]")
{
	int val = 1;

	auto id = _loop.post(1s, [&] { val++; });
	_loop.post([&] { val++; _loop.cancel_callback(id); });

	cycle();
	REQUIRE(val == 2); // second callback happens

	fake_clock::advance(2s);
	cycle();
	REQUIRE(val == 2); // first callback was cancelled earlier, value stays same
}

TEST_CASE_METHOD(loop_test, "loop schedule called once", "[loop]")
{
	int val = 1;

	auto c = _loop.schedule([&] { val++; });
	cycle();
	cycle();
	REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop schedule inside callback", "[loop]")
{
	int val = 1;

	closer inner;
	auto c = _loop.schedule([&] { inner = _loop.schedule([&] { val++; }); });
	cycle();
	REQUIRE(val == 1);
	cycle();
	REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop signal handler", "[loop]")
{
	int val = 1;

	auto c = _loop.connect_to_unix_signal(unix::sig::sigusr1, [&] { val = 2; });

	std::raise(SIGUSR1);
	REQUIRE(val == 1);

	cycle();
	REQUIRE(val == 2);
}

TEST_CASE_METHOD(loop_test, "loop signal handler closed", "[loop]")
{
	int val = 1;

	auto c = _loop.connect_to_unix_signal(unix::sig::sigusr1, [&] { val = 2; });
	c.close();

	std::raise(SIGUSR1);
	REQUIRE(val == 1);

	cycle();
	REQUIRE(val == 1);
}

TEST_CASE_METHOD(loop_test, "loop signal handler closed during cycle", "[loop]")
{
	int val = 1;

	auto c = _loop.connect_to_unix_signal(unix::sig::sigusr1, [&] { val = 2; });
	auto cb_closer = _loop.schedule([&] { std::raise(SIGUSR1); c.close(); val = 99; });

	cycle();
	REQUIRE(val == 99);
}

TEST_CASE_METHOD(loop_test, "loop real run with callback", "[loop]")
{
	loop<> l;

	l.post([&] { l.stop(); });

	l.run();
}

}
