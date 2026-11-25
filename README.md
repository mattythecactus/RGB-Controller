# RGB-Controller

PS4 homebrew (OpenOrbis) that changes the DualShock 4 light bar colour.
Built with the OpenOrbis PS4 Toolchain v0.5.4 on Linux Mint.

> **Tested on Baikal PS4 Slim (CUH-2202A, firmware 9.00, GoldHEN)** — Baikal is the Slim/Pro southbridge.
> **Very early stage project** — very early setup, expect rough edges and changes.
> **Vibe-coded** — built AI-assisted (vibe coded) with Muse / OpenOrbis.

- **Title ID:** RGBB00001
- **Content ID:** IV0000-RGBB00001_00-RGBBARCONTROL000
- **Build:** open the folder in VS Code and press Ctrl+Shift+B, or run `make` in a terminal with `OO_PS4_TOOLCHAIN` set and `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1`.

## Release notes — v2.00 (v2)

**RGB-Controller v2** — on-screen menu + R/G/B sliders + disco speed control.
- On-screen menu, navigable with Pad 1: D-pad Up/Down moves between rows,
  D-pad Left/Right adjusts (hold to repeat), X confirms/advances a row,
  Triangle toggles Disco <-> Custom, Circle jumps back to Disco.
- Menu rows: Mode (PRESETS / DISCO / CUSTOM), Pad (1-4 or All), Red,
  Green, Blue (0-255 sliders per pad, with on-screen slider bars),
  Preset (blue/red/green/magenta/yellow/cyan/white/orange), Disco speed.
- R/G/B sliders are per pad — set each pad its own colour, or set Pad to
  All and adjust every pad at once. Editing a slider or applying a preset
  switches to CUSTOM mode so the colour stays.
- Disco speed 1-5 (Slow / Slow+ / Normal / Fast / Very fast). Speed 3 is
  Normal, about the same pace as v1.1; speed 1 is the slowest, speed 5
  cycles the rainbow in about 3-4 seconds.
- Kept from v1.1: presets on startup, disco mode, on-screen display with
  return codes, /data/rgb_log.txt logging, rumble pulse at startup, and
  the 512x512 rainbow icon.
- PKG built 2026-11-25, version 2.00, same filename
  IV0000-RGBB00001_00-RGBBARCONTROL000.pkg, 6.4 MB. USB was not plugged
  in, so the PKG was left ready in ~/RGBBarControl/ to copy to
  /media/matt/PS4DATA/pkg/ when Matty wakes. **Untested on real PS4 yet**
  — Matty was napping when v2 was built, so the menu/input needs a real
  PS4 test (see test notes below).

## Release notes — v1.10 (v1.1)

**RGB-Controller v1.1** — light-bar fix + on-screen display + new icon.
- Light bar fix first: v1.0 ignored return codes and hung forever if
  `scePadInit()` failed, so the light bars never changed. v1.1 logs the
  return code and tries `scePadOpen` anyway, gets the real login user
  via `sceUserServiceGetInitialUser`, polls `scePadReadState` every
  tick, records every `scePadSetLightBar` return code on screen and in
  `/data/rgb_log.txt`, and does a short rumble pulse at startup to
  prove the pad handle works.
- On-screen display (was a black screen in v1.0): background follows
  the disco hue, 4 colour bars show each pad's colour, plus mode,
  pads-open count and return codes as text.
- New 512x512 icon (`sce_sys/icon0.png`): rainbow light bar on a dark
  background with "RGB". The v1.0 icon was the OpenOrbis Hello World
  sample placeholder. Icon design source: `RGBBarControl/icon/icon_gen.py`.
- Same as v1.0 otherwise: presets (Pad 1 blue, Pad 2 red, Pad 3 green,
  Pad 4 magenta, held 3 seconds), then disco mode (rainbow hue cycle,
  colour changes every 0.5s, each pad offset by 90 degrees).

## Test log

### v1.0 test — 2026-11-25, PS4 Slim 9.00, GoldHEN
- PKG installed, title "RGB-Controller" correct.
- Screen was **black** and controller light bars did **NOT** change.
- Known v1.0 bugs:
  1. No on-screen graphics — `main.cpp` only `printf()`d and set light
     bars, so the screen was black by design.
  2. Icon was still the OpenOrbis Hello World sample placeholder
     (`sce_sys/icon0.png`).
  3. Light bar did not change — `scePadSetLightBar` did not take
     effect, and return codes were ignored in v1 so the cause was
     unknown at test time.
- v1.1 (in progress at the time of this log): fix the light bar FIRST
  (check `OrbisPadColor` layout, return codes, `scePadOpen` pattern,
  focus/privilege/read loop), then the on-screen display, then the new
  512x512 icon.
- What the v1.1 test should show: the screen is no longer black (title,
  mode, pads-open count and return codes visible), the controller
  buzzes once at startup (rumble test = pad handle works), and the
  light bars change colour — presets first, then disco. If the bars
  still do not change, the on-screen return codes and
  `/data/rgb_log.txt` say exactly which call failed.

### v2.0 test — needs Matty on real PS4 (not tested yet, built while he napped)
- Install the v2 PKG from /media/matt/PS4DATA/pkg/ (copy it there first — USB was unplugged).
- Check the menu shows: Mode / Pad / Red / Green / Blue / Preset / Disco speed.
- D-pad Up/Down moves the > cursor; Left/Right changes the value; hold Left/Right to repeat.
- Set Pad to 1, move Red to 255, Green/Blue to 0 — Pad 1 light bar should go red and stay (CUSTOM mode).
- Try a preset, try Disco speed 1 vs 5 — rainbow should be clearly slower/faster.
- Triangle should toggle Disco <-> Custom; Circle should jump back to Disco.
- If a button does nothing, check /data/rgb_log.txt — menu changes are logged there.

## How it works

- Gets the real login user with `sceUserServiceGetInitialUser`, opens
  pads with `scePadOpen`, sets colours with
  `scePadSetLightBar(handle, OrbisPadColor*)`.
- Polls `scePadReadState` every tick to keep the handle alive.
- Starting mode: preset colours — Pad 1 blue, Pad 2 red, Pad 3 green,
  Pad 4 magenta — held for 3 seconds.
- Then DISCO MODE: rainbow hue cycle, colour changes every 0.5s, each
  pad offset by 90° so they are never all the same colour at once.
- On-screen display uses the OpenOrbis `Scene2D` + FreeType font
  sample pattern (`graphics.cpp` / `graphics.h` copied from the
  toolchain `_common` folder; font `Gontserrat-Regular.ttf` in
  `assets/fonts/`).

## Versions

- **v1.0:** preset colours + Disco Mode (rainbow hue cycle). No menu
  yet — just run the app and the light bars cycle. Tested 2026-11-25:
  black screen, light bars did not change (see test log above).
- **v1.1:** light-bar fix, on-screen display, new icon. Tested 2026-11-25: works.
- **v2 (this build):** on-screen menu with R/G/B sliders per pad, presets,
  disco speed control. Untested on real PS4 yet — Matty was napping.
- **v3 (next, planned):** music / audio out of the controller
  — play sound through the DualShock 4's built-in speaker
  (tinny mono).
- **v4 (planned, future only):** custom music selectable — let the user
  pick their own music/audio file to play through the controller
  speaker (from v3), e.g. choose a track in the menu.
- **v5 (planned, last):** background mode — a GoldHEN plugin (.prx),
  not a normal app, so colours keep working in games.
  Harder/riskier.

> Roadmap reordered 2026-11-25: music first (v3 speaker music, v4
> custom music), background GoldHEN plugin last (v5).

## Limits — read this

- The colour only lasts while this app is open and in the foreground.
  Quitting, disconnecting, or launching a game lets the system / game
  take the light bar back.
- The PS4 console itself has no RGB light — its strip is blue/white/orange
  for power/rest only. Console RGB is not possible in software; it needs
  a physical LED kit.
