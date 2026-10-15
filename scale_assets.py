
#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

SCALED_ATTRIBUTES = {
    "x", "y", "width", "height",
    "frameX", "frameY", "frameWidth", "frameHeight",
}

OVERRIDES = {
    "ui/title/gf-dance-title": 0.25
}

def scale_png(source, destination, factor):
    destination.parent.mkdir(parents=True, exist_ok=True)

    with Image.open(source) as image:
        skimmedSource = str(source.with_suffix("")).split("assets/")[-1]
        if OVERRIDES.get(skimmedSource, None) != None: 
            factor = OVERRIDES.get(str(skimmedSource))
        size = (max(1, round(image.width * factor)), max(1, round(image.height * factor)),)
        image.resize(size, Image.Resampling.LANCZOS).save(destination, format="PNG")

def scale_xml(source, destination, factor):
    tree = ET.parse(source)
    skimmedSource = str(source.with_suffix("")).split("assets/")[-1]
    if OVERRIDES.get(skimmedSource, None) != None: 
        factor = OVERRIDES.get(str(skimmedSource))
    for element in tree.getroot().iter():
        for attribute in SCALED_ATTRIBUTES:
            value = element.get(attribute)
            if value is None:
                continue

            try:
                element.set(attribute, format(float(value) * factor, ".10g"))
            except ValueError:
                pass

    destination.parent.mkdir(parents=True, exist_ok=True)
    tree.write(destination, encoding="utf-8", xml_declaration=True)

def convert_png(png_path, dry_run=False):
    t3x_path = png_path.with_suffix(".t3x")
    command = ["tex3ds", str(png_path), "-o", str(t3x_path)]

    print("[TEX3DS]", " ".join(command))

    if dry_run:
        return

    try:
        subprocess.run(command, check=True)
    except FileNotFoundError:
        raise RuntimeError("tex3ds was not found. Make sure devkitPro is installed and tex3ds is available in PATH.")
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"tex3ds failed for {png_path}; keeping the PNG.") from error

    if not t3x_path.is_file():
        raise RuntimeError(f"tex3ds did not create {t3x_path}; keeping the PNG.")

    png_path.unlink()
    print("[DELETE]", png_path)

def main():
    parser = argparse.ArgumentParser(description="Scale PNG/XML assets and convert PNGs to 3DS T3X.")
    parser.add_argument("folder", type=Path, help="Source folder")
    parser.add_argument("factor", type=float, help="Scale factor, e.g. 0.5")
    parser.add_argument("--romfs", type=Path, default=Path("romfs"), help="Destination root (default: ./romfs)",)
    parser.add_argument("--dry-run", action="store_true", help="List planned operations without modifying files",)
    args = parser.parse_args()

    source = args.folder.resolve()
    romfs = args.romfs.resolve()

    if not source.is_dir():
        parser.error(f"Folder does not exist: {source}")
    if args.factor <= 0:
        parser.error("Scale factor must be greater than zero.")

    destination_root = romfs / source.name

    if destination_root == source or source in destination_root.parents:
        parser.error("Destination must not be inside the source folder.")

    print(f"Source: {source}")
    print(f"Destination: {destination_root}")
    print(f"Scale factor: {args.factor}")

    pngs = []

    for file in sorted(source.rglob("*")):
        if not file.is_file():
            continue

        relative = file.relative_to(source)
        destination = destination_root / relative
        suffix = file.suffix.lower()

        try:
            if suffix == ".png":
                if not args.dry_run:
                    scale_png(file, destination, args.factor)
                pngs.append(destination)

            elif suffix == ".xml":
                if not args.dry_run:
                    scale_xml(file, destination, args.factor)

            else:
                if not args.dry_run:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(file, destination)

        except (OSError, ET.ParseError) as error:
            print(f"[ERROR] {file}: {error}", file=sys.stderr)
            return 1

    for png in pngs:
        try:
            convert_png(png, args.dry_run)
        except RuntimeError as error:
            print(f"[ERROR] {error}", file=sys.stderr)
            return 1

    print("Done!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
