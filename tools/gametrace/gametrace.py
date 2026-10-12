#!/usr/bin/env python3
"""Record what the running game does, through a GDB server.

    gametrace.py run   [--config tools/gametrace/events.toml] [--out FILE]   (default: a new file per run)
                       [--host HOST] [--port PORT]      (these override the config's [gdb] section)
                       [--now]                          (attach to a game already running: no install_at wait)
    gametrace.py check [--config ...]     resolve the events, build the blob, show both; no connection
    gametrace.py refilter LOG --out FILE  put a log already written through the config's filters
    gametrace.py blob  [--config ...]     write the assembled blob and its listing in the log directory

It needs nothing but Python: the blob is assembled by the small assembler in this file. Where the system has MIPS
binutils, `blob` also assembles the same source with them and says whether the bytes agree.

One config file lists the events. An event is a function: its entry, and its returns where the config gives them.
Each is watched by a hook written into the running game - nothing stops when an event happens, and nothing in the
game's files changes.

  entry   the function's first two instructions are replaced by a jump to a stub in the blob. The stub writes
          the event's captures into a buffer, runs the two displaced instructions and jumps back to the third.
  exit    `exits` lists the offsets of the function's `jr $ra` instructions. Each, with its delay slot, is
          replaced by a jump to a stub that runs the delay-slot instruction, writes the exit captures, and
          returns to the caller itself.

What an event records is its `capture` list (and `exit_capture` for its returns), each item one of

  "a0"            a register
  "a0:16"         16 bytes at the address a register holds;  "sp+16:4" adds an offset first
  "0x800714C4:8"  8 bytes at a fixed address

so records differ in length. The buffer is one stretch of bytes (`blob.bytes`, 1 MB by default) that the stubs fill
front to back. A stub first checks that its record fits; if not it calls the one place in the blob where the tool
holds a breakpoint, the tool reads everything written so far, and the buffer starts again. It checks once more
after writing. That breakpoint, and the `install_at` ones, are the only times the game stops; Ctrl-C reads what
is left, takes the hooks out and lets the game run.

A pointer capture is only read when the register holds an address inside the console's memory; otherwise the bytes
are recorded as zero. Interrupts are held off while a record is written, so a hook reached from an interrupt
cannot land inside another's record.

The tool expects to be started with the game paused, before it has run. It continues the game itself up to the
first `install_at` address - a function by which memory is set up, such as the program's `main` - and only there
writes the blob and the hooks of the events in the images that are in memory from the start.

The tracer knows nothing about any one game. What code there is comes as images: a name, where it loads, its bytes
on disc and its symbols. A hooks file declares them from an `@init` function (see `init`, `image`), or the config
does in `[[image]]` tables (`name`, `load`, `file`, and optionally `skip`, `symbols`, `start`). An image marked
`start` is in memory when the tracer sets up. An event without `image = "..."` belongs to the start image holding
its address. `[log] dir` is where logs go, from the config's directory.

An event with a handler is not a hook but a breakpoint on its own address: it has to stop the game anyway, and
a breakpoint displaces nothing, so it may sit on any instruction. Its captures are read when it stops there.

An event in an image that is loaded later (`image = "..."`) gets its hooks when the tracer is told the image is in memory, and it does not work that out by itself:
an event with `handler = "name"` runs a python function at the end of its hook, with the game stopped, and that
function calls `trace.resident(image)` (see `handler`). Put it on the game's own "this file has finished loading"
point. `--resident IMAGE` and the `resident` command say the same by hand. A hook is still only written where
the instructions it replaces are in memory as the image has them. An event's address may be any instruction, not
only a function's first. An instruction a hook displaces must be free to move: `check` refuses a branch
or a jump there (see `movable`).

Where the emulator publishes the console's memory as a shared block and the tool runs on the same machine, the
buffer is read from that block as the game runs: no stop for a drain at all, and the log follows the game within
a quarter of a second (`[memory] shared` is the block's name, in which `{pid}` stands for the id of the process `[memory] process`
names; false, the default, turns it off). The block
is checked before it is trusted - the blob written through the server must be readable in it. Writes always go
through the server.

The blob needs memory of its own (`blob.address`); by default just above 2 MB, which a target has only with more
than the console's 2 MB. The tool refuses to write where that memory is a mirror of lower memory.

Memory being there is not the same as the game being allowed to reach it: a program can tell the machine how
much memory it has, after which anything above faults. `blob.poke` lists words, `[address, value]`, that every
stub writes before it touches the blob's memory - such as the register that says how much memory there is.

Between the events and the log sit filters (`[log] filters = [...]`, see `filter`): every line about to be written
goes through them. `repeats`, built in, writes a run of lines that repeats the last one, or the last few in order,
only once, followed by `{"event": "repeated", "last": N, "times": T}` - the N lines before this one came T more
times. Every event takes part unless it says `collapse = false` or has a handler; a line of one that does not
ends the run, so it keeps its exact place.

The tracer can also act on the game, through commands. It only gives the means and judges nothing: where the
game is stopped (`registers`, `where`), `step` and `run_to`, memory to read and `poke`, MIPS source assembled
into free memory (`inject`, `free`), and `hook` / `unhook` to send the game there from any instruction. `call`
puts those together to have the game call one of its own functions from where it stands and give back what it
returned; `call F A B` at the prompt is that, raw. What a place is good for is the business of whoever writes a
command: a hooks file adds commands with `@command("name")` (see `command`), each of which runs with the game
stopped, to its end, and carries its own rules - where it may act, where it takes the game first. What is done
this way is noted in the log, since a game that was acted on is not the game as played.

The buffer is read when it is full, when the tracer detaches (Ctrl-C: what is left is read, the hooks are taken
out and the game runs on), and after every event marked `drain = true` - at its returns where the config gives
them, otherwise straight after its entry. A rare event, such as a room load, makes a good one: the log then
arrives in pieces as the game goes, without a stop for each sound.

While it runs the tool takes commands on its input, each carried out with the game stopped: `pause` and `resume`
stop and continue the game; `note TEXT` writes a line of free text into the log where the game is now (the log's
`{"event": "note"}` lines are annotations: yours, a hook's, the tracer's own or a later tool's);
`session NAME` reads the buffer and begins a new session, which is a marker line in
the log saying what the records after it are; `name NAME` renames the current one; `drop` marks the current session as
dropped - its records stay in the log, since what was loaded during it is still loaded, with a marker after them -
and stops the game until the next session begins (`session` alone begins one of the same name); `drain`, `status`, `quit`. `--session NAME` names the first.

A record is one JSON line: id, name, phase ("enter" or "exit"), then each capture under its own text; a captured
`ra` also gives `from`, the function it returns into. An event with `render = "name"` is written by a renderer
instead: a python function, registered under that name by one of the files the config's `[hooks]` section
lists, which gets the captures as numbers and bytes and returns what the log should say (see `renderer`).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import time
import tomllib
from bisect import bisect_right
from pathlib import Path

REGS = ["zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"]
STATUS = 32      # the status register's place in it
PC = 37          # the program counter's place in the `g` packet, after r0..r31, sr, lo, hi, bad, cause
JR_RA = 0x03E00008
FRAME = 32       # a stub's stack frame: ra, t0, t1, t2, the status register
SAVED = {"ra": 0, "t0": 4, "t1": 8, "t2": 12}
CAPTURE = re.compile(r"^(?:(?P<reg>[a-z][a-z0-9]{1,3})(?:\+(?P<off>0x[0-9A-Fa-f]+|\d+))?|(?P<addr>0x[0-9A-Fa-f]{8}))(?::(?P<len>0x[0-9A-Fa-f]+|\d+))?$")
MAX_BYTES = 256  # of one memory capture: it is copied by straight-line code


# ---- the config

def parse_capture(text: str) -> dict:
    m = CAPTURE.match(text.strip())
    if not m:
        raise ValueError(f"capture {text!r} is not a register (\"a0\"), bytes at one (\"a0:16\", \"sp+16:4\") or bytes at an address (\"0x80012345:8\")")
    length = int(m["len"], 0) if m["len"] else None
    if m["addr"]:
        if length is None:
            raise ValueError(f"capture {text!r} needs a length: \"{m['addr']}:4\"")
        item = {"text": text, "kind": "fixed", "address": int(m["addr"], 16), "length": length}
    elif m["reg"] not in REGS or m["reg"] in ("zero", "at", "k0", "k1"):
        raise ValueError(f"capture {text!r}: {m['reg']} is not a register a hook can read")
    elif length is None:
        if m["off"]:
            raise ValueError(f"capture {text!r}: an offset only makes sense with a length, for bytes at the address")
        item = {"text": text, "kind": "reg", "reg": m["reg"], "length": 4}
    else:
        item = {"text": text, "kind": "mem", "reg": m["reg"], "offset": int(m["off"] or "0", 0), "length": length}
    if item["kind"] != "reg" and not 0 < item["length"] <= MAX_BYTES:
        raise ValueError(f"capture {text!r}: between 1 and {MAX_BYTES} bytes")
    item["size"] = (item["length"] + 3) & ~3
    return item


def load(path: Path) -> dict:
    cfg = tomllib.loads(path.read_text(encoding="utf-8"))
    gdb, blob = cfg.setdefault("gdb", {}), cfg.setdefault("blob", {})
    gdb.setdefault("host", "127.0.0.1")
    gdb.setdefault("port", 2345)
    blob.setdefault("address", 0x80200000)
    blob.setdefault("bytes", 0x100000)
    blob.setdefault("poke", [])
    cfg.setdefault("memory", {}).setdefault("shared", False)
    cfg["_dir"] = path.resolve().parent                                   # paths in the config are from the config's own directory
    cfg["_out"] = (cfg["_dir"] / cfg.get("log", {}).get("dir", ".")).resolve()
    at = cfg.get("install_at", [])
    cfg["install_at"] = [at] if isinstance(at, int) else list(at)
    problems, seen = [], {}
    for event in cfg.get("event", []):
        for field in ("id", "name", "address"):
            if field not in event:
                problems.append(f"an event lacks `{field}`: {event}")
        if not 0 < event.get("id", 0) < 0x10000:
            problems.append(f"{event.get('name')}: id must be between 1 and 65535")
        if event.get("id") in seen:
            problems.append(f"event id {event['id']} is used by {seen[event['id']]} and {event.get('name')}")
        seen[event.get("id")] = event.get("name")
        event.setdefault("capture", ["a0", "a1", "a2", "a3", "ra"])
        event.setdefault("exits", [])
        event.setdefault("exit_capture", ["v0"])
        event.setdefault("drain", False)
        # An event with a handler stops the game whatever else it does, so it needs no hook: it is a breakpoint
        # on its own address. Nothing is displaced, so any instruction will do - a jump or a branch included.
        event["_break"] = bool(event.get("handler"))
        if event["_break"] and event["exits"]:
            problems.append(f"{event.get('name')}: an event with a handler is a breakpoint at its address and has no exits; give its returns to a second event")
        try:
            event["_enter"] = [parse_capture(c) for c in event["capture"]]
            event["_exit"] = [parse_capture(c) for c in event["exit_capture"]]
        except ValueError as error:
            problems.append(f"{event.get('name')}: {error}")
    if blob["address"] % 16:
        problems.append("blob.address must be a multiple of 16")
    if blob["bytes"] % 4 or blob["bytes"] < 0x1000:
        problems.append("blob.bytes must be a multiple of four, and at least 4096")
    if problems:
        raise SystemExit("\n".join(problems))
    return cfg


# ---- python hooks: files the config names, whose functions the tracer calls for an event.
# A renderer turns an event's captures into what the log should say; a handler acts on the event, game stopped.

RENDERERS: dict[str, object] = {}


def renderer(name: str):
    """Register a function as the renderer `name`. An event asks for it with `render = "name"`.

        from gametrace import renderer

        @renderer("file_load")
        def file_load(record, trace):
            ...

    `record` is the event as captured: `id`, `name`, `phase`, and each capture under its own text - a register as
    an int, memory as bytes. `trace` gives `trace.symbol(address)` (the function an address is in), `trace.note(text)`
    (an annotation line in the log), `trace.write(text)` (a raw line of its own) and `trace.out` (the log file). What the function returns is what is logged:
    a dict is written as a JSON line, a string as a line of text, and None means the renderer wrote what it
    wanted itself, or wants the event left out."""
    def register(function):
        RENDERERS[name] = function
        return function
    return register


HANDLERS: dict[str, object] = {}
FILTERS: dict[str, object] = {}
COMMANDS: dict[str, object] = {}


def command(name: str):
    """Register a function as the command `name`: something to type while the tracer runs.

        @command("flag")
        def flag(trace, text):
            "flag ID VALUE   set a game flag"
            ...

    The game is stopped while it runs, and it runs to its end before anything else can be typed. It gets the
    tracer and the rest of the line. It can talk to whoever typed it - `trace.say(text)`, `trace.ask(prompt)` for
    a line, `trace.choose(title, options)` to have one picked from a list. To act on the game it has the tracer's
    primitives, which check nothing: `registers()`, `where()`, `step()`, `run_to(address)`, `read(address,
    length)`, `poke(address, bytes)`, `address("symbol")`, `inject(source)` / `free(labels)`, `hook(address,
    target)` / `unhook(address)`, and `call(function, a0, a1, a2, a3)` built from them. Where it is safe to act,
    and when a hook it placed may go, is the command's own to decide. `trace.note(text)` writes into the log.
    What it returns is printed; its docstring's first line is its entry in `help`. An error it raises is printed
    and nothing else happens."""
    def register(function):
        COMMANDS[name] = function
        return function
    return register


def symbol_address(name: str) -> int | None:
    """Where a symbol of the game is: looked for in the images in memory from the start, then in the rest."""
    for item in sorted(IMAGES.values(), key=lambda i: not i.start):
        for address, known in item.table:
            if known == name:
                return address
    return None


def filter(name: str):  # noqa: A001 - the name a hooks file imports
    """Register a class as the filter `name`. The config's `[log] filters = ["name", ...]` puts filters between the
    events and the log: every line about to be written goes through them in turn, and each says what is written
    in its place - the line itself, nothing, or several lines.

        @filter("name")
        class Mine:
            def line(self, line, event, trace):    # a line (a dict) of `event` (its config entry); return a list of lines
                return [line]
            def end(self, trace):                  # give up anything held back: called before a session's marker
                return []                          # and at the end of the log

    One object is made per run, so a filter may keep what it needs between lines. `repeats` is built in."""
    def register(cls):
        FILTERS[name] = cls
        return cls
    return register


def handler(name: str):
    """Register a function as the handler `name`. An event asks for it with `handler = "name"`.

    Where a renderer says how an event is written, a handler acts on it: the game is stopped at the end of the
    event's hook until the handler has returned. It gets the same `record` and `trace` as a renderer, and `trace`
    then also offers

        trace.resident("overlay")      the image is in memory, whole, from now: write the hooks of its events
        trace.read(address, length)    the console's memory as it is at this moment

    The tracer does not work out by itself when an image is loaded: a handler on the game's own "this file has
    finished loading" point tells it. What a handler returns is ignored."""
    def register(function):
        HANDLERS[name] = function
        return function
    return register


def load_hooks(cfg: dict, config_path: Path) -> None:
    import importlib.util
    sys.modules.setdefault("gametrace", sys.modules[__name__])       # so a hooks file can `from gametrace import renderer, handler`
    files = (cfg.get("hooks") or {}).get("files", [])
    for n, name in enumerate([files] if isinstance(files, str) else files):
        path = next((p for p in (config_path.parent / name, Path(name)) if p.is_file()), None)
        if path is None:
            raise SystemExit(f"hooks file {name} not found (looked beside the config and from the current directory)")
        spec = importlib.util.spec_from_file_location(f"gametrace_hooks_{n}", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    for entry in cfg.get("image", []):                                     # images the config declares itself
        image(entry["name"], entry["load"], config_path.parent / entry["file"], skip=entry.get("skip", 0), start=entry.get("start", False),
              symbols=[config_path.parent / name for name in ([entry["symbols"]] if isinstance(entry.get("symbols"), str) else entry.get("symbols", []))])
    for function in INITS:
        function(cfg)
    problems = []
    for event in cfg.get("event", []):                                     # which image each event's address is in
        if event.get("image") is None:
            event["image"] = next((i.name for i in IMAGES.values() if i.start and i.words(event["address"], 2)), None)
            if event["image"] is None:
                problems.append(f"{event['name']}: {event['address']:#010x} is in no image that is in memory from the start; name its image with `image = \"...\"`")
        elif event["image"] not in IMAGES:
            problems.append(f"{event['name']}: no image is called {event['image']!r}; declare it in an init hook or an [[image]] table")
    if problems:
        raise SystemExit("\n".join(problems))
    missing = sorted({e["render"] for e in cfg.get("event", []) if e.get("render") and e["render"] not in RENDERERS})
    if missing:
        raise SystemExit(f"no renderer named {', '.join(missing)}; the hooks files register: {', '.join(sorted(RENDERERS)) or 'none'}")
    missing = sorted(set(cfg.get("log", {}).get("filters", [])) - set(FILTERS))
    if missing:
        raise SystemExit(f"no filter named {', '.join(missing)}; there are: {', '.join(sorted(FILTERS)) or 'none'}")
    missing = sorted({e["handler"] for e in cfg.get("event", []) if e.get("handler") and e["handler"] not in HANDLERS})
    if missing:
        raise SystemExit(f"no handler named {', '.join(missing)}; the hooks files register: {', '.join(sorted(HANDLERS)) or 'none'}")


def record_size(captures: list[dict]) -> int:
    return 4 + sum(c["size"] for c in captures)


def header(event: dict, phase: int, captures: list[dict]) -> int:
    """A record's first word: the event's id, whether it is an exit, and the record's whole length in bytes."""
    return event["id"] | (phase << 16) | (record_size(captures) << 17)


# ---- the project: the images of code the game is made of. The tracer knows nothing about any game; a hooks file's
# init function, or the config's [[image]] tables, say what there is.

class Image:
    """A block of code as it is on disc: where it loads, its bytes, and the names of the addresses in it.

    `data` is the bytes, a file holding them, or a function returning them; `skip` is how much of a file comes
    before the byte that is loaded at `load` (a header). `symbols` is (address, name) pairs, a file of
    `name = 0xADDRESS;` lines, several such files, or a function returning pairs. Files are read when first needed.
    `start` says the image is in memory when the tracer sets up; any other is written to only once a handler,
    `--resident` or the `resident` command has said it is there."""

    def __init__(self, name: str, load: int, data, symbols=(), skip: int = 0, start: bool = False) -> None:
        self.name, self.load, self.skip, self.start = name, load, skip, start
        self._data, self._symbols, self._bytes, self._table = data, symbols, None, None

    @property
    def bytes(self) -> bytes:
        if self._bytes is None:
            data = self._data() if callable(self._data) else self._data
            if isinstance(data, (str, Path)):
                data = Path(data).read_bytes() if Path(data).is_file() else b""
            self._bytes = bytes(data or b"")[self.skip:]
        return self._bytes

    @property
    def end(self) -> int:
        return self.load + len(self.bytes)

    def words(self, address: int, count: int) -> tuple[int, ...] | None:
        """`count` instructions at `address`, or None when the image does not hold them all."""
        if self.load <= address and address + 4 * count <= self.end:
            return struct.unpack_from(f"<{count}I", self.bytes, address - self.load)
        return None

    @property
    def table(self) -> list[tuple[int, str]]:
        """The image's own symbols in address order. A symbol file also lists what its image only refers to, and
        those are not places inside it."""
        if self._table is None:
            found = self._symbols() if callable(self._symbols) else self._symbols
            if isinstance(found, (str, Path)):
                found = [found]
            pairs: list = []
            for item in found or ():
                pairs += symbols_in(item) if isinstance(item, (str, Path)) else [tuple(item)]
            self._table = sorted((a, n) for a, n in pairs if self.load <= a < self.end)
        return self._table


IMAGES: dict[str, Image] = {}
INITS: list = []


def image(name: str, load: int, data, symbols=(), skip: int = 0, start: bool = False) -> Image:
    """Declare an image (see `Image`). For an init function; the config's [[image]] tables do the same."""
    IMAGES[name] = Image(name, load, data, symbols, skip, start)
    return IMAGES[name]


def init(function):
    """Register a function a hooks file wants called once, with the config, before anything is resolved: the place
    to declare the project's images.

        from gametrace import image, init

        @init
        def project(cfg):
            image("main", 0x80010000, "path/to/program.bin", symbols="path/to/symbols.txt", start=True)"""
    INITS.append(function)
    return function


def symbols_in(path) -> list[tuple[int, str]]:
    """The (address, name) pairs of a file of `name = 0xADDRESS;` lines."""
    line = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]{8})\s*;")
    try:
        text = Path(path).read_text(errors="replace")
    except OSError:
        return []
    return [(int(m[2], 16), m[1]) for m in map(line.match, text.splitlines()) if m]


def name_at(address: int, images) -> str:
    """The symbol at or before `address` in the images that hold it; an address in none of them is left as a number."""
    best = None
    for item in images:
        if not item.load <= address < item.end:
            continue
        i = bisect_right(item.table, (address, chr(0xFFFF))) - 1
        if i >= 0 and address - item.table[i][0] < 0x4000 and (best is None or item.table[i][0] > best[0]):
            best = item.table[i]
    return f"{best[1]}+{address - best[0]:#x}" if best else f"{address:#010x}"


def started() -> list[Image]:
    return [item for item in IMAGES.values() if item.start]


# ---- MIPS: which instructions may be moved out of a function's entry

def movable(word: int) -> str | None:
    """Why this instruction cannot be executed somewhere else, or None when it can."""
    op, funct, rt = word >> 26, word & 0x3F, (word >> 16) & 0x1F
    if op in (2, 3):
        return "a jump"
    if op == 0 and funct in (8, 9):
        return "a register jump"
    if op in (4, 5, 6, 7) or (op == 1 and rt in (0, 1, 16, 17)):
        return "a branch"
    if op in (0x10, 0x11, 0x12, 0x13) and ((word >> 21) & 0x1F) == 8:
        return "a coprocessor branch"
    return None



# ---- the blob: the buffer's cursor, the one place the game stops when the buffer is full, and a stub per hook

def capture_code(captures: list[dict], tag: str) -> list[str]:
    """Instructions that store each capture at its place in the record. In: t1 = the record's address; the hooked
    code's ra, t0, t1, t2 are in the stub's frame and its sp is FRAME above. t0 and t2 are free."""
    lines, at = [], 4
    def value(reg: str, into: str) -> list[str]:
        if reg in SAVED:
            return [f"    lw    ${into}, {SAVED[reg]}($sp)", "    nop"]
        if reg == "sp":
            return [f"    addiu ${into}, $sp, {FRAME}"]
        return [f"    addu  ${into}, ${reg}, $zero"]
    for n, c in enumerate(captures):
        lines.append(f"    # {c['text']}")
        if c["kind"] == "reg":
            if c["reg"] in SAVED or c["reg"] == "sp":
                lines += value(c["reg"], "t0") + [f"    sw    $t0, {at}($t1)"]
            else:
                lines.append(f"    sw    ${c['reg']}, {at}($t1)")
        else:
            words = range(0, c["size"], 4)
            if c["kind"] == "fixed":
                lines.append(f"    li    $t0, {c['address']:#010x}")
            else:
                lines += value(c["reg"], "t0")
                if c["offset"]:
                    lines.append(f"    addiu $t0, $t0, {c['offset']}")
                # Only an address inside the console's memory is read; anything else would fault.
                lines += ["    srl   $t2, $t0, 23", "    xori  $t2, $t2, 0x100", f"    bnez  $t2, {tag}_zero{n}", "    nop"]
            for w in words:                            # unaligned, a word at a time; a load is not usable in the next instruction
                lines += [f"    lwr   $t2, {w}($t0)", f"    lwl   $t2, {w + 3}($t0)", "    nop", f"    sw    $t2, {at + w}($t1)"]
            if c["kind"] == "mem":
                lines += [f"    b     {tag}_done{n}", "    nop", f"{tag}_zero{n}:"]
                lines += [f"    sw    $zero, {at + w}($t1)" for w in words]
                lines.append(f"{tag}_done{n}:")
        at += c["size"]
    return lines


def record_code(event: dict, phase: int, captures: list[dict], tag: str, largest: int, pokes: list) -> list[str]:
    """A stub's middle: save what it uses, make room, write the record, restore."""
    size = record_size(captures)
    unlock = []
    for address, value in pokes:                       # see `blob.poke`: what has to hold before the blob's memory is touched
        high = ((address + 0x8000) >> 16) & 0xFFFF
        low = (address & 0xFFFF) - (0x10000 if address & 0x8000 else 0)
        unlock += [f"    li    $t0, {value:#010x}", f"    lui   $t2, {high:#06x}", f"    sw    $t0, {low}($t2)"]
    return [
        f"    addiu $sp, $sp, -{FRAME}",
        "    sw    $ra, 0($sp)",
        "    sw    $t0, 4($sp)",
        "    sw    $t1, 8($sp)",
        "    sw    $t2, 12($sp)",
        "    mfc0  $t0, $12              # no interrupt lands in the middle of a record",
        "    nop",
        "    sw    $t0, 16($sp)",
        "    addiu $t2, $zero, -2",
        "    and   $t0, $t0, $t2",
        "    mtc0  $t0, $12",
        "    nop",
        *unlock,
        "    lui   $t0, %hi(gt_cursor)",
        "    lw    $t1, %lo(gt_cursor)($t0)",
        "    lui   $t2, %hi(gt_end)",
        "    addiu $t2, $t2, %lo(gt_end)",
        f"    addiu $t0, $t1, {size}",
        "    sltu  $t0, $t2, $t0         # does this record fit?",
        f"    beqz  $t0, {tag}_room",
        "    nop",
        "    jal   gt_drain",
        "    nop",
        f"{tag}_room:",
        f"    li    $t0, {header(event, phase, captures):#010x}",
        "    sw    $t0, 0($t1)",
        *capture_code(captures, tag),
        f"    addiu $t1, $t1, {size}",
        "    lui   $t0, %hi(gt_cursor)",
        "    sw    $t1, %lo(gt_cursor)($t0)",
        "    lui   $t2, %hi(gt_end)",
        "    addiu $t2, $t2, %lo(gt_end)",
        f"    addiu $t0, $t1, {largest}",
        "    sltu  $t0, $t2, $t0         # and is there room for the largest record after it?",
        f"    beqz  $t0, {tag}_out",
        "    nop",
        "    jal   gt_drain",
        "    nop",
        f"{tag}_out:",
        "    lw    $t0, 16($sp)",
        "    nop",
        "    mtc0  $t0, $12",
        "    lw    $ra, 0($sp)",
        "    lw    $t0, 4($sp)",
        "    lw    $t1, 8($sp)",
        "    lw    $t2, 12($sp)",
        f"    addiu $sp, $sp, {FRAME}",
    ]


def stub_source(cfg: dict, sites: dict) -> str:
    blob = cfg["blob"]
    events = [e for e in cfg.get("event", []) if not e["_break"]]
    largest = max([record_size(e["_enter"]) for e in events] + [record_size(e["_exit"]) for e in events if e["exits"]] + [4])
    lines = [
        ".set noreorder", ".set noat", ".text",
        "# where the next record goes",
        "gt_cursor: .word gt_buffer",
        "",
        "# The buffer cannot take the record. The tracer holds a breakpoint on gt_full and reads everything from",
        "# gt_buffer to the cursor before the next instruction starts the buffer again; with no tracer attached the",
        "# old records are simply dropped. Out: t1 = where to write. Uses t0.",
        "gt_drain:",
        "gt_full:",
        "    lui   $t1, %hi(gt_buffer)",
        "    addiu $t1, $t1, %lo(gt_buffer)",
        "    lui   $t0, %hi(gt_cursor)",
        "    jr    $ra",
        "    sw    $t1, %lo(gt_cursor)($t0)",
        "",
    ]
    for event in events:
        first, second = sites[(event["id"], "enter")]
        lines += [f"# event {event['id']}: {event['name']} at {event['address']:#010x}", f"gt_enter_{event['id']}:"]
        lines += record_code(event, 0, event["_enter"], f"e{event['id']}", largest, blob["poke"])
        lines += [f"    .word {first:#010x}          # the function's own first instruction", f"    .word {second:#010x}          # and its second",
                  f"gt_enter_{event['id']}_done:",
                  f"    j     {event['address'] + 8:#010x}", "    nop", ""]
        for k, offset in enumerate(event["exits"]):
            _jr, delay = sites[(event["id"], "exit", offset)]
            lines += [f"# event {event['id']}: {event['name']} returns at +{offset:#x}", f"gt_exit_{event['id']}_{k}:",
                      f"    .word {delay:#010x}          # the return's delay-slot instruction, which ran before the return did"]
            lines += record_code(event, 1, event["_exit"], f"x{event['id']}_{k}", largest, blob["poke"])
            lines += [f"gt_exit_{event['id']}_{k}_done:", "    jr    $ra", "    nop", ""]
    lines += ["    .balign 4", "gt_buffer:", f"    .space {blob['bytes']}", "gt_end:", ""]
    return "\n".join(lines)


CALL_SAVED = ["ra", "a0", "a1", "a2", "a3", "v0", "v1", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9"]


def call_source(target: int, args: list[int], back: int, pokes: list) -> str:
    """The code of one call into the game: keep the registers, call `target` with its arguments, keep what it
    returned, put the registers back, and go on at `back` - where the game was when it was sent here."""
    frame = 16 + 4 * len(CALL_SAVED)                   # the callee's argument area, then the registers
    unlock = []
    for address, value in pokes:                       # the blob's memory has to be reachable before `result` is written
        high = ((address + 0x8000) >> 16) & 0xFFFF
        low = (address & 0xFFFF) - (0x10000 if address & 0x8000 else 0)
        unlock += [f"    li    $t0, {value:#010x}", f"    lui   $t2, {high:#06x}", f"    sw    $t0, {low}($t2)"]
    return "\n".join([
        "run:",
        f"    addiu $sp, $sp, -{frame}",
        *[f"    sw    ${name}, {16 + 4 * n}($sp)" for n, name in enumerate(CALL_SAVED)],
        *unlock,
        *[f"    li    $a{n}, {value & 0xFFFFFFFF:#010x}" for n, value in enumerate(args)],
        f"    li    $t9, {target:#010x}",
        "    jalr  $t9",
        "    nop",
        "    lui   $t0, %hi(result)",
        "    sw    $v0, %lo(result)($t0)",
        *[f"    lw    ${name}, {16 + 4 * n}($sp)" for n, name in enumerate(CALL_SAVED)],
        f"    addiu $sp, $sp, {frame}",
        "done:",
        f"    j     {back:#010x}",
        "    nop",
        "result: .word 0",
        "",
    ])


# ---- an assembler for exactly what stub_source writes, so the tool needs nothing installed

REG_NUMBER = {name: n for n, name in enumerate(REGS)}
OPERAND = re.compile(r"\s*,\s*")


class Assembler:
    """Two passes over the blob's source: where every label is, then the words.

    It knows every instruction of the console's processor (MIPS I, with the system-control and GTE coprocessors;
    a GTE command is `cop2 NUMBER`) and the short forms people write without thinking - nop move li la b bal beqz
    bnez neg negu not; `.word`, `.balign` and `.space`; labels, `%hi()` and `%lo()`. A name that is no label of
    the source is looked up among the game's symbols (`names`). It does not reorder anything: a delay slot is
    written where it goes. Anything it does not know stops with the line that has it."""

    def __init__(self, base: int) -> None:
        self.base = base
        self.symbols: dict[str, int] = {}
        self.names = None                             # name -> address, for what is no label of the source

    def number(self, text: str) -> int:
        text = text.strip()
        m = re.fullmatch(r"%(hi|lo)\((\w+)\)", text)
        if m:
            address = self.symbols[m[2]]
            return ((address + 0x8000) >> 16) & 0xFFFF if m[1] == "hi" else address & 0xFFFF
        if text in self.symbols:
            return self.symbols[text]
        try:
            return int(text, 0)
        except ValueError:
            found = self.names(text) if self.names else None
            if found is None:
                raise ValueError(f"`{text}` is not a number, a label, or a symbol of the game")
            return found

    @staticmethod
    def reg(text: str) -> int:
        return REG_NUMBER[text.strip().lstrip("$")]

    def li(self, value: int) -> int:
        """How many instructions `li` takes for this value, as an assembler chooses them."""
        value &= 0xFFFFFFFF
        return 1 if value < 0x10000 or value >= 0xFFFF8000 else (1 if not value & 0xFFFF else 2)

    def size(self, op: str, args: list[str]) -> int:
        if op == ".space":
            return int(args[0], 0)
        if op == "li":
            return 4 * self.li(int(args[1], 0))
        if op == "la":
            return 8
        return 4

    # The whole of the R3000's instruction set, by how each is written. The numbers are the opcode (or, for the
    # register forms, the function code): MIPS I, plus the two coprocessors a PlayStation has.
    THREE = {"add": 0x20, "addu": 0x21, "sub": 0x22, "subu": 0x23, "and": 0x24, "or": 0x25, "xor": 0x26, "nor": 0x27, "slt": 0x2A, "sltu": 0x2B}
    SHIFT = {"sll": 0x00, "srl": 0x02, "sra": 0x03}                    # rd, rt, amount
    SHIFTV = {"sllv": 0x04, "srlv": 0x06, "srav": 0x07}                # rd, rt, rs
    MULDIV = {"mult": 0x18, "multu": 0x19, "div": 0x1A, "divu": 0x1B}  # rs, rt
    HILO_FROM = {"mfhi": 0x10, "mflo": 0x12}                           # rd
    HILO_TO = {"mthi": 0x11, "mtlo": 0x13}                             # rs
    IMMEDIATE = {"addi": 8, "addiu": 9, "slti": 10, "sltiu": 11, "andi": 12, "ori": 13, "xori": 14}     # rt, rs, value
    MEMORY = {"lb": 0x20, "lh": 0x21, "lwl": 0x22, "lw": 0x23, "lbu": 0x24, "lhu": 0x25, "lwr": 0x26,
              "sb": 0x28, "sh": 0x29, "swl": 0x2A, "sw": 0x2B, "swr": 0x2E}
    MEMORY_COP2 = {"lwc2": 0x32, "swc2": 0x3A}                         # a coprocessor register, offset(base)
    BRANCH2 = {"beq": 4, "bne": 5}                                     # rs, rt, target
    BRANCH1 = {"blez": 6, "bgtz": 7}                                   # rs, target
    BRANCHZ = {"bltz": 0x00, "bgez": 0x01, "bltzal": 0x10, "bgezal": 0x11}     # rs, target (opcode 1, selected by rt)
    COP_MOVE = {"mfc0": (0x10, 0), "mtc0": (0x10, 4), "mfc2": (0x12, 0), "cfc2": (0x12, 2), "mtc2": (0x12, 4), "ctc2": (0x12, 6)}   # rt, coprocessor register

    def encode(self, op: str, a: list[str], pc: int) -> list[int]:
        R = self.reg
        def cop(text: str) -> int:
            return int(text.strip().lstrip("$"), 0)
        def itype(code: int, rs: int, rt: int, imm: int) -> int:
            return (code << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)
        def rtype(funct: int, rs: int, rt: int, rd: int, sa: int = 0) -> int:
            return (rs << 21) | (rt << 16) | (rd << 11) | ((sa & 31) << 6) | funct
        def memory(code: int, rt: int) -> int:
            m = re.fullmatch(r"(.*)\(\s*(\$\w+)\s*\)", a[1].strip())
            return itype(code, R(m[2]), rt, self.number(m[1] or "0"))
        def branch(code: int, rs: int, rt: int, target: str) -> int:
            return itype(code, rs, rt, (self.number(target) - (pc + 4)) >> 2)
        if op == "nop":
            return [0]
        if op == ".word":
            return [self.number(a[0]) & 0xFFFFFFFF]
        if op in self.THREE:
            return [rtype(self.THREE[op], R(a[1]), R(a[2]), R(a[0]))]
        if op in self.SHIFT:
            return [rtype(self.SHIFT[op], 0, R(a[1]), R(a[0]), self.number(a[2]))]
        if op in self.SHIFTV:
            return [rtype(self.SHIFTV[op], R(a[2]), R(a[1]), R(a[0]))]
        if op in self.MULDIV:
            return [rtype(self.MULDIV[op], R(a[-2]), R(a[-1]), 0)]     # `div rs, rt`, or the long form `div $zero, rs, rt`
        if op in self.HILO_FROM:
            return [rtype(self.HILO_FROM[op], 0, 0, R(a[0]))]
        if op in self.HILO_TO:
            return [rtype(self.HILO_TO[op], R(a[0]), 0, 0)]
        if op in self.IMMEDIATE:
            return [itype(self.IMMEDIATE[op], R(a[1]), R(a[0]), self.number(a[2]))]
        if op == "lui":
            return [itype(15, 0, R(a[0]), self.number(a[1]))]
        if op in self.MEMORY:
            return [memory(self.MEMORY[op], R(a[0]))]
        if op in self.MEMORY_COP2:
            return [memory(self.MEMORY_COP2[op], cop(a[0]))]
        if op in self.BRANCH2:
            return [branch(self.BRANCH2[op], R(a[0]), R(a[1]), a[2])]
        if op in self.BRANCH1:
            return [branch(self.BRANCH1[op], R(a[0]), 0, a[1])]
        if op in self.BRANCHZ:
            return [branch(1, R(a[0]), self.BRANCHZ[op], a[1])]
        if op in ("beqz", "bnez"):
            return [branch(4 if op == "beqz" else 5, R(a[0]), 0, a[1])]
        if op == "b":
            return [branch(4, 0, 0, a[0])]
        if op == "bal":
            return [branch(1, 0, 0x11, a[0])]
        if op in ("j", "jal"):
            return [((2 if op == "j" else 3) << 26) | ((self.number(a[0]) >> 2) & 0x03FFFFFF)]
        if op == "jr":
            return [rtype(0x08, R(a[0]), 0, 0)]
        if op == "jalr":                                               # jalr rs   or   jalr rd, rs
            return [rtype(0x09, R(a[-1]), 0, R(a[0]) if len(a) == 2 else 31)]
        if op in ("syscall", "break"):
            return [((self.number(a[0]) if a else 0) << (6 if op == "syscall" else 16)) | (0x0C if op == "syscall" else 0x0D)]
        if op in self.COP_MOVE:
            code, sub = self.COP_MOVE[op]
            return [(code << 26) | (sub << 21) | (R(a[0]) << 16) | (cop(a[1]) << 11)]
        if op == "rfe":
            return [0x42000010]
        if op == "cop2":                                               # a GTE command, by its 25-bit number
            return [(0x12 << 26) | (1 << 25) | (self.number(a[0]) & 0x01FFFFFF)]
        if op == "move":
            return [rtype(0x21, R(a[1]), 0, R(a[0]))]                  # addu rd, rs, $zero, as the game's own code has it
        if op in ("neg", "negu"):
            return [rtype(0x22 if op == "neg" else 0x23, 0, R(a[-1]), R(a[0]))]
        if op == "not":
            return [rtype(0x27, R(a[-1]), 0, R(a[0]))]                 # nor rd, rs, $zero
        if op == "li":
            value, rt = int(a[1], 0) & 0xFFFFFFFF, R(a[0])
            if value >= 0xFFFF8000 or value < 0x8000:
                return [itype(9, 0, rt, value)]                       # addiu rt, $zero, value
            if value < 0x10000:
                return [itype(13, 0, rt, value)]                      # ori rt, $zero, value
            return [itype(15, 0, rt, value >> 16)] + ([itype(13, rt, rt, value)] if value & 0xFFFF else [])
        if op == "la":                                                 # always two instructions, so its size never depends on the address
            value, rt = self.number(a[1]) & 0xFFFFFFFF, R(a[0])
            return [itype(15, 0, rt, ((value + 0x8000) >> 16)), itype(9, rt, rt, value)]
        raise ValueError(f"the assembler has no instruction `{op}`")

    def lines(self, source: str):
        for number, raw in enumerate(source.splitlines(), 1):
            text = raw.split("#", 1)[0].strip()
            while True:
                m = re.match(r"(\w+):\s*", text)
                if not m:
                    break
                yield number, m[1], None, None
                text = text[m.end():]
            if text and not text.startswith((".set", ".text")):
                op, _, rest = text.partition(" ")
                yield number, None, op, [x for x in OPERAND.split(rest.strip()) if x] if rest.strip() else []

    def assemble(self, source: str) -> tuple[bytes, list[tuple[int, int, str]]]:
        """(the bytes up to the first `.space`, a listing of (address, word, source line))."""
        pc = self.base
        for number, label, op, args in self.lines(source):
            if label:
                self.symbols[label] = pc
            elif op == ".balign":
                pc = (pc + int(args[0], 0) - 1) & -int(args[0], 0)
            else:
                pc += self.size(op, args)
        out, listing, pc, text = bytearray(), [], self.base, source.splitlines()
        for number, label, op, args in self.lines(source):
            if label:
                continue
            if op == ".balign":
                aligned = (pc + int(args[0], 0) - 1) & -int(args[0], 0)
                out += bytes(aligned - pc)
                pc = aligned
                continue
            if op == ".space":
                break                                  # the buffer: its place is known, its bytes are not written
            try:
                words = self.encode(op, args, pc)
            except (KeyError, ValueError, AttributeError, IndexError) as error:
                raise SystemExit(f"blob source line {number}: {text[number - 1].strip()}\n{error}")
            for word in words:
                listing.append((pc, word, text[number - 1].strip()))
                out += struct.pack("<I", word)
                pc += 4
        return bytes(out), listing


def assemble(cfg: dict, sites: dict) -> tuple[bytes, dict[str, int], str]:
    """(the blob's code, symbol -> address, listing). The buffer is not part of what is written into the game:
    only the cursor in front of it has to be right."""
    source = stub_source(cfg, sites)
    asm = Assembler(cfg["blob"]["address"])
    code, rows = asm.assemble(source)
    names = {address: name for name, address in asm.symbols.items()}
    listing = "\n".join((f"\n{names[address]}:\n" if address in names else "") + f"{address:08x}:  {word:08x}  {line}" for address, word, line in rows) + "\n"
    return code, {name: address for name, address in asm.symbols.items() if name.startswith("gt_")}, listing


def binutils_agree(cfg: dict, sites: dict, code: bytes) -> str | None:
    """Assemble the same source with the system's MIPS binutils, where they are installed, and compare.
    None when they are not; otherwise a line saying whether the bytes are the same."""
    import shutil
    import tempfile
    if not all(shutil.which(f"mipsel-linux-gnu-{tool}") for tool in ("as", "ld", "objcopy")):
        return None
    with tempfile.TemporaryDirectory() as tmp:
        src, obj, elf, raw = (Path(tmp) / name for name in ("blob.s", "blob.o", "blob.elf", "blob.bin"))
        src.write_text(stub_source(cfg, sites), encoding="utf-8")
        for cmd in (["mipsel-linux-gnu-as", "-mips1", "-EL", "-o", str(obj), str(src)],
                    ["mipsel-linux-gnu-ld", "-EL", "-Ttext", f"{cfg['blob']['address']:#x}", "-e", "gt_drain", "-o", str(elf), str(obj)],
                    ["mipsel-linux-gnu-objcopy", "-O", "binary", "-j", ".text", str(elf), str(raw)]):
            done = subprocess.run(cmd, capture_output=True, text=True)
            if done.returncode:
                return f"binutils could not assemble the blob: {done.stderr.strip()[:200]}"
        theirs = raw.read_bytes()[: len(code)]
    if theirs == code:
        return f"the same {len(code)} bytes as the system's MIPS binutils produce"
    first = next(n for n in range(0, len(code), 4) if theirs[n:n + 4] != code[n:n + 4])
    return f"DIFFERS from the system's MIPS binutils at offset {first:#x}: {code[first:first + 4][::-1].hex()} here, {theirs[first:first + 4][::-1].hex()} there"


# ---- what a hook replaces, read from the image on disc

def image_words(address: int, count: int, image: str) -> tuple[int, ...] | None:
    """`count` instructions at `address` in the image of that name."""
    return IMAGES[image].words(address, count) if image in IMAGES else None


def sites_of(cfg: dict) -> dict:
    """Every place a hook goes and the two instructions there now: (id, "enter") and (id, "exit", offset)."""
    out, problems = {}, []
    for event in cfg.get("event", []):
        name, image = event["name"], event["image"]
        if event["_break"]:
            if image_words(event["address"], 1, image) is None:
                problems.append(f"{name}: {event['address']:#010x} is not in the image {image} as it is on disc")
            continue
        words = image_words(event["address"], 2, image)
        if words is None:
            problems.append(f"{name}: {event['address']:#010x} is not in the image {image} as it is on disc")
            continue
        for n, word in enumerate(words):
            if movable(word):
                problems.append(f"{name}: instruction {n + 1} at its entry ({word:#010x}) is {movable(word)} and cannot be displaced")
        out[(event["id"], "enter")] = words
        # A hook replaces two instructions with a jump and a no-op. Code that branches to the second of them would
        # land on the no-op and skip the instruction that used to be there.
        # An event's address need not be a function's first instruction, so the code before it is looked at too.
        back = next((b for b in (0x2000, 0x800, 0x100, 0) if image_words(event["address"] - b, 1, image)), 0)
        first = event["address"] - back
        body = image_words(first, (back + 0x1000) // 4, image) or image_words(first, (back + 0x100) // 4, image) or ()
        ends = {event["address"] + 4} | {event["address"] + o + 4 for o in event["exits"]}
        for n, word in enumerate(body):
            op, rt = word >> 26, (word >> 16) & 0x1F
            if op in (4, 5, 6, 7) or (op == 1 and rt in (0, 1, 16, 17)):
                target = first + 4 * n + 4 + ((word & 0xFFFF) - (0x10000 if word & 0x8000 else 0)) * 4
                if target in ends:
                    problems.append(f"{name}: a branch at {first + 4 * n:#010x} goes to {target:#010x}, the second instruction of a hook; that place cannot be hooked")
        for offset in event["exits"]:
            pair = image_words(event["address"] + offset, 2, image)
            if pair is None or pair[0] != JR_RA:
                problems.append(f"{name}: no `jr $ra` at +{offset:#x} ({'outside the image' if pair is None else f'{pair[0]:#010x}'}); `check` lists where they are")
            elif movable(pair[1]):
                problems.append(f"{name}: the delay slot of the return at +{offset:#x} ({pair[1]:#010x}) is {movable(pair[1])}")
            elif offset in (0, 4):
                problems.append(f"{name}: the return at +{offset:#x} overlaps the entry hook")
            else:
                out[(event["id"], "exit", offset)] = pair
    if problems:
        raise SystemExit("\n".join(problems))
    return out


def returns_in(event: dict) -> list[int]:
    """The offsets of the `jr $ra` instructions between the event's address and the next symbol."""
    image = event["image"]
    table = IMAGES[image].table
    later = sorted(a for a, _ in table if a > event["address"])
    span = min((later[0] - event["address"]) if later else 0x400, 0x4000)
    words = image_words(event["address"], span // 4, image) or ()
    return [4 * n for n, word in enumerate(words) if word == JR_RA]


# ---- the GDB remote protocol. Only the packets every stub has are used: ? g m M c Z z
# (memory is written with `M`, in hex, because not every server takes the binary `X`).

class Gdb:
    def __init__(self, host: str, port: int) -> None:
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(None)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)   # every packet is small and waits for its answer
        self.buf = b""
        self.set: set[int] = set()                    # the breakpoints in place
        self.doubles: bool | None = None              # whether this server reports a breakpoint stop twice (not yet known)
        self.protocol = None                          # a file every packet is written to, for --protocol
        self.step_over: bool | None = None            # whether this server needs it; found out at the first stop
        self.chunk = 0x400                            # bytes per memory read; raised to what the server's packets hold
        self.sock.sendall(b"+")

    def note(self, way: str, text: str) -> None:
        if self.protocol is not None:
            self.protocol.write(f"{time.monotonic():.4f} {way} {text[:120]}\n")
            self.protocol.flush()

    def _send(self, payload: str) -> None:
        self.note(">", payload)
        body = payload.encode()
        self.sock.sendall(b"$" + body + b"#" + f"{sum(body) & 0xFF:02x}".encode())

    def _recv(self, wait: float | None = None) -> str | None:
        """The next packet; with `wait`, None when none arrives within that many seconds."""
        while True:
            start = self.buf.find(b"$")
            end = self.buf.find(b"#", start + 1) if start >= 0 else -1
            if start >= 0 and end >= 0 and len(self.buf) >= end + 3:
                packet, self.buf = self.buf[start + 1:end], self.buf[end + 3:]
                self.sock.sendall(b"+")
                self.note("<", packet.decode(errors="replace"))
                return packet.decode(errors="replace")
            if wait is not None and not select.select([self.sock], [], [], wait)[0]:
                return None
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("the GDB server closed the connection")
            self.buf += chunk

    def settle(self, stop: str) -> None:
        """A stop has been reported: wait until the server is done stopping before anything is asked of it.

        Some servers report a breakpoint twice - once when it is hit and once when the machine has actually come
        to rest. Between the two the machine is still on its way to stopping, and a continue sent then is
        overtaken: the machine resumes, then finishes its pause, and stays stopped with nobody told. So where a
        server is seen to report twice, the second report is waited for (half a second at most) at every
        breakpoint stop; a stop this tool asked for with an interrupt is reported once and is not waited on."""
        if stop[:3] == "S05" and self.doubles is not False:
            second = self._recv(0.5 if self.doubles else 0.3)
            if self.doubles is None:
                self.doubles = second is not None and self._is_stop(second)
            elif second is None:
                print("the server's second report of a stop did not come within half a second; going on without it", file=sys.stderr)
        while self._recv(0.005) is not None:          # anything else it sends about the same stop
            pass

    @staticmethod
    def _is_stop(packet: str) -> bool:
        # A stop reply is S or T and a signal number. No other reply this tool reads can look like one: memory and
        # registers are lower-case hex, and the rest are "OK", an "E" error, or empty.
        return len(packet) >= 3 and packet[0] in "ST" and all(c in "0123456789abcdefABCDEF" for c in packet[1:3])

    def ask(self, payload: str) -> str:
        """Send a query and return its reply. A server can report one stop twice (once for the breakpoint and once
        for the pause that follows); the extra report arrives in front of the next reply and is passed over."""
        self._send(payload)
        while True:
            packet = self._recv()
            if not self._is_stop(packet):
                return packet

    def halt_reason(self) -> str:
        size = re.search(r"PacketSize=([0-9a-fA-F]+)", self.ask("qSupported") or "")
        if size:                                      # a reply is two hex digits a byte, plus its frame
            self.chunk = max(0x100, min(0x4000, int(size[1], 16) // 2 - 16))
        self._send("?")
        return self._recv()

    def registers(self) -> list[int]:
        """Every register of the `g` reply, in its order. A server writes one it does not have (the FPU's, on this
        CPU) as `xxxxxxxx`; those read as zero here, and nothing this tool uses is among them."""
        reply = self.ask("g")
        if reply.startswith("E") or len(reply) < 8 * (PC + 1):
            raise OSError(f"the GDB server's register reply is too short or an error: {reply[:40]!r}")
        out = []
        for at in range(0, len(reply) - 7, 8):
            word = reply[at:at + 8]
            out.append(0 if "x" in word.lower() else int.from_bytes(bytes.fromhex(word), "little"))
        return out

    def read(self, address: int, length: int) -> bytes:
        out = b""
        while len(out) < length:                      # in pieces: a reply is twice its length in hex
            n = min(self.chunk, length - len(out))
            reply = self.ask(f"m{address + len(out):x},{n:x}")
            if reply.startswith("E") or not reply:
                raise OSError(f"the GDB server refused to read {n} bytes at {address + len(out):#010x}: {reply!r}")
            out += bytes.fromhex(reply)
        return out

    def write(self, address: int, data: bytes) -> None:
        for at in range(0, len(data), 0x200):
            piece = data[at:at + 0x200]
            reply = self.ask(f"M{address + at:x},{len(piece):x}:{piece.hex()}")
            if reply != "OK":
                raise OSError(f"the GDB server refused to write {len(piece)} bytes at {address + at:#010x}: {reply!r}")

    def breakpoint(self, address: int, on: bool = True) -> None:
        reply = self.ask(f"{'Z' if on else 'z'}1,{address:x},4")
        if reply != "OK":
            raise OSError(f"the GDB server refused a breakpoint at {address:#010x}: {reply!r}")
        (self.set.add if on else self.set.discard)(address)

    def _go(self, command: str, timeout: float | None = None) -> bool:
        """Send a command that runs the target and wait for it to stop. With `timeout`, a target still running
        after that many seconds is interrupted. Returns whether it stopped by itself."""
        while self._recv(0) is not None:              # nothing left over from the last stop may answer for this one
            pass
        self._send(command)
        by_itself = True
        while True:
            packet = self._recv(timeout)
            if packet is None:
                self.sock.sendall(b"\x03")
                by_itself, timeout = False, None
            elif self._is_stop(packet):
                break
        self.settle(packet)
        return by_itself

    def learn_step_over(self, here: int) -> None:
        """Find out, once, what this server does when continued with a breakpoint under the program counter.

        Some stop again at once on the same instruction, and a client has to lift the breakpoint and single-step
        past it; others move on by themselves. A second breakpoint on the next instruction tells them apart: the
        target stops there if it moved, and at `here` again if it did not. An instruction that jumps or branches
        cannot be tested this way; then stepping over is assumed, which is correct for both kinds, only slower."""
        if self.step_over is not None:
            return
        word = struct.unpack("<I", self.read(here, 4))[0]
        if movable(word) is not None or here + 4 in self.set:
            self.step_over = True
            return
        self.breakpoint(here + 4)
        self._go("c")
        landed = self.registers()[PC]
        self.breakpoint(here + 4, False)
        self.step_over = landed != here + 4
        if self.step_over and landed == here:         # still on it: move past by hand this once
            self.breakpoint(here, False)
            self._go("s")
            self.breakpoint(here)

    def resume(self, stopped_at: int | None = None, timeout: float | None = None) -> bool:
        """Continue and wait for the next stop; False when `timeout` ran out and the target was interrupted.
        `stopped_at` is where the target is now, for a server that has to be stepped past a breakpoint there."""
        if stopped_at in self.set:
            self.learn_step_over(stopped_at)
            if self.step_over and self.registers()[PC] == stopped_at:
                self.breakpoint(stopped_at, False)
                self._go("s")
                self.breakpoint(stopped_at)
                if self.registers()[PC] in self.set:
                    return True
        return self._go("c", timeout)

    def go(self, stopped_at: int | None = None) -> bool:
        """Let the target run, without waiting for it. False when it is stopped again already: the step past a
        breakpoint under `stopped_at` landed on another one."""
        if stopped_at in self.set:
            self.learn_step_over(stopped_at)
            if self.step_over and self.registers()[PC] == stopped_at:
                self.breakpoint(stopped_at, False)
                self._go("s")
                self.breakpoint(stopped_at)
                if self.registers()[PC] in self.set:
                    return False
        while self._recv(0) is not None:
            pass
        self._send("c")
        return True

    def wait(self, also: list, timeout: float | None = None) -> str:
        """Block until the running target stops ("stop"), one of the files in `also` has something to read ("input"),
        or `timeout` seconds pass ("tick")."""
        while True:
            packet = self._recv(0)
            if packet is not None:
                if self._is_stop(packet):
                    self.settle(packet)
                    return "stop"
                continue
            ready = select.select([self.sock, *also], [], [], timeout)[0]
            if not ready:
                return "tick"
            if self.sock in ready:
                chunk = self.sock.recv(65536)
                if not chunk:
                    raise ConnectionError("the GDB server closed the connection")
                self.buf += chunk
            else:
                return "input"

    def interrupt(self) -> None:
        """Stop the running target and wait until it has."""
        self.note(">", "interrupt")
        self.sock.sendall(b"\x03")
        while not self._is_stop(self._recv() or ""):
            pass
        self.settle("S02")

    def release(self, running: bool, resume: bool = True) -> None:
        """Take every breakpoint out - one left behind stops the target with nobody listening - and let the target
        run, or with `resume` false leave it stopped where it is."""
        if running:
            self.sock.sendall(b"\x03")
            while not self._is_stop(self._recv() or ""):
                pass
        for address in sorted(self.set):
            self.breakpoint(address, False)
        if resume:
            self._send("c")


# ---- the console's memory, shared by the emulator: read without asking the server and without stopping the game

class Shared:
    """An emulator can publish the console's memory as a named block of shared memory other programs may open.
    Reading it costs nothing and does not stop the game, where a read through the GDB server is a round trip with
    the game stopped. It is only ever read here: a write would bypass the emulator's own bookkeeping of changed code.

    The block belongs to the machine the emulator runs on. A tool running elsewhere - in a Linux guest beside
    a Windows emulator, say - cannot open it, and goes on through the server."""

    def __init__(self, name: str, ram: int = 0x80000000, offset: int = 0) -> None:
        from multiprocessing import shared_memory
        self.name, self.ram, self.offset = name, ram & 0x1FFFFFFF, offset
        try:
            self.block = shared_memory.SharedMemory(name=name, track=False)     # the block is the emulator's: never ours to remove
        except TypeError:                                                      # before Python 3.13 there is no `track`
            self.block = shared_memory.SharedMemory(name=name)
            if os.name == "posix":                                             # ... and attaching would have the block removed when this tool exits
                from multiprocessing import resource_tracker
                resource_tracker.unregister(self.block._name, "shared_memory")
        self.size = self.block.size

    def read(self, address: int, length: int) -> bytes | None:
        """`length` bytes of the console's memory at `address`, or None when the block does not hold them."""
        at = (address & 0x1FFFFFFF) - self.ram + self.offset
        return bytes(self.block.buf[at:at + length]) if 0 <= at and at + length <= self.size else None

    def close(self) -> None:
        self.block.close()

    @staticmethod
    def find(wanted: str, process: str = "") -> list[str]:
        """The blocks to try: the name the config gives, with `{pid}` standing for the id of each running process
        whose name begins with `memory.process`."""
        if "{pid}" not in wanted:
            return [wanted]
        if not process:
            raise SystemExit("memory.shared has {pid} in it: memory.process has to say which program's")
        if sys.platform == "win32":
            listing = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
            pids = [row.split(",")[1].strip('"') for row in listing.splitlines() if row.lower().startswith('"' + process.lower())]
        else:
            pids = []
            for entry in Path("/proc").iterdir() if Path("/proc").is_dir() else ():
                try:
                    if entry.name.isdigit() and (entry / "comm").read_text().strip().lower().startswith(process.lower()):
                        pids.append(entry.name)
                except OSError:
                    pass
        return [wanted.replace("{pid}", pid) for pid in pids]


# ---- repeats: an event sent over and over is written once, with a line saying how often it came again

@filter("repeats")
class Repeats:
    """Writes a run of lines that repeats once, with a count. It keeps a short history of the lines written.
    When new lines keep matching the last one, or the last few in the same order, they are held back and counted
    instead of written. The first line that does not fit ends the run, and three things are written in order:
    `{"event": "repeated", "last": N, "times": T}` - the N lines before this one came T more times - then the
    lines held of a repetition that was only partly through, then the new line. Nothing is lost, and expanding
    each marker in place gives back every line in its order.

    Every event takes part unless it says `collapse = false`, or has a handler: a line of such an event always
    ends the run before it is written, so it keeps its exact place among the others."""

    def __init__(self, longest: int = 8) -> None:
        self.longest = longest
        self.history: list[str] = []       # the last lines written, as text
        self.pattern: list[str] = []       # the lines being repeated, once a repeat is under way
        self.held: list[dict] = []         # the lines of the repetition now in progress
        self.times = 0                     # whole repetitions not written

    def line(self, line: dict, event: dict, trace) -> list[dict]:
        if event.get("collapse", True) and not event.get("handler"):
            return self.offer(line)
        return self.end() + [line]         # a marker counts the lines just before it: nothing may match across this one

    def offer(self, line: dict) -> list[dict]:
        """A new line of a collapsed event; returns the lines to write now."""
        text = json.dumps(line, sort_keys=True)
        if self.pattern:
            if text == self.pattern[len(self.held)]:
                self.held.append(line)
                if len(self.held) == len(self.pattern):
                    self.times, self.held = self.times + 1, []
                return []
            out = self.flush()
        else:
            out = []
        for length in range(1, min(self.longest, len(self.history)) + 1):      # does it begin the last few again?
            if self.history[-length] == text:
                self.pattern, self.held, self.times = self.history[-length:], [line], 0
                if length == 1:
                    self.times, self.held = 1, []
                return out
        self.history = (self.history + [text])[-self.longest:]
        return out + [line]

    def end(self, trace=None) -> list[dict]:
        """For the tracer, before it writes a line of its own (a session's marker): give up what is held, and
        match nothing across that line."""
        out = self.flush()
        self.history = []
        return out

    def flush(self) -> list[dict]:
        """Stop holding back: the count of what was repeated, then what was held of an unfinished repetition."""
        out = [{"event": "repeated", "last": len(self.pattern), "times": self.times}] if self.times else []
        out += self.held
        # The history is the end of the lines as they would read with every marker expanded: the repeats just
        # counted belong to it, or a later marker's "the N lines before this one" would name the wrong lines.
        self.history = (self.history + self.pattern * self.times + [json.dumps(line, sort_keys=True) for line in self.held])[-self.longest:]
        self.pattern, self.held, self.times = [], [], 0
        return out


# ---- a session

class Session:
    def __init__(self, cfg: dict, gdb: Gdb, out) -> None:
        self.cfg, self.gdb, self.out = cfg, gdb, out
        self.filters = [FILTERS[name]() for name in cfg.get("log", {}).get("filters", [])]
        self.events = cfg.get("event", [])
        self.count = 0
        self.ready = self.running = self.paused = False
        self.session_number = self.session_at = self.session_count = 0
        self.session_name = self.awaiting = ""          # awaiting: the name of a session just dropped, until the next begins
        self.drains: set[int] = set()
        self.shared: Shared | None = None
        self.waiting = None                           # (where, since when, the event): the reader is held at a handler's record until the game stops
        self.bad_cursor = None
        self.typed = None
        self.count_records = 0
        self.read_by: dict[str, int] = {}             # records read while the game was "running" and while it was "stopped"
        self.late_stops = 0
        self.before_stop = 0
        self.read_at = 0                              # how far into the buffer the log has got
        self.sites = sites_of(cfg)
        self.code, self.symbols, _ = assemble(cfg, self.sites)
        self.code_at = (self.symbols["gt_end"] + 15) & ~15        # free memory after the buffer, for `inject`
        self.injected: list[tuple[int, int]] = []                   # what is injected there now: (start, end)
        self.held: dict[int, bytes] = {}                            # hooks a command has placed: address -> the bytes it replaced
        self.layout = {}                              # a record's first word -> (event, phase, captures)
        self.hooks = []                               # (address, the two words there in the image, the stub)
        self.by_image: dict[str, list] = {}           # the same, by the image the address is in
        self.halted = True                            # whether the game is stopped right now
        self.residents: dict = {}                     # load address -> the image a handler last said is there
        self.breaks: dict[int, dict] = {}             # address -> the event with a handler whose breakpoint is set there
        self.breaks_by_image: dict[str, list] = {}
        for event in self.events:
            if event["_break"]:
                self.breaks_by_image.setdefault(event["image"], []).append(event)
                continue
            self.layout[header(event, 0, event["_enter"])] = (event, "enter", event["_enter"])
            self.hooks.append((event["address"], self.sites[(event["id"], "enter")], self.symbols[f"gt_enter_{event['id']}"]))
            image = event["image"]
            self.by_image.setdefault(image, []).append(self.hooks[-1])
            for k, offset in enumerate(event["exits"]):
                self.layout[header(event, 1, event["_exit"])] = (event, "exit", event["_exit"])
                self.hooks.append((event["address"] + offset, self.sites[(event["id"], "exit", offset)], self.symbols[f"gt_exit_{event['id']}_{k}"]))
                self.by_image[image].append(self.hooks[-1])

    def note(self, text: str, **fields) -> None:
        """A line of free text in the log, `{"event": "note", "text": ...}`: an annotation, from the `note` command,
        a hook (`trace.note("...")`), the tracer itself, or a tool that works on a log afterwards. It belongs
        where it is written, so anything the filters hold back is written first. Extra fields ride along."""
        self.settle()
        self.emit({"event": "note", "text": text, **fields})

    # ---- talking to whoever is at the prompt, for commands

    def say(self, text: str = "") -> None:
        """Print to the prompt."""
        print(text, file=sys.stderr, flush=True)

    def ask(self, prompt: str = "> ") -> str | None:
        """Print a prompt and wait for a line; the game stays stopped meanwhile. The line without its end, or
        None when the input has closed."""
        if self.typed is None:
            return None
        lines, wake, woken = self.typed
        print(prompt, end="", file=sys.stderr, flush=True)
        woken.recv(1)
        line = lines.get()
        if line is None:                              # the input closed: leave that for the main loop to find
            lines.put(None)
            wake.send(b"x")
            return None
        return line.strip()

    def choose(self, title: str, options: list, show: int = 30):
        """Let whoever is at the prompt pick one of `options`: by its number, or by typing part of its name to
        narrow the list. Each option is a name, or a (name, description) pair; the chosen name is returned, or
        None when an empty line (or a closed input) gives up."""
        named = [(o, "") if isinstance(o, str) else (o[0], o[1]) for o in options]
        listed = named
        while True:
            self.say(title)
            for n, (name, about) in enumerate(listed[:show], 1):
                self.say(f"  {n:>3}  {name}" + (f"   {about}" if about else ""))
            if len(listed) > show:
                self.say(f"       ... and {len(listed) - show} more: type part of a name to narrow them")
            answer = self.ask("a number, part of a name, or nothing to give up: ")
            if not answer:
                return None
            if answer.isdigit() and 1 <= int(answer) <= min(len(listed), show):
                return listed[int(answer) - 1][0]
            exact = [o for o in named if str(o[0]).lower() == answer.lower()]
            found = exact or [o for o in named if answer.lower() in str(o[0]).lower() or answer.lower() in str(o[1]).lower()]
            if len(found) == 1:
                return found[0][0]
            if not found:
                self.say(f"nothing has `{answer}` in it")
                listed = named
            else:
                listed = found

    # ---- acting on the game, for commands

    def poke(self, address: int, data: bytes) -> None:
        """Write the console's memory. The game must be stopped, as it is while a command or a handler runs."""
        self.gdb.write(address, bytes(data))

    def address(self, what) -> int:
        """A number as it is, or the address of a symbol of the game."""
        if isinstance(what, int):
            return what
        try:
            return int(str(what), 0)
        except ValueError:
            found = symbol_address(str(what))
            if found is None:
                raise ValueError(f"`{what}` is not a number or a symbol of the game")
            return found

    def registers(self) -> dict[str, int]:
        """The registers where the game is stopped: the 32 by name, and `status`, `lo`, `hi`, `bad`, `cause`, `pc`."""
        regs = self.gdb.registers()
        return {**dict(zip(REGS, regs)), "status": regs[32], "lo": regs[33], "hi": regs[34], "bad": regs[35], "cause": regs[36], "pc": regs[PC]}

    def where(self) -> dict:
        """Plain facts about the place the game is stopped at, for a command to judge by:

            pc           the instruction it is about to run
            symbol       that address as `function+0xoffset`
            image        the image the address is in (one in memory from the start, or one said to be resident), or None
            delay_slot   the instruction before it is a jump or a branch, so this one belongs to it
            interrupts   whether the status register has interrupts enabled; None when the server does not give it"""
        regs = self.gdb.registers()
        pc = regs[PC]
        homes = started() + [IMAGES[name] for name in self.residents.values() if name in IMAGES]
        before = struct.unpack("<I", self.gdb.read(pc - 4, 4))[0]
        return {"pc": pc, "symbol": self.symbol(pc), "image": next((item.name for item in homes if item.load <= pc < item.end), None),
                "delay_slot": movable(before) is not None, "interrupts": bool(regs[STATUS] & 1) if regs[STATUS] else None}

    def step(self, count: int = 1) -> int:
        """Run `count` instructions and stop. Returns where the game then is."""
        for _ in range(count):
            self.gdb._go("s")
        return self.gdb.registers()[PC]

    def run_to(self, *addresses) -> int:
        """Let the game run until it stops at one of `addresses` (numbers or symbols), however long that takes.
        Returns the one it stopped at. On the way the tracer goes on tracing: any other stop - a package arriving,
        a full buffer - is dealt with as usual and the game continues. Nothing is done at the address itself."""
        targets = {self.address(a) for a in addresses}
        added = [a for a in targets if a not in self.gdb.set]
        for a in added:
            self.gdb.breakpoint(a)
        try:
            pc = self.gdb.registers()[PC]
            while True:
                self.halted = False
                if self.gdb.go(pc):
                    self.gdb.wait([])
                self.halted = True
                regs = self.gdb.registers()
                pc = regs[PC]
                if pc in targets:
                    return pc
                self.dispatch(pc, regs)
        finally:
            for a in added:
                self.gdb.breakpoint(a, False)

    def assemble(self, source: str, at: int) -> tuple[bytes, dict[str, int]]:
        """MIPS source as the bytes it is at address `at`, and the addresses of its labels. Written as it should
        run: delay slots where they go, nothing reordered. A name that is no label of the source is a symbol of
        the game or of the tracer's own blob."""
        asm = Assembler(at)
        asm.names = lambda name: self.symbols.get(name, symbol_address(name))
        code, _rows = asm.assemble(source)
        return code, dict(asm.symbols)

    def inject(self, source: str) -> dict[str, int]:
        """Assemble MIPS source into free memory (after the buffer) and write it there. Returns its labels'
        addresses, with `"."` for where it starts. It stays until `free` gives the memory back."""
        at = self.code_at
        code, labels = self.assemble(source, at)
        self.gdb.write(at, code)
        self.code_at = (at + len(code) + 15) & ~15
        self.injected.append((at, self.code_at))
        return {**labels, ".": at}

    def free(self, labels: dict) -> None:
        """Give back the memory of something injected. Memory is handed out like a stack: what was injected last
        is freed first, and the space of anything else comes back once everything after it is freed."""
        self.injected = [(a, b) for a, b in self.injected if a != labels["."]]
        self.code_at = max([b for _a, b in self.injected], default=(self.symbols["gt_end"] + 15) & ~15)

    def hook(self, address, target) -> bytes:
        """Swap the two instructions at `address` for a jump to `target` (and the no-op after it). Returns the
        eight bytes that were there, for `unhook`. Nothing is checked: where it is put is the caller's business."""
        address, target = self.address(address), self.address(target)
        original = self.gdb.read(address, 8)
        self.gdb.write(address, struct.pack("<2I", 0x08000000 | ((target >> 2) & 0x03FFFFFF), 0))
        self.held[address] = original
        return original

    def unhook(self, address, original: bytes | None = None) -> None:
        """Put back what `hook` replaced at `address`."""
        address = self.address(address)
        self.gdb.write(address, original if original is not None else self.held[address])
        self.held.pop(address, None)

    def call(self, function, *args) -> int:
        """Have the game call one of its functions, with up to four arguments, from where it is stopped; returns
        what the function returned, with the game stopped where it was and its registers as they were.

        It is the primitives put together, and it judges nothing about the place: a few instructions are injected
        - keep the registers, make the call, keep the result, put the registers back, jump to where the game was;
        the game's next two instructions are hooked to them; the game runs to their start, where the hook comes
        out again, so the game's code is whole before the call is made; then to their end, however long the
        function takes; then back to where it stood. A command that wants the call made somewhere in particular
        takes the game there first (`run_to`), and one whose function does not come back uses the primitives
        itself and says when its hook can go. The log gets a note of the call."""
        if len(args) > 4:
            raise ValueError("a call takes at most four arguments")
        target, values = self.address(function), [self.address(a) & 0xFFFFFFFF for a in args]
        name = function if isinstance(function, str) and not function[:1].isdigit() else f"{target:#010x}"
        here = self.gdb.registers()[PC]
        code = self.inject(call_source(target, values, here, self.cfg["blob"]["poke"]))
        original = self.hook(here, code["run"])
        self.run_to(code["run"])
        self.unhook(here, original)
        self.run_to(code["done"])
        value = struct.unpack("<I", self.gdb.read(code["result"], 4))[0]
        self.run_to(here)                             # out of the injected code: the game stands where it stood
        self.free(code)
        self.note(f"called {name}({', '.join(f'{v:#x}' for v in values)}) from {self.symbol(here)}: returned {value:#010x}", call=name, args=values, result=value)
        self.out.flush()
        return value

    def emit(self, record: dict) -> None:
        self.out.write(json.dumps(record) + "\n")
        self.count += 1

    # ---- sessions: a marker line in the log that says what the records after it are

    def session(self, name: str | None = None) -> None:
        """Begin a session: everything logged from here on belongs to it."""
        self.settle()
        self.session_number += 1
        self.session_name = name or f"session {self.session_number}"
        self.out.flush()
        self.session_at = self.out.tell()
        self.session_count = self.count
        import datetime
        self.session_started = datetime.datetime.now().isoformat(timespec="seconds")
        self.out.write(json.dumps(self.marker()) + "\n")
        self.out.flush()

    def marker(self) -> dict:
        return {"session": self.session_name, "number": self.session_number, "started": self.session_started}

    def rename(self, name: str) -> None:
        """Give the current session another name: its marker line is rewritten in place."""
        self.out.flush()
        self.out.seek(self.session_at)
        _old, _, rest = self.out.read().partition("\n")
        self.session_name = name
        self.out.truncate(self.session_at)
        self.out.seek(self.session_at)
        self.out.write(json.dumps(self.marker()) + "\n" + rest)
        self.out.flush()

    def drop(self) -> int:
        """Mark the current session as one to disregard. Its records stay in the log - what was loaded during it is
        still loaded, and whoever replays the log needs to see that - with a marker after them saying so."""
        if self.ready:
            self.drain(reset=True)
        self.settle()
        import datetime
        self.out.write(json.dumps({"dropped": self.session_name, "number": self.session_number, "at": datetime.datetime.now().isoformat(timespec="seconds")}) + "\n")
        self.out.flush()
        return self.count - self.session_count

    COMMANDS = """commands (the game is stopped while one is carried out; the letter alone does the same):
  p  pause            stop the game and keep it stopped
  r  resume           let it run again
  s  session NAME     read the buffer into the log, then begin a new session called NAME
  n  name NAME        rename the current session
     note TEXT        write a line of your own into the log, here: an annotation for whoever reads it
     drop             mark the current session as dropped (its records stay, a marker follows them) and stop the
                      game until `session NAME` begins the next one; plain `session` begins one of the same name
     resident IMAGE   the image is in memory now: write the hooks of its events (a handler normally says so)
  d  drain            read the buffer into the log now
     status           where things stand
     call F [A ...]   have the game call its function F (a name or an address) with up to four arguments, from
                      where it is stopped, and show what it returns. Raw: nothing is checked about the place
     peek WHERE [N]   show N bytes of memory (16 by default) at an address or a symbol
     poke WHERE W ... write words to memory at an address or a symbol (noted in the log)
  q  quit             read the buffer, take the hooks out and leave; the game runs on, or stays paused if it was
  h  help             this list"""

    def command(self, line: str) -> None:
        word, _, rest = line.strip().partition(" ")
        rest = rest.strip()
        say = lambda text: print(text, file=sys.stderr)  # noqa: E731
        if word in ("pause", "p"):
            self.paused = True
            say("paused")
        elif word in ("resume", "r", "continue", "c"):
            if self.awaiting:
                say(f"the last session was dropped: begin the next one first, with `session NAME`, or `session` for \"{self.awaiting}\" again")
            else:
                self.paused = False
                say("running")
        elif word in ("session", "s"):
            if self.ready:
                self.drain(reset=True)
            self.session(rest or self.awaiting or None)
            if self.awaiting:                         # the game was only stopped to wait for this
                self.awaiting, self.paused = "", False
            say(f"session {self.session_number}: {self.session_name}" + ("" if self.paused else "; running"))
        elif word in ("name", "n") and rest:
            self.rename(rest)
            say(f"session {self.session_number} is now: {self.session_name}")
        elif word == "note" and rest:
            if self.ready:
                self.drain(reset=self.shared is None)   # what happened up to now first: the note goes after it
            self.note(rest)
            say("noted")
        elif word == "drop":
            count = self.drop()
            self.awaiting, self.paused = self.session_name, True
            say(f"session {self.session_number} ({self.session_name}) marked dropped, {count} records kept; the game is stopped until `session NAME`, or `session` for the same name")
        elif word == "resident" and rest:
            if rest not in self.by_image:
                say(f"no event is in an image called {rest}; the images are: {', '.join(self.by_image)}")
            elif self.ready:
                self.resident(rest)
        elif word in ("drain", "d"):
            before = self.before_stop                 # stopping for the command already read the buffer: count from before that
            if self.ready:
                self.drain(reset=True)
            say(f"{self.count - before} records read")
        elif word == "status":
            placed = sum(struct.unpack("<2I", self.gdb.read(address, 8)) == (0x08000000 | ((stub >> 2) & 0x03FFFFFF), 0) for address, _o, stub in self.hooks) if self.ready else 0
            say(f"session {self.session_number}: {self.session_name} - {self.count - self.session_count} records in it, {self.count} in all; "
                f"{placed} of {len(self.hooks)} hooks in place; {'stopped until the next session begins' if self.awaiting else 'paused' if self.paused else 'running'}" + (self.reader_state() if self.ready else "; waiting for an install_at address"))
        elif word in ("quit", "q", "detach", "exit"):
            raise KeyboardInterrupt
        elif word in ("call", "peek", "poke") and not self.ready:
            say("not set up yet: the game has not reached an install_at address")
        elif word == "call" and rest:
            try:
                name, *args = rest.split()
                value = self.call(name, *args)
                say(f"returned {value:#010x} ({value - (1 << 32) if value & 0x80000000 else value})")
            except (ValueError, OSError) as error:
                say(str(error))
        elif word == "peek" and rest:
            try:
                where, *more = rest.split()
                data = self.read(self.address(where), int(more[0], 0) if more else 16)
                say(" ".join(f"{w:08x}" for w in struct.unpack(f"<{len(data) // 4}I", data[:len(data) & ~3])) + (f"  {data[len(data) & ~3:].hex()}" if len(data) & 3 else ""))
            except (ValueError, OSError, IndexError) as error:
                say(str(error))
        elif word == "poke" and len(rest.split()) >= 2:
            try:
                where, *values = rest.split()
                self.poke(self.address(where), struct.pack(f"<{len(values)}I", *[int(v, 0) & 0xFFFFFFFF for v in values]))
                self.note(f"poked {where}: {' '.join(values)}")
                say("written")
            except (ValueError, OSError) as error:
                say(str(error))
        elif word in COMMANDS:
            if not self.ready:
                say("not set up yet: the game has not reached an install_at address")
            else:
                self.drain(reset=self.shared is None)   # the log first, so what the command notes comes after
                try:
                    said = COMMANDS[word](self, rest)
                except Exception as error:  # noqa: BLE001 - a command's mistake must not end the trace
                    said = f"{type(error).__name__}: {error}"
                if said:
                    say(str(said))
        elif word in ("help", "h", "?"):
            extra = [f"     {(function.__doc__ or name).strip().splitlines()[0]}" for name, function in sorted(COMMANDS.items())]
            say(self.COMMANDS + ("\nfrom the hooks files:\n" + "\n".join(extra) if extra else ""))
        elif word:
            say(f"no command `{word}`\n{self.COMMANDS}")

    def symbol(self, address: int) -> str:
        """The function an address is in, as `name+0xoffset`: in an image that is there from the start, or one a
        handler has said is in memory."""
        return name_at(address, started() + [IMAGES[name] for name in self.residents.values() if name in IMAGES])

    def extent(self, image: str) -> tuple:
        """(image, where it loads, where it ends)."""
        return (image, IMAGES[image].load, IMAGES[image].end) if image in IMAGES else (image, 0, 0)

    def write(self, text: str) -> None:
        """A line of its own in the log, for a renderer."""
        self.settle()
        self.out.write(text.rstrip("\n") + "\n")
        self.count += 1

    def act(self, function, record: dict) -> None:
        try:
            function(record, self)
        except Exception as error:  # noqa: BLE001 - a handler's mistake must not end the trace
            print(f"handler for {record.get('name')}: {type(error).__name__}: {error}", file=sys.stderr)

    def log(self, event: dict, record: dict) -> None:
        """One captured event into the log: through its renderer, or as it is - registers in hex, memory as hex bytes."""
        act = HANDLERS.get(event.get("handler") or "")
        if act is not None:
            self.act(act, dict(record))
        render = RENDERERS.get(event.get("render") or "")
        if render is not None:
            try:
                shown = render(record, self)
            except Exception as error:  # noqa: BLE001 - a renderer's mistake must not cost the capture
                shown = {"render_error": f"{type(error).__name__}: {error}", **record}
            if shown is None:
                return
            if isinstance(shown, str):
                self.write(shown)
                return
            record = shown
        plain = {}
        for key, value in record.items():
            if isinstance(value, (bytes, bytearray)):
                plain[key] = bytes(value).hex()
            elif isinstance(value, int) and not isinstance(value, bool) and key not in ("id",):
                plain[key] = f"{value:#010x}" if key in REGS or key == "ret" else value
                if key == "ra" and "from" not in record:
                    plain["from"] = self.symbol(value)
            else:
                plain[key] = value
        lines = [plain]
        for step in self.filters:
            lines = [out for line in lines for out in (step.line(line, event, self) or [])]
        for line in lines:
            self.emit(line)

    def settle(self) -> None:
        """Have the filters give up whatever they are holding back: before a session's marker, a line a renderer
        writes itself, and the end of the log."""
        for n, step in enumerate(self.filters):
            lines = step.end(self) or []
            for later in self.filters[n + 1:]:
                lines = [out for line in lines for out in (later.line(line, {}, self) or [])]
            for line in lines:
                self.emit(line)

    def start(self) -> None:
        """Wait for the game to reach the first `install_at` address (it is continued to there), then set up."""
        self.gdb.halt_reason()
        self.installers = set(self.cfg["install_at"])
        if not self.installers:
            self.setup()
        else:
            for address in self.installers:
                self.gdb.breakpoint(address)
            print(f"waiting for the game to reach {', '.join(name_at(a, started()) for a in sorted(self.installers))}", file=sys.stderr)

    def peek(self, address: int, length: int) -> bytes:
        """Memory for reading only: from the shared block where there is one, else through the server (stopped)."""
        if self.shared is not None:
            data = self.shared.read(address, length)
            if data is not None:
                return data
        return self.gdb.read(address, length)

    def open_shared(self) -> None:
        """Find the emulator's shared memory and make sure it is this game's: the blob just written through the
        server has to be readable in it, at the same place."""
        wanted = self.cfg["memory"]["shared"]
        if wanted in (False, "", "no", "off"):
            return
        base, reasons = self.cfg["blob"]["address"], []
        for name in Shared.find(wanted, self.cfg["memory"].get("process", "")):
            try:
                shared = Shared(name)
            except OSError as error:
                reasons.append(str(error))
                continue
            if shared.read(base, len(self.code)) == self.code:
                self.shared = shared
                print(f"reading memory from the shared block {name}: the buffer is read as the game runs, without stopping it", file=sys.stderr)
                return
            reasons.append(f"{name} does not hold this game's memory where expected")
            shared.close()
        print("no usable shared memory (" + "; ".join(reasons or ["none found"]) + "): reading through the GDB server", file=sys.stderr)

    def setup(self) -> None:
        base = self.cfg["blob"]["address"]
        self.refuse_mirror(base)
        try:
            self.gdb.write(base, self.code)
        except OSError as error:
            raise SystemExit(f"{error}\nThe blob lives at {base:#010x} (blob.address): the target has to have writable memory there.")
        self.gdb.breakpoint(self.symbols["gt_full"])
        self.read_at = self.symbols["gt_buffer"]
        self.open_shared()
        # An event marked `drain` has the buffer read each time it is over: at its returns where the config gives
        # them, and otherwise straight after its entry record. With shared memory the buffer is read as the game
        # runs, and none of these stops is needed.
        self.drains = set()
        for event in self.events:
            if event["drain"] and self.shared is None and not event["_break"]:
                ends = [f"gt_exit_{event['id']}_{k}_done" for k in range(len(event["exits"]))] or [f"gt_enter_{event['id']}_done"]
                self.drains |= {self.symbols[name] for name in ends}
        for address in self.drains:
            self.gdb.breakpoint(address)
        self.ready = True
        there = [item.name for item in started()] + [name for name in self.cfg.get("resident", []) if name in IMAGES and not IMAGES[name].start]
        placed = sum(self.install(name) for name in there)
        for name in self.cfg.get("resident", []):
            self.residents[IMAGES[name].load if name in IMAGES else name] = name
        others = sorted(name for name in {*self.by_image, *self.breaks_by_image} if name not in there)
        print(f"{placed} of {len(self.hooks)} hooks in place, {len(self.breaks)} breakpoints for events with a handler" + (f"; those in {', '.join(others)} are written when a handler says the image is resident" if others else ""), file=sys.stderr)
        if others and not any(e.get("handler") for e in self.events):
            print("no event has a handler, so nothing will say so: name the images already in memory with --resident, or use the `resident` command", file=sys.stderr)

    def refuse_mirror(self, base: int) -> None:
        """Stop before writing the blob into memory that is really somewhere else.

        A console with 2 MB of memory shows it again at every 2 MB above: 0x80200000 is then 0x80000000, the
        kernel's own tables and its exception vector, and a blob written there takes the machine down. Memory that
        is its own differs from what lies 2 MB below it; where the two read the same, or nothing has been loaded
        yet and both are empty, nothing is written."""
        span, step = 0x200, 0x200000
        here = self.gdb.read(base, span)
        lower = base - step
        while True:
            try:
                below = self.gdb.read(lower, span)
            except OSError:
                break
            if below == here:
                what = "is empty, like the memory below it: start once the game has booted" if not any(here) else f"is the same memory as {lower:#010x}"
                raise SystemExit(f"blob.address {base:#010x} {what}.\nNothing was written. The blob needs memory of its own - "
                                 "a target with memory above 2 MB (an 8 MB mode), or an address inside the 2 MB that the game leaves free.")
            if (lower & 0x1FFFFFFF) < step:
                break
            lower -= step

    def install(self, image: str | None = None) -> int:
        """Write the hooks of one image - all images when None - wherever the image's own instructions are in
        memory. Returns how many of those hooks are in place. The game must be stopped."""
        placed = 0
        for name, hooks in self.by_image.items():
            if image is not None and name != image:
                continue
            for address, original, stub in hooks:
                jump = (0x08000000 | ((stub >> 2) & 0x03FFFFFF), 0)
                now = struct.unpack("<2I", self.gdb.read(address, 8))
                if now == tuple(original):
                    self.gdb.write(address, struct.pack("<2I", *jump))
                    now = jump
                placed += now == jump
        for name, events in self.breaks_by_image.items():
            if image is not None and name != image:
                continue
            for event in events:                      # a breakpoint only where the image's own instruction is in memory
                if struct.unpack("<I", self.gdb.read(event["address"], 4)) == image_words(event["address"], 1, name):
                    self.gdb.breakpoint(event["address"])
                    self.breaks[event["address"]] = event
        return placed

    def hit(self, event: dict, regs: list[int]) -> None:
        """The game is stopped on the breakpoint of an event with a handler: make its record from the registers and
        memory as they stand, then log it like any other (its handler runs first)."""
        if struct.unpack("<I", self.gdb.read(event["address"], 4)) != image_words(event["address"], 1, event["image"]):
            return                                    # another image is at this address now: not this event
        record = {"id": event["id"], "name": event["name"], "phase": "enter"}
        for c in event["_enter"]:
            if c["kind"] == "reg":
                record[c["text"]] = regs[REGS.index(c["reg"])]
                continue
            address = c["address"] if c["kind"] == "fixed" else (regs[REGS.index(c["reg"])] + c["offset"]) & 0xFFFFFFFF
            readable = 0x80000000 <= address and address + c["length"] <= 0x80800000
            record[c["text"]] = self.peek(address, c["length"]) if readable else bytes(c["length"])
        self.log(event, record)
        self.out.flush()
        try:
            os.fsync(self.out.fileno())
        except (AttributeError, OSError, ValueError):
            pass

    def resident(self, image: str) -> None:
        """An image is in memory, whole, from here on: its symbols name addresses in the log, and the hooks of the
        events in it are written. It takes the place of whatever was at its load address. For a handler, with the
        game stopped."""
        self.residents[IMAGES[image].load if image in IMAGES else image] = image
        if image in self.by_image:
            placed = self.install(image)
            print(f"{image}: {placed} of {len(self.by_image[image])} hooks in place", file=sys.stderr)

    def read(self, address: int, length: int) -> bytes:
        """The console's memory, for a handler."""
        return self.peek(address, length)

    def remove(self) -> None:
        """Put back what the hooks replaced, where the hooks are still in place."""
        for address, original, stub in self.hooks:
            jump = (0x08000000 | ((stub >> 2) & 0x03FFFFFF), 0)
            if struct.unpack("<2I", self.gdb.read(address, 8)) == jump:
                self.gdb.write(address, struct.pack("<2I", *original))

    def drain(self, reset: bool) -> None:
        """Write out what the buffer holds beyond what the log already has. `reset` starts the buffer again from
        here (the game must be stopped); where the game is stopped on gt_full its own next instructions do that."""
        start = self.symbols["gt_buffer"]
        # The game may be writing the cursor as it is read here; a value seen twice running was not caught halfway.
        cursor = struct.unpack("<I", self.peek(self.symbols["gt_cursor"], 4))[0]
        for _ in range(8):
            again = struct.unpack("<I", self.peek(self.symbols["gt_cursor"], 4))[0]
            if again == cursor:
                break
            cursor = again
        if not start <= cursor <= self.symbols["gt_end"] or (cursor - start) % 4:
            if cursor != self.bad_cursor:             # said once per value: nothing can be read while it lasts
                self.bad_cursor = cursor
                print(f"the buffer's cursor reads {cursor:#010x}, which is not a place in the buffer ({start:#010x}..{self.symbols['gt_end']:#010x}); nothing is read until it is", file=sys.stderr)
            return
        if cursor < self.read_at:                     # the game started the buffer again since the last look
            self.read_at = start
        data, at = self.peek(self.read_at, cursor - self.read_at), 0
        records_before, began_at = self.count_records, self.read_at
        while at + 4 <= len(data):
            word = struct.unpack_from("<I", data, at)[0]
            known = self.layout.get(word)
            if known is None:
                self.note(f"the buffer does not parse at {self.read_at + at:#010x} (first word {word:#010x}); what was read after it is dropped")
                break
            event, phase, captures = known
            if event.get("handler") and not self.halted:
                # Read while the game runs. A handler acts with the game stopped, and what comes after its event
                # may depend on what it does; the game stops at the end of this event's hook in a moment, and
                # the rest is read then.
                cursor = self.read_at + at
                if self.waiting is None or self.waiting[0] != cursor:
                    self.waiting = (cursor, time.monotonic(), event["name"])
                break
            record, field = {"id": event["id"], "name": event["name"], "phase": phase}, at + 4
            for c in captures:
                record[c["text"]] = struct.unpack_from("<I", data, field)[0] if c["kind"] == "reg" else data[field:field + c["length"]]
                field += c["size"]
            self.log(event, record)
            at = field
            self.waiting = None
            self.count_records += 1
        self.out.flush()
        if self.count_records != records_before:
            # Flushed is not yet visible: Windows shows a file's new length to other programs only once the
            # writer has pushed it to the disc (or closed the file), so a viewer following the log sees
            # nothing new until then.
            try:
                os.fsync(self.out.fileno())
            except (AttributeError, OSError, ValueError):
                pass
        # For --protocol: every look at the buffer that found something, with the time, whether the game was
        # running (a look as it runs) or stopped, and how far it got. A log that only moves at stops shows here.
        how = "stopped" if self.halted else "running"
        if cursor != began_at or self.count_records != records_before:
            self.read_by[how] = self.read_by.get(how, 0) + self.count_records - records_before
            self.gdb.note("#", f"buffer read while {how}: {self.count_records - records_before} records, {began_at:#010x} -> {self.read_at + at:#010x}, written to {cursor:#010x}" + (f", held at {self.waiting[2]}" if self.waiting else ""))
        self.read_at = cursor
        # Starting the buffer again is only done where no stub is part-way through a record: a stub holds its own
        # copy of the cursor, and would put the old value back when it finished. Elsewhere the buffer simply goes
        # on filling; the log has what it holds so far either way.
        if reset and self.between_records():
            if cursor != start:
                self.gdb.write(self.symbols["gt_cursor"], struct.pack("<I", start))
            self.read_at = start

    def reader_state(self) -> str:
        """Where the log stands against the buffer, for `status`: how much is unread, what is held back and why."""
        cursor = struct.unpack("<I", self.peek(self.symbols["gt_cursor"], 4))[0]
        held = sum(len(getattr(step, "held", ())) + getattr(step, "times", 0) * len(getattr(step, "pattern", ())) for step in self.filters)
        return (f"\n  buffer: read to {self.read_at:#010x}, written to {cursor:#010x} ({max(cursor - self.read_at, 0)} bytes unread) of {self.symbols['gt_buffer']:#010x}..{self.symbols['gt_end']:#010x}"
                + (f"; held at a {self.waiting[2]} record for its stop" if self.waiting else "")
                + (f"\n  {held} repeated lines counted and not written (the repeats filter writes them as one line when the run ends)" if held else "")
                + f"\n  records read while the game ran: {self.read_by.get('running', 0)}; while it was stopped: {self.read_by.get('stopped', 0)}"
                + (f"\n  {self.late_stops} stops that did not come by themselves" if self.late_stops else ""))

    def between_records(self) -> bool:
        """Whether the stopped game is outside the stubs, or at one of the places in them where the record is done."""
        pc = self.gdb.registers()[PC]
        inside = self.cfg["blob"]["address"] <= pc < self.symbols["gt_buffer"]
        return not inside or pc == self.symbols["gt_full"] or pc in self.drains

    def close(self) -> None:
        """Leave the game with no hook and no breakpoint, running - or paused, if it was paused when the tracer was
        told to leave. What the buffer still holds is read first."""
        if self.gdb.sock.fileno() < 0:
            return
        if self.running:
            self.gdb.sock.sendall(b"\x03")
            while not Gdb._is_stop(self.gdb._recv() or ""):
                pass
        if self.ready:
            # A hook cannot be taken out from under a stub that has still to jump back through it: let the game
            # run on to the end of the stub first (a stub is a few dozen instructions).
            for _ in range(400):
                if self.between_records():
                    break
                self.gdb._go("s")
            self.drain(reset=True)
            self.settle()
            for address in list(self.held):           # a hook a command still held: the game's instructions go back
                self.unhook(address)
            self.remove()
        # A game the user had paused stays paused: the hooks and breakpoints are gone, and it is theirs to resume.
        self.gdb.release(False, resume=not self.paused)
        print("the game is left paused" if self.paused else "the game is running", file=sys.stderr)

    def stopped(self) -> int:
        """The game has stopped: do what the place it stopped at asks for. Returns that place."""
        self.halted = True
        regs = self.gdb.registers()
        pc = regs[PC]
        self.dispatch(pc, regs)
        return pc

    def dispatch(self, pc: int, regs: list[int]) -> None:
        """What a stop at `pc` asks for."""
        if self.ready and pc in self.breaks:
            self.drain(reset=self.shared is None)     # what led up to it first, so the log keeps its order
            self.hit(self.breaks[pc], regs)
        elif pc in self.installers:
            if not self.ready:
                self.setup()
            else:
                for item in started():
                    self.install(item.name)
        elif self.ready and pc == self.symbols["gt_full"]:
            self.drain(reset=False)
            self.read_at = self.symbols["gt_buffer"]  # the game's next instructions start the buffer again
        elif self.ready and pc in self.drains:
            self.drain(reset=self.shared is None)
        elif self.ready and self.shared is not None:
            self.drain(reset=False)                   # stopped for a command: bring the log up to here

    def run(self, first_session: str | None = None) -> None:
        self.session(first_session)
        self.start()
        # Commands arrive from a thread: a keyboard cannot be waited on together with a socket on every system.
        import queue
        import threading
        lines: queue.Queue = queue.Queue()
        wake, woken = socket.socketpair()
        self.typed = (lines, wake, woken)             # for a command that asks its own questions (`ask`)
        def listen() -> None:
            for line in sys.stdin:
                lines.put(line)
                wake.send(b"x")
            lines.put(None)
            wake.send(b"x")
        threading.Thread(target=listen, daemon=True).start()
        keyboard, pc = [woken], None
        print("type `help` for the commands; Ctrl-C detaches", file=sys.stderr)
        while True:
            if not self.running and not self.paused:
                self.halted = False
                self.running = self.gdb.go(pc)
                if not self.running:
                    pc = self.stopped()
                    continue
            if self.running:
                what = self.gdb.wait(keyboard, 0.25 if self.shared is not None and self.ready else None)
                if what == "stop":
                    self.running = False
                    pc = self.stopped()
                    continue
                if what == "tick":                    # shared memory: bring the log up to date as the game runs
                    self.drain(reset=False)
                    # The reader holds at a handler's record until the game stops at the end of that event's
                    # hook, which is a matter of microseconds. If the stop has not come, it is not coming -
                    # and everything after the record would wait for the next stop of any kind. Stop the game
                    # here instead: the handler then runs a little late, which is better than not at all.
                    if self.waiting and time.monotonic() - self.waiting[1] > 1.0:
                        print(f"no stop came for {self.waiting[2]} (record at {self.waiting[0]:#010x}); stopping the game to carry on from it", file=sys.stderr)
                        self.late_stops += 1
                        self.gdb.interrupt()
                        self.running = False
                        pc = self.stopped()
                    continue
            elif keyboard:
                select.select(keyboard, [], [])
            else:
                self.paused = False                   # nobody can say `resume` any more
                continue
            woken.recv(1)
            line = lines.get()
            if line is None:
                keyboard = []                         # the input is closed; the trace goes on without commands
                continue
            if not line.strip():
                continue
            self.before_stop = self.count
            if self.running:                          # a command is carried out with the game stopped
                self.gdb.interrupt()
                self.running = False
                pc = self.stopped()                   # it may have been about to stop on its own
            self.command(line)
            self.out.flush()


class NoLog:
    """What the log is under --no-log: everything written to it is dropped."""

    def write(self, text: str) -> int:
        return len(text)

    def read(self) -> str:
        return ""

    def tell(self) -> int:
        return 0

    def flush(self) -> None: ...
    def seek(self, *_) -> None: ...
    def truncate(self, *_) -> None: ...

    def __enter__(self):
        return self

    def __exit__(self, *_) -> None: ...


def refilter(cfg: dict, log: Path, out: Path) -> tuple[int, int]:
    """Put a log already written through the config's filters, as if they had been there when it was recorded.
    A line belongs to the event whose name its `event` (or `name`) is; session and dropped markers end what the
    filters hold, as they do in a run."""
    events = {event["name"]: event for event in cfg.get("event", [])}
    filters = [FILTERS[name]() for name in cfg.get("log", {}).get("filters", [])]
    read = written = 0

    def through(lines: list, event: dict, start: int = 0) -> list:
        for step in filters[start:]:
            lines = [o for line in lines for o in (step.line(line, event, None) or [])]
        return lines

    def settle() -> list:
        return [o for n, step in enumerate(filters) for o in through(step.end(None) or [], {}, n + 1)]

    with log.open(encoding="utf-8", errors="replace") as source, out.open("w", encoding="utf-8") as target:
        for text in source:
            read += 1
            try:
                line = json.loads(text)
            except ValueError:
                line = None
            if not isinstance(line, dict) or "event" not in line and "name" not in line:      # a marker, or a line a renderer wrote itself
                lines = settle()
                target.write("".join(json.dumps(o) + "\n" for o in lines) + text)
                written += len(lines) + 1
                continue
            lines = through([line], events.get(line.get("event") or line.get("name"), {}))
            target.write("".join(json.dumps(o) + "\n" for o in lines))
            written += len(lines)
        lines = settle()
        target.write("".join(json.dumps(o) + "\n" for o in lines))
    return read, written + len(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["run", "check", "blob", "refilter"])
    ap.add_argument("log", nargs="?", type=Path, help="refilter: the log to put through the config's filters; the result goes to --out")
    ap.add_argument("--config", type=Path, default=Path(__file__).with_name("events.toml"))
    ap.add_argument("--out", type=Path, help="the log; by default a new file per run, trace-<date>-<time>.jsonl in the config's log directory")
    ap.add_argument("--host", help="the GDB server's address, instead of the config's gdb.host")
    ap.add_argument("--port", type=int, help="the GDB server's port, instead of the config's gdb.port")
    ap.add_argument("--session", help="the name of the first session (a marker line in the log); more are begun with the `session` command")
    ap.add_argument("--resident", action="append", metavar="IMAGE", help="an image already in memory at setup, whose hooks are written at setup (repeatable); with --now, usually gameplay")
    ap.add_argument("--no-log", action="store_true", help="write no trace log: for a run that is only there to act on the game")
    ap.add_argument("--protocol", type=Path, metavar="FILE", help="write every packet exchanged with the GDB server to FILE, with times: for finding out why a stop went wrong")
    ap.add_argument("--now", action="store_true", help="set up at once in the game as it stands, ignoring install_at: for attaching to a game already running")
    args = ap.parse_args()
    cfg = load(args.config)
    load_hooks(cfg, args.config)
    if args.host:
        cfg["gdb"]["host"] = args.host
    if args.port:
        cfg["gdb"]["port"] = args.port
    if args.now:
        cfg["install_at"] = []
    cfg["resident"] = args.resident or []
    if args.command == "refilter":
        if args.log is None or args.out is None:
            raise SystemExit("refilter LOG --out FILE")
        print("{} lines in, {} out".format(*refilter(cfg, args.log, args.out)))
        return 0
    if args.command in ("check", "blob"):
        for address in cfg["install_at"]:
            print(f"set up at {address:#010x}  {name_at(address, started())}")
        for event in cfg.get("event", []):
            print(f"{event['id']:>5}  {event['name']:<24} {event['address']:#010x}  {name_at(event['address'], [IMAGES[event['image']]])}")
            if event["_break"]:
                print(f"       a breakpoint: the game stops here for the handler {event['handler']}; read then: {', '.join(event['capture'])}" + (f"   rendered by {event['render']}" if event.get("render") else ""))
                continue
            found = returns_in(event)
            print(f"       enter: {', '.join(event['capture'])}  ({record_size(event['_enter'])} bytes)" + (f"   rendered by {event['render']}" if event.get("render") else "") + ("   drains the buffer" if event["drain"] else ""))
            if event["exits"]:
                print(f"       exit at {', '.join(f'+{o:#x}' for o in event['exits'])}: {', '.join(event['exit_capture'])}  ({record_size(event['_exit'])} bytes)")
            missing = [o for o in found if o not in event["exits"]]
            if missing:
                print(f"       returns not hooked: {', '.join(f'{o:#x}' for o in missing)}   (exits = [{', '.join(f'{o:#x}' for o in found)}] hooks them all)")
        code, symbols, listing = assemble(cfg, sites_of(cfg))
        print(f"\nblob: {len(code)} bytes of code at {cfg['blob']['address']:#010x}, buffer of {cfg['blob']['bytes']} bytes at {symbols['gt_buffer']:#010x}; "
              f"the game stops only at {symbols['gt_full']:#010x}, when the buffer is full")
        if args.command == "blob":
            OUT = cfg["_out"]
            OUT.mkdir(parents=True, exist_ok=True)
            (OUT / "blob.bin").write_bytes(code)
            (OUT / "blob.s").write_text(stub_source(cfg, sites_of(cfg)), encoding="utf-8")
            (OUT / "blob.lst").write_text(listing, encoding="utf-8")
            print(f"written: {OUT / 'blob.bin'}, {OUT / 'blob.s'}, {OUT / 'blob.lst'}")
            agree = binutils_agree(cfg, sites_of(cfg), code)
            if agree:
                print(agree)
        return 0
    if args.no_log:
        print("writing no log", file=sys.stderr)
    else:
        if args.out is None:
            import datetime
            args.out = cfg["_out"] / f"trace-{datetime.datetime.now():%Y%m%d-%H%M%S}.jsonl"
        args.out.parent.mkdir(parents=True, exist_ok=True)
        print(f"writing {args.out}", file=sys.stderr)
    try:
        gdb = Gdb(cfg["gdb"]["host"], cfg["gdb"]["port"])
        if args.protocol:
            gdb.protocol = args.protocol.open("w", encoding="utf-8")
    except OSError as error:
        raise SystemExit(f"no GDB server at {cfg['gdb']['host']}:{cfg['gdb']['port']} ({error}). Start the server, or give its address with --host and --port.")
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))   # detach as Ctrl-C does
    with (NoLog() if args.no_log else args.out.open("a+", encoding="utf-8")) as out:
        session = Session(cfg, gdb, out)
        try:
            session.run(args.session)
        except KeyboardInterrupt:
            try:
                session.close()
            except (OSError, ConnectionError) as error:
                print(f"could not restore the game: {error}", file=sys.stderr)
        except ConnectionError as error:
            print(error, file=sys.stderr)
        print(f"{session.count} records in {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
