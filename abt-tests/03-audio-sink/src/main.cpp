#include "AudioEngine.h"
#include <android_native_app_glue.h>
#include <android/input.h>
#include <time.h>
#include <thread>
#include <chrono>

// ── Demo: play through MIDI notes touching all subsystems ─────────────────────
struct AsApp {
    as::AudioEngine engine;
    bool            ready    = false;
    bool            hasFocus = false;

    // Demo sequencer state
    double   lastNoteTimeSec = 0.0;
    int      seqStep         = 0;
    int      currentVoice    = -1;
    bool     capturing       = false;
};

static const int CHORD_SEQUENCE[][4] = {
    // C major:  C4 E4 G4 B4
    {60, 64, 67, 71},
    // F major:  F4 A4 C5 E5
    {65, 69, 72, 76},
    // G major:  G4 B4 D5 F#5
    {67, 71, 74, 78},
    // Am:       A4 C5 E5 G5
    {69, 72, 76, 79},
};
static constexpr int SEQ_LEN = 4;

static double nowSec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void tickSequencer(AsApp& app, double now) {
    if (now - app.lastNoteTimeSec < 1.2) return;  // 1.2 sec per chord
    app.lastNoteTimeSec = now;

    // Release previous chord
    app.engine.noteOffAll();

    // Fire new chord
    const int* chord = CHORD_SEQUENCE[app.seqStep % SEQ_LEN];
    static const as::WaveShape SHAPES[] = {
        as::WaveShape::SINE, as::WaveShape::TRIANGLE,
        as::WaveShape::SAWTOOTH, as::WaveShape::SINE,
    };
    for (int i = 0; i < 4; ++i) {
        float pan = (i == 0 || i == 2) ? -0.4f : 0.4f;
        app.engine.noteOn(chord[i], 0.22f, SHAPES[i % 4], pan);
    }
    ASLOGI("Chord step %d  voices=%d", app.seqStep, app.engine.activeVoices());
    app.seqStep++;

    // Tweak effects on every other chord
    if (app.seqStep % 2 == 0 && app.engine.effects()) {
        float cutoff = 4000.f + 4000.f * sinf((float)app.seqStep * 0.7f);
        app.engine.effects()->setLpfCutoff(cutoff);
    }
}

static void onAppCmd(android_app* aapp, int32_t cmd) {
    auto* app = static_cast<AsApp*>(aapp->userData);
    switch (cmd) {
        case APP_CMD_RESUME: {
            if (!app->ready) {
                std::string path = aapp->activity->internalDataPath
                    ? aapp->activity->internalDataPath : "/data/local/tmp";
                if (app->engine.init(path)) {
                    // Start capturing to file immediately
                    app->engine.startCapture(path + "/capture.wav");
                    app->capturing = true;
                    app->ready     = true;
                    app->lastNoteTimeSec = nowSec() - 1.5; // trigger first chord
                }
            }
            break;
        }
        case APP_CMD_PAUSE:
            if (app->capturing) {
                app->engine.stopCapture();
                app->capturing = false;
            }
            app->engine.noteOffAll();
            break;
        case APP_CMD_DESTROY:
            app->engine.shutdown();
            break;
        case APP_CMD_GAINED_FOCUS: app->hasFocus = true;  break;
        case APP_CMD_LOST_FOCUS:   app->hasFocus = false; break;
        default: break;
    }
}

static int32_t onInput(android_app* aapp, AInputEvent* ev) {
    auto* app = static_cast<AsApp*>(aapp->userData);
    if (!app->ready) return 0;
    // Touch → toggle LPF on / off
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION &&
        AMotionEvent_getAction(ev) == AMOTION_EVENT_ACTION_DOWN) {
        static bool lpfOn = true;
        lpfOn = !lpfOn;
        if (app->engine.effects()) app->engine.effects()->setLpfEnabled(lpfOn);
        ASLOGI("LPF %s", lpfOn ? "on" : "off");
        return 1;
    }
    return 0;
}

extern "C" void android_main(android_app* app) {
    AsApp ctx;
    app->userData    = &ctx;
    app->onAppCmd    = onAppCmd;
    app->onInputEvent= onInput;

    ASLOGI("audio-sink starting");

    while (!app->destroyRequested) {
        int evs;
        android_poll_source* src;
        int timeout = ctx.ready ? 0 : -1;

        while (ALooper_pollOnce(timeout, nullptr, &evs,
                                reinterpret_cast<void**>(&src)) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
        }
        if (app->destroyRequested) break;

        if (ctx.ready && ctx.hasFocus) {
            tickSequencer(ctx, nowSec());
            // Brief sleep to keep CPU usage sane (audio runs on its own thread)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    ctx.engine.shutdown();
    ASLOGI("audio-sink exiting");
}
