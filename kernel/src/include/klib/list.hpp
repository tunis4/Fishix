// kernel/src/include/klib/list.hpp
// Linked list operations untuk Fishix kernel

#ifndef _KLIB_LIST_HPP
#define _KLIB_LIST_HPP

#include <klib/common.hpp>

namespace klib {

// Simple doubly-linked list (similar to Linux kernel list)
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

// Initialize list head
static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

// Check if list is empty
static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

// Add entry between two consecutive entries
static inline void __list_add(struct list_head *new_entry,
                               struct list_head *prev,
                               struct list_head *next) {
    next->prev = new_entry;
    new_entry->next = next;
    new_entry->prev = prev;
    prev->next = new_entry;
}

// Add entry at head
static inline void list_add(struct list_head *new_entry, struct list_head *head) {
    __list_add(new_entry, head, head->next);
}

// Add entry at tail
static inline void list_add_tail(struct list_head *new_entry, struct list_head *head) {
    __list_add(new_entry, head->prev, head);
}

// Delete entry between two consecutive entries
static inline void __list_del(struct list_head *prev, struct list_head *next) {
    next->prev = prev;
    prev->next = next;
}

// Delete entry
static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
    entry->next = nullptr;
    entry->prev = nullptr;
}

// Pop first entry dari list
static inline struct list_head* list_pop_front(struct list_head *head) {
    if (list_empty(head)) return nullptr;
    struct list_head *entry = head->next;
    list_del(entry);
    return entry;
}

// Get container dari list entry
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Iterate over list
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// Iterate over list safely (bisa delete selama iterasi)
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

// Iterate over list entries
#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

} // namespace klib

#endif // _KLIB_LIST_HPP
