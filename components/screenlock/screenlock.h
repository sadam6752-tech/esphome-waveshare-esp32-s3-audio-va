#pragma once

// Screen lock (docs/TILES.md, "Screen lock"): while locked the display shows only the clock, a lock icon instead of
// the gear, and the wake word is ignored. A touch opens a keypad; the PIN is checked here against a SHA-256 hash
// that ioBroker sends (the PIN itself never leaves ioBroker). The settings are kept in flash, so the lock works
// after a restart and without ioBroker.

#include <cstdint>
#include <functional>
#include <string>

#include <lvgl.h>

namespace esphome {
namespace screenlock {

// When the display locks itself again after an unlock
enum class Relock : uint8_t { IDLE = 0, CLOCK = 1, MANUAL = 2 };

struct Fonts {
  const lv_font_t *digits;  // keypad and stars, ~34 px
  const lv_font_t *text;    // messages, ~18 px
  const lv_font_t *icons;   // Material Design Icons (backspace, close)
};

// locked, failed attempts in a row
using ReportCallback = std::function<void(bool locked, int failed)>;

class ScreenLock {
 public:
  // Loads the settings from flash; locked at boot when the lock is on.
  void setup(const Fonts &fonts, ReportCallback report);

  // From ioBroker: lock on/off, SHA-256 of "satellite-display:" + PIN as 64 hex digits, its length, relock mode
  // and minutes. Stored in flash when it changed.
  void configure(bool enabled, const std::string &pin_hash, int length, int mode, int minutes);
  // Lock now (true) or unlock (false) from ioBroker or the open lock icon.
  void set_locked(bool locked);

  bool enabled() const { return this->enabled_; }
  bool locked() const { return this->enabled_ && this->locked_; }

  // A touch on the clock page while locked: shows the keypad.
  void on_clock_touched();
  // Every 250 ms from the display interval: keypad timeout, lockout countdown, relock.
  // ms_since_touch: time since the last touch; clock_by_itself: the display just went back to the clock by itself.
  void tick(uint32_t ms_since_touch, bool clock_by_itself, int lang);

  // keypad buttons (0..9, 10 = backspace, 11 = close)
  void on_key(int key);

 protected:
  struct Stored {
    uint8_t version;
    bool enabled;
    uint8_t length;
    uint8_t mode;
    uint16_t minutes;
    char hash[65];
  };

  void save_();
  void show_keypad_();
  void hide_keypad_();
  void show_entry_();
  void check_();
  void report_();

  Fonts fonts_{};
  ReportCallback report_cb_;
  bool enabled_{false};
  bool locked_{false};
  std::string hash_;
  int length_{4};
  Relock mode_{Relock::IDLE};
  int minutes_{5};

  std::string entry_;
  int failed_{0};
  uint32_t lockout_until_{0};
  uint32_t keypad_since_{0};
  uint32_t wrong_since_{0};
  int lang_{0};

  lv_obj_t *keypad_{nullptr};
  lv_obj_t *stars_{nullptr};
  lv_obj_t *message_{nullptr};
};

// The one instance, used by the lambdas of base/display.yaml.
ScreenLock &get();

}  // namespace screenlock
}  // namespace esphome
