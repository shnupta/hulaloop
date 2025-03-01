#pragma once

#include "closer.h"

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

namespace hula {

namespace detail {

template<class T, class Tag = void>
struct slot_picker
{
	using type = std::function<void(T&&)>;
};

template<class Tag>
struct slot_picker<void, Tag>
{
	using type = std::function<void()>;
};

}

// signal which can fire, calling all connected slots with argument of type T.
// closing the returned closer will disconnect.
template<class T = void, class Tag = void>
class signal
{
public:
	using slot = detail::slot_picker<T, Tag>::type;

	template<class U = T, typename = std::enable_if<std::is_void_v<T>>>
	void operator()()
	{
		_during_call = true;

		for (auto& sub : _slots)
		{
			sub();
		}

		_during_call = false;
		do_pending_disconnects();
		do_pending_connects();
	}

	template<class U = T, typename = std::enable_if<!std::is_void_v<T>>>
	void operator()(U&& arg)
	{
		_during_call = true;

		for (auto& sub : _slots)
		{
			sub(std::forward<U>(arg));
		}

		_during_call = false;
		do_pending_disconnects();
		do_pending_connects();
	}

	// closing the handle will guarantee that the slot will not be triggered afterwards
	closer connect(slot s)
	{
		auto id = _next_id++;
		if (_during_call)
		{
			_pending_connections.emplace_back(slot_info{
					._id = id,
					._slot = s,
					});
		}
		else
		{
			_slots.emplace_back(slot_info{
					._id = id,
					._slot = s,
					});
		}

		return closer([=]() { disconnect(id); });
	}

private:
	void disconnect(uint64_t id)
	{
		auto it = std::find_if(_slots.begin(), _slots.end(), [=](const slot_info& s) { return s._id == id; });
		if (_during_call)
		{
				if (it != _slots.end())
				{
					it->_active = false;
				}
			_pending_disconnections.emplace_back(id);
			return;
		}
		if (it != _slots.end())
		{
			_slots.erase(it);
			return;
		}

		auto pending_it = std::find_if(_pending_connections.begin(), _pending_connections.end(), [=](const slot_info& s) { return s._id == id;});
		if (pending_it != _pending_connections.end())
		{
			_pending_connections.erase(pending_it);
		}
	}

	void do_pending_disconnects()
	{
		assert(!_during_call);
		for (const auto id : _pending_disconnections)
			disconnect(id);

		_pending_disconnections.clear();
	}
	
	void do_pending_connects()
	{
		assert(!_during_call);
		for (const auto& slot : _pending_connections)
			_slots.emplace_back(std::move(slot));

		_pending_connections.clear();
	}

	struct slot_info
	{
		uint64_t _id;
		slot _slot;
		bool _active = true;

		template<class U = T, typename = std::enable_if<std::is_void_v<T>, void>>
		void operator()()
		{
			if (_active)
				_slot();
		}

		template<class U = T, typename = std::enable_if<!std::is_void_v<T>, void>>
		void operator()(U&& arg)
		{
			if (_active)
				_slot(std::forward<U>(arg));
		}
	};

	uint64_t _next_id = 0;
	std::vector<slot_info> _slots;
	bool _during_call = false;

	std::vector<slot_info> _pending_connections;
	std::vector<uint64_t> _pending_disconnections;
};

}
