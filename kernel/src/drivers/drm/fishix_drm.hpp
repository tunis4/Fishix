// kernel/src/drivers/drm/fishix_drm.hpp
// Fishix DRM Driver header

#ifndef _FISHIX_DRM_HPP
#define _FISHIX_DRM_HPP

#include <klib/common.hpp>

namespace drm {

struct drm_device;

// Initialize Fishix DRM
int fishix_drm_init(void);

// Cleanup Fishix DRM
void fishix_drm_cleanup(void);

// Get DRM device
struct drm_device* fishix_drm_get_device(void);

} // namespace drm

// C interface
extern "C" {
    int fishix_drm_init(void);
    void fishix_drm_cleanup(void);
}

#endif // _FISHIX_DRM_HPP
