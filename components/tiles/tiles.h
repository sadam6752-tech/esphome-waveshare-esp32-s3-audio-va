#pragma once

// Tile pages of the display (docs/TILES.md): up to 4 pages with up to 8 tiles each on a grid of 2 columns x 4 rows.
// ioBroker sends the layout and the values over the actions tile_* (base/display.yaml), a touch goes back as the
// event esphome.tile_input. The firmware keeps nothing: after a reconnect ioBroker sends everything again.

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <lvgl.h>

namespace esphome {
namespace tiles {

static const int PAGES = 4;
static const int SLOTS = 8;
static const size_t HISTORY = 60;

enum class Size : uint8_t { S, Q, W, L };
enum class Type : uint8_t { NONE, VALUE, GAUGE, BAR, CHART, SWITCH, DIMMER, BUTTON, THERMOSTAT };

struct Fonts {
  const lv_font_t *label;   // tile name, ~15 px
  const lv_font_t *small;   // percent of a dimmer, ~18 px
  const lv_font_t *medium;  // value in small and wide tiles, ~22 px
  const lv_font_t *large;   // value in square and large tiles, ~34 px
  const lv_font_t *icons;   // Material Design Icons
  const lv_font_t *icons_small;  // - / + of a square thermostat, ~20 px
};

// page 1..4, slot 0..7, input "switch" / "level" / "press" / "setpoint"
using InputCallback = std::function<void(int page, int slot, const char *input, float value)>;

struct Tile {
  Type type{Type::NONE};
  Size size{Size::S};
  int col{0};
  int row{0};
  int decimals{1};
  float min{0};
  float max{100};
  std::string unit;
  float value{NAN};
  bool on{false};
  // thermostat: target temperature, its step, and whether the relay state is known (shows the dot)
  float setpoint{NAN};
  float step{0.5f};
  bool has_state{false};
  // colour of the value, the arc, the bar and the line (0xRRGGBB); 0 = the colours of the theme
  uint32_t color{0};
  lv_obj_t *box{nullptr};
  lv_obj_t *label{nullptr};
  lv_obj_t *value_label{nullptr};
  lv_obj_t *arc{nullptr};
  lv_obj_t *bar{nullptr};
  lv_obj_t *line{nullptr};
  lv_obj_t *sw{nullptr};
  lv_obj_t *slider{nullptr};
  lv_obj_t *setpoint_label{nullptr};
  lv_obj_t *dot{nullptr};
  // sends the setpoint a moment after the last press of - / +
  lv_timer_t *timer{nullptr};
  int line_w{0};
  int line_h{0};
  std::vector<float> history;
  std::vector<lv_point_precise_t> points;
};

class Tiles {
 public:
  void setup(lv_obj_t *page_1, lv_obj_t *page_2, lv_obj_t *page_3, lv_obj_t *page_4, const Fonts &fonts,
             InputCallback on_input);

  // Places one tile (replaces what was in the slot). False, with a warning in the log, when it does not fit.
  bool set(int page, int slot, int col, int row, const std::string &size, const std::string &type,
           const std::string &label, const std::string &unit, const std::string &icon, int decimals, float min,
           float max);
  void set_value(int page, int slot, float value);
  void set_text(int page, int slot, const std::string &text);
  void set_state(int page, int slot, bool on);
  // Colour of the value and its arc, bar or line (0xRRGGBB), e.g. red below a threshold; 0 = the theme again.
  void set_color(int page, int slot, int color);
  // Target temperature of a thermostat and the step of its - / + buttons.
  void set_setpoint(int page, int slot, float value, float step);
  void set_history(int page, int slot, const std::vector<float> &points);
  // Removes the tiles of a page, 0 = of all pages.
  void clear(int page);

  // True when the page (1..4) has a tile.
  bool page_used(int page) const;
  // The next page with tiles from `page` (0 = the clock) in direction +1 / -1; 0 = none (the clock for -1).
  int next_page(int page, int dir) const;

  // LVGL event handlers (index = (page - 1) * SLOTS + slot)
  void on_box_clicked(int index);
  void on_switch_changed(int index);
  void on_slider_moved(int index);
  void on_slider_released(int index);
  // - (dir -1) or + (dir +1) of a thermostat
  void on_step(int index, int dir);
  void on_setpoint_timer(int index);

 protected:
  Tile *tile_(int page, int slot);
  void remove_(Tile &t);
  void show_value_(Tile &t);
  void show_state_(Tile &t);
  void show_setpoint_(Tile &t);
  void show_color_(Tile &t);
  void draw_chart_(Tile &t);
  void send_(int index, const char *input, float value);

  lv_obj_t *pages_[PAGES]{};
  Tile tiles_[PAGES][SLOTS];
  Fonts fonts_{};
  InputCallback on_input_;
};

// The one instance, used by the lambdas of base/display.yaml.
Tiles &get();

}  // namespace tiles
}  // namespace esphome
