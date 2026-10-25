#include "actors/actor_503500.h"
#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of a slider: one of the two long slab models the Brahman
/// intro script slides into place along a precomputed path while the screen
/// shakes.
///
/// It is allocated zeroed and kept at `Task::work`. The two matrices are the
/// model's own light and colour pair, bound to `TmdObject::lightMtx` /
/// `colorMtx` in place of the shared defaults.
typedef struct {
    MATRIX light;         // Light matrix the model is lit with
    MATRIX color;         // Colour matrix paired with `light`
    s16    timer;         // Path step, counting up, while `path` is set; otherwise frames of shaking left, counting down
    byte   pad_42[0x2];
    s8     freeCountdown; // Frames until the model's buffers are freed; negative once that is done
    s8     path;          // Path being followed (0 none, 1 first table, 2 second table)
    byte   pad_46[0x2];
} _Actor503500SliderWork;
STATIC_ASSERT_SIZEOF(_Actor503500SliderWork, 0x48);

static void _actor503500SliderExit(Task* task);
static void _actor503500SliderBindLighting(Task* task);

static u32     _gActor503500Model15820PartVerts[1];
static SVECTOR _gActor503500Model15820Verts[92];
static TmdBone _gActor503500Model15820Skeleton[1];
static u32     _gActor503500Model15820Stream[459];

static s32 _actor503500SliderSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32 _actor503500SliderApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

/// Placement, model-draw and room-command messages for the boss-scene slider.
///
/// Placement borrows an ActorTransform; draw takes an integer mode and commands
/// borrow an ActorCommand. The task borrows this table while the package is loaded.
/// The final end-ID/null row stops searches for unhandled messages.
static TaskMessageEntry _gActor503500SliderMessages[4] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor503500SliderSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor503500SliderApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Two-step slider pulse: idle binary motor, play variable segment 1, then stop.
///
/// Odd eligible slider ticks spawn a task borrowing these commands and
/// `_gActor503500SliderVibrationSegments` until both lanes stop.
static PadScriptCmd _gActor503500SliderVibrationCommands[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Two-frame vibration segments for the slider pulse's variable motor.
///
/// Only entry 1 is played, ramping intensity from 156 toward 106. Entry 0 is
/// silent and unused. The final authored bytes (0, 1) are ignored by the interpreter.
static PadScriptVibrationSegment _gActor503500SliderVibrationSegments[2] = {
    { .startIntensity = 0, .endIntensity = 0, .durationFrames = 2, .field_3 = 0 },
    { .startIntensity = 156, .endIntensity = 106, .durationFrames = 2, .field_3 = 1 },
};

static TmdBone _gActor503500Model14DA0Skeleton[1] = {
#include "assets/actor_503500_model_14DA0_skeleton.inc"
};

static u32 _gActor503500Model14DA0PartVerts[1] = {
#include "assets/actor_503500_model_14DA0_partVerts.inc"
};

static SVECTOR _gActor503500Model14DA0Verts[92] = {
#include "assets/actor_503500_model_14DA0_verts.inc"
};

static u32 _gActor503500Model14DA0Stream[469] = {
#include "assets/actor_503500_model_14DA0_stream.inc"
};

TmdSource gActor503500Model14DA0 = {
    0,
    3600,
    0,
    1,
    _gActor503500Model14DA0PartVerts,
    _gActor503500Model14DA0Verts,
    &_gActor503500Model14DA0Verts[92],
    _gActor503500Model14DA0Skeleton,
    _gActor503500Model14DA0Stream,
};

static TmdBone _gActor503500Model15820Skeleton[1] = {
#include "assets/actor_503500_model_15820_skeleton.inc"
};

static u32 _gActor503500Model15820PartVerts[1] = {
#include "assets/actor_503500_model_15820_partVerts.inc"
};

static SVECTOR _gActor503500Model15820Verts[92] = {
#include "assets/actor_503500_model_15820_verts.inc"
};

static u32 _gActor503500Model15820Stream[459] = {
#include "assets/actor_503500_model_15820_stream.inc"
};

TmdSource gActor503500Model15820 = {
    0,
    3552,
    0,
    1,
    _gActor503500Model15820PartVerts,
    _gActor503500Model15820Verts,
    &_gActor503500Model15820Verts[92],
    _gActor503500Model15820Skeleton,
    _gActor503500Model15820Stream,
};

enum { ACTOR_503500_SLIDER_PATH_SAMPLES = 360 };

/// First slab's 360 X/Z positions for the staff-card scene, one per actor update.
///
/// Signed halfwords are absolute coordinates in the model root's parent frame.
/// Path selector 1 resets the timer to zero; the update consumes samples 0..359
/// and then stops. Y is left unchanged; the table remains separate from path 2.
static DVECTOR_XZ _gActor503500SliderPathFirst[ACTOR_503500_SLIDER_PATH_SAMPLES] = {
#include "assets/actor_503500_motion_15F70.inc"
};

/// Second slab's 360 X/Z positions for the staff-card scene, one per actor update.
///
/// Signed halfwords are absolute coordinates in the model root's parent frame.
/// Path selector 2 resets the timer to zero; the update uses path 1's equal extent
/// as its bound and leaves Y unchanged. Stored separately from the first path.
static DVECTOR_XZ _gActor503500SliderPathSecond[ACTOR_503500_SLIDER_PATH_SAMPLES] = {
#include "assets/actor_503500_motion_16510.inc"
};

static void _actor503500SliderInit(Task* task);

/// Pulses shared display shake and controller vibration on alternating slider ticks.
///
/// Borrows a live enemy/work pair. Acts only when the low placement-key nibble
/// is zero. Odd timers request vibration and set vertical shake to -1 pixel;
/// even timers clear it. A failed vibration spawn still sets display shake.
/// The script arrays must remain loaded until the requested vibration task exits.
static inline void _actor503500SliderPulseShake(const Enemy* enemy, const _Actor503500SliderWork* work)
{
    enum {
        ACTOR_503500_SLIDER_SHAKE_KEY_MASK = 0xF,
        ACTOR_503500_SLIDER_SHAKE_Y        = -1,
    };

    if (!(enemy->placeKey & ACTOR_503500_SLIDER_SHAKE_KEY_MASK)) {
        if (work->timer & 1) {
            padScriptSpawn(_gActor503500SliderVibrationCommands, _gActor503500SliderVibrationSegments);
            displaySetShakeY(ACTOR_503500_SLIDER_SHAKE_Y);
        } else {
            displaySetShakeY(0);
        }
    }
}

/// Advances an intro slider's path or stationary shake and maintains its model.
///
/// Requires initialized work, a live enemy and model. Paths 1/2 consume one
/// of 360 X/Z samples per call, with a nonnegative timer; path 0 counts down
/// stationary shake. Only a zero low key nibble drives display/vibration.
/// Visible models refresh lighting from the composed root's world translation.
/// A nonnegative release countdown frees buffers on the call finding zero,
/// then becomes negative. Work and model remain owned by the task.
static void _actor503500SliderUpdate(Task* task)
{
    enum {
        ACTOR_503500_SLIDER_PATH_NONE      = 0,
        ACTOR_503500_SLIDER_PATH_FIRST     = 1,
        ACTOR_503500_SLIDER_SHAKE_KEY_MASK = 0xF,
        ACTOR_503500_SLIDER_LIGHT_COUNT    = 3,
    };
    TmdObject*              model;
    _Actor503500SliderWork* work;
    Enemy*                  enemy;
    GfxCoord*               rootCoord;
    const DVECTOR_XZ*       pathSample;
    VECTOR                  unusedWorldPosition;

    model     = task->extra.tmd;
    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    rootCoord = model->coords;
    // Motion and shake share the timer, with opposite counting directions.
    if (work->path != ACTOR_503500_SLIDER_PATH_NONE) {
        if (work->timer < ARRAY_SIZE(_gActor503500SliderPathFirst)) {
            if (work->path == ACTOR_503500_SLIDER_PATH_FIRST) {
                pathSample = &_gActor503500SliderPathFirst[work->timer];
            } else {
                pathSample = &_gActor503500SliderPathSecond[work->timer];
            }
            rootCoord->coord.t[0] = pathSample->vx;
            rootCoord->coord.t[2] = pathSample->vz;
            _actor503500SliderPulseShake(enemy, work);
            work->timer++;
        } else {
            work->timer = 0;
            work->path  = ACTOR_503500_SLIDER_PATH_NONE;
            if (!(enemy->placeKey & ACTOR_503500_SLIDER_SHAKE_KEY_MASK)) {
                displaySetShakeY(0);
            }
        }
    } else if (work->timer > 0) {
        _actor503500SliderPulseShake(enemy, work);
        work->timer--;
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        // Filled and never read: the original passes the matrix's own
        // translation instead, but the stores are still emitted.
        unusedWorldPosition.vx = rootCoord->workm.t[0];
        unusedWorldPosition.vy = rootCoord->workm.t[1];
        unusedWorldPosition.vz = rootCoord->workm.t[2];
        worldCoordSetModelLighting(model, rootCoord->workm.t, 0, ACTOR_503500_SLIDER_LIGHT_COUNT);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Allocates and initializes the hidden slider before its first update.
///
/// Requires a live enemy task and TMD model. Owns zeroed primary-heap work;
/// the model borrows its lighting matrices until default teardown. Drawing and
/// automatic buffering start disabled. A zero release countdown requests a
/// buffer release on the first update, harmless when no buffer exists.
/// Allocation failure exits the task; success installs messages/teardown and
/// advances to update state 1.
static void _actor503500SliderInit(Task* task)
{
    TmdObject*              model;
    _Actor503500SliderWork* work;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(_Actor503500SliderWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work    = work;
    model->flags |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    // The first update releases any existing buffer; a NULL buffer is a no-op.
    work->freeCountdown = 0;
    _actor503500SliderBindLighting(task);
    task->msgTable     = _gActor503500SliderMessages;
    task->exitCallback = _actor503500SliderExit;
    task->state       += 1;
}

/// Releases the slider's enemy allocation and begins default task teardown.
///
/// Exit callback and final task state. Requires the live, primary-heap Enemy
/// owned through `spawnArg2.pointer`; target tracking is detached before it is
/// freed. Default teardown owns model/work release and bypasses this callback.
/// The enemy is invalid on return; callers must not access the task afterward.
static void _actor503500SliderExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Lends the slider's light and colour matrices to its model.
///
/// Requires initialized slider work and a live TMD object. The model retains
/// both pointers, so work must survive every model draw until teardown. Does
/// not initialize the matrices or allocate storage.
static void _actor503500SliderBindLighting(Task* task)
{
    TmdObject*              model;
    _Actor503500SliderWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
}

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Sets a slider model's drawing and primitive-buffer policy.
///
/// Requires a live TMD model, plus initialized work for mode 2. Modes 0/1 hide
/// or show with automatic buffering; showing requests a buffer. Mode 2 hides,
/// suppresses automatic buffering and sets a two-tick release countdown. The
/// tick finding zero frees the buffer. Mode 3 shows with automatic buffering
/// suppressed. Show requests retain any pending release. Ignores message ID and
/// second payload. Returns 0 for modes 0..3 (even on allocation failure), 1 otherwise.
static s32 _actor503500SliderSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum {
        ACTOR_503500_SLIDER_DRAW_HIDE             = 0,
        ACTOR_503500_SLIDER_DRAW_SHOW             = 1,
        ACTOR_503500_SLIDER_DRAW_HIDE_AND_RELEASE = 2,
    };
    TmdObject*              model;
    _Actor503500SliderWork* work;
    s32                     result;

    model  = task->extra.tmd;
    result = 0;
    switch (drawMode) {
        case ACTOR_503500_SLIDER_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_503500_SLIDER_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_503500_SLIDER_DRAW_HIDE_AND_RELEASE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work          = task->work;
            // Retain the request word: release is checked after subsequent ticks.
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Starts a slider path, stops its motion/shake, or starts stationary shaking.
///
/// Borrows initialized slider work, a live model and a readable command for
/// this call. Actions 0..3 are `ACTOR_503500_SLIDER_COMMAND_*`; the command
/// context is ignored. Paths reset their step to zero and set OT offsets in
/// tags; stationary shaking counts down for 10000 update calls. Stop clears the
/// persistent display shake immediately. Unknown commands preserve all state.
/// Ignores message ID and second payload, retains no payload pointer and returns 0.
static s32 _actor503500SliderApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_503500_SLIDER_PATH_NONE             = 0,
        ACTOR_503500_SLIDER_PATH_FIRST            = 1,
        ACTOR_503500_SLIDER_PATH_SECOND           = 2,
        ACTOR_503500_SLIDER_PATH_FIRST_OT_OFFSET  = 21,
        ACTOR_503500_SLIDER_PATH_SECOND_OT_OFFSET = 20,
        ACTOR_503500_SLIDER_SHAKE_TICKS           = 10000,
    };
    _Actor503500SliderWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_503500_SLIDER_COMMAND_STOP:
            work->path  = ACTOR_503500_SLIDER_PATH_NONE;
            work->timer = 0;
            displaySetShakeY(0);
            break;
        case ACTOR_503500_SLIDER_COMMAND_PATH_FIRST:
            work->path                = ACTOR_503500_SLIDER_PATH_FIRST;
            work->timer               = 0;
            task->extra.tmd->otOffset = ACTOR_503500_SLIDER_PATH_FIRST_OT_OFFSET;
            break;
        case ACTOR_503500_SLIDER_COMMAND_PATH_SECOND:
            work->path                = ACTOR_503500_SLIDER_PATH_SECOND;
            work->timer               = 0;
            task->extra.tmd->otOffset = ACTOR_503500_SLIDER_PATH_SECOND_OT_OFFSET;
            break;
        case ACTOR_503500_SLIDER_COMMAND_SHAKE:
            work->path  = ACTOR_503500_SLIDER_PATH_NONE;
            work->timer = ACTOR_503500_SLIDER_SHAKE_TICKS;
            break;
    }
    return 0;
}

/// Slab-slider task phases: initialize, follow the selected path or shake, then exit.
///
/// Indexed by Task state 0..2 and copied before the scene-control gate.
/// The update requires initialized work and a live model/enemy; no terminator.
static const TaskFuncTable3 _gActor503500SliderTaskStates = {
    .funcs = {
        _actor503500SliderInit,
        _actor503500SliderUpdate,
        _actor503500SliderExit,
    },
};

void actor503500SliderTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor503500SliderTaskStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}
