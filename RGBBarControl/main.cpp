// RGB-Controller v1.1 — PS4 homebrew (OpenOrbis)
//
// v1.0 TEST RESULT (2026-11-25, PS4 Slim 9.00, GoldHEN):
//   PKG installed, title correct, but screen was BLACK and the
//   controller light bars did NOT change at all.
// v1.1 fixes, in order:
//   1. LIGHT BAR: v1 ignored almost every return code and, worst of
//      all, did `if (scePadInit() != 0) { sleep forever; }` — if
//      scePadInit fails in this context (it is owned by ShellCore and
//      is known to fail from some contexts) v1 hung forever with a
//      black screen and never even tried scePadOpen. v1.1 logs the
//      return code and tries scePadOpen anyway. It also gets the real
//      login user via sceUserServiceGetInitialUser (like the OpenOrbis
//      input sample) instead of assuming user IDs 0-3, polls
//      scePadReadState every tick to keep the handle alive, and
//      records every scePadSetLightBar return code on screen and in
//      /data/rgb_log.txt. A short rumble pulse at startup proves
//      whether the pad handle itself works.
//   2. ON-SCREEN DISPLAY: Scene2D (from the OpenOrbis graphics/font
//      samples) so the screen is no longer black — background follows
//      the disco hue, 4 colour bars show each pad's colour, plus
//      mode, pads-open count and return codes as text.
//   3. NEW ICON: sce_sys/icon0.png replaced (was the Hello World
//      placeholder).
//
// LIMIT: colour only lasts while this app is open and in the
// foreground. Quitting, disconnecting, or launching a game lets the
// system / game take the light bar back. No software way to change
// the PS4 console's own light strip — it is not RGB.

#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/VideoOut.h>
#include <cstdio>
#include <cstring>

#include "graphics.h"
#include "log.h"

std::stringstream debugLogStream;

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define FRAME_DEPTH 4

static const OrbisPadColor kPresets[4] = {
    {0, 0, 255, 255},      // Pad 1: blue
    {255, 0, 0, 255},      // Pad 2: red
    {0, 255, 0, 255},      // Pad 3: green
    {255, 0, 255, 255},    // Pad 4: magenta
};
static const char *kPresetNames[4] = {"blue", "red", "green", "magenta"};

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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_log = fopen("/data/rgb_log.txt", "w");
    LogLine("RGB-Controller v1.1 starting...");

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
    // scePadInit is owned by ShellCore and is known to fail from some
    // contexts while scePadOpen still works, so try Open regardless.
    int32_t initRc = scePadInit();
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "scePadInit rc=%d", (int)initRc);
        LogLine(buf);
    }

    // ---- Real login user (like the OpenOrbis input sample). If
    // UserService fails, fall back to user 0 — never fail-hard here.
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

    // ---- Rumble pulse on the first open pad: proves the handle
    // works at all. If the controller vibrates but the light bar
    // still does not change, the problem is light-bar specific.
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

    int32_t setRc[4] = {-999, -999, -999, -999};
    OrbisPadData padData;
    int hue = 0;
    bool disco = false;
    const char *modeText = "PRESETS";

    // Draw one frame + set light bars for the current tick.
    // Returns false only if there is nothing to draw with.
    auto tick = [&](OrbisPadColor *colours) {
        for (int i = 0; i < 4; i++)
        {
            if (handles[i] < 0)
                continue;
            // Poll the pad every tick (every sample does this) to
            // keep the handle alive.
            scePadReadState(handles[i], &padData);
            setRc[i] = scePadSetLightBar(handles[i], &colours[i]);
        }
        if (!haveScene)
            return;
        // Background: dark shade of pad 1's current colour.
        Color bg = {(uint8_t)(colours[0].r / 10), (uint8_t)(colours[0].g / 10),
                    (uint8_t)(colours[0].b / 10)};
        scene->FrameBufferFill(bg);
        // 4 colour bars, one per pad.
        for (int i = 0; i < 4; i++)
        {
            Color bar = {colours[i].r, colours[i].g, colours[i].b};
            scene->DrawRectangle(120 + i * 440, 720, 400, 110, bar);
        }
        if (haveFont)
        {
            Color white = {255, 255, 255};
            Color black = {0, 0, 0};
            char line[256];
            scene->DrawText((char *)"RGB-CONTROLLER", fontLarge, 120, 240, black, white);
            scene->DrawText((char *)"v1.1 - Presets then Disco Mode", fontSmall, 120, 340, black, white);
            snprintf(line, sizeof(line), "MODE: %s   hue %d", modeText, hue);
            scene->DrawText(line, fontSmall, 120, 430, black, white);
            snprintf(line, sizeof(line), "Pads open: %d / 4    Init rc=%d    User=%d",
                     opened, (int)initRc, (int)initialUser);
            scene->DrawText(line, fontSmall, 120, 500, black, white);
            snprintf(line, sizeof(line), "SetLightBar rc: P1 %d  P2 %d  P3 %d  P4 %d",
                     (int)setRc[0], (int)setRc[1], (int)setRc[2], (int)setRc[3]);
            scene->DrawText(line, fontSmall, 120, 570, black, white);
            scene->DrawText((char *)"Colour lasts while this app is open", fontSmall, 120, 900, black, white);
        }
        scene->SubmitFlip(frameID);
        scene->FrameWait(frameID);
        scene->FrameBufferSwap();
        frameID++;
    };

    // ---- Phase 1: presets, held 3 seconds (6 x 0.5s ticks).
    {
        OrbisPadColor colours[4];
        for (int i = 0; i < 4; i++)
            colours[i] = (handles[i] >= 0) ? kPresets[i] : HueToRgb(0);
        for (int t = 0; t < 6; t++)
        {
            modeText = "PRESETS";
            tick(colours);
            sceKernelUsleep(500000);
        }
    }

    // ---- Phase 2: DISCO MODE, forever. Hue +10 per 0.5s tick,
    // pads offset 90 degrees.
    LogLine("Disco mode running...");
    for (;;)
    {
        OrbisPadColor colours[4];
        for (int i = 0; i < 4; i++)
            colours[i] = HueToRgb(hue + i * 90);
        modeText = "DISCO";
        tick(colours);
        hue = (hue + 10) % 360;
        sceKernelUsleep(500000);
    }

    return 0;
}
