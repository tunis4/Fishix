// kernel/src/drivers/drm/fishix_drm.cpp
// Fishix DRM Driver - Integrasi dengan Fishix Framebuffer
// Driver DRM yang menggunakan framebuffer Fishix yang sudah ada

#include "drm_internal.hpp"
#include <gfx/framebuffer.hpp>
#include <mem/dma.hpp>
#include <mem/vmm.hpp>
#include <dev/devnode.hpp>
#include <klib/cstdio.hpp>
#include <klib/cstring.hpp>
#include <cpu/cpu.hpp>
#include <sched/sched.hpp>
#include <panic.hpp>

namespace drm {

// Fishix DRM private data
struct fishix_drm_private {
    // Framebuffer info dari Fishix
    uptr fb_phys;
    uptr fb_virt;
    u32 fb_width;
    u32 fb_height;
    u32 fb_pitch;
    u32 fb_bpp;
    
    // Hardware state
    struct drm_crtc *crtc;
    struct drm_encoder *encoder;
    struct drm_connector *connector;
    struct drm_plane *primary_plane;
    
    // Current display buffer
    void *display_buffer;
    paddr_t display_buffer_phys;
};

// DRM DevNode implementation
struct DRMDevNode final : public dev::SeekableCharDevNode {
    DRMDevNode() {}
    isize open(vfs::file *fd) override { return drm_open(fd); }
    void close(vfs::file *fd) override { drm_close(fd); }
    isize read(vfs::file *fd, void *buf, usize count, usize offset) override { return drm_read(fd, buf, count); }
    isize write(vfs::file *fd, const void *buf, usize count, usize offset) override { return drm_write(fd, buf, count); }
    isize ioctl(vfs::file *fd, usize cmd, void *arg) override { return drm_ioctl(fd, cmd, arg); }
    isize mmap(vfs::FileDescription *fd, uptr addr, usize length, isize offset, int prot, int flags) override {
        (void)flags;
        sched::Process *process = cpu::get_current_thread()->process;
        u64 page_flags = mem::mmap_prot_to_page_flags(prot);
        
        // Add range of type FILE to represent the GEM mapping
        auto *vma = process->pagemap->add_range(addr, length, page_flags, mem::MappedRange::Type::FILE, 0, fd, offset);
        if (!vma) return -ENOMEM;
        
        int ret = drm::drm_mmap(vma);
        if (ret < 0) {
            // Range will be cleaned up by process pagemap on exit or munmap
            return ret;
        }
        return addr;
    }
    isize poll(vfs::file *fd, isize events) override { return drm_poll(fd); }
};

// Forward declarations
static int fishix_drm_load(struct drm_device *dev);
static void fishix_drm_unload(struct drm_device *dev);
static struct drm_gem_object* fishix_drm_gem_create(struct drm_device *dev, u32 size);
static void fishix_drm_gem_destroy(struct drm_gem_object *obj);
static int fishix_drm_gem_mmap(struct drm_gem_object *obj, mem::MappedRange *vma);
static int fishix_drm_crtc_set_config(struct drm_crtc *crtc, struct drm_mode_crtc *mode);
static int fishix_drm_page_flip(struct drm_crtc *crtc, struct drm_framebuffer *fb, u64 user_data);
static int fishix_drm_fb_create(struct drm_device *dev, struct drm_mode_fb_cmd2 *cmd);
static void fishix_drm_fb_destroy(struct drm_framebuffer *fb);

// Fishix DRM driver operations
static struct drm_driver_ops fishix_drm_ops = {
    .load = fishix_drm_load,
    .unload = fishix_drm_unload,
    .gem_create = fishix_drm_gem_create,
    .gem_destroy = fishix_drm_gem_destroy,
    .gem_mmap = fishix_drm_gem_mmap,
    .crtc_set_config = fishix_drm_crtc_set_config,
    .page_flip = fishix_drm_page_flip,
    .fb_create = fishix_drm_fb_create,
    .fb_destroy = fishix_drm_fb_destroy,
};

// Static DRM device instance
static struct drm_device *fishix_drm_dev = nullptr;

// Initialize Fishix DRM
int fishix_drm_init(void) {
    klib::printf("Fishix DRM: Initializing...\n");
    
    // Initialize DMA subsystem
    mem::dma::init();
    
    // Initialize DRM core
    drm_init();
    
    // Create DRM device
    fishix_drm_dev = drm_device_create(&fishix_drm_ops, nullptr);
    if (!fishix_drm_dev) {
        klib::printf("Fishix DRM: Failed to create device\n");
        return -ENOMEM;
    }
    
    // Register device node /dev/dri/card0
    dev::CharDevNode::register_node_initializer(DRM_MAJOR, 0, "dri/card0", []() {
        DRMDevNode *node = new DRMDevNode();
        if (fishix_drm_dev) fishix_drm_dev->inode = node;
        return node;
    });
    
    klib::printf("Fishix DRM: Initialized successfully\n");
    
    return 0;
}

// Cleanup Fishix DRM
void fishix_drm_cleanup(void) {
    if (fishix_drm_dev) {
        drm_device_destroy(fishix_drm_dev);
        fishix_drm_dev = nullptr;
    }
}

// Driver load callback
static int fishix_drm_load(struct drm_device *dev) {
    klib::printf("Fishix DRM: Loading driver...\n");
    
    // Allocate private data
    struct fishix_drm_private *priv = new struct fishix_drm_private();
    if (!priv) return -ENOMEM;
    
    // Get framebuffer info dari Fishix
    priv->fb_width = gfx::main_framebuffer.width;
    priv->fb_height = gfx::main_framebuffer.height;
    priv->fb_pitch = gfx::main_framebuffer.pitch;
    priv->fb_bpp = gfx::main_framebuffer.pixel_width * 8;
    priv->fb_phys = (uptr)gfx::main_framebuffer.addr - mem::hhdm;
    priv->fb_virt = (uptr)gfx::main_framebuffer.addr;
    
    klib::printf("Fishix DRM: Framebuffer %ux%u@%u, pitch %u, phys %#lX\n",
                 priv->fb_width, priv->fb_height, priv->fb_bpp, 
                 priv->fb_pitch, priv->fb_phys);
    
    // Allocate display buffer (double buffering)
    u32 fb_size = priv->fb_pitch * priv->fb_height;
    priv->display_buffer = mem::dma::alloc_coherent(fb_size, &priv->display_buffer_phys);
    if (!priv->display_buffer) {
        klib::printf("Fishix DRM: Failed to allocate display buffer\n");
        delete priv;
        return -ENOMEM;
    }
    
    // Create CRTC
    priv->crtc = drm_crtc_create(dev);
    if (!priv->crtc) {
        delete priv;
        return -ENOMEM;
    }
    
    // Create encoder
    priv->encoder = drm_encoder_create(dev, DRM_MODE_ENCODER_NONE);
    if (!priv->encoder) {
        delete priv;
        return -ENOMEM;
    }
    priv->encoder->crtc = priv->crtc;
    
    // Create connector
    priv->connector = drm_connector_create(dev, DRM_MODE_CONNECTOR_VIRTUAL);
    if (!priv->connector) {
        delete priv;
        return -ENOMEM;
    }
    priv->connector->encoder = priv->encoder;
    priv->connector->encoder_id = priv->encoder->id;
    priv->connector->mm_width = (priv->fb_width * 254) / 9600;  // 96 DPI approximation
    priv->connector->mm_height = (priv->fb_height * 254) / 9600;
    
    // Set connector modes
    drm_connector_set_modes(priv->connector);
    
    // Create primary plane
    u32 formats[] = { DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_RGB888 };
    priv->primary_plane = drm_plane_create(dev, DRM_PLANE_TYPE_PRIMARY, formats, 3);
    if (!priv->primary_plane) {
        delete priv;
        return -ENOMEM;
    }
    priv->primary_plane->crtc = priv->crtc;
    
    // Set initial mode (preferred)
    if (priv->connector->num_modes > 0) {
        memcpy(&priv->crtc->mode, &priv->connector->modes[0], sizeof(priv->crtc->mode));
        priv->crtc->enabled = true;
    }
    
    // Save private data
    dev->driver_private = priv;
    
    klib::printf("Fishix DRM: Driver loaded successfully\n");
    
    return 0;
}

// Driver unload callback
static void fishix_drm_unload(struct drm_device *dev) {
    klib::printf("Fishix DRM: Unloading driver...\n");
    
    struct fishix_drm_private *priv = (struct fishix_drm_private*)dev->driver_private;
    if (priv) {
        if (priv->display_buffer) {
            mem::dma::free_coherent(priv->fb_pitch * priv->fb_height, 
                                   priv->display_buffer, priv->display_buffer_phys);
        }
        delete priv;
        dev->driver_private = nullptr;
    }
    
    klib::printf("Fishix DRM: Driver unloaded\n");
}

// GEM create callback
static struct drm_gem_object* fishix_drm_gem_create(struct drm_device *dev, u32 size) {
    (void)dev;
    // Use default GEM implementation
    return drm_gem_object_create(dev, size);
}

// GEM destroy callback
static void fishix_drm_gem_destroy(struct drm_gem_object *obj) {
    // Use default GEM implementation
    drm_gem_object_destroy(obj);
}

// GEM mmap callback
static int fishix_drm_gem_mmap(struct drm_gem_object *obj, mem::MappedRange *vma) {
    (void)obj;
    (void)vma;
    return 0;
}

// CRTC set config callback
static int fishix_drm_crtc_set_config(struct drm_crtc *crtc, struct drm_mode_crtc *mode) {
    (void)crtc;
    (void)mode;
    
    klib::printf("Fishix DRM: Set CRTC config, mode=%s\n", 
                 mode->mode_valid ? mode->mode.name : "none");
    
    return 0;
}

// Page flip callback
static int fishix_drm_page_flip(struct drm_crtc *crtc, struct drm_framebuffer *fb, u64 user_data) {
    (void)user_data;
    
    struct drm_device *dev = crtc->dev;
    struct fishix_drm_private *priv = (struct fishix_drm_private*)dev->driver_private;
    
    if (!priv || !fb || !fb->obj[0]) return -EINVAL;
    
    struct drm_gem_object *obj = fb->obj[0];
    
    klib::printf("Fishix DRM: Page flip to FB %u, GEM %p\n", fb->id, obj);
    
    // Copy framebuffer content ke hardware framebuffer
    memcpy(gfx::main_framebuffer.addr, obj->vaddr, 
           priv->fb_pitch * priv->fb_height);
    
    // Complete page flip (synchronous untuk demo)
    drm_mode_page_flip_complete(crtc);
    
    return 0;
}

// FB create callback
static int fishix_drm_fb_create(struct drm_device *dev, struct drm_mode_fb_cmd2 *cmd) {
    (void)dev;
    (void)cmd;
    
    klib::printf("Fishix DRM: Create FB %ux%u, format %.4s\n",
                 cmd->width, cmd->height, (char*)&cmd->pixel_format);
    
    return 0;
}

// FB destroy callback
static void fishix_drm_fb_destroy(struct drm_framebuffer *fb) {
    klib::printf("Fishix DRM: Destroy FB %u\n", fb->id);
}

// Get DRM device
struct drm_device* fishix_drm_get_device(void) {
    return fishix_drm_dev;
}

} // namespace drm

// C interface untuk inisialisasi
extern "C" {

int fishix_drm_init(void) {
    return drm::fishix_drm_init();
}

void fishix_drm_cleanup(void) {
    drm::fishix_drm_cleanup();
}

} // extern "C"
