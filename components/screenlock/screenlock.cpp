#include "screenlock.h"

#include <cstdio>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "mbedtls/sha256.h"

namespace esphome {
namespace screenlock {

static const char *const TAG = "screenlock";

static const uint32_t COLOR_BG = 0x0B1118;
static const uint32_t COLOR_KEY = 0x18212B;
static const uint32_t COLOR_KEY_PRESSED = 0x1F4F70;
static const uint32_t COLOR_TEXT = 0xFFFFFF;
static const uint32_t COLOR_DIM = 0x8A9AAC;
static const uint32_t COLOR_WRONG = 0xEF4444;

static const char *const GLYPH_BACKSPACE = "\U000F006E";
static const char *const GLYPH_CLOSE = "\U000F0156";

static const uint8_t STORE_VERSION = 1;
// the keypad goes after this time without a key
static const uint32_t KEYPAD_TIMEOUT_MS = 30000;
// after this many wrong PINs in a row the keypad waits
static const int MAX_FAILED = 5;
static const uint32_t LOCKOUT_MS = 60000;
// "wrong PIN" stays this long
static const uint32_t WRONG_MS = 1500;
// the display goes back to the clock after this time without a touch (base/display.yaml)
static const uint32_t CLOCK_FALLBACK_MS = 60000;
static const char *const HASH_PREFIX = "satellite-display:";

static ESPPreferenceObject pref;

static const char *const T_WRONG[] = {"PIN falsch", "Wrong PIN", "Неверный PIN"};
static const char *const T_LOCKOUT[] = {"Gesperrt: %d s", "Locked: %d s", "Блокировка: %d с"};

static std::string sha256_hex(const std::string &text) {
  unsigned char out[32];
  mbedtls_sha256(reinterpret_cast<const unsigned char *>(text.data()), text.size(), out, 0);
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", out[i]);
  return std::string(hex, 64);
}

static void *user_data(int key) { return reinterpret_cast<void *>(static_cast<intptr_t>(key)); }

ScreenLock &get() {
  static ScreenLock instance;
  return instance;
}

void ScreenLock::setup(const Fonts &fonts, ReportCallback report) {
  this->fonts_ = fonts;
  this->report_cb_ = std::move(report);
  pref = global_preferences->make_preference<Stored>(0x51C0C7A1u);
  Stored s{};
  if (pref.load(&s) && s.version == STORE_VERSION) {
    s.hash[64] = 0;
    this->enabled_ = s.enabled && std::strlen(s.hash) == 64;
    this->hash_ = s.hash;
    this->length_ = s.length;
    this->mode_ = static_cast<Relock>(s.mode <= 2 ? s.mode : 0);
    this->minutes_ = s.minutes > 0 ? s.minutes : 5;
  }
  // after a restart a locked display stays locked
  this->locked_ = this->enabled_;
  ESP_LOGI(TAG, "Screen lock %s", this->enabled_ ? "on, locked" : "off");
}

void ScreenLock::save_() {
  Stored s{};
  s.version = STORE_VERSION;
  s.enabled = this->enabled_;
  s.length = static_cast<uint8_t>(this->length_);
  s.mode = static_cast<uint8_t>(this->mode_);
  s.minutes = static_cast<uint16_t>(this->minutes_);
  std::strncpy(s.hash, this->hash_.c_str(), 64);
  s.hash[64] = 0;
  pref.save(&s);
  global_preferences->sync();
}

void ScreenLock::configure(bool enabled, const std::string &pin_hash, int length, int mode, int minutes) {
  bool valid = pin_hash.size() == 64 && length >= 4 && length <= 6;
  bool on = enabled && valid;
  if (enabled && !valid) ESP_LOGW(TAG, "Screen lock: no valid PIN, the lock stays off");
  Relock m = static_cast<Relock>(mode >= 0 && mode <= 2 ? mode : 0);
  int min = minutes > 0 && minutes <= 1440 ? minutes : 5;
  bool changed = on != this->enabled_ || (on && (pin_hash != this->hash_ || length != this->length_)) ||
                 m != this->mode_ || min != this->minutes_;
  bool was_on = this->enabled_;
  this->enabled_ = on;
  if (on) {
    this->hash_ = pin_hash;
    this->length_ = length;
  }
  this->mode_ = m;
  this->minutes_ = min;
  if (changed) {
    this->save_();
    ESP_LOGI(TAG, "Screen lock %s (relock %d, %d min)", on ? "on" : "off", static_cast<int>(m), min);
  }
  // switched on: lock at once; switched off: no keypad, no lock
  if (on && !was_on) this->locked_ = true;
  if (!on) {
    this->locked_ = false;
    this->hide_keypad_();
  }
  this->report_();
}

void ScreenLock::set_locked(bool locked) {
  if (!this->enabled_) return;
  if (locked == this->locked_) {
    this->report_();
    return;
  }
  this->locked_ = locked;
  if (!locked) {
    this->failed_ = 0;
    this->hide_keypad_();
  }
  ESP_LOGI(TAG, "Screen %s", locked ? "locked" : "unlocked");
  this->report_();
}

void ScreenLock::report_() {
  if (this->report_cb_) this->report_cb_(this->locked(), this->failed_);
}

void ScreenLock::on_clock_touched() {
  if (!this->locked() || this->keypad_ != nullptr) return;
  this->show_keypad_();
}

void ScreenLock::show_keypad_() {
  this->entry_.clear();
  this->keypad_since_ = millis();
  lv_obj_t *k = lv_obj_create(lv_layer_top());
  this->keypad_ = k;
  lv_obj_set_size(k, 320, 240);
  lv_obj_set_pos(k, 0, 0);
  lv_obj_set_style_bg_color(k, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(k, 0, 0);
  lv_obj_set_style_radius(k, 0, 0);
  lv_obj_set_style_pad_all(k, 0, 0);
  lv_obj_remove_flag(k, LV_OBJ_FLAG_SCROLLABLE);
  // a touch beside the keys does not reach the page below
  lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(k, LV_OBJ_FLAG_GESTURE_BUBBLE);

  this->stars_ = lv_label_create(k);
  lv_obj_set_style_text_font(this->stars_, this->fonts_.digits, 0);
  lv_obj_set_style_text_color(this->stars_, lv_color_hex(COLOR_TEXT), 0);
  lv_obj_align(this->stars_, LV_ALIGN_TOP_MID, 0, 4);

  this->message_ = lv_label_create(k);
  lv_obj_set_style_text_font(this->message_, this->fonts_.text, 0);
  lv_obj_set_style_text_color(this->message_, lv_color_hex(COLOR_WRONG), 0);
  lv_label_set_text(this->message_, "");
  lv_obj_align(this->message_, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(this->message_, LV_OBJ_FLAG_HIDDEN);

  // 1 2 3 / 4 5 6 / 7 8 9 / close 0 backspace
  static const int KEYS[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 0, 10};
  for (int i = 0; i < 12; i++) {
    int key = KEYS[i];
    lv_obj_t *b = lv_button_create(k);
    lv_obj_set_size(b, 96, 42);
    lv_obj_set_pos(b, 8 + (i % 3) * 104, 50 + (i / 3) * 47);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_KEY), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_KEY_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_t *l = lv_label_create(b);
    if (key <= 9) {
      char d[2] = {static_cast<char>('0' + key), 0};
      lv_obj_set_style_text_font(l, this->fonts_.digits, 0);
      lv_obj_set_style_text_color(l, lv_color_hex(COLOR_TEXT), 0);
      lv_label_set_text(l, d);
    } else {
      lv_obj_set_style_text_font(l, this->fonts_.icons, 0);
      lv_obj_set_style_text_color(l, lv_color_hex(COLOR_DIM), 0);
      lv_label_set_text(l, key == 10 ? GLYPH_BACKSPACE : GLYPH_CLOSE);
    }
    lv_obj_center(l);
    lv_obj_add_event_cb(
        b, [](lv_event_t *e) { get().on_key(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)))); },
        LV_EVENT_CLICKED, user_data(key));
  }
  this->show_entry_();
}

void ScreenLock::hide_keypad_() {
  if (this->keypad_ != nullptr) lv_obj_delete(this->keypad_);
  this->keypad_ = nullptr;
  this->stars_ = nullptr;
  this->message_ = nullptr;
  this->entry_.clear();
}

void ScreenLock::show_entry_() {
  if (this->stars_ == nullptr) return;
  uint32_t now = millis();
  bool lockout = this->lockout_until_ != 0 && static_cast<int32_t>(this->lockout_until_ - now) > 0;
  bool wrong = this->wrong_since_ != 0 && now - this->wrong_since_ < WRONG_MS;
  if (lockout || wrong) {
    char buf[40];
    if (lockout) snprintf(buf, sizeof(buf), T_LOCKOUT[this->lang_], int((this->lockout_until_ - now + 999) / 1000));
    else snprintf(buf, sizeof(buf), "%s", T_WRONG[this->lang_]);
    lv_label_set_text(this->message_, buf);
    lv_obj_remove_flag(this->message_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(this->stars_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(this->message_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(this->stars_, LV_OBJ_FLAG_HIDDEN);
  // a star per digit, a line for each one still to come
  std::string s;
  for (int i = 0; i < this->length_; i++) {
    if (i) s += ' ';
    s += i < static_cast<int>(this->entry_.size()) ? "*" : "_";
  }
  lv_label_set_text(this->stars_, s.c_str());
}

void ScreenLock::on_key(int key) {
  if (this->keypad_ == nullptr) return;
  uint32_t now = millis();
  this->keypad_since_ = now;
  if (key == 11) {
    this->hide_keypad_();
    return;
  }
  if (this->lockout_until_ != 0 && static_cast<int32_t>(this->lockout_until_ - now) > 0) return;
  this->wrong_since_ = 0;
  if (key == 10) {
    if (!this->entry_.empty()) this->entry_.pop_back();
  } else if (key >= 0 && key <= 9 && static_cast<int>(this->entry_.size()) < this->length_) {
    this->entry_ += static_cast<char>('0' + key);
  }
  this->show_entry_();
  // the PIN is checked as soon as it has its length: no OK key
  if (static_cast<int>(this->entry_.size()) == this->length_) this->check_();
}

void ScreenLock::check_() {
  bool ok = sha256_hex(HASH_PREFIX + this->entry_) == this->hash_;
  this->entry_.clear();
  if (ok) {
    ESP_LOGI(TAG, "PIN right, unlocked");
    this->set_locked(false);
    return;
  }
  this->failed_++;
  this->wrong_since_ = millis();
  ESP_LOGW(TAG, "Wrong PIN (%d in a row)", this->failed_);
  if (this->failed_ % MAX_FAILED == 0) this->lockout_until_ = millis() + LOCKOUT_MS;
  this->show_entry_();
  this->report_();
}

void ScreenLock::tick(uint32_t ms_since_touch, bool clock_by_itself, int lang) {
  this->lang_ = lang >= 0 && lang <= 2 ? lang : 0;
  uint32_t now = millis();
  if (this->lockout_until_ != 0 && static_cast<int32_t>(this->lockout_until_ - now) <= 0) this->lockout_until_ = 0;
  if (this->keypad_ != nullptr) {
    this->show_entry_();
    if (now - this->keypad_since_ > KEYPAD_TIMEOUT_MS) this->hide_keypad_();
  }
  if (!this->enabled_ || this->locked_) return;
  switch (this->mode_) {
    case Relock::IDLE:
      if (ms_since_touch > static_cast<uint32_t>(this->minutes_) * 60000u) this->set_locked(true);
      break;
    case Relock::CLOCK:
      // back on the clock by itself, or a minute untouched on the clock
      if (clock_by_itself || ms_since_touch > CLOCK_FALLBACK_MS) this->set_locked(true);
      break;
    case Relock::MANUAL:
      break;
  }
}

}  // namespace screenlock
}  // namespace esphome
