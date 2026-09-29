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
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// 0x54 work block of the helipad enemy task, hung off the `Task::work`
/// slot -- it is the `memCalloc(0x54)` block that
/// `func_acropolis_helicopter_landing_pad_8017D658` allocates, not a
/// `TaskIdMap`. Reach it with `(AhlpEnemyWork*)task->work`.
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_acropolis_helicopter_landing_pad_8017D7B0` points the `TmdObject`'s
/// `field_1C` / `field_20` at them and fills them from the three
/// `D_acropolis_helicopter_landing_pad_80182340` lights.
typedef struct AhlpEnemyWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ s32    field_C;
    /* 0x10 */ MATRIX lightMtx;
    /* 0x30 */ MATRIX colorMtx;
    /* 0x50 */ s16    field_50;
    /* 0x52 */ byte   pad_52[0x2];
} AhlpEnemyWork;
STATIC_ASSERT_SIZEOF(AhlpEnemyWork, 0x54);

/// Main-executable globals with no module header yet, both of which hold the
/// phase tick back from phase 2: `Gp_StateC08.field_A` while it equals 1, `gDisplayState.pendingMode`
/// while it is non-zero.

/// Message entries with the payload signature selected by each message id.
typedef struct {
    s32 id; // Message id; 0x7FFFFFFF terminates the table
    union {
        s32 (*animation)(Task*, s32, AnimationPlayRequest*, GpMessageArg);
        s32 (*placement)(Task*, s32, GpXformArg*, s32);
    } handler; // Callback with the argument views required by that message
} _AcropolisHelicopterLandingPadMessageEntry;
STATIC_ASSERT_SIZEOF(_AcropolisHelicopterLandingPadMessageEntry, 8);

extern _AcropolisHelicopterLandingPadMessageEntry D_acropolis_helicopter_landing_pad_80182328[];
extern GsF_LIGHT                                  D_acropolis_helicopter_landing_pad_80182340[3];
/// Per-camera-view visibility table indexed by `(u8)gGameSession->at4.loc.view`:
/// a non-zero byte keeps the enemy model visible in that view.
extern s8 D_acropolis_helicopter_landing_pad_80182370[];

extern GpXformArg D_acropolis_helicopter_landing_pad_80182394;
extern GpXformArg D_acropolis_helicopter_landing_pad_801823AC;

static void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task);
s32         func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 msgId, GpXformArg* placement, s32 arg3);

s32 func_acropolis_helicopter_landing_pad_8017D824(Task*, s32, AnimationPlayRequest*, GpMessageArg);
s32 func_acropolis_helicopter_landing_pad_8017D8E8(Task*, s32, GpXformArg*, s32);

_AcropolisHelicopterLandingPadMessageEntry D_acropolis_helicopter_landing_pad_80182328[3] = {
    { 2003, { .animation = func_acropolis_helicopter_landing_pad_8017D824 } },
    { 2004, { .placement = func_acropolis_helicopter_landing_pad_8017D8E8 } },
    { 0x7FFFFFFF, { .animation = NULL } },
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

GpXformArg D_acropolis_helicopter_landing_pad_80182394 = { { -5340, 120, -1900, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_helicopter_landing_pad_801823AC = { { -5340, -2880, -1900, 0 }, { 0, 0, 0, 0 } };

TmdBone D_acropolis_helicopter_landing_pad_801823C4[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0612C_skeleton.inc"
};

u32 D_acropolis_helicopter_landing_pad_801823E8[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0612C_partVerts.inc"
};

SVECTOR D_acropolis_helicopter_landing_pad_801823EC[191] = {
#include "assets/acropolis_helicopter_landing_pad_model_0612C_verts.inc"
};

SVECTOR D_acropolis_helicopter_landing_pad_801829E4[11] = {
#include "assets/acropolis_helicopter_landing_pad_model_0612C_normals.inc"
};

u32 D_acropolis_helicopter_landing_pad_80182A3C[812] = {
#include "assets/acropolis_helicopter_landing_pad_model_0612C_stream.inc"
};

TmdSource D_acropolis_helicopter_landing_pad_801836EC = {
    0,
    6956,
    0,
    1,
    D_acropolis_helicopter_landing_pad_801823E8,
    D_acropolis_helicopter_landing_pad_801823EC,
    D_acropolis_helicopter_landing_pad_801829E4,
    D_acropolis_helicopter_landing_pad_801823C4,
    D_acropolis_helicopter_landing_pad_80182A3C,
};

static void func_acropolis_helicopter_landing_pad_8017D658(Task* task);
static void func_acropolis_helicopter_landing_pad_8017D6E0(Task* task);

/// State-0 entry of the room's enemy task: allocates the 0x54-byte work block
/// into `Task::work`, marks the model (`field_E = 8`, clears bit 0x80 of
/// `field_C`), runs the placement setup and installs the message table.
static void func_acropolis_helicopter_landing_pad_8017D658(Task* task)
{
    TmdObject* obj = task->extra.tmd;
    void*      mem;

    mem = memCalloc(0x54, false);
    if (mem == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work    = mem;
    obj->otOffset = 8;
    obj->flags   &= 0xFF7F;
    func_acropolis_helicopter_landing_pad_8017D7B0(task);
    task->msgTable      = D_acropolis_helicopter_landing_pad_80182328;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Per-frame update of the enemy task's model. While the `field_50` countdown
/// armed by the 0x7D3 handler is running, the model's coordinate translation
/// is stepped by the work block's three velocity words and marked dirty; the
/// countdown is clamped at zero once it expires. When `gGameSession->viewReady`
/// is set, the model is hidden (bit 0x80 of `field_C`) in every camera view
/// whose entry in the per-view table is zero and shown again otherwise.
static void func_acropolis_helicopter_landing_pad_8017D6E0(Task* task)
{
    AhlpEnemyWork* work  = (AhlpEnemyWork*)task->work;
    GfxCoord*      coord = task->extra.tmd->coords;
    TmdObject*     obj   = task->extra.tmd;
    s16            n;

    n = --work->field_50;
    if (n >= 0) {
        coord->coord.t[0]  += work->field_0;
        coord->coord.t[1]  += work->field_4;
        coord->coord.t[2]  += work->field_8;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        work->field_50 = 0;
    }
    if (gGameSession->viewReady != 0) {
        if (D_acropolis_helicopter_landing_pad_80182370[gGameSession->at4.loc.view] != 0) {
            obj->flags &= 0xFF7F;
        } else {
            obj->flags |= 0x80;
        }
    }
}

/// Points the model's light / colour matrices at the work block's own copies
/// and loads the room's three flat lights into them.
static void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->work;
    TmdObject*     obj  = task->extra.tmd;
    GsF_LIGHT*     light;
    s32            i;

    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    for (i = 0, light = D_acropolis_helicopter_landing_pad_80182340; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

/// Selects one of three scripted model-placement phases from the animation id.
///
/// The two opening phases arm a countdown and movement step; the last
/// returns to the first placement and clears the countdown.
s32 func_acropolis_helicopter_landing_pad_8017D824(Task* task, s32 msgId, AnimationPlayRequest* msg, GpMessageArg arg3)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->work;

    switch (msg->animationId) {
        case 0:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->field_50 = 0x78;
            work->field_0  = 0;
            work->field_4  = -0x19;
            work->field_8  = 0;
            break;
        case 1:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_801823AC, 0);
            work->field_50 = 0x78;
            work->field_0  = 0;
            work->field_4  = 0x19;
            work->field_8  = 0;
            break;
        case 2:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->field_50 = 0;
            break;
    }
    return 0;
}

/// Msg 0x7D4 handler, also called directly by the 0x7D3 handler. Places the
/// task's model at `placement`: copies the position onto the coordinate's
/// translation, the Euler angles onto its rotation, rebuilds the rotation
/// matrix and marks the coordinate dirty.
s32 func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 msgId, GpXformArg* placement, s32 arg3)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// State handlers of the enemy task `func_acropolis_helicopter_landing_pad_8017D964`,
/// indexed by `Task::state`: set-up, the per-frame model update and
/// `Gp_EnemyTaskExit`.
static const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5C4 = {
    { func_acropolis_helicopter_landing_pad_8017D658, func_acropolis_helicopter_landing_pad_8017D6E0, Gp_EnemyTaskExit },
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
/// 0x7D6 to slot-4 entry 0; once that returns 0 and neither `Gp_StateC08.field_A` nor
/// `gDisplayState.pendingMode` holds it back, it moves to phase 2, starts the script pair
/// and queues sound 0xA2. Camera view 5 of the session raises
/// `D_acropolis_helicopter_landing_pad_80184E0C`; a cleared
/// `gGameSession->eventState` resets `D_acropolis_helicopter_landing_pad_80187F84`.
void func_acropolis_helicopter_landing_pad_8017D9BC(Task* task)
{
    s32 phase = D_acropolis_helicopter_landing_pad_80184D9C;

    if (phase == 1) {
        if (Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0) {
            if ((Gp_StateC08.field_A != phase) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                D_acropolis_helicopter_landing_pad_80184D9C = 2;
                func_800E8634(D_acropolis_helicopter_landing_pad_80184124, 0,
                              D_acropolis_helicopter_landing_pad_801844B4);
                func_800E3FAC(0xA2, 8);
            }
        }
    }
    if (gGameSession->at4.loc.view == 5) {
        D_acropolis_helicopter_landing_pad_80184E0C = 1;
    }
    if (gGameSession->eventState == 0) {
        D_acropolis_helicopter_landing_pad_80187F84 = 0;
    }
}
