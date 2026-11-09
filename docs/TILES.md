# Tile pages: format between firmware and ioBroker

Status: firmware 2.0.0 (unreleased); code in `components/tiles`, actions in `base/display.yaml`.

The display gets up to **4 tile pages**. What is on them is not compiled into the firmware: an ioBroker
adapter sends the layout and the values over the ESPHome Native API, and gets touches back as events.
The firmware stays the same for every house.

The adapter talks to the satellite on its own API connection, next to ioBroker.assistant. The voice
(wake word, audio, TTS, ring) stays with assistant; the adapter never subscribes to the voice assistant.
The API is unencrypted (see `base/core.yaml`), so the adapter only needs the IP address (port 6053).

## Screen and grid

The screen is 320 × 240 (landscape). A page is a grid of **2 columns × 4 rows**, 6 px gap and margin;
one cell is 151 × 52 px.

| Size | Cells (cols × rows) | Pixels    | Example             |
| ---- | ------------------- | --------- | ------------------- |
| `s`  | 1 × 1               | 151 × 52  | small switch        |
| `q`  | 1 × 2               | 151 × 111 | gauge, big value    |
| `w`  | 2 × 1               | 308 × 52  | power with a chart  |
| `l`  | 2 × 2               | 308 × 111 | 24 h chart          |

A page holds up to **8 tiles** (slot 0–7); 4 pages give 32 tiles. A tile is placed by its top left cell
(`col` 0–1, `row` 0–3). A tile that leaves the grid or covers another one is refused with a warning
in the log.

## Tile types

| Type     | Shows                                  | Sizes       | Touch                    |
| -------- | -------------------------------------- | ----------- | ------------------------ |
| `value`  | label, value with unit (or a text)     | s q w l     | none                     |
| `gauge`  | label, arc from min to max, value      | q l         | none                     |
| `bar`    | label, value, bar from min to max      | q w l       | none                     |
| `chart`  | label, value, line of the last values  | w l         | none                     |
| `switch` | label, toggle (lit tile when on)       | s q w       | toggle → `switch`        |
| `dimmer` | label, toggle, slider, percent         | w l         | toggle → `switch`, slider → `level` |
| `button` | icon, label                            | s q w       | press → `press`          |

`tile_set` creates the widgets of the tile; a new `tile_set` for the slot or `tile_clear` deletes them.
A square `switch` shows its icon bottom left; a `button` shows its icon (without one: only the name).
Other types ignore the icon.

## Actions (adapter → satellite)

ESPHome actions have no optional parameters: every parameter is always sent.

| Action          | Parameters | Effect |
| --------------- | ---------- | ------ |
| `tile_set`      | `page` int 1–4, `slot` int 0–7, `col` int, `row` int, `size` string, `type` string, `label` string, `unit` string, `icon` string, `decimals` int, `min` float, `max` float | Places and configures one tile; the value is cleared |
| `tile_value`    | `page`, `slot`, `value` float | Number of `value`, `gauge`, `bar`, `chart`; level 0–100 of `dimmer`. A `chart` also appends it to its line |
| `tile_text`     | `page`, `slot`, `value` string | Shows a text instead of a number (`value` only, e.g. "Offen") |
| `tile_state`    | `page`, `slot`, `on` bool | On/off of `switch` and `dimmer` |
| `tile_history`  | `page`, `slot`, `points` float[] | Replaces the line of a `chart` (at most 60 points, oldest first) |
| `tile_clear`    | `page` int (0 = all pages) | Removes the tiles of a page |

Rules:

- `label` at most 24 characters, `unit` at most 8. The fonts hold Latin, German umlauts, Cyrillic and °.
- `icon` is a name from the fixed icon list below, with or without `mdi:` (the icon font only contains
  compiled glyphs); an unknown name shows no icon and a warning in the log.
- `decimals` 0–3. A value tile shows "–" until it gets a value.
- `tile_text` is not called `text`: ESPHome reserves that name for action variables.
- `min`/`max` are used by `gauge`, `bar` and the scale of `chart` (equal values: the chart scales itself).
- A value for an empty or different-type slot is ignored.
- The firmware does not keep the layout: after every reconnect the adapter sends `tile_clear 0`, all
  `tile_set`, then the current values and histories.
- A `chart` keeps its last 60 values itself, so `tile_history` is only needed after a (re)connect.

## Events (satellite → adapter)

A touch sends the Home Assistant event `esphome.tile_input` with the data:

| Field   | Value |
| ------- | ----- |
| `page`  | 1–4 |
| `slot`  | 0–7 |
| `input` | `switch`, `level` or `press` |
| `value` | `switch`: `1`/`0`; `level`: `0`–`100`; `press`: `1` |

The toggle and the slider change at once on the screen (optimistic); the adapter then writes the
bound state and sends the confirmed value back with `tile_state` / `tile_value`, which corrects the
screen when the device did not follow. The slider sends `level` once when it is released, not while
it moves; a dimmer moved above 0 shows itself on, moved to 0 off.

A tap on a `switch` tile anywhere toggles it; a `dimmer` has its own toggle and slider. A drag on a
slider does not swipe to the next page.

Other API clients (assistant) receive the event too; it carries no meaning for them.

## Pages and navigation

- Fixed pages stay: clock, system, settings, and the dialog overlays of the assistant
  (listening, answer, yes/no), which cover any page and return to it.
- The tile pages replace the pages "values" and "switches". A page without tiles is skipped.
- Swipe left from the clock goes through the tile pages 1 → 4, swipe right goes back.
- Without any tiles the display shows only the clock and the system pages.
- Like every page but the clock, a tile page goes back to the clock after a minute without a touch.

## Icon list

Fixed set of Material Design Icons (as in `components/tiles/tiles.cpp`):
`lightbulb`, `lamp`, `ceiling-light`, `power-plug`, `power`, `fan`, `radiator`, `thermometer`,
`water-percent`, `flash`, `solar-power`, `battery`, `window-closed`, `window-open`, `door`, `garage`,
`blinds`, `lock`, `lock-open`, `bell`, `television`, `movie`, `music`, `speaker`, `coffee`,
`washing-machine`, `dishwasher`, `car`, `home`, `bed`, `sofa`, `shower`, `weather-sunny`,
`weather-night`, `alarm`, `play`, `pause`, `stop`, `check`, `close`.

A new icon needs a new firmware (the glyph in `font_icons` of `base/display.yaml` and the name in
`tiles.cpp`); the adapter offers exactly this list.

## Decisions

- The adapter is **ioBroker.satellite-display**.
- The old entities `Value 1–4`, `Switch 1–4` and their labels, units and decimals (used via
  `assistant.0.satellites.<satellite>.controls.*`) are **removed** together with the pages "values" and
  "switches". This breaks those controls, so the firmware with tiles is released as **2.0.0**.
