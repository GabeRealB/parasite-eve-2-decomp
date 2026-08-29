#!/usr/bin/env python3
"""Overlay-agnostic helpers for the matching decomp workflow.

Discovers every splat unit under ``asm/<version>/`` and ``configs/<version>/``,
including nested overlays that do not exist yet (``asm/USA/stage1/101/...``).
Nothing here hardcodes main / gameplay / title.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Optional


REPO_ROOT = Path(__file__).resolve().parent.parent

INCLUDE_ASM_RE = re.compile(
    r"INCLUDE_ASM\s*\(\s*\"([^\"]+)\"\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)"
)
JAL_RE = re.compile(r"\bjal\s+([A-Za-z_][A-Za-z0-9_]*)")
YAML_KEY_RE = re.compile(r"^\s*(asm_path|src_path|basename):\s*(.+)$", re.M)


@dataclass(frozen=True)
class Overlay:
    """One splat unit (main executable or any pe2pkg overlay)."""

    name: str
    version: str
    asm_path: Path
    src_path: Path
    yaml_path: Optional[Path] = None

    @property
    def include_path(self) -> Optional[Path]:
        candidate = REPO_ROOT / "include" / self.name
        return candidate if candidate.is_dir() else None


@dataclass(frozen=True)
class FunctionLoc:
    name: str
    overlay: Overlay
    asm_file: Path
    kind: str
    unit: str

    @property
    def include_asm_folder(self) -> str:
        return str(
            self.asm_file.parent.relative_to(REPO_ROOT / "asm" / self.overlay.version)
        )

    @property
    def c_file(self) -> Optional[Path]:
        found = find_include_asm_file(self.name)
        if found is not None:
            return found
        candidate = self.overlay.src_path / f"{self.unit}.c"
        return candidate if candidate.is_file() else candidate


def _strip_yaml_value(raw: str) -> str:
    value = raw.split("#", 1)[0].strip()
    if len(value) >= 2 and value[0] in "\"'" and value[-1] == value[0]:
        value = value[1:-1]
    return value


def parse_splat_yaml(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    fields: dict = {}
    for match in YAML_KEY_RE.finditer(text):
        fields[match.group(1)] = _strip_yaml_value(match.group(2))
    return fields


def iter_versions(version: Optional[str] = None) -> list[str]:
    asm = REPO_ROOT / "asm"
    if version:
        return [version]
    if not asm.is_dir():
        return []
    return sorted(p.name for p in asm.iterdir() if p.is_dir())


def _overlay_from_asm_dir(asm_dir: Path, version: str) -> Overlay:
    rel = asm_dir.relative_to(REPO_ROOT / "asm" / version)
    name = str(rel)
    return Overlay(
        name=name,
        version=version,
        asm_path=asm_dir,
        src_path=REPO_ROOT / "src" / name,
    )


def discover_overlays(version: Optional[str] = None) -> list[Overlay]:
    """Union of splat yamls and on-disk ``nonmatchings`` / ``matchings`` trees."""

    by_name: dict[tuple[str, str], Overlay] = {}

    for ver in iter_versions(version):
        asm_root = REPO_ROOT / "asm" / ver
        if asm_root.is_dir():
            for marker in ("nonmatchings", "matchings"):
                for marker_dir in asm_root.rglob(marker):
                    if not marker_dir.is_dir():
                        continue
                    overlay = _overlay_from_asm_dir(marker_dir.parent, ver)
                    by_name[(ver, overlay.name)] = overlay

        cfg_dir = REPO_ROOT / "configs" / ver
        if not cfg_dir.is_dir():
            continue
        # Hand-written configs sit directly in configs/<ver>/; the generated
        # overlay families are one level down. overlay.template.yaml lives
        # alongside the hand-written ones and is not a config - its fields are
        # still @@PLACEHOLDER@@ text, so skip anything that still has them.
        yaml_paths = sorted(cfg_dir.glob("*.yaml")) + sorted(cfg_dir.glob("generated/*.yaml"))
        for yaml_path in yaml_paths:
            fields = parse_splat_yaml(yaml_path)
            asm_path_s = fields.get("asm_path")
            if not asm_path_s or "@@" in asm_path_s:
                continue
            asm_path = REPO_ROOT / asm_path_s
            src_path_s = fields.get("src_path")
            src_path = (
                REPO_ROOT / src_path_s
                if src_path_s
                else REPO_ROOT / "src" / yaml_path.stem
            )
            try:
                name = str(asm_path.relative_to(REPO_ROOT / "asm" / ver))
            except ValueError:
                name = fields.get("basename") or yaml_path.stem
            by_name[(ver, name)] = Overlay(
                name=name,
                version=ver,
                asm_path=asm_path,
                src_path=src_path,
                yaml_path=yaml_path,
            )

    return sorted(by_name.values(), key=lambda o: (o.version, o.name))


def overlay_key(overlay: Overlay) -> str:
    return f"{overlay.version}/{overlay.name}"


def overlay_matches(overlay: Overlay, needle: str) -> bool:
    """True if *needle* names this overlay.

    Accepts ``gameplay``, ``USA/gameplay``, or an ``asm/...`` path suffix.
    """
    raw = needle.strip().strip("/")
    if not raw:
        return False
    if overlay.name == raw:
        return True
    if overlay_key(overlay) == raw:
        return True
    try:
        rel = overlay.asm_path.relative_to(REPO_ROOT)
    except ValueError:
        rel = overlay.asm_path
    rel_s = str(rel).strip("/")
    if rel_s == raw or rel_s.endswith("/" + raw):
        return True
    if raw.startswith("asm/") and rel_s == raw[len("asm/") :]:
        return True
    return False


def resolve_overlays(
    needle: str, version: Optional[str] = None
) -> list[Overlay]:
    hits = [o for o in discover_overlays(version) if overlay_matches(o, needle)]
    # Prefer an exact name match when both a parent and a nested unit hit.
    exact = [o for o in hits if o.name == needle.strip().strip("/")]
    return exact or hits


def list_nonmatching_dirs(
    version: Optional[str] = None, overlay: Optional[str] = None
) -> list[Path]:
    overlays = discover_overlays(version)
    if overlay:
        overlays = resolve_overlays(overlay, version)
        if not overlays:
            known = ", ".join(overlay_key(o) for o in discover_overlays(version)) or "(none)"
            raise ValueError(f"unknown overlay '{overlay}'. Known: {known}")
    dirs: list[Path] = []
    for item in overlays:
        candidate = item.asm_path / "nonmatchings"
        if candidate.is_dir():
            dirs.append(candidate)
    return dirs


def _iter_named_asm(overlay: Overlay, kind: str, name: str) -> Iterator[Path]:
    root = overlay.asm_path / kind
    if not root.is_dir():
        return
    matches = sorted(root.rglob(f"{name}.s"))
    for path in matches:
        if path.is_file():
            yield path


def find_function(name: str, version: Optional[str] = None) -> Optional[FunctionLoc]:
    name = name.removesuffix(".s")
    overlays = discover_overlays(version)
    for kind in ("nonmatchings", "matchings"):
        for overlay in overlays:
            for asm_file in _iter_named_asm(overlay, kind, name):
                return FunctionLoc(
                    name=name,
                    overlay=overlay,
                    asm_file=asm_file,
                    kind=kind,
                    unit=asm_file.parent.name,
                )
    return None


def find_include_asm_file(func_name: str) -> Optional[Path]:
    src_root = REPO_ROOT / "src"
    if not src_root.is_dir():
        return None
    needle = func_name.removesuffix(".s")
    for path in src_root.rglob("*.c"):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for folder, name in INCLUDE_ASM_RE.findall(text):
            if name == needle:
                return path
    return None


def loc_to_dict(loc: FunctionLoc) -> dict:
    c_file = loc.c_file
    include_path = loc.overlay.include_path
    return {
        "name": loc.name,
        "overlay": loc.overlay.name,
        "version": loc.overlay.version,
        "asm_file": str(loc.asm_file.relative_to(REPO_ROOT)),
        "kind": loc.kind,
        "unit": loc.unit,
        "c_file": str(c_file.relative_to(REPO_ROOT))
        if c_file is not None
        else None,
        "c_file_exists": bool(c_file is not None and c_file.is_file()),
        "include_asm_folder": loc.include_asm_folder,
        "src_path": str(loc.overlay.src_path.relative_to(REPO_ROOT)),
        "include_path": str(include_path.relative_to(REPO_ROOT))
        if include_path is not None
        else None,
        "yaml_path": str(loc.overlay.yaml_path.relative_to(REPO_ROOT))
        if loc.overlay.yaml_path is not None
        else None,
    }


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def _callees(asm_file: Path) -> list[str]:
    names: list[str] = []
    seen: set[str] = set()
    for match in JAL_RE.finditer(_read_text(asm_file)):
        name = match.group(1)
        if name not in seen:
            seen.add(name)
            names.append(name)
    return names


def _callers(func_name: str, limit: int = 8) -> list[str]:
    hits: list[str] = []
    src_root = REPO_ROOT / "src"
    if src_root.is_dir():
        for path in sorted(src_root.rglob("*.c")):
            try:
                text = _read_text(path)
            except OSError:
                continue
            if func_name not in text:
                continue
            rel = str(path.relative_to(REPO_ROOT))
            for i, line in enumerate(text.splitlines(), 1):
                if func_name in line and "INCLUDE_ASM" not in line:
                    hits.append(f"{rel}:{i}: {line.strip()}")
                    if len(hits) >= limit:
                        return hits
    return hits


def _include_asm_context(c_file: Path, func_name: str, radius: int = 8) -> str:
    lines = _read_text(c_file).splitlines()
    idx = None
    for i, line in enumerate(lines):
        if "INCLUDE_ASM" in line and func_name in line:
            idx = i
            break
    if idx is None:
        return f"(no INCLUDE_ASM for {func_name} in {c_file})"
    start = max(0, idx - radius)
    end = min(len(lines), idx + radius + 1)
    numbered = [f"{i + 1:5d}| {lines[i]}" for i in range(start, end)]
    return "\n".join(numbered)


def _sibling_functions(c_file: Path, func_name: str, limit: int = 4) -> list[str]:
    """Names of real C functions in the same TU, nearest the INCLUDE_ASM."""

    text = _read_text(c_file)
    lines = text.splitlines()
    include_line = 0
    for i, line in enumerate(lines):
        if "INCLUDE_ASM" in line and func_name in line:
            include_line = i
            break

    proto_re = re.compile(
        r"^(?:static\s+)?(?:[A-Za-z_][\w\s\*]*?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("
    )
    defined: list[tuple[int, str]] = []
    for i, line in enumerate(lines):
        if line.startswith(" ") or line.startswith("\t") or line.startswith("#"):
            continue
        match = proto_re.match(line)
        if not match:
            continue
        name = match.group(1)
        if name in {"if", "for", "while", "switch", "return"}:
            continue
        defined.append((i, name))

    defined.sort(key=lambda item: abs(item[0] - include_line))
    out: list[str] = []
    for _, name in defined:
        if name == func_name:
            continue
        if name not in out:
            out.append(name)
        if len(out) >= limit:
            break
    return out


def _struct_field_hits(needles: Iterable[str], limit_lines: int = 36) -> str:
    catalog = REPO_ROOT / "STRUCT_FIELDS.md"
    if not catalog.is_file():
        return ""
    keys: list[str] = []
    seen: set[str] = set()
    for raw in needles:
        name = raw.strip("`")
        if len(name) < 5 or name.startswith("func_") or name.startswith("D_"):
            continue
        if name[0].isdigit() or name in seen:
            continue
        seen.add(name)
        keys.append(name)
    if not keys:
        return ""
    pattern = re.compile(r"(?:^#+\s+`?|(?<![A-Za-z0-9_]))(" + "|".join(re.escape(k) for k in keys) + r")(?:`|\b)")
    lines = _read_text(catalog).splitlines()
    hits: list[str] = []
    used = 0
    for i, line in enumerate(lines):
        if not pattern.search(line):
            continue
        start = max(0, i)
        end = min(len(lines), i + 4)
        chunk = "\n".join(lines[start:end])
        if chunk in hits:
            continue
        hits.append(chunk)
        used += end - start
        if used >= limit_lines:
            break
    return "\n--\n".join(hits)


def pack_context(func_name: str, version: Optional[str] = None) -> str:
    loc = find_function(func_name, version)
    if loc is None:
        raise SystemExit(f"Error: function '{func_name}' not found under asm/")

    info = loc_to_dict(loc)
    c_file = Path(REPO_ROOT / info["c_file"]) if info["c_file"] else None
    callees = _callees(loc.asm_file)
    callers = _callers(loc.name)
    siblings: list[str] = []
    include_ctx = ""
    if c_file is not None and c_file.is_file():
        siblings = _sibling_functions(c_file, loc.name)
        include_ctx = _include_asm_context(c_file, loc.name)

    type_re = re.compile(r"\b([A-Z][A-Za-z0-9_]{3,})\b")
    needles: list[str] = []
    if include_ctx:
        needles.extend(type_re.findall(include_ctx))

    fields = _struct_field_hits(needles)

    project_root = str(REPO_ROOT)
    giveup = REPO_ROOT / "tools" / "giveups" / loc.name / "base.c"
    giveup_line = (
        f"- Give-up seed: `{giveup.relative_to(REPO_ROOT)}` — bootstrap uses this as `base.c`; do not restart from m2c"
        if giveup.is_file()
        else ""
    )
    lines = [
        f"# Decomp brief: {loc.name}",
        "",
        f"- Overlay: `{loc.overlay.name}` ({loc.overlay.version})",
        f"- Kind: `{loc.kind}`",
        f"- Unit: `{loc.unit}`",
        f"- ASM: `{info['asm_file']}`",
        f"- INCLUDE_ASM folder: `{loc.include_asm_folder}`",
        f"- C file: `{info['c_file']}`"
        + (" (missing — create it)" if not info["c_file_exists"] else ""),
        f"- Headers: `{info['include_path'] or 'none yet; use this overlay include/ when it exists'}`",
        f"- Yaml: `{info['yaml_path'] or 'none'}`",
        f"- Project root: `{project_root}`",
    ]
    if giveup_line:
        lines.append(giveup_line)
    lines += [
        "",
        "Scratch env is already bootstrapped. Do **not** run `./tools/claude`.",
        "From the scratch directory: `./build.sh base.c` (prints a Penalties: mix;",
        "auto-dumps RTL at ≥90%). Do not rewrite `base.c` from the asm before the",
        "first score. Read the dump summary; unpin / split locals before adding",
        "`register … asm(\"\")` pins.",
        "From project root after a 100% match: `./tools/build-and-verify.sh`",
        "If the best score stays >= 95% on register/scheduling diffs, run",
        f"`./permute.sh --run -j4 {loc.name} {info['asm_file']} <scratch>/base_N.c`",
        "from the project root.",
        "",
        "## INCLUDE_ASM site",
        include_ctx or "(no host C file yet)",
        "",
        "## Nearby matched functions in this TU",
        ", ".join(f"`{s}`" for s in siblings) or "(none)",
        "",
        "## Callees (jal)",
        ", ".join(f"`{c}`" for c in callees) or "(none)",
        "",
        "## Callers",
        "\n".join(callers) or "(none found in src/)",
        "",
        "## STRUCT_FIELDS.md hits",
        fields or "(none)",
        "",
        "Grep `DECOMPILATION_LEARNINGS.md` for the mismatch pattern.",
        "Do not read that file end-to-end.",
        "New types for this overlay live under its own `include/`, not `include/main/unknown_syms.h`.",
    ]
    return "\n".join(lines) + "\n"


def _cmd_find(args: argparse.Namespace) -> int:
    loc = find_function(args.name, args.version)
    if loc is None:
        print(f"Error: function '{args.name}' not found under asm/", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(loc_to_dict(loc), indent=2))
    else:
        info = loc_to_dict(loc)
        print(f"{info['name']}\t{info['overlay']}\t{info['asm_file']}\t{info['c_file']}")
    return 0


def _cmd_pack(args: argparse.Namespace) -> int:
    sys.stdout.write(pack_context(args.name, args.version))
    return 0


def _cmd_list_nonmatchings(args: argparse.Namespace) -> int:
    try:
        dirs = list_nonmatching_dirs(args.version, overlay=args.overlay)
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    for path in dirs:
        print(path.relative_to(REPO_ROOT))
    return 0


def _cmd_list_overlays(args: argparse.Namespace) -> int:
    rows = []
    for overlay in discover_overlays(args.version):
        row = {
            "name": overlay.name,
            "version": overlay.version,
            "asm_path": str(overlay.asm_path.relative_to(REPO_ROOT)),
            "src_path": str(overlay.src_path.relative_to(REPO_ROOT)),
            "yaml_path": str(overlay.yaml_path.relative_to(REPO_ROOT))
            if overlay.yaml_path
            else None,
            "nonmatchings": (overlay.asm_path / "nonmatchings").is_dir(),
        }
        rows.append(row)
        if not args.json:
            print(
                f"{overlay.version}/{overlay.name}\t{row['asm_path']}\t{row['src_path']}"
            )
    if args.json:
        print(json.dumps(rows, indent=2))
    return 0


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--version",
        default=None,
        help="Region directory under asm/ and configs/ (default: all)",
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_find = sub.add_parser("find", help="Resolve a function to overlay + paths")
    p_find.add_argument("name")
    p_find.add_argument("--json", action="store_true")
    p_find.set_defaults(func=_cmd_find)

    p_pack = sub.add_parser("pack", help="Print a matching brief for a function")
    p_pack.add_argument("name")
    p_pack.set_defaults(func=_cmd_pack)

    p_nm = sub.add_parser(
        "list-nonmatchings", help="Print every overlay nonmatchings directory"
    )
    p_nm.add_argument(
        "--overlay",
        default=None,
        help="Restrict to one overlay (name, version/name, or asm path suffix)",
    )
    p_nm.set_defaults(func=_cmd_list_nonmatchings)

    p_ov = sub.add_parser("list-overlays", help="Print discovered overlays")
    p_ov.add_argument("--json", action="store_true")
    p_ov.set_defaults(func=_cmd_list_overlays)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
