# RGB-Controller

PS4 homebrew (OpenOrbis) that changes the DualShock 4 light bar colour.
Built with the OpenOrbis PS4 Toolchain v0.5.4 on Linux Mint.

> **Tested on Baikal PS4 Slim (CUH-2202A, firmware 9.00, GoldHEN)** — Baikal is the Slim/Pro southbridge.
> **Very early stage project** — very early setup, expect rough edges and changes.
> **Vibe-coded** — built AI-assisted (vibe coded) with Muse / OpenOrbis.

- **Title ID:** RGBB00001
- **Content ID:** IV0000-RGBB00001_00-RGBBARCONTROL000
- **Build:** open the folder in VS Code and press Ctrl+Shift+B, or run `make` in a terminal with `OO_PS4_TOOLCHAIN` set and `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1`.

## Release notes — 1.04

**RGB-Controller 1.04** — custom music: Track menu + FTP folder on the console (part 1).
- New menu row "Track" (row 9, bottom row): D-pad Up/Down to reach it,
  Left/Right or X cycles tracks. Switching a track loads it and loops it
  from the start. Menu rows are slightly tighter (50 px spacing) and the
  4 colour bars moved down to fit 9 rows.
- The app creates **`/data/rgb_music/` on the PS4 on startup** (mkdir,
  logged in /data/rgb_log.txt). FTP your own custom music files there
  and they show up in the Track menu — see "Custom music via FTP" below.
  The folder is rescanned when you first move onto the Track row, so
  files added since startup appear without restarting the app.
- Bundled tracks (selectable in the Track menu):
  - Track 1: PS4 Startup — "Sony's Playstation 4 Start-up Tune
    (TXZ1'S EPIC REMIX)" (4:19).
  - Track 2: Stayin' Alive — Bee Gees "Stayin' Alive (San Pacho
    Remix)" (4:23), from "Bee Gees - Stayin' Alive (San Pacho
    Remix).mp3" (6.1 MB), converted at build time to 48 kHz mono PCM
    (25,291,548 bytes).
  - Track 3: Coffin Dance — "Coffin Dance (Official Music Video HD)"
    (2:35), from "Coffin Dance (Official Music Video HD).mp3"
    (3.6 MB), converted at build time to 48 kHz mono PCM
    (14,901,640 bytes).
  FTP tracks are listed after the 3 bundled tracks, alphabetically,
  by filename (without the .pcm extension).
- Kept from 1.03.1: looped music, Audio row (Pad speaker / TV / Both /
  Off), audio thread (TV smooth), 8-row menu + sliders, presets, disco
  speed, return codes, /data/rgb_log.txt logging, rumble pulse, icon.
- PKG built 2026-11-25, version 1.04, same filename
  IV0000-RGBB00001_00-RGBBARCONTROL000.pkg, 80 MB. **TESTED 2026-11-25
  on Baikal PS4 Slim: everything works** — Track cycling 1-2-3,
  smooth TV audio + looping, /data/rgb_music/ created. Pad speaker
  still does NOT work (unchanged) — see test notes below.

## Custom music via FTP

How to put your own songs into RGB-Controller:

1. **Convert the song to PCM on the laptop.** The PS4 app cannot play
   MP3s — there is no usable MP3 decoder in the OpenOrbis toolchain —
   so convert each song first with ffmpeg (already on the laptop):
   ```
   ffmpeg -i "My Song.mp3" -ar 48000 -ac 1 -f s16le "My Song.pcm"
   ```
   That makes raw 48 kHz mono 16-bit PCM, which is exactly what the app
   plays. One ~4-minute song is about 25 MB as PCM (~6 MB per minute).
   Keep filenames short and simple — the menu shows the filename
   (without .pcm), and long names get cut off.
2. **Run RGB-Controller once on the PS4.** It creates the folder
   `/data/rgb_music/` on the console on startup. You only need to do
   this once — the folder stays there.
3. **FTP the .pcm files to the PS4.** The PS4 must be in GoldHEN mode
   (PS4 OS, not PS4Linux/CachyOS — the two never run at the same time).
   GoldHEN FTP is at **10.226.87.188:2121**, local home WiFi only, no
   password. Use AndFTP on your phone, or any FTP client on the laptop
   (e.g. FileZilla), connect to that address and port, and copy the
   .pcm files into `/data/rgb_music/`. Do NOT send MP3s — the app only
   lists `.pcm` files, and MP3s in the folder are ignored.
4. **Pick the track in the app.** Move to the Track row (bottom row) —
   the app rescans the folder when you first move onto it — then
   Left/Right or X to cycle tracks. The track loads and loops.
   The on-screen line shows "Tracks: N" (1 bundled + your FTP tracks),
   and /data/rgb_log.txt lists every track found, e.g.
   `FTP music: 3 track(s) total (1 bundled + 2 from /data/rgb_music/)`.

Limits — read this:
- **PCM only.** MP3/WAV/FLAC files in /data/rgb_music/ are ignored.
  Empty, odd-sized or over-100 MB .pcm files are skipped and logged.
- **Max 15 FTP tracks** (16 total including the bundled tune).
- **TV is the working output.** The pad speaker is difficult to get
  working and does NOT work yet (see 1.03.1 test notes) — custom tracks
  play through the TV / Both / Off settings the same as the bundled tune.
- Music only plays while the app is open and in the foreground.
- If a track does not show up: check it is a .pcm file, check the
  spelling of the folder (`/data/rgb_music/`), and check
  /data/rgb_log.txt for the `FTP music:` lines — they say exactly what
  was found and what was skipped, and why.

## Release notes — 1.03

**RGB-Controller 1.03** — music out of the DualShock 4 controller speaker.
- Default track, LOOPED continuously: "Sony's Playstation 4 Start-up Tune
  (TXZ1'S EPIC REMIX) and also _ eljon9's Theme Song" — the file Matty
  downloaded, `Sony's Playstation 4 Start-up Tune (TXZ1'S EPIC REMIX) and
  also _ eljon9's Theme Song (1).mp3` (4,619,325 bytes, 4:19, in
  ~/Downloads/, left exactly as downloaded — NOT renamed). The identical
  file without " (1)" has the same md5, so either copy is the same track.
- The track is bundled two ways in the PKG:
  - `assets/audio/ps4_startup.mp3` — the original MP3, kept for reference
    and for 1.04 (custom music).
  - `assets/audio/ps4_startup.pcm` — the track decoded at BUILD time on
    the laptop with ffmpeg to raw 48 kHz mono 16-bit PCM (24,911,482
    bytes, 12,455,741 samples, 4:19). This is what the app actually
    plays. There is no usable in-app MP3 decoder in this toolchain
    (libSceAudiodec has an MP3 codec id, but its control structs are
    undocumented "unk" fields), so the MP3 is decoded before the build
    instead of on the console.
- New menu row "Audio" (row 8, D-pad Up/Down to reach it, Left/Right or X
  to change it): **Pad speaker** (default) / **TV** / **Both** / **Off**.
  - Pad speaker: music out of the DualShock 4's built-in mono speaker
    (tinny, as expected) via `sceAudioOutOpen` on the pad-speaker port.
  - TV: music out of the TV over HDMI via the MAIN audio port — the same
    proven pattern as the toolchain's audio-wav sample.
  - Both: the same track fed to the pad speaker AND the TV at the same
    time via `sceAudioOutOutputs`.
  - Off: no music. The track keeps looping from where it was when you
    switch back (play position is kept across mode changes).
- Kept from 1.02: on-screen menu (now 8 rows), R/G/B sliders, presets,
  disco speed, return codes, /data/rgb_log.txt logging, rumble pulse,
  icon. Menu rows are slightly tighter (56 px spacing) and the 4 colour
  bars moved down to make room for the 8th row.
- PKG built 2026-11-25, version 1.03, same filename
  IV0000-RGBB00001_00-RGBBARCONTROL000.pkg, ~33.5 MB (the PCM audio is
  ~25 MB of that). **Untested on real PS4 yet** — see test notes below.

### Honest limits — read before testing (1.03 audio)
- The pad-speaker port is NOT defined in the OpenOrbis headers (only the
  MAIN/TV port is). 1.03 opens it as port value 3, from the official PS4
  SDK port order (MAIN=0, BGM=1, VOICE=2, PADSPK=3). That value is
  UNVERIFIED on real hardware: if the PS4 rejects it, the on-screen
  "Audio:" line will say "Pad spk OPEN FAILED" and /data/rgb_log.txt
  will show the `sceAudioOutOpen PADSPK` return code, and the app keeps
  running (light bars unaffected). Switch Audio to TV in that case.
- "Both" relies on `sceAudioOutOutputs`, also untested on real hardware.
  If it fails, the app falls back to Pad speaker only and logs it.
- If a feed call fails mid-song, music stops for good (that run) and the
  log says why; menu + light bars keep working. Restart the app to retry.
- Music (like the colours) only plays while this app is open in the
  foreground. Quitting stops it.

## Release notes — 1.03.1

**RGB-Controller 1.03.1** — audio fix for 1.03 (tested 2026-11-25: skippy
on TV, silent on controller speaker).
- **Root cause of the skipping:** in 1.03 the MAIN thread fed audio in
  bursts of 19 grains (~101 ms of sound) and then did a full 1080p
  Scene2D render + flip (tens of ms) before feeding again. Every cycle
  therefore had a silence gap, so the TV stuttered. The controller
  speaker, with its much smaller buffer, starved completely and stayed
  silent. The toolchain audio-wav sample never does anything but feed
  audio — that is the pattern that works.
- **Fix:** audio now runs on its OWN thread (`std::thread`, as in the
  toolchain threading sample). It feeds exactly ONE 256-sample grain per
  iteration with the sample pattern `sceAudioOutOutput(h, NULL)` (wait
  for the previous grain) then `sceAudioOutOutput(h, grain)`. The
  NULL-wait paces the thread at ~real time (~5.33 ms/grain @ 48 kHz), so
  graphics/input on the main thread can never starve the audio again.
  Mode changes are a request flag the audio thread applies itself;
  audio handles are owned only by the audio thread.
- **Pad speaker:** port value is still 3 (MAIN=0, BGM=1, VOICE=2,
  PADSPK=3), still unverified in the OpenOrbis headers — no authoritative
  source for a different value could be found. 1.03.1 now tries the
  login user first, then the SYSTEM user, and logs BOTH
  `sceAudioOutOpen PADSPK` return codes, plus the first grain fed, so
  the next test can say definitively whether port 3 opens at all. With
  the thread fix, a working port should now actually be heard instead
  of starving. If it still says "Pad spk OPEN FAILED", the return code
  in /data/rgb_log.txt is the key evidence.
  **Test result 2026-11-25: the pad speaker is difficult to get working
  and still does NOT play — no sound from the controller in testing,
  even after the thread fix. The port value (3) remains unverified and
  may be wrong, or the pad speaker may need a different call/params
  than sceAudioOut. TV is the working output for now.**
- "Both" now waits on both ports with `sceAudioOutOutputs` (NULL
  pointers) before feeding both, and falls back to Pad only if that
  fails, telling the main thread to show Pad in the menu. A feed failure
  no longer kills music for good — switching the Audio row re-opens the
  ports and retries.
- Kept from 1.03: looped PS4 startup tune, Audio row (Pad speaker / TV
  / Both / Off, default Pad speaker), 8-row menu, R/G/B sliders,
  presets, disco speed, return codes, /data/rgb_log.txt logging, rumble
  pulse, icon.
- PKG built 2026-11-25, version 1.03.1, same filename
  IV0000-RGBB00001_00-RGBBARCONTROL000.pkg, ~33.5 MB.
  **Tested 2026-11-25 on Baikal PS4 Slim: TV speaker works — smooth,
  no skipping (thread fix confirmed). Pad / controller speaker is
  difficult to get working and does NOT work yet — see below.**

## Release notes — 1.02

**RGB-Controller 1.02** — on-screen menu + R/G/B sliders + disco speed control.
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
  Normal, about the same pace as 1.01; speed 1 is the slowest, speed 5
  cycles the rainbow in about 3-4 seconds.
- Kept from 1.01: presets on startup, disco mode, on-screen display with
  return codes, /data/rgb_log.txt logging, rumble pulse at startup, and
  the 512x512 rainbow icon.
- PKG built 2026-11-25, version 1.02, same filename
  IV0000-RGBB00001_00-RGBBARCONTROL000.pkg, 6.4 MB. USB was not plugged
  in, so the PKG was left ready in ~/RGBBarControl/ to copy to
  /media/matt/PS4DATA/pkg/ when Matty wakes. **TESTED 2026-11-25 on Baikal
  PS4 Slim: disco speed and sliders work** (see test notes below).

## Release notes — 1.01

**RGB-Controller 1.01** — light-bar fix + on-screen display + new icon.
- Light bar fix first: 1.00 ignored return codes and hung forever if
  `scePadInit()` failed, so the light bars never changed. 1.01 logs the
  return code and tries `scePadOpen` anyway, gets the real login user
  via `sceUserServiceGetInitialUser`, polls `scePadReadState` every
  tick, records every `scePadSetLightBar` return code on screen and in
  `/data/rgb_log.txt`, and does a short rumble pulse at startup to
  prove the pad handle works.
- On-screen display (was a black screen in 1.00): background follows
  the disco hue, 4 colour bars show each pad's colour, plus mode,
  pads-open count and return codes as text.
- New 512x512 icon (`sce_sys/icon0.png`): rainbow light bar on a dark
  background with "RGB". The 1.00 icon was the OpenOrbis Hello World
  sample placeholder. Icon design source: `RGBBarControl/icon/icon_gen.py`.
- Same as 1.00 otherwise: presets (Pad 1 blue, Pad 2 red, Pad 3 green,
  Pad 4 magenta, held 3 seconds), then disco mode (rainbow hue cycle,
  colour changes every 0.5s, each pad offset by 90 degrees).

## Test log

### 1.00 test — 2026-11-25, PS4 Slim 9.00, GoldHEN
- PKG installed, title "RGB-Controller" correct.
- Screen was **black** and controller light bars did **NOT** change.
- Known 1.00 bugs:
  1. No on-screen graphics — `main.cpp` only `printf()`d and set light
     bars, so the screen was black by design.
  2. Icon was still the OpenOrbis Hello World sample placeholder
     (`sce_sys/icon0.png`).
  3. Light bar did not change — `scePadSetLightBar` did not take
     effect, and return codes were ignored in 1.00 so the cause was
     unknown at test time.
- 1.01 (in progress at the time of this log): fix the light bar FIRST
  (check `OrbisPadColor` layout, return codes, `scePadOpen` pattern,
  focus/privilege/read loop), then the on-screen display, then the new
  512x512 icon.
- What the 1.01 test should show: the screen is no longer black (title,
  mode, pads-open count and return codes visible), the controller
  buzzes once at startup (rumble test = pad handle works), and the
  light bars change colour — presets first, then disco. If the bars
  still do not change, the on-screen return codes and
  `/data/rgb_log.txt` say exactly which call failed.

### 1.02 test — TESTED 2026-11-25, Baikal PS4 Slim (CUH-2202A, FW 9.00, GoldHEN): disco speed and sliders work
- Tested 2026-11-25 on Baikal PS4 Slim: disco speed control works and R/G/B sliders work.
- Install the 1.02 PKG from /media/matt/PS4DATA/pkg/ (copy it there first — USB was unplugged).
- Check the menu shows: Mode / Pad / Red / Green / Blue / Preset / Disco speed.
- D-pad Up/Down moves the > cursor; Left/Right changes the value; hold Left/Right to repeat.
- Set Pad to 1, move Red to 255, Green/Blue to 0 — Pad 1 light bar should go red and stay (CUSTOM mode).
- Try a preset, try Disco speed 1 vs 5 — rainbow should be clearly slower/faster.
- Triangle should toggle Disco <-> Custom; Circle should jump back to Disco.
- If a button does nothing, check /data/rgb_log.txt — menu changes are logged there.

### 1.03 test — needs Matty on real PS4 (not tested yet)
- Install the 1.03 PKG from /media/matt/PS4DATA/pkg/ (copy it there first
  if the USB wasn't plugged in — the PKG is ~33.5 MB now).
- On launch (default Audio: Pad speaker) the PS4 startup tune should
  start playing out of the controller speaker within a second or two,
  tinny mono, and LOOP when it ends (4:19 per loop).
- Check the on-screen "Audio:" line: it should say "Pad spk OK". If it
  says "Pad spk OPEN FAILED", note the `sceAudioOutOpen PADSPK` return
  code in /data/rgb_log.txt — that tells us the port value is wrong.
- Move to the Audio row (bottom row) and try TV: the tune should come
  out of the TV instead. Try Both: tune from pad AND TV at once. Try
  Off: silence. Switching back resumes the tune from where it was.
- If you hear nothing anywhere, check /data/rgb_log.txt for the
  `sceAudioOutInit`, `PCM loaded`, and `sceAudioOutOpen` lines — they
  say exactly which step failed.
- Also re-check the 1.02 menu still works (all 8 rows now) — the Audio
  row is the only new one.

### 1.03.1 test — TESTED 2026-11-25, Baikal PS4 Slim (CUH-2202A, FW 9.00, GoldHEN)
- **TV: TESTED, WORKS — smooth, no skipping.** Set Audio to TV and the
  tune plays smoothly and loops every 4:19. The audio-thread fix is
  confirmed.
- **Pad speaker: TESTED, DOES NOT WORK.** No sound from the controller
  speaker, even after the thread fix. The pad speaker is difficult to
  get working — port value 3 is unverified in the OpenOrbis headers and
  may be wrong, or it may need a different call/params. The
  `sceAudioOutOpen PADSPK` return codes in /data/rgb_log.txt (login
  user + SYSTEM user) are the key evidence for the next attempt.
- Try Both: tune from pad AND TV at once. Try Off: silence. Switching
  back should resume and re-open the ports.
- If you hear nothing anywhere, check /data/rgb_log.txt for
  `sceAudioOutInit`, `PCM loaded`, `sceAudioOutOpen`, and
  `Audio: first grain fed OK` — they say exactly which step failed.

### 1.04 test — TESTED 2026-11-25, Baikal PS4 Slim (CUH-2202A, FW 9.00, GoldHEN): everything works
- Install the 1.04 PKG from /media/matt/PS4DATA/pkg/ (copy it there first
  if the USB wasn't plugged in).
- Menu now has 9 rows — check Track is the bottom row and the 4 colour
  bars + "SetLightBar rc" line still fit on screen.
- Track row: cycle Track 1 PS4 Startup -> Track 2 Stayin' Alive (4:23)
  -> Track 3 Coffin Dance (2:35) — each should play smooth on TV and
  loop at the end. Switching tracks should load the new song from the
  start.
- Run the app once, then check with GoldHEN FTP that `/data/rgb_music/`
  was created on the console. Check /data/rgb_log.txt for the
  `FTP music:` lines (folder created/already exists, track count).
- Convert a song on the laptop (`ffmpeg -i "Song.mp3" -ar 48000 -ac 1
  -f s16le "Song.pcm"`), FTP the .pcm into /data/rgb_music/, move onto
  the Track row — the song should appear there without restarting the
  app. Cycle to it: it should play smooth on TV and loop at the end.
- Also check: an MP3 put in /data/rgb_music/ is ignored (not listed),
  and switching back to Track 1 (PS4 Startup) works.
- Re-check the 1.03.1 menu still works (all 9 rows now) and TV audio is
  still smooth.

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

- Music (1.03): the default track is decoded at build time to raw PCM
  (`assets/audio/ps4_startup.pcm`, 48 kHz mono s16) and fed to the audio
  ports in 256-sample grains with `sceAudioOutOutput` / `sceAudioOutOutputs`,
  looping at the end of the track. Pad speaker = PADSPK port (value 3,
  unverified — see limits above), TV = MAIN port, Both = both at once.
- Music (1.03.1): the feeding moved to a dedicated audio thread — one
  grain per iteration, `Output(h, NULL)` then `Output(h, grain)`, paced
  by the audio hardware, so rendering can never starve it.
- Music (1.04): 3 bundled tracks (PS4 startup tune, Stayin' Alive,
  Coffin Dance — all converted at build time to raw 48 kHz mono s16 PCM
  with ffmpeg; the original MP3s are also in the PKG). The app creates
  `/data/rgb_music/` on startup and scans it for .pcm files (raw 48 kHz
  mono s16, converted with ffmpeg on the laptop — MP3s cannot be decoded
  on the console). The Track menu row cycles the bundled tracks + FTP
  tracks; switching a track is a request
  to the audio thread, which frees the old PCM, loads the new file and
  loops it. The folder is rescanned when the Track row is first entered.

## Versions

- **1.00:** preset colours + Disco Mode (rainbow hue cycle). No menu
  yet — just run the app and the light bars cycle. Tested 2026-11-25:
  black screen, light bars did not change (see test log above).
- **1.01:** light-bar fix, on-screen display, new icon. Tested 2026-11-25: works.
- **1.02:** on-screen menu with R/G/B sliders per pad, presets,
  disco speed control. Tested 2026-11-25: works — disco speed and sliders tested on Baikal Slim.
- **1.03:** music / audio out of the DualShock 4 controller speaker —
  default track looped, Audio menu row (Pad speaker / TV / Both / Off).
  Tested 2026-11-25: audio skippy on TV, silent on controller speaker.
- **1.03.1:** audio-thread fix — music fed on its own
  thread (one grain at a time, paced by the audio hardware), pad open
  tries login user then SYSTEM user with both return codes logged.
  Tested 2026-11-25: TV smooth, no skipping; pad speaker difficult
  to get working and does NOT work yet.
- **1.04 (this build):** custom music — Track menu row (3 bundled tracks:
  PS4 Startup, Stayin' Alive, Coffin Dance, + FTP .pcm files from
  /data/rgb_music/, created by the app on startup), rescanned on entering
  the Track row. TESTED 2026-11-25 on Baikal PS4 Slim: everything works
  (Track cycling, smooth TV audio + looping, /data/rgb_music/ created).
  Pad speaker still does NOT work.
- **2.0 (planned, last):** background mode — a GoldHEN plugin (.prx),
  not a normal app, so colours keep working in games.
  Harder/riskier.

> Roadmap reordered 2026-11-25: music first (1.03 speaker music, 1.04
> custom music), background GoldHEN plugin last (2.0).
> Releases renamed 2026-11-25: 1.00 / 1.01 / 1.02 / 1.03 / 1.04, with the
> background GoldHEN plugin as major version 2.0.


## Limits — read this

- The colour only lasts while this app is open and in the foreground.
  Quitting, disconnecting, or launching a game lets the system / game
  take the light bar back.
- The PS4 console itself has no RGB light — its strip is blue/white/orange
  for power/rest only. Console RGB is not possible in software; it needs
  a physical LED kit.
