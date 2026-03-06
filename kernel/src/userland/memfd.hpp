#pragma once

#include <fs/vfs.hpp>

namespace userland {
    struct MemFD final : public vfs::VNode {
        usize size = 0, capacity = 0;
        uptr address = 0;
        uint seals = 0;

        MemFD();
        virtual ~MemFD() {}

        isize read(vfs::FileDescription *fd, void *buf, usize count, usize offset) override;
        isize write(vfs::FileDescription *fd, const void *buf, usize count, usize offset) override;
        isize seek(vfs::FileDescription *fd, usize position, isize offset, int whence) override;
        isize mmap(vfs::FileDescription *fd, uptr addr, usize length, isize offset, int prot, int flags) override;
        isize truncate(vfs::FileDescription *fd, usize length) override;

    private:
        void grow_to(usize length);
    };

    isize syscall_memfd_create(const char *name, uint flags);
}
