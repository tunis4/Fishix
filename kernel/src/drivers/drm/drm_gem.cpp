// kernel/src/drivers/drm/drm_gem.cpp
// GEM (Graphics Execution Manager) untuk Fishix Wayland support
// Manajemen buffer graphics memory

#include "drm_internal.hpp"
#include <mem/dma.hpp>
#include <mem/pmm.hpp>
#include <mem/vmm.hpp>
#include <klib/cstdio.hpp>
#include <klib/cstring.hpp>
#include <panic.hpp>

namespace drm {

// Global GEM name counter
static u32 next_gem_name = 1;
static klib::Spinlock gem_name_lock;

// Initialize GEM subsystem
void gem_init(void) {
    (gem_name_lock).unlock();
    klib::printf("DRM GEM: Initialized\n");
}

// Create GEM object
struct drm_gem_object* drm_gem_object_create(struct drm_device *dev, u32 size) {
    if (!dev || size == 0) return nullptr;
    
    // Allocate GEM object structure
    struct drm_gem_object *obj = new struct drm_gem_object();
    if (!obj) return nullptr;
    
    // Align size ke page boundary
    u32 aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Allocate DMA coherent memory
    paddr_t phys_addr;
    void *vaddr = mem::dma::alloc_coherent(aligned_size, &phys_addr);
    if (!vaddr) {
        delete obj;
        return nullptr;
    }
    
    // Initialize object
    obj->handle = 0;  // Akan di-set saat handle create
    obj->name = 0;    // Akan di-set saat flink
    obj->size = aligned_size;
    obj->vaddr = vaddr;
    obj->paddr = phys_addr;
    obj->refcount = 1;
    obj->dev = dev;
    obj->dmabuf = nullptr;
    obj->export_attach = false;
    (obj->list).init();
    
    klib::printf("DRM GEM: Created object %p, size %u, phys %#lX\n", 
                 obj, aligned_size, phys_addr);
    
    return obj;
}

// Destroy GEM object
void drm_gem_object_destroy(struct drm_gem_object *obj) {
    if (!obj) return;
    
    // Decrement refcount
    if (--obj->refcount > 0) return;
    
    klib::printf("DRM GEM: Destroying object %p\n", obj);
    
    // Free DMA memory
    if (obj->vaddr && obj->size > 0) {
        mem::dma::free_coherent(obj->size, obj->vaddr, obj->paddr);
    }
    
    // Remove dari device list
    if (obj->dev) {
        (obj->dev->lock).lock();
        for (usize i = 0; i < obj->dev->gem_objects.size(); i++) {
            if (obj->dev->gem_objects[i] == obj) {
                obj->dev->gem_objects.remove(i);
                break;
            }
        }
        (obj->dev->lock).unlock();
    }
    
    delete obj;
}

// Lookup GEM object by handle
struct drm_gem_object* drm_gem_object_lookup(struct drm_file *file, u32 handle) {
    if (!file || handle == 0) return nullptr;
    
    (file->dev->lock).lock();
    
    // Cari dalam file's object list
    for (usize i = 0; i < file->objects.size(); i++) {
        struct drm_gem_object *obj = file->objects[i];
        if (obj && obj->handle == handle) {
            // Increment refcount
            obj->refcount++;
            (file->dev->lock).unlock();
            return obj;
        }
    }
    
    (file->dev->lock).unlock();
    return nullptr;
}

// Create handle untuk GEM object
int drm_gem_handle_create(struct drm_file *file, struct drm_gem_object *obj, u32 *handle) {
    if (!file || !obj || !handle) return -EINVAL;
    
    (file->dev->lock).lock();
    
    // Generate new handle
    u32 new_handle = file->dev->next_handle++;
    if (new_handle == 0) new_handle = file->dev->next_handle++;  // Skip 0
    
    obj->handle = new_handle;
    
    // Add ke file's object list
    file->objects.push_back(obj);
    
    // Add ke device's GEM object list
    file->dev->gem_objects.push_back(obj);
    
    (file->dev->lock).unlock();
    
    *handle = new_handle;
    
    klib::printf("DRM GEM: Created handle %u for object %p\n", new_handle, obj);
    
    return 0;
}

// Delete handle
int drm_gem_handle_delete(struct drm_file *file, u32 handle) {
    if (!file || handle == 0) return -EINVAL;
    
    (file->dev->lock).lock();
    
    // Cari object dengan handle ini
    for (usize i = 0; i < file->objects.size(); i++) {
        struct drm_gem_object *obj = file->objects[i];
        if (obj && obj->handle == handle) {
            // Remove dari file's list
            file->objects.remove(i);
            
            (file->dev->lock).unlock();
            
            // Decrement refcount (akan destroy jika 0)
            drm_gem_object_destroy(obj);
            
            klib::printf("DRM GEM: Deleted handle %u\n", handle);
            return 0;
        }
    }
    
    (file->dev->lock).unlock();
    return -ENOENT;
}

// Flink - create global name untuk GEM object
int drm_gem_flink(struct drm_file *file, u32 handle, u32 *name) {
    if (!file || handle == 0 || !name) return -EINVAL;
    
    struct drm_gem_object *obj = drm_gem_object_lookup(file, handle);
    if (!obj) return -ENOENT;
    
    // Generate global name
    (gem_name_lock).lock();
    u32 new_name = next_gem_name++;
    if (new_name == 0) new_name = next_gem_name++;  // Skip 0
    obj->name = new_name;
    (gem_name_lock).unlock();
    
    *name = new_name;
    
    // Decrement refcount (drm_gem_object_lookup increment)
    drm_gem_object_destroy(obj);
    
    klib::printf("DRM GEM: Flink handle %u -> name %u\n", handle, new_name);
    
    return 0;
}

// Open - lookup GEM object by global name
int drm_gem_open(struct drm_file *file, u32 name, u32 *handle, u64 *size) {
    if (!file || name == 0 || !handle) return -EINVAL;
    
    (file->dev->lock).lock();
    
    // Cari object dengan name ini
    for (usize i = 0; i < file->dev->gem_objects.size(); i++) {
        struct drm_gem_object *obj = file->dev->gem_objects[i];
        if (obj && obj->name == name) {
            // Create handle untuk object ini
            u32 new_handle = file->dev->next_handle++;
            if (new_handle == 0) new_handle = file->dev->next_handle++;
            
            obj->handle = new_handle;
            obj->refcount++;
            
            file->objects.push_back(obj);
            
            (file->dev->lock).unlock();
            
            *handle = new_handle;
            if (size) *size = obj->size;
            
            klib::printf("DRM GEM: Open name %u -> handle %u\n", name, new_handle);
            
            return 0;
        }
    }
    
    (file->dev->lock).unlock();
    return -ENOENT;
}

// Close GEM object (decrement refcount)
void drm_gem_object_close(struct drm_file *file, struct drm_gem_object *obj) {
    if (!file || !obj) return;
    
    drm_gem_object_destroy(obj);
}

// MMAP GEM object
int drm_gem_mmap(struct drm_file *file, u32 handle, vfs::vm_area *vma) {
    if (!file || handle == 0 || !vma) return -EINVAL;
    
    struct drm_gem_object *obj = drm_gem_object_lookup(file, handle);
    if (!obj) return -ENOENT;
    
    // Map DMA buffer ke userspace
    int ret = mem::dma::mmap_buffer(vma, obj->paddr, obj->size);
    
    // Decrement refcount
    drm_gem_object_destroy(obj);
    
    return ret;
}

// IOCTL: GEM Close
int drm_ioctl_gem_close(struct drm_file *file, struct drm_gem_close *close) {
    if (!file || !close) return -EINVAL;
    return drm_gem_handle_delete(file, close->handle);
}

// IOCTL: GEM Flink
int drm_ioctl_gem_flink(struct drm_file *file, struct drm_gem_flink *flink) {
    if (!file || !flink) return -EINVAL;
    u32 name;
    int ret = drm_gem_flink(file, flink->handle, &name);
    flink->name = name;
    return ret;
}

// IOCTL: GEM Open
int drm_ioctl_gem_open(struct drm_file *file, struct drm_gem_open *open) {
    if (!file || !open) return -EINVAL;
    u32 handle;
    u64 size;
    int ret = drm_gem_open(file, open->name, &handle, &size);
    open->handle = handle;
    open->size = size;
    return ret;
}

} // namespace drm
