#!/usr/bin/env python3
"""Record what the running game does, through a GDB server.

    gametrace.py run   [--config tools/gametrace/events.toml] [--out local/gametrace/trace.jsonl]
                       [--host HOST] [--port PORT]      (these override the config's [gdb] section)
                       [--now]                          (attach to a game already running: no install_at wait)
    gametrace.py check [--config ...]     resolve the events, build the blob, show both; no connection
    gametrace.py blob  [--config ...]     write the assembled blob and its listing under local/gametrace/

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
writes the blob and the hooks. Each later hit of an `install_at` address applies the hooks again, which is how a
function in an image loaded later gets its hook; so does every read of the buffer. A hook is written only where
the instructions it replaces are in memory as the image has them, so an address shared by several overlays is only
ever hooked under the right one. An instruction a hook displaces must be free to move: `check` refuses a branch
or a jump there (see `movable`).

The blob needs memory of its own (`blob.address`); by default just above 2 MB, which a target has only with more
than the console's 2 MB. The tool refuses to write where that memory is a mirror of lower memory.

A record is one JSON line: id, name, phase ("enter" or "exit"), then each capture under its own text; a captured
`ra` also gives `from`, the function it returns into.
"""
from __future__ import annotations

import argparse
import json
import re
import select
import socket
import struct
import subprocess
import sys
import tomllib
from bisect import bisect_right
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "local" / "gametrace"
REGS = ["zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"]
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
        for old in ("patch", "ret", "stack", "mem"):
            if old in event:
                problems.append(f"{event.get('name')}: `{old}` is gone - every event is a hook now; list what to record in `capture`, and returns in `exits`")
        event.setdefault("capture", ["a0", "a1", "a2", "a3", "ra"])
        event.setdefault("exits", [])
        event.setdefault("exit_capture", ["v0"])
        try:
            event["_enter"] = [parse_capture(c) for c in event["capture"]]
            event["_exit"] = [parse_capture(c) for c in event["exit_capture"]]
        except ValueError as error:
            problems.append(f"{event.get('name')}: {error}")
    if blob["bytes"] % 4 or blob["bytes"] < 0x1000:
        problems.append("blob.bytes must be a multiple of four, and at least 4096")
    if problems:
        raise SystemExit("\n".join(problems))
    return cfg


def record_size(captures: list[dict]) -> int:
    return 4 + sum(c["size"] for c in captures)


def header(event: dict, phase: int, captures: list[dict]) -> int:
    """A record's first word: the event's id, whether it is an exit, and the record's whole length in bytes."""
    return event["id"] | (phase << 16) | (record_size(captures) << 17)


# ---- names, from the symbol maps: only to say where a call came from

class Names:
    def __init__(self) -> None:
        line = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]{8})\s*;")
        self.by_image: dict[str, list[tuple[int, str]]] = {}
        for path in sorted((ROOT / "configs" / "USA").rglob("*.txt")):
            if "imports" in path.name:
                continue
            image = path.stem.removeprefix("sym.")
            found = sorted((int(m[2], 16), m[1]) for m in map(line.match, path.read_text(errors="replace").splitlines()) if m)
            if found:
                self.by_image[image] = found

    def at(self, address: int, images: tuple[str, ...] = ("main", "gameplay")) -> str:
        """The symbol at or before `address` in the resident images; an overlay's address is left as a number."""
        best = None
        for image in images:
            table = self.by_image.get(image) or []
            i = bisect_right(table, (address, "￿")) - 1
            if i >= 0 and address - table[i][0] < 0x4000 and (best is None or table[i][0] > best[0]):
                best = table[i]
        return f"{best[1]}+{address - best[0]:#x}" if best else f"{address:#010x}"


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


def record_code(event: dict, phase: int, captures: list[dict], tag: str, largest: int) -> list[str]:
    """A stub's middle: save what it uses, make room, write the record, restore."""
    size = record_size(captures)
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
    events = cfg.get("event", [])
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
        lines += record_code(event, 0, event["_enter"], f"e{event['id']}", largest)
        lines += [f"    .word {first:#010x}          # the function's own first instruction", f"    .word {second:#010x}          # and its second",
                  f"    j     {event['address'] + 8:#010x}", "    nop", ""]
        for k, offset in enumerate(event["exits"]):
            _jr, delay = sites[(event["id"], "exit", offset)]
            lines += [f"# event {event['id']}: {event['name']} returns at +{offset:#x}", f"gt_exit_{event['id']}_{k}:",
                      f"    .word {delay:#010x}          # the return's delay-slot instruction, which ran before the return did"]
            lines += record_code(event, 1, event["_exit"], f"x{event['id']}_{k}", largest)
            lines += ["    jr    $ra", "    nop", ""]
    lines += ["    .balign 4", "gt_buffer:", f"    .space {blob['bytes']}", "gt_end:", ""]
    return "\n".join(lines)


def assemble(cfg: dict, sites: dict) -> tuple[bytes, dict[str, int], str]:
    """(the blob's code, symbol -> address, listing), with the system's MIPS binutils. The buffer is not part of
    what is written into the game: only the cursor in front of it has to be right."""
    OUT.mkdir(parents=True, exist_ok=True)
    src, obj, elf, raw = OUT / "blob.s", OUT / "blob.o", OUT / "blob.elf", OUT / "blob.bin"
    src.write_text(stub_source(cfg, sites), encoding="utf-8")
    base = cfg["blob"]["address"]
    for cmd in (["mipsel-linux-gnu-as", "-mips1", "-EL", "-o", str(obj), str(src)],
                ["mipsel-linux-gnu-ld", "-EL", "-Ttext", f"{base:#x}", "-e", "gt_drain", "-o", str(elf), str(obj)]):
        done = subprocess.run(cmd, capture_output=True, text=True)
        if done.returncode:
            raise SystemExit(f"{' '.join(cmd)}\n{done.stderr}")
    symbols = {}
    for line in subprocess.run(["mipsel-linux-gnu-nm", str(elf)], capture_output=True, text=True).stdout.splitlines():
        cells = line.split()
        if len(cells) == 3 and cells[2].startswith("gt_"):
            symbols[cells[2]] = int(cells[0], 16)
    end = symbols["gt_buffer"]
    done = subprocess.run(["mipsel-linux-gnu-objcopy", "-O", "binary", "-j", ".text", str(elf), str(raw)], capture_output=True, text=True)
    if done.returncode:
        raise SystemExit(done.stderr)
    code = raw.read_bytes()[: end - base]
    raw.write_bytes(code)
    listing = subprocess.run(["mipsel-linux-gnu-objdump", "-d", "--stop-address", f"{end:#x}", str(elf)], capture_output=True, text=True).stdout
    return code, symbols, listing


# ---- what a hook replaces, read from the image on disc

def image_words(address: int, count: int, image: str | None) -> tuple[int, ...] | None:
    """`count` instructions at `address` in the executable, gameplay, or the package the event names."""
    def within(data: bytes, load: int, skip: int = 0) -> tuple[int, ...] | None:
        if load <= address and address + 4 * count <= load + len(data) - skip:
            return struct.unpack_from(f"<{count}I", data, skip + address - load)
        return None
    if image is None:
        exe = ROOT / "assets" / "USA" / "main.exe"
        if exe.is_file():
            data = exe.read_bytes()
            found = within(data, struct.unpack_from("<I", data, 0x18)[0], 0x800)
            if found:
                return found
        image, load = "gameplay", 0x80093800
    else:
        sys.path.insert(0, str(ROOT / "tools" / "peassets"))
        import asset_residency
        load = asset_residency.package_loads(tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text())).get(image)
        if load is None:
            raise SystemExit(f"image {image!r} is not a package of configs/USA/overlays.toml")
    pkg = ROOT / "assets" / "USA" / "pe2pkg" / f"{image}.pe2pkg"
    return within(pkg.read_bytes(), load) if pkg.is_file() else None


def sites_of(cfg: dict) -> dict:
    """Every place a hook goes and the two instructions there now: (id, "enter") and (id, "exit", offset)."""
    out, problems = {}, []
    for event in cfg.get("event", []):
        name, image = event["name"], event.get("image")
        words = image_words(event["address"], 2, image)
        if words is None:
            problems.append(f"{name}: {event['address']:#010x} is in neither the executable nor gameplay; name its package with `image = \"...\"`")
            continue
        for n, word in enumerate(words):
            if movable(word):
                problems.append(f"{name}: instruction {n + 1} at its entry ({word:#010x}) is {movable(word)} and cannot be displaced")
        out[(event["id"], "enter")] = words
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


def returns_in(event: dict, names: "Names") -> list[int]:
    """The offsets of the `jr $ra` instructions between the event's address and the next symbol."""
    image = event.get("image")
    table = names.by_image.get(image or "main", []) + (names.by_image.get("gameplay", []) if image is None else [])
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
        self.step_over: bool | None = None            # whether this server needs it; found out at the first stop
        self.chunk = 0x400                            # bytes per memory read; raised to what the server's packets hold
        self.sock.sendall(b"+")

    def _send(self, payload: str) -> None:
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
                return packet.decode(errors="replace")
            if wait is not None and not select.select([self.sock], [], [], wait)[0]:
                return None
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("the GDB server closed the connection")
            self.buf += chunk

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
        while self._recv(0.005) is not None:          # the same stop, reported again
            pass
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

    def release(self, running: bool) -> None:
        """Take every breakpoint out and let the target run: a breakpoint left behind stops it with nobody listening."""
        if running:
            self.sock.sendall(b"\x03")
            while not self._is_stop(self._recv() or ""):
                pass
        for address in sorted(self.set):
            self.breakpoint(address, False)
        self._send("c")


# ---- a session

class Session:
    def __init__(self, cfg: dict, gdb: Gdb, out) -> None:
        self.cfg, self.gdb, self.out, self.names = cfg, gdb, out, Names()
        self.events = cfg.get("event", [])
        self.count = 0
        self.ready = self.running = False
        self.sites = sites_of(cfg)
        self.code, self.symbols, _ = assemble(cfg, self.sites)
        self.layout = {}                              # a record's first word -> (event, phase, captures)
        self.hooks = []                               # (address, the two words there in the image, the stub)
        for event in self.events:
            self.layout[header(event, 0, event["_enter"])] = (event, "enter", event["_enter"])
            self.hooks.append((event["address"], self.sites[(event["id"], "enter")], self.symbols[f"gt_enter_{event['id']}"]))
            for k, offset in enumerate(event["exits"]):
                self.layout[header(event, 1, event["_exit"])] = (event, "exit", event["_exit"])
                self.hooks.append((event["address"] + offset, self.sites[(event["id"], "exit", offset)], self.symbols[f"gt_exit_{event['id']}_{k}"]))

    def emit(self, record: dict) -> None:
        self.out.write(json.dumps(record) + "\n")
        self.count += 1

    def start(self) -> None:
        """Wait for the game to reach the first `install_at` address (it is continued to there), then set up."""
        self.gdb.halt_reason()
        self.installers = set(self.cfg["install_at"])
        if not self.installers:
            self.setup()
        else:
            for address in self.installers:
                self.gdb.breakpoint(address)
            print(f"waiting for the game to reach {', '.join(self.names.at(a) for a in sorted(self.installers))}", file=sys.stderr)

    def setup(self) -> None:
        base = self.cfg["blob"]["address"]
        self.refuse_mirror(base)
        try:
            self.gdb.write(base, self.code)
        except OSError as error:
            raise SystemExit(f"{error}\nThe blob lives at {base:#010x} (blob.address): the target has to have writable memory there.")
        self.gdb.breakpoint(self.symbols["gt_full"])
        self.ready = True
        print(f"{self.install()} of {len(self.hooks)} hooks in place; the rest are written when their code is loaded", file=sys.stderr)

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

    def install(self) -> int:
        """Write each hook whose place holds the image's own instructions (a load may have put them back).
        Returns how many hooks are in place."""
        placed = 0
        for address, original, stub in self.hooks:
            jump = (0x08000000 | ((stub >> 2) & 0x03FFFFFF), 0)
            now = struct.unpack("<2I", self.gdb.read(address, 8))
            if now == tuple(original):
                self.gdb.write(address, struct.pack("<2I", *jump))
                now = jump
            placed += now == jump
        return placed

    def remove(self) -> None:
        """Put back what the hooks replaced, where the hooks are still in place."""
        for address, original, stub in self.hooks:
            jump = (0x08000000 | ((stub >> 2) & 0x03FFFFFF), 0)
            if struct.unpack("<2I", self.gdb.read(address, 8)) == jump:
                self.gdb.write(address, struct.pack("<2I", *original))

    def drain(self, reset: bool) -> None:
        """Write out everything the buffer holds. `reset` starts it again from here; where the game is stopped on
        gt_full its own next instructions do that."""
        start = self.symbols["gt_buffer"]
        cursor = struct.unpack("<I", self.gdb.read(self.symbols["gt_cursor"], 4))[0]
        if not start < cursor <= self.symbols["gt_end"]:
            return
        data, at = self.gdb.read(start, cursor - start), 0
        while at + 4 <= len(data):
            word = struct.unpack_from("<I", data, at)[0]
            known = self.layout.get(word)
            if known is None:
                self.emit({"note": f"the buffer does not parse from byte {at} of {len(data)} (first word {word:#010x}); the rest of this read is dropped"})
                break
            event, phase, captures = known
            record, field = {"id": event["id"], "name": event["name"], "phase": phase}, at + 4
            for c in captures:
                if c["kind"] == "reg":
                    value = struct.unpack_from("<I", data, field)[0]
                    record[c["text"]] = f"{value:#010x}"
                    if c["reg"] == "ra":
                        record["from"] = self.names.at(value)
                else:
                    record[c["text"]] = data[field:field + c["length"]].hex()
                field += c["size"]
            self.emit(record)
            at = field
        self.out.flush()
        if reset:
            self.gdb.write(self.symbols["gt_cursor"], struct.pack("<I", start))

    def close(self) -> None:
        """Leave the game as it was found, running: no hook, no breakpoint. What the buffer still holds is read first."""
        if self.gdb.sock.fileno() < 0:
            return
        if self.running:
            self.gdb.sock.sendall(b"\x03")
            while not Gdb._is_stop(self.gdb._recv() or ""):
                pass
        if self.ready:
            self.drain(reset=True)
            self.remove()
        self.gdb.release(False)

    def run(self) -> None:
        self.start()
        pc = None
        while True:
            self.running = True
            self.gdb.resume(pc)
            self.running = False
            pc = self.gdb.registers()[PC]
            if pc in self.installers:
                if not self.ready:
                    self.setup()
                else:
                    self.install()
            elif self.ready and pc == self.symbols["gt_full"]:
                self.drain(reset=False)
                self.install()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["run", "check", "blob"])
    ap.add_argument("--config", type=Path, default=Path(__file__).with_name("events.toml"))
    ap.add_argument("--out", type=Path, default=OUT / "trace.jsonl")
    ap.add_argument("--host", help="the GDB server's address, instead of the config's gdb.host")
    ap.add_argument("--port", type=int, help="the GDB server's port, instead of the config's gdb.port")
    ap.add_argument("--now", action="store_true", help="set up at once in the game as it stands, ignoring install_at: for attaching to a game already running")
    args = ap.parse_args()
    cfg = load(args.config)
    if args.host:
        cfg["gdb"]["host"] = args.host
    if args.port:
        cfg["gdb"]["port"] = args.port
    if args.now:
        cfg["install_at"] = []
    names = Names()
    if args.command in ("check", "blob"):
        for address in cfg["install_at"]:
            print(f"set up at {address:#010x}  {names.at(address)}")
        for event in cfg.get("event", []):
            found = returns_in(event, names)
            print(f"{event['id']:>5}  {event['name']:<24} {event['address']:#010x}  {names.at(event['address'])}")
            print(f"       enter: {', '.join(event['capture'])}  ({record_size(event['_enter'])} bytes)")
            if event["exits"]:
                print(f"       exit at {', '.join(f'+{o:#x}' for o in event['exits'])}: {', '.join(event['exit_capture'])}  ({record_size(event['_exit'])} bytes)")
            missing = [o for o in found if o not in event["exits"]]
            if missing:
                print(f"       returns not hooked: {', '.join(f'{o:#x}' for o in missing)}   (exits = [{', '.join(f'{o:#x}' for o in found)}] hooks them all)")
        code, symbols, listing = assemble(cfg, sites_of(cfg))
        print(f"\nblob: {len(code)} bytes of code at {cfg['blob']['address']:#010x}, buffer of {cfg['blob']['bytes']} bytes at {symbols['gt_buffer']:#010x}; "
              f"the game stops only at {symbols['gt_full']:#010x}, when the buffer is full")
        if args.command == "blob":
            (OUT / "blob.lst").write_text(listing, encoding="utf-8")
            print(f"written: {OUT / 'blob.bin'}, {OUT / 'blob.s'}, {OUT / 'blob.lst'}")
        return 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    try:
        gdb = Gdb(cfg["gdb"]["host"], cfg["gdb"]["port"])
    except OSError as error:
        raise SystemExit(f"no GDB server at {cfg['gdb']['host']}:{cfg['gdb']['port']} ({error}). Start the server, or give its address with --host and --port.")
    with args.out.open("a", encoding="utf-8") as out:
        session = Session(cfg, gdb, out)
        try:
            session.run()
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
