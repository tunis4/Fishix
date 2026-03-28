// kernel/src/drivers/drm/drm_fops.cpp
// DRM File Operations untuk Fishix Wayland support
// Implementasi open, close, ioctl, mmap, poll

#include "drm_internal.hpp"
#include <fs/vfs.hpp>
#include <dev/devnode.hpp>
#include <sched/sched.hpp>
#include <sched/event.hpp>
#include <klib/cstdio.hpp>
#include <klib/cstring.hpp>
#include <panic.hpp>

namespace drm {

// Global file ID counter
static u32 next_file_id = 1;

// Find device by inode
static struct drm_device* find_device_by_inode(vfs::inode *inode) {
    if (!inode) return nullptr;
    
    (drm_devices_lock).lock();
    
    for (usize i = 0; i < drm_devices.size(); i++) {
        struct drm_device *dev = drm_devices[i];
        if (dev->inode == inode) {
            (drm_devices_lock).unlock();
            return dev;
        }
    }
    
    (drm_devices_lock).unlock();
    return nullptr;
}

// Open DRM device
int drm_open(vfs::file *file) {
    if (!file) return -EINVAL;
    
    vfs::inode *inode = file->vnode;
    if (!inode) return -ENOENT;
    
    struct drm_device *dev = find_device_by_inode(inode);
    if (!dev) {
        return -ENODEV;
    }
    
    // Create drm_file
    struct drm_file *drm_file = new struct drm_file();
    if (!drm_file) {
        return -ENOMEM;
    }
    
    drm_file->id = next_file_id++;
    drm_file->authenticated = true;  // Auto-authenticate untuk simplicity
    drm_file->master = (dev->files.size() == 0);  // First opener is master
    drm_file->dev = dev;
    (drm_file->event_list).init();
    (drm_file->event_lock).unlock();
    (drm_file->list).init();
    
    (dev->lock).lock();
    dev->files.push_back(drm_file);
    (dev->lock).unlock();
    
    // Store drm_file in VFS file priv
    file->priv = drm_file;
    
    klib::printf("DRM: Opened file %u, master=%s\n", 
                 drm_file->id, drm_file->master ? "yes" : "no");
    
    return 0;
}

// Close DRM device
int drm_close(vfs::file *file) {
    if (!file || !file->priv) return -EBADF;
    
    struct drm_file *drm_file = (struct drm_file *)file->priv;
    struct drm_device *dev = drm_file->dev;
    
    (dev->lock).lock();
    for (usize i = 0; i < dev->files.size(); i++) {
        if (dev->files[i] == drm_file) {
            dev->files.remove(i);
            break;
        }
    }
    (dev->lock).unlock();
    
    // Cleanup GEM objects
    for (usize i = 0; i < drm_file->objects.size(); i++) {
        drm_gem_object_destroy(drm_file->objects[i]);
    }
    
    // Cleanup events
    while (!list_empty(&drm_file->event_list)) {
        klib::ListHead *entry = list_pop_front(&drm_file->event_list);
        struct drm_pending_event *event = list_entry(entry, struct drm_pending_event, link);
        delete event;
    }
    
    klib::printf("DRM: Closed file %u\n", drm_file->id);
    
    delete drm_file;
    file->priv = nullptr;
    
    return 0;
}

// Read (for events)
ssize_t drm_read(vfs::file *file, void *buf, size_t count) {
    if (!file || !file->priv) return -EBADF;
    
    struct drm_file *drm_file = (struct drm_file *)file->priv;
    
    // Read events dari queue
    (drm_file->event_lock).lock();
    
    klib::ListHead *entry = list_pop_front(&drm_file->event_list);
    if (!entry) {
        (drm_file->event_lock).unlock();
        return -EAGAIN;  // No events
    }
    
    struct drm_pending_event *event = list_entry(entry, struct drm_pending_event, link);
    
    size_t to_copy = sizeof(event->event);
    if (to_copy > count) to_copy = count;
    
    memcpy(buf, &event->event, to_copy);
    
    (drm_file->event_lock).unlock();
    
    delete event;
    
    return to_copy;
}

// Write (not used)
ssize_t drm_write(vfs::file *file, const void *buf, size_t count) {
    (void)file;
    (void)buf;
    (void)count;
    return -EINVAL;
}

// IOCTL handler
int drm_ioctl(vfs::file *file, unsigned long request, void *arg) {
    if (!file || !file->priv) return -EBADF;
    
    struct drm_file *drm_file = (struct drm_file *)file->priv;
    
    // Extract IOCTL number
    unsigned int cmd = request & 0xFF;
    
    int ret = -ENOTTY;
    
    switch (cmd) {
        // Version
        case 0x00:
            ret = drm_ioctl_version(drm_file, (struct drm_version*)arg);
            break;
        
        // Get Cap
        case 0x0c:
            ret = drm_ioctl_get_cap(drm_file, (struct drm_get_cap*)arg);
            break;
        
        // Set Client Cap
        case 0x0d:
            ret = drm_ioctl_set_client_cap(drm_file, (struct drm_set_client_cap*)arg);
            break;
        
        // GEM Close
        case 0x09:
            ret = drm_ioctl_gem_close(drm_file, (struct drm_gem_close*)arg);
            break;
        
        // GEM Flink
        case 0x0a:
            ret = drm_ioctl_gem_flink(drm_file, (struct drm_gem_flink*)arg);
            break;
        
        // GEM Open
        case 0x0b:
            ret = drm_ioctl_gem_open(drm_file, (struct drm_gem_open*)arg);
            break;
        
        // Mode Get Resources
        case 0xa0:
            ret = drm_mode_get_resources(drm_file, (struct drm_mode_card_res*)arg);
            break;
        
        // Mode Get CRTC
        case 0xa1:
            ret = drm_mode_get_crtc(drm_file, (struct drm_mode_crtc*)arg);
            break;
        
        // Mode Set CRTC
        case 0xa2:
            ret = drm_mode_set_crtc(drm_file, (struct drm_mode_crtc*)arg);
            break;
        
        // Mode Cursor
        case 0xa3:
            ret = -ENOTTY;  // Not implemented
            break;
        
        // Mode Get Encoder
        case 0xa6:
            ret = drm_mode_get_encoder(drm_file, (struct drm_mode_get_encoder*)arg);
            break;
        
        // Mode Get Connector
        case 0xa7:
            ret = drm_mode_get_connector(drm_file, (struct drm_mode_get_connector*)arg);
            break;
        
        // Mode Get FB
        case 0xad:
            ret = drm_mode_getfb(drm_file, (struct drm_mode_fb_cmd*)arg);
            break;
        
        // Mode RM FB
        case 0xaf:
            ret = drm_mode_rmfb(drm_file, *(u32*)arg);
            break;
        
        // Mode Page Flip
        case 0xb0:
            ret = drm_mode_page_flip(drm_file, (struct drm_mode_crtc_page_flip*)arg);
            break;
        
        // Mode Create Dumb
        case 0xb2:
            ret = drm_mode_create_dumb(drm_file, (struct drm_mode_create_dumb*)arg);
            break;
        
        // Mode Map Dumb
        case 0xb3:
            ret = drm_mode_map_dumb(drm_file, (struct drm_mode_map_dumb*)arg);
            break;
        
        // Mode Destroy Dumb
        case 0xb4:
            ret = drm_mode_destroy_dumb(drm_file, (struct drm_mode_destroy_dumb*)arg);
            break;
        
        // Mode Add FB2
        case 0xb8:
            ret = drm_mode_addfb2(drm_file, (struct drm_mode_fb_cmd2*)arg);
            break;
        
        default:
            klib::printf("DRM: Unknown IOCTL 0x%x\n", cmd);
            ret = -ENOTTY;
            break;
    }
    
    return ret;
}

// MMAP handler
int drm_mmap(vfs::vm_area *vma) {
    if (!vma || !vma->file || !vma->file->priv) return -EINVAL;
    
    struct drm_file *drm_file = (struct drm_file *)vma->file->priv;
    
    // Extract handle dari offset (offset = handle * PAGE_SIZE)
    u32 handle = (u32)(vma->file_offset / PAGE_SIZE);
    
    // Lookup GEM object
    struct drm_gem_object *obj = drm_gem_object_lookup(drm_file, handle);
    if (!obj) return -ENOENT;
    
    // Map buffer
    int ret = mem::dma::mmap_buffer(vma, obj->paddr, obj->size);
    
    drm_gem_object_destroy(obj);
    
    return ret;
}

// Poll handler
int drm_poll(vfs::file *file) {
    if (!file || !file->priv) return -EBADF;
    
    struct drm_file *drm_file = (struct drm_file *)file->priv;
    
    // Check if events available
    (drm_file->event_lock).lock();
    bool has_events = !list_empty(&drm_file->event_list);
    (drm_file->event_lock).unlock();
    
    return has_events ? POLLIN : 0;
}

// IOCTL: Get version
int drm_ioctl_version(struct drm_file *file, struct drm_version *version) {
    if (!file || !version) return -EINVAL;
    
    version->version_major = 1;
    version->version_minor = 6;
    version->version_patchlevel = 0;
    
    return 0;
}

// IOCTL: Get capability
int drm_ioctl_get_cap(struct drm_file *file, struct drm_get_cap *cap) {
    if (!file || !cap) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    switch (cap->capability) {
        case DRM_CAP_DUMB_BUFFER:
            cap->value = dev->caps.dumb_buffer ? 1 : 0;
            break;
        case DRM_CAP_VBLANK_HIGH_CRTC:
            cap->value = 0;
            break;
        case DRM_CAP_DUMB_PREFERRED_DEPTH:
            cap->value = 24;
            break;
        case DRM_CAP_DUMB_PREFER_SHADOW:
            cap->value = 0;
            break;
        case DRM_CAP_PRIME:
            cap->value = dev->caps.prime ? 1 : 0;
            break;
        case DRM_CAP_TIMESTAMP_MONOTONIC:
            cap->value = 1;
            break;
        case DRM_CAP_ASYNC_PAGE_FLIP:
            cap->value = dev->caps.async_page_flip ? 1 : 0;
            break;
        case DRM_CAP_CURSOR_WIDTH:
            cap->value = dev->mode_config.cursor_width;
            break;
        case DRM_CAP_CURSOR_HEIGHT:
            cap->value = dev->mode_config.cursor_height;
            break;
        case DRM_CAP_ADDFB2_MODIFIERS:
            cap->value = dev->caps.modifiers ? 1 : 0;
            break;
        default:
            cap->value = 0;
            break;
    }
    
    return 0;
}

// IOCTL: Set client capability
int drm_ioctl_set_client_cap(struct drm_file *file, struct drm_set_client_cap *cap) {
    (void)file;
    (void)cap;
    return 0;
}

// Send event ke file
int drm_send_event(struct drm_file *file, struct drm_event *event, u64 user_data) {
    if (!file || !event) return -EINVAL;
    
    struct drm_pending_event *pending = new struct drm_pending_event();
    if (!pending) return -ENOMEM;
    
    memcpy(&pending->event, event, sizeof(*event));
    pending->user_data = user_data;
    pending->file = file;
    (pending->link).init();
    
    (file->event_lock).lock();
    list_add_tail(&pending->link, &file->event_list);
    (file->event_lock).unlock();
    
    return 0;
}

// Send flip complete event
int drm_send_flip_event(struct drm_file *file, u32 crtc_id, u64 user_data) {
    struct drm_event_vblank event;
    
    event.base.type = DRM_EVENT_FLIP_COMPLETE;
    event.base.length = sizeof(event);
    event.user_data = user_data;
    event.tv_sec = 0;
    event.tv_usec = 0;
    event.sequence = 0;
    event.crtc_id = crtc_id;
    
    return drm_send_event(file, (struct drm_event*)&event, user_data);
}

// Send vblank event
int drm_send_vblank_event(struct drm_file *file, u32 crtc_id, u64 user_data) {
    struct drm_event_vblank event;
    
    event.base.type = DRM_EVENT_VBLANK;
    event.base.length = sizeof(event);
    event.user_data = user_data;
    event.tv_sec = 0;
    event.tv_usec = 0;
    event.sequence = 0;
    event.crtc_id = crtc_id;
    
    return drm_send_event(file, (struct drm_event*)&event, user_data);
}

} // namespace drm
