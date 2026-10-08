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
#include "../../shared/actor_messages.h"

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
/// Script pair handed to `padScriptSpawn` on every odd pulse frame.
extern PadScriptCmd              D_actor_503500_801468A8[2];
extern PadScriptVibrationSegment D_actor_503500_801468B0[2];
/// Two 360-entry X/Z paths `_actor503500SliderUpdate` walks the model along,
/// selected by `_Actor503500SliderWork::path` (1 or 2).
extern DVECTOR_XZ D_actor_503500_80147D90[];
extern DVECTOR_XZ D_actor_503500_80148330[];

static u32     _gActor503500Model15820PartVerts[1];
static SVECTOR _gActor503500Model15820Verts[92];
static TmdBone _gActor503500Model15820Skeleton[1];
static u32     _gActor503500Model15820Stream[459];

static s32 _actor503500SliderSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32 _actor503500SliderApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

TaskMessageEntry D_actor_503500_80146888[4] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor503500SliderSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor503500SliderApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

PadScriptCmd D_actor_503500_801468A8[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_801468B0[2] = { { 0, 0, 2, 0 }, { 156, 106, 2, 1 } };

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

DVECTOR_XZ D_actor_503500_80147D90[360] = {
#include "assets/actor_503500_motion_15F70.inc"
};

DVECTOR_XZ D_actor_503500_80148330[360] = {
#include "assets/actor_503500_motion_16510.inc"
};

static void _actor503500SliderInit(Task* task);

/// Pulses display shake and controller vibration for the first intro slider.
///
/// Borrows a live enemy/work pair. Only a zero low key nibble owns the shared
/// shake. An odd timer emits vibration and a -1 vertical shake; even clears it.
static inline void _actor503500SliderPulseShake(const Enemy* enemy, const _Actor503500SliderWork* work)
{
    enum {
        ACTOR_503500_SLIDER_SHAKE_KEY_MASK = 0xF,
        ACTOR_503500_SLIDER_SHAKE_Y        = -1,
    };

    if (!(enemy->placeKey & ACTOR_503500_SLIDER_SHAKE_KEY_MASK)) {
        if (work->timer & 1) {
            padScriptSpawn(D_actor_503500_801468A8, D_actor_503500_801468B0);
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
        if (work->timer < ARRAY_SIZE(D_actor_503500_80147D90)) {
            if (work->path == ACTOR_503500_SLIDER_PATH_FIRST) {
                pathSample = &D_actor_503500_80147D90[work->timer];
            } else {
                pathSample = &D_actor_503500_80148330[work->timer];
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
    task->msgTable     = D_actor_503500_80146888;
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

#include "../../shared/actor_messages_place_euler.inc.c"

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
        ACTOR_503500_SLIDER_DRAW_HIDE                  = 0,
        ACTOR_503500_SLIDER_DRAW_SHOW                  = 1,
        ACTOR_503500_SLIDER_DRAW_HIDE_AND_RELEASE      = 2,
        ACTOR_503500_SLIDER_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
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
        case ACTOR_503500_SLIDER_DRAW_SHOW_SKIP_AUTO_BUFFER:
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

/// `Task::state` handlers `actor503500SliderTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131E24 = {
    {
        _actor503500SliderInit,
        _actor503500SliderUpdate,
        _actor503500SliderExit,
    },
};

void actor503500SliderTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_503500_80131E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}
