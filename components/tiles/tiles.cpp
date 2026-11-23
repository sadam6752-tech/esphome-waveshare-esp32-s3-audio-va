#include "tiles.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace tiles {

static const char *const TAG = "tiles";

// colors (the theme of base/display.yaml)
static const uint32_t COLOR_TILE = 0x18212B;
static const uint32_t COLOR_TILE_ON = 0x0C4A6E;
static const uint32_t COLOR_TILE_PRESSED = 0x1F4F70;
static const uint32_t COLOR_LABEL = 0x8A9AAC;
static const uint32_t COLOR_VALUE = 0xFFFFFF;
static const uint32_t COLOR_ACCENT = 0x38BDF8;
static const uint32_t COLOR_TRACK = 0x2E3A47;
// the relay of a thermostat: heating on / off
static const uint32_t COLOR_HEAT_ON = 0x4CAF50;
static const uint32_t COLOR_HEAT_OFF = 0x4B5563;
// the fill of a battery when ioBroker sends no colour
static const uint32_t COLOR_BATTERY = 0x22C55E;
// icons of the - / + buttons of a thermostat (in font_icons, not in the list of the tiles)
static const char *const GLYPH_MINUS = "\U000F0374";
static const char *const GLYPH_PLUS = "\U000F0415";
// the bolt of a charging battery ("flash", also in font_icons_small)
static const char *const GLYPH_BOLT = "\U000F0241";
// a press of - / + is sent this long after the last one
static const uint32_t SETPOINT_DELAY_MS = 1000;
// the indicator of a switch that is on
static const lv_style_selector_t SELECTOR_CHECKED =
    static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED);

// grid: 2 columns x 4 rows, 6 px margin and gap, a cell is 151 x 52.5 px on a 320 px wide screen
// (111 x 52.5 on the 240 px of the 1.54 inch panel, set_screen_width)
static int g_cell_w = 151;
static int cell_w() { return g_cell_w; }
static int wide_w() { return 2 * g_cell_w + 6; }
static int col_x(int col) { return 6 + col * (g_cell_w + 6); }
// the arc of a tall thermostat (size t): as wide as the cell allows, at most 100 px
static int thermostat_tall_arc() { return std::min(g_cell_w - 16, 100); }
static int row_y(int row) { return 6 + row * 117 / 2; }

// Icons that the font of base/display.yaml contains (font_icons); the same list is in docs/TILES.md.
struct Icon {
  const char *name;
  uint32_t code;
};
static const Icon ICONS[] = {
    {"lightbulb", 0xF0335},     {"lamp", 0xF06B5},          {"ceiling-light", 0xF0769}, {"power-plug", 0xF06A5},
    {"power", 0xF0425},         {"fan", 0xF0210},           {"radiator", 0xF0438},      {"thermometer", 0xF050F},
    {"water-percent", 0xF058E}, {"flash", 0xF0241},         {"solar-power", 0xF0A72},   {"battery", 0xF0079},
    {"window-closed", 0xF05AE}, {"window-open", 0xF05B1},   {"door", 0xF081A},          {"garage", 0xF06D9},
    {"blinds", 0xF00AC},        {"lock", 0xF033E},          {"lock-open", 0xF033F},     {"bell", 0xF009A},
    {"television", 0xF0502},    {"movie", 0xF0381},         {"music", 0xF075A},         {"speaker", 0xF04C3},
    {"coffee", 0xF0176},        {"washing-machine", 0xF072A}, {"dishwasher", 0xF0AAC},  {"car", 0xF010B},
    {"home", 0xF02DC},          {"bed", 0xF02E3},           {"sofa", 0xF04B9},          {"shower", 0xF09A0},
    {"weather-sunny", 0xF0599}, {"weather-night", 0xF0594}, {"alarm", 0xF0020},         {"play", 0xF040A},
    {"pause", 0xF03E4},         {"stop", 0xF04DB},          {"check", 0xF012C},         {"close", 0xF0156},
};

// UTF-8 of an icon name ("mdi:" is allowed in front), empty when unknown
static std::string icon_text(const std::string &name) {
  std::string n = name.compare(0, 4, "mdi:") == 0 ? name.substr(4) : name;
  for (const auto &icon : ICONS) {
    if (n == icon.name) {
      uint32_t c = icon.code;
      char buf[5] = {char(0xF0 | (c >> 18)), char(0x80 | ((c >> 12) & 0x3F)), char(0x80 | ((c >> 6) & 0x3F)),
                     char(0x80 | (c & 0x3F)), 0};
      return buf;
    }
  }
  if (!n.empty()) ESP_LOGW(TAG, "Unknown icon \"%s\"", n.c_str());
  return "";
}

// at most `max` characters of a UTF-8 text, never a part of one (a cut umlaut shows as a box)
static std::string utf8_prefix(const std::string &s, size_t max) {
  size_t i = 0, n = 0;
  while (i < s.size() && n < max) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    if (i + len > s.size()) break;
    i += len;
    n++;
  }
  return s.substr(0, i);
}

static bool parse_size(const std::string &s, Size &size, int &cols, int &rows) {
  if (s == "s") { size = Size::S; cols = 1; rows = 1; }
  else if (s == "q") { size = Size::Q; cols = 1; rows = 2; }
  else if (s == "w") { size = Size::W; cols = 2; rows = 1; }
  else if (s == "l") { size = Size::L; cols = 2; rows = 2; }
  else if (s == "t") { size = Size::T; cols = 1; rows = 4; }
  else if (s == "f") { size = Size::F; cols = 2; rows = 4; }
  else return false;
  return true;
}

static void cells(Size size, int &cols, int &rows) {
  cols = (size == Size::W || size == Size::L || size == Size::F) ? 2 : 1;
  rows = (size == Size::F || size == Size::T) ? 4 : ((size == Size::Q || size == Size::L) ? 2 : 1);
}

static bool parse_type(const std::string &s, Type &type) {
  if (s == "value") type = Type::VALUE;
  else if (s == "gauge") type = Type::GAUGE;
  else if (s == "bar") type = Type::BAR;
  else if (s == "chart") type = Type::CHART;
  else if (s == "switch") type = Type::SWITCH;
  else if (s == "dimmer") type = Type::DIMMER;
  else if (s == "button") type = Type::BUTTON;
  else if (s == "thermostat") type = Type::THERMOSTAT;
  else if (s == "group") type = Type::GROUP;
  else if (s == "level") type = Type::LEVEL;
  else if (s == "battery") type = Type::BATTERY;
  else if (s == "status") type = Type::STATUS;
  else return false;
  return true;
}

// which sizes a type comes in
static bool size_allowed(Type type, Size size) {
  // the tall and the whole-page size are for a thermostat only
  if ((size == Size::T || size == Size::F) && type != Type::THERMOSTAT) return false;
  switch (type) {
    case Type::VALUE: return size != Size::F;
    case Type::GAUGE: return size == Size::Q || size == Size::L;
    case Type::BAR: return size != Size::F;
    case Type::CHART: return size != Size::Q && size != Size::F;
    case Type::SWITCH: return size != Size::L && size != Size::F;
    case Type::DIMMER: return size == Size::W || size == Size::L;
    case Type::BUTTON: return size != Size::L && size != Size::F;
    case Type::THERMOSTAT: return size == Size::Q || size == Size::T || size == Size::L || size == Size::F;
    case Type::GROUP: return size != Size::F;
    case Type::LEVEL: return size == Size::Q || size == Size::L;
    case Type::BATTERY: return size != Size::F;
    case Type::STATUS: return size != Size::L && size != Size::F;
    default: return false;
  }
}

static void no_scroll(lv_obj_t *obj) {
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
}

// a plain filled rectangle (the marks of a tank, the cap of a battery)
static lv_obj_t *make_rect(lv_obj_t *parent, int w, int h, uint32_t color) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_set_size(r, w, h);
  lv_obj_set_style_bg_color(r, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_shadow_width(r, 0, 0);
  lv_obj_set_style_radius(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  no_scroll(r);
  return r;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, text);
  return l;
}

static void *user_data(int index) { return reinterpret_cast<void *>(static_cast<intptr_t>(index)); }
static int event_index(lv_event_t *e) { return static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))); }
// the - / + buttons carry the direction in the user data: index * 2 + (1 for +)
static const int STEP_UP = 1;

static lv_obj_t *make_arc(lv_obj_t *parent, int d) {
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, d, d);
  lv_arc_set_bg_angles(arc, 135, 45);
  lv_arc_set_range(arc, 0, 1000);
  lv_arc_set_value(arc, 0);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(arc, 0, 0);
  return arc;
}

// a - / + button of a thermostat: an icon on a round area that lights up while pressed
static lv_obj_t *make_step_button(lv_obj_t *parent, const lv_font_t *icons, const char *glyph, int code, int w,
                                  int h) {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_TILE_PRESSED), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_radius(b, h / 2, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  no_scroll(b);
  lv_obj_set_ext_click_area(b, 6);
  lv_obj_t *ic = make_label(b, icons, COLOR_LABEL, glyph);
  lv_obj_center(ic);
  lv_obj_add_event_cb(
      b, [](lv_event_t *e) {
        int c = event_index(e);
        get().on_step(c / 2, (c % 2) == STEP_UP ? 1 : -1);
      },
      LV_EVENT_CLICKED, user_data(code));
  return b;
}

Tiles &get() {
  static Tiles instance;
  return instance;
}

void Tiles::set_screen_width(int width) { g_cell_w = std::max(60, (width - 18) / 2); }

void Tiles::setup(lv_obj_t *page_1, lv_obj_t *page_2, lv_obj_t *page_3, lv_obj_t *page_4, const Fonts &fonts,
                  InputCallback on_input) {
  this->pages_[0] = page_1;
  this->pages_[1] = page_2;
  this->pages_[2] = page_3;
  this->pages_[3] = page_4;
  this->fonts_ = fonts;
  this->on_input_ = std::move(on_input);
}

Tile *Tiles::tile_(int page, int slot) {
  if (page < 1 || page > PAGES || slot < 0 || slot >= SLOTS) return nullptr;
  return &this->tiles_[page - 1][slot];
}

void Tiles::remove_(Tile &t) {
  if (t.timer != nullptr) lv_timer_delete(t.timer);
  if (t.box != nullptr) lv_obj_delete(t.box);
  t = Tile();
}

bool Tiles::set(int page, int slot, int col, int row, const std::string &size_name, const std::string &type_name,
                const std::string &label, const std::string &unit, const std::string &icon, int decimals, float min,
                float max) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || this->pages_[page - 1] == nullptr) {
    ESP_LOGW(TAG, "Tile %d/%d: no such page or slot", page, slot);
    return false;
  }
  Size size;
  Type type;
  int cols, rows;
  if (!parse_size(size_name, size, cols, rows)) {
    ESP_LOGW(TAG, "Tile %d/%d: unknown size \"%s\"", page, slot, size_name.c_str());
    return false;
  }
  if (!parse_type(type_name, type)) {
    ESP_LOGW(TAG, "Tile %d/%d: unknown type \"%s\"", page, slot, type_name.c_str());
    return false;
  }
  if (!size_allowed(type, size)) {
    ESP_LOGW(TAG, "Tile %d/%d: a %s tile has no size \"%s\"", page, slot, type_name.c_str(), size_name.c_str());
    return false;
  }
  if (col < 0 || row < 0 || col + cols > 2 || row + rows > 4) {
    ESP_LOGW(TAG, "Tile %d/%d: does not fit the grid at column %d, row %d", page, slot, col, row);
    return false;
  }
  for (int i = 0; i < SLOTS; i++) {
    const Tile &o = this->tiles_[page - 1][i];
    if (i == slot || o.type == Type::NONE) continue;
    int ocols, orows;
    cells(o.size, ocols, orows);
    if (col < o.col + ocols && o.col < col + cols && row < o.row + orows && o.row < row + rows) {
      ESP_LOGW(TAG, "Tile %d/%d: covers tile %d/%d", page, slot, page, i);
      return false;
    }
  }

  this->remove_(*t);
  t->type = type;
  t->size = size;
  t->col = col;
  t->row = row;
  t->decimals = std::max(0, std::min(3, decimals));
  t->min = min;
  t->max = max;
  t->unit = utf8_prefix(unit, 8);
  int index = (page - 1) * SLOTS + slot;
  const std::string icon_glyph = icon_text(icon);

  int w = (cols == 2) ? wide_w() : cell_w();
  int h = row_y(row + rows) - 6 - row_y(row);
  t->width = w;
  t->height = h;
  lv_obj_t *box = lv_obj_create(this->pages_[page - 1]);
  t->box = box;
  lv_obj_set_pos(box, col_x(col), row_y(row));
  lv_obj_set_size(box, w, h);
  lv_obj_set_style_bg_color(box, lv_color_hex(COLOR_TILE), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 10, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_shadow_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 0, 0);
  no_scroll(box);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);

  const Fonts &f = this->fonts_;
  std::string name = utf8_prefix(label, 24);
  bool tall = rows == 2;
  // a small bar or chart: name and value on the first line, the bar or the line below
  bool small_two_lines = size == Size::S && (type == Type::BAR || type == Type::CHART);
  // group, level, battery and status place their name themselves (it starts top left)
  bool own_layout = type == Type::GROUP || type == Type::LEVEL || type == Type::BATTERY || type == Type::STATUS;
  bool one_line = !own_layout &&
                  ((size == Size::S && type != Type::BUTTON && !small_two_lines) ||
                   (size == Size::W && (type == Type::SWITCH || type == Type::DIMMER || type == Type::BUTTON)));

  // the name: top left, or left in the middle on a one-line tile
  if (type != Type::BUTTON) {
    t->label = make_label(box, f.label, COLOR_LABEL, name.c_str());
    lv_label_set_long_mode(t->label, LV_LABEL_LONG_MODE_DOTS);
    if (one_line) {
      int lw = type == Type::DIMMER ? 54 : (type == Type::VALUE ? 64 : w - 18 - 54);
      lv_obj_set_width(t->label, lw);
      lv_obj_align(t->label, LV_ALIGN_LEFT_MID, 9, 0);
    } else {
      int lw = w - 18;
      if (type == Type::DIMMER && tall) lw = w - 18 - 54;
      if (small_two_lines) lw = w - 18 - 62;
      lv_obj_set_width(t->label, lw);
      lv_obj_set_pos(t->label, 9, 6);
    }
  }

  switch (type) {
    case Type::VALUE:
    case Type::BAR:
    case Type::CHART: {
      const lv_font_t *font = tall ? f.large : f.medium;
      t->value_label = make_label(box, font, COLOR_VALUE, "–");
      if (small_two_lines) {
        lv_obj_align(t->value_label, LV_ALIGN_TOP_RIGHT, -9, 2);
      } else if (size == Size::S) {
        lv_obj_align(t->value_label, LV_ALIGN_RIGHT_MID, -9, 0);
      } else if (size == Size::W) {
        lv_obj_align(t->value_label, LV_ALIGN_BOTTOM_LEFT, 9, -4);
      } else if (type == Type::VALUE) {
        lv_obj_align(t->value_label, LV_ALIGN_BOTTOM_LEFT, 9, -8);
      } else {
        lv_obj_set_pos(t->value_label, 9, 22);
      }
      if (type == Type::BAR) {
        lv_obj_t *bar = lv_bar_create(box);
        t->bar = bar;
        lv_bar_set_range(bar, 0, 1000);
        lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        if (size == Size::W) {
          lv_obj_set_size(bar, w - 130 - 12, 8);
          lv_obj_align(bar, LV_ALIGN_RIGHT_MID, -12, 0);
        } else if (size == Size::S) {
          lv_obj_set_size(bar, w - 18, 6);
          lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 9, -9);
        } else {
          lv_obj_set_size(bar, w - 18, 8);
          lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 9, -12);
        }
      }
      if (type == Type::CHART) {
        lv_obj_t *line = lv_line_create(box);
        t->line = line;
        if (size == Size::W) {
          t->line_w = w - 125 - 10;
          t->line_h = h - 16;
          lv_obj_set_pos(line, 125, 8);
        } else if (size == Size::S) {
          t->line_w = w - 18;
          t->line_h = h - 30 - 5;
          lv_obj_set_pos(line, 9, 30);
        } else {
          t->line_w = w - 18;
          t->line_h = h - 64 - 6;
          lv_obj_set_pos(line, 9, 64);
        }
        lv_obj_set_size(line, t->line_w, t->line_h);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_color(line, lv_color_hex(COLOR_ACCENT), 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
        t->history.reserve(HISTORY);
        t->points.reserve(HISTORY);
      }
      break;
    }

    case Type::GAUGE: {
      // below the name with some air, the value inside the arc
      int d = 78;
      lv_obj_t *arc = make_arc(box, d);
      t->arc = arc;
      if (size == Size::Q) lv_obj_set_pos(arc, (w - d) / 2, h - d - 4);
      else lv_obj_set_pos(arc, 20, h - d - 4);
      if (size == Size::Q) {
        t->value_label = make_label(box, f.small, COLOR_VALUE, "–");
        lv_obj_align_to(t->value_label, arc, LV_ALIGN_CENTER, 0, 2);
      } else {
        t->value_label = make_label(box, f.large, COLOR_VALUE, "–");
        lv_obj_align(t->value_label, LV_ALIGN_LEFT_MID, 20 + d + 20, 8);
      }
      break;
    }

    case Type::THERMOSTAT: {
      // the current temperature in the arc, the setpoint between - and +, the relay as a dot top right
      lv_obj_set_width(t->label, w - 18 - 24);
      lv_obj_t *dot = lv_obj_create(box);
      t->dot = dot;
      lv_obj_set_size(dot, 16, 16);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_border_width(dot, 0, 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, -10, 8);
      lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);

      int code = index * 2;
      if (size == Size::T) {
        // half the width, the whole height: the arc at the top, the setpoint under it, - and + side by side below
        int d = thermostat_tall_arc();
        lv_obj_t *arc = make_arc(box, d);
        t->arc = arc;
        lv_obj_set_pos(arc, (w - d) / 2, 30);
        lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
        t->value_label = make_label(box, w >= 140 ? f.medium : f.small, COLOR_VALUE, "–");
        lv_obj_align_to(t->value_label, arc, LV_ALIGN_CENTER, 0, 0);
        int bw = (w - 18) / 2;
        lv_obj_t *minus = make_step_button(box, f.icons, GLYPH_MINUS, code, bw, 44);
        lv_obj_t *plus = make_step_button(box, f.icons, GLYPH_PLUS, code + STEP_UP, bw, 44);
        lv_obj_align(minus, LV_ALIGN_BOTTOM_LEFT, 6, -6);
        lv_obj_align(plus, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
        t->setpoint_label = make_label(box, f.medium, COLOR_VALUE, "–");
        lv_obj_align(t->setpoint_label, LV_ALIGN_TOP_MID, 0, 30 + d + 8);
      } else if (size == Size::Q) {
        // a bigger arc; the target and - / + small, low in the gap at the bottom of the arc
        int d = 82;
        lv_obj_t *arc = make_arc(box, d);
        t->arc = arc;
        lv_obj_set_pos(arc, (w - d) / 2, 21);
        t->value_label = make_label(box, f.small, COLOR_VALUE, "–");
        lv_obj_align_to(t->value_label, arc, LV_ALIGN_CENTER, 0, 0);
        lv_obj_t *minus = make_step_button(box, f.icons_small, GLYPH_MINUS, code, 34, 28);
        lv_obj_t *plus = make_step_button(box, f.icons_small, GLYPH_PLUS, code + STEP_UP, 34, 28);
        lv_obj_align(minus, LV_ALIGN_BOTTOM_LEFT, 2, 0);
        lv_obj_align(plus, LV_ALIGN_BOTTOM_RIGHT, -2, 0);
        t->setpoint_label = make_label(box, f.label, COLOR_LABEL, "–");
        lv_obj_align(t->setpoint_label, LV_ALIGN_BOTTOM_MID, 0, -3);
      } else if (size == Size::F) {
        // the whole page: a big arc with the current temperature, below it - setpoint + across the tile
        int d = std::min(w - 40, h - 86);
        lv_obj_t *arc = make_arc(box, d);
        t->arc = arc;
        lv_obj_set_pos(arc, (w - d) / 2, 30);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
        t->value_label = make_label(box, f.large, COLOR_VALUE, "–");
        lv_obj_align_to(t->value_label, arc, LV_ALIGN_CENTER, 0, 0);
        lv_obj_t *minus = make_step_button(box, f.icons, GLYPH_MINUS, code, 56, 44);
        lv_obj_t *plus = make_step_button(box, f.icons, GLYPH_PLUS, code + STEP_UP, 56, 44);
        lv_obj_align(minus, LV_ALIGN_BOTTOM_LEFT, 8, -8);
        lv_obj_align(plus, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
        t->setpoint_label = make_label(box, f.large, COLOR_VALUE, "–");
        lv_obj_align(t->setpoint_label, LV_ALIGN_BOTTOM_MID, 0, -14);
      } else {
        lv_obj_t *minus = make_step_button(box, f.icons, GLYPH_MINUS, code, 40, 34);
        lv_obj_t *plus = make_step_button(box, f.icons, GLYPH_PLUS, code + STEP_UP, 40, 34);
        int d = 78;
        lv_obj_t *arc = make_arc(box, d);
        t->arc = arc;
        lv_obj_set_pos(arc, 20, h - d - 4);
        t->value_label = make_label(box, f.medium, COLOR_VALUE, "–");
        lv_obj_align_to(t->value_label, arc, LV_ALIGN_CENTER, 0, 2);
        // the right half: - setpoint +
        int mid = 20 + d + (w - 20 - d) / 2;
        lv_obj_align(minus, LV_ALIGN_LEFT_MID, 20 + d + 14, 10);
        lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -12, 10);
        t->setpoint_label = make_label(box, f.large, COLOR_VALUE, "–");
        lv_obj_align(t->setpoint_label, LV_ALIGN_LEFT_MID, mid - 40, 10);
      }
      t->timer = lv_timer_create([](lv_timer_t *tm) { get().on_setpoint_timer(static_cast<int>(
                                     reinterpret_cast<intptr_t>(lv_timer_get_user_data(tm)))); },
                                 SETPOINT_DELAY_MS, user_data(index));
      lv_timer_pause(t->timer);
      break;
    }

    case Type::SWITCH: {
      lv_obj_t *sw = lv_switch_create(box);
      t->sw = sw;
      lv_obj_set_size(sw, 44, 24);
      lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
      lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_ACCENT), SELECTOR_CHECKED);
      // the whole tile is the switch
      lv_obj_remove_flag(sw, LV_OBJ_FLAG_CLICKABLE);
      if (tall) lv_obj_align(sw, LV_ALIGN_BOTTOM_RIGHT, -9, -10);
      else lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -9, 0);
      const std::string &glyph = icon_glyph;
      if (tall && !glyph.empty()) {
        lv_obj_t *ic = make_label(box, f.icons, COLOR_LABEL, glyph.c_str());
        lv_obj_align(ic, LV_ALIGN_BOTTOM_LEFT, 9, -8);
        t->icon = ic;
      }
      lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(box, lv_color_hex(COLOR_TILE_PRESSED), LV_STATE_PRESSED);
      lv_obj_add_event_cb(box, [](lv_event_t *e) { get().on_box_clicked(event_index(e)); }, LV_EVENT_CLICKED,
                          user_data(index));
      break;
    }

    case Type::DIMMER: {
      lv_obj_t *sw = lv_switch_create(box);
      t->sw = sw;
      lv_obj_set_size(sw, 44, 24);
      lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
      lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_ACCENT), SELECTOR_CHECKED);
      lv_obj_set_ext_click_area(sw, 6);
      lv_obj_add_event_cb(sw, [](lv_event_t *e) { get().on_switch_changed(event_index(e)); },
                          LV_EVENT_VALUE_CHANGED, user_data(index));

      lv_obj_t *sl = lv_slider_create(box);
      t->slider = sl;
      lv_slider_set_range(sl, 0, 100);
      lv_obj_set_style_bg_color(sl, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
      lv_obj_set_style_bg_color(sl, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
      lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
      lv_obj_set_ext_click_area(sl, 10);
      // a drag on the slider is not a swipe to the next page
      lv_obj_remove_flag(sl, LV_OBJ_FLAG_GESTURE_BUBBLE);
      lv_obj_add_event_cb(sl, [](lv_event_t *e) { get().on_slider_moved(event_index(e)); }, LV_EVENT_VALUE_CHANGED,
                          user_data(index));
      lv_obj_add_event_cb(sl, [](lv_event_t *e) { get().on_slider_released(event_index(e)); }, LV_EVENT_RELEASED,
                          user_data(index));

      if (tall) {
        lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -9, 6);
        t->value_label = make_label(box, f.large, COLOR_VALUE, "–");
        lv_obj_set_pos(t->value_label, 9, 26);
        lv_obj_set_size(sl, w - 36, 10);
        lv_obj_align(sl, LV_ALIGN_BOTTOM_MID, 0, -16);
      } else {
        lv_obj_align(sw, LV_ALIGN_LEFT_MID, 66, 0);
        lv_obj_set_size(sl, w - 120 - 58, 8);
        lv_obj_align(sl, LV_ALIGN_LEFT_MID, 120, 0);
        t->value_label = make_label(box, f.small, COLOR_VALUE, "–");
        lv_obj_set_width(t->value_label, 50);
        lv_obj_set_style_text_align(t->value_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(t->value_label, LV_ALIGN_RIGHT_MID, -9, 0);
      }
      break;
    }

    case Type::BUTTON: {
      const std::string &glyph = icon_glyph;
      lv_obj_t *text = make_label(box, f.label, COLOR_VALUE, name.c_str());
      t->label = text;
      lv_label_set_long_mode(text, LV_LABEL_LONG_MODE_DOTS);
      if (glyph.empty()) {
        lv_obj_set_width(text, w - 18);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
      } else {
        lv_obj_t *ic = make_label(box, f.icons, COLOR_ACCENT, glyph.c_str());
        if (tall) {
          lv_obj_align(ic, LV_ALIGN_CENTER, 0, -14);
          lv_obj_set_width(text, w - 18);
          lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
          lv_obj_align(text, LV_ALIGN_BOTTOM_MID, 0, -14);
        } else {
          lv_obj_align(ic, LV_ALIGN_LEFT_MID, 9, 0);
          lv_obj_set_width(text, w - 9 - 48);
          lv_obj_align(text, LV_ALIGN_LEFT_MID, 48, 0);
        }
      }
      lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(box, lv_color_hex(COLOR_TILE_PRESSED), LV_STATE_PRESSED);
      lv_obj_add_event_cb(box, [](lv_event_t *e) { get().on_box_clicked(event_index(e)); }, LV_EVENT_CLICKED,
                          user_data(index));
      break;
    }

    case Type::GROUP:
      // tall: the title above the rows; flat: no room for it, only the values side by side. The rows come with
      // tile_item (layout_group_).
      if (!tall) lv_obj_add_flag(t->label, LV_OBJ_FLAG_HIDDEN);
      break;

    case Type::LEVEL: {
      // a tank: a vertical bar with marks at a quarter, a half and three quarters, the value right of it, the second
      // amount (tile_item 1) below the value. Under the icon of a large tile the tank starts lower.
      int top = (size == Size::L && !icon_glyph.empty()) ? 38 : 28;
      int tw = size == Size::Q ? 26 : 34;
      int th = h - top - 8;
      lv_obj_t *bar = lv_bar_create(box);
      t->bar = bar;
      lv_bar_set_range(bar, 0, 1000);
      lv_obj_set_size(bar, tw, th);
      lv_obj_set_pos(bar, 9, top);
      lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
      lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
      lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
      lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
      lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
      for (int pct : {25, 50, 75}) {
        lv_obj_t *mark = make_rect(box, 6, 2, COLOR_LABEL);
        lv_obj_set_pos(mark, 9, top + th - th * pct / 100 - 1);
      }
      int x = 9 + tw + 12;
      t->value_label = make_label(box, size == Size::Q ? f.medium : f.large, COLOR_VALUE, "–");
      lv_obj_set_pos(t->value_label, x, top + 2);
      t->item_x = x;
      t->item_y = top + (size == Size::Q ? 34 : 44);
      break;
    }

    case Type::BATTERY: {
      // the symbol: a frame with a cap, the fill inside it (3 px from the frame), the bolt over it while charging
      int bw, bh, by;
      if (size == Size::S) {
        bw = 44;
        bh = 24;
        by = (h - bh) / 2;
      } else if (size == Size::W) {
        bw = 60;
        bh = 28;
        by = (h - bh) / 2;
      } else if (size == Size::Q) {
        bw = w - 18 - 6;
        bh = 36;
        by = 30;
      } else {
        bw = 120;
        bh = 50;
        by = 36;
      }
      lv_obj_t *frame = lv_obj_create(box);
      lv_obj_set_size(frame, bw, bh);
      lv_obj_set_pos(frame, 9, by);
      lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(frame, lv_color_hex(COLOR_LABEL), 0);
      lv_obj_set_style_border_width(frame, 3, 0);
      lv_obj_set_style_radius(frame, 5, 0);
      lv_obj_set_style_shadow_width(frame, 0, 0);
      lv_obj_set_style_pad_all(frame, 0, 0);
      lv_obj_remove_flag(frame, LV_OBJ_FLAG_CLICKABLE);
      no_scroll(frame);
      int ch = bh * 2 / 5;
      lv_obj_t *cap = make_rect(box, 4, ch, COLOR_LABEL);
      lv_obj_set_pos(cap, 9 + bw, by + (bh - ch) / 2);
      lv_obj_t *bar = lv_bar_create(frame);
      t->bar = bar;
      lv_bar_set_range(bar, 0, 1000);
      lv_obj_set_size(bar, bw - 12, bh - 12);
      lv_obj_center(bar);
      lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_BATTERY), LV_PART_INDICATOR);
      lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
      lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
      lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
      t->bolt = make_label(frame, tall ? f.icons : f.icons_small, COLOR_VALUE, GLYPH_BOLT);
      lv_obj_center(t->bolt);
      lv_obj_add_flag(t->bolt, LV_OBJ_FLAG_HIDDEN);
      if (size == Size::S || size == Size::W) {
        // the name and the value right of the symbol; a wide one keeps room for the power at the right
        int x = size == Size::S ? 62 : 82;
        lv_obj_set_width(t->label, w - x - 9 - (size == Size::W ? 90 : 0));
        lv_obj_set_pos(t->label, x, 6);
        t->value_label = make_label(box, f.medium, COLOR_VALUE, "–");
        lv_obj_align(t->value_label, LV_ALIGN_BOTTOM_LEFT, x, -4);
      } else if (size == Size::Q) {
        t->value_label = make_label(box, f.medium, COLOR_VALUE, "–");
        lv_obj_align(t->value_label, LV_ALIGN_BOTTOM_LEFT, 9, -6);
      } else {
        t->value_label = make_label(box, f.large, COLOR_VALUE, "–");
        lv_obj_set_pos(t->value_label, wide_w() / 2 - 4, 30);
      }
      break;
    }

    case Type::STATUS: {
      // like a switch that cannot be touched: the icon, the value and the tile light up while on (show_state_)
      if (size == Size::Q) {
        if (!icon_glyph.empty()) {
          t->icon = make_label(box, f.icons, COLOR_LABEL, icon_glyph.c_str());
          lv_obj_align(t->icon, LV_ALIGN_TOP_MID, 0, 8);
        }
        lv_obj_set_style_text_align(t->label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(t->label, LV_ALIGN_TOP_MID, 0, 46);
        t->value_label = make_label(box, f.medium, COLOR_VALUE, "–");
        lv_obj_set_width(t->value_label, w - 18);
        lv_label_set_long_mode(t->value_label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(t->value_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(t->value_label, LV_ALIGN_BOTTOM_MID, 0, -8);
      } else {
        int x = 9;
        if (!icon_glyph.empty()) {
          t->icon = make_label(box, f.icons, COLOR_LABEL, icon_glyph.c_str());
          lv_obj_align(t->icon, LV_ALIGN_LEFT_MID, 9, 0);
          x = 47;
        }
        lv_obj_set_width(t->label, w - x - 9);
        lv_obj_set_pos(t->label, x, 6);
        t->value_label = make_label(box, f.small, COLOR_VALUE, "–");
        lv_obj_set_width(t->value_label, w - x - 9);
        lv_label_set_long_mode(t->value_label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(t->value_label, LV_ALIGN_BOTTOM_LEFT, x, -5);
      }
      break;
    }

    default:
      break;
  }

  // an icon on the small, wide and large tiles (a square one keeps its layout; button and square switch have theirs;
  // of the own layouts only a large level and a large battery take it, a status draws its own)
  bool takes_icon = !own_layout || ((type == Type::LEVEL || type == Type::BATTERY) && size == Size::L);
  if (type != Type::BUTTON && size != Size::Q && takes_icon && !icon_glyph.empty()) {
    this->place_icon_(*t, icon_glyph, !name.empty(), w, h);
  }

  this->show_value_(*t);
  this->show_state_(*t);
  this->show_setpoint_(*t);
  this->show_color_(*t);
  ESP_LOGD(TAG, "Tile %d/%d: %s %s at column %d, row %d", page, slot, type_name.c_str(), size_name.c_str(), col, row);
  return true;
}

void Tiles::show_value_(Tile &t) {
  if (t.value_label == nullptr) return;
  char buf[40];
  if (t.type == Type::DIMMER) {
    if (std::isnan(t.value)) snprintf(buf, sizeof(buf), "–");
    else snprintf(buf, sizeof(buf), "%d %%", int(t.value + 0.5f));
  } else if (std::isnan(t.value)) {
    snprintf(buf, sizeof(buf), "–");
  } else {
    snprintf(buf, sizeof(buf), "%.*f%s%s", t.decimals, t.value, t.unit.empty() ? "" : " ", t.unit.c_str());
  }
  lv_label_set_text(t.value_label, buf);
  if (t.type == Type::GAUGE && t.size == Size::Q && t.arc != nullptr) {
    lv_obj_align_to(t.value_label, t.arc, LV_ALIGN_CENTER, 0, 2);
  } else if (t.type == Type::THERMOSTAT && t.arc != nullptr) {
    lv_obj_align_to(t.value_label, t.arc, LV_ALIGN_CENTER, 0, t.size == Size::Q ? 0 : 2);
  }

  // a tank or a battery without a scale of its own (min = max) shows percent
  float lo = t.min, hi = t.max;
  if ((t.type == Type::LEVEL || t.type == Type::BATTERY) && lo == hi) {
    lo = 0;
    hi = 100;
  }
  float frac = 0;
  if (!std::isnan(t.value) && hi > lo) frac = std::max(0.0f, std::min(1.0f, (t.value - lo) / (hi - lo)));
  if (t.arc != nullptr) lv_arc_set_value(t.arc, int(frac * 1000));
  if (t.bar != nullptr) lv_bar_set_value(t.bar, int(frac * 1000), LV_ANIM_OFF);
  if (t.slider != nullptr && !std::isnan(t.value) && !lv_obj_has_state(t.slider, LV_STATE_PRESSED)) {
    lv_slider_set_value(t.slider, int(t.value + 0.5f), LV_ANIM_OFF);
  }
}

void Tiles::show_state_(Tile &t) {
  if (t.dot != nullptr) {
    if (t.has_state) {
      lv_obj_set_style_bg_color(t.dot, lv_color_hex(t.on ? COLOR_HEAT_ON : COLOR_HEAT_OFF), 0);
      lv_obj_remove_flag(t.dot, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(t.dot, LV_OBJ_FLAG_HIDDEN);
    }
  }
  // the bolt of a battery: hidden until the first tile_state, then while it charges
  if (t.bolt != nullptr) {
    if (t.has_state && t.on) lv_obj_remove_flag(t.bolt, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(t.bolt, LV_OBJ_FLAG_HIDDEN);
  }
  if (t.type == Type::STATUS) {
    uint32_t lit = t.color != 0 ? t.color : COLOR_ACCENT;
    if (t.icon != nullptr) lv_obj_set_style_text_color(t.icon, lv_color_hex(t.on ? lit : COLOR_LABEL), 0);
    if (t.value_label != nullptr) lv_obj_set_style_text_color(t.value_label, lv_color_hex(t.on ? lit : COLOR_VALUE), 0);
    this->show_background_(t);
    return;
  }
  if (t.sw == nullptr) return;
  if (t.on) lv_obj_add_state(t.sw, LV_STATE_CHECKED);
  else lv_obj_remove_state(t.sw, LV_STATE_CHECKED);
  if (t.type == Type::SWITCH) {
    this->show_background_(t);
    if (t.icon != nullptr) {
      uint32_t lit = t.color != 0 ? t.color : COLOR_ACCENT;
      lv_obj_set_style_text_color(t.icon, lv_color_hex(t.on ? lit : COLOR_LABEL), 0);
    }
  }
}

void Tiles::show_background_(Tile &t) {
  if (t.box == nullptr) return;
  // a switch or a status that is on lights up; else the own background or the theme
  uint32_t bg = t.background != 0 ? t.background : COLOR_TILE;
  if ((t.type == Type::SWITCH || t.type == Type::STATUS) && t.on) bg = COLOR_TILE_ON;
  lv_obj_set_style_bg_color(t.box, lv_color_hex(bg), 0);
}

void Tiles::set_toggle(int page, int slot, bool show) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || t->type != Type::SWITCH || t->sw == nullptr) return;
  t->tile_switch = !show;
  if (show) lv_obj_remove_flag(t->sw, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(t->sw, LV_OBJ_FLAG_HIDDEN);
  // a small or wide switch with an icon: the name stands right of it, as wide as the toggle leaves room
  if (t->label != nullptr && t->icon != nullptr && t->size != Size::Q) {
    int w = t->size == Size::W ? wide_w() : cell_w();
    lv_obj_set_width(t->label, w - 47 - 9 - (show ? 54 : 0));
    lv_obj_align(t->label, LV_ALIGN_LEFT_MID, 47, 0);
  }
  this->show_state_(*t);
}

void Tiles::set_background(int page, int slot, int color) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || t->type == Type::NONE) return;
  uint32_t c = static_cast<uint32_t>(color) & 0xFFFFFF;
  if (c == t->background) return;
  t->background = c;
  this->show_background_(*t);
}

// The icon left in the middle of a small or wide tile (the rest moves right), or top left on a large one (the
// name next to it, the value lower). Without a name the icon takes its place.
void Tiles::place_icon_(Tile &t, const std::string &glyph, bool named, int w, int h) {
  static const int ICON_W = 38;  // icon 30 px and 8 px air
  const Fonts &f = this->fonts_;
  t.icon = make_label(t.box, f.icons, COLOR_LABEL, glyph.c_str());
  if (t.label != nullptr && !named) lv_obj_add_flag(t.label, LV_OBJ_FLAG_HIDDEN);

  if (t.size == Size::L || t.size == Size::F || t.size == Size::T) {
    lv_obj_set_pos(t.icon, 9, 4);
    if (t.label != nullptr) {
      lv_obj_set_width(t.label, lv_obj_get_width(t.label) - ICON_W);
      lv_obj_set_pos(t.label, 9 + ICON_W, 11);
    }
    // the value below the icon, not next to it
    if (t.value_label != nullptr && (t.type == Type::BAR || t.type == Type::CHART)) lv_obj_set_pos(t.value_label, 9, 36);
    if (t.value_label != nullptr && t.type == Type::DIMMER) lv_obj_set_pos(t.value_label, 9, 38);
    if (t.line != nullptr) {
      t.line_h = h - 74 - 6;
      lv_obj_set_pos(t.line, 9, 74);
      lv_obj_set_size(t.line, t.line_w, t.line_h);
    }
    return;
  }

  // small and wide: the icon left in the middle
  lv_obj_align(t.icon, LV_ALIGN_LEFT_MID, 9, 0);
  int x = 9 + ICON_W;
  bool small = t.size == Size::S;
  switch (t.type) {
    case Type::SWITCH:
      if (named) {
        // icon and name: the whole tile is the switch, the toggle goes
        t.tile_switch = true;
        if (t.sw != nullptr) lv_obj_add_flag(t.sw, LV_OBJ_FLAG_HIDDEN);
        if (t.label != nullptr) {
          lv_obj_set_width(t.label, w - x - 9);
          lv_obj_align(t.label, LV_ALIGN_LEFT_MID, x, 0);
        }
      }
      break;
    case Type::VALUE:
      if (t.label != nullptr) {
        if (small) lv_obj_align(t.label, LV_ALIGN_LEFT_MID, x, 0);
        else lv_obj_set_pos(t.label, x, 6);
      }
      if (t.value_label != nullptr && !small) lv_obj_align(t.value_label, LV_ALIGN_BOTTOM_LEFT, x, -4);
      break;
    case Type::BAR:
    case Type::CHART:
      if (t.label != nullptr) {
        lv_obj_set_width(t.label, lv_obj_get_width(t.label) - ICON_W);
        lv_obj_set_pos(t.label, x, 6);
      }
      if (small) {
        if (t.bar != nullptr) {
          lv_obj_set_size(t.bar, w - x - 9, 6);
          lv_obj_align(t.bar, LV_ALIGN_BOTTOM_LEFT, x, -9);
        }
        if (t.line != nullptr) {
          t.line_w = w - x - 9;
          lv_obj_set_pos(t.line, x, 30);
          lv_obj_set_size(t.line, t.line_w, t.line_h);
        }
      } else if (t.value_label != nullptr) {
        lv_obj_align(t.value_label, LV_ALIGN_BOTTOM_LEFT, x, -4);
      }
      break;
    case Type::DIMMER:
      // wide: icon, name, toggle, slider, percent
      if (t.label != nullptr) lv_obj_align(t.label, LV_ALIGN_LEFT_MID, x, 0);
      if (t.sw != nullptr) lv_obj_align(t.sw, LV_ALIGN_LEFT_MID, (named ? 66 : 9) + ICON_W, 0);
      if (t.slider != nullptr) {
        int sx = (named ? 120 : 63) + ICON_W;
        lv_obj_set_size(t.slider, w - sx - 58, 8);
        lv_obj_align(t.slider, LV_ALIGN_LEFT_MID, sx, 0);
      }
      break;
    default:
      break;
  }
}

void Tiles::show_setpoint_(Tile &t) {
  if (t.setpoint_label == nullptr) return;
  char buf[40];
  if (std::isnan(t.setpoint)) {
    snprintf(buf, sizeof(buf), "–");
  } else {
    // as many decimals as the step needs (0.5 -> 1), at least those of the tile
    int dec = t.decimals;
    if (t.step < 1 && dec < 1) dec = 1;
    if (t.step < 0.1f && dec < 2) dec = 2;
    snprintf(buf, sizeof(buf), "%.*f%s%s", dec, t.setpoint, t.unit.empty() ? "" : " ", t.unit.c_str());
  }
  lv_label_set_text(t.setpoint_label, buf);
  if (t.size == Size::T) {
    lv_obj_align(t.setpoint_label, LV_ALIGN_TOP_MID, 0, 30 + thermostat_tall_arc() + 8);
  } else if (t.size == Size::Q) {
    lv_obj_align(t.setpoint_label, LV_ALIGN_BOTTOM_MID, 0, -3);
  } else if (t.size == Size::F) {
    lv_obj_align(t.setpoint_label, LV_ALIGN_BOTTOM_MID, 0, -14);
  } else {
    int d = 78, w = wide_w();
    int mid = 20 + d + (w - 20 - d) / 2;
    lv_obj_update_layout(t.setpoint_label);
    lv_obj_align(t.setpoint_label, LV_ALIGN_LEFT_MID, mid - lv_obj_get_width(t.setpoint_label) / 2, 10);
  }
}

void Tiles::draw_chart_(Tile &t) {
  if (t.line == nullptr) return;
  size_t n = t.history.size();
  t.points.clear();
  if (n >= 2) {
    float lo = t.min, hi = t.max;
    if (!(hi > lo)) {
      lo = *std::min_element(t.history.begin(), t.history.end());
      hi = *std::max_element(t.history.begin(), t.history.end());
    }
    if (hi - lo < 1e-6f) {
      lo -= 1;
      hi += 1;
    }
    for (size_t i = 0; i < n; i++) {
      float v = std::max(lo, std::min(hi, t.history[i]));
      lv_point_precise_t p;
      p.x = int(i * (t.line_w - 2) / (n - 1)) + 1;
      p.y = int((t.line_h - 2) - (v - lo) / (hi - lo) * (t.line_h - 3) + 0.5f) + 1;
      t.points.push_back(p);
    }
  }
  lv_line_set_points(t.line, t.points.data(), t.points.size());
}

void Tiles::set_value(int page, int slot, float value) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || t->type == Type::NONE || t->type == Type::SWITCH || t->type == Type::BUTTON ||
      t->type == Type::GROUP)
    return;
  t->value = value;
  if (t->type == Type::DIMMER && !std::isnan(value)) t->value = std::max(0.0f, std::min(100.0f, value));
  this->show_value_(*t);
  if (t->type == Type::CHART && !std::isnan(value)) {
    if (t->history.size() >= HISTORY) t->history.erase(t->history.begin());
    t->history.push_back(value);
    this->draw_chart_(*t);
  }
}

void Tiles::set_text(int page, int slot, const std::string &text) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || (t->type != Type::VALUE && t->type != Type::STATUS) || t->value_label == nullptr) return;
  lv_label_set_text(t->value_label, utf8_prefix(text, 32).c_str());
}

void Tiles::set_state(int page, int slot, bool on) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || (t->type != Type::SWITCH && t->type != Type::DIMMER && t->type != Type::THERMOSTAT &&
                       t->type != Type::BATTERY && t->type != Type::STATUS))
    return;
  t->on = on;
  t->has_state = true;
  this->show_state_(*t);
}

void Tiles::show_color_(Tile &t) {
  bool own = t.color != 0;
  lv_color_t accent = lv_color_hex(own ? t.color : COLOR_ACCENT);
  // the colour of a group is that of its title; its rows have their own (tile_item_color)
  if (t.type == Type::GROUP) {
    if (t.label != nullptr) lv_obj_set_style_text_color(t.label, lv_color_hex(own ? t.color : COLOR_LABEL), 0);
    return;
  }
  // a status shows its colour only while it is on
  if (t.type == Type::STATUS) {
    this->show_state_(t);
    return;
  }
  // the value takes the colour only when it is set; the theme writes it white
  if (t.value_label != nullptr && t.type != Type::DIMMER) {
    lv_obj_set_style_text_color(t.value_label, own ? accent : lv_color_hex(COLOR_VALUE), 0);
  }
  if (t.arc != nullptr) lv_obj_set_style_arc_color(t.arc, accent, LV_PART_INDICATOR);
  // a battery without a colour of its own is green
  if (t.bar != nullptr) {
    lv_color_t fill = (t.type == Type::BATTERY && !own) ? lv_color_hex(COLOR_BATTERY) : accent;
    lv_obj_set_style_bg_color(t.bar, fill, LV_PART_INDICATOR);
  }
  if (t.line != nullptr) lv_obj_set_style_line_color(t.line, accent, 0);
  // toggles and sliders of switches and dimmers take the main colour too
  if (t.sw != nullptr) lv_obj_set_style_bg_color(t.sw, accent, SELECTOR_CHECKED);
  if (t.slider != nullptr) lv_obj_set_style_bg_color(t.slider, accent, LV_PART_INDICATOR);
}

void Tiles::set_color(int page, int slot, int color) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || t->type == Type::NONE) return;
  uint32_t c = static_cast<uint32_t>(color) & 0xFFFFFF;
  if (c == t->color) return;
  t->color = c;
  this->show_color_(*t);
  // the icon of a lit tile switch takes the colour too
  if (t->type == Type::SWITCH) this->show_state_(*t);
}

void Tiles::set_setpoint(int page, int slot, float value, float step) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || t->type != Type::THERMOSTAT) return;
  if (step > 0 && !std::isnan(step)) t->step = step;
  // while - / + are being pressed the screen keeps what the user set
  if (t->timer != nullptr && !lv_timer_get_paused(t->timer)) return;
  t->setpoint = value;
  this->show_setpoint_(*t);
}

void Tiles::set_history(int page, int slot, const std::vector<float> &points) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || t->type != Type::CHART) return;
  t->history.clear();
  size_t start = points.size() > HISTORY ? points.size() - HISTORY : 0;
  for (size_t i = start; i < points.size(); i++) {
    if (!std::isnan(points[i])) t->history.push_back(points[i]);
  }
  this->draw_chart_(*t);
}

Item *Tiles::item_(int page, int slot, int index) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || index < 0 || index >= ITEMS || !t->items[index].used) return nullptr;
  return &t->items[index];
}

void Tiles::set_item(int page, int slot, int index, const std::string &label, const std::string &unit, int decimals) {
  Tile *t = this->tile_(page, slot);
  if (t == nullptr || index < 0 || index >= ITEMS) return;
  // a group has up to 4 rows; a level and a battery one second amount (index 1), a small battery no room for it
  bool group = t->type == Type::GROUP;
  bool second = (t->type == Type::LEVEL || (t->type == Type::BATTERY && t->size != Size::S)) && index == 1;
  if (!group && !second) return;
  const Fonts &f = this->fonts_;
  bool tall = t->size == Size::Q || t->size == Size::L;
  Item &it = t->items[index];
  if (!it.used) {
    it.used = true;
    if (group) {
      it.base = COLOR_VALUE;
      it.label = make_label(t->box, f.label, COLOR_LABEL, "");
      lv_label_set_long_mode(it.label, LV_LABEL_LONG_MODE_DOTS);
      it.value_label = make_label(t->box, t->size == Size::W ? f.medium : f.small, it.base, "–");
      // side by side the columns are narrow: a long value ends in dots; in a row it stands right-aligned
      if (!tall) lv_label_set_long_mode(it.value_label, LV_LABEL_LONG_MODE_DOTS);
    } else if (t->type == Type::LEVEL) {
      it.base = COLOR_LABEL;
      it.value_label = make_label(t->box, f.small, it.base, "–");
      lv_obj_set_pos(it.value_label, t->item_x, t->item_y);
    } else {
      // the power of a battery
      it.base = COLOR_VALUE;
      if (t->size == Size::W) {
        it.value_label = make_label(t->box, f.medium, it.base, "–");
        lv_obj_align(it.value_label, LV_ALIGN_RIGHT_MID, -9, 0);
      } else if (t->size == Size::Q) {
        it.value_label = make_label(t->box, f.small, it.base, "–");
        lv_obj_align(it.value_label, LV_ALIGN_BOTTOM_RIGHT, -9, -8);
      } else {
        it.value_label = make_label(t->box, f.medium, it.base, "–");
        lv_obj_set_pos(it.value_label, wide_w() / 2 - 4, 72);
      }
    }
  }
  if (it.label != nullptr) lv_label_set_text(it.label, utf8_prefix(label, 24).c_str());
  it.unit = utf8_prefix(unit, 8);
  it.decimals = std::max(0, std::min(3, decimals));
  it.value = NAN;
  it.color = 0;
  lv_obj_set_style_text_color(it.value_label, lv_color_hex(it.base), 0);
  this->show_item_(it);
  if (group) this->layout_group_(*t);
}

void Tiles::show_item_(Item &it) {
  if (it.value_label == nullptr) return;
  char buf[40];
  if (std::isnan(it.value)) snprintf(buf, sizeof(buf), "–");
  else snprintf(buf, sizeof(buf), "%.*f%s%s", it.decimals, it.value, it.unit.empty() ? "" : " ", it.unit.c_str());
  lv_label_set_text(it.value_label, buf);
}

// The rows of a group: in a tall tile one under the other (name left, value right), in a flat one side by side
// (name over value). Done again for every new row, so the rows always share the tile evenly.
void Tiles::layout_group_(Tile &t) {
  int n = 0;
  for (int i = 0; i < ITEMS; i++) {
    if (t.items[i].used) n = i + 1;
  }
  if (n == 0) return;
  const Fonts &f = this->fonts_;
  int w = t.width, h = t.height;
  if (t.size == Size::Q || t.size == Size::L) {
    bool titled = t.label != nullptr && lv_label_get_text(t.label)[0] != '\0';
    int top = titled ? 26 : 6;
    int row_h = (h - top - 4) / n;
    int lh = lv_font_get_line_height(f.label);
    int vh = lv_font_get_line_height(f.small);
    int label_w = w - 18 - (t.size == Size::Q ? 68 : 110);
    for (int i = 0; i < n; i++) {
      Item &it = t.items[i];
      if (!it.used) continue;
      int y = top + i * row_h;
      lv_obj_set_width(it.label, label_w);
      lv_obj_align(it.label, LV_ALIGN_TOP_LEFT, 9, y + (row_h - lh) / 2);
      lv_obj_align(it.value_label, LV_ALIGN_TOP_RIGHT, -9, y + (row_h - vh) / 2);
    }
  } else {
    int col_w = (w - 18) / n;
    for (int i = 0; i < n; i++) {
      Item &it = t.items[i];
      if (!it.used) continue;
      int x = 9 + i * col_w;
      lv_obj_set_width(it.label, col_w - 4);
      lv_obj_align(it.label, LV_ALIGN_TOP_LEFT, x, 6);
      lv_obj_set_width(it.value_label, col_w - 4);
      lv_obj_align(it.value_label, LV_ALIGN_BOTTOM_LEFT, x, -5);
    }
  }
}

void Tiles::set_item_value(int page, int slot, int index, float value) {
  Item *it = this->item_(page, slot, index);
  if (it == nullptr) return;
  it->value = value;
  this->show_item_(*it);
}

void Tiles::set_item_text(int page, int slot, int index, const std::string &text) {
  Item *it = this->item_(page, slot, index);
  if (it == nullptr || it->value_label == nullptr) return;
  lv_label_set_text(it->value_label, utf8_prefix(text, 32).c_str());
}

void Tiles::set_item_color(int page, int slot, int index, int color) {
  Item *it = this->item_(page, slot, index);
  if (it == nullptr || it->value_label == nullptr) return;
  it->color = static_cast<uint32_t>(color) & 0xFFFFFF;
  lv_obj_set_style_text_color(it->value_label, lv_color_hex(it->color != 0 ? it->color : it->base), 0);
}

void Tiles::clear(int page) {
  for (int p = 1; p <= PAGES; p++) {
    if (page != 0 && page != p) continue;
    for (int s = 0; s < SLOTS; s++) this->remove_(this->tiles_[p - 1][s]);
  }
}

bool Tiles::page_used(int page) const {
  if (page < 1 || page > PAGES) return false;
  for (int s = 0; s < SLOTS; s++) {
    if (this->tiles_[page - 1][s].type != Type::NONE) return true;
  }
  return false;
}

int Tiles::next_page(int page, int dir) const {
  for (int p = page + dir; p >= 1 && p <= PAGES; p += dir) {
    if (this->page_used(p)) return p;
  }
  return 0;
}

void Tiles::send_(int index, const char *input, float value) {
  int page = index / SLOTS + 1, slot = index % SLOTS;
  ESP_LOGI(TAG, "Touch on tile %d/%d: %s %g", page, slot, input, value);
  if (this->on_input_) this->on_input_(page, slot, input, value);
}

void Tiles::on_box_clicked(int index) {
  if (index < 0 || index >= PAGES * SLOTS) return;
  Tile &t = this->tiles_[index / SLOTS][index % SLOTS];
  if (t.type == Type::SWITCH) {
    // at once on the screen; ioBroker confirms with tile_state
    t.on = !t.on;
    this->show_state_(t);
    this->send_(index, "switch", t.on ? 1 : 0);
  } else if (t.type == Type::BUTTON) {
    this->send_(index, "press", 1);
  }
}

void Tiles::on_switch_changed(int index) {
  if (index < 0 || index >= PAGES * SLOTS) return;
  Tile &t = this->tiles_[index / SLOTS][index % SLOTS];
  if (t.sw == nullptr) return;
  t.on = lv_obj_has_state(t.sw, LV_STATE_CHECKED);
  this->send_(index, "switch", t.on ? 1 : 0);
}

void Tiles::on_slider_moved(int index) {
  if (index < 0 || index >= PAGES * SLOTS) return;
  Tile &t = this->tiles_[index / SLOTS][index % SLOTS];
  if (t.slider == nullptr || t.value_label == nullptr) return;
  char buf[12];
  snprintf(buf, sizeof(buf), "%d %%", int(lv_slider_get_value(t.slider)));
  lv_label_set_text(t.value_label, buf);
}

void Tiles::on_slider_released(int index) {
  if (index < 0 || index >= PAGES * SLOTS) return;
  Tile &t = this->tiles_[index / SLOTS][index % SLOTS];
  if (t.slider == nullptr) return;
  t.value = lv_slider_get_value(t.slider);
  // a light that is dimmed up is on, dimmed to 0 it is off
  bool on = t.value > 0;
  if (on != t.on) {
    t.on = on;
    this->show_state_(t);
  }
  this->send_(index, "level", t.value);
}

void Tiles::on_step(int index, int dir) {
  if (index < 0 || index >= PAGES * SLOTS) return;
  Tile &t = this->tiles_[index / SLOTS][index % SLOTS];
  if (t.type != Type::THERMOSTAT || std::isnan(t.setpoint)) return;
  // to the grid of the step, within min..max
  float v = std::round((t.setpoint + dir * t.step) / t.step) * t.step;
  if (t.max > t.min) v = std::max(t.min, std::min(t.max, v));
  t.setpoint = v;
  this->show_setpoint_(t);
  if (t.timer != nullptr) {
    lv_timer_reset(t.timer);
    lv_timer_resume(t.timer);
  }
}

void Tiles::on_setpoint_timer(int index) {
  if (index < 0 || index >= PAGES * SLOTS) return;
  Tile &t = this->tiles_[index / SLOTS][index % SLOTS];
  if (t.timer != nullptr) lv_timer_pause(t.timer);
  if (t.type == Type::THERMOSTAT && !std::isnan(t.setpoint)) this->send_(index, "setpoint", t.setpoint);
}

}  // namespace tiles
}  // namespace esphome
