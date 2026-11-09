# Tile pages of the display: the layout and the values come from ioBroker (ioBroker.satellite-display) over the
# actions tile_* of base/display.yaml. The code is tiles.h / tiles.cpp; this file only loads it. Format: docs/TILES.md.
import esphome.config_validation as cv

CODEOWNERS = []
DEPENDENCIES = ["lvgl"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass
