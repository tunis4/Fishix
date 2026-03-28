#pragma once

#include <klib/common.hpp>
#include <klib/list.hpp>
#include <panic.hpp>
#include <limine.hpp>

namespace mem { extern uptr hhdm; }

namespace pmm {
    struct Page {
        klib::ListHead link;
        bool free : 1; // true if its in the freelist
        u64 pfn : 63; // page frame number (the physical address of the page >> 12)
        uptr mapped_addr; // virtual address that it is mapped to if this is anonymous memory

        inline uptr phy() const { return pfn * PAGE_SIZE; }

        template<typename T>
        inline T* as() const { return (T*)(phy() + mem::hhdm); }
    };
    static_assert(sizeof(Page) == 32);

    struct Region {
        klib::ListHead link;
        usize num_pages;
        usize padding;

        inline usize base_phy() const { return (uptr)this - mem::hhdm; }
        inline usize end_phy() const { return base_phy() + num_pages * PAGE_SIZE; }
        inline usize num_pages_reserved() const { return (sizeof(Region) + num_pages * sizeof(Page) + PAGE_SIZE - 1) / PAGE_SIZE; }
        inline usize num_pages_usable() const { return num_pages - num_pages_reserved(); }
        inline Page* pages_array() const { return (Page*)((uptr)this + sizeof(Region)); }
    };
    static_assert(sizeof(Region) == 32);

    void init(uptr hhdm, limine_memmap_response *memmap_res);
    void absorb_memmap_entry(uptr hhdm, limine_memmap_entry *entry);
    void reclaim_bootloader_mem(uptr hhdm, limine_memmap_response *memmap_res);

    Page* find_page(uptr phy);

    Page* alloc_page();
    void free_page(Page *page);

    uptr alloc_contiguous_pages(usize num_pages);
    void free_contiguous_pages(uptr base, usize num_pages);

    inline uptr alloc_pages(usize num_pages) {
        ASSERT(num_pages == 1);
        Page *page = alloc_page();
        return page->phy();
    }

    struct Stats {
        usize total_pages_usable = 0;
        usize total_pages_reserved = 0;
        usize total_free_pages = 0;
    };

    extern Stats stats;
}
