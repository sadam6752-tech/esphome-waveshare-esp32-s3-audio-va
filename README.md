![ESPHome and Home Assistant voice assistant on the Waveshare ESP32-S3-AUDIO-Board](docs/hero.jpg)

# ESPHome Voice Assistant for the Waveshare ESP32-S3-AUDIO-Board

A **Home Assistant voice satellite** running on the
[Waveshare ESP32-S3-AUDIO-Board](https://www.waveshare.com/esp32-s3-audio-board.htm),
the little AI smart-speaker devkit with a dual-mic array, an ES8311 codec, three
buttons and a 7-LED RGB ring. Pure ESPHome, no custom C firmware: an always-on
core you pull as a package, plus one thin config file you actually edit.

> [!TIP]
> ⭐ **Enjoying this project?** Every star is real motivation to keep it going.
>
> ☕ Want to say thanks another way? You can [buy me a coffee](https://buymeacoffee.com/zanula).

<!-- The badges live OUTSIDE the alert on purpose: Home Assistant rewrites a
GitHub alert into <ha-alert> and drops every child whose textContent is empty,
which silently removes any <img> placed inside it. -->

[![Star this repo](https://img.shields.io/github/stars/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va?style=for-the-badge&logo=github&label=STAR%20THIS%20REPO&labelColor=555555&color=ffc107)](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va) [![Buy me a coffee](https://img.shields.io/badge/BUY%20ME%20A%20COFFEE-FFDD00?style=for-the-badge&logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/zanula)

![Trailer: the voice assistant, the LED ring animations and the Home Assistant controls](docs/trailer.webp)

> **Status: stable (v1.2.0).** Wake word, STT/TTS, clean playback and the LED
> ring are confirmed on-device. Full docs are in the
> [Wiki](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki);
> the release history is in [CHANGELOG.md](CHANGELOG.md).

```
You  ──▶  Waveshare ESP32-S3  ──▶  Home Assistant Assist
         (wake word + audio)      (STT / LLM / TTS)
```

## What it does

![Home Assistant entities, the LED ring animation picker, the media player and the wake-word controls](docs/features.jpg)

- **Voice assistant**: on-device wake word via `micro_wake_word` (Alexa, Okay
  Nabu, Hey Jarvis or Hey Mycroft, picked in Home Assistant), the full Home
  Assistant Assist pipeline (STT / LLM / TTS), a wake beep and music ducking
  while it listens.
- **Simultaneous music and announcements**: a mixer speaker blends the media and
  announcement pipelines, so a doorbell announcement ducks the music instead of
  fighting it. Both are exposed to Music Assistant.
- **LED ring**: one state machine drives it. Boot, no-Wi-Fi, no-HA, listening,
  thinking, replying, timer counting, ringing, volume changed - each a distinct
  colour/effect. Brightness and the animation for the listening / thinking /
  replying phases are pickable from HA: solid plus 14 animations - pulses,
  breathe, wipe, scan, spinner, comet, twinkle, fireworks, fire, rainbows.
- **Timers**: set by voice, with an on-ring countdown and a "Next timer" sensor
  in HA. A ringing timer or alarm is silenced by saying the wake word or by
  pressing Play/Pause. (A daily-alarm engine is present but its entities are
  hidden by default.)
- **Buttons**: the three onboard keys do volume down, play-pause, volume up.
  While a timer or alarm is ringing, Play/Pause silences it instead.
- **Boot chime**: a short "ready" sound once the device connects to HA
  (toggleable, and it also settles the amp so the ring boots silent).
- **Night mode**: one switch dims the ring to its own brightness and skips the
  wake beep and boot chime. Drive it from any Home Assistant automation.
- **Touch display (optional)**: the Waveshare 2.8inch Capacitive Touch LCD on
  the LCD connector shows the clock and date, the running timer and a question
  of the assistant; a touch stops a ringing timer or alarm. See
  [Touch display](#touch-display-optional).
- **Tunable live from HA**: the wake word, microphone mute, ES7210 mic gain, LED
  brightness and wake-word sensitivity are all entities, so there's no
  reflashing to tune it.

https://github.com/user-attachments/assets/3236cd4f-7184-4eee-a16a-0db6fe1603a3

## Ready-made firmware

No ESPHome build needed: open the install page
**https://sadam6752-tech.github.io/esphome-waveshare-esp32-s3-audio-va/**
in Chrome or Edge, connect the board by USB-C and press **Install** - with or
without the touch display. Then set up Wi-Fi: on the page right after
flashing (Improv), on the display (Settings > Wi-Fi: scan, pick, password on
the keyboard), or through the access point **Waveshare-VA Setup** that the
board opens when no network is reachable. Updates arrive over the air as the
**Firmware** update entity (checked every 6 hours).

The ready-made firmware is made for ioBroker.assistant: the clock comes from
NTP, the wake word "ioBroker" is built in, the time zone is Germany (change
it with the `set_time_zone` action) and the device name gets the end of the
MAC address, so several boards do not clash. It is built by
`.github/workflows/release.yml` from `release-display.yaml` and
`release-core.yaml` on every tag `v*`.

## Quick start

> Requires **ESPHome 2026.8.0+**.

1. Copy `secrets.example.yaml` to `secrets.yaml` and fill in your Wi-Fi. The
   native API is unencrypted by default; enable encryption in `base/core.yaml`
   if you want it (see the commented block there).
2. Copy **`waveshare-va.yaml`** next to it and edit the `substitutions:` at the
   top (device name, timezone, volume limits). That thin file is the only
   firmware file you keep. The core is **pulled from GitHub at compile time**,
   see its `packages:` block.
3. **First flash over USB**, then updates go wireless:
   ```
   esphome run waveshare-va.yaml
   ```
   Or drop both files into the ESPHome dashboard's `/config/esphome/` and hit
   Install.
4. In Home Assistant: the new ESPHome device appears, open **Configure** and
   assign an Assist pipeline.
5. Say "Alexa". The ring should go violet. To use another wake word, change
   **Wake word** on the device page (see [Wake words](#wake-words)).

The example config tracks `main` with `refresh: 0s`, so every build fetches the
latest version. To update, just run `esphome run waveshare-va.yaml` (or
**Install** in the dashboard). After upgrading ESPHome itself, run
`esphome clean waveshare-va.yaml` first if the build fails.

## Documentation

The [Wiki](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki)
has the full guide:

- **[Installation](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/Installation)**: first flash, Home Assistant setup, updating.
- **[Configuration](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/Configuration)**: every substitution and every Home Assistant entity.
- **[Audio architecture](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/Audio-architecture)**: the shared-I2S two-bus design, in depth.
- **[LED ring](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/LED-ring)**: the state machine and every ring effect.
- **[Hardware](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/Hardware)**: pinout, I2C map, and sourced gotchas.
- **[Troubleshooting](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/Troubleshooting)** and **[FAQ](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/wiki/FAQ)**.

## How the shared I2S bus is handled

The board wires the **ES8311 (DAC) and the ES7210 (ADC) to the same BCLK/LRCLK
pins**, and only one device can drive those clocks. ESPHome also cannot run a
single I2S bus full-duplex: a microphone and a speaker on one bus each try to
init the port, and the second fails with "Parent bus is busy".

The layout that works, all on **stock ESPHome components**:

- **Two I2S buses** (two ports) over the shared pins. The **mic bus is the I2S
  master**: it is always capturing for the wake word, so it drives BCLK/LRCLK/MCLK
  continuously. The **speaker bus is a slave** that reads the mic's clock, so it
  never needs a port of its own to master.
- The ES8311 and ES7210 are stock and slave to the mic's clock.
- The mic is pinned to **16-bit** (the i2s_audio default is 32-bit); since the
  mic is master it sets the frame's slot width, and a 32-bit frame against the
  16-bit DAC comes out as noise.

This gives simultaneous capture + playback with no custom component. The
annotated config is in the audio section of `base/core.yaml`.

## Repository layout

```
waveshare-va.yaml          # YOUR config: copy + edit this (pulls the rest from GitHub)
secrets.example.yaml       # copy to secrets.yaml
base/
  core.yaml                # the always-on core, pulled as a remote package
  display.yaml             # optional: the 2.8inch touch display
  release.yaml             # shared part of the ready-made firmware
release-display.yaml       # ready-made firmware with the display
release-core.yaml          # ready-made firmware without a display
install/index.html         # install page (ESP Web Tools) on GitHub Pages
components/
  cst3530/                 # touch controller of the display (Rev 1.1), not in ESPHome
ci/
  build.yaml               # CI build target: compiles the core from the working tree
.github/workflows/
  build.yml                # compiles the firmware on every push and pull request
docs/
  HARDWARE.md              # pinout, I2C map, gotchas
scripts/
  validate.py              # offline YAML check (syntax, substitutions, duplicate ids)
  esplog.py                # stream device logs over the native API
skill/
  waveshare-esp32-s3-audio/  # Claude Code skill: pinout + hard-won gotchas
```

## Configuration

Everything worth changing day to day is a Home Assistant entity, not a config
edit: mic gain, LED brightness, the ring animation per assistant phase
(Listening / Thinking / Replying effect), the wake word and its sensitivity,
night mode, wake sound, boot sound, microphone mute.

What lives in `waveshare-va.yaml`:

| Substitution | Default | What it does |
|---|---|---|
| `name` / `friendly_name` | `waveshare-va` / `Waveshare Voice` | Device name. Changing `name` re-creates every entity in HA. |
| `posix_timezone` | `CET-1CEST,...` | Clock zone in POSIX form (the device has no IANA database). DST automatic. |
| `volume_min` / `volume_max` | `0.4` / `0.8` | Media player clamps, because the onboard amp distorts near the top. |
| `hidden_ssid` | `false` | `true` enables `fast_connect` for a hidden SSID. |
| `boot_sound_file` | repo `startup.mp3` | The connect-to-HA chime. Any URL or local MP3/FLAC/WAV. |

Pins and the audio format are substitutions too (in `base/core.yaml`), but you
should not need them unless you are porting to another board.

### Wake words

The firmware ships four wake word models: `alexa` (the default), `okay_nabu`,
`hey_jarvis` and `hey_mycroft`. Pick the active one with the **Wake
word** select on the device page in Home Assistant; the choice is stored on the
device and survives reboots and updates.

If several satellites share one Home Assistant instance and can hear the same
voice, give each a different wake word. Home Assistant ignores a wake word it
already heard within the last two seconds, on any device, so two satellites on
the same word cancel each other out (the second one beeps and does nothing).

### Night mode

The **Night mode** switch dims the ring to **Night LED Ring Brightness** and
skips the wake beep and the boot chime. Replies, timers and the alarm still
play at the normal volume. It is a plain switch, so any automation can drive it:

```yaml
automation:
  - alias: Voice satellite night mode
    triggers:
      - trigger: sun
        event: sunset
        id: "on"
      - trigger: sun
        event: sunrise
        id: "off"
    actions:
      - action: switch.turn_{{ trigger.id }}
        target:
          entity_id: switch.waveshare_va_night_mode
```

### Touch display (optional)

The board takes the **Waveshare 2.8inch Capacitive Touch LCD** (240x320,
ST7789) on its 18-pin LCD connector. Plug the ribbon in with the board
unpowered. Without a display nothing changes: the display lives in its own
package, `base/display.yaml`, and the core does not need it.

To use it, build from a local clone of this repository and add the package to
your `waveshare-va.yaml`:

```yaml
packages:
  core: !include base/core.yaml
  display: !include base/display.yaml
```

`display.yaml` loads the touch driver from the local `components/` folder, so
it is not pulled as a remote package.

What it shows (landscape, 320x240). Swipe from the clock to the right for the
system page, to the left for the values page; the gear opens the settings.
Other pages fall back to the clock after a minute without a touch.

- **Clock**: the time, the date without the year (a German weekday and month
  with the year do not fit), the alarm of the core when it is on, and the
  running timers at the bottom: one with its name, several as `T1 4:59  T2 12:00`.
- **State of the assistant**: a small icon in the top right corner on every
  page - a blue microphone while it listens, amber dots while it thinks, a
  green speaker while it answers, red for no connection to the assistant, no
  Wi-Fi or an error, a grey crossed-out microphone while the microphone is
  muted. Nothing when it is idle.
- **Conversation**: what the satellite heard and the answer of the assistant,
  a few seconds after the end. When the assistant asks something and listens
  again for the answer (for example "Turn on the lamp? Say yes or no."), a
  hint to answer by voice is added. Only a text with a "?" counts as a
  question. There are no yes/no buttons, because a satellite cannot send a
  text to the assistant.
- **Volume**: a bar for two seconds whenever the volume changes.
- **Ringing**: a ringing timer or alarm fills the screen with a red "Stop";
  a touch anywhere silences it, like the Play/Pause button.
- **System** (swipe right): the firmware version (the release, or the build
  date of a build of your own), Wi-Fi with signal, IP, uptime, free memory, the
  connection to the assistant. **Firmware-Update** (top right) looks for a new
  release: without one a message says that the latest firmware is installed;
  with one it asks Yes/No and installs it over the air, with the progress on
  the screen (ready-made firmware only).
- **Values** (swipe left): four tiles with numbers that something else writes
  into the entities **Value 1** ... **Value 4** (see below).
- **Settings** (gear): volume, display brightness by day and at night, LED
  ring brightness, microphone off, night mode, wake sound, language.

Entities of the display:

| Entity | Default | What it does |
|---|---|---|
| **Display language** | `Deutsch` | Language of the date and the texts on the screen: Deutsch, English, Русский. The satellite does not learn the language of the assistant, so it is set here. The first value comes from the substitution `display_language`. |
| **Display brightness** | 100 % | Backlight during the day. |
| **Display night brightness** | 0 % | Backlight while **Night mode** is on; 0 turns it off. A touch lights the display for 10 seconds, and it lights by itself while you talk to the assistant or a timer rings. |

**Values from ioBroker.** ioBroker.assistant shows the entities of the
satellite as states, and writing such a state sets the entity. So a script
fills the values page; the labels, units and decimals are substitutions in
your `waveshare-va.yaml`:

```yaml
substitutions:
  value_1_label: "Außen"
  value_1_unit: "°C"
  value_1_decimals: "1"
```

```js
// ioBroker JavaScript: copy states to the values page of the satellite "Gast"
const target = 'assistant.0.satellites.Gast.controls.';
const values = {
    value_1: 'hm-rpc.0.ABC123.1.TEMPERATURE', // your states
    value_2: 'mqtt.0.livingroom.humidity',
};
for (const [slot, source] of Object.entries(values)) {
    const copy = val => setState(target + slot, Number(val)); // ack false = a command to the device
    on({ id: source, change: 'ne' }, obj => copy(obj.state.val));
    getState(source, (err, state) => state && copy(state.val));
}
```

A value of -999999 (the start value) is shown as "–".

**Which touch controller?** The display exists in two versions. **Rev 1.1**
(V2, printed on the back) has a **CST3530** at I2C 0x58, which ESPHome does
not support; `components/cst3530` is a driver for it, written after
Waveshare's `esp_lcd_touch_cst3530` (Apache-2.0). The older version has a
**CST328** at 0x1A: replace the `touchscreen:` block in `display.yaml` with
ESPHome's own `platform: cst328` (not tested here).

Touch points are logged at DEBUG level (`touch x=… y=…`), which helps when
checking the orientation.

### Custom wake word models

Any of the four slots can be replaced with any microWakeWord v2 model by
extending it by `id` in your own config:

```yaml
micro_wake_word:
  models:
    - id: !extend alexa
      model: https://example.com/my_wake_word.json
      # probability_cutoff: 0.85   # optional, overrides the manifest default
```

The **Wake word sensitivity** presets are relative to each model's default
cutoff (its manifest value, or `probability_cutoff:` above): *Slightly
sensitive* uses the default, *Moderately* and *Very sensitive* lower it step by
step. For exact control, set the select to **Custom** and use the **Wake word
threshold** slider, from 0.50 to 0.99. It applies to whichever wake word is
active. Lower values trigger more easily and give more false activations. On
first boot the slider starts at the active model's default cutoff; the models
differ (0.90 for Alexa, 0.97 for Okay Nabu), so revisit it after changing the
wake word.

## Claude Code skill

This repo ships a [Claude Code](https://claude.com/claude-code) skill at
[`skill/waveshare-esp32-s3-audio/`](skill/waveshare-esp32-s3-audio/SKILL.md):
the pinout, the shared-I2S constraint, and the gotchas that cost real debugging
time. Install it user-wide so any session picks it up:

```bash
cp -r skill/waveshare-esp32-s3-audio ~/.claude/skills/
```

## Credits

- **[jensenbox](https://github.com/jensenbox/waveshare-esp32-s3-audio)**: the
  ESP-master I2S layout for this board that the audio setup is based on.
- **ESPHome**: everything the firmware is built out of.
- **[Home Assistant Voice PE](https://github.com/esphome/home-assistant-voice-pe)**:
  the sounds, and the phase/ducking model the LED state machine follows.
- **[clemenszwerg](https://github.com/clemenszwerg)**: diagnosed the ESPHome
  2026.10 build break and found the `const` fix ([#11](https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va/issues/11)).
