#include "rooms/acropolis_helicopter_landing_pad.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "acropolis_helicopter_landing_pad_private.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

/// Frames one scripted run of the landing pad's lift lasts.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_FRAMES 120

/// World units the lift moves per frame of a run.
///
/// Over `ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_FRAMES` this covers the
/// 3000 units between the lift's lower stop and the landing pad.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_STEP 25

/// Work block of the landing pad's lift, the railed platform that carries the
/// player up to the pad.
///
/// The lift's task allocates it zeroed into `Task::work`. It holds the
/// scripted travel of the lift's model and the light matrices that model
/// draws with, which the model borrows for as long as the task lives.
typedef struct {
    s32    stepX;      // World units added to the model's X translation each frame of travel
    s32    stepY;      // As `stepX` for Y; negative is upward
    s32    stepZ;      // As `stepX` for Z
    s32    field_C;    // Never accessed; role unproven
    MATRIX lightMtx;   // Light-direction matrix the model borrows, filled from the room's three flat lights
    MATRIX colorMtx;   // Light-colour matrix the model borrows, filled with `lightMtx`
    s16    stepFrames; // Frames of travel left; the lift is at rest at 0
} _AcropolisHelicopterLandingPadLiftWork;
STATIC_ASSERT_SIZEOF(_AcropolisHelicopterLandingPadLiftWork, 0x54);

extern TaskMessageEntry D_acropolis_helicopter_landing_pad_80182328[];
extern GsF_LIGHT        D_acropolis_helicopter_landing_pad_80182340[3];
/// Per-camera-view visibility table indexed by `(u8)gGameSession->location.loc.view`:
/// a non-zero byte keeps the lift model visible in that view.
extern s8 D_acropolis_helicopter_landing_pad_80182370[];

extern ActorTransform D_acropolis_helicopter_landing_pad_80182394;
extern ActorTransform D_acropolis_helicopter_landing_pad_801823AC;

static void _acropolisHelicopterLandingPadLiftInitLighting(Task* task);
static void _acropolisHelicopterLandingPadLiftInit(Task* task);
static void _acropolisHelicopterLandingPadLiftUpdate(Task* task);

static s32 _acropolisHelicopterLandingPadLiftHandleRunRequest(Task* task, s32 messageId,
                                                              const AnimationPlayRequest* request, s32 unusedArg);

TaskMessageEntry D_acropolis_helicopter_landing_pad_80182328[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _acropolisHelicopterLandingPadLiftHandleRunRequest },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { TASK_MESSAGE_TABLE_END, NULL },
};

GsF_LIGHT D_acropolis_helicopter_landing_pad_80182340[3] = {
    { 0, 4096, 0, 128, 128, 128 },
    { 4096, 0, 0, 128, 128, 128 },
    { 0, 0, 4096, 128, 128, 128 },
};

s8 D_acropolis_helicopter_landing_pad_80182370[36] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

ActorTransform D_acropolis_helicopter_landing_pad_80182394 = { { -5340, 120, -1900, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_801823AC = { { -5340, -2880, -1900, 0 }, { 0, 0, 0, 0 } };

static TmdBone _gAcropolisHelicopterLandingPadModel0547CSkeleton[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0547C_skeleton.inc"
};

static u32 _gAcropolisHelicopterLandingPadModel0547CPartVerts[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0547C_partVerts.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadModel0547CVerts[191] = {
#include "assets/acropolis_helicopter_landing_pad_model_0547C_verts.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadModel0547CNormals[11] = {
#include "assets/acropolis_helicopter_landing_pad_model_0547C_normals.inc"
};

static u32 _gAcropolisHelicopterLandingPadModel0547CStream[812] = {
#include "assets/acropolis_helicopter_landing_pad_model_0547C_stream.inc"
};

TmdSource gAcropolisHelicopterLandingPadModel0547C = {
    0,
    6956,
    0,
    1,
    _gAcropolisHelicopterLandingPadModel0547CPartVerts,
    _gAcropolisHelicopterLandingPadModel0547CVerts,
    _gAcropolisHelicopterLandingPadModel0547CNormals,
    _gAcropolisHelicopterLandingPadModel0547CSkeleton,
    _gAcropolisHelicopterLandingPadModel0547CStream,
};

/// Initializes the lift's model, lighting and message handling.
///
/// Requires state 0 and an attached TMD body with scene-owned enemy bookkeeping.
/// The task owns the zeroed primary-heap work block; allocation failure tears
/// down the task. Success makes the model drawable and advances to state 1.
static void _acropolisHelicopterLandingPadLiftInit(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_OT_OFFSET = 8 };

    TmdObject*                              liftModel = task->extra.tmd;
    _AcropolisHelicopterLandingPadLiftWork* work;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    liftModel->otOffset = ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_OT_OFFSET;
    liftModel->flags   &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    _acropolisHelicopterLandingPadLiftInitLighting(task);
    task->msgTable      = D_acropolis_helicopter_landing_pad_80182328;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Advances an active lift run and refreshes visibility for a ready camera view.
///
/// Requires the initialized lift task and a live root coordinate. A run moves
/// once per tick for exactly its armed frame count, in whole parent-coordinate
/// units. At rest the countdown remains zero. Ready views index the room's
/// visibility table directly; the room's mapped views are 1..27.
static void _acropolisHelicopterLandingPadLiftUpdate(Task* task)
{
    _AcropolisHelicopterLandingPadLiftWork* work      = task->work;
    GfxCoord*                               rootCoord = task->extra.tmd->coords;
    TmdObject*                              liftModel = task->extra.tmd;
    s16                                     remainingFrames;

    // Decrement before stepping so the tick reaching zero still moves the lift.
    remainingFrames = --work->stepFrames;
    if (remainingFrames >= 0) {
        rootCoord->coord.t[0]  += work->stepX;
        rootCoord->coord.t[1]  += work->stepY;
        rootCoord->coord.t[2]  += work->stepZ;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        work->stepFrames = 0;
    }
    if (gGameSession->viewReady != 0) {
        if (D_acropolis_helicopter_landing_pad_80182370[gGameSession->location.loc.view] != 0) {
            liftModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            liftModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Binds the lift model to its task-owned flat-light matrices.
///
/// Requires the allocated, zeroed work block and a live TMD body. The model
/// borrows these matrices until task teardown; the three light records supply
/// direction rows and Q12 colour columns, leaving zero ambient translation.
static void _acropolisHelicopterLandingPadLiftInitLighting(Task* task)
{
    _AcropolisHelicopterLandingPadLiftWork* work      = task->work;
    TmdObject*                              liftModel = task->extra.tmd;
    const GsF_LIGHT*                        flatLight;
    s32                                     lightIndex;

    liftModel->lightMtx = &work->lightMtx;
    liftModel->colorMtx = &work->colorMtx;
    for (lightIndex = 0, flatLight = D_acropolis_helicopter_landing_pad_80182340;
         lightIndex < (s32)ARRAY_SIZE(D_acropolis_helicopter_landing_pad_80182340); lightIndex++, flatLight++) {
        gfxSetFlatLight(lightIndex, flatLight, &work->lightMtx, &work->colorMtx);
    }
}

/// Resets the lift to a starting stop and arms fixed-duration vertical travel.
///
/// Requires an initialized lift task with its work block and live TMD root.
/// `startPlacement` is borrowed only through this call; its position and signed
/// Euler orientation replace the root placement and invalidate its composed
/// transform. Position and `stepY` use whole units in the root parent's frame;
/// `stepY` is the signed Y displacement per update tick, negative upward.
/// X/Z travel is cleared and the next 120 update ticks apply the new step,
/// replacing any unfinished run. Callers pass -25 to raise or 25 to lower.
static inline void _acropolisHelicopterLandingPadLiftStartRun(Task* task, const ActorTransform* startPlacement, s32 stepY)
{
    _AcropolisHelicopterLandingPadLiftWork* work = task->work;

    actorMsgPlaceEuler(task, 0, startPlacement, 0);
    work->stepFrames = ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_FRAMES;
    work->stepX      = 0;
    work->stepY      = stepY;
    work->stepZ      = 0;
}

/// Handles the lift's scripted run selector carried by a play-animation request.
///
/// Requires the initialized lift task and a request readable through dispatch.
/// Only `animationId` is read: 0 raises from the lower stop, 1 lowers from the
/// pad, and 2 parks at the lower stop. Travel restarts at the selected stop and
/// lasts 120 ticks at 25 world units per tick; negative Y is upward. Other ids
/// leave the lift unchanged. Retains no request pointer and always returns 0;
/// the message ID and second payload word are ignored.
static s32 _acropolisHelicopterLandingPadLiftHandleRunRequest(Task* task, s32 messageId,
                                                              const AnimationPlayRequest* request, s32 unusedArg)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_RUN_RAISE      = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_RUN_LOWER      = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_RUN_PARK_LOWER = 2,
    };

    _AcropolisHelicopterLandingPadLiftWork* work = task->work;

    switch (request->animationId) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_RUN_RAISE:
            _acropolisHelicopterLandingPadLiftStartRun(task, &D_acropolis_helicopter_landing_pad_80182394,
                                                       -ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_STEP);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_RUN_LOWER:
            _acropolisHelicopterLandingPadLiftStartRun(task, &D_acropolis_helicopter_landing_pad_801823AC,
                                                       ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_STEP);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_RUN_PARK_LOWER:
            actorMsgPlaceEuler(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->stepFrames = 0;
            break;
    }
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// State handlers of the lift task `acropolisHelicopterLandingPadLiftTask`,
/// indexed by `Task::state`: set-up, the per-frame model update and
/// `enemyTaskExit`.
static const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5C4 = {
    { _acropolisHelicopterLandingPadLiftInit, _acropolisHelicopterLandingPadLiftUpdate, enemyTaskExit },
};

void acropolisHelicopterLandingPadLiftTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_acropolis_helicopter_landing_pad_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

/// Per-frame phase tick of the room's script task. In phase 1 it posts msg
/// 0x7D6 to slot-4 entry 0; once that returns 0 and neither `Gp_StateC08.mode` nor
/// `gDisplayState.pendingMode` holds it back, it moves to phase 2, starts the script pair
/// and queues sound 0xA2. Camera view 5 of the session raises
/// `D_acropolis_helicopter_landing_pad_80184E0C`; a cleared
/// `gGameSession->eventState` resets `D_acropolis_helicopter_landing_pad_80187F84`.
void func_acropolis_helicopter_landing_pad_8017D9BC(Task* task)
{
    s32 phase = D_acropolis_helicopter_landing_pad_80184D9C;

    if (phase == 1) {
        if (taskMessageDispatch(Gp_LookupSlot4(0), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
            if ((Gp_StateC08.mode != phase) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                D_acropolis_helicopter_landing_pad_80184D9C = 2;
                func_800E8634(D_acropolis_helicopter_landing_pad_80184124, 0,
                              D_acropolis_helicopter_landing_pad_801844B4);
                func_800E3FAC(0xA2, 8);
            }
        }
    }
    if (gGameSession->location.loc.view == 5) {
        D_acropolis_helicopter_landing_pad_80184E0C = 1;
    }
    if (gGameSession->eventState == 0) {
        D_acropolis_helicopter_landing_pad_80187F84 = 0;
    }
}
