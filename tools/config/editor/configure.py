"""`config.py editor`: an editor project made, or made the active one, and the ninja build that has the editor build it.

    config.py editor [DIR] [--config NAME] [--release R]... [--roms DIR] [--fresh] [--no-extract] [--no-extensions]
                     [--no-trust]
    config.py editor build-tools [--jobs N]

With no project in DIR (`project/` by default), one is made by the editor's own commands (`pe2-cli project new`, then
`layer add`, `mount add`, `mount locate`, `config add`): the layer of the game's nodes `.layers/game` below the
project's own `.layers/project`, the generated layer `.layers/game-files` its plan fills with their files, the unmanaged
mount `roms` where the disc images are (`roms/` in the project, or wherever `--roms` says, which this machine's
`project.local.toml` keeps), a read-only mount of this checkout's `packages/`, and a configuration per release the
decompilation builds. The extensions the project builds and plays its movies with are added (`pe2.psx`, `pe2.ffmpeg`),
and the game's nodes are written from the declarations (every asset's node, and the plan filling them,
`.layers/game-files.plan.jsonl`, stating the discs it reads by name, size and sha1), and the editor runs the plan
(`pe2-cli project extract`), which finds the discs in `roms`. A project is of every release declared (`--release` for
fewer). `--fresh` writes the game's nodes and plan anew in a project there is, and extracts again; the project's own
layer is kept.

Then the project is trusted (all that is inside it: what runs unsandboxed there, `--no-trust` to leave it), made the
active one - the repository's `compile_commands.json` a link to the one its build writes - and `build.ninja` written:
`ninja` has the editor build the project (`pe2-cli project build`, always, as ninja cannot see what the editor reads) and
holds what it deploys to `configs/<release>/checksum.sha` (`check_build.py`).

Nothing of the project is read here: its layers are the names given them when it was made, and the releases are the
repository's (`tools/config/releases.py`) and the declarations'. The editor comes from `cargo xtask dist`
(`build-tools`, run first when none is laid out). Standard library only: an editor project needs none of the splat
build's packages.
"""
from __future__ import annotations

import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))   # tools/config: declarations, releases
import declarations as declared  # noqa: E402
import init as layer_init  # noqa: E402
import plan as plans  # noqa: E402
import project_lock  # noqa: E402
import releases as known  # noqa: E402
from cli import EDITOR, dist_cli  # noqa: E402

# What every project holds, in the layer of the game's nodes: the PlayStation target its packages build for, and ffmpeg,
# its movies' video files.
EXTENSIONS = ["pe2.psx", "pe2.ffmpeg"]
NODES = "game"                      # the layer of the game's nodes, `.layers/game`
FILES = "game-files"                # the generated layer filling them, its plan `.layers/game-files.plan.jsonl`
MOUNTS = [("packages", ROOT / "packages", True)]
# The languages a project's configs may be built in, English the game's own.
LANGUAGES = ["en", "fr", "de", "es", "it"]
# What this script makes that is this machine's: the mounts' links, and what builds write.
IGNORE = ["/build/"] + [f"/{at}" for at, _p, _r in MOUNTS]
PYTHON = sys.executable


def built_releases() -> list[str]:
    """The releases the decompilation builds, in its order."""
    return [v.metadata.version_dir for v in known.GAME_VERSIONS]


def declared_releases() -> list[str]:
    """The releases the declarations know, in the order a plan prefers them: those the decompilation builds first, as
    it orders them, then the rest by name."""
    found = {p.name for p in (Path(declared.DECLARATIONS) / "disc").iterdir() if p.is_dir()}
    built = built_releases()
    return [r for r in built if r in found] + sorted(found - set(built))


def relative(target: Path, start: Path) -> str:
    return Path(os.path.relpath(target.resolve(), start.resolve())).as_posix()


def quote(argument: str) -> str:
    """One argument of a ninja rule's command, quoted for whatever splits the line (/bin/sh, or the program on
    Windows)."""
    return subprocess.list2cmdline([argument]) if os.name == "nt" else shlex.quote(argument)


def make_link(path: Path, target: Path) -> None:
    """A mount, realized as a link (a junction on Windows)."""
    if path.is_symlink():
        path.unlink()
    elif path.exists():
        raise SystemExit(f"{path} exists and is not a link")
    try:
        path.symlink_to(relative(target, path.parent), target_is_directory=True)
    except OSError:
        if sys.platform != "win32":
            raise
        subprocess.run(["cmd", "/c", "mklink", "/J", str(path), str(target.resolve())], check=True, capture_output=True)


def editing(directory: Path, cli: Path):
    def edit(*arguments: str) -> None:
        subprocess.run([str(cli), "project", *arguments, "--project", str(directory)], check=True)
    return edit


def locate(directory: Path, roms: Path | None, cli: Path) -> None:
    """Where the disc images are on this machine: `roms/` in the project unless said (`project.local.toml`)."""
    if roms is None:
        (directory / "roms").mkdir(exist_ok=True)
    else:
        editing(directory, cli)("mount", "locate", "roms", str(roms.absolute()))


def make(directory: Path, wanted: list[str], roms: Path | None, cli: Path) -> None:
    """A new project in `directory` (absent or empty), by the editor's own commands: an empty project, then the layer
    of the game's nodes and the generated layer filling them, where the disc images are, its mounts and its
    configurations. What is not the project's file - the links and their `.gitignore` entries - is written here."""
    if directory.exists() and any(directory.iterdir()):
        raise SystemExit(f"{directory} is not empty and holds no project: a project is made in a new or empty folder")
    subprocess.run([str(cli), "project", "new", str(directory)], check=True)
    edit = editing(directory, cli)
    edit("layer", "add", NODES, "--bottom")
    edit("layer", "add", FILES, "--generated", "--fills", NODES)
    edit("mount", "add", "roms", "--unmanaged")
    locate(directory, roms, cli)
    for at, path, read_only in MOUNTS:
        edit("mount", "add", at, relative(path, directory), *(["--read-only"] if read_only else []))
        make_link(directory / at, path)
    # A configuration for each release the decompilation builds.
    for release in (r for r in wanted if r in built_releases()):
        edit("config", "add", release.lower(), "--release", release, "--target", "psx")
    # The language a build's text is in: a string table's own where it has none of that language (`ui/dialogue/translated`).
    edit("config", "declare", "language", "--values", ",".join(LANGUAGES), "--default", "en")
    ignore = directory / ".gitignore"
    text = ignore.read_text(encoding="utf-8") if ignore.is_file() else ""
    ignore.write_text(text + "".join(f"{entry}\n" for entry in IGNORE if entry not in text.splitlines()),
                      encoding="utf-8")
    print(f"made the project {directory}")


def add_extensions(directory: Path, cli: Path) -> None:
    """The extensions every project holds (EXTENSIONS), each copied into the layer of the game's nodes, which ships
    with them, and required."""
    for extension in EXTENSIONS:
        subprocess.run([str(cli), "project", "extension", "add", extension, "--layer", NODES,
                        "--project", str(directory)], check=True)


def write_layer(directory: Path, cli: Path, wanted: list[str]) -> None:
    """The game's nodes as the declarations make them, and the plan filling them."""
    import json
    registry = json.loads(subprocess.run([str(cli), "project", "registry", "--project", str(directory)],
                                         check=True, capture_output=True, text=True).stdout)
    decl = declared.Declarations()
    # Written as a tree of files, which the editor's own command makes the layer's records (`project layer import`):
    # the layer is the declarations' alone.
    staging = Path(tempfile.mkdtemp(prefix="pe2-nodes-")) / NODES
    try:
        layer_init.write_nodes(staging, layer_init.layer_nodes(decl, registry, wanted))
        subprocess.run([str(cli), "project", "layer", "import", NODES, "--from", str(staging),
                        "--project", str(directory)], check=True)
    finally:
        shutil.rmtree(staging.parent, ignore_errors=True)
    made = plans.make(decl, registry, wanted)
    plan = directory / ".layers" / f"{FILES}.plan.jsonl"
    plan.write_text(made.text(), encoding="utf-8")
    print(f"wrote the game's nodes, and the plan filling them ({len(made.ops)} operations) into {plan}")


def fill(directory: Path, cli: Path, wanted: list[str], extract: bool) -> None:
    """The game's nodes and their plan written from the declarations, and the editor's extraction run unless not
    `extract`, holding the project's lock."""
    _lock = project_lock.hold(directory, "config.py editor")
    write_layer(directory, cli, wanted)
    if extract:
        subprocess.run([str(cli), "project", "extract", "--project", str(directory)], check=True)


def configurations(directory: Path, cli: Path) -> dict[str, str]:
    """The project's configurations and the release each is of, as the editor lists them (`usa: release = USA, ...`)."""
    listed = subprocess.run([str(cli), "project", "config", "list", "--project", str(directory)],
                            capture_output=True, text=True, check=True).stdout
    return {m.group(1): m.group(2) for m in re.finditer(r"(?m)^(\S+): .*\brelease = (\S+?)(?:,|$)", listed)}


def activate(directory: Path) -> None:
    """The active project: the compile database its build writes, where the editors and clangd look for it."""
    database = ROOT / "compile_commands.json"
    if database.is_symlink() or database.exists():
        database.unlink()
    target = directory / "build" / "compile_commands.json"
    try:
        database.symlink_to(os.path.relpath(target, ROOT))
    except OSError:
        raise SystemExit(f"config.py: compile_commands.json could not link to {target}: links need privileges here")


def write_ninja(directory: Path, cli: Path, config: str, release: str) -> None:
    """build.ninja: `ninja` runs the editor's build of `config` and checks what it deploys."""
    executable = known.named(release).metadata.exe_disk1
    deployed = directory / "build" / config
    stamp = f"build/{release}/project.checked"
    built = f"build/{release}/project.built"             # never made, so the editor's build always runs
    check = relative(HERE / "check_build.py", ROOT)
    for stale in ("rules.ninja", "matching.ninja", "objdiff.ninja"):         # the splat build's
        (ROOT / stale).unlink(missing_ok=True)
    (ROOT / "build.ninja").write_text(f"""\
# The editor builds {directory} ({config}); its images are held to the checksums. Written by `config.py editor`.
builddir = build

rule editor-build
  command = {quote(str(cli))} project build --project {quote(str(directory))} --config {quote(config)}
  description = editor build {config}
  pool = console

rule editor-check
  command = {quote(PYTHON)} {check} {quote(str(deployed))} configs/{release}/checksum.sha --executable {executable} --stamp $out
  description = check the editor's images

build {built}: editor-build
build {stamp}: editor-check {built}
default {stamp}
""", encoding="utf-8")


def build_tools(jobs: int = 0) -> Path:
    """`config.py editor build-tools`: the editor and its extensions laid out (`cargo xtask dist`). Returns pe2-cli."""
    if not shutil.which("cargo"):
        raise SystemExit("config.py: the editor is built with Rust's cargo, which is not installed")
    env = dict(os.environ)
    jobs = jobs or int(os.environ.get("PE2_JOBS") or 0)
    if jobs:
        env["CARGO_BUILD_JOBS"] = str(jobs)
    print("laying out the editor and its extensions (cargo xtask dist)")
    subprocess.run(["cargo", "xtask", "dist"], cwd=EDITOR, env=env, check=True)
    cli = dist_cli()
    if cli is None:
        raise SystemExit("config.py: cargo xtask dist laid out no pe2-cli")
    print(f"the editor's command line: {cli}")
    return cli


def configure(directory: Path | None = None, config: str | None = None, releases: list[str] | None = None,
              roms: Path | None = None, fresh: bool = False, extract: bool = True, extensions: bool = True,
              trust: bool = True) -> None:
    """`config.py editor`: the project in `directory` made where there is none (or its game's part made anew,
    `fresh`), trusted, made the active one, and build.ninja written to have the editor build `config`."""
    directory = (directory or ROOT / "project").absolute()
    cli = dist_cli() or build_tools()
    declared_ones = declared_releases()
    unknown = [r for r in releases or [] if r not in declared_ones]
    if unknown:
        raise SystemExit(f"config.py: unknown release {', '.join(unknown)}; declared: {', '.join(declared_ones)}")
    wanted = [r for r in declared_ones if r in releases] if releases else declared_ones
    if not (directory / "project.toml").is_file():
        print(f"making a project in {directory}")
        make(directory, wanted, roms, cli)
        if extensions:
            add_extensions(directory, cli)
        fill(directory, cli, wanted, extract)
    else:
        if roms is not None:
            locate(directory, roms, cli)
        if fresh:
            fill(directory, cli, wanted, extract)
    if trust:
        subprocess.run([str(cli), "project", "trust", "--all", "--project", str(directory)], check=True)
    configs = configurations(directory, cli)
    if config is None:
        # The configuration of the first release the decompilation builds that the project has one of.
        for release in built_releases():
            found = [name for name, of in configs.items() if of == release]
            if len(found) > 1:
                raise SystemExit(f"config.py: {directory}: {len(found)} configurations of {release}: name one (--config)")
            if found:
                config = found[0]
                break
        else:
            raise SystemExit(f"config.py: {directory} has no configuration of a release the decompilation builds")
    elif config not in configs:
        raise SystemExit(f"config.py: {directory} has no configuration {config} ({', '.join(configs) or 'none'})")
    release = configs[config]
    if release not in built_releases():
        raise SystemExit(f"config.py: {config} is of {release}, which the decompilation has no checksums of")
    activate(directory)
    write_ninja(directory, cli, config, release)
    import fetch_tools
    ninja = fetch_tools.ensure("ninja") / fetch_tools.exe("ninja")
    print(f"configured: `ninja` has the editor build {directory} ({config}) and checks its images (ninja: {ninja})")
