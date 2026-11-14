#!/usr/bin/env python3
"""The tools the decomp builds and matches with, fetched by URL and SHA-256 into `local/tools/`.

What is fetched is pinned in `fetched_tools.toml` beside this file: the toolchain - GCC 2.8.1's `cc1` (gcc-psx), the MIPS
binutils (binutils-mipsel), the preprocessor (aro-cpp), the releases the editor's pe2.psx downloads too, pinned there
alike - maspsx, ninja, the developer tools, the GCC tracers' `cc1`, and the patches the decomp applies. What is built for one
platform is kept by platform, `local/tools/<platform>/<name>/`, so that one checkout used from several systems keeps
each one's; what runs anywhere (Python) once, `local/tools/<name>/`. Each folder records what it was made from
(`.pin`): a changed pin is fetched again, an unchanged one reused.

    python3 config.py splat fetch-tools                 fetch everything this platform has
    python3 config.py splat fetch-tools --path <name>   fetch one, and print its folder
"""

from __future__ import annotations

import hashlib
import io
import json
import os
import platform
import shutil
import stat
import subprocess
import sys
import tarfile
import tomllib
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STORE = ROOT / "local" / "tools"
TABLE = Path(__file__).resolve().with_name("fetched_tools.toml")


class FetchError(RuntimeError):
    pass


def host() -> str:
    """This machine's platform, as the manifests name it."""
    machine = platform.machine().lower()
    arch = {"amd64": "x86_64", "x86_64": "x86_64", "arm64": "aarch64", "aarch64": "aarch64"}.get(machine, machine)
    system = {"linux": "linux", "darwin": "macos", "win32": "windows"}.get(sys.platform, sys.platform)
    return f"{system}-{arch}"


def exe(name: str) -> str:
    """A program's file name on this machine."""
    return name + ".exe" if sys.platform == "win32" else name


def entries() -> dict[str, dict]:
    """Every tool this platform has: its URL, SHA-256, what is taken of it, how it is unpacked and patched."""
    here = host()
    found: dict[str, dict] = {}
    for name, spec in tomllib.loads(TABLE.read_text(encoding="utf-8")).items():
        # A tool built for each platform has a table a platform (`[mkpsxiso.linux-x86_64]`): this one's, if any.
        by_platform = {k: v for k, v in spec.items() if isinstance(v, dict) and "url" in v}
        if by_platform:
            if here not in by_platform:
                continue
            spec = {**by_platform[here], "platform": here}
        if "url" not in spec or spec.get("platform", here) != here:
            continue
        entry = {k: v for k, v in spec.items() if k != "take"}
        entry.setdefault("platform", None)
        if "take" in spec:
            entry["take"] = {source: taken(to) for source, to in spec["take"].items()}
        found[name] = entry
    return found


def taken(to: str | list | dict) -> tuple[tuple[str, ...], str | None]:
    """Where a file taken of an archive goes, and the SHA-256 it is held to: a place, several (a DLL beside each
    program loading it), or a place with the file's SHA-256 (`{ to, sha256 }`)."""
    if isinstance(to, dict):
        return (to["to"],), to.get("sha256")
    if isinstance(to, list):
        return tuple(to), None
    return (to,), None


def folder(name: str, entry: dict) -> Path:
    return STORE / entry["platform"] / name if entry["platform"] else STORE / name


def pin_of(entry: dict) -> dict:
    """What a folder is made from: the download, what is taken of it, and each patch's bytes."""
    patches = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in entry.get("patches", [])}
    take = {k: [list(to), sha256] for k, (to, sha256) in entry.get("take", {}).items()}
    return {"url": entry["url"], "sha256": entry["sha256"], "file": entry.get("file"), "take": take,
            "patches": patches}


def download(url: str, sha256: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "pe2-decomp-fetch-tools"})
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
    except OSError as error:
        raise FetchError(f"{url}: cannot be downloaded ({error})") from error
    found = hashlib.sha256(data).hexdigest()
    if found != sha256:
        raise FetchError(f"{url}: its SHA-256 is {found}, not the {sha256} pinned")
    return data


def unpack(data: bytes, into: Path, only: set[str] | None = None) -> None:
    """A zip or a gzip'd tar, as it is, its files' permissions kept; of a zip, only the files named in `only` where
    it is given (a whole toolchain's archive, of which a tool takes two programs)."""
    if data[:2] == b"PK":
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for info in archive.infolist():
                if only is not None and info.filename not in only:
                    continue
                target = into / info.filename
                if not target.resolve().is_relative_to(into.resolve()):
                    raise FetchError(f"{info.filename}: outside the archive")
                if info.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(info))
                mode = info.external_attr >> 16
                if mode & 0o111:
                    target.chmod(target.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    else:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:*") as archive:
            archive.extractall(into, filter="data")


def apply_patches(where: Path, patches: list[str]) -> None:
    """Each patch applied to the unpacked tree. git is kept from finding the decomp's repository around it, where
    `git apply` would take the patch's paths as the repository's and skip them all."""
    env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(where.parent))
    for patch in patches:
        result = subprocess.run(
            ["git", "apply", "--whitespace=nowarn", str(ROOT / patch)], cwd=where, env=env,
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            raise FetchError(f"{patch} does not apply: {result.stderr.strip()}")


def make(name: str, entry: dict, final: Path) -> None:
    data = download(entry["url"], entry["sha256"])
    staging = final.with_name(f"{final.name}.part-{os.getpid()}")
    unpacked = final.with_name(f"{final.name}.unpacked-{os.getpid()}")
    for path in (staging, unpacked):
        shutil.rmtree(path, ignore_errors=True)
    try:
        unpacked.mkdir(parents=True)
        if entry.get("file"):
            # The download is the program itself.
            staging.mkdir(parents=True)
            target = staging / entry["file"]
            target.write_bytes(data)
            target.chmod(target.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        elif "take" in entry:
            unpack(data, unpacked, only=set(entry["take"]))
            staging.mkdir(parents=True)
            for source, (destinations, sha256) in entry["take"].items():
                path = unpacked / source
                if not path.is_file():
                    raise FetchError(f"{name}: its archive holds no {source}")
                if sha256 and hashlib.sha256(path.read_bytes()).hexdigest() != sha256:
                    raise FetchError(f"{name}: {source} is not the file pinned")
                for to in destinations:
                    (staging / to).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, staging / to)
        else:
            unpack(data, unpacked)
            top = unpacked / entry["strip"] if entry.get("strip") else unpacked
            if not top.is_dir():
                raise FetchError(f"{name}: its archive has no {entry['strip']}/")
            top.rename(staging)
        apply_patches(staging, entry.get("patches", []))
        (staging / ".pin").write_text(json.dumps(pin_of(entry), indent=1, sort_keys=True) + "\n", encoding="utf-8")
        if final.exists():
            shutil.rmtree(final)
        try:
            staging.rename(final)
        except OSError:
            # Made at the same time by another process: theirs is the same.
            if not current(final, entry):
                raise
    finally:
        shutil.rmtree(staging, ignore_errors=True)
        shutil.rmtree(unpacked, ignore_errors=True)


def current(path: Path, entry: dict) -> bool:
    pin = path / ".pin"
    try:
        return json.loads(pin.read_text(encoding="utf-8")) == pin_of(entry)
    except (OSError, ValueError):
        return False


def ensure(name: str, quiet: bool = False) -> Path:
    """The folder of a tool, fetched first where it is not there as pinned."""
    every = entries()
    if name not in every:
        raise FetchError(f"no tool {name} for {host()} (tools/fetched_tools.toml, editor/extensions/psx)")
    entry = every[name]
    path = folder(name, entry)
    if not current(path, entry):
        if not quiet:
            print(f"fetching {name} into {path.relative_to(ROOT)}", file=sys.stderr)
        path.parent.mkdir(parents=True, exist_ok=True)
        make(name, entry, path)
    return path


def fetch(name: str | None = None) -> int:
    """`config.py splat fetch-tools`: every tool this platform has, each with its folder printed; or one, `name`, and
    only its folder."""
    try:
        if name:
            print(ensure(name, quiet=True))
        else:
            for each in sorted(entries()):
                print(f"{each}: {ensure(each)}")
    except FetchError as error:
        print(f"config.py: {error}", file=sys.stderr)
        return 1
    return 0
