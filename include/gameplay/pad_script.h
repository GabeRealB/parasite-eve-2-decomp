#ifndef GAMEPLAY_PAD_SCRIPT_H
#define GAMEPLAY_PAD_SCRIPT_H

#include "common.h"

#include "main/task_types.h"

// Scripted controller-vibration commands and their task entry points.

/// Opcodes packed into the low byte of a `PadScriptLane`.
///
/// A lane saves its command word, and this opcode selects the lane's next
/// task state, so the values are that state's index. A lane at
/// `PAD_SCRIPT_STOP` is idle. The script ends when both lanes are idle.
enum {
    PAD_SCRIPT_STOP = 0, // Idle. A nonzero operand is stored and ignored.
    PAD_SCRIPT_PLAY = 1, // Play vibration record `operand`, then wait out its duration.
    PAD_SCRIPT_WAIT = 2, // Wait `operand` frames without posting vibration.
    PAD_SCRIPT_LOOP = 3, // Load `operand` into the loop counter when it is 0; otherwise decrement it.
    PAD_SCRIPT_JUMP = 4, // If the counter is nonzero, go to step `operand`; otherwise advance. Step again immediately.
};

/// One packed lane word of a controller-vibration script.
///
/// `command` is the halfword `PAD_SCRIPT_COMMAND` writes. The opcode occupies
/// its low byte and the operand its high byte; `bytes` is that same storage.
/// A `PadScriptCmd` holds one word for the on/off motor and one for the
/// variable-intensity motor. A lane saves the word it loads, and `opcode`
/// is the index of the lane's task state until the next step.
typedef union {
    u16 command;    // Packed opcode and operand
    struct {
        u8 opcode;  // Low byte (`PAD_SCRIPT_STOP` through `PAD_SCRIPT_JUMP`)
        u8 operand; // High byte: record, frames, loop count or step; stop ignores it
    } bytes;        // Byte view of `command`
} PadScriptLane;
STATIC_ASSERT_SIZEOF(PadScriptLane, 2);

/// Pack a lane opcode and its operand into one `PadScriptLane` command word.
///
/// The opcode occupies the low byte and the operand the high byte. Each
/// argument is truncated to eight bits and evaluated once. The result
/// initializes `PadScriptLane.command`. The operand is a record index for
/// `PAD_SCRIPT_PLAY`, a frame count for `PAD_SCRIPT_WAIT`, a loop count for
/// `PAD_SCRIPT_LOOP` and a step index for `PAD_SCRIPT_JUMP`.
#define PAD_SCRIPT_COMMAND(opcode, operand) ((u16)(((opcode) & 0xFF) | (((operand) & 0xFF) << 8)))

/// One step of a two-lane controller-vibration script.
///
/// The lanes share this encoding and walk the same array, each with its own
/// program counter, delay and loop counter. `holdCommand` refreshes the
/// controller's on/off motor; `lerpCommand` refreshes its variable-intensity
/// motor. Playing a record posts that motor for the record's duration and
/// waits out that duration before the next command.
typedef struct PadScriptCmd {
    PadScriptLane holdCommand; // On/off motor lane, packed with `PAD_SCRIPT_COMMAND`
    PadScriptLane lerpCommand; // Variable-intensity motor lane, packed the same way
} PadScriptCmd;
STATIC_ASSERT_SIZEOF(PadScriptCmd, 4);

/// One timed vibration segment selected by a `PAD_SCRIPT_PLAY` operand.
///
/// The operand is a zero-based table index and must address a live entry.
/// Both lanes wait for the duration; the on/off motor ignores the intensity
/// endpoints, while the variable motor ramps toward the target in Q8 steps
/// after script attenuation. Tables are borrowed for the script task's lifetime.
/// A zero duration posts no vibration and wraps the byte-sized lane wait to
/// 256 script frames. Stored tables use durations from 1 through 255.
typedef struct PadScriptVibrationSegment {
    u8 startIntensity; // Initial variable-motor intensity (0..255), before attenuation
    u8 endIntensity;   // Target variable-motor intensity (0..255), before attenuation
    u8 durationFrames; // Playback and lane wait in script frames (normally 1..255)
    u8 field_3;        // Stored as 0 or 1; role unproven, with no interpreter read
} PadScriptVibrationSegment;
STATIC_ASSERT_SIZEOF(PadScriptVibrationSegment, 4);

/// Suspends pad-driven scripting: raises the script, hold and lerp halt
/// flags, clears `GameSession::padScriptFlags` and clears port 0's vibration requests.
void Gp_HaltPadScripts(void);

Task* Gp_SpawnScript18(PadScriptCmd* commands, PadScriptVibrationSegment* segments);

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

Task* Gp_SpawnScript18Ex(PadScriptCmd* commands, PadScriptVibrationSegment* segments, s32 arg2);

void Gp_PadHoldTask(Task* task);

void Gp_PadLerpTask(Task* task);

void Gp_Script18Task(Task* arg0);

#endif // GAMEPLAY_PAD_SCRIPT_H
