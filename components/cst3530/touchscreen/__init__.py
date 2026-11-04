# CST3530 capacitive touch controller (Waveshare 2.8inch Capacitive Touch LCD, Rev 1.1 / V2).
# ESPHome has no platform for it; the protocol follows Waveshare's esp_lcd_touch_cst3530 1.0.0 (Apache-2.0).
from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN

from .. import cst3530_ns

DEPENDENCIES = ["i2c"]

CST3530Touchscreen = cst3530_ns.class_(
    "CST3530Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = (
    touchscreen.touchscreen_schema("50ms")
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CST3530Touchscreen),
            # Any GPIO, also one of an I/O expander: it is only driven low while the chip is reset, as the
            # Waveshare driver does, and read never (the touches are polled).
            cv.Optional(CONF_INTERRUPT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x58))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)
    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))
    if reset_pin := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))
