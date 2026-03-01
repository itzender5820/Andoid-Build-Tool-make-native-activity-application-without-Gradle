#include "StateManager.h"
#include "InputHandler.h"
#include <android_native_app_glue.h>
#include <android/log.h>

// ── Glue state ────────────────────────────────────────────────────────────────
struct AppCtx {
    sn::Logger           log    {"sn.main"};
    sn::Config*          cfg    = nullptr;
    sn::EventQueue*      events = nullptr;
    sn::InputHandler*    input  = nullptr;
    sn::LifecycleManager* life  = nullptr;
    sn::StateManager*    state  = nullptr;
    bool                 ready  = false;
};

static void onAppCmd(android_app* app, int32_t cmd) {
    auto* ctx = static_cast<AppCtx*>(app->userData);
    switch (cmd) {
        case APP_CMD_START:   if(ctx->life) ctx->life->onStart();   break;
        case APP_CMD_RESUME:  if(ctx->life) ctx->life->onResume();  break;
        case APP_CMD_PAUSE:   if(ctx->life) ctx->life->onPause();   break;
        case APP_CMD_STOP:    if(ctx->life) ctx->life->onStop();    break;
        case APP_CMD_DESTROY: if(ctx->life) ctx->life->onDestroy(); break;
        case APP_CMD_INIT_WINDOW:
            if (ctx->life && app->window)
                ctx->life->onWindowInit(app->window);
            break;
        case APP_CMD_TERM_WINDOW:
            if (ctx->life) ctx->life->onWindowTerm();
            break;
        case APP_CMD_WINDOW_RESIZED:
            if (ctx->life && app->window)
                ctx->life->onWindowResized(
                    ANativeWindow_getWidth(app->window),
                    ANativeWindow_getHeight(app->window));
            break;
        case APP_CMD_GAINED_FOCUS: if(ctx->life) ctx->life->onFocusGained(); break;
        case APP_CMD_LOST_FOCUS:   if(ctx->life) ctx->life->onFocusLost();   break;
        default: break;
    }
}

static int32_t onInputEvent(android_app*, AInputEvent* ev) {
    // ctx pointer not available here, accessed via global (pattern: user data on app)
    return 0; // handled below in the loop
}

extern "C" void android_main(android_app* app) {
    AppCtx ctx;
    app->userData = &ctx;
    app->onAppCmd = onAppCmd;

    // Build storage path
    std::string storagePath = app->activity->internalDataPath
        ? app->activity->internalDataPath : "/data/local/tmp";

    sn::Logger log("sn.main");
    sn::Config  cfg(storagePath, log);
    cfg.load();

    sn::EventQueue      events(log);
    sn::InputHandler    input(events, log);
    sn::LifecycleManager life(cfg, events, log);
    sn::StateManager    state(life, events, log,
                               cfg.getInt("app.fps_limit", 60));

    ctx.cfg    = &cfg;
    ctx.events = &events;
    ctx.input  = &input;
    ctx.life   = &life;
    ctx.state  = &state;
    ctx.ready  = true;

    state.start();
    log.info("simple-native ready — entering main loop");

    while (!app->destroyRequested && !state.shouldStop()) {
        // Poll ALooper events
        int events_out;
        android_poll_source* source;
        int timeout = (life.hasWindow() && life.hasFocus()) ? 0 : -1;

        while (ALooper_pollOnce(timeout, nullptr, &events_out,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) break;
        }
        if (app->destroyRequested) break;

        // Forward touch/key input
        if (AInputQueue* iq = app->inputQueue) {
            AInputEvent* ev = nullptr;
            while (AInputQueue_hasEvents(iq) > 0 &&
                   AInputQueue_getEvent(iq, &ev) >= 0) {
                if (AInputQueue_preDispatchEvent(iq, ev)) continue;
                bool consumed = input.processEvent(ev);
                AInputQueue_finishEvent(iq, ev, consumed);
            }
        }

        // Tick main logic
        if (life.hasWindow() && life.hasFocus())
            state.tick();
    }

    log.info("Exiting — %s", state.frameStats().summary().c_str());
    cfg.save();
}
