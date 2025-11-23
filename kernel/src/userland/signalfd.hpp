#pragma once

#include <fs/vfs.hpp>

namespace userland {
    struct SignalFD final : public vfs::VNode {
        u64 mask;

        SignalFD();
        virtual ~SignalFD() {}

        isize read(vfs::FileDescription *fd, void *buf, usize count, usize offset) override;
        isize poll(vfs::FileDescription *fd, isize events) override;
    };

    isize syscall_signalfd4(int fd, const u64 *mask, usize mask_size, int flags);
    isize syscall_signalfd(int fd, const u64 *mask, usize mask_size);
}
