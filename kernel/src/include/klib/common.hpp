// kernel/src/include/klib/common.hpp
// Common definitions untuk Fishix kernel
// (Ini adalah file placeholder - gunakan common.hpp yang sudah ada di Fishix)

#ifndef _KLIB_COMMON_HPP
#define _KLIB_COMMON_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Type aliases (sesuaikan dengan Fishix yang ada)
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;
using uptr = uintptr_t;
using paddr_t = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using isize = intptr_t;

// Constants
constexpr usize PAGE_SIZE = 4096;
constexpr usize PAGE_SHIFT = 12;

// Page alignment macros
#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_OFFSET(x) ((x) & (PAGE_SIZE - 1))
#define PAGE_NUMBER(x) ((x) >> PAGE_SHIFT)

// HHDM (Higher Half Direct Map) offset
extern uptr hhdm;

// Error codes (sesuaikan dengan Fishix)
#define EINVAL      22
#define ENOMEM      12
#define ENOENT      2
#define EBUSY       16
#define ENODEV      19
#define EBADF       9
#define EAGAIN      11
#define ENOTTY      25
#define EACCES      13
#define EIO         5
#define ENOSPC      28

// File permissions
#define S_IFCHR     0020000
#define S_IFBLK     0060000
#define S_IFDIR     0040000
#define S_IFREG     0100000
#define S_IFIFO     0010000
#define S_IFLNK     0120000
#define S_IFSOCK    0140000

// Helper macros
#define offsetof(type, member) __builtin_offsetof(type, member)
#define container_of(ptr, type, member) ({ \
    void *__mptr = (void *)(ptr); \
    ((type *)(__mptr - offsetof(type, member))); \
})

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define UNUSED(x) ((void)(x))

// Spinlock (placeholder - gunakan implementasi Fishix)
struct spinlock_t {
    volatile u32 locked;
};

#define INIT_SPINLOCK(lock) ((lock)->locked = 0)

static inline void spin_lock(spinlock_t *lock) {
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static inline void spin_unlock(spinlock_t *lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

// Memory allocation flags
#define GFP_KERNEL  0
#define GFP_ATOMIC  1

// kmalloc/kfree (placeholder - gunakan implementasi Fishix)
void* kmalloc(usize size, int flags);
void kfree(void *ptr);

// kzalloc
static inline void* kzalloc(usize size, int flags) {
    void *ptr = kmalloc(size, flags);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

// C string functions
extern "C" {
    void* memcpy(void *dest, const void *src, usize n);
    void* memset(void *s, int c, usize n);
    int memcmp(const void *s1, const void *s2, usize n);
    int strcmp(const char *s1, const char *s2);
    int strncmp(const char *s1, const char *s2, usize n);
    usize strlen(const char *s);
    char* strncpy(char *dest, const char *src, usize n);
    char* strncat(char *dest, const char *src, usize n);
}

// Panic function (placeholder)
void panic(const char *fmt, ...);

#endif // _KLIB_COMMON_HPP
