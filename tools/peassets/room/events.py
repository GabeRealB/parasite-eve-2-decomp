"""A room's events as text: its event scripts and the payloads they send, `<group>/events.evs`.

A payload is declared by its kind and name, one object or an array of them:

    transform OpeningPlayerLeadInTransform = at (1937, 0, -492) turn (0, 1224, 0)
    animation OpeningViewFigureLeadInAnimationRequest = clip 1 bank 0 blend 0 frames 0 collision 0
    command HideStrangerCommand = stage 1 area 3 command 0x5
    bank_copy FollowUpPlayerBankCopyRequest = &_gAcropolisPatioFollowUpPlayerAnimationSets count 3
    scene_key OpeningSceneKey = group 1 stream 2 sub 0
    move_animations PierceRunAnimations = approach 3 arrival 0
    walk_animation StrangerWalk = clip 2 next 4
    transform LiftPositions[]
        at (0, 0, 0) turn (0, 0, 0)
        at (0, -200, 0) turn (0, 0, 0)

and a script by its name, its commands indented one a line:

    script OpeningScript
        cap_control.cap_start 1
        stop_area_music 10
        scene[0].scene_actor_place OpeningOriginTransform
        player.weapon_animation WeaponAnimationRequest
        wait_frames 72
        end

A message is `<recipient>.<message> [payload [second payload]]`: the recipient is a task slot (player, companion,
cap_control, room, room_effect, ...), `scene[i]` a scene actor by placement index, `scene.manager`, or `child[id]`;
the message is named from event_vocabulary.py, else `message_0x...`. Every other command is its opcode's name and
the operands it reads, zeros included; a command with an operand set that its opcode does not read is written raw:
`raw <opcode> a b c d e`.

Names: the room's own objects by their symbol less the room's prefix (`_gAcropolisPatioX` is `_X`, `gAcropolisPatioX`
is `X`), a command inside a script as `Script[k]`, anything else by its full symbol after `&`, and an address
nothing names as a number.
"""
from __future__ import annotations

import re
import struct

from .event_vocabulary import MESSAGES, OPCODES, SLOTS

KINDS = {"animation": "AnimationPlayRequest", "transform": "ActorTransform", "command": "ActorCommand",
         "bank_copy": "AnimationBankCopyRequest", "scene_key": "EvsSceneKey", "move_animations": "GameActorMoveAnim",
         "walk_animation": "ActorMotionWalkAnim", "area_update": "AreaApplyRec", "script": "EvsCommand"}
KEYWORDS = {v: k for k, v in KINDS.items()}
# Operands an opcode reads; send_message and play_weapon_animation have forms of their own.
ARITY = {"end": 0, "start_flash": 2, "set_view": 1, "wait_frames": 1, "cancel_primary_fade": 0, "restore_hud": 0,
         "set_event_state": 1, "wait_animation": 1, "wait_cap_cue": 0, "restore_view": 0, "select_scene": 1,
         "callback": 2, "start_vibration": 2, "start_sound": 3, "stop_sound": 2, "set_framebuffer_blend": 1,
         "start_area_music": 1, "stop_area_music": 1, "request_scene_music": 3, "wait_music_load": 0, "shake_screen": 2,
         "cleanup_scene": 0, "start_primary_fade": 2, "return_primary_fade": 0, "fade_volume": 2,
         "fade_sound_attenuation": 3, "rebuild_tmd_buffers": 0, "wait_actor_action": 1, "play_scene_audio": 0,
         "start_scene_audio": 0, "set_ambient_rgb": 3, "clear_ambient_rgb": 0, "finish_scene_stream": 0,
         "start_secondary_fade": 3, "return_secondary_fade": 1, "cancel_secondary_fade": 0, "restore_weapons": 1,
         "hide_weapons": 1, "set_light_scale": 1, "set_cap_direct_view_ids": 1, "start_stage_sound": 1, "jump": 1,
         "call_script": 1, "return": 0, "set_skip_target": 1, "set_skip_keep_sound": 1, "set_dirty_view": 1,
         "save_view": 0}
OPCODE_OF = {v: k for k, v in OPCODES.items()}
MESSAGE_OF = {v: k for k, v in MESSAGES.items()}
SLOT_OF = {v: k for k, v in SLOTS.items()}
SCENE, OTHER_CHILD, MANAGER = SLOT_OF["scene"], -1, -1
ANIMATION_PLAY = MESSAGE_OF["animation_play"]

# The payload a command's operand points to: by opcode and operand, and for a message by its id and operand.
_MESSAGE_PAYLOADS = {
    "scene_actor_play_animation": {3: "AnimationPlayRequest"}, "animation_play": {3: "AnimationPlayRequest"},
    "animation_install_and_play": {3: "AnimationPlayRequest"}, "scene_actor_set_model_draw": {4: "AnimationPlayRequest"},
    "actor_place": {3: "ActorTransform"}, "scene_actor_place": {3: "ActorTransform"},
    "scene_actor_walk_to": {3: "ActorTransform", 4: "ActorMotionWalkAnim"}, "actor_turn_to_yaw": {3: "ActorTransform"},
    "actor_move_to": {3: "ActorTransform", 4: "GameActorMoveAnim"}, "actor_run_to": {3: "ActorTransform", 4: "GameActorMoveAnim"},
    "command_apply": {3: "ActorCommand"}, "scene_broadcast_to_actors": {3: "ActorCommand"},
    "animation_copy_bank_extension": {3: "AnimationBankCopyRequest"},
}
MESSAGE_PAYLOADS = {MESSAGE_OF[m]: v for m, v in _MESSAGE_PAYLOADS.items() if m in MESSAGE_OF}
OPCODE_PAYLOADS = {OPCODE_OF["play_weapon_animation"]: {3: "AnimationPlayRequest"},
                   OPCODE_OF["select_scene"]: {0: "EvsSceneKey"},
                   OPCODE_OF["start_vibration"]: {0: "PadScriptCmd", 1: "PadScriptVibrationSegment"},
                   OPCODE_OF["jump"]: {0: "EvsCommand"}, OPCODE_OF["call_script"]: {0: "EvsCommand"},
                   OPCODE_OF["set_skip_target"]: {0: "EvsCommand"}}


# The message family a recipient's messages come from: written without its prefix where that names exactly one id.
FAMILY = {"player": "actor_", "companion": "actor_", "scene": "scene_actor_", "child": "scene_actor_",
          "cap_control": "cap_"}


def _message_text(target: str, mid: int) -> str:
    full = MESSAGES.get(mid)
    if not full:
        return f"message_0x{mid & 0xFFFFFFFF:X}"
    family = FAMILY.get(target.split("[")[0].split(".")[0], "")
    short = full[len(family):] if family and full.startswith(family) else full
    return short if short not in MESSAGE_OF else full     # a short name that is some other message's: in full


def _message_value(target: str, name: str) -> int:
    family = FAMILY.get(target, "")
    if family and family + name in MESSAGE_OF and name not in MESSAGE_OF:
        return MESSAGE_OF[family + name]
    if name in MESSAGE_OF:
        return MESSAGE_OF[name]
    return int(name[len("message_"):], 0)


def payload_targets(words: tuple[int, ...]) -> list[tuple[int, str]]:
    """(address, kind) of what one command's operands point to (an event script: its six words)."""
    opcode, ops = words[0], words[1:]
    table = MESSAGE_PAYLOADS.get(ops[2], {}) if opcode == OPCODE_OF["send_message"] else OPCODE_PAYLOADS.get(opcode, {})
    return [(ops[i] & 0xFFFFFFFF, kind) for i, kind in table.items() if ops[i]]


# A sound request: its bank type in bits 28..31, its bank in 16..27 (an area bank's stage in 8..11 and area in 0..7),
# its sound-script entry in 0..15 - written by bank where that is exact.
SOUND_OPERANDS = {"start_sound": 0, "fade_sound_attenuation": 0}
_SOUND_TYPES = {0: "common", 2: "weapon", 4: "character", 5: "area"}


def sound_text(v: int, n: "Names") -> str:
    u = v & 0xFFFFFFFF
    kind, bank, entry = u >> 28, (u >> 16) & 0xFFF, u & 0xFFFF
    if v == 0 or kind not in _SOUND_TYPES:
        return str(v)
    if kind == 5 and (bank >> 8, bank & 0xFF) == n.area and entry in n.sounds:
        return f"room_sound({n.sounds[entry]})"           # the room's own bank: its slot's name
    if kind == 5:
        return f"area_sound({bank >> 8},{bank & 0xFF},{entry})"
    if kind == 0:
        return f"common_sound({entry})" if bank == 0 else str(v)
    return f"{_SOUND_TYPES[kind]}_sound({bank},{entry})"


def sound_value(token: str, n: "Names") -> int | None:
    m = re.fullmatch(r"room_sound\((\w+)\)", token)
    if m:
        entry = {v: k for k, v in n.sounds.items()}[m.group(1)]
        return 5 << 28 | (n.area[0] << 8 | n.area[1]) << 16 | entry
    m = re.fullmatch(r"(common|weapon|character|area)_sound\(([\d,]+)\)", token)
    if not m:
        return None
    args = [int(x) for x in m.group(2).split(",")]
    kind = {v: k for k, v in _SOUND_TYPES.items()}[m.group(1)]
    bank = (args[0] << 8 | args[1]) if kind == 5 else args[0] if kind != 0 else 0
    entry = args[-1]
    u = kind << 28 | bank << 16 | entry
    return u - (1 << 32) if u >= 0x80000000 else u


# --- names -----------------------------------------------------------------------------------------------------

class Names:
    """A room's symbols as the text writes them, and back."""

    def __init__(self, prefix: str, local: set[str], area: tuple[int, int] = (0, 0), sounds: dict[int, str] | None = None):
        self.prefix, self.local = prefix, local          # local: the symbols of the room's event objects
        self.area, self.sounds = area, sounds or {}      # the room's (stage, area), its sound banks' slots by entry

    def write(self, v) -> str:
        if isinstance(v, int):
            return str(v)
        m = re.fullmatch(r"(\w+)(\[\d+\])?", v)
        sym, index = m.group(1), m.group(2) or ""
        if sym in self.local:
            for lead, short in ((f"_g{self.prefix}", "_"), (f"g{self.prefix}", "")):
                if sym.startswith(lead) and sym[len(lead):][:1].isupper():
                    return short + sym[len(lead):] + index
        return "&" + sym + index

    def read(self, token: str):
        if re.fullmatch(r"-?(0x[0-9A-Fa-f]+|\d+)", token):
            return int(token, 0)
        m = re.fullmatch(r"(&?)(\w+)(\[\d+\])?", token)
        if not m:
            raise ValueError(f"not a name or a number: {token}")
        amp, name, index = m.group(1), m.group(2), m.group(3) or ""
        if amp:
            return name + index
        sym = (f"_g{self.prefix}{name[1:]}" if name.startswith("_") else f"g{self.prefix}{name}")
        return sym + index


def prefix_of(package: str) -> str:
    """The room's symbol prefix: its package name in CamelCase (acropolis_patio: AcropolisPatio, mist_r18: MistR18)."""
    return "".join(w[:1].upper() + w[1:] for w in package.split("_"))


# --- writing ---------------------------------------------------------------------------------------------------

def _payload_text(kind: str, v: dict, n: Names) -> str:
    w = n.write
    if kind == "AnimationPlayRequest":
        return (f"clip {v['animationId']} bank {w(v['source'])} blend {v['blend']} frames {v['blendFrames']} "
                f"collision {v['enableWorldCollision']}")
    if kind == "ActorTransform":
        p, r = v["pos"], v["rot"]
        pad = f" pad ({p[3]}, {r[3]})" if p[3] or r[3] else ""
        return f"at ({p[0]}, {p[1]}, {p[2]}) turn ({r[0]}, {r[1]}, {r[2]}){pad}"
    if kind == "ActorCommand":
        c = v["context"]
        return f"stage {c['stage']} area {c['area']} command 0x{v['command']:X}"
    if kind == "AnimationBankCopyRequest":
        return f"{w(v['sets'])} count {v['count']}"
    if kind == "EvsSceneKey":
        return f"group {v['group']} stream {v['streamId']} sub {v['subId']}"
    if kind == "GameActorMoveAnim":
        return f"approach {v['approachAnimId']} arrival {v['arrivalAnimId']}"
    if kind == "ActorMotionWalkAnim":
        return f"clip {v['animationId']} next {v['nextAnimId']}"
    if kind == "AreaApplyRec":
        if (v["stage"], v["area"], v["variant"], v["policy"]) == (0xFF, 0, 0, 0):
            return "end"
        return f"area {v['stage']}/{v['area']} variant {v['variant']} policy 0x{v['policy']:02X}"
    raise ValueError(kind)


def _command_text(v: dict, n: Names) -> str:
    w = n.write
    opcode = v["opcode"]
    ops = [v[f"operand{i}"] for i in range(5)]
    op = OPCODES.get(opcode)
    if op == "send_message" and isinstance(ops[0], int) and isinstance(ops[1], int) and isinstance(ops[2], int):
        slot, index = ops[0], ops[1]
        if slot == SCENE:
            target = "scene.manager" if index == MANAGER else f"scene[{index}]"
        elif slot == OTHER_CHILD:
            target = f"child[{index}]"
        else:
            target = (SLOTS.get(slot) or f"slot_{slot}") + (f"[{index}]" if index else "")
        message = _message_text(target, ops[2] & 0xFFFFFFFF if ops[2] < 0 else ops[2])
        args = [ops[3], ops[4]]
        while args and args[-1] == 0:
            args.pop()
        return f"{target}.{message}" + "".join(" " + w(a) for a in args)
    if op == "play_weapon_animation" and ops[1] == 0 and ops[2] == ANIMATION_PLAY and isinstance(ops[0], int) \
            and ops[0] in SLOTS:
        return f"{SLOTS[ops[0]]}.weapon_animation {w(ops[3])}" + (f" {w(ops[4])}" if ops[4] != 0 else "")
    if op in ARITY and all(o == 0 for o in ops[ARITY[op]:]):
        args = [w(a) for a in ops[:ARITY[op]]]
        if op in SOUND_OPERANDS and isinstance(ops[SOUND_OPERANDS[op]], int):
            args[SOUND_OPERANDS[op]] = sound_text(ops[SOUND_OPERANDS[op]], n)
        return op + "".join(" " + a for a in args)
    return f"raw {opcode} " + " ".join(w(a) for a in ops)


def write(objects: dict[str, dict], n: Names, order: list[str]) -> str:
    """The events text: each object in `order` (address order), declarations and scripts."""
    lines: list[str] = []
    previous = None
    for sym in order:
        rec = objects[sym]
        kind = rec["kind"]
        name = n.write(sym)
        if kind != "EvsCommand" and previous in (None, "EvsCommand"):     # a scene: its payloads, then its scripts
            lines += ["", f"# scene {1 + sum(1 for l in lines if l.startswith('# scene '))}"]
        previous = kind
        if kind == "EvsCommand":
            lines += ["", f"script {name}"] + [f"    {_command_text(c, n)}" for c in rec["values"]]
        elif "value" in rec:
            lines.append(f"{KEYWORDS[kind]} {name} = {_payload_text(kind, rec['value'], n)}")
        else:
            lines += [f"{KEYWORDS[kind]} {name}[]"] + [f"    {_payload_text(kind, v, n)}" for v in rec["values"]]
        if kind == "EvsCommand":
            lines.append("")
    return "\n".join(re.sub(r"\n{3,}", "\n\n", "\n".join(lines)).strip("\n").split("\n")) + "\n"


# --- reading ---------------------------------------------------------------------------------------------------

def _payload_value(kind: str, text: str, n: Names) -> dict:
    r = n.read
    t = text.replace("(", " ( ").replace(")", " ) ").replace(",", " ").split()

    def after(word: str, k: int = 1):
        i = t.index(word)
        return t[i + 1:i + 1 + k] if k > 1 else t[i + 1]

    def tuple_after(word: str) -> list[int]:
        i = t.index(word)
        j = t.index(")", i)
        return [int(x, 0) for x in t[i + 2:j]]
    if kind == "AnimationPlayRequest":
        return {"source": r(after("bank")), "animationId": int(after("clip"), 0), "blend": int(after("blend"), 0),
                "blendFrames": int(after("frames"), 0), "enableWorldCollision": int(after("collision"), 0)}
    if kind == "ActorTransform":
        pad = tuple_after("pad") if "pad" in t else [0, 0]
        return {"pos": tuple_after("at") + [pad[0]], "rot": tuple_after("turn") + [pad[1]]}
    if kind == "ActorCommand":
        return {"context": {"stage": int(after("stage"), 0), "area": int(after("area"), 0)}, "command": int(after("command"), 0)}
    if kind == "AnimationBankCopyRequest":
        return {"sets": r(t[0]), "count": int(after("count"), 0)}
    if kind == "EvsSceneKey":
        return {"group": int(after("group"), 0), "streamId": int(after("stream"), 0), "subId": int(after("sub"), 0)}
    if kind == "GameActorMoveAnim":
        return {"approachAnimId": int(after("approach"), 0), "arrivalAnimId": int(after("arrival"), 0)}
    if kind == "ActorMotionWalkAnim":
        return {"animationId": int(after("clip"), 0), "nextAnimId": int(after("next"), 0)}
    if kind == "AreaApplyRec":
        if t == ["end"]:
            return {"stage": 0xFF, "area": 0, "variant": 0, "policy": 0}
        stage, area = (int(x, 0) for x in after("area").split("/"))
        return {"stage": stage, "area": area, "variant": int(after("variant"), 0), "policy": int(after("policy"), 0)}
    raise ValueError(kind)


def _command_value(text: str, n: Names) -> dict:
    t = text.split()
    r = n.read
    if t[0] == "raw":
        ops = [r(x) for x in t[2:7]]
        return {"opcode": int(t[1], 0), **{f"operand{i}": ops[i] for i in range(5)}}
    m = re.fullmatch(r"(\w+)(?:\[(-?\d+)\]|\.(manager))?\.(\w+)", t[0])
    if m:
        target, index, manager, message = m.groups()
        if message == "weapon_animation":
            ops = [SLOT_OF[target], 0, ANIMATION_PLAY, r(t[1]), r(t[2]) if len(t) > 2 else 0]
            return {"opcode": OPCODE_OF["play_weapon_animation"], **{f"operand{i}": ops[i] for i in range(5)}}
        slot = OTHER_CHILD if target == "child" else SLOT_OF.get(target) if target in SLOT_OF else int(target[5:])
        idx = MANAGER if manager else int(index) if index else 0
        mid = _message_value(target, message)
        mid = mid - (1 << 32) if mid >= 0x80000000 else mid
        args = [r(x) for x in t[1:3]] + [0] * (2 - len(t[1:3]))
        ops = [slot, idx, mid, *args]
        return {"opcode": OPCODE_OF["send_message"], **{f"operand{i}": ops[i] for i in range(5)}}
    ops = [sound_value(x, n) if sound_value(x, n) is not None else r(x) for x in t[1:]] + [0] * (5 - len(t[1:]))
    return {"opcode": OPCODE_OF[t[0]], **{f"operand{i}": ops[i] for i in range(5)}}


def read(text: str, n: Names) -> dict[str, dict]:
    """{symbol: {kind, value or values}} of the events text."""
    out: dict[str, dict] = {}
    current = None
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line.startswith("    "):
            rec = out[current]
            body = line.strip()
            rec["values"].append(_command_value(body, n) if rec["kind"] == "EvsCommand" else _payload_value(rec["kind"], body, n))
            continue
        m = re.fullmatch(r"(\w+) (&?\w+)(\[\])?(?: = (.*))?", line.strip())
        if not m:
            raise ValueError(f"cannot read: {line}")
        keyword, name, array, body = m.groups()
        kind = KINDS[keyword]
        sym = n.read(name)
        if kind == "EvsCommand" or array:
            out[sym] = {"kind": kind, "values": []}
            current = sym
        else:
            out[sym] = {"kind": kind, "value": _payload_value(kind, body, n)}
            current = None
    return out
