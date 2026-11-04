#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::cst3530 {

class CST3530Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(GPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void continue_setup_();
  i2c::ErrorCode command_(uint32_t cmd);
  i2c::ErrorCode read_(uint32_t cmd, uint8_t *data, size_t len);

  GPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint32_t firmware_{};
  uint8_t tries_{};
  bool setup_complete_{};
};

}  // namespace esphome::cst3530
