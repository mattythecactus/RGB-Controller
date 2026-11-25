# RGB-Controller

## Release notes — v1.00 (GitHub)

**RGB-Controller v1** — per-controller light bar colours + disco mode.
- Preset colours: Pad 1 blue, Pad 2 red, Pad 3 green, Pad 4 magenta (held 3 seconds).
- Disco mode: rainbow hue cycle, colour changes every 0.5s, each pad offset by 90 degrees.
- Colours re-apply while the app is open and in the foreground.

Planned: v2 menu + R/G/B sliders, v3 background GoldHEN plugin, v4 music out of the DualShock 4 speaker, v5 custom music selectable.

Limits: colour only lasts while the app is open/foreground; quitting or launching a game lets the system take the light bar back. Console RGB is not possible in software.


PS4 homebrew (OpenOrbis) that changes the DualShock 4 light bar colour.
Built with the OpenOrbis PS4 Toolchain v0.5.4 on Linux Mint.

- **Title ID:** RGBB00001
- **Content ID:** IV0000-RGBB00001_00-RGBBARCONTROL000
- **Build:** open the folder in VS Code and press Ctrl+Shift+B, or run `make` in a terminal with `OO_PS4_TOOLCHAIN` set and `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1`.

## How it works

- Opens pads 1–4 with `scePadOpen`, sets colours with `scePadSetLightBar(handle, OrbisPadColor*)`.
- Starting mode: preset colours — Pad 1 blue, Pad 2 red, Pad 3 green, Pad 4 magenta — held for 3 seconds.
- Then DISCO MODE: rainbow hue cycle, colour changes every 0.5s, each pad offset by 90° so they are never all the same colour at once.

## Versions

- **v1 (this build):** preset colours + Disco Mode (rainbow hue cycle). No menu yet — just run the app and the light bars cycle.
- **v2 (later):** on-screen menu with R/G/B sliders per pad, presets, rainbow cycle speed control.
- **v3 (later):** background mode — a GoldHEN plugin (.prx), not a normal app, so colours keep working in games. Harder/riskier, not attempted in v1.
- **v4 (planned, future only):** music / audio out of the controller — play sound through the DualShock 4's built-in speaker.
- **v5 (planned, future only):** custom music selectable — let the user pick their own music/audio file to play through the controller speaker (from v4), e.g. choose a track in the menu.

## Limits — read this

- The colour only lasts while this app is open and in the foreground. Quitting, disconnecting, or launching a game lets the system / game take the light bar back.
- The PS4 console itself has no RGB light — its strip is blue/white/orange for power/rest only. Console RGB is not possible in software; it needs a physical LED kit.
- Untested on real hardware — the PKG builds, but it has not been run on a PS4 yet.
