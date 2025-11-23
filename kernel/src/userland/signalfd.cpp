#include <userland/signalfd.hpp>
#include <cpu/cpu.hpp>
#include <cpu/syscall/syscall.hpp>
#include <sched/sched.hpp>
#include <klib/cstdio.hpp>
#include <sys/signalfd.h>

// FIXME: multithreaded signal semantics are broken
namespace userland {
    SignalFD::SignalFD() {
        node_type = vfs::NodeType::SIGNALFD;
        event = &cpu::get_current_thread()->process->get_main_thread()->signal_event;
    }

    isize SignalFD::read(vfs::FileDescription *fd, void *buf, usize count, usize offset) {
        sched::Process *process = cpu::get_current_thread()->process;
        sched::Thread *thread = process->get_main_thread();

        usize num_siginfos = count / sizeof(signalfd_siginfo);
        if (num_siginfos < 1)
            return -EINVAL;

        while ((thread->pending_signals & this->mask) == 0) {
            if (fd->flags & O_NONBLOCK)
                return -EWOULDBLOCK;
            if (event->wait() == -EINTR)
                return -EINTR;
        }

        usize num_returned = 0;
        for (; num_returned < num_siginfos; num_returned++) {
            u64 accepted_signals = thread->pending_signals & this->mask;
            if (accepted_signals == 0)
                break;

            int signal = -1;
            for (int i = 0; i < 64; i++) {
                if ((accepted_signals >> i) & 1) {
                    signal = i + 1;
                    break;
                }
            }
            ASSERT(signal != -1);
            thread->pending_signals &= ~(get_signal_bit(signal));

            // FIXME: siginfo is incomplete
            auto *siginfo = (signalfd_siginfo*)((uptr)buf + num_returned * sizeof(signalfd_siginfo));
            memset(siginfo, 0, sizeof(signalfd_siginfo));
            siginfo->ssi_signo = signal;
        }
        return num_returned * sizeof(signalfd_siginfo);
    }

    isize SignalFD::poll(vfs::FileDescription *fd, isize events) {
        sched::Process *process = cpu::get_current_thread()->process;
        isize revents = 0;
        if (events & POLLIN)
            if (process->get_main_thread()->pending_signals & this->mask)
                revents |= POLLIN;
        return revents;
    }

    static isize signalfd_impl(int fd, const u64 *mask, int flags) {
        sched::Process *process = cpu::get_current_thread()->process;

        if (flags & ~(SFD_NONBLOCK | SFD_CLOEXEC))
            return -EINVAL;

        if (fd == -1) {
            auto *signalfd = new SignalFD();
            signalfd->mask = *mask;

            int sfd = process->allocate_fdnum();
            auto *description = new vfs::FileDescription(signalfd, O_RDONLY | (flags & SFD_NONBLOCK) ? O_NONBLOCK : 0);
            process->file_descriptors[sfd].init(description, (flags & SFD_CLOEXEC) ? FD_CLOEXEC : 0);
            return sfd;
        } else {
            auto *description = vfs::get_file_description(fd);
            if (!description)
                return -EBADF;
            if (description->vnode->node_type != vfs::NodeType::SIGNALFD)
                return -EINVAL;

            auto *signalfd = (SignalFD*)description->vnode;
            signalfd->mask = *mask;
            return fd;
        }
    }

    isize syscall_signalfd4(int fd, const u64 *mask, usize mask_size, int flags) {
        log_syscall("signalfd4(%d, %#lX, %#lX, %#X)\n", fd, (uptr)mask, mask_size, flags);
        if (mask_size != sizeof(*mask)) return -EINVAL;
        return signalfd_impl(fd, mask, flags);
    }

    isize syscall_signalfd(int fd, const u64 *mask, usize mask_size) {
        log_syscall("signalfd(%d, %#lX, %#lX)\n", fd, (uptr)mask, mask_size);
        if (mask_size != sizeof(*mask)) return -EINVAL;
        return signalfd_impl(fd, mask, 0);
    }
}
