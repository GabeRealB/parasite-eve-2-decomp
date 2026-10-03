#include "gameplay/pad_script.h"

#include "common.h"

#include "pad_script.h"
#include "gameplay/scene_combat.h"

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

/// Work block of one controller-vibration script task, owned through `Task::work`.
///
/// A script runs two lanes over one borrowed step array: the hold lane drives
/// the controller's on/off motor and the lerp lane its variable-intensity
/// motor. Each lane keeps its own step index, wait, segment index and loop
/// counter, and saves the command word it last loaded; that word's opcode
/// selects what the lane does on each script frame, and the script ends once
/// both lanes hold `PAD_SCRIPT_STOP`. The block is allocated zeroed, so both
/// lanes start at step 0 with no loop pending.
typedef struct {
    PadScriptCmd*              commands;       // Borrowed step array both lanes walk
    PadScriptVibrationSegment* segments;       // Borrowed segment table indexed by `PAD_SCRIPT_PLAY` operands
    s16                        sourceDepth;    // Signed view depth of the vibration's source; an eighth of it, or 1 when that is 0, divides the lerp lane's intensities
    PadScriptLane              holdCommand;    // Command word the hold lane last loaded
    PadScriptLane              lerpCommand;    // Command word the lerp lane last loaded
    u8                         holdStep;       // Index in `commands` of the hold lane's next step
    u8                         lerpStep;       // Index in `commands` of the lerp lane's next step
    u8                         holdWaitFrames; // Script frames before the hold lane steps again (a stored 0 waits 256)
    u8                         lerpWaitFrames; // Script frames before the lerp lane steps again (a stored 0 waits 256)
    u8                         holdSegment;    // Index in `segments` of the hold lane's last played segment
    u8                         lerpSegment;    // Index in `segments` of the lerp lane's last played segment
    u8                         holdLoopCount;  // Hold lane's loop counter: loaded when 0, else decremented, by `PAD_SCRIPT_LOOP`; `PAD_SCRIPT_JUMP` branches while it is nonzero
    u8                         lerpLoopCount;  // Lerp lane's loop counter, used the same way
} _PadScriptWork;
STATIC_ASSERT_SIZEOF(_PadScriptWork, 0x18);

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
    _PadScriptWork*            state;
    PadScriptCmd*              table;
    PadScriptVibrationSegment* segments;
    u16                        cmd;
    s32                        opcode;
    u8                         tmp;

    state    = task->work;
    table    = state->commands;
    segments = state->segments;
    cmd      = table[state->holdStep].holdCommand.command;
    opcode   = cmd & 0xFF;
    // The saved word's opcode is this lane's state until the next step.
    state->holdCommand.command = cmd;

    if (opcode != PAD_SCRIPT_STOP) {
        if (opcode == PAD_SCRIPT_PLAY) {
            state->holdSegment    = cmd >> 8;
            state->holdWaitFrames = segments[state->holdSegment].durationFrames;
            Gp_SpawnPadHold(state->holdWaitFrames);
            state->holdStep++;
        } else if (opcode == PAD_SCRIPT_WAIT) {
            state->holdWaitFrames = cmd >> 8;
            state->holdStep++;
        } else if (opcode == PAD_SCRIPT_LOOP) {
            tmp = state->holdLoopCount;
            if (tmp == 0) {
                tmp                  = cmd >> 8;
                state->holdLoopCount = tmp;
                state->holdStep++;
            } else {
                tmp--;
                state->holdLoopCount = tmp;
                state->holdStep++;
            }
        } else if (opcode == PAD_SCRIPT_JUMP) {
            if (state->holdLoopCount == 0) {
                state->holdStep++;
            } else {
                state->holdStep = table[state->holdStep].holdCommand.command >> 8;
            }
            Gp_StepScriptA(task);
        }
    }
}

static void Gp_StepScriptB(Task* task)
{
    _PadScriptWork*            state;
    PadScriptCmd*              table;
    PadScriptVibrationSegment* segments;
    u16                        cmd;
    s32                        opcode;
    u8                         tmp;

    state    = task->work;
    table    = state->commands;
    segments = state->segments;
    cmd      = table[state->lerpStep].lerpCommand.command;
    opcode   = cmd & 0xFF;
    // The saved word's opcode is this lane's state until the next step.
    state->lerpCommand.command = cmd;

    if (opcode != PAD_SCRIPT_STOP) {
        if (opcode == PAD_SCRIPT_PLAY) {
            state->lerpSegment    = cmd >> 8;
            state->lerpWaitFrames = segments[state->lerpSegment].durationFrames;
            Gp_SpawnPadLerpScaled(state->lerpWaitFrames, segments[state->lerpSegment].startIntensity, segments[state->lerpSegment].endIntensity, state->sourceDepth);
            state->lerpStep++;
        } else if (opcode == PAD_SCRIPT_WAIT) {
            state->lerpWaitFrames = cmd >> 8;
            state->lerpStep++;
        } else if (opcode == PAD_SCRIPT_LOOP) {
            tmp = state->lerpLoopCount;
            if (tmp == 0) {
                tmp                  = cmd >> 8;
                state->lerpLoopCount = tmp;
                state->lerpStep++;
            } else {
                tmp--;
                state->lerpLoopCount = tmp;
                state->lerpStep++;
            }
        } else if (opcode == PAD_SCRIPT_JUMP) {
            if (state->lerpLoopCount == 0) {
                state->lerpStep++;
            } else {
                state->lerpStep = table[state->lerpStep].lerpCommand.command >> 8;
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
    Task*           task;
    _PadScriptWork* mem;

    mem = memCalloc(sizeof(*mem), 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->work       = mem;
            mem->sourceDepth = 0;
            mem->commands    = arg0.commands;
            mem->segments    = arg1.records;
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
    TaskFuncTable5  tableA;
    TaskFuncTable5  tableB;
    _PadScriptWork* state;

    state  = task->work;
    tableA = Gp_ScriptAStates;
    tableB = Gp_ScriptBStates;
    tableA.funcs[state->holdCommand.bytes.opcode](task);
    tableB.funcs[state->lerpCommand.bytes.opcode](task);
    if (state->holdCommand.bytes.opcode == PAD_SCRIPT_STOP && state->lerpCommand.bytes.opcode == PAD_SCRIPT_STOP) {
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
    Task*           task;
    _PadScriptWork* mem;

    mem = memCalloc(sizeof(*mem), 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->work       = mem;
            mem->sourceDepth = arg2;
            mem->commands    = arg0.commands;
            mem->segments    = arg1.records;
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
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
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
    _PadScriptWork* state;

    state = task->work;
    if (--state->holdWaitFrames == 0) {
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
    _PadScriptWork* state;

    state = task->work;
    if (--state->lerpWaitFrames == 0) {
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
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
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
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
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
