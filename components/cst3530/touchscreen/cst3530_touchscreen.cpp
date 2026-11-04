// Protocol from Waveshare's esp_lcd_touch_cst3530 1.0.0 (Apache-2.0): 32-bit commands sent MSB first,
// a touch report of up to 32 bytes, a clear command after every read.
#include "cst3530_touchscreen.h"
#include "esphome/core/log.h"

namespace esphome::cst3530 {

static const char *const TAG = "cst3530.touchscreen";

static const uint32_t CMD_READ = 0xD0070000;
static const uint32_t CMD_INFO = 0xD0030000;
static const uint32_t CMD_CLEAR = 0xD00002AB;
static const uint8_t MAX_POINTS = 5;
static const size_t REPORT_SIZE = 32;
static const uint8_t INFO_TRIES = 10;

i2c::ErrorCode CST3530Touchscreen::command_(uint32_t cmd) {
  const uint8_t buf[4] = {uint8_t(cmd >> 24), uint8_t(cmd >> 16), uint8_t(cmd >> 8), uint8_t(cmd)};
  return this->write(buf, sizeof(buf));
}

i2c::ErrorCode CST3530Touchscreen::read_(uint32_t cmd, uint8_t *data, size_t len) {
  const uint8_t buf[4] = {uint8_t(cmd >> 24), uint8_t(cmd >> 16), uint8_t(cmd >> 8), uint8_t(cmd)};
  return this->write_read(buf, sizeof(buf), data, len);
}

void CST3530Touchscreen::setup() {
  // The Waveshare driver holds INT low through the reset.
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->digital_write(false);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
  }
  // let the firmware start before the first question
  this->set_timeout(50, [this] {
    if (this->interrupt_pin_ != nullptr) {
      this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    }
    this->continue_setup_();
  });
}

void CST3530Touchscreen::continue_setup_() {
  uint8_t buf[50]{};
  if (this->read_(CMD_INFO, buf, sizeof(buf)) != i2c::ERROR_OK || buf[2] != 0xCA || buf[3] != 0xCA) {
    if (++this->tries_ < INFO_TRIES) {
      this->set_timeout(20, [this] { this->continue_setup_(); });
      return;
    }
    ESP_LOGE(TAG, "No CST3530 answered at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }
  uint16_t res_x = buf[28] | (buf[29] << 8);
  uint16_t res_y = buf[30] | (buf[31] << 8);
  this->firmware_ = buf[32] | (buf[33] << 8) | (buf[34] << 16) | (uint32_t(buf[35]) << 24);
  if (res_x != 0 && res_y != 0) {
    this->x_raw_max_ = res_x;
    this->y_raw_max_ = res_y;
  } else {
    this->x_raw_max_ = this->display_->get_native_width();
    this->y_raw_max_ = this->display_->get_native_height();
  }
  this->command_(CMD_CLEAR);
  this->setup_complete_ = true;
  ESP_LOGD(TAG, "CST3530 ready, %ux%u, firmware 0x%08X", this->x_raw_max_, this->y_raw_max_,
           (unsigned) this->firmware_);
}

void CST3530Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST3530 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u, firmware 0x%08X", this->x_raw_max_, this->y_raw_max_,
                (unsigned) this->firmware_);
}

void CST3530Touchscreen::update_touches() {
  if (!this->setup_complete_) {
    this->skip_update_ = true;
    return;
  }
  uint8_t buf[REPORT_SIZE]{};
  if (this->read_(CMD_READ, buf, sizeof(buf)) != i2c::ERROR_OK) {
    this->status_set_warning();
    this->skip_update_ = true;
    return;
  }
  this->status_clear_warning();
  uint8_t touches = buf[3] & 0x0F;
  uint8_t keys = buf[3] >> 4;
  if (buf[2] == 0xFF && touches > 0 && touches <= MAX_POINTS) {
    uint16_t sum = 0x55;
    for (size_t i = 0; i < size_t(keys + touches) * 5 && 4 + i < REPORT_SIZE; i++) {
      sum += buf[4 + i];
    }
    if (sum != uint16_t(buf[0] | (buf[1] << 8))) {
      ESP_LOGV(TAG, "Checksum mismatch");
      this->skip_update_ = true;
    } else {
      for (uint8_t i = 0; i < touches; i++) {
        size_t idx = size_t(keys + i) * 5;
        if (idx + 8 >= REPORT_SIZE) {
          break;
        }
        uint8_t event = buf[idx + 8] >> 4;
        if (event == 0) {
          continue;  // lifted
        }
        uint8_t id = buf[idx + 8] & 0x0F;
        uint8_t high = buf[idx + 7];
        int16_t x = buf[idx + 4] | ((high & 0x0F) << 8);
        int16_t y = buf[idx + 5] | ((high & 0xF0) << 4);
        this->add_raw_touch_position_(id, x, y, buf[idx + 6]);
      }
    }
  }
  this->command_(CMD_CLEAR);
}

}  // namespace esphome::cst3530
