#pragma once

#include <klib/common.hpp>
#include <cpu/cpu.hpp>
#include <linux/limits.h>
#include <errno.h>

namespace klib {
    template<typename T = u8>
    class UserPtr {
        using NonConst = RemoveConst<T>::type;

        T *ptr;

    public:
        isize load(RemoveConst<T> *dst, usize size_bytes) { return cpu::copy_from_user(dst, ptr, size_bytes); }
        isize store(const T *src, usize size_bytes) { return cpu::copy_to_user(ptr, src, size_bytes); }
        isize load_string(NonConst *dst, usize size) { return cpu::string_copy_from_user(dst, ptr, size); }
        operator uptr() { return (uptr)ptr; }
    };
    using UserStr = UserPtr<const char>;

    template<typename T, usize max_size = 1>
    struct UserBuffer {
        T buffer[max_size];

        isize from(UserPtr<T> ptr, usize count = 1) { return ptr.load(buffer, count * sizeof(T)); }
        isize to(UserPtr<T> ptr, usize count = 1) { return ptr.store(buffer, count * sizeof(T)); }
        operator T*() { return buffer; }
    };

    template<usize max_size>
    struct UserString {
        char buffer[max_size];
        isize from(UserStr ptr) { return ptr.load_string(buffer, max_size); }
        operator char*() { return buffer; }
    };

    struct UserPath : UserString<PATH_MAX> {
        isize from(UserStr ptr) {
            isize ret = UserString::from(ptr);
            if (ret == PATH_MAX) return -ENAMETOOLONG;
            return ret;
        }
    };
}

using klib::UserPtr;
using klib::UserStr;

#define get_user_path(path) klib::UserPath path; if (isize ret = path.from(u_ ## path); ret < 0) return ret
