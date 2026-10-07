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

/// Resident descriptor bank and entries for the script interpreter and its two motor tasks.
enum {
    PAD_SCRIPT_TASK_BANK                     = 2,
    PAD_SCRIPT_BINARY_MOTOR_HOLD_TASK_TYPE   = 0xB,
    PAD_SCRIPT_VARIABLE_MOTOR_RAMP_TASK_TYPE = 0xC,
    PAD_SCRIPT_TASK_TYPE                     = 0xD,
};

/// Interpreter task phases, in dispatch order; the two motor lanes keep separate opcode states.
enum {
    PAD_SCRIPT_TASK_STATE_START  = 0,
    PAD_SCRIPT_TASK_STATE_RUN    = 1,
    PAD_SCRIPT_TASK_STATE_FINISH = 2,
};

/// Eight signed source-depth units per intensity divisor; zero after this shift uses divisor 1.
enum { PAD_SCRIPT_DEPTH_DIVISOR_SHIFT = 3 };

/// Byte positions of the opcode and operand in a packed lane halfword.
enum {
    PAD_SCRIPT_OPCODE_MASK   = 0xFF,
    PAD_SCRIPT_OPERAND_SHIFT = 8,
};

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
    const PadScriptCmd*              commands;       // Borrowed step array both lanes walk
    const PadScriptVibrationSegment* segments;       // Borrowed segment table indexed by `PAD_SCRIPT_PLAY` operands
    s16                              sourceDepth;    // Signed view depth of the vibration's source; an eighth of it, or 1 when that is 0, divides the lerp lane's intensities
    PadScriptLane                    holdCommand;    // Command word the hold lane last loaded
    PadScriptLane                    lerpCommand;    // Command word the lerp lane last loaded
    u8                               holdStep;       // Index in `commands` of the hold lane's next step
    u8                               lerpStep;       // Index in `commands` of the lerp lane's next step
    u8                               holdWaitFrames; // Script frames before the hold lane steps again (a stored 0 waits 256)
    u8                               lerpWaitFrames; // Script frames before the lerp lane steps again (a stored 0 waits 256)
    u8                               holdSegment;    // Index in `segments` of the hold lane's last played segment
    u8                               lerpSegment;    // Index in `segments` of the lerp lane's last played segment
    u8                               holdLoopCount;  // Hold lane's loop counter: loaded when 0, else decremented, by `PAD_SCRIPT_LOOP`; `PAD_SCRIPT_JUMP` branches while it is nonzero
    u8                               lerpLoopCount;  // Lerp lane's loop counter, used the same way
} _PadScriptWork;
STATIC_ASSERT_SIZEOF(_PadScriptWork, 0x18);

u8 Gp_PadScriptHalt;

u8 Gp_PadHoldHalt;

u8 Gp_PadLerpHalt;

static const TaskFuncTable3 Gp_Script18States;

static const TaskFuncTable5 Gp_ScriptAStates;

static const TaskFuncTable5 Gp_ScriptBStates;

static void _padScriptStepBinaryLane(Task* task);

static void _padScriptStepVariableLane(Task* task);

static void _padScriptSpawnBinaryMotorHold(s16 durationFrames);

static void _padScriptSpawnDepthScaledVariableMotorRamp(s16 durationFrames, u8 startIntensity, u8 endIntensity, s16 sourceDepth);

static void _padScriptStartLanes(Task* task);

static void _padScriptTickLanes(Task* task);

static void _padScriptBinaryLaneStoppedState(Task* task);

static void Gp_TickScriptADelay(Task* task);

static void _padScriptBinaryLaneLoopState(Task* task);

static void _padScriptBinaryLaneJumpState(Task* task);

static void _padScriptVariableLaneStoppedState(Task* task);

static void Gp_TickScriptBDelay(Task* task);

static void _padScriptVariableLaneLoopState(Task* task);

static void _padScriptVariableLaneJumpState(Task* task);

static const TaskFuncTable3 Gp_Script18States;
static const TaskFuncTable5 Gp_ScriptAStates;
static const TaskFuncTable5 Gp_ScriptBStates;

/// Initializes an owned ramp from Q8 endpoints and a nonzero signed frame count.
///
/// The signed span division truncates toward zero. The motor task posts before
/// adding the step, so the endpoint supplies the slope without an extra post.
static inline void _padScriptInitializeRamp(_PadScriptLerpWork* work, s16 durationFrames, s32 startQ8, s32 endQ8)
{
    work->framesRemaining = durationFrames;
    work->intensity.q8    = startQ8;
    work->intensityStep   = (endQ8 - startQ8) / durationFrames;
}

/// Updates a lane's LOOP counter and advances its byte-sized command cursor.
///
/// Both pointers address distinct fields of the live script work. A zero
/// counter loads the command's operand; a nonzero counter decrements instead.
/// Cursor increment wraps modulo 256, leaving execution for the next frame.
static inline void _padScriptAdvanceLoop(u8* counter, u8* nextStep, u16 commandWord)
{
    u8 loopCount;

    loopCount = *counter;
    if (loopCount == 0) {
        loopCount = commandWord >> PAD_SCRIPT_OPERAND_SHIFT;
        *counter  = loopCount;
        (*nextStep)++;
    } else {
        loopCount--;
        *counter = loopCount;
        (*nextStep)++;
    }
}

/// Executes the binary motor lane's next command in a live script task.
///
/// The borrowed arrays must cover every reachable byte-sized step and segment
/// index. PLAY and WAIT advance the cursor and save an unsigned frame delay;
/// zero wraps to a 256-frame wait in the delay callback. LOOP changes the
/// counter once, leaving the next step for the following frame. JUMP steps
/// recursively in the same frame, so an immediate jump chain must terminate.
static void _padScriptStepBinaryLane(Task* task)
{
    _PadScriptWork*                  work;
    const PadScriptCmd*              commands;
    const PadScriptVibrationSegment* segments;
    u16                              commandWord;
    s32                              opcode;

    work        = task->work;
    commands    = work->commands;
    segments    = work->segments;
    commandWord = commands[work->holdStep].holdCommand.command;
    opcode      = commandWord & PAD_SCRIPT_OPCODE_MASK;
    // Save the opcode as this lane's dispatch state until the next command.
    work->holdCommand.command = commandWord;

    if (opcode != PAD_SCRIPT_STOP) {
        if (opcode == PAD_SCRIPT_PLAY) {
            work->holdSegment    = commandWord >> PAD_SCRIPT_OPERAND_SHIFT;
            work->holdWaitFrames = segments[work->holdSegment].durationFrames;
            _padScriptSpawnBinaryMotorHold(work->holdWaitFrames);
            work->holdStep++;
        } else if (opcode == PAD_SCRIPT_WAIT) {
            work->holdWaitFrames = commandWord >> PAD_SCRIPT_OPERAND_SHIFT;
            work->holdStep++;
        } else if (opcode == PAD_SCRIPT_LOOP) {
            _padScriptAdvanceLoop(&work->holdLoopCount, &work->holdStep, commandWord);
        } else if (opcode == PAD_SCRIPT_JUMP) {
            if (work->holdLoopCount == 0) {
                work->holdStep++;
            } else {
                work->holdStep = commands[work->holdStep].holdCommand.command >> PAD_SCRIPT_OPERAND_SHIFT;
            }
            _padScriptStepBinaryLane(task);
        }
    }
}

/// Executes the variable motor lane's next command in a live script task.
///
/// Uses the same unchecked byte-sized cursor, segment and wait domains as the
/// binary lane. PLAY starts a depth-scaled ramp and waits for its duration;
/// LOOP defers the next command by one frame and JUMP steps immediately.
static void _padScriptStepVariableLane(Task* task)
{
    _PadScriptWork*                  work;
    const PadScriptCmd*              commands;
    const PadScriptVibrationSegment* segments;
    u16                              commandWord;
    s32                              opcode;

    work        = task->work;
    commands    = work->commands;
    segments    = work->segments;
    commandWord = commands[work->lerpStep].lerpCommand.command;
    opcode      = commandWord & PAD_SCRIPT_OPCODE_MASK;
    // Save the opcode as this lane's dispatch state until the next command.
    work->lerpCommand.command = commandWord;

    if (opcode != PAD_SCRIPT_STOP) {
        if (opcode == PAD_SCRIPT_PLAY) {
            work->lerpSegment    = commandWord >> PAD_SCRIPT_OPERAND_SHIFT;
            work->lerpWaitFrames = segments[work->lerpSegment].durationFrames;
            _padScriptSpawnDepthScaledVariableMotorRamp(work->lerpWaitFrames, segments[work->lerpSegment].startIntensity, segments[work->lerpSegment].endIntensity, work->sourceDepth);
            work->lerpStep++;
        } else if (opcode == PAD_SCRIPT_WAIT) {
            work->lerpWaitFrames = commandWord >> PAD_SCRIPT_OPERAND_SHIFT;
            work->lerpStep++;
        } else if (opcode == PAD_SCRIPT_LOOP) {
            _padScriptAdvanceLoop(&work->lerpLoopCount, &work->lerpStep, commandWord);
        } else if (opcode == PAD_SCRIPT_JUMP) {
            if (work->lerpLoopCount == 0) {
                work->lerpStep++;
            } else {
                work->lerpStep = commands[work->lerpStep].lerpCommand.command >> PAD_SCRIPT_OPERAND_SHIFT;
            }
            _padScriptStepVariableLane(task);
        }
    }
}

/// Starts a port-0 binary-motor hold for a nonzero signed frame count.
///
/// Normal playback requires a positive count; zero spawns nothing. The count
/// is sign-extended into the bodyless task's first spawn argument. Spawn failure
/// silently drops this hold, while the interpreter still waits its duration.
static void _padScriptSpawnBinaryMotorHold(s16 durationFrames)
{
    if (durationFrames != 0) {
        taskSpawn(PAD_SCRIPT_TASK_BANK, PAD_SCRIPT_BINARY_MOTOR_HOLD_TASK_TYPE, (s32)durationFrames, 0);
    }
}

void padScriptSpawnVariableMotorRamp(s16 durationFrames, u8 startIntensity, u8 endIntensity)
{
    Task*               task;
    _PadScriptLerpWork* work;
    s32                 startQ8;
    s32                 endQ8;

    if (durationFrames != 0) {
        work = memCalloc(sizeof(*work), 0);
        if (work != NULL) {
            task = taskSpawn(PAD_SCRIPT_TASK_BANK, PAD_SCRIPT_VARIABLE_MOTOR_RAMP_TASK_TYPE, 0, 0);
            if (task == NULL) {
                memFree(work);
            } else {
                endQ8      = (endIntensity & 0xFF) << PAD_SCRIPT_LERP_FRACTION_BITS;
                startQ8    = (startIntensity & 0xFF) << PAD_SCRIPT_LERP_FRACTION_BITS;
                task->work = work;
                _padScriptInitializeRamp(work, durationFrames, startQ8, endQ8);
            }
        }
    }
}

/// Starts a port-0 variable-motor ramp with signed source-depth attenuation.
///
/// Positive `durationFrames` gives ordinary playback; zero spawns nothing.
/// Divides each byte intensity by `sourceDepth >> PAD_SCRIPT_DEPTH_DIVISOR_SHIFT`,
/// substituting 1 only for a zero divisor, before Q8 conversion. Negative depths
/// retain a negative divisor and signed truncation; the motor task posts the
/// accumulated value's whole byte. Allocation or spawn failure drops the ramp.
static void _padScriptSpawnDepthScaledVariableMotorRamp(s16 durationFrames, u8 startIntensity, u8 endIntensity, s16 sourceDepth)
{
    Task*               task;
    _PadScriptLerpWork* work;
    s32                 startQ8;
    s32                 endQ8;
    s16                 intensityDivisor;
    s32                 shiftedDepth;

    if (durationFrames != 0) {
        work = memCalloc(sizeof(*work), 0);
        if (work != NULL) {
            task = taskSpawn(PAD_SCRIPT_TASK_BANK, PAD_SCRIPT_VARIABLE_MOTOR_RAMP_TASK_TYPE, 0, 0);
            if (task == NULL) {
                memFree(work);
            } else {
                // Attach ownership before deriving the signed, zero-protected divisor.
                task->work   = work;
                shiftedDepth = sourceDepth >> PAD_SCRIPT_DEPTH_DIVISOR_SHIFT;
                if (shiftedDepth == 0) {
                    intensityDivisor = 1;
                } else {
                    intensityDivisor = shiftedDepth;
                }
                endQ8     = (endIntensity & 0xFF) / intensityDivisor;
                startQ8   = (startIntensity & 0xFF) / intensityDivisor;
                endQ8   <<= PAD_SCRIPT_LERP_FRACTION_BITS;
                startQ8 <<= PAD_SCRIPT_LERP_FRACTION_BITS;
                _padScriptInitializeRamp(work, durationFrames, startQ8, endQ8);
            }
        }
    }
}

void padScriptHalt(void)
{
    Gp_PadScriptHalt             = 1;
    Gp_PadHoldHalt               = 1;
    Gp_PadLerpHalt               = 1;
    gGameSession->padScriptFlags = 0;
    padClearVibrationRequests(0);
}

Task* padScriptSpawn(const PadScriptCmd* commands, const PadScriptVibrationSegment* segments)
{
    Task*           task;
    _PadScriptWork* work;

    work = memCalloc(sizeof(*work), 0);
    if (work != NULL) {
        task = taskSpawn(PAD_SCRIPT_TASK_BANK, PAD_SCRIPT_TASK_TYPE, 0, 0);
        if (task != NULL) {
            task->work        = work;
            work->sourceDepth = 0;
            work->commands    = commands;
            work->segments    = segments;
            return task;
        }
        memFree(work);
    }
    return NULL;
}

/// Starts both vibration lanes at their zero-initialized cursors, then enters per-frame dispatch.
static void _padScriptStartLanes(Task* task)
{
    _padScriptStepBinaryLane(task);
    _padScriptStepVariableLane(task);
    task->state++;
}

/// Dispatches the binary lane before the variable lane and finishes once both have stopped.
///
/// Saved opcodes must be in `PAD_SCRIPT_STOP` through `PAD_SCRIPT_JUMP`.
/// Completion selects teardown for the next eligible interpreter frame; the
/// independently spawned motor tasks retain their own countdowns and lifetime.
static void _padScriptTickLanes(Task* task)
{
    TaskFuncTable5  binaryLaneStates;
    TaskFuncTable5  variableLaneStates;
    _PadScriptWork* work;

    work               = task->work;
    binaryLaneStates   = Gp_ScriptAStates;
    variableLaneStates = Gp_ScriptBStates;
    binaryLaneStates.funcs[work->holdCommand.bytes.opcode](task);
    variableLaneStates.funcs[work->lerpCommand.bytes.opcode](task);
    if (work->holdCommand.bytes.opcode == PAD_SCRIPT_STOP && work->lerpCommand.bytes.opcode == PAD_SCRIPT_STOP) {
        task->state++;
    }
}

void padScriptClearHalt(void)
{
    Gp_PadScriptHalt = 0;
    Gp_PadHoldHalt   = 0;
    Gp_PadLerpHalt   = 0;
}

Task* padScriptSpawnDepthScaled(const PadScriptCmd* commands, const PadScriptVibrationSegment* segments, s32 sourceDepth)
{
    Task*           task;
    _PadScriptWork* work;

    work = memCalloc(sizeof(*work), 0);
    if (work != NULL) {
        task = taskSpawn(PAD_SCRIPT_TASK_BANK, PAD_SCRIPT_TASK_TYPE, 0, 0);
        if (task != NULL) {
            task->work        = work;
            work->sourceDepth = sourceDepth;
            work->commands    = commands;
            work->segments    = segments;
            return task;
        }
        memFree(work);
    }
    return NULL;
}

void padScriptTask(Task* task)
{
    TaskFuncTable3 states;

    states = Gp_Script18States;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING || (gGameSession->padScriptFlags & GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE)) {
        if (Gp_PadScriptHalt != 0) {
            task->state = PAD_SCRIPT_TASK_STATE_FINISH;
        }
        states.funcs[task->state](task);
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
        _padScriptStepBinaryLane(task);
    }
}

/// Advances the binary lane on the frame after its LOOP counter was updated.
static void _padScriptBinaryLaneLoopState(Task* task)
{
    _padScriptStepBinaryLane(task);
}

/// Continues the binary lane if an immediate jump chain leaves JUMP as its saved opcode.
///
/// Finite valid chains step through their destination immediately, replacing
/// the saved opcode before this callback is selected.
static void _padScriptBinaryLaneJumpState(Task* task)
{
    _padScriptStepBinaryLane(task);
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
        _padScriptStepVariableLane(task);
    }
}

/// Advances the variable lane on the frame after its LOOP counter was updated.
static void _padScriptVariableLaneLoopState(Task* task)
{
    _padScriptStepVariableLane(task);
}

/// Continues the variable lane if an immediate jump chain leaves JUMP as its saved opcode.
///
/// Finite valid chains step through their destination immediately, replacing
/// the saved opcode before this callback is selected.
static void _padScriptVariableLaneJumpState(Task* task)
{
    _padScriptStepVariableLane(task);
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
    _padScriptStartLanes,
    _padScriptTickLanes,
    taskKill,
} };

// Indexed by the lane opcode: stop, play, wait, loop, jump.
static const TaskFuncTable5 Gp_ScriptAStates = { {
    _padScriptBinaryLaneStoppedState,
    Gp_TickScriptADelay,
    Gp_TickScriptADelay,
    _padScriptBinaryLaneLoopState,
    _padScriptBinaryLaneJumpState,
} };

// Indexed by the lane opcode: stop, play, wait, loop, jump.
static const TaskFuncTable5 Gp_ScriptBStates = { {
    _padScriptVariableLaneStoppedState,
    Gp_TickScriptBDelay,
    Gp_TickScriptBDelay,
    _padScriptVariableLaneLoopState,
    _padScriptVariableLaneJumpState,
} };
