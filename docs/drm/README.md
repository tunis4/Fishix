# DRM (Direct Rendering Manager) & KMS Engine

The DRM subsystem in Fishix provides a kernel-mode setting (KMS) interface for managing displays and graphics buffers, enabling modern windowing systems like Wayland.

## Components

### 1. DRM Core
The standard framework for managing DRM devices, files, and objects.
- **Header**: `kernel/src/drivers/drm/drm_internal.hpp`
- **Source**: `kernel/src/drivers/drm/drm_core.cpp`

### 2. KMS (Kernel Mode Setting)
Implements modesetting operations like:
- **Device Resources**: CRTCs, Encoders, Connectors, and Planes.
- **Mode Selection**: Detection and setting of screen resolutions.
- **Page Flipping**: Support for `DRM_IOCTL_MODE_PAGE_FLIP` to allow smooth, tear-free rendering with double/triple buffering.
- **Source**: `kernel/src/drivers/drm/drm_modeset.cpp`

### 3. GEM (Graphics Execution Manager)
Handles graphics buffer allocation and sharing.
- **Dumb Buffers**: Simple linear buffers allocated via `DRM_IOCTL_MODE_CREATE_DUMB` that are mapped to userspace for drawing.
- **Flink & Open**: Name-based buffer sharing between processes.
- **Source**: `kernel/src/drivers/drm/drm_gem.cpp`

### 4. Fishix-DRM Virtual Driver
A virtual DRM driver that wraps the standard Fishix hardware framebuffer.
- **Feature**: Allows high-level DRM-aware apps to render to the existing Fishix display through a unified API.
- **Device Node**: `/dev/dri/card0`
- **Source**: `kernel/src/drivers/drm/fishix_drm.cpp`

## VFS Integration
The DRM subsystem integrates into Fishix's VFS through a custom `DRMDevNode` that implements `open`, `ioctl`, `mmap`, and `poll` handlers.

---
*For more info about Wayland specifically, see the [Wayland Documentation](../wayland/README.md).*
