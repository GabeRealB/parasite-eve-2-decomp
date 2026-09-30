#ifndef GAMEPLAY_PAD_SCRIPT_H
#define GAMEPLAY_PAD_SCRIPT_H

#include "common.h"

#include "main/task_types.h"

// Scripted controller-vibration commands and their task entry points.

/// Opcodes packed into the low byte of a `PadScriptCmd` lane.
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

/// Pack a lane opcode and its operand into one command word.
///
/// The opcode occupies the low byte and the operand the high byte. Each
/// argument is truncated to eight bits and evaluated once. The operand is a
/// record index for `PAD_SCRIPT_PLAY`, a frame count for `PAD_SCRIPT_WAIT`,
/// a loop count for `PAD_SCRIPT_LOOP` and a step index for `PAD_SCRIPT_JUMP`.
#define PAD_SCRIPT_COMMAND(opcode, operand) ((u16)(((opcode) & 0xFF) | (((operand) & 0xFF) << 8)))

/// One step of a two-lane controller-vibration script.
///
/// The lanes share this encoding and walk the same array, each with its own
/// program counter, delay and loop counter. `holdCommand` refreshes the
/// controller's on/off motor; `lerpCommand` refreshes its variable-intensity
/// motor. Playing a record posts that motor for the record's duration and
/// waits out that duration before the next command.
typedef struct PadScriptCmd {
    u16 holdCommand; // On/off motor lane, packed with `PAD_SCRIPT_COMMAND`
    u16 lerpCommand; // Variable-intensity motor lane, packed the same way
} PadScriptCmd;
STATIC_ASSERT_SIZEOF(PadScriptCmd, 4);

/// 4-byte pad record. `GpState18::field_4` is an array of these, indexed by
/// the operand of a `PAD_SCRIPT_PLAY` command. `field_2` is the delay copied to
/// `field_10` / `field_11`; `field_0` / `field_1` are the variable-intensity
/// lane's start and end.
typedef struct _GpScriptRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2; // delay
    /* 0x3 */ u8 field_3;
} GpScriptRec;
STATIC_ASSERT_SIZEOF(GpScriptRec, 4);

/// Serialized EVS operands and native pointers share the PS1 address word.
typedef union GpScriptCmdAddress {
    s32           address;
    PadScriptCmd* commands;
    void*         storage;
} GpScriptCmdAddress __attribute__((transparent_union));

typedef union GpScriptRecAddress {
    s32          address;
    GpScriptRec* records;
    void*        storage;
} GpScriptRecAddress __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(GpScriptCmdAddress, 4);
STATIC_ASSERT_SIZEOF(GpScriptRecAddress, 4);

/// Suspends pad-driven scripting: raises the script, hold and lerp halt
/// flags, clears `GameSession::padScriptFlags` and flushes the pad event queue.
void Gp_HaltPadScripts(void);

Task* Gp_SpawnScript18(GpScriptCmdAddress arg0, GpScriptRecAddress arg1);

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

Task* Gp_SpawnScript18Ex(GpScriptCmdAddress arg0, GpScriptRecAddress arg1, s32 arg2);

void Gp_PadHoldTask(Task* task);

void Gp_PadLerpTask(Task* task);

void Gp_Script18Task(Task* arg0);

#endif // GAMEPLAY_PAD_SCRIPT_H
