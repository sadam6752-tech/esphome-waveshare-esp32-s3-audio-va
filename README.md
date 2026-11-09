![The Waveshare ESP32-S3-AUDIO-Board as a voice satellite](docs/hero.jpg)

# Waveshare ESP32-S3-AUDIO voice satellite for ioBroker

A voice satellite for **[ioBroker.assistant](https://github.com/ioBroker/ioBroker.assistant)** on the
[Waveshare ESP32-S3-AUDIO-Board](https://www.waveshare.com/esp32-s3-audio-board.htm): wake word on the board,
speaker, LED ring, three buttons and, optionally, the **Waveshare 2.8inch Capacitive Touch LCD** with clock,
timers and tile pages filled from ioBroker.

**Ready-made firmware: nothing to build.** Flash it from the browser, set up Wi-Fi on the screen or the phone,
updates come over the air.

```
You  ──▶  Waveshare board  ──▶  ioBroker.assistant  ──▶  pi-agent / cloud model
          (wake word, audio)    (speech recognition, TTS, rules)
```

This is a fork of [MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va)
(a Home Assistant satellite). The audio core comes from there; this fork adds the touch display, the ready-made
firmware and everything for ioBroker.

## Install

You need the board, a USB-C cable and Chrome or Edge on a computer.

1. Open the install page **https://sadam6752-tech.github.io/esphome-waveshare-esp32-s3-audio-va/** and press
   **Install** - *with display* or *without display*. If no port shows up, hold **BOOT** while plugging in the cable.
   Tick **Erase device** when you come from another firmware.
2. Set up **Wi-Fi**, one of:
   - right after flashing, on the install page (it offers it by itself);
   - on the display: it opens the Wi-Fi page by itself when there is no network (later: gear > **WLAN**) -
     *Suchen*, pick the network, type the password;
   - with a phone: without a network the board opens the access point **Waveshare-VA Setup**; join it and pick
     your network.
3. In **ioBroker.assistant**, tab *Satellites*: add the board (its IP, port 6053) and give it a room. Its settings
   then appear under `assistant.0.satellites.<room>.controls.*`.

The board's name gets the end of its MAC address (`waveshare-va-eb1614`), so several boards do not clash.

## Updates

New versions are published here (GitHub Releases); the board looks for them at start and every 6 hours.

- **On the display:** swipe to *System* > **FW Update** > **Ja**. A blue screen shows the progress - do not unplug
  the board until it restarts.
- **From ioBroker:** `controls.firmware.check` looks now, `controls.firmware.install` installs what
  `controls.firmware.latestVersion` shows.

Settings (brightness, microphone gain ...) stay on the board across updates.

## Requirements

| | Version | For |
|---|---|---|
| ioBroker.assistant | **1.4.9** or newer | Yes/No buttons, reminders as timers on the satellite; 1.4.8 for labels from ioBroker, 1.4.7 for the display language |
| ioBroker.pi-agent | 0.9.5 or newer (optional) | reminders asked on the satellite come back there |
| Display | Waveshare 2.8inch Capacitive Touch LCD, **Rev 1.1** | the touch controller CST3530 (see [the display](#the-touch-display)) |

## What it does

- **Voice:** wake word on the board - **"ioBroker"** (built in), Alexa, Okay Nabu, Hey Jarvis, Hey Mycroft; which
  ones listen is chosen in ioBroker.assistant. Speech recognition, the answer and TTS run in ioBroker.assistant.
- **LED ring:** listening, thinking, answering, timer, ringing, no Wi-Fi, no connection - each its own color and
  effect; brightness and effects are settings.
- **Timers:** set by voice (or by pi-agent as a reminder), countdown on the ring and the display, then it rings.
  Stop it with the wake word, the middle button or a touch on the screen; it stops by itself after a minute.
- **Buttons:** volume down, play/pause, volume up; while it rings, the middle one stops it.
- **Night mode:** the ring and the display dim, no wake beep.
- **Everything is a setting in ioBroker**: microphone gain, wake word sensitivity, LED brightness, night mode,
  display ... under `assistant.0.satellites.<room>.controls.*`.

## The touch display

The **Waveshare 2.8inch Capacitive Touch LCD** (240x320, ST7789) goes on the 18-pin LCD connector of the board -
plug the ribbon in with the board unpowered. Take the firmware *with display*. **Rev 1.1** (printed on the back)
has the touch controller CST3530, for which this firmware brings its own driver; the older version (CST328) is not
tested.

### Pages

```
System  ◀─ swipe ─  Clock  ─ swipe ▶  Tiles 1 ▶ 2 ▶ 3 ▶ 4        Settings: the gear on the clock
```

- **Clock:** time, date, the alarm when it is on, the running timers at the bottom (one with its name, several as
  `T1 4:59  T2 12:00`).
- **State of the assistant** (top right, on every page): blue microphone = listens, amber dots = thinks, green
  speaker = answers, red = no Wi-Fi / no connection / error, grey crossed-out microphone = microphone off.
- **Conversation:** what the board heard and the answer. A question of the assistant ("testLamp ausschalten? Sag
  ja oder nein.") gets **Ja / Nein** buttons - a touch answers like the spoken word.
- **Now playing:** while music or a radio stream plays (or is paused), a bar at the bottom of the clock shows its
  title and **⏯ ⏹ − +**.
- **Volume:** a bar while the volume changes.
- **Ringing:** an orange **Stopp** over everything; a touch anywhere stops it.
- **System:** firmware version, Wi-Fi and signal, IP, uptime, memory, connection to the assistant; **FW Update**
  and **Neustart** (restart, asks first).
- **Tiles:** up to four pages of tiles from ioBroker - values, gauges, bars, charts, switches, dimmers,
  buttons and thermostats (see [Tile pages](#tile-pages)). A page without tiles is skipped.
- **Settings:** volume, display brightness by day and at night, LED ring, microphone gain (dB), microphone off,
  night mode, wake sound, language, Wi-Fi.

Other pages go back to the clock after a minute without a touch. In night mode the display goes dark (or dims);
a touch lights it for 10 seconds, and it lights by itself while you talk or a timer rings.

### Settings of the display in ioBroker

All under `assistant.0.satellites.<room>.controls.`:

| State | What it does |
|---|---|
| `display_language` | Deutsch, English, Русский - the assistant sets it to its voice language |
| `display_brightness` / `display_night_brightness` | backlight by day / in night mode in % (0 = off) |
| `confirm_answer` | the last Ja/Nein touch (`yes` / `no`), read only |
| `media_title` | title of what plays (a station name), shown in the now-playing bar |
| `waveshare_voice_<mac>.command` / `.volume` / `.state` | the media player: play, pause, stop, volume_up/down ...; volume 0…1 |
| `reboot` | `true` restarts the board |

### Tile pages

Up to four pages on a grid of 2 x 4 cells, each with up to eight tiles in four sizes (small, square, wide,
large) and eight types: value, gauge, bar, chart, switch, dimmer, button and thermostat. What is on them is not part of the
firmware: the adapter [ioBroker.satellite-display](https://github.com/sadam6752-tech/ioBroker.satellite-display) connects to the board next to
ioBroker.assistant, sends the layout and the values of your states and switches them when a tile is touched.

The board keeps no layout: after a restart or a lost connection the adapter sends it again. The format (the
actions `tile_*` and the event `esphome.tile_input`) is in [docs/TILES.md](docs/TILES.md).

Firmware 2.0.0 replaced the fixed values and switches pages (`value_1` ... `value_4`, `switch_1` ... `switch_4`
and their labels) with the tile pages.

## Reminders (with pi-agent)

Asked on the satellite ("Erinnere mich in zehn Minuten daran, die Wäsche aufzuhängen"), pi-agent 0.9.5+ sets the
reminder as a **timer on this satellite**: it counts down on the display, rings, and then the text is spoken. A
reminder further than 24 hours ahead is spoken on the satellite when it is due. Short phrases like "in einer
Minute" are often taken by ioBroker.assistant itself as a plain timer.

## Troubleshooting

- **No port on the install page:** hold BOOT while plugging in the USB cable; use a data cable, not a charging one.
- **Wrong Wi-Fi password:** the board opens the access point *Waveshare-VA Setup* after about a minute; on the
  display, gear > WLAN.
- **The clock is wrong:** it comes from the internet (NTP) and the time zone is Germany.
- **Logs:** in ioBroker.assistant (debug level) or, with ESPHome installed, `esphome logs release-display.yaml
  --device <ip>`.

## Building it yourself (optional)

Not needed to use the board - only to change the firmware.

- `release-display.yaml` / `release-core.yaml`: the ready-made firmware (with / without display), built by
  `.github/workflows/release.yml` on every tag `v*` and published with the install page on GitHub Pages.
- `base/core.yaml`: audio, wake word, LED ring, buttons, timers; `base/display.yaml`: the display;
  `base/release.yaml`: Wi-Fi setup, updates, NTP clock, the "ioBroker" wake word.
- `components/cst3530`: the touch driver (ESPHome has none for this chip), written after Waveshare's
  `esp_lcd_touch_cst3530` (Apache-2.0).

```bash
pip install esphome==2026.10.2
cp secrets.example.yaml secrets.yaml   # the release firmware removes its Wi-Fi again
esphome compile release-display.yaml
```

Hardware notes - pinout, the shared I2S bus of the two codecs, the expander, the gotchas - are in
[docs/HARDWARE.md](docs/HARDWARE.md); the history is in [CHANGELOG.md](CHANGELOG.md).

## Credits

- **[MichalZaniewicz](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va)**: the original
  ESPHome satellite for this board - audio core, LED ring, timers - that this fork builds on.
- **[jensenbox](https://github.com/jensenbox/waveshare-esp32-s3-audio)**: the I2S layout of this board the audio is
  based on.
- **[ioBroker.assistant](https://github.com/ioBroker/ioBroker.assistant)** (@GermanBluefox): the satellite side in
  ioBroker, the Yes/No answer, timers and labels of the screen.
- **ESPHome** and **[Home Assistant Voice PE](https://github.com/esphome/home-assistant-voice-pe)** (sounds, the
  phase model of the LED ring).
- **Waveshare**: `esp_lcd_touch_cst3530`, the reference for the touch driver.
