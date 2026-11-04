#!/usr/bin/env python3
"""Write the GitHub Pages site of a release: one folder per firmware variant with the binaries and a
manifest.json that serves both ESP Web Tools (install page, "parts") and ESPHome's http_request update
("ota" with md5).

    release_manifest.py --version 1.3.0 --release-url URL --artifacts artifacts --site site

artifacts/firmware-<variant>/firmware.{factory,ota}.bin  ->  site/firmware/<variant>/...
"""

import argparse
import hashlib
import json
import shutil
from pathlib import Path

NAMES = {
    "display": "Waveshare Voice with display",
    "core": "Waveshare Voice",
}
SUMMARIES = {
    "display": "ESP32-S3-AUDIO-Board with the Waveshare 2.8inch Capacitive Touch LCD",
    "core": "ESP32-S3-AUDIO-Board without a display",
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--release-url", required=True)
    parser.add_argument("--artifacts", required=True, type=Path)
    parser.add_argument("--site", required=True, type=Path)
    args = parser.parse_args()

    for folder in sorted(args.artifacts.glob("firmware-*")):
        variant = folder.name.removeprefix("firmware-")
        target = args.site / "firmware" / variant
        target.mkdir(parents=True, exist_ok=True)
        for name in ("firmware.factory.bin", "firmware.ota.bin"):
            shutil.copy2(folder / name, target / name)
        md5 = hashlib.md5((target / "firmware.ota.bin").read_bytes()).hexdigest()
        manifest = {
            "name": NAMES.get(variant, variant),
            "version": args.version,
            "home_assistant_domain": "esphome",
            "new_install_prompt_erase": True,
            "builds": [
                {
                    "chipFamily": "ESP32-S3",
                    "ota": {
                        "path": "firmware.ota.bin",
                        "md5": md5,
                        "summary": SUMMARIES.get(variant, ""),
                        "release_url": args.release_url,
                    },
                    "parts": [{"path": "firmware.factory.bin", "offset": 0}],
                }
            ],
        }
        (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(f"{variant}: {args.version}, ota md5 {md5}")


if __name__ == "__main__":
    main()
