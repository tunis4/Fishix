# DMA (Direct Memory Access) Subsystem

The DMA subsystem in Fishix provides essential services for hardware devices to access system memory efficiently and safely.

## Key Components

### 1. Coherent Memory Allocation
`mem::dma::alloc_coherent()` allocates physically contiguous memory that is guaranteed to be cache-coherent between the CPU and devices. 
- **Implementation**: Uses the PMM (Physical Memory Manager) to find contiguous pages and returns a virtual address from the HHDM (Higher Half Direct Mapping) or kernel heap.
- **Header**: `kernel/src/mem/dma.hpp`
- **Source**: `kernel/src/mem/dma.cpp`

### 2. Physical Page Management
Low-level operations to allocate and free contiguous physical pages:
- `alloc_contiguous_pages(num_pages)`: Returns the base physical address.
- `free_contiguous_pages(base, num_pages)`

### 3. Userspace Mapping (mmap)
`mmap_buffer()` allows the kernel to map a dma-coherent buffer directly into a process's virtual address space, which is critical for zero-copy graphics rendering in Wayland.

## Compatibility
The DMA constants used in Fishix (e.g., `DMA_BIDIRECTIONAL`) are kept compatible with the Linux kernel's `dma-buf.h` to simplify driver porting.
