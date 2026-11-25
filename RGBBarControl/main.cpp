// RGB-Controller v1.03.1 — PS4 homebrew (OpenOrbis)
//
// v1.03.1 fixes 1.03 audio (tested 2026-11-25: skippy on TV, silent on pad):
//   Audio now runs on its OWN thread (std::thread, as in the toolchain
//   threading sample), feeding exactly one 256-sample grain per iteration
//   with the audio-wav sample pattern Output(h, NULL) then Output(h, grain).
//   In 1.03 the MAIN thread fed 19 grains (~101 ms) then did a full 1080p
//   Scene2D render + flip (tens of ms) before feeding again, so every cycle
//   had a silence gap and the TV stuttered; the pad port, with its tiny
//   buffer, starved completely. The audio thread is paced only by the audio
//   hardware, so graphics/input can never starve it. Mode changes are a
//   request flag the audio thread applies itself; handles are owned only
//   by the audio thread. Pad open now tries the login user first, then the
//   SYSTEM user, and logs BOTH return codes.
//
// v1.03 adds music out of the DualShock 4 controller speaker (roadmap v3):
//   Default track (LOOPED): "Sony's Playstation 4 Start-up Tune (TXZ1'S EPIC
//   REMIX) and also _ eljon9's Theme Song" — bundled as
//   assets/audio/ps4_startup.mp3, pre-decoded at BUILD time with ffmpeg to
//   raw 48kHz mono 16-bit PCM (assets/audio/ps4_startup.pcm), which is what
//   the app actually plays. There is no usable in-app MP3 decoder in this
//   toolchain: libSceAudiodec exists (ORBIS_AUDIO_DEC_CODEC_MP3 = 0x0002)
//   but its control structs are undocumented "unk" fields in the OpenOrbis
//   headers, so decoding MP3 on the console was not attempted.
//   New menu row "Audio": Pad speaker / TV / Both / Off. Default: Pad speaker.
//   Audio is fed in 256-sample grains via sceAudioOutOutput (MAIN port for
//   TV, PADSPK port for the pad speaker); Both uses sceAudioOutOutputs.
//   HONEST LIMITS (see README): the PADSPK port value (3) is NOT defined in
//   the OpenOrbis headers — only MAIN is. It comes from the official PS4 SDK
//   port order (MAIN=0, BGM=1, VOICE=2, PADSPK=3) and is UNVERIFIED on real
//   hardware. If the pad-speaker open fails, the return code is logged and
//   shown on screen and the app keeps running; TV is the proven fallback
//   (same sceAudioOutOpen pattern as the toolchain's audio-wav sample).
//   "Both" relies on sceAudioOutOutputs, also untested on real hardware:
//   if it fails, the app falls back to Pad speaker only and logs it.
// Menu rows (8): Mode, Pad, Red, Green, Blue, Preset, Disco Speed, Audio.
// R/G/B sliders are per pad (0-255 each); Pad can be 1-4 or All.
// Presets apply to the selected pad (or All) and switch to Custom mode.
// Disco speed 1-5 controls the hue-cycle speed.
// v1.1 behaviour kept: scePadInit/scePadOpen with return codes logged,
// real login user, scePadReadState polled every tick, return codes on
// screen + /data/rgb_log.txt, rumble pulse at startup, Scene2D display.
// LIMIT: colour AND music only last while this app is open and in the
// foreground. Quitting the app stops both.

#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/VideoOut.h>
#include <orbis/AudioOut.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <atomic>

#include "graphics.h"
#include "log.h"

std::stringstream debugLogStream;

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define FRAME_DEPTH 4

static const OrbisPadColor kPresets[8] = {
    {0, 0, 255, 255},      // blue
    {255, 0, 0, 255},      // red
    {0, 255, 0, 255},      // green
    {255, 0, 255, 255},    // magenta
    {255, 255, 0, 255},    // yellow
    {0, 255, 255, 255},    // cyan
    {255, 255, 255, 255},  // white
    {255, 128, 0, 255},    // orange
};
static const char *kPresetNames[8] = {"blue", "red", "green", "magenta",
                                      "yellow", "cyan", "white", "orange"};
static const char *kModeNames[3] = {"PRESETS", "DISCO", "CUSTOM"};
static const char *kSpeedNames[6] = {"", "Slow", "Slow+", "Normal", "Fast", "Very fast"};
static const char *kAudioNames[4] = {"Pad speaker", "TV", "Both", "Off"};

static OrbisPadColor HueToRgb(int hue)
{
    hue %= 360;
    if (hue < 0)
        hue += 360;
    int sector = hue / 60;
    int frac = (hue % 60) * 255 / 60;
    OrbisPadColor c = {0, 0, 0, 255};
    switch (sector)
    {
    case 0: c.r = 255; c.g = (uint8_t)frac; c.b = 0; break;
    case 1: c.r = (uint8_t)(255 - frac); c.g = 255; c.b = 0; break;
    case 2: c.r = 0; c.g = 255; c.b = (uint8_t)frac; break;
    case 3: c.r = 0; c.g = (uint8_t)(255 - frac); c.b = 255; break;
    case 4: c.r = (uint8_t)frac; c.g = 0; c.b = 255; break;
    default: c.r = 255; c.g = 0; c.b = (uint8_t)(255 - frac); break;
    }
    return c;
}

static int Clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static FILE *g_log = nullptr;
static void LogLine(const char *line)
{
    printf("%s\n", line);
    if (g_log)
    {
        fprintf(g_log, "%s\n", line);
        fflush(g_log);
    }
}

// ---- Audio (v1.03.1: dedicated audio thread) ----
// PADSPK port: NOT defined in OpenOrbis headers (only MAIN = 0 is).
// Value 3 is the pad-speaker port in the official PS4 SDK order
// (MAIN=0, BGM=1, VOICE=2, PADSPK=3). Still UNVERIFIED on real hardware:
// the open return codes are logged + shown on screen, and if the login
// user fails, the SYSTEM user is tried as a fallback and both rcs logged.
#define ORBIS_AUDIO_OUT_PORT_TYPE_PADSPK ((OrbisAudioOutPort)3)

static int audioMode = 0; // main-thread display copy: 0 Pad, 1 TV, 2 Both, 3 Off
static std::atomic<int> g_audioModeReq{-1}; // main -> audio thread: apply this mode
static std::atomic<bool> g_audioThreadRun{true};
static std::atomic<bool> g_audioBothFailed{false}; // audio -> main: Both fell back to Pad
static std::atomic<long> g_audioGrainsFed{0};
static int g_audioModeCurrent = -1; // audio-thread owned
static int32_t g_audioUserId = 0;   // login user, set in main before thread starts
static int32_t g_audioPad = -1;     // audio-thread owned
static int32_t g_audioTv = -1;      // audio-thread owned
static int16_t *g_pcm = nullptr;
static size_t g_pcmSamples = 0;
static size_t g_playPos = 0;        // audio-thread owned (wraps = loop)
static bool g_audioFailed = false;  // audio-thread owned
static int32_t g_audioInitRc = -999;
static int32_t g_audioPadRc = -999;
static int32_t g_audioTvRc = -999;
static char g_audioStatus[160] = "not started";
static bool g_audioFedOnce = false; // per mode, for the first-grain log line

static void AudioCloseAll()
{
    if (g_audioPad >= 0) { sceAudioOutClose(g_audioPad); g_audioPad = -1; }
    if (g_audioTv >= 0) { sceAudioOutClose(g_audioTv); g_audioTv = -1; }
}

// Audio-thread only. (Re)opens ports for `mode`. Never hangs the app:
// a failed open leaves that handle at -1 and records the return code.
static void AudioApplyMode(int mode)
{
    AudioCloseAll();
    g_audioPadRc = -999;
    g_audioTvRc = -999;
    g_audioFailed = false;
    g_audioFedOnce = false;
    g_audioModeCurrent = mode;
    if (g_audioInitRc != 0)
    {
        snprintf(g_audioStatus, sizeof(g_audioStatus), "audio lib init FAILED rc=%d", (int)g_audioInitRc);
        LogLine("Audio: not opening ports, sceAudioOutInit failed");
        return;
    }
    if (!g_pcm || g_pcmSamples == 0)
    {
        snprintf(g_audioStatus, sizeof(g_audioStatus), "No PCM loaded");
        return;
    }
    if (mode == 3) // Off
    {
        snprintf(g_audioStatus, sizeof(g_audioStatus), "Off");
        LogLine("Audio: Off");
        return;
    }
    char buf[200];
    if (mode == 0 || mode == 2)
    {
        // Pad speaker: mono 48kHz s16, grain 256. Try the login user first,
        // then SYSTEM — both return codes are logged so a real-hardware
        // test can tell us which (if either) the pad port accepts.
        g_audioPad = sceAudioOutOpen(g_audioUserId, ORBIS_AUDIO_OUT_PORT_TYPE_PADSPK, 0, 256, 48000,
                                     ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
        g_audioPadRc = g_audioPad;
        snprintf(buf, sizeof(buf), "sceAudioOutOpen PADSPK user=%d handle/rc=%d", (int)g_audioUserId, (int)g_audioPadRc);
        LogLine(buf);
        if (g_audioPad <= 0)
        {
            int32_t alt = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_PADSPK, 0, 256, 48000,
                                          ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
            snprintf(buf, sizeof(buf), "sceAudioOutOpen PADSPK SYSTEM user handle/rc=%d", (int)alt);
            LogLine(buf);
            if (alt > 0) { g_audioPad = alt; g_audioPadRc = alt; }
            else g_audioPad = -1;
        }
        if (g_audioPad <= 0) g_audioPad = -1;
    }
    if (mode == 1 || mode == 2)
    {
        // TV: stereo 48kHz s16 on MAIN, SYSTEM user — the toolchain
        // audio-wav sample pattern.
        g_audioTv = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000,
                                    ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        g_audioTvRc = g_audioTv;
        if (g_audioTv <= 0) g_audioTv = -1;
        snprintf(buf, sizeof(buf), "sceAudioOutOpen MAIN handle/rc=%d", (int)g_audioTvRc);
        LogLine(buf);
    }
    if (mode == 0)
        snprintf(g_audioStatus, sizeof(g_audioStatus), "Pad spk %s (rc=%d)", (g_audioPad >= 0) ? "OK" : "OPEN FAILED", (int)g_audioPadRc);
    else if (mode == 1)
        snprintf(g_audioStatus, sizeof(g_audioStatus), "TV %s (rc=%d)", (g_audioTv >= 0) ? "OK" : "OPEN FAILED", (int)g_audioTvRc);
    else
        snprintf(g_audioStatus, sizeof(g_audioStatus), "Both: pad %s (rc=%d) / TV %s (rc=%d)",
                 (g_audioPad >= 0) ? "OK" : "FAILED", (int)g_audioPadRc,
                 (g_audioTv >= 0) ? "OK" : "FAILED", (int)g_audioTvRc);
    snprintf(buf, sizeof(buf), "Audio mode -> %s (%s)", kAudioNames[mode], g_audioStatus);
    LogLine(buf);
}

// Audio thread: feeds ONE 256-sample grain per iteration, exactly like
// the toolchain audio-wav sample (Output(h, NULL) waits for the previous
// grain, then Output(h, grain) queues the next). The NULL-wait paces the
// thread at ~real time (~5.33 ms/grain @ 48 kHz), so graphics/input on
// the main thread can never starve the audio — that starvation is what
// made 1.03 skip on TV. Loop wraps g_playPos at the end of the track.
static void AudioThreadMain()
{
    int16_t monoGrain[256];
    int16_t stereoGrain[256 * 2];
    while (g_audioThreadRun.load())
    {
        int req = g_audioModeReq.exchange(-1);
        if (req >= 0)
            AudioApplyMode(req);
        if (!g_pcm || g_pcmSamples == 0 || g_audioFailed || (g_audioPad < 0 && g_audioTv < 0))
        {
            sceKernelUsleep(10000);
            continue;
        }
        for (int i = 0; i < 256; i++)
        {
            monoGrain[i] = g_pcm[g_playPos];
            if (++g_playPos >= g_pcmSamples) g_playPos = 0; // loop
        }
        int32_t rc = 0;
        if (g_audioPad >= 0 && g_audioTv >= 0)
        {
            for (int i = 0; i < 256; i++)
            {
                stereoGrain[i * 2] = monoGrain[i];
                stereoGrain[i * 2 + 1] = monoGrain[i];
            }
            // Wait for both ports, then feed both at once. If the NULL
            // wait is not supported by Outputs, log it and feed anyway.
            OrbisAudioOutOutputParam waitParams[2];
            waitParams[0].handle = g_audioPad; waitParams[0].pointer = nullptr;
            waitParams[1].handle = g_audioTv;  waitParams[1].pointer = nullptr;
            int32_t wrc = sceAudioOutOutputs(waitParams, 2);
            if (wrc < 0)
            {
                char buf[160];
                snprintf(buf, sizeof(buf), "WARN: sceAudioOutOutputs NULL-wait rc=%d, feeding without wait", (int)wrc);
                LogLine(buf);
            }
            OrbisAudioOutOutputParam params[2];
            params[0].handle = g_audioPad; params[0].pointer = monoGrain;
            params[1].handle = g_audioTv;  params[1].pointer = stereoGrain;
            rc = sceAudioOutOutputs(params, 2);
            if (rc < 0)
            {
                // Both failed: fall back to Pad speaker only, tell the
                // main thread to show Pad in the menu.
                LogLine("WARN: sceAudioOutOutputs (Both) failed, falling back to Pad only");
                sceAudioOutClose(g_audioTv);
                g_audioTv = -1;
                g_audioModeCurrent = 0;
                g_audioBothFailed.store(true);
                snprintf(g_audioStatus, sizeof(g_audioStatus), "Both FAILED -> Pad spk %s",
                         (g_audioPad >= 0) ? "OK" : "OPEN FAILED");
                rc = sceAudioOutOutput(g_audioPad, monoGrain);
            }
        }
        else if (g_audioPad >= 0)
        {
            sceAudioOutOutput(g_audioPad, NULL);
            rc = sceAudioOutOutput(g_audioPad, monoGrain);
        }
        else
        {
            for (int i = 0; i < 256; i++)
            {
                stereoGrain[i * 2] = monoGrain[i];
                stereoGrain[i * 2 + 1] = monoGrain[i];
            }
            sceAudioOutOutput(g_audioTv, NULL);
            rc = sceAudioOutOutput(g_audioTv, stereoGrain);
        }
        if (rc < 0)
        {
            char buf[192];
            snprintf(buf, sizeof(buf), "Audio feed FAILED rc=%d (pad=%d tv=%d) grainsFed=%ld — music stopped, menu still works",
                     (int)rc, (int)g_audioPad, (int)g_audioTv, g_audioGrainsFed.load());
            LogLine(buf);
            snprintf(g_audioStatus, sizeof(g_audioStatus), "FEED FAILED rc=%d", (int)rc);
            g_audioFailed = true;
            AudioCloseAll();
            continue;
        }
        g_audioGrainsFed.fetch_add(1);
        if (!g_audioFedOnce)
        {
            g_audioFedOnce = true;
            char buf[160];
            snprintf(buf, sizeof(buf), "Audio: first grain fed OK (%s)", g_audioStatus);
            LogLine(buf);
        }
    }
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_log = fopen("/data/rgb_log.txt", "w");
    LogLine("RGB-Controller v1.03.1 starting...");

    // ---- On-screen display (best effort: pads still work without it)
    Scene2D *scene = nullptr;
    FT_Face fontLarge, fontSmall;
    bool haveFont = false;
    bool haveScene = false;
    int frameID = 0;
    scene = new Scene2D(FRAME_WIDTH, FRAME_HEIGHT, FRAME_DEPTH);
    if (scene->Init(0xC000000, 2))
    {
        haveScene = true;
        scene->FrameBufferClear();
        if (scene->InitFont(&fontLarge, "/app0/assets/fonts/Gontserrat-Regular.ttf", 96) &&
            scene->InitFont(&fontSmall, "/app0/assets/fonts/Gontserrat-Regular.ttf", 44))
        {
            haveFont = true;
        }
        else
        {
            LogLine("WARN: font init failed, no text on screen");
        }
    }
    else
    {
        LogLine("WARN: Scene2D init failed, no on-screen display");
    }

    // ---- Pad init: log the return code, NEVER hang on it.
    int32_t initRc = scePadInit();
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "scePadInit rc=%d", (int)initRc);
        LogLine(buf);
    }

    int32_t initialUser = 0;
    {
        OrbisUserServiceInitializeParams param;
        param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        int32_t usRc = sceUserServiceInitialize(&param);
        int32_t guRc = sceUserServiceGetInitialUser(&initialUser);
        char buf[160];
        snprintf(buf, sizeof(buf), "UserService init rc=%d getInitialUser rc=%d user=%d",
                 (int)usRc, (int)guRc, (int)initialUser);
        LogLine(buf);
        if (guRc != 0)
            initialUser = 0;
    }

    // ---- Open pads: initial user first, then users 0-3 (deduped).
    int32_t handles[4] = {-1, -1, -1, -1};
    int32_t openRc[4] = {-999, -999, -999, -999};
    int32_t tryUsers[5] = {initialUser, 0, 1, 2, 3};
    int opened = 0;
    for (int t = 0; t < 5 && opened < 4; t++)
    {
        int32_t u = tryUsers[t];
        bool dup = false;
        for (int k = 0; k < t; k++)
            if (tryUsers[k] == u)
                dup = true;
        if (dup)
            continue;
        int32_t h = scePadOpen(u, ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
        char buf[160];
        snprintf(buf, sizeof(buf), "scePadOpen user=%d handle=%d", (int)u, (int)h);
        LogLine(buf);
        if (h >= 0)
        {
            handles[opened] = h;
            openRc[opened] = h;
            opened++;
        }
    }
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Pads open: %d / 4", opened);
        LogLine(buf);
    }

    // ---- Rumble pulse on the first open pad.
    if (opened > 0)
    {
        OrbisPadVibeParam vibe = {0, 160};
        int32_t vr = scePadSetVibration(handles[0], &vibe);
        sceKernelUsleep(400000);
        OrbisPadVibeParam off = {0, 0};
        scePadSetVibration(handles[0], &off);
        char buf[128];
        snprintf(buf, sizeof(buf), "Rumble test rc=%d (feel a buzz = handle OK)", (int)vr);
        LogLine(buf);
    }

    // ---- Audio (v1.03): init lib, load decoded PCM, open ports for the
    // default mode (Pad speaker). Music is best-effort: any failure here
    // only disables music, never the light bars or the menu.
    g_audioInitRc = sceAudioOutInit();
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "sceAudioOutInit rc=%d", (int)g_audioInitRc);
        LogLine(buf);
    }
    if (g_audioInitRc == 0)
    {
        FILE *af = fopen("/app0/assets/audio/ps4_startup.pcm", "rb");
        if (af)
        {
            fseek(af, 0, SEEK_END);
            long sz = ftell(af);
            fseek(af, 0, SEEK_SET);
            if (sz > 0 && (sz % 2) == 0)
            {
                g_pcm = (int16_t *)malloc((size_t)sz);
                if (g_pcm && fread(g_pcm, 1, (size_t)sz, af) == (size_t)sz)
                {
                    g_pcmSamples = (size_t)sz / 2;
                    char buf[192];
                    snprintf(buf, sizeof(buf), "PCM loaded: %zu samples = %zu:%02zu @ 48kHz mono (%ld bytes)",
                             g_pcmSamples, g_pcmSamples / 48000 / 60, (g_pcmSamples / 48000) % 60, sz);
                    LogLine(buf);
                }
                else
                {
                    LogLine("WARN: PCM malloc/read failed — music disabled");
                    if (g_pcm)
                    {
                        free(g_pcm);
                        g_pcm = nullptr;
                    }
                }
            }
            else
            {
                LogLine("WARN: PCM asset has odd/empty size — music disabled");
            }
            fclose(af);
        }
        else
        {
            LogLine("WARN: /app0/assets/audio/ps4_startup.pcm missing — music disabled");
        }
    }
    else
    {
        LogLine("WARN: sceAudioOutInit failed — music disabled");
    }
    g_audioUserId = initialUser;
    g_audioModeReq.store(0); // default: Pad speaker; audio thread opens it
    std::thread audioThread(AudioThreadMain);
    audioThread.detach();

    // ---- v1.02 state (kept)
    // mode: 0 PRESETS, 1 DISCO, 2 CUSTOM. Start on PRESETS (v1 look).
    int mode = 0;
    int hue = 0;
    int discoSpeed = 3;                 // 1..5, 3 = Normal
    int selectedPad = 0;                // 0..3 = Pad 1..4, 4 = All
    int presetIdx = 0;
    int menuRow = 0;                    // 0..7 (v1.03: Audio is row 7)
    OrbisPadColor custom[4];
    for (int i = 0; i < 4; i++)
        custom[i] = kPresets[i % 4];

    int32_t setRc[4] = {-999, -999, -999, -999};
    OrbisPadData padData;
    memset(&padData, 0, sizeof(padData));
    uint32_t prevButtons = 0;
    uint32_t heldButton = 0;            // Left or Right currently held
    int heldTicks = 0;

    // Apply a Left/Right adjustment (dir = -1 / +1) to the selected row.
    auto adjustRow = [&](int dir) {
        char buf[160];
        switch (menuRow)
        {
        case 0: // Mode
            mode = (mode + dir + 3) % 3;
            snprintf(buf, sizeof(buf), "Mode -> %s", kModeNames[mode]);
            LogLine(buf);
            break;
        case 1: // Pad
            selectedPad = (selectedPad + dir + 5) % 5;
            break;
        case 2: case 3: case 4: { // R / G / B, step 5
            int ch = menuRow - 2; // 0=R, 1=G, 2=B
            for (int i = 0; i < 4; i++)
            {
                if (selectedPad != 4 && i != selectedPad)
                    continue;
                uint8_t *v = (ch == 0) ? &custom[i].r : (ch == 1) ? &custom[i].g : &custom[i].b;
                *v = (uint8_t)Clamp255((int)*v + dir * 5);
            }
            mode = 2; // editing a slider switches to CUSTOM
            break;
        }
        case 5: // Preset: cycle and apply to selected pad(s)
            presetIdx = (presetIdx + dir + 8) % 8;
            for (int i = 0; i < 4; i++)
            {
                if (selectedPad != 4 && i != selectedPad)
                    continue;
                custom[i] = kPresets[presetIdx];
            }
            mode = 2; // preset applied -> CUSTOM so it stays
            snprintf(buf, sizeof(buf), "Preset %s applied", kPresetNames[presetIdx]);
            LogLine(buf);
            break;
        case 6: // Disco speed 1..5
            discoSpeed += dir;
            if (discoSpeed < 1) discoSpeed = 1;
            if (discoSpeed > 5) discoSpeed = 5;
            break;
        case 7: // Audio output: Pad speaker / TV / Both / Off
            audioMode = (audioMode + dir + 4) % 4;
            g_audioModeReq.store(audioMode); // audio thread applies it
            break;
        }
    };

    // X (Cross) on the selected row: advance it.
    auto confirmRow = [&]() {
        if (menuRow == 0)
            adjustRow(1);
        else if (menuRow == 1)
            adjustRow(1);
        else if (menuRow == 5)
            adjustRow(1);
        else if (menuRow == 6)
        {
            discoSpeed++;
            if (discoSpeed > 5) discoSpeed = 1;
        }
        else if (menuRow == 7)
            adjustRow(1);
        // R/G/B rows: X does nothing (use Left/Right).
    };

    // Draw one frame + set light bars for the current tick (100 ms).
    auto tick = [&](OrbisPadColor *colours) {
        for (int i = 0; i < 4; i++)
        {
            if (handles[i] < 0)
                continue;
            scePadReadState(handles[i], &padData);
            setRc[i] = scePadSetLightBar(handles[i], &colours[i]);
        }
        if (!haveScene)
            return;
        Color bg = {(uint8_t)(colours[0].r / 10), (uint8_t)(colours[0].g / 10),
                    (uint8_t)(colours[0].b / 10)};
        scene->FrameBufferFill(bg);
        // 4 colour bars, one per pad (moved down in v1.03: menu has 8 rows now).
        for (int i = 0; i < 4; i++)
        {
            Color bar = {colours[i].r, colours[i].g, colours[i].b};
            scene->DrawRectangle(120 + i * 440, 925, 400, 55, bar);
        }
        if (haveFont)
        {
            Color white = {255, 255, 255};
            Color black = {0, 0, 0};
            Color grey = {180, 180, 180};
            char line[256];
            scene->DrawText((char *)"RGB-CONTROLLER", fontLarge, 120, 180, black, white);
            scene->DrawText((char *)"v1.03.1 - Menu + RGB Sliders + Disco Speed + Music (audio thread fix)", fontSmall, 120, 280, black, white);
            snprintf(line, sizeof(line), "MODE: %s   hue %d   Disco speed: %s (%d/5)",
                     kModeNames[mode], hue, kSpeedNames[discoSpeed], discoSpeed);
            scene->DrawText(line, fontSmall, 120, 350, black, white);
            snprintf(line, sizeof(line), "Pads open: %d / 4    Init rc=%d    User=%d    Audio: %s",
                     opened, (int)initRc, (int)initialUser, g_audioStatus);
            scene->DrawText(line, fontSmall, 120, 410, black, white);

            // Menu rows, y = 470 + row*56 (v1.03: 8 rows, tighter spacing).
            const char *labels[8] = {"Mode", "Pad", "Red", "Green", "Blue", "Preset", "Disco speed", "Audio"};
            for (int r = 0; r < 8; r++)
            {
                int y = 470 + r * 56;
                const char *cursor = (r == menuRow) ? ">" : " ";
                char val[96] = "";
                Color fg = (r == menuRow) ? white : grey;
                switch (r)
                {
                case 0: snprintf(val, sizeof(val), "%s", kModeNames[mode]); break;
                case 1:
                    if (selectedPad == 4) snprintf(val, sizeof(val), "All pads");
                    else snprintf(val, sizeof(val), "Pad %d", selectedPad + 1);
                    break;
                case 2: case 3: case 4: {
                    int ch = r - 2;
                    int v = (selectedPad == 4)
                        ? (ch == 0 ? custom[0].r : ch == 1 ? custom[0].g : custom[0].b)
                        : (ch == 0 ? custom[selectedPad].r : ch == 1 ? custom[selectedPad].g : custom[selectedPad].b);
                    snprintf(val, sizeof(val), "%d", v);
                    break;
                }
                case 5: snprintf(val, sizeof(val), "%s", kPresetNames[presetIdx]); break;
                case 6: snprintf(val, sizeof(val), "%s (%d/5)", kSpeedNames[discoSpeed], discoSpeed); break;
                case 7: snprintf(val, sizeof(val), "%s", kAudioNames[audioMode]); break;
                }
                snprintf(line, sizeof(line), "%s %s: %s", cursor, labels[r], val);
                scene->DrawText(line, fontSmall, 120, y, black, fg);
                // Slider bar for R/G/B rows.
                if (r >= 2 && r <= 4)
                {
                    int ch = r - 2;
                    int v = (selectedPad == 4)
                        ? (ch == 0 ? custom[0].r : ch == 1 ? custom[0].g : custom[0].b)
                        : (ch == 0 ? custom[selectedPad].r : ch == 1 ? custom[selectedPad].g : custom[selectedPad].b);
                    int bx = 700, bw = 600, bh = 28, by = y - 28;
                    Color track = {60, 60, 60};
                    Color fill = (ch == 0) ? Color{255, 60, 60} : (ch == 1) ? Color{60, 255, 60} : Color{60, 120, 255};
                    scene->DrawRectangle(bx, by, bw, bh, track);
                    scene->DrawRectangle(bx, by, bw * v / 255, bh, fill);
                }
            }
            snprintf(line, sizeof(line), "SetLightBar rc: P1 %d  P2 %d  P3 %d  P4 %d",
                     (int)setRc[0], (int)setRc[1], (int)setRc[2], (int)setRc[3]);
            scene->DrawText(line, fontSmall, 120, 1030, black, white);
        }
        scene->SubmitFlip(frameID);
        scene->FrameWait(frameID);
        scene->FrameBufferSwap();
        frameID++;
    };

    LogLine("v1.03.1 menu running — D-pad to move, Left/Right to adjust, X to confirm");
    for (;;)
    {
        // ---- Input (Pad 1 only), edge + hold-repeat for Left/Right.
        uint32_t buttons = 0;
        if (opened > 0)
        {
            scePadReadState(handles[0], &padData);
            buttons = padData.buttons;
        }
        uint32_t pressed = buttons & ~prevButtons;
        if (pressed & ORBIS_PAD_BUTTON_UP)
        {
            menuRow = (menuRow + 7) % 8;
            heldButton = 0; heldTicks = 0;
        }
        if (pressed & ORBIS_PAD_BUTTON_DOWN)
        {
            menuRow = (menuRow + 1) % 8;
            heldButton = 0; heldTicks = 0;
        }
        if (pressed & ORBIS_PAD_BUTTON_CROSS)
            confirmRow();
        if (pressed & ORBIS_PAD_BUTTON_TRIANGLE)
        {
            mode = (mode == 1) ? 2 : 1; // toggle Disco <-> Custom
            char buf[96];
            snprintf(buf, sizeof(buf), "Triangle: Mode -> %s", kModeNames[mode]);
            LogLine(buf);
        }
        if (pressed & ORBIS_PAD_BUTTON_CIRCLE)
        {
            mode = 1; // back to Disco
            LogLine("Circle: Mode -> DISCO");
        }
        // Left/Right: edge = one step, hold >500ms = repeat every tick.
        uint32_t lr = buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT);
        if (lr != 0 && lr != heldButton)
        {
            heldButton = lr; heldTicks = 0;
            adjustRow((lr & ORBIS_PAD_BUTTON_LEFT) ? -1 : 1);
        }
        else if (lr != 0 && lr == heldButton)
        {
            heldTicks++;
            if (heldTicks >= 5) // held 500ms at 100ms/tick
                adjustRow((lr & ORBIS_PAD_BUTTON_LEFT) ? -1 : 1);
        }
        else
        {
            heldButton = 0; heldTicks = 0;
        }
        prevButtons = buttons;

        // ---- Colours for this tick.
        OrbisPadColor colours[4];
        for (int i = 0; i < 4; i++)
        {
            if (mode == 1)
                colours[i] = HueToRgb(hue + i * 90);
            else if (mode == 2)
                colours[i] = custom[i];
            else
                colours[i] = kPresets[i % 4];
        }

        // ---- Music runs on its own thread now (v1.03.1): main loop
        // never feeds audio, so rendering can never starve it.
        if (g_audioBothFailed.exchange(false))
        {
            audioMode = 0; // Both fell back to Pad only; show that in menu
            LogLine("Audio: Both failed, menu now shows Pad speaker");
        }
        tick(colours);
        if (mode == 1)
            hue = (hue + discoSpeed * 2) % 360; // 100ms tick: speed 3 = 6deg/tick
        sceKernelUsleep(100000);
    }

    return 0;
}
