# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

A pocket voice recorder on the M5StickS3. KEY1 (the blue button right of the
screen) starts and stops recording to internal flash; KEY2 (on the top edge)
turns on an open Wi-Fi access point "Voice Recorder" with a web page at
http://192.168.4.1 that starts and stops recording and lists, plays,
downloads and deletes the recordings (one by one or all at once).
The device is held in landscape. Recordings are IMA ADPCM WAV, 8 kHz mono,
about 24 minutes in total.

## Language

All documentation and the project itself are kept in English: this file, code
comments, identifiers, strings shown on the display, commit messages and any
other text files. New text is written in English too, even when the
conversation with the user happens in another language.

## Commands

PlatformIO is not installed globally but by the official installer into a
venv. The binary lives at `~/.platformio/penv/bin/pio`; it is not on `PATH`,
so it has to be called by its full path.

```bash
~/.platformio/penv/bin/pio test -e native                         # host unit tests for the logic
~/.platformio/penv/bin/pio test -e native -f test_ima_adpcm       # a single test suite
~/.platformio/penv/bin/pio run -e sticks3                         # build the firmware
~/.platformio/penv/bin/pio run -e sticks3 -t upload               # flash the board
~/.platformio/penv/bin/pio run -e sticks3 -t merged               # single image for M5Burner
~/.platformio/penv/bin/pio device monitor -e sticks3              # serial monitor, 115200
```

The xtensa-esp32s3 toolchain is installed into `~/.platformio/packages` once
per machine and is shared by all projects.

## The design

The screens are numbered, and the user refers to them by number. The design
was approved as a whole (mockups in the first session, screens 8, 9 and the
web page v2 in the second) and is frozen; change it only when the user asks.

| # | Screen | KEY1 zone (right edge) | KEY2 hint (top) |
|---|---|---|---|
| 1 | Ready: minutes left, storage bar, MB free | red circle, "Record" | "▲ Wi-Fi" |
| 2 | Recording: timer, MB, time left, waveform, storage bar | 12 px stop square in a pulsing ring, "Stop" | none |
| 3 | Saved: name, duration, MB, min left (or "Memory full"), centred | none | none |
| 4 | Wi-Fi: network name, no password, 192.168.4.1, devices | empty | "▲ Exit" |
| 5 | Ready with memory full, amber | grey circle, "Full" | "▲ Wi-Fi" |
| 6 | Preparing storage (first start) | | |
| 7 | No microphone | | |
| 8 | Recording with Wi-Fi on (started from the web page) = screen 2 | as on 2 | none |
| 9 | Wi-Fi with memory full = screen 4, bottom line amber "Memory full · N recordings" | empty | "▲ Exit" |

- The battery (icon and %) is in the top-left corner on every screen. There is
  no "REC" label on screen 2: the pulsing ring says it.
- Screen 3 stays 4 s, then fades to black and screen 1 (or 5, or 4/9 when
  Wi-Fi is on) fades in; a key press on 3 only skips to that fade. 1 ↔ 4 fade
  the same way. The battery does not fade.
- While the access point is on, a small cyan Wi-Fi mark sits right of the
  battery percentage on every screen (4, 8, 3, 9).
- In Wi-Fi mode recording starts **from the web page only** (the user's
  choice): KEY1 does nothing on 4 and 9, but stops a recording on 8, like on
  2. KEY2 does nothing while recording. Wi-Fi stays on across a recording.
- Button positions, checked against a photo of the device: KEY1 is on the
  front, right of the screen, at mid-height; KEY2 is on the top edge above
  the right third of the screen (screen x ≈ 142–232), so its hint is centred
  at x = 187. The display uses `setRotation(1)`.

## Architecture

The split into `lib/` and `src/` is not cosmetic here, it is load-bearing:

- `lib/` is the logic: plain C++ with no Arduino, no M5Unified and no hardware
  access of any kind.
  - `ImaAdpcm` — encoder/decoder of 256-byte, 505-sample mono blocks.
  - `WavHeader` — the 60-byte header, repair after a power cut, bytes ↔ time.
  - `Recordings` — `REC_nnnn.wav` names (also the guard for web routes),
    duration and MB formatting.
  - `RecorderState` — modes Ready / Recording / Saved plus a separate Wi-Fi
    switch; keys and web Record/Stop go in, commands for `main.cpp` come
    out; the fade timing.
  - `ScreenPolicy` — backlight On / Dim / Off (off after 3 min idle; while
    recording only dims after 30 s).
  - `LevelMeter` — the waveform bars (block peak on a -60…0 dBFS scale), also
    as hex for the web page.
  - `BatteryFilter` — steadies the battery percentage.
  - `BlockRing` — which capture blocks are finished (from M5SpectrumAnalyzer).
- `src/` is everything that knows about the board: `AudioCapture` (mic),
  `RecordingWriter` (files), `Storage` (LittleFS), `WebPortal` + `WebPage.h`
  (Wi-Fi mode), `Renderer` (screens), `main.cpp` (wiring and Serial test
  hooks).

The `native` environment builds only `lib/` (PlatformIO's `test_build_src`
defaults to `no`), so the logic is tested on the Mac without the board.
**Do not pull hardware dependencies into `lib/`: that breaks the tests.**

### Time is passed in as a parameter

The logic in `lib/` does not call `millis()` itself; it receives the current
time as an argument. That way tests can substitute any moment without
waiting.

`millis()` overflows after roughly 49 days. Compute intervals with unsigned
subtraction `now - since`, so the overflow goes unnoticed.

### Storage and format

`partitions.csv`: one 2 MB app (no OTA; the firmware is ~1.1 MB) and a
5.9 MB LittleFS partition labelled `spiffs` (the label LittleFS looks for by
default). LittleFS rather than FAT because it survives a power cut in the
middle of a write. The first start formats it (screen 6).

IMA ADPCM WAV, format tag 0x11, 8000 Hz mono, 4 bits, 256-byte blocks: about
4 KB/s, 0.24 MB per minute. The user chose 8 kHz (telephone quality) over
16 kHz for twice the recording time. macOS (CoreAudio), VLC and Windows play
the files; browsers do not, so the web page decodes them in JavaScript.

Writing: blocks are encoded as they arrive, written 16 at a time (4 KB, about
1 s), and the file is synced every 10 s. The header gets its sizes on stop;
after a power cut `Storage::repairAll()` fixes it at the next start from the
file length. Stopping takes 0.1–0.25 s (the header rewrite makes LittleFS
copy a flash block). `Storage::refresh()` walks the whole file system and
takes about 0.4 s, so after a stop only `noteAdded()` updates the cached list
and the free space (rounded to 4 KB blocks; it matched a real refresh
exactly); real refreshes happen on Wi-Fi off, list requests from the page,
deletions and at start-up. Recording stops when only `Storage::kReserveBytes` (64 KB) is
left. Numbers only go up (the last one is kept in NVS), so a new recording
never takes the name of a deleted one.

### Audio capture

`M5.Mic` at 8 kHz, `over_sampling = 1` (the ES8311 decimates to 8 kHz itself
and filters everything above 4 kHz), `magnification = 2` (unity gain),
`noise_filter_level = 0`, I2S DMA ring of 8 × 256 frames = 512 ms. The mic is
always running, also when not recording: the ES8311 returns zeros for about a
second after it is powered up.

Capture requests are **1010 samples (two ADPCM blocks), not 505**. M5Unified's
mono capture yields samples in pairs; with an odd request length it carries
the second of a pair over to the next request, and on the board that path
left one wrong sample at the start of a block every 74 blocks (a tick every
4.7 s). Even-length requests never use the carry. This was found with
`tools/analyze_recording.py --tone`, which reports such jumps.

Flash writes stop both cores; measured write times are 10–20 ms, rarely up to
100 ms, well inside the 512 ms DMA ring. Over a 3-minute recording the audio
length matched the Mac's clock to 20 ms, with no lost blocks.

### Wi-Fi mode

`WebPortal`: open access point, `WebServer` on port 80. Requests are
handled inside `loop()` one at a time, so the handlers call into `main.cpp`
through `WebPortal::Hooks`; web Record/Stop go through `RecorderState` like
the keys.

| Route | |
|---|---|
| `GET /` | the page from `src/WebPage.h` (self-contained, always dark: the user's choice) |
| `GET /api/status` | `{state, name, seconds, bytes, secondsLeft, canRecord, stoppedFull, levels}`; cheap, no file-system walk |
| `POST /api/record`, `POST /api/stop` | start / stop; 409 with a reason when not possible |
| `GET /api/recordings` | `{total, free, secondsLeft, recordings: [{name, bytes, seconds}]}`; the cached list while recording |
| `DELETE /api/recordings` | Delete all (recordings only); 409 while recording |
| `GET /rec/<name>` | the file, `Content-Disposition: attachment`; 409 while recording |
| `DELETE /rec/<name>` | removes it; 409 while recording |

While recording, play / download / delete are refused and greyed out on the
page: `streamFile` would hold the loop for seconds and the recording would
get gaps (the user chose this over an asynchronous server). The page polls
`/api/status` every 400 ms while recording (timer and the same 54-bar
waveform as the stick) and every 2 s otherwise. Delete all sits under the
end of the list, away from Record, and asks in an in-page dialog first.

Recording with the access point on (no client joined): the 1 kHz tone test
showed no dips, jumps or lost blocks, and the noise floor rose from -60.2 to
-58.8 dBFS.

There is no captive-portal DNS on purpose: macOS would open the page in its
captive-network sheet, which cannot download files. Switching the AP off makes
ESP-IDF 4.4 log a couple of harmless "rxcb … failed" netif errors whatever the
order of the calls; the radio does go off.

`tools/mock_portal.py <dir> [port] [--full]` serves the same page and routes
on the Mac from a folder of recordings, fakes a running recording (timer,
waveform) and memory full, for working on the page without joining the
recorder's network. The screenshots of the page in `docs/screens/` come from
it (headless Chrome).

### Rendering

`Renderer` composes the whole frame in an `M5Canvas` (a sprite in PSRAM) and
pushes it with a single `pushSprite`. `main.cpp` fills a `ScreenModel`
(zeroed with `memset` so that equal models compare equal byte for byte) and
`draw()` skips a frame whose model has not changed; `invalidate()` after the
display wakes up. Fades scale every colour toward black; at low levels RGB565
gives grey a slight green tint, which is invisible in a 300 ms fade.

Fonts are ASCII only: the middle dot, the arrows, the check mark and the icons
are drawn. Small text is `DejaVu9`, labels `DejaVu12` drawn twice 1 px apart
for bold, big numbers `FreeSansBold18pt7b`, the IP `FreeSansBold9pt7b`.
Font0 is 6 px per character, so labels under the Wi-Fi icon must stay within
about 9 characters.

## Self-testing on the board

Audio and storage changes are checked end to end from the Mac: it plays known
sounds into the microphone, the firmware records them, the file comes back
over USB and is analysed. Not over Wi-Fi: joining "Voice Recorder" would cut
the Mac (and the Claude Code session) off the internet.

Serial test hooks (lines, 115200): `r` = KEY1, `w` = KEY2, `wr` / `ws` =
web Record / web Stop (records with Wi-Fi on without joining the network),
`l` list, `g <name>` send a file (`FILE <name> <bytes>` + raw bytes), `s`
screenshot (`SNAP <w> <h>` + RGB565), `v <n>` show screen n (1–9) for 3 s,
`fill <KB>` take up storage with `/filler.bin` to test screens 5 and 9
(`fill 0` removes it). A screenshot requested while a slow command (such as
a stop) runs is taken before the next redraw. While
recording, a line a second: `rec sps … write max … lost … data … left …`.

```bash
PY=~/.platformio/penv/bin/python
$PY tools/make_test_audio.py /tmp/audio         # tone_1k, sweep, alias_5k, bursts
$PY tools/serial_cmd.py record 33 --play /tmp/audio/tone_1k.wav --volume 0.5 --out rec.wav
$PY tools/analyze_recording.py rec.wav --tone 1000 --afconvert
$PY tools/analyze_recording.py sweep.wav --sweep 200:3500:20
$PY tools/serial_cmd.py snap screen.png
```

Results on 2026-09-27 (Mac speaker at `afplay -v 0.5`, device next to it):
1 kHz tone at -18 dBFS over a -52…-60 dBFS noise floor, no dips or jumps;
sweep flat within ±5 dB from 200 to 3500 Hz; a 5 kHz tone is not folded back
(nothing above the noise floor); speech peaks at -13 dBFS with no clipping;
macOS `afconvert` decodes the files bit-identically to our decoder (and so
does the page's JavaScript). A reset via RTS in the middle of a recording
kept the audio up to the last sync (9 s of 15 s) and the file was repaired at
the next start.

**Opening the port.** The USB-Serial-JTAG resets the chip while RTS is high
and DTR is low. macOS raises both lines when a port opens, and pyserial then
lowers DTR first, which passes through that reset state: every script run
rebooted the board. `tools/serial_cmd.py` keeps DTR high until RTS is low
(`open_port()`), and the board no longer resets. Use it (or import it) for
any script that talks to the board.

The Wi-Fi page is the one part not tested on the board automatically; it was
tested with `tools/mock_portal.py` in a browser (record, stop, delete all,
memory full, phone width). The user checked it on the board on 2026-09-27:
v1 (joining the network, playing in the browser, downloading and playing on
the computer, deleting) and v2 (Record / Stop from the page, KEY1 stop on
screen 8, the list paused while recording, Delete all) all work.

## Board specifics

M5StickS3 is an ESP32-S3-PICO-1-N8R8 with 8 MB of flash, 8 MB of octal PSRAM,
an ST7789P3 135x240 display, a 250 mAh battery, an ES8311 codec with a MEMS
microphone and no RTC (hence numbered recordings).

- PlatformIO has **no** `m5stack-sticks3` board id. The project uses
  `esp32-s3-devkitc-1` plus `board_build.arduino.memory_type = qio_opi` and
  its own `partitions.csv`. Do not "fix" this to a non-existent id.
- USB is native, with no CH9102 bridge, so on macOS the port is called
  `/dev/cu.usbmodem*`, not `/dev/cu.usbserial*`. Serial output needs the
  `-DARDUINO_USB_CDC_ON_BOOT=1` flag, which is already set.
- Buttons: KEY1 on G11 (`M5.BtnA`), KEY2 on G12 (`M5.BtnB`). A press while
  the screen is off only wakes it. Grove (G9/G10) and HAT2 (G1–G8, G43, G44)
  are free.
- The speaker shares the I2S clock lines with the mic and cannot run at the
  same time: `cfg.internal_spk = false`.
- The battery percentage comes from the voltage and sags under load (Wi-Fi on
  took 6 % off within seconds), hence `BatteryFilter`. `M5.Power.isCharging()`
  gave inconsistent answers during testing, so the screen does not show
  charging.

### Publishing to M5Burner

M5Burner writes the uploaded file starting at address 0x0, so it needs a full
image. A bare `firmware.bin` is meant for address 0x10000: written at 0x0, it
overwrites the bootloader. `pio run -e sticks3 -t merged` (the extra script
`tools/merged_image.py`) merges the bootloader, the partition table,
`boot_app0` and the application into `.pio/build/sticks3/firmware-merged.bin`
with esptool `merge_bin`, taking addresses and flash parameters from
PlatformIO's upload settings. The image does not include the LittleFS
partition; the firmware formats it on the first start.

How this is known. Checked in M5SpectrumAnalyzer on 2026-09-27: of the six
StickS3 firmwares on burner.m5stack.com, five, including the official
UIFlow2.0, are full images: bootloader at 0x0 (header `e9 03 02 3f`),
partition table at 0x8000 (`aa 50`), application at 0x10000. This project's
v1.0.0 image has the same layout, with `firmware.bin` byte for byte at
0x10000. It was written on its own at 0x0 with esptool `write_flash 0x0` on
2026-09-27, the way M5Burner writes it: the board booted to screen 1, kept its
recordings, and a 1 kHz tone recorded clean (no dips, jumps or lost blocks).

The image covers NVS (0x9000–0xE000) with blank `0xFF`, so flashing it loses
the recording counter. Names still never clash with files on the storage
(`Storage::nextName` starts from the highest number there), but the number of
a deleted latest recording can come back.

For the upload, copy the image and the cover into `dist/` (ignored by git):
`dist/M5VoiceRecorder-v<version>.bin` and `dist/M5VoiceRecorder-cover.png`. A
file picker can't easily reach `.pio/`, a hidden folder. The cover is screen 2
(`docs/screens/2-recording.png`) scaled to 1440×810 with nearest-neighbour: the
screen is exactly 16:9, which is the shape M5Burner stores covers in.

The upload form is at burner.m5stack.com/developer/firmware/upload. It asks
for a name, a category and the supported devices (StickS3); a firmware
description and a version description, both in Markdown; the version number
and a link to the project; the `.bin` file; visibility (Public requires
moderation); a cover image. The upload is done through Claude in Chrome,
where the user is signed in and files can be attached; the built-in browser
can't attach files.

v1.0.0 was uploaded on 2026-09-27 as "Voice Recorder", category Audio & Media,
StickS3, Public, and went to review (Pending).

### The power button is handled by the PMIC, not the firmware

| Action | Result |
|---|---|
| Single press | Power on / reset |
| Double press | Power off |
| Long hold | Download mode (the internal green LED blinks) |

So the firmware has no power-off of its own. A double press while recording
cuts the power; the recording survives up to the last 10 s sync.

`M5.Power` does not set `_wakeupPin` for the StickS3, so there is no
ready-made wake-up from deep sleep by button; it would have to be configured
manually with `esp_sleep_enable_ext0_wakeup`.

### If flashing fails

`A fatal error occurred: Failed to connect to ESP32-S3: No serial data received.`

The board shows up as `USB JTAG_serial debug unit` (VID 0x303A, PID 0x1001):
the built-in USB-Serial-JTAG, not a CDC port (a consequence of
`ARDUINO_USB_MODE=1`). Auto-reset into download mode through it does not
always work. The fix is manual: hold the power button until the green LED
blinks. (In this project's first session every upload worked automatically.)

### If the board is stuck in the bootloader

The firmware does not start, and the port shows `boot:0x0 (DOWNLOAD(USB/UART0))`
and `waiting for download`. The way out is a single short press of the power
button. It can be caused by a script toggling DTR/RTS on the port; see
"Opening the port" above.

## Tests

Unit tests cover the logic in `lib/`, with one directory
`test/test_<module>/test_main.cpp` per module; exact values in them are
worked out by hand in comments. Rendering is checked by screenshots
(`serial_cmd.py snap`, `v <n>`) against the design: do not try to write tests
for `Renderer`; that would require mocking all of LovyanGFX and would prove
nothing useful.

`main` in the tests returns the number of failures from `UNITY_END()`, and
PlatformIO reports a non-zero exit code as a signal number. A line like
`Program received signal SIGALRM` with failing tests is a reporting artifact,
not a separate problem; it disappears once the tests pass.
