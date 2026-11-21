#!/usr/bin/env python3
"""Build a firmware version and publish it as an OTA update for Home Assistant.

Usage:
    python3 scripts/publish_ota.py guition-esp32-s3-4848s040 1.0.0
    python3 scripts/publish_ota.py guition-esp32-s3-4848s040 1.0.0 --skip-build

It compiles the panel's build profile with the given version, copies the OTA
image to firmware/<slug>/<slug>.ota.bin and writes firmware/<slug>/native.json,
the manifest the panel's "Firmware: Install Update" button reads. Commit and
push the firmware/ folder afterwards; the panel downloads from there.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RELEASE_URL = "https://github.com/eriklouw-png/ha-esptouch-control"
CHIP_FAMILIES = {"esp32-s3": "ESP32-S3", "esp32-p4": "ESP32-P4"}


class PublishError(RuntimeError):
    pass


def chip_family(slug: str) -> str:
    manifest = json.loads((ROOT / "devices" / "manifest.json").read_text(encoding="utf-8"))
    device = manifest["devices"].get(slug)
    if device is None:
        raise PublishError(f"{slug} is not in devices/manifest.json")
    chip = device["firmware"]["build"]["chip"]
    return chip


def prepare_profile(slug: str) -> Path:
    """Write a local copy of builds/<slug>.yaml that points at this checkout."""
    source = ROOT / "builds" / f"{slug}.yaml"
    if not source.exists():
        raise PublishError(f"{source} not found")
    text = source.read_text(encoding="utf-8")
    text = text.replace("file:///config", f"file://{ROOT}").replace("/config/common", f"{ROOT}/common")
    target = ROOT / "builds" / f".release-{slug}.yaml"
    target.write_text(text, encoding="utf-8")
    return target


def build_name(profile: Path) -> str:
    match = re.search(r'^\s+name:\s*"([^"]+)"', profile.read_text(encoding="utf-8"), re.M)
    if not match:
        raise PublishError("could not find the esphome name in the build profile")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("slug")
    parser.add_argument("version", help='new firmware version, e.g. "1.0.0"; must differ from what the panel runs')
    parser.add_argument("--esphome", default="esphome", help="path to the esphome executable")
    parser.add_argument("--skip-build", action="store_true", help="reuse the last compiled image")
    args = parser.parse_args()

    if not re.fullmatch(r"[0-9A-Za-z][0-9A-Za-z._+-]{0,30}", args.version):
        raise PublishError("version may only contain letters, digits, '.', '_', '+' and '-'")

    chip = chip_family(args.slug)
    profile = prepare_profile(args.slug)
    name = build_name(profile)
    build_dir = ROOT / "builds" / ".esphome" / "build" / name / "build"

    if not args.skip_build:
        secrets = ROOT / "builds" / "secrets.yaml"
        if not secrets.exists():
            secrets.write_text('wifi_ssid: "x"\nwifi_password: "xxxxxxxx"\n', encoding="utf-8")
        subprocess.run(
            [args.esphome, "-s", "firmware_version", args.version, "compile", str(profile)],
            cwd=ROOT / "builds",
            check=True,
        )

    ota = build_dir / "firmware.ota.bin"
    if not ota.exists():
        raise PublishError(f"{ota} not found; compile first")

    out_dir = ROOT / "firmware" / args.slug
    out_dir.mkdir(parents=True, exist_ok=True)
    target = out_dir / f"{args.slug}.ota.bin"
    shutil.copyfile(ota, target)
    md5 = hashlib.md5(target.read_bytes()).hexdigest()

    manifest = {
        "name": f"EspControl {args.slug}",
        "version": args.version,
        "builds": [
            {
                "chipFamily": chip,
                "ota": {
                    "path": target.name,
                    "md5": md5,
                    "summary": f"EspControl {args.version}",
                    "release_url": RELEASE_URL,
                },
            }
        ],
    }
    (out_dir / "native.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Published {args.slug} {args.version}: {target.relative_to(ROOT)} ({target.stat().st_size} bytes, md5 {md5})")
    print(f"Manifest: {(out_dir / 'native.json').relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except PublishError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
