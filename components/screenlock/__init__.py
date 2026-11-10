# Screen lock of the display: a PIN keypad in front of everything but the clock, set from ioBroker
# (ioBroker.satellite-display, action lock_config of base/display.yaml). The code is screenlock.h / screenlock.cpp;
# this file only loads it. Format: docs/TILES.md, section "Screen lock".
import esphome.config_validation as cv

CODEOWNERS = []
DEPENDENCIES = ["lvgl"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass
