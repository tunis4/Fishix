// kernel/src/include/uapi/linux/dma-buf.h
// DMA-BUF API untuk Fishix Wayland support
// DMA-BUF digunakan untuk zero-copy buffer sharing antara GPU dan display

#pragma once
#include <kernel.hpp>

#ifndef _FISHIX_DMA_BUF_H
#define _FISHIX_DMA_BUF_H

#include <klib/common.hpp>

// DMA-BUF IOCTLs
#define DMA_BUF_BASE            'b'
#define DMA_BUF_IOCTL_SYNC      _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)
#define DMA_BUF_IOCTL_SET_NAME  _IOW(DMA_BUF_BASE, 1, const char*)

// DMA-BUF sync flags
#define DMA_BUF_SYNC_READ       (1 << 0)
#define DMA_BUF_SYNC_WRITE      (2 << 0)
#define DMA_BUF_SYNC_RW         (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START      (0 << 2)
#define DMA_BUF_SYNC_END        (1 << 2)
#define DMA_BUF_SYNC_VALID_FLAGS_MASK \
    (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_END)

// DMA-BUF sync structure
struct [[gnu::packed]] dma_buf_sync {
    u64 flags;
};

// DMA-BUF attachment info
struct dma_buf_attachment {
    struct dma_buf *dmabuf;
    struct device *dev;
    klib::ListHead node;
    void *priv;
};

// DMA data direction
enum dma_data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

// DMA-BUF ops (kernel internal)
struct dma_buf_ops {
    int (*attach)(struct dma_buf *, struct dma_buf_attachment *);
    void (*detach)(struct dma_buf *, struct dma_buf_attachment *);
    struct sg_table *(*map_dma_buf)(struct dma_buf_attachment *,
                                     enum dma_data_direction);
    void (*unmap_dma_buf)(struct dma_buf_attachment *,
                          struct sg_table *,
                          enum dma_data_direction);
    void (*release)(struct dma_buf *);
    int (*begin_cpu_access)(struct dma_buf *, enum dma_data_direction);
    int (*end_cpu_access)(struct dma_buf *, enum dma_data_direction);
    void *(*vmap)(struct dma_buf *);
    void (*vunmap)(struct dma_buf *, void *vaddr);
    int (*mmap)(struct dma_buf *, struct vm_area_struct *vma);
};

// DMA-BUF structure (kernel internal)
struct dma_buf {
    u32 size;
    u32 file_count;
    const struct dma_buf_ops *ops;
    klib::ListHead attachments;
    void *priv;
    const char *name;
    u32 id;
};

// Scatterlist table (simplified)
struct scatterlist {
    paddr_t dma_address;
    u32 length;
    u32 offset;
};

struct sg_table {
    struct scatterlist *sgl;
    u32 nents;
    u32 orig_nents;
};

namespace dmabuf {

// Function prototypes
struct dma_buf* allocate(u32 size, const struct dma_buf_ops *ops, void *priv);
void release(struct dma_buf *buf);
int attach(struct dma_buf *buf, struct dma_buf_attachment *attach);
void detach(struct dma_buf *buf, struct dma_buf_attachment *attach);
int begin_cpu_access(struct dma_buf *buf, enum dma_data_direction dir);
int end_cpu_access(struct dma_buf *buf, enum dma_data_direction dir);
void* vmap(struct dma_buf *buf);
void vunmap(struct dma_buf *buf, void *vaddr);
int sync(struct dma_buf *buf, u64 flags);

} // namespace dmabuf

#endif // _FISHIX_DMA_BUF_H
