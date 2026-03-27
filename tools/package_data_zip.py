#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import zipfile


STORED_SUFFIXES = {
    ".bmp",
    ".png",
    ".ogg",
    ".mp3",
    ".wav",
}

ALLOWED_FILES = {
    "cptf.txt",
    "datatable.dat",
    "global_parameter_list.txt",
    "gpl_list_size.txt",
    "icon.png",
    "project_settings.txt",
    "scriptfile.tsl",
    "tilemaps.dat",
    "tilesets.dat",
}

ALLOWED_DIRS = {
    "paths",
    "prefabs",
    "sound_fx",
    "sprites",
    "streams",
}


def should_include(relative_path: Path) -> bool:
    if relative_path.name in ALLOWED_FILES:
        return True

    if len(relative_path.parts) >= 2 and relative_path.parts[0] in ALLOWED_DIRS:
        return True

    return False


def compression_for(path: Path) -> int:
    if path.suffix.lower() in STORED_SUFFIXES:
        return zipfile.ZIP_STORED
    return zipfile.ZIP_DEFLATED


def main() -> int:
    parser = argparse.ArgumentParser(description="Build WizBall data.zip asset archive.")
    parser.add_argument("--root", required=True, help="Repository root.")
    parser.add_argument("--output", required=True, help="Output zip path.")
    args = parser.parse_args()

    root_dir = Path(args.root).resolve()
    data_root = root_dir / "wizball" / "wizball"
    archive_root = data_root.parent
    output_path = Path(args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(output_path, "w") as zf:
        for source in sorted(data_root.rglob("*")):
            relative_path = source.relative_to(data_root)
            if not source.is_file() or not should_include(relative_path):
                continue
            arcname = source.relative_to(archive_root).as_posix()
            zf.write(source, arcname=arcname, compress_type=compression_for(source))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
