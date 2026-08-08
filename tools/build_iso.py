#!/usr/bin/env python3
"""Build Parasite Eve 2 BIN/CUE images from ISO creation manifests.

Each ISO manifest (``iso_disk1.json``, ``iso_disk2.json``) describes **one**
disc: identifiers, license, image/cue names, and a single ``directory_tree``.

The corresponding directory tree must already exist (from
``tools/peassets/pack.py --iso ...``), containing the files listed in the
manifest plus a ``layout.xml`` (written by pack).

Writes images under ``build/<version>/rom/``::

    build/USA/rom/SLUS-01042.bin + .cue
    build/USA/rom/SLUS-01055.bin + .cue

Examples::

    # Both discs using default paths for USA
    python3 tools/build_iso.py --version USA

    # Explicit ISO manifests + tree root
    python3 tools/build_iso.py \\
        --iso assets/USA/iso_disk1.json \\
        --iso assets/USA/iso_disk2.json \\
        --tree-root build/USA/rom \\
        --output build/USA/rom
"""

from __future__ import annotations

import argparse
import json
import logging
import platform
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR = REPO_ROOT / "tools"


def find_mkpsxiso() -> Path:
    system = platform.system()
    if system == "Windows":
        candidate = TOOLS_DIR / "windows" / "mkpsxiso" / "mkpsxiso.exe"
    elif system == "Darwin":
        candidate = TOOLS_DIR / "macos" / "mkpsxiso" / "mkpsxiso"
    else:
        candidate = TOOLS_DIR / "linux" / "mkpsxiso" / "mkpsxiso"
    if not candidate.exists():
        raise FileNotFoundError(f"mkpsxiso not found at {candidate}")
    return candidate


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def default_image_names(disk_name: str) -> tuple[str, str]:
    if disk_name.lower() in ("disk1", "disc1", "1"):
        return "SLUS-01042.bin", "SLUS-01042.cue"
    if disk_name.lower() in ("disk2", "disc2", "2"):
        return "SLUS-01055.bin", "SLUS-01055.cue"
    return f"{disk_name}.bin", f"{disk_name}.cue"


def resolve_image_names(iso_manifest: dict[str, Any], disk_dir: Path) -> tuple[str, str]:
    """Pick BIN/CUE names from the ISO manifest, with layout.xml / defaults fallback."""
    fallback_bin, fallback_cue = default_image_names(
        str(iso_manifest.get("name") or disk_dir.name)
    )
    image_name = iso_manifest.get("image_name") or ""
    cue_name = iso_manifest.get("cue_sheet") or ""

    layout = disk_dir / "layout.xml"
    if layout.exists() and (not image_name or not cue_name):
        root = ET.parse(layout).getroot()
        image_name = image_name or root.attrib.get("image_name", "")
        cue_name = cue_name or root.attrib.get("cue_sheet", "")

    if not image_name or str(image_name).startswith("mkpsxiso"):
        image_name = fallback_bin
    if not cue_name or str(cue_name).startswith("mkpsxiso"):
        cue_name = fallback_cue

    return Path(str(image_name)).name, Path(str(cue_name)).name


def write_layout_xml_from_iso_manifest(
    iso_manifest: dict[str, Any], disk_dir: Path
) -> Path:
    """(Re)write layout.xml inside disk_dir from an ISO creation manifest."""
    track = iso_manifest.get("track", iso_manifest)
    identifiers = track.get("identifiers", {})
    default_attrs = track.get("default_attributes", {})
    image_name, cue_name = resolve_image_names(iso_manifest, disk_dir)

    iso_project = ET.Element(
        "iso_project",
        {"image_name": image_name, "cue_sheet": cue_name},
    )
    track_el = ET.SubElement(
        iso_project,
        "track",
        {
            "type": str(track.get("type", "data")),
            "xa_edc": str(track.get("xa_edc", True)).lower(),
            "new_type": str(track.get("new_type", False)).lower(),
        },
    )

    id_attrs = {
        "system": str(identifiers.get("system", "PLAYSTATION")),
        "application": str(identifiers.get("application", "PLAYSTATION")),
    }
    if "volume" in identifiers:
        id_attrs["volume"] = str(identifiers["volume"])
    if "creation_date" in identifiers:
        id_attrs["creation_date"] = str(identifiers["creation_date"])
    ET.SubElement(track_el, "identifiers", id_attrs)

    if (disk_dir / "license_data.dat").exists():
        ET.SubElement(track_el, "license", {"file": "./license_data.dat"})

    if default_attrs:
        ET.SubElement(
            track_el,
            "default_attributes",
            {k: str(v) for k, v in default_attrs.items()},
        )

    tree_el = ET.SubElement(track_el, "directory_tree")
    for entry in track.get("directory_tree", iso_manifest.get("directory_tree", [])):
        if "dummy_sectors" in entry or entry.get("name") == "dummy":
            dummy_type = entry.get("dummy_type", entry.get("type", 0))
            if dummy_type == "dummy":
                dummy_type = 0
            ET.SubElement(
                tree_el,
                "dummy",
                {
                    "sectors": str(
                        entry.get("dummy_sectors", entry.get("sectors", 150))
                    ),
                    "type": str(dummy_type),
                },
            )
            continue

        name = entry["name"]
        if not (disk_dir / name).exists():
            raise FileNotFoundError(
                f"{disk_dir}: missing {name} listed in ISO manifest directory_tree"
            )
        ET.SubElement(
            tree_el,
            "file",
            {
                "name": name,
                "source": name,
                "type": str(entry.get("type", "data")),
            },
        )

    xml_path = disk_dir / "layout.xml"
    tree = ET.ElementTree(iso_project)
    ET.indent(tree, space="    ")
    tree.write(xml_path, encoding="utf-8", xml_declaration=True)
    logging.info("Wrote %s", xml_path)
    return xml_path


def disk_dir_for_manifest(
    iso_manifest: dict[str, Any], tree_root: Path, explicit_tree: Path | None
) -> Path:
    if explicit_tree is not None:
        return explicit_tree.resolve()
    name = iso_manifest.get("name")
    if not name:
        raise ValueError("ISO manifest missing required 'name' (e.g. disk1)")
    return (tree_root / str(name)).resolve()


def build_one_iso(
    iso_manifest: dict[str, Any],
    disk_dir: Path,
    output_dir: Path,
    *,
    rewrite_layout: bool = True,
) -> tuple[Path, Path]:
    if not disk_dir.is_dir():
        raise FileNotFoundError(
            f"ISO directory tree not found: {disk_dir}\n"
            "Run tools/peassets/pack.py --stages ... --iso <this manifest> first."
        )

    if rewrite_layout:
        layout = write_layout_xml_from_iso_manifest(iso_manifest, disk_dir)
    else:
        layout = disk_dir / "layout.xml"
        if not layout.exists():
            layout = write_layout_xml_from_iso_manifest(iso_manifest, disk_dir)

    image_name, cue_name = resolve_image_names(iso_manifest, disk_dir)
    out_bin = output_dir / image_name
    out_cue = output_dir / cue_name
    output_dir.mkdir(parents=True, exist_ok=True)

    mkpsxiso = find_mkpsxiso()
    cmd = [
        str(mkpsxiso),
        "-y",
        "-o",
        str(out_bin),
        "-c",
        str(out_cue),
        str(layout),
    ]
    logging.info("Building %s from %s", out_bin.name, disk_dir)
    logging.info("  %s", " ".join(cmd))
    result = subprocess.run(cmd, cwd=str(disk_dir), check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"mkpsxiso failed for {disk_dir} (exit {result.returncode})"
        )

    if not out_bin.exists():
        cwd_bin = disk_dir / image_name
        cwd_cue = disk_dir / cue_name
        if cwd_bin.exists():
            cwd_bin.replace(out_bin)
        if cwd_cue.exists():
            cwd_cue.replace(out_cue)

    if not out_bin.exists():
        raise FileNotFoundError(f"expected output image missing: {out_bin}")

    logging.info("  wrote %s (%d bytes)", out_bin, out_bin.stat().st_size)
    if out_cue.exists():
        logging.info("  wrote %s", out_cue)
    return out_bin, out_cue


def discover_iso_manifests(version: str) -> list[Path]:
    """Find iso_disk*.json under assets/<version>/ (and build/<version>/rom/)."""
    candidates = [
        REPO_ROOT / "assets" / version,
        REPO_ROOT / "build" / version / "rom",
    ]
    found: list[Path] = []
    for base in candidates:
        if not base.is_dir():
            continue
        for pattern in ("iso_disk*.json", "iso_*.json"):
            found.extend(sorted(base.glob(pattern)))
        if found:
            break
    # Prefer disk1 then disk2 order
    def sort_key(p: Path) -> tuple[int, str]:
        name = p.stem.lower()
        if "disk1" in name or name.endswith("1"):
            return (0, name)
        if "disk2" in name or name.endswith("2"):
            return (1, name)
        return (2, name)

    return sorted(set(found), key=sort_key)


def main(argv: list[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--version",
        "-ver",
        default="USA",
        help="Game version name under build/ and assets/ (default: USA)",
    )
    parser.add_argument(
        "--iso",
        "-i",
        type=Path,
        action="append",
        dest="iso_manifests",
        help="ISO creation manifest (iso_diskN.json). Repeat for each disc. "
        "Default: discover assets/<version>/iso_disk*.json",
    )
    parser.add_argument(
        "--tree-root",
        type=Path,
        help="Parent of disk1/ disk2/ trees (default: build/<version>/rom)",
    )
    parser.add_argument(
        "--tree",
        type=Path,
        help="Explicit directory tree for a single --iso (overrides --tree-root naming)",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        help="Directory for BIN/CUE output (default: build/<version>/rom)",
    )
    parser.add_argument(
        "--no-rewrite-layout",
        action="store_true",
        help="Use existing layout.xml if present instead of regenerating from the ISO manifest",
    )
    args = parser.parse_args(argv)

    version = args.version
    tree_root = (
        args.tree_root.resolve()
        if args.tree_root
        else (REPO_ROOT / "build" / version / "rom")
    )
    output_dir = (
        args.output.resolve()
        if args.output
        else (REPO_ROOT / "build" / version / "rom")
    )

    if args.iso_manifests:
        iso_paths = [p.resolve() for p in args.iso_manifests]
    else:
        iso_paths = discover_iso_manifests(version)
        if not iso_paths:
            logging.error(
                "No ISO manifests found. Pass --iso assets/%s/iso_disk1.json "
                "(and iso_disk2.json), or run write_manifest.py first.",
                version,
            )
            return 1

    if args.tree is not None and len(iso_paths) != 1:
        logging.error("--tree requires exactly one --iso manifest")
        return 1

    results: list[tuple[Path, Path]] = []
    for iso_path in iso_paths:
        iso_manifest = load_json(iso_path)
        if "disks" in iso_manifest:
            logging.error(
                "%s contains multiple disks; split into one iso_diskN.json per disc",
                iso_path,
            )
            return 1
        if "stages" in iso_manifest or "stage0" in iso_manifest:
            logging.error(
                "%s looks like a stages/content manifest; pass iso_diskN.json instead",
                iso_path,
            )
            return 1

        disk_dir = disk_dir_for_manifest(iso_manifest, tree_root, args.tree)
        logging.info("ISO manifest %s -> tree %s", iso_path, disk_dir)
        results.append(
            build_one_iso(
                iso_manifest,
                disk_dir,
                output_dir,
                rewrite_layout=not args.no_rewrite_layout,
            )
        )

    logging.info("Built %d disc image(s) under %s", len(results), output_dir)
    for bin_path, cue_path in results:
        logging.info("  %s", bin_path)
        if cue_path.exists():
            logging.info("  %s", cue_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
