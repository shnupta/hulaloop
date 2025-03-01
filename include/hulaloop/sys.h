#pragma once

namespace hula {

enum class sys_type { unix, mac };

#if defined(__unix__)

#define _HULA_UNIX
static constexpr sys_type sys = sys_type::unix;

#elif defined(__APPLE__)

#define _HULA_MAC
static constexpr sys_type sys = sys_type::mac;

#endif

}  // namespace hula
