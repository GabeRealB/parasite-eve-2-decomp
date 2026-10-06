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
/// a non-zero byte keeps the enemy model visible in that view.
extern s8 D_acropolis_helicopter_landing_pad_80182370[];

extern ActorTransform D_acropolis_helicopter_landing_pad_80182394;
extern ActorTransform D_acropolis_helicopter_landing_pad_801823AC;

static void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task);

s32 func_acropolis_helicopter_landing_pad_8017D824(Task*, s32, AnimationPlayRequest*, s32);

TaskMessageEntry D_acropolis_helicopter_landing_pad_80182328[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_acropolis_helicopter_landing_pad_8017D824 },
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

static void func_acropolis_helicopter_landing_pad_8017D658(Task* task);
static void func_acropolis_helicopter_landing_pad_8017D6E0(Task* task);

/// State-0 entry of the room's enemy task: allocates the
/// `_AcropolisHelicopterLandingPadLiftWork` block into `Task::work`, marks the
/// model (`field_E = 8`, clears bit 0x80 of `field_C`), runs the placement
/// setup and installs the message table.
static void func_acropolis_helicopter_landing_pad_8017D658(Task* task)
{
    TmdObject*                              obj = task->extra.tmd;
    _AcropolisHelicopterLandingPadLiftWork* work;

    work = memCalloc(sizeof(_AcropolisHelicopterLandingPadLiftWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work    = work;
    obj->otOffset = 8;
    obj->flags   &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    func_acropolis_helicopter_landing_pad_8017D7B0(task);
    task->msgTable      = D_acropolis_helicopter_landing_pad_80182328;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Per-frame update of the enemy task's model. While the `stepFrames` countdown
/// armed by the 0x7D3 handler is running, the model's coordinate translation
/// is stepped by the work block's `stepX` / `stepY` / `stepZ` and marked dirty;
/// the countdown is clamped at zero once it expires. When
/// `gGameSession->viewReady` is set, the model is hidden (bit 0x80 of
/// `field_C`) in every camera view whose entry in the per-view table is zero
/// and shown again otherwise.
static void func_acropolis_helicopter_landing_pad_8017D6E0(Task* task)
{
    _AcropolisHelicopterLandingPadLiftWork* work  = task->work;
    GfxCoord*                               coord = task->extra.tmd->coords;
    TmdObject*                              obj   = task->extra.tmd;
    s16                                     n;

    n = --work->stepFrames;
    if (n >= 0) {
        coord->coord.t[0]  += work->stepX;
        coord->coord.t[1]  += work->stepY;
        coord->coord.t[2]  += work->stepZ;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        work->stepFrames = 0;
    }
    if (gGameSession->viewReady != 0) {
        if (D_acropolis_helicopter_landing_pad_80182370[gGameSession->location.loc.view] != 0) {
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Points the model's light / colour matrices at the work block's own copies
/// and loads the room's three flat lights into them.
static void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task)
{
    _AcropolisHelicopterLandingPadLiftWork* work = task->work;
    TmdObject*                              obj  = task->extra.tmd;
    GsF_LIGHT*                              light;
    s32                                     i;

    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    for (i = 0, light = D_acropolis_helicopter_landing_pad_80182340; i < 3; i++, light++) {
        gfxSetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

/// Selects one of three scripted model-placement phases from the animation id.
///
/// The two opening phases arm a countdown and movement step; the last
/// returns to the first placement and clears the countdown.
s32 func_acropolis_helicopter_landing_pad_8017D824(Task* task, s32 msgId, AnimationPlayRequest* msg, s32 arg3)
{
    _AcropolisHelicopterLandingPadLiftWork* work = task->work;

    switch (msg->animationId) {
        case 0:
            actorMsgPlaceEuler(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->stepFrames = ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_FRAMES;
            work->stepX      = 0;
            work->stepY      = -ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_STEP;
            work->stepZ      = 0;
            break;
        case 1:
            actorMsgPlaceEuler(task, 0, &D_acropolis_helicopter_landing_pad_801823AC, 0);
            work->stepFrames = ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_FRAMES;
            work->stepX      = 0;
            work->stepY      = ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_TRAVEL_STEP;
            work->stepZ      = 0;
            break;
        case 2:
            actorMsgPlaceEuler(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->stepFrames = 0;
            break;
    }
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// State handlers of the enemy task `func_acropolis_helicopter_landing_pad_8017D964`,
/// indexed by `Task::state`: set-up, the per-frame model update and
/// `enemyTaskExit`.
static const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5C4 = {
    { func_acropolis_helicopter_landing_pad_8017D658, func_acropolis_helicopter_landing_pad_8017D6E0, enemyTaskExit },
};

/// The enemy task: runs the state handler
/// `D_acropolis_helicopter_landing_pad_8017D5C4` names for `Task::state`,
/// through a copy of the table taken onto the stack.
void func_acropolis_helicopter_landing_pad_8017D964(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_helicopter_landing_pad_8017D5C4;
    sp.funcs[task->state](task);
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
