# Wayland Support in Fishix

Fishix aims to support native Wayland compositors (such as Weston, Sway, or GNOME/KDE's Wayland backends) through an implemented Linux ABI and a fully functional DRM/KMS subsystem.

## Wayland Rendering Stack

1.  **Wayland Compositor**: (e.g. `weston` with the `drm` backend selected)
2.  **DRM Subsystem**: (KMS) handles screen modes and framebuffers.
3.  **GEM & DMA**: (Dumb Buffers) provide the linear memory for compositors to draw the UI.
4.  **Hardware FB**: The actual screen display through the virtual DRM driver.

## Key Requirements for Wayland to Work

### 1. DRM Device Node (`/dev/dri/card0`)
The compositor must be able to open the DRM device to query capabilities and set modes.

### 2. KMS Capability Implementation
Fishix must support:
- `DRM_IOCTL_GET_RESOURCES`
- `DRM_IOCTL_MODE_ADDFB2`
- `DRM_IOCTL_MODE_SETCRTC`
- `DRM_IOCTL_MODE_PAGE_FLIP` (with VBlank events)

### 3. Dumb Buffer mmap
The compositor must be able to allocate linear buffers via `DRM_IOCTL_MODE_CREATE_DUMB` and `mmap` them to its own address space.

## Status in Fishix
As of now, the kernel-side API for Wayland support is **complete**:
- [x] GEM allocation and sharing (Flink/Open)
- [x] KMS Modesetting and Page flipping
- [x] Device node registration for `/dev/dri/card0`
- [x] `mmap` support for graphics buffers
- [x] VBlank event emission for frame timing

Compositors can now be tested by running them in the Fishix userspace.
---
*See the [DRM Document](../drm/README.md) for deeper kernel implementation details.*
