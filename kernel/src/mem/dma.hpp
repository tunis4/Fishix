// kernel/src/mm/dma.hpp
// DMA Memory Operations header untuk Fishix Wayland support

#ifndef _FISHIX_DMA_HPP
#define _FISHIX_DMA_HPP

#include <mem/pmm.hpp>
#include <klib/common.hpp>
#include <klib/lock.hpp>

namespace mem { struct MappedRange; }

namespace mem {

namespace dma {

// DMA data direction (match dengan dma-buf.h)
enum dma_data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

// Initialize DMA subsystem
void init(void);

// Allocate contiguous DMA memory (coherent)
void* alloc_coherent(usize size, u64 *phys_addr);

// Free DMA coherent memory
void free_coherent(usize size, void *vaddr, u64 phys_addr);

// Allocate contiguous physical pages
u64 alloc_contiguous_pages(usize num_pages);

// Free contiguous pages
void free_contiguous_pages(u64 base, usize num_pages);

// Map DMA buffer ke userspace
int mmap_buffer(mem::MappedRange *vma, u64 phys_base, usize size);

// Sync operations
void sync_for_cpu(u64 phys_addr, usize size, enum dma_data_direction dir);
void sync_for_device(u64 phys_addr, usize size, enum dma_data_direction dir);

// Address translation
u64 virt_to_phys(void *vaddr);
void* phys_to_virt(u64 paddr);

// Utility
bool is_coherent(u64 paddr);
void get_dma_statistics(usize *total, usize *used, usize *free);
void debug_stats(void);

} // namespace dma

} // namespace mem

#endif // _FISHIX_DMA_HPP
