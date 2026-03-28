// kernel/src/drivers/drm/drm_internal.hpp
// Internal DRM structures untuk Fishix kernel

#ifndef _FISHIX_DRM_INTERNAL_HPP
#define _FISHIX_DRM_INTERNAL_HPP

#include <klib/common.hpp>
#include <klib/vector.hpp>
#include <klib/cstring.hpp>
#include <uapi/drm/drm.h>
#include <uapi/linux/dma-buf.h>
#include <fs/vfs.hpp>
#include <kernel.hpp>
#include <sched/event.hpp>

namespace drm {

// Forward declarations
struct drm_device;
struct drm_file;
struct drm_gem_object;
struct drm_framebuffer;
struct drm_crtc;
struct drm_encoder;
struct drm_connector;
struct drm_plane;
struct drm_mode_config;

// DRM driver capabilities
struct drm_driver_caps {
    bool dumb_buffer : 1;
    bool prime : 1;
    bool async_page_flip : 1;
    bool atomic : 1;
    bool modifiers : 1;
};

// DRM driver operations
struct drm_driver_ops {
    // Device operations
    int (*load)(struct drm_device *dev);
    void (*unload)(struct drm_device *dev);
    
    // GEM operations
    struct drm_gem_object* (*gem_create)(struct drm_device *dev, u32 size);
    void (*gem_destroy)(struct drm_gem_object *obj);
    int (*gem_mmap)(struct drm_gem_object *obj, vfs::vm_area *vma);
    
    // Mode setting operations
    int (*crtc_set_config)(struct drm_crtc *crtc, struct drm_mode_crtc *mode);
    int (*page_flip)(struct drm_crtc *crtc, struct drm_framebuffer *fb, u64 user_data);
    
    // Framebuffer operations
    int (*fb_create)(struct drm_device *dev, 
                      struct drm_mode_fb_cmd2 *cmd);
    void (*fb_destroy)(struct drm_framebuffer *fb);
};

// GEM Object - Graphics memory object
struct drm_gem_object {
    u32 handle;              // Userspace handle
    u32 name;                // Global name (flink)
    u32 size;                // Size in bytes
    
    void *vaddr;             // Kernel virtual address
    paddr_t paddr;           // Physical address
    
    u32 refcount;            // Reference count
    struct drm_device *dev;  // Parent device
    
    // DMA-BUF attachment
    struct dma_buf *dmabuf;
    bool export_attach;
    
    // List node
    klib::ListHead list;
};

// Framebuffer
struct drm_framebuffer {
    u32 id;                  // FB ID
    u32 width;               // Width in pixels
    u32 height;              // Height in pixels
    u32 pitch;               // Bytes per row
    u32 bpp;                 // Bits per pixel
    u32 depth;               // Color depth
    u32 pixel_format;        // FourCC format
    
    struct drm_gem_object *obj[4];  // GEM objects (planes)
    u32 pitches[4];          // Pitch per plane
    u32 offsets[4];          // Offset per plane
    u64 modifiers[4];        // Format modifiers
    
    struct drm_device *dev;
    klib::ListHead list;
};

// CRTC (CRT Controller)
struct drm_crtc {
    u32 id;                  // CRTC ID
    u32 index;               // CRTC index
    
    struct drm_framebuffer *fb;     // Current framebuffer
    struct drm_mode_modeinfo mode;  // Current mode
    
    bool enabled;            // Is enabled?
    u32 x, y;                // Position on screen
    
    // Page flip state
    bool page_flip_pending;
    u64 page_flip_user_data;
    struct drm_framebuffer *pending_fb;
    
    struct drm_device *dev;
    klib::ListHead list;
};

// Encoder
struct drm_encoder {
    u32 id;                  // Encoder ID
    u32 encoder_type;        // Type (TMDS, LVDS, etc.)
    
    struct drm_crtc *crtc;   // Connected CRTC
    u32 possible_crtcs;      // Bitmask of possible CRTCs
    u32 possible_clones;     // Bitmask of cloneable encoders
    
    struct drm_device *dev;
    klib::ListHead list;
};

// Connector
struct drm_connector {
    u32 id;                  // Connector ID
    u32 connector_type;      // Type (HDMI, VGA, etc.)
    u32 connector_type_id;   // Type-specific ID
    
    struct drm_encoder *encoder;  // Connected encoder
    u32 encoder_id;          // Encoder ID for userspace
    
    u32 connection;          // DRM_MODE_CONNECTED/DISCONNECTED
    u32 mm_width, mm_height; // Physical size
    u32 subconnector;        // Subconnector type
    
    struct drm_mode_modeinfo *modes;  // Available modes
    u32 num_modes;
    
    struct drm_device *dev;
    klib::ListHead list;
};

// Plane
struct drm_plane {
    u32 id;                  // Plane ID
    u32 index;               // Plane index
    u32 type;                // Type (primary, cursor, overlay)
    
    struct drm_crtc *crtc;   // Connected CRTC
    struct drm_framebuffer *fb;  // Current framebuffer
    
    u32 possible_crtcs;      // Bitmask of possible CRTCs
    u32 *formats;            // Supported formats
    u32 num_formats;
    
    i32 crtc_x, crtc_y;      // Position on CRTC
    u32 crtc_w, crtc_h;      // Size on CRTC
    u32 src_x, src_y;        // Source position
    u32 src_w, src_h;        // Source size
    
    struct drm_device *dev;
    klib::ListHead list;
};

// Mode configuration
struct drm_mode_config {
    u32 min_width, max_width;
    u32 min_height, max_height;
    
    klib::Vector<struct drm_crtc*> crtcs;
    klib::Vector<struct drm_encoder*> encoders;
    klib::Vector<struct drm_connector*> connectors;
    klib::Vector<struct drm_plane*> planes;
    klib::Vector<struct drm_framebuffer*> fbs;
    
    // Cursor size
    u32 cursor_width;
    u32 cursor_height;
};

// DRM File (per-open state)
struct drm_file {
    u32 id;                  // File ID
    bool authenticated;      // Is authenticated?
    bool master;             // Is master?
    
    struct drm_device *dev;
    
    // Handle -> GEM object mapping
    klib::Vector<struct drm_gem_object*> objects;
    
    // Event queue
    klib::ListHead event_list;
    klib::Spinlock event_lock;
    
    // List node
    klib::ListHead list;
};

// DRM Event
struct drm_pending_event {
    struct drm_event event;
    u64 user_data;
    struct drm_file *file;
    klib::ListHead link;
};

// DRM Device
struct drm_device {
    u32 id;                  // Device ID
    char name[32];           // Device name
    
    // Driver
    struct drm_driver_ops *ops;
    struct drm_driver_caps caps;
    void *driver_private;    // Driver-specific data
    
    // Mode config
    struct drm_mode_config mode_config;
    
    // Objects
    klib::Vector<struct drm_file*> files;
    klib::Vector<struct drm_gem_object*> gem_objects;
    
    // ID counters
    u32 next_crtc_id;
    u32 next_encoder_id;
    u32 next_connector_id;
    u32 next_plane_id;
    u32 next_fb_id;
    u32 next_handle;
    
    // VFS
    vfs::inode *inode;
    vfs::file *file;
    
    // Lock
    klib::Spinlock lock;
    
    // List node
    klib::ListHead list;
};

// Plane types
#define DRM_PLANE_TYPE_PRIMARY  0
#define DRM_PLANE_TYPE_CURSOR   1
#define DRM_PLANE_TYPE_OVERLAY  2

// Global DRM state
extern klib::Vector<struct drm_device*> drm_devices;
extern klib::Spinlock drm_devices_lock;

// Function prototypes
int drm_init(void);
struct drm_device* drm_device_create(struct drm_driver_ops *ops, void *private_data);
void drm_device_destroy(struct drm_device *dev);
void gem_init(void);
void drm_connector_set_modes(struct drm_connector *connector);
void drm_mode_page_flip_complete(struct drm_crtc *crtc);

// File operations
int drm_open(vfs::file *file);
int drm_close(vfs::file *file);
ssize_t drm_read(vfs::file *file, void *buf, size_t count);
ssize_t drm_write(vfs::file *file, const void *buf, size_t count);
int drm_ioctl(vfs::file *file, unsigned long request, void *arg);
int drm_mmap(vfs::vm_area *vma);
int drm_poll(vfs::file *file);

// GEM operations
struct drm_gem_object* drm_gem_object_create(struct drm_device *dev, u32 size);
void drm_gem_object_destroy(struct drm_gem_object *obj);
struct drm_gem_object* drm_gem_object_lookup(struct drm_file *file, u32 handle);
int drm_gem_handle_create(struct drm_file *file, struct drm_gem_object *obj, u32 *handle);
int drm_gem_handle_delete(struct drm_file *file, u32 handle);
int drm_gem_flink(struct drm_file *file, u32 handle, u32 *name);
int drm_gem_open(struct drm_file *file, u32 name, u32 *handle, u64 *size);

// Framebuffer operations
struct drm_framebuffer* drm_framebuffer_create(struct drm_device *dev);
void drm_framebuffer_destroy(struct drm_framebuffer *fb);
struct drm_framebuffer* drm_framebuffer_lookup(struct drm_device *dev, u32 id);

// CRTC operations
struct drm_crtc* drm_crtc_create(struct drm_device *dev);
void drm_crtc_destroy(struct drm_crtc *crtc);
struct drm_crtc* drm_crtc_find(struct drm_device *dev, u32 id);

// Encoder operations
struct drm_encoder* drm_encoder_create(struct drm_device *dev, u32 type);
void drm_encoder_destroy(struct drm_encoder *encoder);
struct drm_encoder* drm_encoder_find(struct drm_device *dev, u32 id);

// Connector operations
struct drm_connector* drm_connector_create(struct drm_device *dev, u32 type);
void drm_connector_destroy(struct drm_connector *connector);
struct drm_connector* drm_connector_find(struct drm_device *dev, u32 id);

// Plane operations
struct drm_plane* drm_plane_create(struct drm_device *dev, u32 type, u32 *formats, u32 num_formats);
void drm_plane_destroy(struct drm_plane *plane);
struct drm_plane* drm_plane_find(struct drm_device *dev, u32 id);

// Mode operations
int drm_mode_set_crtc(struct drm_file *file, struct drm_mode_crtc *crtc);
int drm_mode_page_flip(struct drm_file *file, struct drm_mode_crtc_page_flip *flip);
int drm_mode_get_resources(struct drm_file *file, struct drm_mode_card_res *res);
int drm_mode_get_connector(struct drm_file *file, struct drm_mode_get_connector *conn);
int drm_mode_get_encoder(struct drm_file *file, struct drm_mode_get_encoder *enc);
int drm_mode_get_crtc(struct drm_file *file, struct drm_mode_crtc *crtc);
int drm_mode_create_dumb(struct drm_file *file, struct drm_mode_create_dumb *dumb);
int drm_mode_map_dumb(struct drm_file *file, struct drm_mode_map_dumb *dumb);
int drm_mode_destroy_dumb(struct drm_file *file, struct drm_mode_destroy_dumb *dumb);
int drm_mode_addfb2(struct drm_file *file, struct drm_mode_fb_cmd2 *fb);
int drm_mode_rmfb(struct drm_file *file, u32 fb_id);
int drm_mode_getfb(struct drm_file *file, struct drm_mode_fb_cmd *fb);

// Event operations
int drm_send_event(struct drm_file *file, struct drm_event *event, u64 user_data);
int drm_send_vblank_event(struct drm_file *file, u32 crtc_id, u64 user_data);
int drm_send_flip_event(struct drm_file *file, u32 crtc_id, u64 user_data);

// IOCTL handlers
int drm_ioctl_version(struct drm_file *file, struct drm_version *version);
int drm_ioctl_get_cap(struct drm_file *file, struct drm_get_cap *cap);
int drm_ioctl_set_client_cap(struct drm_file *file, struct drm_set_client_cap *cap);
int drm_ioctl_gem_close(struct drm_file *file, struct drm_gem_close *close);
int drm_ioctl_gem_flink(struct drm_file *file, struct drm_gem_flink *flink);
int drm_ioctl_gem_open(struct drm_file *file, struct drm_gem_open *open);

} // namespace drm

#endif // _FISHIX_DRM_INTERNAL_HPP
