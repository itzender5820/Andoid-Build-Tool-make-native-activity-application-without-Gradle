#pragma once
#include "GLTypes.h"
#include <EGL/egl.h>
#include <android/native_window.h>

namespace gl {

// ── EGLHelper ─────────────────────────────────────────────────────────────────
// Manages EGL display, context, and surface for a NativeActivity.
// Abstracts away the boilerplate required to bring up OpenGL ES 3.0.
class EGLHelper {
public:
    EGLHelper() = default;
    ~EGLHelper();

    EGLHelper(const EGLHelper&) = delete;
    EGLHelper& operator=(const EGLHelper&) = delete;

    // Initialize EGL, choose a config, create context + surface.
    bool init(ANativeWindow* window);

    // Destroy surface only (keep context); call before window is destroyed.
    void destroySurface();

    // Recreate surface on a new window (context survives, no resource reload).
    bool recreateSurface(ANativeWindow* window);

    // Swap buffers; returns false if context was lost.
    bool swapBuffers();

    // Destroy everything.
    void shutdown();

    bool isReady()   const { return surface_ != EGL_NO_SURFACE && ctx_ != EGL_NO_CONTEXT; }
    int  width()     const { return w_; }
    int  height()    const { return h_; }

private:
    bool chooseConfig();
    bool createContext();
    bool createSurface(ANativeWindow* window);
    void querySurfaceSize();

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext ctx_     = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLConfig  config_  = nullptr;
    int        w_ = 0, h_ = 0;
};

} // namespace gl
