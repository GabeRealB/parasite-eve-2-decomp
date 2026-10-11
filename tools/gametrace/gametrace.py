#!/usr/bin/env python3
"""Record what the running game does, through a GDB server.

    gametrace.py run   [--config tools/gametrace/events.toml] [--out local/gametrace/trace.jsonl]
                       [--host HOST] [--port PORT]      (these override the config's [gdb] section)
                       [--now]                          (attach to a game already running: no install_at wait)
    gametrace.py check [--config ...]     resolve the events, build the patch blob, show both; no connection
    gametrace.py blob  [--config ...]     write the assembled blob and its listing under local/gametrace/

One config file lists the events: an id, a name, the address of a function, and whether it is watched by a
breakpoint or by a patch. Every event is recorded the same way - its id and the registers at the function's entry
- and what the numbers mean is decided afterwards, by whoever reads the log. Nothing in the game's files changes.

  breakpoint  the target stops at the address, this tool reads the registers and continues. Nothing is written
              into the game. Each hit is a round trip, so it suits events that happen a few times a second.
  patch       the function's first two instructions are replaced by a jump to a small stub at `blob.address` (the target
              must have memory there that the game leaves alone; by default that is just above 2 MB). The stub calls one shared handler, which appends a record to a ring
              buffer, then runs the two displaced instructions and returns to the third. This tool reads the ring
              every `drain_seconds`, by interrupting the game for a moment. It suits events too frequent to stop for.

The tool expects to be started with the game paused, before it has run. It continues the game itself up to the
first `install_at` address - a function by which memory is set up, such as the program's `main` - and only there
writes the blob, the patches and the event breakpoints. Each later hit of an `install_at` address applies the
patches again, which is how a function in an image loaded later gets its patch. Without `install_at` everything is
written at once, into the game as it stands.

A patch is written only where the function's own first two instructions are in memory, and is written again when
an image load has replaced them, so an address shared by several overlays is only ever patched under the right one.
The two instructions it displaces must be free to move: `check` refuses a branch, a jump, or a load that follows
one (see `movable`).

A record is one JSON line: seq, id, name, a0..a3, ra, sp, the name of the function `ra` returns into, and for a
breakpoint event whatever the event asks for besides: `ret` (v0 on return), `stack` (words above the frame),
`mem` (bytes at a register, "a0:16").
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
RECORD = 32      # bytes of one ring record: id, a0, a1, a2, a3, ra, sp, seq
PC = 37          # the program counter's place in the `g` packet, after r0..r31, sr, lo, hi, bad, cause


# ---- the config

def load(path: Path) -> dict:
    cfg = tomllib.loads(path.read_text(encoding="utf-8"))
    gdb, blob = cfg.setdefault("gdb", {}), cfg.setdefault("blob", {})
    gdb.setdefault("host", "127.0.0.1")
    gdb.setdefault("port", 2345)
    blob.setdefault("address", 0x80200000)
    blob.setdefault("records", 1024)
    at = cfg.get("install_at", blob.get("install_at", []))
    cfg["install_at"] = [at] if isinstance(at, int) else list(at)
    problems, seen = [], {}
    for event in cfg.get("event", []):
        for field in ("id", "name", "address"):
            if field not in event:
                problems.append(f"an event lacks `{field}`: {event}")
        event.setdefault("patch", False)
        if event.get("id") in seen:
            problems.append(f"event id {event['id']} is used by {seen[event['id']]} and {event.get('name')}")
        seen[event.get("id")] = event.get("name")
        if event["patch"] and any(k in event for k in ("ret", "stack", "mem")):
            problems.append(f"{event.get('name')}: ret, stack and mem are read by this tool at a breakpoint; a patch records the registers only")
    if blob["records"] & (blob["records"] - 1):
        problems.append("blob.records must be a power of two")
    blob.setdefault("drain_seconds", 1.0)
    if problems:
        raise SystemExit("\n".join(problems))
    return cfg


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


def stub_source(cfg: dict, originals: dict[int, tuple[int, int]]) -> str:
    """The blob as assembly: the ring's header, one shared handler, and a stub per patched event."""
    blob = cfg["blob"]
    lines = [
        ".set noreorder", ".set noat", ".text",
        "# header: the next sequence number, then how many records the ring holds",
        "gt_seq:    .word 0",
        f"gt_count:  .word {blob['records']}",
        "",
        "# The one handler. In: t0 = the event's id, and the stub's frame at sp (the caller's ra at 0, t0 at 4).",
        "# Everything else is as the hooked function received it, and is left so.",
        "gt_handler:",
        "    addiu $sp, $sp, -16",
        "    sw    $t1, 0($sp)",
        "    sw    $t2, 4($sp)",
        "    sw    $t3, 8($sp)",
        "    lui   $t1, %hi(gt_seq)",
        "    lw    $t2, %lo(gt_seq)($t1)",
        f"    andi  $t3, $t2, {blob['records'] - 1}",
        "    sll   $t3, $t3, 5",
        "    lui   $t1, %hi(gt_ring)",
        "    addiu $t1, $t1, %lo(gt_ring)",
        "    addu  $t3, $t3, $t1",
        "    sw    $t0, 0($t3)",
        "    sw    $a0, 4($t3)",
        "    sw    $a1, 8($t3)",
        "    sw    $a2, 12($t3)",
        "    sw    $a3, 16($t3)",
        "    lw    $t0, 16($sp)          # the caller's return address, saved by the stub",
        "    addiu $t1, $sp, 32          # the stack pointer the function was entered with",
        "    sw    $t0, 20($t3)",
        "    sw    $t1, 24($t3)",
        "    sw    $t2, 28($t3)",
        "    addiu $t2, $t2, 1",
        "    lui   $t1, %hi(gt_seq)",
        "    sw    $t2, %lo(gt_seq)($t1)",
        "    lw    $t1, 0($sp)",
        "    lw    $t2, 4($sp)",
        "    lw    $t3, 8($sp)",
        "    jr    $ra",
        "    addiu $sp, $sp, 16",
        "",
    ]
    for event in cfg.get("event", []):
        if not event["patch"]:
            continue
        first, second = originals[event["id"]]
        lines += [
            f"# event {event['id']}: {event['name']} at {event['address']:#010x}",
            f"gt_stub_{event['id']}:",
            "    addiu $sp, $sp, -16",
            "    sw    $ra, 0($sp)",
            "    sw    $t0, 4($sp)",
            "    jal   gt_handler",
            f"    ori   $t0, $zero, {event['id']}",
            "    lw    $ra, 0($sp)",
            "    lw    $t0, 4($sp)",
            "    addiu $sp, $sp, 16",
            f"    .word {first:#010x}          # the function's own first instruction",
            f"    .word {second:#010x}          # and its second",
            f"    j     {event['address'] + 8:#010x}",
            "    nop",
            "",
        ]
    lines += ["    .balign 32", "gt_ring:", f"    .space {blob['records'] * RECORD}", ""]
    return "\n".join(lines)


def assemble(cfg: dict, originals: dict[int, tuple[int, int]]) -> tuple[bytes, dict[str, int], str]:
    """(blob bytes, symbol -> address, listing), with the system's MIPS binutils."""
    OUT.mkdir(parents=True, exist_ok=True)
    src, obj, elf, raw = OUT / "blob.s", OUT / "blob.o", OUT / "blob.elf", OUT / "blob.bin"
    src.write_text(stub_source(cfg, originals), encoding="utf-8")
    base = cfg["blob"]["address"]
    for cmd in (["mipsel-linux-gnu-as", "-mips1", "-EL", "-o", str(obj), str(src)],
                ["mipsel-linux-gnu-ld", "-EL", "-Ttext", f"{base:#x}", "-e", "gt_handler", "-o", str(elf), str(obj)],
                ["mipsel-linux-gnu-objcopy", "-O", "binary", "-j", ".text", str(elf), str(raw)]):
        done = subprocess.run(cmd, capture_output=True, text=True)
        if done.returncode:
            raise SystemExit(f"{' '.join(cmd)}\n{done.stderr}")
    symbols = {}
    for line in subprocess.run(["mipsel-linux-gnu-nm", str(elf)], capture_output=True, text=True).stdout.splitlines():
        cells = line.split()
        if len(cells) == 3 and cells[2].startswith("gt_"):
            symbols[cells[2]] = int(cells[0], 16)
    listing = subprocess.run(["mipsel-linux-gnu-objdump", "-d", "--stop-address", f"{symbols['gt_ring']:#x}", str(elf)], capture_output=True, text=True).stdout
    code = raw.read_bytes()[: symbols["gt_ring"] - base]      # the ring itself is not sent: the game's memory there is zero or stale, and seq says what is valid
    return code, symbols, listing


# ---- where a function's first instructions are, without a running game: the image on disc

def image_words(address: int) -> tuple[int, int] | None:
    """The two words at `address` in the executable or gameplay, the images always resident."""
    exe = ROOT / "assets" / "USA" / "main.exe"
    if exe.is_file():
        data = exe.read_bytes()
        load, size = struct.unpack_from("<2I", data, 0x18)
        if load <= address < load + size - 8:
            return struct.unpack_from("<2I", data, 0x800 + address - load)
    pkg = ROOT / "assets" / "USA" / "pe2pkg" / "gameplay.pe2pkg"
    if pkg.is_file():
        data, load = pkg.read_bytes(), 0x80093800
        if load <= address < load + len(data) - 8:
            return struct.unpack_from("<2I", data, address - load)
    return None


def originals_of(cfg: dict) -> dict[int, tuple[int, int]]:
    """Each patch event's own first two instructions: stated in the config (`words`), or read from the image."""
    out, problems = {}, []
    for event in cfg.get("event", []):
        if not event["patch"]:
            continue
        words = tuple(event["words"]) if "words" in event else image_words(event["address"])
        if words is None or len(words) != 2:
            problems.append(f"{event['name']}: {event['address']:#010x} is in neither the executable nor gameplay; give its first two instructions as `words = [0x..., 0x...]`")
            continue
        for n, word in enumerate(words):
            why = movable(word)
            if why:
                problems.append(f"{event['name']}: instruction {n + 1} at its entry ({word:#010x}) is {why} and cannot be displaced; watch it with a breakpoint instead")
        out[event["id"]] = words
    if problems:
        raise SystemExit("\n".join(problems))
    return out


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
            n = min(0x400, length - len(out))
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
        self.by_address = {e["address"]: e for e in self.events if not e["patch"]}
        self.patched = [e for e in self.events if e["patch"]]
        self.pending: dict[int, list[dict]] = {}       # return address -> records waiting for their v0
        self.seen = 0                                 # ring records already written out
        self.count = 0
        if self.patched:
            self.originals = originals_of(cfg)
            self.code, self.symbols, _ = assemble(cfg, self.originals)

    def emit(self, record: dict) -> None:
        # Registers are addresses and packed ids more often than counts: written in hex.
        for key in ("a0", "a1", "a2", "a3", "ra", "sp", "ret"):
            if isinstance(record.get(key), int):
                record[key] = f"{record[key]:#010x}"
        if "stack" in record:
            record["stack"] = [f"{word:#010x}" for word in record["stack"]]
        self.out.write(json.dumps(record) + "\n")
        self.out.flush()
        self.count += 1

    def start(self) -> None:
        """Wait for the game to reach the first `install_at` address (it is continued to there), then set up."""
        self.gdb.halt_reason()
        self.installers = set(self.cfg["install_at"])
        self.ready = not self.installers
        if self.ready:
            self.setup()
        else:
            for address in self.installers:
                self.gdb.breakpoint(address)
            print(f"waiting for the game to reach {', '.join(self.names.at(a) for a in sorted(self.installers))}", file=sys.stderr)

    def setup(self) -> None:
        self.ready = True
        if self.patched:
            base = self.cfg["blob"]["address"]
            self.refuse_mirror(base)
            try:
                self.gdb.write(base, self.code)
            except OSError as error:
                raise SystemExit(f"{error}\nThe patch blob lives at {base:#010x} (blob.address): the target has to have writable memory there.")
            if "drain" in self.cfg["blob"]:
                self.gdb.breakpoint(self.cfg["blob"]["drain"])
            self.install()
        for address in self.by_address:
            self.gdb.breakpoint(address)

    def refuse_mirror(self, base: int) -> None:
        """Stop before writing the blob into memory that is really somewhere else.

        A console with 2 MB of memory shows it again at every 2 MB above: 0x80200000 is then 0x80000000, the
        kernel's own tables and its exception vector, and a blob written there takes the machine down. Memory that
        is its own differs from what lies 2 MB below it; where the two read the same, or nothing has been loaded
        yet and both are empty, nothing is written."""
        span, step = 0x200, 0x200000
        here = self.gdb.read(base, span)
        lower = base - step
        while (lower & 0x1FFFFFFF) >= 0:
            try:
                below = self.gdb.read(lower, span)
            except OSError:
                break
            if below == here:
                what = "is empty, like the memory below it: start once the game has booted" if not any(here) else f"is the same memory as {lower:#010x}"
                raise SystemExit(f"blob.address {base:#010x} {what}.\nNothing was written. The patch blob needs memory of its own - "
                                 "a target with memory above 2 MB (an 8 MB mode), or an address inside the 2 MB that the game leaves free. "
                                 "Events with patch = false need none of this.")
            if (lower & 0x1FFFFFFF) < step:
                break
            lower -= step

    def install(self) -> None:
        """Write each patch whose function is in memory unpatched (a load may have put it back)."""
        for event in self.patched:
            first, second = self.originals[event["id"]]
            now = struct.unpack("<2I", self.gdb.read(event["address"], 8))
            if now == (first, second):
                stub = self.symbols[f"gt_stub_{event['id']}"]
                jump = 0x08000000 | ((stub >> 2) & 0x03FFFFFF)
                self.gdb.write(event["address"], struct.pack("<2I", jump, 0))

    def remove(self) -> None:
        """Put back the instructions the patches displaced, where the patches are still in place."""
        for event in self.patched:
            stub = self.symbols[f"gt_stub_{event['id']}"]
            jump = 0x08000000 | ((stub >> 2) & 0x03FFFFFF)
            if struct.unpack("<2I", self.gdb.read(event["address"], 8)) == (jump, 0):
                self.gdb.write(event["address"], struct.pack("<2I", *self.originals[event["id"]]))

    def close(self) -> None:
        """Leave the game as it was found, running: no patch, no breakpoint. What the ring still holds is read first."""
        if self.gdb.sock.fileno() < 0:
            return
        running = getattr(self, "running", False)
        if running:
            self.gdb.sock.sendall(b"\x03")
            while not Gdb._is_stop(self.gdb._recv() or ""):
                pass
        if self.patched and self.ready:
            self.drain()
            self.remove()
        self.gdb.release(False)

    def drain(self) -> None:
        base = self.cfg["blob"]["address"]
        seq, count = struct.unpack("<2I", self.gdb.read(base, 8))
        if seq == self.seen:
            return
        ring = self.symbols["gt_ring"]
        lost = max(0, seq - self.seen - count)
        if lost:
            self.emit({"lost": lost, "note": "the ring wrapped before it was read; raise blob.records or drain more often"})
        names = {e["id"]: e["name"] for e in self.patched}
        for n in range(max(self.seen, seq - count), seq):
            ident, a0, a1, a2, a3, ra, sp, stamp = struct.unpack("<8I", self.gdb.read(ring + (n & (count - 1)) * RECORD, RECORD))
            self.emit({"seq": stamp, "id": ident, "name": names.get(ident, "?"), "a0": a0, "a1": a1, "a2": a2, "a3": a3, "ra": ra, "sp": sp, "from": self.names.at(ra), "by": "patch"})
        self.seen = seq

    def hit(self, event: dict, regs: list[int]) -> None:
        r = dict(zip(REGS, regs))
        record = {"id": event["id"], "name": event["name"], "a0": r["a0"], "a1": r["a1"], "a2": r["a2"], "a3": r["a3"], "ra": r["ra"], "sp": r["sp"], "from": self.names.at(r["ra"]), "by": "breakpoint"}
        if event.get("stack"):
            words = struct.unpack(f"<{event['stack']}I", self.gdb.read(r["sp"] + 0x10, 4 * event["stack"]))
            record["stack"] = list(words)
        for spec in event.get("mem", []):
            reg, _, length = spec.partition(":")
            try:
                record.setdefault("mem", {})[spec] = self.gdb.read(r[reg], int(length, 0)).hex()
            except (OSError, KeyError, ValueError) as error:
                record.setdefault("mem", {})[spec] = f"unreadable: {error}"
        if event.get("ret"):
            if r["ra"] not in self.pending:
                self.gdb.breakpoint(r["ra"])
            self.pending.setdefault(r["ra"], []).append(record)
        else:
            self.emit(record)

    def run(self) -> None:
        self.start()
        drain_at = self.cfg["blob"].get("drain") if self.patched else None
        every, frames = self.cfg["blob"].get("drain_every", 1), 0
        pc = None
        period = self.cfg["blob"]["drain_seconds"] if self.patched and drain_at is None else None
        while True:
            self.running = True
            by_itself = self.gdb.resume(pc, period if self.ready else None)
            self.running = False
            if not by_itself:                         # the timer: read the ring, put back any patch a load removed
                pc = None
                self.drain()
                self.install()
                continue
            regs = self.gdb.registers()
            pc = regs[PC]
            if pc in self.pending:                    # a watched function has returned
                for record in self.pending.pop(pc):
                    record["ret"] = regs[2]
                    self.emit(record)
                if pc not in self.by_address and pc != drain_at and pc not in self.installers:
                    self.gdb.breakpoint(pc, False)
            if pc in self.installers:
                if not self.ready:
                    self.setup()
                    self.emit({"note": f"set up at {self.names.at(pc)}", "patches": len(self.patched), "breakpoints": len(self.by_address)})
                elif self.patched:
                    self.install()
            if not self.ready:
                continue
            if pc == drain_at:
                frames += 1
                if frames % every == 0:
                    self.drain()
                    self.install()
            if pc in self.by_address:
                self.hit(self.by_address[pc], regs)


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
            print(f"{event['id']:>4}  {event['name']:<28} {event['address']:#010x}  {'patch' if event['patch'] else 'breakpoint':<10} {names.at(event['address'])}")
        if any(e["patch"] for e in cfg.get("event", [])):
            code, symbols, listing = assemble(cfg, originals_of(cfg))
            how = f"at {cfg['blob']['drain']:#010x} ({names.at(cfg['blob']['drain'])})" if "drain" in cfg["blob"] else f"every {cfg['blob']['drain_seconds']} s, by interrupting the game"
            print(f"\nblob: {len(code)} bytes of code at {cfg['blob']['address']:#010x}, ring of {cfg['blob']['records']} records at {symbols['gt_ring']:#010x}; read {how}")
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
