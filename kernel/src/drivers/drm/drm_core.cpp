// kernel/src/drivers/drm/drm_core.cpp
// DRM Core untuk Fishix Wayland support
// Implementasi utama DRM device, mode setting, dan framebuffer management

#include "drm_internal.hpp"
#include <drivers/drm/drm_internal.hpp>
#include <mem/dma.hpp>
#include <mem/vmm.hpp>
#include <fs/vfs.hpp>
#include <dev/devnode.hpp>
#include <klib/cstdio.hpp>
#include <klib/cstring.hpp>
#include <panic.hpp>

namespace drm {

// Global DRM state
klib::Vector<struct drm_device*> drm_devices;
klib::Spinlock drm_devices_lock;
static u32 next_device_id = 0;

// Initialize DRM subsystem
int drm_init(void) {
    (drm_devices_lock).unlock();
    
    // Initialize GEM subsystem
    gem_init();
    
    klib::printf("DRM: Core initialized\n");
    return 0;
}

// Create DRM device
struct drm_device* drm_device_create(struct drm_driver_ops *ops, void *private_data) {
    if (!ops) return nullptr;
    
    struct drm_device *dev = new struct drm_device();
    if (!dev) return nullptr;
    
    // Initialize device
    dev->id = next_device_id++;
    klib::snprintf(dev->name, sizeof(dev->name), "card%u", dev->id);
    
    dev->ops = ops;
    dev->driver_private = private_data;
    
    // Initialize caps (default)
    dev->caps.dumb_buffer = true;
    dev->caps.prime = true;
    dev->caps.async_page_flip = false;
    dev->caps.atomic = false;
    dev->caps.modifiers = false;
    
    // Initialize mode config
    dev->mode_config.min_width = 0;
    dev->mode_config.max_width = 16384;
    dev->mode_config.min_height = 0;
    dev->mode_config.max_height = 16384;
    dev->mode_config.cursor_width = 64;
    dev->mode_config.cursor_height = 64;
    
    // Initialize ID counters
    dev->next_crtc_id = 1;
    dev->next_encoder_id = 1;
    dev->next_connector_id = 1;
    dev->next_plane_id = 1;
    dev->next_fb_id = 1;
    dev->next_handle = 1;
    
    (dev->lock).unlock();
    (dev->list).init();
    
    // Call driver load
    if (ops->load) {
        int ret = ops->load(dev);
        if (ret < 0) {
            delete dev;
            return nullptr;
        }
    }
    
    // Add ke global list
    (drm_devices_lock).lock();
    drm_devices.push_back(dev);
    (drm_devices_lock).unlock();
    
    klib::printf("DRM: Created device %s\n", dev->name);
    
    return dev;
}

// Destroy DRM device
void drm_device_destroy(struct drm_device *dev) {
    if (!dev) return;
    
    // Call driver unload
    if (dev->ops->unload) {
        dev->ops->unload(dev);
    }
    
    // Remove dari global list
    (drm_devices_lock).lock();
    for (usize i = 0; i < drm_devices.size(); i++) {
        if (drm_devices[i] == dev) {
            drm_devices.remove(i);
            break;
        }
    }
    (drm_devices_lock).unlock();
    
    // Cleanup GEM objects
    for (usize i = 0; i < dev->gem_objects.size(); i++) {
        drm_gem_object_destroy(dev->gem_objects[i]);
    }
    
    // Cleanup framebuffers
    for (usize i = 0; i < dev->mode_config.fbs.size(); i++) {
        drm_framebuffer_destroy(dev->mode_config.fbs[i]);
    }
    
    // Cleanup CRTCs
    for (usize i = 0; i < dev->mode_config.crtcs.size(); i++) {
        drm_crtc_destroy(dev->mode_config.crtcs[i]);
    }
    
    // Cleanup encoders
    for (usize i = 0; i < dev->mode_config.encoders.size(); i++) {
        drm_encoder_destroy(dev->mode_config.encoders[i]);
    }
    
    // Cleanup connectors
    for (usize i = 0; i < dev->mode_config.connectors.size(); i++) {
        drm_connector_destroy(dev->mode_config.connectors[i]);
    }
    
    // Cleanup planes
    for (usize i = 0; i < dev->mode_config.planes.size(); i++) {
        drm_plane_destroy(dev->mode_config.planes[i]);
    }
    
    delete dev;
}

// Create CRTC
struct drm_crtc* drm_crtc_create(struct drm_device *dev) {
    if (!dev) return nullptr;
    
    struct drm_crtc *crtc = new struct drm_crtc();
    if (!crtc) return nullptr;
    
    crtc->id = dev->next_crtc_id++;
    crtc->index = dev->mode_config.crtcs.size();
    crtc->fb = nullptr;
    crtc->enabled = false;
    crtc->x = 0;
    crtc->y = 0;
    crtc->page_flip_pending = false;
    crtc->pending_fb = nullptr;
    crtc->dev = dev;
    (crtc->list).init();
    
    memset(&crtc->mode, 0, sizeof(crtc->mode));
    
    dev->mode_config.crtcs.push_back(crtc);
    
    klib::printf("DRM: Created CRTC %u\n", crtc->id);
    
    return crtc;
}

// Destroy CRTC
void drm_crtc_destroy(struct drm_crtc *crtc) {
    if (!crtc) return;
    delete crtc;
}

// Find CRTC by ID
struct drm_crtc* drm_crtc_find(struct drm_device *dev, u32 id) {
    if (!dev || id == 0) return nullptr;
    
    for (usize i = 0; i < dev->mode_config.crtcs.size(); i++) {
        if (dev->mode_config.crtcs[i]->id == id) {
            return dev->mode_config.crtcs[i];
        }
    }
    return nullptr;
}

// Create encoder
struct drm_encoder* drm_encoder_create(struct drm_device *dev, u32 type) {
    if (!dev) return nullptr;
    
    struct drm_encoder *encoder = new struct drm_encoder();
    if (!encoder) return nullptr;
    
    encoder->id = dev->next_encoder_id++;
    encoder->encoder_type = type;
    encoder->crtc = nullptr;
    encoder->possible_crtcs = 0xFFFFFFFF;  // All CRTCs
    encoder->possible_clones = 0;
    encoder->dev = dev;
    (encoder->list).init();
    
    dev->mode_config.encoders.push_back(encoder);
    
    klib::printf("DRM: Created encoder %u (type %u)\n", encoder->id, type);
    
    return encoder;
}

// Destroy encoder
void drm_encoder_destroy(struct drm_encoder *encoder) {
    if (!encoder) return;
    delete encoder;
}

// Find encoder by ID
struct drm_encoder* drm_encoder_find(struct drm_device *dev, u32 id) {
    if (!dev || id == 0) return nullptr;
    
    for (usize i = 0; i < dev->mode_config.encoders.size(); i++) {
        if (dev->mode_config.encoders[i]->id == id) {
            return dev->mode_config.encoders[i];
        }
    }
    return nullptr;
}

// Create connector
struct drm_connector* drm_connector_create(struct drm_device *dev, u32 type) {
    if (!dev) return nullptr;
    
    struct drm_connector *connector = new struct drm_connector();
    if (!connector) return nullptr;
    
    connector->id = dev->next_connector_id++;
    connector->connector_type = type;
    connector->connector_type_id = connector->id;  // Simplified
    connector->encoder = nullptr;
    connector->encoder_id = 0;
    connector->connection = DRM_MODE_CONNECTED;
    connector->mm_width = 527;   // 24" monitor default
    connector->mm_height = 296;
    connector->subconnector = DRM_MODE_SUBCONNECTOR_Unknown;
    connector->modes = nullptr;
    connector->num_modes = 0;
    connector->dev = dev;
    (connector->list).init();
    
    dev->mode_config.connectors.push_back(connector);
    
    klib::printf("DRM: Created connector %u (type %u)\n", connector->id, type);
    
    return connector;
}

// Destroy connector
void drm_connector_destroy(struct drm_connector *connector) {
    if (!connector) return;
    if (connector->modes) delete[] connector->modes;
    delete connector;
}

// Find connector by ID
struct drm_connector* drm_connector_find(struct drm_device *dev, u32 id) {
    if (!dev || id == 0) return nullptr;
    
    for (usize i = 0; i < dev->mode_config.connectors.size(); i++) {
        if (dev->mode_config.connectors[i]->id == id) {
            return dev->mode_config.connectors[i];
        }
    }
    return nullptr;
}

// Create plane
struct drm_plane* drm_plane_create(struct drm_device *dev, u32 type, u32 *formats, u32 num_formats) {
    if (!dev) return nullptr;
    
    struct drm_plane *plane = new struct drm_plane();
    if (!plane) return nullptr;
    
    plane->id = dev->next_plane_id++;
    plane->index = dev->mode_config.planes.size();
    plane->type = type;
    plane->crtc = nullptr;
    plane->fb = nullptr;
    plane->possible_crtcs = 0xFFFFFFFF;
    plane->formats = new u32[num_formats];
    plane->num_formats = num_formats;
    plane->crtc_x = 0;
    plane->crtc_y = 0;
    plane->crtc_w = 0;
    plane->crtc_h = 0;
    plane->src_x = 0;
    plane->src_y = 0;
    plane->src_w = 0;
    plane->src_h = 0;
    plane->dev = dev;
    (plane->list).init();
    
    for (u32 i = 0; i < num_formats; i++) {
        plane->formats[i] = formats[i];
    }
    
    dev->mode_config.planes.push_back(plane);
    
    klib::printf("DRM: Created plane %u (type %u)\n", plane->id, type);
    
    return plane;
}

// Destroy plane
void drm_plane_destroy(struct drm_plane *plane) {
    if (!plane) return;
    if (plane->formats) delete[] plane->formats;
    delete plane;
}

// Find plane by ID
struct drm_plane* drm_plane_find(struct drm_device *dev, u32 id) {
    if (!dev || id == 0) return nullptr;
    
    for (usize i = 0; i < dev->mode_config.planes.size(); i++) {
        if (dev->mode_config.planes[i]->id == id) {
            return dev->mode_config.planes[i];
        }
    }
    return nullptr;
}

// Create framebuffer
struct drm_framebuffer* drm_framebuffer_create(struct drm_device *dev) {
    if (!dev) return nullptr;
    
    struct drm_framebuffer *fb = new struct drm_framebuffer();
    if (!fb) return nullptr;
    
    fb->id = dev->next_fb_id++;
    fb->width = 0;
    fb->height = 0;
    fb->pitch = 0;
    fb->bpp = 0;
    fb->depth = 0;
    fb->pixel_format = 0;
    
    for (int i = 0; i < 4; i++) {
        fb->obj[i] = nullptr;
        fb->pitches[i] = 0;
        fb->offsets[i] = 0;
        fb->modifiers[i] = 0;
    }
    
    fb->dev = dev;
    (fb->list).init();
    
    dev->mode_config.fbs.push_back(fb);
    
    return fb;
}

// Destroy framebuffer
void drm_framebuffer_destroy(struct drm_framebuffer *fb) {
    if (!fb) return;
    
    // Release GEM objects
    for (int i = 0; i < 4; i++) {
        if (fb->obj[i]) {
            drm_gem_object_destroy(fb->obj[i]);
        }
    }
    
    delete fb;
}

// Find framebuffer by ID
struct drm_framebuffer* drm_framebuffer_lookup(struct drm_device *dev, u32 id) {
    if (!dev || id == 0) return nullptr;
    
    for (usize i = 0; i < dev->mode_config.fbs.size(); i++) {
        if (dev->mode_config.fbs[i]->id == id) {
            return dev->mode_config.fbs[i];
        }
    }
    return nullptr;
}

} // namespace drm
