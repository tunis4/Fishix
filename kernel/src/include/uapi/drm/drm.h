// kernel/src/include/uapi/drm/drm.h
// UAPI DRM header untuk Fishix - Compatible dengan Linux DRM ABI
// Untuk Wayland support

#ifndef _FISHIX_DRM_H
#define _FISHIX_DRM_H

#include <klib/common.hpp>

#define DRM_MAJOR 226
#define DRM_IOCTL_BASE 'd'

#define DRM_IO(nr) _IOC(DRM_IOCTL_BASE, nr, 0, 0)
#define DRM_IOR(nr, type) _IOR(DRM_IOCTL_BASE, nr, type)
#define DRM_IOW(nr, type) _IOW(DRM_IOCTL_BASE, nr, type)
#define DRM_IOWR(nr, type) _IOWR(DRM_IOCTL_BASE, nr, type)

// IOCTL numbers (from Linux drm_ioctl.h)
#define DRM_IOCTL_VERSION           DRM_IOWR(0x00, struct drm_version)
#define DRM_IOCTL_GET_UNIQUE        DRM_IOWR(0x01, struct drm_unique)
#define DRM_IOCTL_GET_MAGIC         DRM_IOR(0x02, struct drm_auth)
#define DRM_IOCTL_IRQ_BUSID         DRM_IOWR(0x03, struct drm_irq_busid)
#define DRM_IOCTL_GET_MAP           DRM_IOWR(0x04, struct drm_map)
#define DRM_IOCTL_GET_CLIENT        DRM_IOWR(0x05, struct drm_client)
#define DRM_IOCTL_GET_STATS         DRM_IOR(0x06, struct drm_stats)
#define DRM_IOCTL_SET_VERSION       DRM_IOWR(0x07, struct drm_set_version)
#define DRM_IOCTL_MODESET_CTL       DRM_IOW(0x08, struct drm_modeset_ctl)
#define DRM_IOCTL_GEM_CLOSE         DRM_IOW(0x09, struct drm_gem_close)
#define DRM_IOCTL_GEM_FLINK         DRM_IOWR(0x0a, struct drm_gem_flink)
#define DRM_IOCTL_GEM_OPEN          DRM_IOWR(0x0b, struct drm_gem_open)
#define DRM_IOCTL_GET_CAP           DRM_IOWR(0x0c, struct drm_get_cap)
#define DRM_IOCTL_SET_CLIENT_CAP    DRM_IOW(0x0d, struct drm_set_client_cap)

// Mode setting IOCTLs
#define DRM_IOCTL_MODE_GETRESOURCES     DRM_IOWR(0xa0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC          DRM_IOWR(0xa1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC          DRM_IOWR(0xa2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_CURSOR           DRM_IOWR(0xa3, struct drm_mode_cursor)
#define DRM_IOCTL_MODE_GETGAMMA         DRM_IOWR(0xa4, struct drm_mode_crtc_lut)
#define DRM_IOCTL_MODE_SETGAMMA         DRM_IOWR(0xa5, struct drm_mode_crtc_lut)
#define DRM_IOCTL_MODE_GETENCODER       DRM_IOWR(0xa6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR     DRM_IOWR(0xa7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_ATTACHMODE       DRM_IOWR(0xa8, struct drm_mode_mode_cmd)
#define DRM_IOCTL_MODE_DETACHMODE       DRM_IOWR(0xa9, struct drm_mode_mode_cmd)
#define DRM_IOCTL_MODE_GETPROPERTY      DRM_IOWR(0xaa, struct drm_mode_get_property)
#define DRM_IOCTL_MODE_SETPROPERTY      DRM_IOWR(0xab, struct drm_mode_connector_set_property)
#define DRM_IOCTL_MODE_GETPROPBLOB      DRM_IOWR(0xac, struct drm_mode_get_blob)
#define DRM_IOCTL_MODE_GETFB            DRM_IOWR(0xad, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_ADDFB            DRM_IOWR(0xae, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_RMFB             DRM_IOWR(0xaf, unsigned int)
#define DRM_IOCTL_MODE_PAGE_FLIP        DRM_IOWR(0xb0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_DIRTYFB          DRM_IOWR(0xb1, struct drm_mode_fb_dirty_cmd)
#define DRM_IOCTL_MODE_CREATE_DUMB      DRM_IOWR(0xb2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB         DRM_IOWR(0xb3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB     DRM_IOWR(0xb4, struct drm_mode_destroy_dumb)
#define DRM_IOCTL_MODE_GETPLANERESOURCES DRM_IOWR(0xb5, struct drm_mode_get_plane_res)
#define DRM_IOCTL_MODE_GETPLANE         DRM_IOWR(0xb6, struct drm_mode_get_plane)
#define DRM_IOCTL_MODE_SETPLANE         DRM_IOWR(0xb7, struct drm_mode_set_plane)
#define DRM_IOCTL_MODE_ADDFB2           DRM_IOWR(0xb8, struct drm_mode_fb_cmd2)
#define DRM_IOCTL_MODE_OBJ_GETPROPERTIES DRM_IOWR(0xb9, struct drm_mode_obj_get_properties)
#define DRM_IOCTL_MODE_OBJ_SETPROPERTY  DRM_IOWR(0xba, struct drm_mode_obj_set_property)
#define DRM_IOCTL_MODE_CURSOR2          DRM_IOWR(0xbb, struct drm_mode_cursor2)
#define DRM_IOCTL_MODE_ATOMIC           DRM_IOWR(0xbc, struct drm_mode_atomic)

// Capabilities
#define DRM_CAP_DUMB_BUFFER             0x1
#define DRM_CAP_VBLANK_HIGH_CRTC        0x2
#define DRM_CAP_DUMB_PREFERRED_DEPTH    0x3
#define DRM_CAP_DUMB_PREFER_SHADOW      0x4
#define DRM_CAP_PRIME                   0x5
#define DRM_CAP_TIMESTAMP_MONOTONIC     0x6
#define DRM_CAP_ASYNC_PAGE_FLIP         0x7
#define DRM_CAP_CURSOR_WIDTH            0x8
#define DRM_CAP_CURSOR_HEIGHT           0x9
#define DRM_CAP_ADDFB2_MODIFIERS        0x10
#define DRM_CAP_PAGE_FLIP_TARGET        0x11
#define DRM_CAP_CRTC_IN_VBLANK_EVENT    0x12
#define DRM_CAP_SYNCOBJ                 0x13
#define DRM_CAP_SYNCOBJ_TIMELINE        0x14

// Client capabilities
#define DRM_CLIENT_CAP_STEREO_3D        1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC           3
#define DRM_CLIENT_CAP_ASPECT_RATIO     4
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5

// DRM modes
#define DRM_MODE_CONNECTED              1
#define DRM_MODE_DISCONNECTED           2
#define DRM_MODE_UNKNOWNCONNECTION      3

#define DRM_MODE_SUBCONNECTOR_Automatic 0
#define DRM_MODE_SUBCONNECTOR_Unknown   0
#define DRM_MODE_SUBCONNECTOR_DVID      1
#define DRM_MODE_SUBCONNECTOR_DVIA      2
#define DRM_MODE_SUBCONNECTOR_Composite 3
#define DRM_MODE_SUBCONNECTOR_SVIDEO    4
#define DRM_MODE_SUBCONNECTOR_Component 5
#define DRM_MODE_SUBCONNECTOR_SCART     6

#define DRM_MODE_PROP_PENDING   (1 << 0)
#define DRM_MODE_PROP_RANGE     (1 << 1)
#define DRM_MODE_PROP_IMMUTABLE (1 << 2)
#define DRM_MODE_PROP_ENUM      (1 << 3)
#define DRM_MODE_PROP_BLOB      (1 << 4)
#define DRM_MODE_PROP_BITMASK   (1 << 5)
#define DRM_MODE_PROP_LEGACY_TYPE (1 << 6)
#define DRM_MODE_PROP_EXTENDED_TYPE 0x0000ffc0
#define DRM_MODE_PROP_OBJECT    (1 << 6)
#define DRM_MODE_PROP_SIGNED_RANGE (1 << 7)

#define DRM_MODE_PROP_ATOMIC    0x80000000

#define DRM_MODE_ENCODER_NONE   0
#define DRM_MODE_ENCODER_DAC    1
#define DRM_MODE_ENCODER_TMDS   2
#define DRM_MODE_ENCODER_LVDS   3
#define DRM_MODE_ENCODER_TVDAC  4
#define DRM_MODE_ENCODER_VIRTUAL 5
#define DRM_MODE_ENCODER_DSI    6
#define DRM_MODE_ENCODER_DPMST  7
#define DRM_MODE_ENCODER_DPI    8

#define DRM_MODE_CONNECTOR_Unknown      0
#define DRM_MODE_CONNECTOR_VGA          1
#define DRM_MODE_CONNECTOR_DVII         2
#define DRM_MODE_CONNECTOR_DVID         3
#define DRM_MODE_CONNECTOR_DVIA         4
#define DRM_MODE_CONNECTOR_Composite    5
#define DRM_MODE_CONNECTOR_SVIDEO       6
#define DRM_MODE_CONNECTOR_LVDS         7
#define DRM_MODE_CONNECTOR_Component    8
#define DRM_MODE_CONNECTOR_9PinDIN      9
#define DRM_MODE_CONNECTOR_DisplayPort  10
#define DRM_MODE_CONNECTOR_HDMIA        11
#define DRM_MODE_CONNECTOR_HDMIB        12
#define DRM_MODE_CONNECTOR_TV           13
#define DRM_MODE_CONNECTOR_eDP          14
#define DRM_MODE_CONNECTOR_VIRTUAL      15
#define DRM_MODE_CONNECTOR_DSI          16
#define DRM_MODE_CONNECTOR_DPI          17
#define DRM_MODE_CONNECTOR_WRITEBACK    18
#define DRM_MODE_CONNECTOR_SPI          19

#define DRM_MODE_FLAG_PHSYNC            (1 << 0)
#define DRM_MODE_FLAG_NHSYNC            (1 << 1)
#define DRM_MODE_FLAG_PVSYNC            (1 << 2)
#define DRM_MODE_FLAG_NVSYNC            (1 << 3)
#define DRM_MODE_FLAG_INTERLACE         (1 << 4)
#define DRM_MODE_FLAG_DBLSCAN           (1 << 5)
#define DRM_MODE_FLAG_CSYNC             (1 << 6)
#define DRM_MODE_FLAG_PCSYNC            (1 << 7)
#define DRM_MODE_FLAG_NCSYNC            (1 << 8)
#define DRM_MODE_FLAG_HSKEW             (1 << 9)
#define DRM_MODE_FLAG_BCAST             (1 << 10)
#define DRM_MODE_FLAG_PIXMUX            (1 << 11)
#define DRM_MODE_FLAG_DBLCLK            (1 << 12)
#define DRM_MODE_FLAG_CLKDIV2           (1 << 13)
#define DRM_MODE_FLAG_3D_MASK           (0x1f << 14)
#define DRM_MODE_FLAG_3D_NONE           (0 << 14)
#define DRM_MODE_FLAG_3D_FRAME_PACKING  (1 << 14)
#define DRM_MODE_FLAG_3D_FIELD_ALTERNATIVE (2 << 14)
#define DRM_MODE_FLAG_3D_LINE_ALTERNATIVE (3 << 14)
#define DRM_MODE_FLAG_3D_SIDE_BY_SIDE_FULL (4 << 14)
#define DRM_MODE_FLAG_3D_L_DEPTH        (5 << 14)
#define DRM_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH (6 << 14)
#define DRM_MODE_FLAG_3D_TOP_AND_BOTTOM (7 << 14)
#define DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF (8 << 14)

#define DRM_MODE_TYPE_BUILTIN           (1 << 0)
#define DRM_MODE_TYPE_CLOCK_C           ((1 << 1) | DRM_MODE_TYPE_BUILTIN)
#define DRM_MODE_TYPE_CRTC_C            ((1 << 2) | DRM_MODE_TYPE_BUILTIN)
#define DRM_MODE_TYPE_PREFERRED         (1 << 3)
#define DRM_MODE_TYPE_DEFAULT           (1 << 4)
#define DRM_MODE_TYPE_USERDEF           (1 << 5)
#define DRM_MODE_TYPE_DRIVER            (1 << 6)
#define DRM_MODE_TYPE_ALL               (DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_USERDEF | DRM_MODE_TYPE_DRIVER)

// Event types
#define DRM_EVENT_VBLANK                0x01
#define DRM_EVENT_FLIP_COMPLETE         0x02
#define DRM_EVENT_CRTC_SEQUENCE         0x03

// FourCC formats (subset yang umum digunakan)
#define DRM_FORMAT_XRGB8888             fourcc_code('X', 'R', '2', '4')
#define DRM_FORMAT_ARGB8888             fourcc_code('A', 'R', '2', '4')
#define DRM_FORMAT_RGB888               fourcc_code('R', 'G', '2', '4')
#define DRM_FORMAT_BGR888               fourcc_code('B', 'G', '2', '4')
#define DRM_FORMAT_XBGR8888             fourcc_code('X', 'B', '2', '4')
#define DRM_FORMAT_ABGR8888             fourcc_code('A', 'B', '2', '4')
#define DRM_FORMAT_RGB565               fourcc_code('R', 'G', '1', '6')
#define DRM_FORMAT_BGR565               fourcc_code('B', 'G', '1', '6')

#define fourcc_code(a, b, c, d) ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

namespace drm {

// Forward declarations
struct drm_device;
struct drm_file;
struct drm_gem_object;

// DRM Version
struct [[gnu::packed]] drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    usize name_len;
    char *name;
    usize date_len;
    char *date;
    usize desc_len;
    char *desc;
};

// DRM Unique
struct [[gnu::packed]] drm_unique {
    usize unique_len;
    char *unique;
};

// DRM Auth
struct [[gnu::packed]] drm_auth {
    u32 magic;
};

// DRM Map
struct [[gnu::packed]] drm_map {
    u64 offset;
    u64 size;
    u32 type;
    u32 flags;
    void *handle;
    int mtrr;
};

// DRM Client
struct [[gnu::packed]] drm_client {
    int idx;
    int auth;
    u32 pid;
    u32 uid;
    u32 magic;
    u64 iocs;
};

// DRM Stats
struct [[gnu::packed]] drm_stats {
    u32 count;
    struct {
        u32 value;
        char name[15];
    } data[15];
};

// DRM Set Version
struct [[gnu::packed]] drm_set_version {
    int drm_di_major;
    int drm_di_minor;
    int drm_dd_major;
    int drm_dd_minor;
};

// DRM Modeset Ctl
struct [[gnu::packed]] drm_modeset_ctl {
    u32 crtc;
    u32 cmd;
};

// DRM GEM Close
struct [[gnu::packed]] drm_gem_close {
    u32 handle;
    u32 pad;
};

// DRM GEM Flink
struct [[gnu::packed]] drm_gem_flink {
    u32 handle;
    u32 name;
};

// DRM GEM Open
struct [[gnu::packed]] drm_gem_open {
    u32 name;
    u32 handle;
    u64 size;
};

// DRM Get Cap
struct [[gnu::packed]] drm_get_cap {
    u64 capability;
    u64 value;
};

// DRM Set Client Cap
struct [[gnu::packed]] drm_set_client_cap {
    u64 capability;
    u64 value;
};

// DRM Mode Info (mode timings)
struct [[gnu::packed]] drm_mode_modeinfo {
    u32 clock;
    u16 hdisplay;
    u16 hsync_start;
    u16 hsync_end;
    u16 htotal;
    u16 vdisplay;
    u16 vsync_start;
    u16 vsync_end;
    u16 vtotal;
    u16 flags;
    u16 type;
    char name[32];
};

// DRM Mode Card Resources
struct [[gnu::packed]] drm_mode_card_res {
    u64 fb_id_ptr;
    u64 crtc_id_ptr;
    u64 connector_id_ptr;
    u64 encoder_id_ptr;
    u32 count_fbs;
    u32 count_crtcs;
    u32 count_connectors;
    u32 count_encoders;
    u32 min_width;
    u32 max_width;
    u32 min_height;
    u32 max_height;
};

// DRM Mode CRTC
struct [[gnu::packed]] drm_mode_crtc {
    u64 set_connectors_ptr;
    u32 count_connectors;
    u32 crtc_id;
    u32 fb_id;
    u32 x;
    u32 y;
    u32 gamma_size;
    u32 mode_valid;
    struct drm_mode_modeinfo mode;
};

// DRM Mode Cursor
struct [[gnu::packed]] drm_mode_cursor {
    u32 flags;
    u32 crtc_id;
    i32 x;
    i32 y;
    u32 width;
    u32 height;
    u32 handle;
};

// DRM Mode Cursor2
struct [[gnu::packed]] drm_mode_cursor2 {
    u32 flags;
    u32 crtc_id;
    i32 x;
    i32 y;
    u32 width;
    u32 height;
    u32 handle;
    i32 hot_x;
    i32 hot_y;
};

// DRM Mode CRTC LUT
struct [[gnu::packed]] drm_mode_crtc_lut {
    u32 crtc_id;
    u32 gamma_size;
    u64 red;
    u64 green;
    u64 blue;
};

// DRM Mode Get Encoder
struct [[gnu::packed]] drm_mode_get_encoder {
    u32 encoder_id;
    u32 encoder_type;
    u32 crtc_id;
    u32 possible_crtcs;
    u32 possible_clones;
};

// DRM Mode Get Connector
struct [[gnu::packed]] drm_mode_get_connector {
    u64 encoders_ptr;
    u64 modes_ptr;
    u64 props_ptr;
    u64 prop_values_ptr;
    u32 count_modes;
    u32 count_props;
    u32 count_encoders;
    u32 encoder_id;
    u32 connector_id;
    u32 connection;
    u32 mm_width;
    u32 mm_height;
    u32 subconnector;
    u32 pad;
};

// DRM Mode Mode Cmd
struct [[gnu::packed]] drm_mode_mode_cmd {
    u32 connector_id;
    struct drm_mode_modeinfo mode;
};

// DRM Mode Get Property
struct [[gnu::packed]] drm_mode_get_property {
    u64 values_ptr;
    u64 enum_blob_ptr;
    u32 prop_id;
    u32 flags;
    char name[32];
    u32 count_values;
    u32 count_enum_blobs;
};

// DRM Mode Connector Set Property
struct [[gnu::packed]] drm_mode_connector_set_property {
    u64 value;
    u32 prop_id;
    u32 connector_id;
};

// DRM Mode Get Blob
struct [[gnu::packed]] drm_mode_get_blob {
    u32 blob_id;
    u32 length;
    u64 data;
};

// DRM Mode FB Cmd
struct [[gnu::packed]] drm_mode_fb_cmd {
    u32 fb_id;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u32 depth;
    u32 handle;
};

// DRM Mode FB Cmd2
struct [[gnu::packed]] drm_mode_fb_cmd2 {
    u32 fb_id;
    u32 width;
    u32 height;
    u32 pixel_format;
    u32 flags;
    u32 handles[4];
    u32 pitches[4];
    u32 offsets[4];
    u64 modifier[4];
};

// DRM Mode FB Dirty Cmd
struct [[gnu::packed]] drm_mode_fb_dirty_cmd {
    u32 fb_id;
    u32 flags;
    u32 color;
    u32 num_clips;
    u64 clips_ptr;
};

// DRM Mode CRTC Page Flip
struct [[gnu::packed]] drm_mode_crtc_page_flip {
    u32 crtc_id;
    u32 fb_id;
    u32 flags;
    u32 reserved;
    u64 user_data;
};

// DRM Mode Create Dumb
struct [[gnu::packed]] drm_mode_create_dumb {
    u32 height;
    u32 width;
    u32 bpp;
    u32 flags;
    u32 handle;
    u32 pitch;
    u64 size;
};

// DRM Mode Map Dumb
struct [[gnu::packed]] drm_mode_map_dumb {
    u32 handle;
    u32 pad;
    u64 offset;
};

// DRM Mode Destroy Dumb
struct [[gnu::packed]] drm_mode_destroy_dumb {
    u32 handle;
};

// DRM Mode Get Plane Resources
struct [[gnu::packed]] drm_mode_get_plane_res {
    u64 plane_id_ptr;
    u32 count_planes;
};

// DRM Mode Get Plane
struct [[gnu::packed]] drm_mode_get_plane {
    u32 plane_id;
    u32 crtc_id;
    u32 fb_id;
    u32 possible_crtcs;
    u32 gamma_size;
    u32 count_format_types;
    u64 format_type_ptr;
};

// DRM Mode Set Plane
struct [[gnu::packed]] drm_mode_set_plane {
    u32 plane_id;
    u32 crtc_id;
    u32 fb_id;
    u32 flags;
    i32 crtc_x;
    i32 crtc_y;
    u32 crtc_w;
    u32 crtc_h;
    u32 src_x;
    u32 src_y;
    u32 src_h;
    u32 src_w;
};

// DRM Mode Obj Get Properties
struct [[gnu::packed]] drm_mode_obj_get_properties {
    u64 props_ptr;
    u64 prop_values_ptr;
    u32 count_props;
    u32 obj_id;
    u32 obj_type;
};

// DRM Mode Obj Set Property
struct [[gnu::packed]] drm_mode_obj_set_property {
    u64 value;
    u32 prop_id;
    u32 obj_id;
    u32 obj_type;
};

// DRM Mode Atomic
struct [[gnu::packed]] drm_mode_atomic {
    u32 flags;
    u32 count_objs;
    u64 objs_ptr;
    u64 count_props_ptr;
    u64 props_ptr;
    u64 prop_values_ptr;
    u64 reserved;
    u64 user_data;
};

// DRM Event
struct [[gnu::packed]] drm_event {
    u32 type;
    u32 length;
};

// DRM Event VBlank
struct [[gnu::packed]] drm_event_vblank {
    struct drm_event base;
    u64 user_data;
    u32 tv_sec;
    u32 tv_usec;
    u32 sequence;
    u32 crtc_id;
};

// DRM Event CRTC Sequence
struct [[gnu::packed]] drm_event_crtc_sequence {
    struct drm_event base;
    u64 user_data;
    u32 time_ns;
    u32 sequence;
};

// IRQ Bus ID
struct [[gnu::packed]] drm_irq_busid {
    u32 irq;
    u32 busnum;
    u32 devnum;
    u32 funcnum;
};

} // namespace drm

#endif // _FISHIX_DRM_H
