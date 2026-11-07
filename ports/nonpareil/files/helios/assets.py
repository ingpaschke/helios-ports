#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Convert upstream Voyager definitions and artwork to native X11 assets."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET

from PIL import Image

MODELS = ("10c", "11c", "12c", "15c", "16c")


def calculator_definition(source, model, destination):
    """Expand the same Nut ROM records as upstream's build_ncd tool."""
    template = source / "ncd" / model / (model + ".ncd.tmpl")
    tree = ET.parse(template)
    root = tree.getroot()
    rom = {}
    notices = ["Nonpareil HP-" + model.upper(),
               root.get("copyright", ""), root.get("license", "")]
    for entry in root.findall("obj_file"):
        filename = template.parent / entry.text.strip()
        data = filename.read_bytes()
        expected = entry.get("hash")
        digest = hashlib.sha256()
        for line in data.decode().splitlines():
            line = line.strip()
            if line.startswith("#"):
                notices.append(line)
                continue
            if not line:
                continue
            address, opcode = line.split(":")
            address, opcode = int(address, 16), int(opcode, 16)
            digest.update(struct.pack("<HH", address, opcode))
            if address in rom and rom[address] != opcode:
                raise ValueError("conflicting ROM record: " + line)
            rom[address] = opcode
        if expected and digest.hexdigest() != expected:
            raise ValueError("ROM checksum mismatch: " + str(filename))
        root.remove(entry)
    for memory in root.findall(".//memory"):
        if memory.get("addr_space") != "inst":
            continue
        if memory.get("banks") != "0":
            raise ValueError("Voyager converter expects unbanked ROM")
        start = int(memory.get("base_addr"), 0)
        size = int(memory.get("size"), 0)
        for address in range(start, start + size):
            ET.SubElement(memory, "loc", addr=hex(address), data=hex(rom[address]))
    ET.indent(tree)
    tree.write(destination, encoding="utf-8", xml_declaration=True)
    destination.with_name("info.txt").write_text("\n".join(notices) + "\n")


def bitmap(image, filename):
    # XBM is readable text and Xlib loads it without another image library.
    image.convert("1", dither=Image.Dither.NONE).save(filename)


def skin(source, model, destination):
    artwork = source / "nui" / "voyager"
    directory = artwork / model
    kml = (directory / (model + ".kml")).read_text()
    title = re.search(r'title\s+"([^"]+)"', kml).group(1)
    background = Image.open(artwork / "voyager-background.png").convert("RGBA")
    background.alpha_composite(Image.open(directory / (model + "-overlay.png")))
    keys = []
    for match in re.finditer(r'button\s+(\d+)\s+image\s+"([^"]+)"\s+offset\s+(\d+)\s+(\d+)\s+end', kml):
        code, filename, x, y = match.groups()
        image = Image.open(directory / filename).convert("RGBA")
        background.alpha_composite(image, (int(x), int(y)))
        keys.append((int(code), int(x), int(y), *image.size))
    if len(keys) != 39:
        raise ValueError("incomplete Voyager keypad: " + model)
    background.convert("RGB").save(destination / "face.ppm")
    lines = ["# Nonpareil Voyager skin; coordinates are pixels.",
             "title " + title,
             "size %d %d" % background.size,
             "display 73 29 305 55",
             "digits 11 27 31 5 4",
             "background 165 175 145", "foreground 0 0 0"]
    lines += ["button %d %d %d %d %d" % key for key in keys]
    for match in re.finditer(r'scancode\s+"([^"]+)"\s+map\s+0\s+(\d+)\s+end', kml):
        lines.append("key %s %s" % match.groups())
    lines += ["key f 42", "key g 43", "key Escape 41"]
    segments = Image.open(artwork / "voyager-segments.png").convert("RGB")
    segments = segments.crop((2, 2, 159, 184))
    for match in re.finditer(r"segment\s+'([a-i])'\s+color\s+(\d+)\s+(\d+)\s+(\d+)\s+end", kml):
        name, red, green, blue = match.groups()
        color = (int(red), int(green), int(blue))
        mask = Image.new("L", segments.size)
        mask.putdata([255 if segments.getpixel((x, y)) == color else 0
                      for y in range(segments.height)
                      for x in range(segments.width)])
        mask = mask.resize((27, 31), Image.Resampling.LANCZOS)
        bitmap(mask.point(lambda value: 255 if value >= 45 else 0),
               destination / ("seg" + name + ".xbm"))
    annunciators = Image.open(artwork / "voyager-annunciators.png").convert("RGBA")
    for match in re.finditer(r'annunciator\s+(\d+)\s+size\s+(\d+)\s+(\d+)\s+offset\s+(\d+)\s+(\d+)\s+end', kml):
        index, width, height, x, y = map(int, match.groups())
        mask = annunciators.getchannel("A").crop((x, y, x + width, y + height))
        bitmap(mask, destination / ("ann%d.xbm" % index))
        lines.append("annunciator %d %d %d %d %d" % (index, x, y, width, height))
    (destination / "skin.txt").write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    for model in MODELS:
        destination = args.destination / model
        destination.mkdir(parents=True, exist_ok=True)
        calculator_definition(args.source, model, destination / "calc.ncd")
        skin(args.source, model, destination)


if __name__ == "__main__":
    main()
