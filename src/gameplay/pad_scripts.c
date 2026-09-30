#include "gameplay/pad_script.h"

#include "common.h"

#include "pad_script.h"
#include "scene.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"

/// 0xC-byte interpolator state allocated by `Gp_SpawnPadLerp` / `Gp_SpawnPadLerpScaled`
/// (`memCalloc(0xC, 0)`) and stored at `Task::work` for bank-2 type 0xC.
/// `field_8` is the duration; `field_4.as_s32` is start<<8; `field_0` is the
/// per-frame step `((end<<8) - (start<<8)) / duration`.
/// `Gp_PadLerpTask` posts `field_4.bytes.as_u8` (the 8-bit interpolator,
/// `as_s32 >> 8` on little-endian) via `Pad_PostEvent`.
typedef struct _GpState0C {
    /* 0x0 */ s32 field_0; // step
    /* 0x4 */ union {
        s32 as_s32;        // start << 8
        struct {
            /* 0x4 */ u8 pad_4;
            /* 0x5 */ u8 as_u8; // (as_s32 >> 8)
        } bytes;
    } field_4;
    /* 0x8 */ s16  field_8; // duration
    /* 0xA */ byte pad_A[2];
} GpState0C;
STATIC_ASSERT_SIZEOF(GpState0C, 0xC);

u8 Gp_PadScriptHalt;

u8 Gp_PadHoldHalt;

u8 Gp_PadLerpHalt;

static const TaskFuncTable3 Gp_Script18States;

static const TaskFuncTable5 Gp_ScriptAStates;

static const TaskFuncTable5 Gp_ScriptBStates;

static void Gp_StepScriptA(Task* task);

static void Gp_StepScriptB(Task* task);

static void Gp_SpawnPadHold(s16 arg0);

static void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3);

static void Gp_KickScriptAB(Task* task);

static void Gp_DispatchScript18(Task* task);

static void Gp_ScriptAState0(Task* task);

static void Gp_TickScriptADelay(Task* task);

static void Gp_ScriptAState3(Task* task);

static void Gp_ScriptAState4(Task* task);

static void Gp_ScriptBState0(Task* task);

static void Gp_TickScriptBDelay(Task* task);

static void Gp_ScriptBState3(Task* task);

static void Gp_ScriptBState4(Task* task);

static const TaskFuncTable3 Gp_Script18States;
static const TaskFuncTable5 Gp_ScriptAStates;
static const TaskFuncTable5 Gp_ScriptBStates;

static void Gp_StepScriptA(Task* task)
{
    GpState18*    state;
    PadScriptCmd* table;
    GpScriptRec*  recs;
    u16           cmd;
    s32           opcode;
    u8            tmp;

    state  = (GpState18*)task->work;
    table  = state->field_0;
    recs   = state->field_4;
    cmd    = table[state->field_E].holdCommand;
    opcode = cmd & 0xFF;
    // The saved word's opcode is this lane's state until the next step.
    state->field_A.command = cmd;

    if (opcode != PAD_SCRIPT_STOP) {
        if (opcode == PAD_SCRIPT_PLAY) {
            state->field_12 = cmd >> 8;
            state->field_10 = recs[state->field_12].field_2;
            Gp_SpawnPadHold(state->field_10);
            state->field_E++;
        } else if (opcode == PAD_SCRIPT_WAIT) {
            state->field_10 = cmd >> 8;
            state->field_E++;
        } else if (opcode == PAD_SCRIPT_LOOP) {
            tmp = state->field_14;
            if (tmp == 0) {
                tmp             = cmd >> 8;
                state->field_14 = tmp;
                state->field_E++;
            } else {
                tmp--;
                state->field_14 = tmp;
                state->field_E++;
            }
        } else if (opcode == PAD_SCRIPT_JUMP) {
            if (state->field_14 == 0) {
                state->field_E++;
            } else {
                state->field_E = table[state->field_E].holdCommand >> 8;
            }
            Gp_StepScriptA(task);
        }
    }
}

static void Gp_StepScriptB(Task* task)
{
    GpState18*    state;
    PadScriptCmd* table;
    GpScriptRec*  recs;
    u16           cmd;
    s32           opcode;
    u8            tmp;

    state  = (GpState18*)task->work;
    table  = state->field_0;
    recs   = state->field_4;
    cmd    = table[state->field_F].lerpCommand;
    opcode = cmd & 0xFF;
    // The saved word's opcode is this lane's state until the next step.
    state->field_C.command = cmd;

    if (opcode != PAD_SCRIPT_STOP) {
        if (opcode == PAD_SCRIPT_PLAY) {
            state->field_13 = cmd >> 8;
            state->field_11 = recs[state->field_13].field_2;
            Gp_SpawnPadLerpScaled(state->field_11, recs[state->field_13].field_0, recs[state->field_13].field_1, state->field_8);
            state->field_F++;
        } else if (opcode == PAD_SCRIPT_WAIT) {
            state->field_11 = cmd >> 8;
            state->field_F++;
        } else if (opcode == PAD_SCRIPT_LOOP) {
            tmp = state->field_15;
            if (tmp == 0) {
                tmp             = cmd >> 8;
                state->field_15 = tmp;
                state->field_F++;
            } else {
                tmp--;
                state->field_15 = tmp;
                state->field_F++;
            }
        } else if (opcode == PAD_SCRIPT_JUMP) {
            if (state->field_15 == 0) {
                state->field_F++;
            } else {
                state->field_F = table[state->field_F].lerpCommand >> 8;
            }
            Gp_StepScriptB(task);
        }
    }
}

static void Gp_SpawnPadHold(s16 arg0)
{
    if (arg0 != 0) {
        Task_Spawn(2, 0xB, (s32)(arg0), 0);
    }
}

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2)
{
    Task*      task;
    GpState0C* mem;
    s32        start;
    s32        end;

    if (arg0 != 0) {
        mem = memCalloc(0xC, 0);
        if (mem != NULL) {
            task = Task_Spawn(2, 0xC, 0, 0);
            if (task == NULL) {
                memFree(mem);
            } else {
                end                 = (arg2 & 0xFF) << 8;
                start               = (arg1 & 0xFF) << 8;
                task->work          = mem;
                mem->field_8        = arg0;
                mem->field_4.as_s32 = start;
                mem->field_0        = (end - start) / arg0;
            }
        }
    }
}

static void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3)
{
    Task*      task;
    GpState0C* mem;
    s32        start;
    s32        end;
    s16        scale;
    s32        temp;

    if (arg0 != 0) {
        mem = memCalloc(0xC, 0);
        if (mem != NULL) {
            task = Task_Spawn(2, 0xC, 0, 0);
            if (task == NULL) {
                memFree(mem);
            } else {
                task->work = mem;
                temp       = arg3 >> 3;
                if (temp == 0) {
                    scale = 1;
                } else {
                    scale = temp;
                }
                end                 = (arg2 & 0xFF) / scale;
                start               = (arg1 & 0xFF) / scale;
                end               <<= 8;
                start             <<= 8;
                mem->field_8        = arg0;
                mem->field_4.as_s32 = start;
                mem->field_0        = (end - start) / arg0;
            }
        }
    }
}

void Gp_HaltPadScripts(void)
{
    Gp_PadScriptHalt             = 1;
    Gp_PadHoldHalt               = 1;
    Gp_PadLerpHalt               = 1;
    gGameSession->padScriptFlags = 0;
    Pad_ClearEvents(0);
}

Task* Gp_SpawnScript18(GpScriptCmdAddress arg0, GpScriptRecAddress arg1)
{
    Task*      task;
    GpState18* mem;

    mem = memCalloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->work   = mem;
            mem->field_8 = 0;
            mem->field_0 = arg0.commands;
            mem->field_4 = arg1.records;
            return task;
        }
        memFree(mem);
    }
    return NULL;
}

static void Gp_KickScriptAB(Task* task)
{
    Gp_StepScriptA(task);
    Gp_StepScriptB(task);
    task->state++;
}

static void Gp_DispatchScript18(Task* task)
{
    TaskFuncTable5 tableA;
    TaskFuncTable5 tableB;
    GpState18*     state;

    state  = (GpState18*)task->work;
    tableA = Gp_ScriptAStates;
    tableB = Gp_ScriptBStates;
    tableA.funcs[state->field_A.bytes.opcode](task);
    tableB.funcs[state->field_C.bytes.opcode](task);
    if (state->field_A.bytes.opcode == 0 && state->field_C.bytes.opcode == 0) {
        task->state++;
    }
}

void Gp_ClearPadHalt(void)
{
    Gp_PadScriptHalt = 0;
    Gp_PadHoldHalt   = 0;
    Gp_PadLerpHalt   = 0;
}

Task* Gp_SpawnScript18Ex(GpScriptCmdAddress arg0, GpScriptRecAddress arg1, s32 arg2)
{
    Task*      task;
    GpState18* mem;

    mem = memCalloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->work   = mem;
            mem->field_8 = arg2;
            mem->field_0 = arg0.commands;
            mem->field_4 = arg1.records;
            return task;
        }
        memFree(mem);
    }
    return NULL;
}

void Gp_Script18Task(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_Script18States;
    if (Gp_StateF0.field_4 == 0 || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
        if (Gp_PadScriptHalt != 0) {
            arg0->state = 2;
        }
        sp.funcs[arg0->state](arg0);
    }
}

static void Gp_ScriptAState0(Task* task)
{
}

static void Gp_TickScriptADelay(Task* task)
{
    GpState18* state;

    state = (GpState18*)task->work;
    if (--state->field_10 == 0) {
        Gp_StepScriptA(task);
    }
}

static void Gp_ScriptAState3(Task* task)
{
    Gp_StepScriptA(task);
}

static void Gp_ScriptAState4(Task* task)
{
    Gp_StepScriptA(task);
}

static void Gp_ScriptBState0(Task* task)
{
}

static void Gp_TickScriptBDelay(Task* task)
{
    GpState18* state;

    state = (GpState18*)task->work;
    if (--state->field_11 == 0) {
        Gp_StepScriptB(task);
    }
}

static void Gp_ScriptBState3(Task* task)
{
    Gp_StepScriptB(task);
}

static void Gp_ScriptBState4(Task* task)
{
    Gp_StepScriptB(task);
}

void Gp_PadHoldTask(Task* task)
{
    if (Gp_StateF0.field_4 == 0 || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
        if (task->spawnArg1.value != 0 && Gp_PadHoldHalt == 0) {
            task->spawnArg1.value--;
            Pad_PostEvent(0, 0, 1, 1);
            gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE;
        } else {
            gGameSession->padScriptFlags &= ~GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE;
            taskKill(task);
        }
    }
}

void Gp_PadLerpTask(Task* task)
{
    GpState0C* state;

    state = (GpState0C*)task->work;
    if (Gp_StateF0.field_4 == 0 || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
        if (state->field_8 != 0 && Gp_PadLerpHalt == 0) {
            state->field_8--;
            Pad_PostEvent(0, 1, state->field_4.bytes.as_u8, 1);
            state->field_4.as_s32        += state->field_0;
            gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE;
        } else {
            gGameSession->padScriptFlags &= ~GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE;
            taskKill(task);
        }
    }
}

static const TaskFuncTable3 Gp_Script18States = { {
    Gp_KickScriptAB,
    Gp_DispatchScript18,
    taskKill,
} };

// Indexed by the lane opcode: stop, play, wait, loop, jump.
static const TaskFuncTable5 Gp_ScriptAStates = { {
    Gp_ScriptAState0,
    Gp_TickScriptADelay,
    Gp_TickScriptADelay,
    Gp_ScriptAState3,
    Gp_ScriptAState4,
} };

// Indexed by the lane opcode: stop, play, wait, loop, jump.
static const TaskFuncTable5 Gp_ScriptBStates = { {
    Gp_ScriptBState0,
    Gp_TickScriptBDelay,
    Gp_TickScriptBDelay,
    Gp_ScriptBState3,
    Gp_ScriptBState4,
} };
