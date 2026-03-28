// kernel/src/mem/dma.cpp
// DMA Memory Operations untuk Fishix Wayland support
// Menyediakan contiguous physical memory untuk GPU buffers

#include <mem/dma.hpp>
#include <mem/pmm.hpp>
#include <mem/vmm.hpp>
#include <klib/cstdio.hpp>
#include <klib/cstring.hpp>
#include <panic.hpp>

namespace mem { struct MappedRange; }

namespace mem {

namespace dma {

// DMA pool untuk small allocations
static struct dma_pool {
    u64 base;
    usize size;
    usize used;
    klib::Spinlock lock;
    bool initialized;
} g_dma_pool = {0, 0, 0, {}, false};

// DMA coherent memory region
static u64 dma_coherent_base = 0;
static usize dma_coherent_size = 0;

// Initialize DMA subsystem
void init(void) {
    // Allocate 32MB untuk DMA coherent memory
    dma_coherent_size = 32 * 1024 * 1024;  // 32MB
    
    // Allocate contiguous physical pages
    usize pages = dma_coherent_size / PAGE_SIZE;
    dma_coherent_base = pmm::alloc_contiguous_pages(pages);
    
    if (!dma_coherent_base) {
        klib::printf("DMA: Failed to allocate coherent memory pool\n");
        return;
    }
    
    // Map ke kernel address space dengan write-combining
    uptr virt_base = dma_coherent_base + mem::hhdm;
    for (usize i = 0; i < pages; i++) {
        u64 phys = dma_coherent_base + (i * PAGE_SIZE);
        uptr virt = virt_base + (i * PAGE_SIZE);
        mem::vmm->kernel_pagemap.map_page(virt, phys, 
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE | PAGE_WRITE_COMBINING);
    }
    
    // Initialize pool
    g_dma_pool.base = dma_coherent_base;
    g_dma_pool.size = dma_coherent_size;
    g_dma_pool.used = 0;
    g_dma_pool.lock.unlock(); // Ensure it's unlocked
    g_dma_pool.initialized = true;
    
    klib::printf("DMA: Initialized %zu MB coherent memory at %#lX\n", 
                 dma_coherent_size / (1024 * 1024), dma_coherent_base);
}

// Allocate contiguous DMA memory
void* alloc_coherent(usize size, u64 *phys_addr) {
    if (!g_dma_pool.initialized) {
        klib::printf("DMA: Pool not initialized\n");
        return nullptr;
    }
    
    // Align size ke page boundary
    usize aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    g_dma_pool.lock.lock();
    
    if (g_dma_pool.used + aligned_size > g_dma_pool.size) {
        g_dma_pool.lock.unlock();
        klib::printf("DMA: Out of coherent memory (requested %zu, available %zu)\n",
                     aligned_size, g_dma_pool.size - g_dma_pool.used);
        return nullptr;
    }
    
    u64 phys = g_dma_pool.base + g_dma_pool.used;
    g_dma_pool.used += aligned_size;
    
    g_dma_pool.lock.unlock();
    
    uptr virt = phys + mem::hhdm;
    
    if (phys_addr) {
        *phys_addr = phys;
    }
    
    // Zero the memory
    memset((void*)virt, 0, aligned_size);
    
    return (void*)virt;
}

// Free DMA coherent memory (simplified - tidak bisa free individual untuk pool allocator)
void free_coherent(usize size, void *vaddr, u64 phys_addr) {
    // Untuk pool allocator sederhana, kita tidak support individual free
    // Dalam implementasi production, gunakan buddy allocator atau slab allocator
    // Untuk Wayland demo, ini cukup karena buffers biasanya long-lived
    (void)size;
    (void)vaddr;
    (void)phys_addr;
}

// Allocate contiguous physical pages untuk large buffers
u64 alloc_contiguous_pages(usize num_pages) {
    return pmm::alloc_contiguous_pages(num_pages);
}

// Free contiguous pages
void free_contiguous_pages(u64 base, usize num_pages) {
    pmm::free_contiguous_pages(base, num_pages);
}

// Map DMA buffer ke userspace
int mmap_buffer(mem::MappedRange *vma, u64 phys_base, usize size) {
    if (!vma) return -EINVAL;
    
    uptr virt_start = vma->base;
    usize pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (usize i = 0; i < pages; i++) {
        u64 phys = phys_base + (i * PAGE_SIZE);
        uptr virt = virt_start + (i * PAGE_SIZE);
        
        // Map dengan user-accessible flags
        u64 flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE;
        
        // Gunakan write-combining untuk framebuffer
        flags |= PAGE_WRITE_COMBINING;
        
        mem::vmm->active_pagemap->map_page(virt, phys, flags);
    }
    
    return 0;
}

// Sync DMA buffer untuk CPU access
void sync_for_cpu(u64 phys_addr, usize size, enum dma_data_direction dir) {
    (void)phys_addr;
    (void)size;
    (void)dir;
    
    // Invalidate CPU cache untuk memastikan CPU melihat data terbaru dari device
    // x86_64: CLFLUSH atau non-temporal hint
    // Untuk simplicity, kita rely pada write-combining memory type
}

// Sync DMA buffer untuk device access
void sync_for_device(u64 phys_addr, usize size, enum dma_data_direction dir) {
    (void)phys_addr;
    (void)size;
    (void)dir;
    
    // Flush CPU cache ke memory agar device melihat data terbaru
    // CLFLUSH untuk range
    uptr virt = phys_addr + mem::hhdm;
    for (usize i = 0; i < size; i += 64) {  // 64-byte cache line
        __asm__ volatile("clflush %0" :: "m"(*(char*)(virt + i)));
    }
    __asm__ volatile("mfence" ::: "memory");
}

// Get physical address dari virtual address
u64 virt_to_phys(void *vaddr) {
    uptr va = (uptr)vaddr;
    
    // Check jika dalam HHDM range
    if (va >= mem::hhdm && va < mem::hhdm + 0x100000000) {
        return va - mem::hhdm;
    }
    
    // Otherwise, lookup page table
    return mem::vmm->active_pagemap->get_physical_addr(va);
}

// Get virtual address dari physical address (HHDM)
void* phys_to_virt(u64 paddr) {
    return (void*)(paddr + mem::hhdm);
}

// Check jika address adalah DMA coherent
bool is_coherent(u64 paddr) {
    if (!g_dma_pool.initialized) return false;
    return (paddr >= dma_coherent_base && 
            paddr < dma_coherent_base + dma_coherent_size);
}

// Get DMA pool stats
void get_dma_statistics(usize *total, usize *used, usize *free) {
    if (total) *total = g_dma_pool.size;
    if (used) *used = g_dma_pool.used;
    if (free) *free = g_dma_pool.size - g_dma_pool.used;
}

// Debug: Print DMA stats
void debug_stats(void) {
    usize total, used, free;
    get_dma_statistics(&total, &used, &free);
    
    klib::printf("DMA Pool Stats:\n");
    klib::printf("  Total: %zu MB\n", total / (1024 * 1024));
    klib::printf("  Used:  %zu MB (%zu%%)\n", used / (1024 * 1024), (used * 100) / total);
    klib::printf("  Free:  %zu MB\n", free / (1024 * 1024));
}

} // namespace dma

} // namespace mem
