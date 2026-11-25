// RGB Bar Control v1 — PS4 homebrew (OpenOrbis)
// Sets each connected DualShock 4 light bar to a preset colour, then
// runs DISCO MODE: a rainbow hue cycle, with each pad offset so the
// pads are never all the same colour at once.
//
// LIMIT: the colour only lasts while this app is open and in the
// foreground. Quitting, disconnecting, or launching a game lets the
// system / game take the light bar back. There is no software way to
// change the PS4 console's own light strip — it is not RGB.
// v2 (later): on-screen menu with R/G/B sliders per pad, presets,
//   rainbow speed control. v3 (later): GoldHEN plugin (.prx) so the
//   colours keep working in games — harder/riskier, not in v1.

#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <cstdio>

// Preset colours, one per pad (r, g, b, a)
static const OrbisPadColor kPresets[4] = {
    {0, 0, 255, 255},      // Pad 1: blue
    {255, 0, 0, 255},      // Pad 2: red
    {0, 255, 0, 255},      // Pad 3: green
    {255, 0, 255, 255},    // Pad 4: magenta
};
static const char *kPresetNames[4] = {"blue", "red", "green", "magenta"};

// Simple HSV (full saturation/value) -> RGB, hue in degrees 0-359.
static OrbisPadColor HueToRgb(int hue)
{
    hue %= 360;
    if (hue < 0)
        hue += 360;
    int sector = hue / 60;
    int frac = (hue % 60) * 255 / 60; // 0-255 within the sector
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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("RGB Bar Control v1 starting...\n");

    if (scePadInit() != 0)
    {
        printf("ERROR: scePadInit failed\n");
        for (;;)
            sceKernelUsleep(1000000);
    }

    // Open pads 1-4 (user IDs 0-3). A pad that is not connected
    // returns a negative handle and is simply skipped.
    int32_t handles[4] = {-1, -1, -1, -1};
    for (int i = 0; i < 4; i++)
    {
        handles[i] = scePadOpen(i, ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
        if (handles[i] >= 0)
            printf("Pad %d opened, preset %s\n", i + 1, kPresetNames[i]);
        else
            printf("Pad %d not connected, skipping\n", i + 1);
    }

    // Starting mode: preset colours, held for 3 seconds.
    for (int i = 0; i < 4; i++)
    {
        if (handles[i] < 0)
            continue;
        OrbisPadColor colour = kPresets[i];
        scePadSetLightBar(handles[i], &colour);
    }
    printf("Presets applied, disco mode starting in 3 seconds...\n");
    sceKernelUsleep(3 * 1000000);

    // DISCO MODE: rainbow hue cycle. Colour changes every 0.5s,
    // hue steps 10 degrees per tick (full cycle ~18s). Each pad is
    // offset by 90 degrees so they are never the same colour.
    printf("Disco mode running...\n");
    int hue = 0;
    for (;;)
    {
        for (int i = 0; i < 4; i++)
        {
            if (handles[i] < 0)
                continue;
            OrbisPadColor colour = HueToRgb(hue + i * 90);
            scePadSetLightBar(handles[i], &colour);
        }
        hue = (hue + 10) % 360;
        sceKernelUsleep(500000);
    }

    return 0;
}
