// kernel/src/include/klib/poll.hpp
// Poll flags untuk Fishix kernel

#ifndef _KLIB_POLL_HPP
#define _KLIB_POLL_HPP

#include <klib/common.hpp>

// Poll flags (compatible dengan Linux)
#define POLLIN      0x001
#define POLLPRI     0x002
#define POLLOUT     0x004
#define POLLERR     0x008
#define POLLHUP     0x010
#define POLLNVAL    0x020
#define POLLRDNORM  0x040
#define POLLRDBAND  0x080
#define POLLWRNORM  0x100
#define POLLWRBAND  0x200
#define POLLMSG     0x400
#define POLLREMOVE  0x1000
#define POLLRDHUP   0x2000

#endif // _KLIB_POLL_HPP
