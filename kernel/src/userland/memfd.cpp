#include <userland/memfd.hpp>
#include <cpu/cpu.hpp>
#include <cpu/syscall/syscall.hpp>
#include <sched/sched.hpp>
#include <klib/cstdio.hpp>
#include <sys/mman.h>

namespace userland {
    MemFD::MemFD() {
        node_type = vfs::NodeType::MEMFD;
    }

    void MemFD::grow_to(usize length) {
        size = length;
        if (capacity < size) {
            capacity = klib::align_up(size, PAGE_SIZE);
            address = mem::vmm->virt_alloc(capacity);
            mem::vmm->kernel_pagemap.map_anonymous(address, capacity, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);

            // FIXME: not sure if this is necessary
            for (usize page = 0; page < capacity; page += PAGE_SIZE)
                *(u8*)(address + page) = 0;
        }
    }

    isize MemFD::read(vfs::FileDescription *fd, void *buf, usize count, usize offset) {
        if (offset >= size)
            return 0;
        usize actual_count = offset + count > size ? size - offset : count;
        memcpy(buf, (void*)(address + offset), actual_count);
        return actual_count;
    }

    isize MemFD::write(vfs::FileDescription *fd, const void *buf, usize count, usize offset) {
        if (count == 0) [[unlikely]] return 0;
        if (offset + count > size)
            grow_to(offset + count);
        memcpy((void*)(address + offset), buf, count);
        return count;
    }

    isize MemFD::seek(vfs::FileDescription *fd, usize position, isize offset, int whence) {
        switch (whence) {
        case SEEK_SET:
            return offset;
        case SEEK_CUR:
            return position + offset;
        case SEEK_END:
            return size + offset;
        case SEEK_DATA:
            if ((usize)offset >= size)
                return -ENXIO;
            return offset;
        case SEEK_HOLE:
            if ((usize)offset >= size)
                return -ENXIO;
            return size;
        default:
            return -EINVAL;
        }
    }

    isize MemFD::mmap(vfs::FileDescription *fd, uptr addr, usize length, isize offset, int prot, int flags) {
        sched::Process *process = cpu::get_current_thread()->process;
        u64 page_flags = mem::mmap_prot_to_page_flags(prot);
        if (flags & MAP_SHARED)
            process->pagemap->add_range(addr, length, page_flags, mem::MappedRange::Type::DIRECT_VIRTUAL, address, nullptr, 0, false, true, false);
        else if (flags & MAP_PRIVATE)
            process->pagemap->map_file(addr, length, page_flags, fd, offset);
        else
            return -EINVAL;
        return addr;
    }

    isize MemFD::truncate(vfs::FileDescription *fd, usize length) {
        if (length > size) {
            grow_to(length);
        } else {
            memset((void*)(address + length), 0, size - length);
        }
        size = length;
        return 0;
    }

    isize syscall_memfd_create(const char *name, uint flags) {
        log_syscall("memfd_create(%s, %#X)\n", name, flags);
        sched::Process *process = cpu::get_current_thread()->process;

        if (flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_HUGETLB))
            klib::printf("memfd_create: unsupported flags %#X\n", flags);

        auto *memfd = new MemFD();
        if (!(flags & MFD_ALLOW_SEALING))
            memfd->seals |= F_SEAL_SEAL;

        int mfd = process->allocate_fdnum();
        auto *description = new vfs::FileDescription(memfd, O_RDWR | O_LARGEFILE);
        process->file_descriptors[mfd].init(description, (flags & MFD_CLOEXEC) ? FD_CLOEXEC : 0);
        return mfd;
    }
}
