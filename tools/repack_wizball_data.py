#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import Dict, List, Optional


def parse_gpl_lists(gpl_path: Path) -> Dict[str, Dict[str, object]]:
    lists: Dict[str, Dict[str, object]] = {}
    current: Optional[str] = None

    for raw in gpl_path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("#PARAMETER LIST NAME = "):
            current = line.split("=", 1)[1].strip()
            lists[current] = {"extension": "", "entries": []}
            continue
        if current is None:
            continue
        if line.startswith("#PARAMETER LIST EXTENSION = "):
            lists[current]["extension"] = line.split("=", 1)[1].strip()
            continue
        if not line or line.startswith("#"):
            continue
        name = line.split("=", 1)[0].strip()
        entries = lists[current]["entries"]
        assert isinstance(entries, list)
        entries.append(name)

    return lists


def find_file_case_insensitive(directory: Path, wanted_name: str) -> Optional[Path]:
    wanted = wanted_name.lower()
    for child in directory.iterdir():
        if child.is_file() and child.name.lower() == wanted:
            return child
    return None


def run_dat(dat_tool: str, pack_file: Path, input_file: Path, tag: str, compress_c0: bool) -> None:
    cmd = [dat_tool, str(pack_file), "-a", str(input_file), "-k", "-t", tag]
    if compress_c0:
        cmd.append("-c0")
    subprocess.run(cmd, check=True)


def stage_named_copy(staging_dir: Path, source_path: Path, staged_name: str) -> Path:
    target = staging_dir / staged_name
    shutil.copy2(source_path, target)
    return target


def pack_list(
    dat_tool: str,
    gpl_lists: Dict[str, Dict[str, object]],
    list_name: str,
    source_subdir: Path,
    pack_file: Path,
    tag: str,
    compress_c0: bool,
    staging_dir: Path,
) -> None:
    spec = gpl_lists.get(list_name)
    if spec is None:
        raise RuntimeError(f"Missing list '{list_name}' in global_parameter_list.txt")

    extension = str(spec.get("extension", ""))
    if not extension:
        raise RuntimeError(f"List '{list_name}' has no extension in global_parameter_list.txt")

    entries = spec.get("entries", [])
    assert isinstance(entries, list)
    for entry in entries:
        wanted_name = f"{entry}{extension}"
        source_path = find_file_case_insensitive(source_subdir, wanted_name)
        if source_path is None:
            raise RuntimeError(f"Could not resolve '{source_subdir.name}/{wanted_name}' case-insensitively.")
        staged = stage_named_copy(staging_dir, source_path, wanted_name)
        run_dat(dat_tool, pack_file, staged, tag, compress_c0)


def pack_sprite_arb_descriptors(
    dat_tool: str,
    gpl_lists: Dict[str, Dict[str, object]],
    sprite_dir: Path,
    pack_file: Path,
    staging_dir: Path,
) -> None:
    spec = gpl_lists.get("SPRITES")
    if spec is None:
        raise RuntimeError("Missing list 'SPRITES' in global_parameter_list.txt")

    entries = spec.get("entries", [])
    assert isinstance(entries, list)
    for entry in entries:
        if "[ARB]" not in entry:
            continue
        wanted_name = f"{entry}.TXT"
        source_path = find_file_case_insensitive(sprite_dir, wanted_name)
        if source_path is None:
            continue
        staged = stage_named_copy(staging_dir, source_path, wanted_name)
        run_dat(dat_tool, pack_file, staged, "DATA", False)


def remove_if_exists(path: Path) -> None:
    if path.exists():
        path.unlink()


def compile_cptf_from_textfiles(gpl_lists: Dict[str, Dict[str, object]], game_dir: Path, out_cptf: Path) -> None:
    textfiles_spec = gpl_lists.get("TEXTFILES")
    if textfiles_spec is None:
        raise RuntimeError("Missing list 'TEXTFILES' in global_parameter_list.txt")

    extension = str(textfiles_spec.get("extension", ""))
    if not extension:
        raise RuntimeError("List 'TEXTFILES' has no extension in global_parameter_list.txt")

    entries = textfiles_spec.get("entries", [])
    assert isinstance(entries, list)

    source_dir = game_dir / "textfiles"
    compiled_lines: List[str] = []

    for entry in entries:
        wanted_name = f"{entry}{extension}"
        source_path = find_file_case_insensitive(source_dir, wanted_name)
        if source_path is None:
            raise RuntimeError(f"Could not resolve 'textfiles/{wanted_name}' case-insensitively.")

        for raw in source_path.read_text(encoding="utf-8", errors="replace").splitlines():
            stripped = raw.strip(" \t\r\n")
            if not stripped or stripped.startswith("//"):
                continue

            # Match TEXTFILE_load_text() behavior:
            # token 1 = tag, token 2 = rest of line (leading spaces/tabs removed).
            parts = raw.split(None, 1)
            if len(parts) < 2:
                continue
            text = parts[1].lstrip(" \t")
            compiled_lines.append(text if text else "EMPTY LINE!")

    with out_cptf.open("w", encoding="utf-8", newline="\n") as fp:
        fp.write(f"{len(compiled_lines)}\n")
        for line in compiled_lines:
            fp.write(f"{len(line)}\n")
            fp.write(f"{line}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Cross-platform WizBall data repacker.")
    parser.add_argument("--root", default=None, help="Repo root (defaults from script location).")
    parser.add_argument("--outdir", default=None, help="Output directory for pack files.")
    parser.add_argument("--include-core-data", action="store_true", help="Rebuild data.dat from text inputs.")
    parser.add_argument("--dat-tool", default=os.environ.get("WIZBALL_DAT_TOOL", "dat"), help="Path/name of Allegro dat tool.")
    args = parser.parse_args()

    if args.root is None:
        root_dir = Path(__file__).resolve().parents[1]
    else:
        root_dir = Path(args.root).resolve()

    game_dir = root_dir / "wizball" / "wizball"
    editor_dir = root_dir / "wizball" / "editor"
    gpl_path = game_dir / "global_parameter_list.txt"

    if not gpl_path.exists():
        print(f"error: missing {gpl_path}", file=sys.stderr)
        return 1

    if args.outdir:
        out_dir = Path(args.outdir).resolve()
    else:
        out_dir = game_dir

    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Repacking WizBall data files in {game_dir}")
    print(f"Output: {out_dir}")

    gpl_lists = parse_gpl_lists(gpl_path)

    with tempfile.TemporaryDirectory() as tmp:
        staging_dir = Path(tmp)

        data_pack = out_dir / "data.dat"
        if args.include_core_data:
            remove_if_exists(data_pack)
            compiled_cptf = out_dir / "cptf.txt"
            compile_cptf_from_textfiles(gpl_lists, game_dir, compiled_cptf)

            run_dat(args.dat_tool, data_pack, game_dir / "global_parameter_list.txt", "DATA", True)
            run_dat(args.dat_tool, data_pack, game_dir / "gpl_list_size.txt", "DATA", True)
            run_dat(args.dat_tool, data_pack, compiled_cptf, "DATA", True)
            run_dat(args.dat_tool, data_pack, game_dir / "project_settings.txt", "DATA", True)

            # Dat-mode runtime still reads several non-packed core binaries directly.
            # Mirror them into the output location so dat-mode tests stay self-contained.
            for core_name in ("tilemaps.dat", "tilesets.dat", "datatable.dat", "prefabs.dat", "scriptfile.tsl"):
                source = game_dir / core_name
                if source.exists():
                    destination = out_dir / core_name
                    if source.resolve() != destination.resolve():
                        shutil.copy2(source, destination)
        elif data_pack != (game_dir / "data.dat") and (game_dir / "data.dat").exists():
            shutil.copy2(game_dir / "data.dat", data_pack)

        gfx_pack = out_dir / "gfx.dat"
        remove_if_exists(gfx_pack)
        pack_sprite_arb_descriptors(args.dat_tool, gpl_lists, game_dir / "sprites", gfx_pack, staging_dir)
        pack_list(args.dat_tool, gpl_lists, "SPRITES", game_dir / "sprites", gfx_pack, "BMP", True, staging_dir)

        for editor_bitmap in ("editor_gui.bmp", "editor_bitmask.bmp", "editor_object_sides.bmp"):
            source = editor_dir / editor_bitmap
            if source.exists():
                run_dat(args.dat_tool, gfx_pack, source, "BMP", True)

        sfx_pack = out_dir / "sfx.dat"
        remove_if_exists(sfx_pack)
        pack_list(args.dat_tool, gpl_lists, "SOUND_FX", game_dir / "sound_fx", sfx_pack, "DATA", True, staging_dir)

        stream_pack = out_dir / "stream.dat"
        remove_if_exists(stream_pack)
        pack_list(args.dat_tool, gpl_lists, "STREAMS", game_dir / "streams", stream_pack, "DATA", True, staging_dir)

        paths_pack = out_dir / "paths.dat"
        remove_if_exists(paths_pack)
        pack_list(args.dat_tool, gpl_lists, "PATHS", game_dir / "paths", paths_pack, "DATA", True, staging_dir)

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
