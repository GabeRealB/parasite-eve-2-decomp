#include "gameplay/pad_script.h"

#include "common.h"

#include "pad_script.h"
#include "gameplay/scene_combat.h"

#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"

/// Duration units of each per-frame motor refresh; one unit spans two serviced controller polls.
enum { PAD_SCRIPT_MOTOR_REFRESH_DURATION_UNITS = 1 };

/// Fraction bits of the Q8 intensity a `_PadScriptLerpWork` ramps.
///
/// The ramp posts the byte above these bits, so the value is also the bit
/// offset of `_PadScriptLerpWork::intensity.bytes.whole`.
#define PAD_SCRIPT_LERP_FRACTION_BITS 8

/// Work block of one variable-intensity vibration ramp, owned through `Task::work`.
///
/// The ramp drives the controller's variable-intensity motor from a start
/// intensity toward an end intensity over a number of frames. On each frame
/// its task runs it posts the whole part of `intensity` for one duration unit,
/// then adds `intensityStep`; it ends when `framesRemaining` reaches 0 or the
/// lerp halt flag is raised. The step is the Q8 span divided by the frame
/// count and truncated toward zero. The first post is the start intensity and
/// each later one a step further, so the Q8 value stops at least one step
/// short of the end intensity. The block is allocated zeroed and released by
/// the task's default teardown.
typedef struct {
    s32 intensityStep;   // Signed Q8 intensity added after each posted frame
    union {
        s32 q8;          // Current intensity in Q8, starting at the start intensity
        struct {
            u8 fraction; // Low byte: the Q8 fraction
            u8 whole;    // Second byte: the intensity posted to the motor
        } bytes;         // Little-endian byte view of `q8`
    } intensity;
    s16 framesRemaining; // Frames left to post; nonzero at creation, where it divides the span
} _PadScriptLerpWork;
STATIC_ASSERT_SIZEOF(_PadScriptLerpWork, 0xC);

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

static void _padScriptBinaryLaneStoppedState(Task* task);

static void Gp_TickScriptADelay(Task* task);

static void Gp_ScriptAState3(Task* task);

static void Gp_ScriptAState4(Task* task);

static void _padScriptVariableLaneStoppedState(Task* task);

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
        taskSpawn(2, 0xB, (s32)(arg0), 0);
    }
}

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2)
{
    Task*               task;
    _PadScriptLerpWork* work;
    s32                 start;
    s32                 end;

    if (arg0 != 0) {
        work = memCalloc(sizeof(*work), 0);
        if (work != NULL) {
            task = taskSpawn(2, 0xC, 0, 0);
            if (task == NULL) {
                memFree(work);
            } else {
                end                   = (arg2 & 0xFF) << PAD_SCRIPT_LERP_FRACTION_BITS;
                start                 = (arg1 & 0xFF) << PAD_SCRIPT_LERP_FRACTION_BITS;
                task->work            = work;
                work->framesRemaining = arg0;
                work->intensity.q8    = start;
                work->intensityStep   = (end - start) / arg0;
            }
        }
    }
}

static void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3)
{
    Task*               task;
    _PadScriptLerpWork* work;
    s32                 start;
    s32                 end;
    s16                 scale;
    s32                 temp;

    if (arg0 != 0) {
        work = memCalloc(sizeof(*work), 0);
        if (work != NULL) {
            task = taskSpawn(2, 0xC, 0, 0);
            if (task == NULL) {
                memFree(work);
            } else {
                task->work = work;
                temp       = arg3 >> 3;
                if (temp == 0) {
                    scale = 1;
                } else {
                    scale = temp;
                }
                end                   = (arg2 & 0xFF) / scale;
                start                 = (arg1 & 0xFF) / scale;
                end                 <<= PAD_SCRIPT_LERP_FRACTION_BITS;
                start               <<= PAD_SCRIPT_LERP_FRACTION_BITS;
                work->framesRemaining = arg0;
                work->intensity.q8    = start;
                work->intensityStep   = (end - start) / arg0;
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
    padClearVibrationRequests(0);
}

Task* Gp_SpawnScript18(PadScriptCmd* commands, PadScriptVibrationSegment* segments)
{
    Task*           task;
    _PadScriptWork* mem;

    mem = memCalloc(sizeof(*mem), 0);
    if (mem != NULL) {
        task = taskSpawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->work       = mem;
            mem->sourceDepth = 0;
            mem->commands    = commands;
            mem->segments    = segments;
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

Task* Gp_SpawnScript18Ex(PadScriptCmd* commands, PadScriptVibrationSegment* segments, s32 arg2)
{
    Task*           task;
    _PadScriptWork* mem;

    mem = memCalloc(sizeof(*mem), 0);
    if (mem != NULL) {
        task = taskSpawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->work       = mem;
            mem->sourceDepth = arg2;
            mem->commands    = commands;
            mem->segments    = segments;
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

/// Keeps the stopped binary-motor lane idle while the other script lane can continue.
///
/// `task` is the shared script task and is unused here. Normal completion
/// waits for both saved lane opcodes to be `PAD_SCRIPT_STOP`.
static void _padScriptBinaryLaneStoppedState(Task* task)
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

/// Keeps the stopped variable-motor lane idle while the other script lane can continue.
///
/// `task` is the shared script task and is unused here. Normal completion
/// waits for both saved lane opcodes to be `PAD_SCRIPT_STOP`.
static void _padScriptVariableLaneStoppedState(Task* task)
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

void padScriptBinaryMotorHoldTask(Task* task)
{
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
        if (task->spawnArg1.value != 0 && Gp_PadHoldHalt == 0) {
            task->spawnArg1.value--;
            padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_BINARY, PAD_VIBRATION_BINARY_ON, PAD_SCRIPT_MOTOR_REFRESH_DURATION_UNITS);
            gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE;
        } else {
            gGameSession->padScriptFlags &= ~GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE;
            taskKill(task);
        }
    }
}

void padScriptVariableMotorRampTask(Task* task)
{
    _PadScriptLerpWork* ramp;

    ramp = task->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
        if (ramp->framesRemaining != 0 && Gp_PadLerpHalt == 0) {
            // Post this frame's whole intensity to the variable motor, then advance the ramp.
            ramp->framesRemaining--;
            padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, ramp->intensity.bytes.whole, PAD_SCRIPT_MOTOR_REFRESH_DURATION_UNITS);
            ramp->intensity.q8           += ramp->intensityStep;
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
    _padScriptBinaryLaneStoppedState,
    Gp_TickScriptADelay,
    Gp_TickScriptADelay,
    Gp_ScriptAState3,
    Gp_ScriptAState4,
} };

// Indexed by the lane opcode: stop, play, wait, loop, jump.
static const TaskFuncTable5 Gp_ScriptBStates = { {
    _padScriptVariableLaneStoppedState,
    Gp_TickScriptBDelay,
    Gp_TickScriptBDelay,
    Gp_ScriptBState3,
    Gp_ScriptBState4,
} };
