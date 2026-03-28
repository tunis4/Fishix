// kernel/src/drivers/drm/drm_modeset.cpp
// Mode Setting untuk Fishix Wayland support
// Implementasi KMS (Kernel Mode Setting) operations

#include <drivers/drm/drm_internal.hpp>
#include <mem/dma.hpp>
#include <klib/cstdio.hpp>
#include <klib/cstring.hpp>
#include <klib/cstring.hpp>
#include <panic.hpp>

namespace drm {

// Standard video modes (simplified)
static struct drm_mode_modeinfo standard_modes[] = {
    // 1920x1080 @ 60Hz
    {
        .clock = 148500,
        .hdisplay = 1920,
        .hsync_start = 2008,
        .hsync_end = 2052,
        .htotal = 2200,
        .vdisplay = 1080,
        .vsync_start = 1084,
        .vsync_end = 1089,
        .vtotal = 1125,
        .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
        .type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER,
        .name = "1920x1080"
    },
    // 1280x720 @ 60Hz
    {
        .clock = 74250,
        .hdisplay = 1280,
        .hsync_start = 1390,
        .hsync_end = 1430,
        .htotal = 1650,
        .vdisplay = 720,
        .vsync_start = 725,
        .vsync_end = 730,
        .vtotal = 750,
        .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
        .type = DRM_MODE_TYPE_DRIVER,
        .name = "1280x720"
    },
    // 1024x768 @ 60Hz
    {
        .clock = 65000,
        .hdisplay = 1024,
        .hsync_start = 1048,
        .hsync_end = 1184,
        .htotal = 1344,
        .vdisplay = 768,
        .vsync_start = 771,
        .vsync_end = 777,
        .vtotal = 806,
        .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
        .type = DRM_MODE_TYPE_DRIVER,
        .name = "1024x768"
    },
    // 800x600 @ 60Hz
    {
        .clock = 40000,
        .hdisplay = 800,
        .hsync_start = 840,
        .hsync_end = 968,
        .htotal = 1056,
        .vdisplay = 600,
        .vsync_start = 601,
        .vsync_end = 605,
        .vtotal = 628,
        .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
        .type = DRM_MODE_TYPE_DRIVER,
        .name = "800x600"
    },
    // 640x480 @ 60Hz
    {
        .clock = 25175,
        .hdisplay = 640,
        .hsync_start = 656,
        .hsync_end = 752,
        .htotal = 800,
        .vdisplay = 480,
        .vsync_start = 490,
        .vsync_end = 492,
        .vtotal = 525,
        .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
        .type = DRM_MODE_TYPE_DRIVER,
        .name = "640x480"
    }
};

static const int num_standard_modes = sizeof(standard_modes) / sizeof(standard_modes[0]);

// Set connector modes (called saat connector created)
void drm_connector_set_modes(struct drm_connector *connector) {
    if (!connector) return;
    
    // Allocate modes array
    connector->modes = new struct drm_mode_modeinfo[num_standard_modes];
    connector->num_modes = num_standard_modes;
    
    // Copy standard modes
    for (int i = 0; i < num_standard_modes; i++) {
        memcpy(&connector->modes[i], &standard_modes[i], sizeof(struct drm_mode_modeinfo));
    }
}

// IOCTL: Get resources
int drm_mode_get_resources(struct drm_file *file, struct drm_mode_card_res *res) {
    if (!file || !res) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    // Copy FB IDs
    if (res->fb_id_ptr && res->count_fbs >= dev->mode_config.fbs.size()) {
        u32 *fb_ids = (u32*)res->fb_id_ptr;
        for (usize i = 0; i < dev->mode_config.fbs.size(); i++) {
            fb_ids[i] = dev->mode_config.fbs[i]->id;
        }
    }
    res->count_fbs = dev->mode_config.fbs.size();
    
    // Copy CRTC IDs
    if (res->crtc_id_ptr && res->count_crtcs >= dev->mode_config.crtcs.size()) {
        u32 *crtc_ids = (u32*)res->crtc_id_ptr;
        for (usize i = 0; i < dev->mode_config.crtcs.size(); i++) {
            crtc_ids[i] = dev->mode_config.crtcs[i]->id;
        }
    }
    res->count_crtcs = dev->mode_config.crtcs.size();
    
    // Copy connector IDs
    if (res->connector_id_ptr && res->count_connectors >= dev->mode_config.connectors.size()) {
        u32 *connector_ids = (u32*)res->connector_id_ptr;
        for (usize i = 0; i < dev->mode_config.connectors.size(); i++) {
            connector_ids[i] = dev->mode_config.connectors[i]->id;
        }
    }
    res->count_connectors = dev->mode_config.connectors.size();
    
    // Copy encoder IDs
    if (res->encoder_id_ptr && res->count_encoders >= dev->mode_config.encoders.size()) {
        u32 *encoder_ids = (u32*)res->encoder_id_ptr;
        for (usize i = 0; i < dev->mode_config.encoders.size(); i++) {
            encoder_ids[i] = dev->mode_config.encoders[i]->id;
        }
    }
    res->count_encoders = dev->mode_config.encoders.size();
    
    // Min/max dimensions
    res->min_width = dev->mode_config.min_width;
    res->max_width = dev->mode_config.max_width;
    res->min_height = dev->mode_config.min_height;
    res->max_height = dev->mode_config.max_height;
    
    (dev->lock).unlock();
    
    return 0;
}

// IOCTL: Get connector
int drm_mode_get_connector(struct drm_file *file, struct drm_mode_get_connector *conn) {
    if (!file || !conn) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_connector *connector = drm_connector_find(dev, conn->connector_id);
    if (!connector) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    // Copy modes
    if (conn->modes_ptr && conn->count_modes >= connector->num_modes) {
        struct drm_mode_modeinfo *modes = (struct drm_mode_modeinfo*)conn->modes_ptr;
        for (u32 i = 0; i < connector->num_modes; i++) {
            memcpy(&modes[i], &connector->modes[i], sizeof(struct drm_mode_modeinfo));
        }
    }
    conn->count_modes = connector->num_modes;
    
    // Copy encoder IDs (simplified - hanya 1 encoder)
    if (conn->encoders_ptr && conn->count_encoders >= 1) {
        u32 *encoders = (u32*)conn->encoders_ptr;
        if (connector->encoder) {
            encoders[0] = connector->encoder->id;
        }
    }
    conn->count_encoders = connector->encoder ? 1 : 0;
    
    // Connector info
    conn->encoder_id = connector->encoder ? connector->encoder->id : 0;
    conn->connection = connector->connection;
    conn->mm_width = connector->mm_width;
    conn->mm_height = connector->mm_height;
    conn->subconnector = connector->subconnector;
    conn->count_props = 0;
    
    (dev->lock).unlock();
    
    return 0;
}

// IOCTL: Get encoder
int drm_mode_get_encoder(struct drm_file *file, struct drm_mode_get_encoder *enc) {
    if (!file || !enc) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_encoder *encoder = drm_encoder_find(dev, enc->encoder_id);
    if (!encoder) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    enc->encoder_type = encoder->encoder_type;
    enc->crtc_id = encoder->crtc ? encoder->crtc->id : 0;
    enc->possible_crtcs = encoder->possible_crtcs;
    enc->possible_clones = encoder->possible_clones;
    
    (dev->lock).unlock();
    
    return 0;
}

// IOCTL: Get CRTC
int drm_mode_get_crtc(struct drm_file *file, struct drm_mode_crtc *crtc) {
    if (!file || !crtc) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_crtc *c = drm_crtc_find(dev, crtc->crtc_id);
    if (!c) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    crtc->fb_id = c->fb ? c->fb->id : 0;
    crtc->x = c->x;
    crtc->y = c->y;
    crtc->gamma_size = 0;
    crtc->mode_valid = c->enabled ? 1 : 0;
    
    if (c->enabled) {
        memcpy(&crtc->mode, &c->mode, sizeof(crtc->mode));
    }
    
    (dev->lock).unlock();
    
    return 0;
}

// IOCTL: Set CRTC
int drm_mode_set_crtc(struct drm_file *file, struct drm_mode_crtc *crtc) {
    if (!file || !crtc) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_crtc *c = drm_crtc_find(dev, crtc->crtc_id);
    if (!c) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    // Find framebuffer
    struct drm_framebuffer *fb = nullptr;
    if (crtc->fb_id != 0) {
        fb = drm_framebuffer_lookup(dev, crtc->fb_id);
        if (!fb) {
            (dev->lock).unlock();
            return -ENOENT;
        }
    }
    
    // Set mode
    if (crtc->mode_valid) {
        memcpy(&c->mode, &crtc->mode, sizeof(c->mode));
        c->enabled = true;
    } else {
        c->enabled = false;
    }
    
    c->x = crtc->x;
    c->y = crtc->y;
    c->fb = fb;
    
    // Call driver callback
    if (dev->ops->crtc_set_config) {
        int ret = dev->ops->crtc_set_config(c, crtc);
        if (ret < 0) {
            (dev->lock).unlock();
            return ret;
        }
    }
    
    (dev->lock).unlock();
    
    klib::printf("DRM: Set CRTC %u, FB %u, mode %s\n", 
                 crtc->crtc_id, crtc->fb_id, 
                 crtc->mode_valid ? crtc->mode.name : "none");
    
    return 0;
}

// IOCTL: Page flip
int drm_mode_page_flip(struct drm_file *file, struct drm_mode_crtc_page_flip *flip) {
    if (!file || !flip) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_crtc *crtc = drm_crtc_find(dev, flip->crtc_id);
    if (!crtc) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    // Check if page flip pending
    if (crtc->page_flip_pending) {
        (dev->lock).unlock();
        return -EBUSY;
    }
    
    // Find framebuffer
    struct drm_framebuffer *fb = drm_framebuffer_lookup(dev, flip->fb_id);
    if (!fb) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    // Set pending state
    crtc->page_flip_pending = true;
    crtc->page_flip_user_data = flip->user_data;
    crtc->pending_fb = fb;
    
    // Call driver callback
    if (dev->ops->page_flip) {
        int ret = dev->ops->page_flip(crtc, fb, flip->user_data);
        if (ret < 0) {
            crtc->page_flip_pending = false;
            crtc->pending_fb = nullptr;
            (dev->lock).unlock();
            return ret;
        }
    } else {
        // Synchronous flip (fallback)
        crtc->fb = fb;
        crtc->page_flip_pending = false;
        
        // Send flip complete event
        if (flip->flags & 0x01) {  // DRM_MODE_PAGE_FLIP_EVENT
            drm_send_flip_event(file, flip->crtc_id, flip->user_data);
        }
    }
    
    (dev->lock).unlock();
    
    klib::printf("DRM: Page flip CRTC %u -> FB %u\n", flip->crtc_id, flip->fb_id);
    
    return 0;
}

// Complete page flip (called by driver saat VBlank)
void drm_mode_page_flip_complete(struct drm_crtc *crtc) {
    if (!crtc) return;
    
    (crtc->dev->lock).lock();
    
    if (crtc->page_flip_pending && crtc->pending_fb) {
        crtc->fb = crtc->pending_fb;
        crtc->pending_fb = nullptr;
        crtc->page_flip_pending = false;
        
        // Send event ke all files
        // (simplified - seharusnya ke file yang request flip)
        for (usize i = 0; i < crtc->dev->files.size(); i++) {
            drm_send_flip_event(crtc->dev->files[i], crtc->id, crtc->page_flip_user_data);
        }
    }
    
    (crtc->dev->lock).unlock();
}

// IOCTL: Create dumb buffer
int drm_mode_create_dumb(struct drm_file *file, struct drm_mode_create_dumb *dumb) {
    if (!file || !dumb) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    // Calculate size
    u32 pitch = dumb->width * ((dumb->bpp + 7) / 8);
    u32 size = pitch * dumb->height;
    
    // Align size ke page boundary
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Create GEM object
    struct drm_gem_object *obj = drm_gem_object_create(dev, size);
    if (!obj) return -ENOMEM;
    
    // Create handle
    u32 handle;
    int ret = drm_gem_handle_create(file, obj, &handle);
    if (ret < 0) {
        drm_gem_object_destroy(obj);
        return ret;
    }
    
    dumb->handle = handle;
    dumb->pitch = pitch;
    dumb->size = size;
    
    klib::printf("DRM: Created dumb buffer %ux%u@%u, handle %u, size %u\n",
                 dumb->width, dumb->height, dumb->bpp, handle, size);
    
    return 0;
}

// IOCTL: Map dumb buffer
int drm_mode_map_dumb(struct drm_file *file, struct drm_mode_map_dumb *dumb) {
    if (!file || !dumb) return -EINVAL;
    
    struct drm_gem_object *obj = drm_gem_object_lookup(file, dumb->handle);
    if (!obj) return -ENOENT;
    
    // Return offset untuk mmap (handle * PAGE_SIZE sebagai fake offset)
    dumb->offset = (u64)dumb->handle * PAGE_SIZE;
    
    drm_gem_object_destroy(obj);
    
    return 0;
}

// IOCTL: Destroy dumb buffer
int drm_mode_destroy_dumb(struct drm_file *file, struct drm_mode_destroy_dumb *dumb) {
    if (!file || !dumb) return -EINVAL;
    return drm_gem_handle_delete(file, dumb->handle);
}

// IOCTL: Add FB2
int drm_mode_addfb2(struct drm_file *file, struct drm_mode_fb_cmd2 *fb) {
    if (!file || !fb) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    // Validate parameters
    if (fb->width == 0 || fb->height == 0) return -EINVAL;
    if (fb->handles[0] == 0) return -EINVAL;
    
    // Create framebuffer
    struct drm_framebuffer *framebuffer = drm_framebuffer_create(dev);
    if (!framebuffer) return -ENOMEM;
    
    framebuffer->width = fb->width;
    framebuffer->height = fb->height;
    framebuffer->pixel_format = fb->pixel_format;
    framebuffer->pitch = fb->pitches[0];
    
    // Set bpp berdasarkan format
    switch (fb->pixel_format) {
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_ABGR8888:
            framebuffer->bpp = 32;
            framebuffer->depth = 24;
            break;
        case DRM_FORMAT_RGB888:
        case DRM_FORMAT_BGR888:
            framebuffer->bpp = 24;
            framebuffer->depth = 24;
            break;
        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_BGR565:
            framebuffer->bpp = 16;
            framebuffer->depth = 16;
            break;
        default:
            framebuffer->bpp = 32;
            framebuffer->depth = 24;
            break;
    }
    
    // Attach GEM objects
    for (int i = 0; i < 4; i++) {
        if (fb->handles[i] != 0) {
            struct drm_gem_object *obj = drm_gem_object_lookup(file, fb->handles[i]);
            if (!obj) {
                drm_framebuffer_destroy(framebuffer);
                return -ENOENT;
            }
            framebuffer->obj[i] = obj;
            framebuffer->pitches[i] = fb->pitches[i];
            framebuffer->offsets[i] = fb->offsets[i];
            framebuffer->modifiers[i] = fb->modifier[i];
        }
    }
    
    // Call driver callback
    if (dev->ops->fb_create) {
        int ret = dev->ops->fb_create(dev, fb);
        if (ret < 0) {
            drm_framebuffer_destroy(framebuffer);
            return ret;
        }
    }
    
    fb->fb_id = framebuffer->id;
    
    klib::printf("DRM: Created FB %u, %ux%u, format %.4s\n",
                 framebuffer->id, fb->width, fb->height, (char*)&fb->pixel_format);
    
    return 0;
}

// IOCTL: Remove FB
int drm_mode_rmfb(struct drm_file *file, u32 fb_id) {
    if (!file || fb_id == 0) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_framebuffer *fb = drm_framebuffer_lookup(dev, fb_id);
    if (!fb) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    // Remove dari list
    for (usize i = 0; i < dev->mode_config.fbs.size(); i++) {
        if (dev->mode_config.fbs[i] == fb) {
            dev->mode_config.fbs.remove(i);
            break;
        }
    }
    
    (dev->lock).unlock();
    
    // Call driver callback
    if (dev->ops->fb_destroy) {
        dev->ops->fb_destroy(fb);
    }
    
    drm_framebuffer_destroy(fb);
    
    klib::printf("DRM: Removed FB %u\n", fb_id);
    
    return 0;
}

// IOCTL: Get FB
int drm_mode_getfb(struct drm_file *file, struct drm_mode_fb_cmd *fb) {
    if (!file || !fb) return -EINVAL;
    
    struct drm_device *dev = file->dev;
    
    (dev->lock).lock();
    
    struct drm_framebuffer *framebuffer = drm_framebuffer_lookup(dev, fb->fb_id);
    if (!framebuffer) {
        (dev->lock).unlock();
        return -ENOENT;
    }
    
    fb->width = framebuffer->width;
    fb->height = framebuffer->height;
    fb->pitch = framebuffer->pitch;
    fb->bpp = framebuffer->bpp;
    fb->depth = framebuffer->depth;
    fb->handle = framebuffer->obj[0] ? framebuffer->obj[0]->handle : 0;
    
    (dev->lock).unlock();
    
    return 0;
}

} // namespace drm
