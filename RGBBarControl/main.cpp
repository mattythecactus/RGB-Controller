// RGB-Controller v2.00 — PS4 homebrew (OpenOrbis)
//
// v2 adds an on-screen menu, navigable with the controller:
//   D-pad Up/Down  move between menu rows
//   D-pad Left/Right  adjust the selected row (hold to repeat)
//   X (Cross)  confirm / advance the selected row
//   Triangle  toggle Disco <-> Custom
//   Circle  jump back to Disco mode
// Menu rows: Mode, Pad, Red, Green, Blue, Preset, Disco Speed.
// R/G/B sliders are per pad (0-255 each); Pad can be 1-4 or All.
// Presets apply to the selected pad (or All) and switch to Custom mode.
// Disco speed 1-5 controls the hue-cycle speed.
// v1.1 behaviour kept: scePadInit/scePadOpen with return codes logged,
// real login user, scePadReadState polled every tick, return codes on
// screen + /data/rgb_log.txt, rumble pulse at startup, Scene2D display.
// LIMIT: colour only lasts while this app is open and in the foreground.

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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_log = fopen("/data/rgb_log.txt", "w");
    LogLine("RGB-Controller v2.00 starting...");

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

    // ---- v2 state
    // mode: 0 PRESETS, 1 DISCO, 2 CUSTOM. Start on PRESETS (v1 look).
    int mode = 0;
    int hue = 0;
    int discoSpeed = 3;                 // 1..5, 3 = Normal
    int selectedPad = 0;                // 0..3 = Pad 1..4, 4 = All
    int presetIdx = 0;
    int menuRow = 0;                    // 0..6
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
        // 4 colour bars, one per pad.
        for (int i = 0; i < 4; i++)
        {
            Color bar = {colours[i].r, colours[i].g, colours[i].b};
            scene->DrawRectangle(120 + i * 440, 900, 400, 80, bar);
        }
        if (haveFont)
        {
            Color white = {255, 255, 255};
            Color black = {0, 0, 0};
            Color grey = {180, 180, 180};
            char line[256];
            scene->DrawText((char *)"RGB-CONTROLLER", fontLarge, 120, 180, black, white);
            scene->DrawText((char *)"v2.00 - Menu + RGB Sliders + Disco Speed", fontSmall, 120, 280, black, white);
            snprintf(line, sizeof(line), "MODE: %s   hue %d   Disco speed: %s (%d/5)",
                     kModeNames[mode], hue, kSpeedNames[discoSpeed], discoSpeed);
            scene->DrawText(line, fontSmall, 120, 350, black, white);
            snprintf(line, sizeof(line), "Pads open: %d / 4    Init rc=%d    User=%d",
                     opened, (int)initRc, (int)initialUser);
            scene->DrawText(line, fontSmall, 120, 410, black, white);

            // Menu rows, y = 470 + row*62.
            const char *labels[7] = {"Mode", "Pad", "Red", "Green", "Blue", "Preset", "Disco speed"};
            for (int r = 0; r < 7; r++)
            {
                int y = 470 + r * 62;
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

    LogLine("v2 menu running — D-pad to move, Left/Right to adjust, X to confirm");
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
            menuRow = (menuRow + 6) % 7;
            heldButton = 0; heldTicks = 0;
        }
        if (pressed & ORBIS_PAD_BUTTON_DOWN)
        {
            menuRow = (menuRow + 1) % 7;
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
        tick(colours);
        if (mode == 1)
            hue = (hue + discoSpeed * 2) % 360; // 100ms tick: speed 3 = 6deg/tick
        sceKernelUsleep(100000);
    }

    return 0;
}
