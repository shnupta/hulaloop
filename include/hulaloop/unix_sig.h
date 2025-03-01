#pragma once

#include "signal.h"
#include "sys.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <csignal>

#include <sys/signal.h>

namespace hula::unix {

enum class sig
{
	sighup = SIGHUP,
	sigint = SIGINT,
	sigquit = SIGQUIT,
	sigill = SIGILL,
	sigtrap = SIGTRAP,
	sigabrt = SIGABRT,
#if (defined(_HULA_UNIX) && !defined(_HULA_MAC))
	sigpoll = SIGPOLL,
#else
	sigemt = SIGEMT,
#endif
	sigfpe = SIGFPE,
	sigkill = SIGKILL,
	sigsegv = SIGSEGV,
	sigsys = SIGSYS,
	sigpipe = SIGPIPE,
	sigalrm = SIGALRM,
	sigterm = SIGTERM,
	sigurg = SIGURG,
	sigstop = SIGSTOP,
	sigtstp = SIGTSTP,
	sigcont = SIGCONT,
	sigchld = SIGCHLD,
	sigttin = SIGTTIN,
	sigttou = SIGTTOU,
	sigio = SIGIO,
	sigxpu = SIGXCPU,
	sigxfsz = SIGXFSZ,
	sigvtalrm = SIGVTALRM,
	sigprof = SIGPROF,
	sigwinch = SIGWINCH,
	siginfo = SIGINFO,
	sigusr1 = SIGUSR1,
	sigusr2 = SIGUSR2
};

using signal = hula::signal<void, struct sig_slot_tag>;

class signal_registry
{
public:
	static closer connect(sig s, signal::slot sl)
	{
		auto [it, inserted] = instance()._signals.try_emplace(s, unix::signal{});
		if (inserted)
		{
			std::signal(static_cast<int>(s), &signal_registry::dispatch);
		}
		return it->second.connect(sl);
	}

private:
	signal_registry() = default;

	static signal_registry& instance()
	{
		static signal_registry s{};
		return s;
	}

	static void dispatch(int s)
	{
		auto unix_s = static_cast<sig>(s);
		auto it = instance()._signals.find(unix_s);
		if (it == instance()._signals.end())
		{
			throw std::runtime_error("unexpected signal call: signal=" + std::to_string(s));
		}
		it->second();
	}

	std::unordered_map<sig, signal> _signals;
};

}
