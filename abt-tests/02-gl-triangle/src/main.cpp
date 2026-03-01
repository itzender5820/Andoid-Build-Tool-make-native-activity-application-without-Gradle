#include "EGLHelper.h"
#include "Renderer.h"
#include <android_native_app_glue.h>
#include <time.h>

struct GlApp {
    gl::EGLHelper egl;
    gl::Renderer  renderer;
    bool          hasFocus = false;
    bool          ready    = false;
};

static double nowSec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void onAppCmd(android_app* app, int32_t cmd) {
    auto* ctx = static_cast<GlApp*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                if (!ctx->egl.isReady()) {
                    if (ctx->egl.init(app->window)) {
                        ctx->renderer.init(ctx->egl.width(), ctx->egl.height());
                        ctx->ready = true;
                    }
                } else {
                    ctx->egl.recreateSurface(app->window);
                    ctx->renderer.resize(ctx->egl.width(), ctx->egl.height());
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            ctx->egl.destroySurface();
            ctx->ready = false;
            break;
        case APP_CMD_GAINED_FOCUS: ctx->hasFocus = true;  break;
        case APP_CMD_LOST_FOCUS:   ctx->hasFocus = false; break;
        case APP_CMD_WINDOW_RESIZED:
            ctx->renderer.resize(ctx->egl.width(), ctx->egl.height());
            break;
        default: break;
    }
}

static int32_t onInput(android_app*, AInputEvent* ev) {
    // Minimal: consume all motion events
    return AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION ? 1 : 0;
}

extern "C" void android_main(android_app* app) {
    GlApp ctx;
    app->userData    = &ctx;
    app->onAppCmd    = onAppCmd;
    app->onInputEvent= onInput;

    double startSec = nowSec();
    GLOGI("gl-triangle starting");

    while (!app->destroyRequested) {
        int evs;
        android_poll_source* src;
        int timeout = (ctx.ready && ctx.hasFocus) ? 0 : -1;

        while (ALooper_pollOnce(timeout, nullptr, &evs,
                                reinterpret_cast<void**>(&src)) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
        }
        if (app->destroyRequested) break;

        if (ctx.ready && ctx.hasFocus) {
            double elapsed = nowSec() - startSec;
            ctx.renderer.draw(elapsed);
            if (!ctx.egl.swapBuffers()) {
                GLOGE("swapBuffers failed — context lost");
                ctx.ready = false;
            }
        }
    }

    ctx.renderer.shutdown();
    ctx.egl.shutdown();
    GLOGI("gl-triangle exiting");
}
