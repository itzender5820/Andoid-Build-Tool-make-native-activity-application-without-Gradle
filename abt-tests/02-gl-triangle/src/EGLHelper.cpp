#include "EGLHelper.h"

namespace gl {

EGLHelper::~EGLHelper() { shutdown(); }

bool EGLHelper::init(ANativeWindow* window) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) { GLOGE("eglGetDisplay failed"); return false; }

    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) {
        GLOGE("eglInitialize failed"); return false;
    }
    GLOGI("EGL %d.%d", major, minor);

    if (!chooseConfig())       return false;
    if (!createContext())      return false;
    if (!createSurface(window)) return false;

    if (!eglMakeCurrent(display_, surface_, surface_, ctx_)) {
        GLOGE("eglMakeCurrent failed: %x", eglGetError()); return false;
    }

    querySurfaceSize();

    // Enable vsync
    eglSwapInterval(display_, 1);

    GLOGI("EGL ready: surface %dx%d  GL=%s", w_, h_,
          glGetString(GL_VERSION));
    return true;
}

bool EGLHelper::chooseConfig() {
    const EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_NONE
    };
    EGLint count = 0;
    if (!eglChooseConfig(display_, attrs, &config_, 1, &count) || count < 1) {
        GLOGE("eglChooseConfig failed"); return false;
    }
    return true;
}

bool EGLHelper::createContext() {
    const EGLint ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    ctx_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, ctxAttrs);
    if (ctx_ == EGL_NO_CONTEXT) { GLOGE("eglCreateContext failed"); return false; }
    return true;
}

bool EGLHelper::createSurface(ANativeWindow* window) {
    surface_ = eglCreateWindowSurface(display_, config_, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        GLOGE("eglCreateWindowSurface failed: %x", eglGetError()); return false;
    }
    return true;
}

void EGLHelper::querySurfaceSize() {
    eglQuerySurface(display_, surface_, EGL_WIDTH,  &w_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &h_);
}

void EGLHelper::destroySurface() {
    if (surface_ != EGL_NO_SURFACE) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }
}

bool EGLHelper::recreateSurface(ANativeWindow* window) {
    destroySurface();
    if (!createSurface(window)) return false;
    if (!eglMakeCurrent(display_, surface_, surface_, ctx_)) return false;
    querySurfaceSize();
    return true;
}

bool EGLHelper::swapBuffers() {
    if (!eglSwapBuffers(display_, surface_)) {
        EGLint err = eglGetError();
        if (err == EGL_BAD_SURFACE || err == EGL_CONTEXT_LOST) return false;
    }
    return true;
}

void EGLHelper::shutdown() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        if (ctx_     != EGL_NO_CONTEXT) eglDestroyContext(display_, ctx_);
        eglTerminate(display_);
    }
    display_ = EGL_NO_DISPLAY;
    ctx_     = EGL_NO_CONTEXT;
    surface_ = EGL_NO_SURFACE;
}

} // namespace gl
