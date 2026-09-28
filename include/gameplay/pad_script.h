#ifndef GAMEPLAY_PAD_SCRIPT_H
#define GAMEPLAY_PAD_SCRIPT_H

#include "common.h"

#include "main/task.h"

// Scripted pad commands and their task entry points.

/// 4-byte dual-script command. `GpState18::field_0` is an array of these.
/// Script A reads `field_0`, script B reads `field_2`. Low byte is the opcode
/// (0 = stop, 1 = timed pad from `field_4`, 2 = set delay, 3 = set/decrement
/// loop, 4 = loop jump); high byte is the payload.
typedef struct _GpScriptCmd {
    /* 0x0 */ u16 field_0; // script A command
    /* 0x2 */ u16 field_2; // script B command
} GpScriptCmd;
STATIC_ASSERT_SIZEOF(GpScriptCmd, 4);

/// 4-byte pad record. `GpState18::field_4` is an array of these, indexed by
/// the high byte of an opcode-1 command. `field_2` is the delay copied to
/// `field_10` / `field_11`; `field_0` / `field_1` are start/end for script B.
typedef struct _GpScriptRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2; // delay
    /* 0x3 */ u8 field_3;
} GpScriptRec;
STATIC_ASSERT_SIZEOF(GpScriptRec, 4);

/// Serialized EVS operands and native pointers share the PS1 address word.
typedef union GpScriptCmdAddress {
    s32          address;
    GpScriptCmd* commands;
    void*        storage;
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
