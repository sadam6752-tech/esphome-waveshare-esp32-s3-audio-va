"""Fill the tile pages of a satellite with a demo layout and print the touches (docs/TILES.md).

Stands in for ioBroker.satellite-display while testing the firmware: it connects next to ioBroker.assistant,
sends two pages with the touchable tile types (and, on 2.4.0 and newer, two more with group, level, battery and
status) and confirms each touch the way the adapter will (switch -> tile_state, level -> tile_value), so the screen
behaves as with the adapter. Stop with Ctrl+C; the tiles stay until the
next restart of the board or the next tile_clear.

    pip install aioesphomeapi
    python scripts/tiles_demo.py 192.168.1.246            # unencrypted API (this firmware)
    python scripts/tiles_demo.py 192.168.1.246 --key ...  # with an API encryption key
    python scripts/tiles_demo.py 192.168.1.246 --clear    # remove all tiles and exit
"""

import argparse
import asyncio
import math
import random

from aioesphomeapi import APIClient

# page, slot, col, row, size, type, label, unit, icon, decimals, min, max
LAYOUT = [
    (1, 0, 0, 0, "q", "gauge", "Temperatur", "°C", "", 1, 10, 30),
    (1, 1, 1, 0, "q", "bar", "Luftfeuchte", "%", "", 0, 0, 100),
    (1, 2, 0, 2, "w", "chart", "Leistung", "W", "", 0, 0, 0),
    (1, 3, 0, 3, "w", "dimmer", "Licht", "", "", 0, 0, 100),
    (2, 0, 0, 0, "l", "chart", "Außentemperatur · 24 h", "°C", "", 1, 0, 0),
    (2, 1, 0, 2, "q", "switch", "Kaffeemaschine", "", "coffee", 0, 0, 0),
    (2, 2, 1, 2, "s", "button", "Szene Kino", "", "movie", 0, 0, 0),
    (2, 3, 1, 3, "s", "value", "Fenster", "", "", 0, 0, 0),
]

# the tiles with items (firmware 2.4.0+, action tile_item); same columns as LAYOUT
LAYOUT_ITEMS = [
    (3, 0, 0, 0, "l", "group", "Temperaturen", "", "", 1, 0, 0),
    (3, 1, 0, 2, "q", "level", "Wassertank", "%", "", 0, 0, 100),
    (3, 2, 1, 2, "q", "battery", "Bordbatterie", "%", "", 0, 0, 100),
    (4, 0, 0, 0, "w", "battery", "Hausbatterie", "%", "", 0, 0, 0),
    (4, 1, 0, 1, "w", "group", "", "", "", 1, 0, 0),
    (4, 2, 0, 2, "q", "status", "Landstrom", "V", "flash", 0, 0, 0),
    (4, 3, 1, 2, "s", "status", "Fenster", "", "window-open", 0, 0, 0),
    (4, 4, 1, 3, "s", "battery", "Handy", "%", "", 0, 0, 0),
]
# page, slot, index, label, unit, decimals
ITEMS = [
    (3, 0, 0, "Außen", "°C", 1),
    (3, 0, 1, "Innen", "°C", 1),
    (3, 0, 2, "Wasser", "°C", 1),
    (3, 0, 3, "Kühlschrank", "°C", 1),
    (3, 1, 1, "", "l", 0),
    (3, 2, 1, "", "W", 0),
    (4, 0, 1, "", "W", 0),
    (4, 1, 0, "PV", "W", 0),
    (4, 1, 1, "Netz", "W", 0),
    (4, 1, 2, "Haus", "W", 0),
]


async def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=6053)
    parser.add_argument("--key", default=None, help="API encryption key (only when the firmware sets one)")
    parser.add_argument("--clear", action="store_true", help="remove all tiles and exit")
    args = parser.parse_args()

    client = APIClient(args.host, args.port, None, noise_psk=args.key, client_info="tiles-demo")
    await client.connect(login=True)
    _, services = await client.list_entities_services()
    actions = {s.name: s for s in services}
    missing = [n for n in ("tile_set", "tile_value", "tile_text", "tile_state", "tile_history", "tile_clear")
               if n not in actions]
    if missing:
        raise SystemExit(f"The firmware has no {', '.join(missing)}: it is older than 2.0.0")

    async def call(name: str, **data) -> None:
        await client.execute_service(actions[name], data)

    await call("tile_clear", page=0)
    if args.clear:
        print("All tiles removed")
        await client.disconnect()
        return

    for page, slot, col, row, size, type_, label, unit, icon, decimals, lo, hi in LAYOUT:
        await call("tile_set", page=page, slot=slot, col=col, row=row, size=size, type=type_, label=label,
                   unit=unit, icon=icon, decimals=decimals, min=float(lo), max=float(hi))

    power = [550 + 40 * math.sin(i / 6) + random.uniform(-15, 15) for i in range(60)]
    outside = [4 + 5 * math.sin((i - 20) / 24 * 2 * math.pi) for i in range(24)]  # ends near the 7.4 below
    await call("tile_value", page=1, slot=0, value=21.1)
    await call("tile_value", page=1, slot=1, value=42.0)
    await call("tile_history", page=1, slot=2, points=power)
    await call("tile_value", page=1, slot=2, value=585.0)
    await call("tile_state", page=1, slot=3, on=True)
    await call("tile_value", page=1, slot=3, value=53.0)
    await call("tile_history", page=2, slot=0, points=outside)
    await call("tile_value", page=2, slot=0, value=7.4)
    await call("tile_state", page=2, slot=1, on=False)
    await call("tile_text", page=2, slot=3, value="zu")

    if "tile_item" in actions:
        for page, slot, col, row, size, type_, label, unit, icon, decimals, lo, hi in LAYOUT_ITEMS:
            await call("tile_set", page=page, slot=slot, col=col, row=row, size=size, type=type_, label=label,
                       unit=unit, icon=icon, decimals=decimals, min=float(lo), max=float(hi))
        for page, slot, index, label, unit, decimals in ITEMS:
            await call("tile_item", page=page, slot=slot, index=index, label=label, unit=unit, decimals=decimals)
        for page, slot, index, value in [(3, 0, 0, 7.4), (3, 0, 1, 21.3), (3, 0, 2, 48.0), (3, 1, 1, 410.0),
                                         (3, 2, 1, -84.0), (4, 0, 1, 1250.0), (4, 1, 0, 3120.0),
                                         (4, 1, 1, -840.0), (4, 1, 2, 2280.0)]:
            await call("tile_item_value", page=page, slot=slot, index=index, value=value)
        await call("tile_item_text", page=3, slot=0, index=3, value="aus")
        await call("tile_item_color", page=3, slot=0, index=2, color=0xF97316)
        await call("tile_value", page=3, slot=1, value=68.0)
        await call("tile_value", page=3, slot=2, value=17.0)
        await call("tile_color", page=3, slot=2, color=0xEF4444)
        await call("tile_state", page=3, slot=2, on=False)
        await call("tile_value", page=4, slot=0, value=82.0)
        await call("tile_state", page=4, slot=0, on=True)
        await call("tile_value", page=4, slot=2, value=231.0)
        await call("tile_state", page=4, slot=2, on=True)
        await call("tile_text", page=4, slot=3, value="Offen")
        await call("tile_state", page=4, slot=3, on=True)
        await call("tile_color", page=4, slot=3, color=0xF59E0B)
        await call("tile_value", page=4, slot=4, value=54.0)
    else:
        print("The firmware has no tile_item (older than 2.4.0): pages 3 and 4 left out")
    print("Demo sent: swipe left from the clock. Touches appear here (Ctrl+C to stop).")

    def on_call(msg) -> None:
        if not msg.is_event or msg.service != "esphome.tile_input":
            return
        d = msg.data
        print(f"touch: page {d.get('page')} slot {d.get('slot')} {d.get('input')} = {d.get('value')}")
        page, slot, value = int(d["page"]), int(d["slot"]), float(d["value"])
        # confirm like the adapter does after writing the state
        if d.get("input") == "switch":
            asyncio.ensure_future(call("tile_state", page=page, slot=slot, on=value > 0))
        elif d.get("input") == "level":
            asyncio.ensure_future(call("tile_value", page=page, slot=slot, value=value))
            asyncio.ensure_future(call("tile_state", page=page, slot=slot, on=value > 0))

    client.subscribe_service_calls(on_call)

    # a live value now and then, so the chart and the gauge move
    try:
        while True:
            await asyncio.sleep(5)
            power.append(power[-1] + random.uniform(-25, 25))
            await call("tile_value", page=1, slot=2, value=round(power[-1]))
            await call("tile_value", page=1, slot=0, value=round(21 + random.uniform(-0.4, 0.4), 1))
    finally:
        await client.disconnect()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
