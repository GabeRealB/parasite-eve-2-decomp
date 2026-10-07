#include "actors/actor_503500.h"

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

static void func_actor_503500_801324C4(Task* task);
static void func_actor_503500_801324EC(Task* arg0);
/// Script pair handed to `padScriptSpawn` on every odd pulse frame.
extern PadScriptCmd              D_actor_503500_801468A8[2];
extern PadScriptVibrationSegment D_actor_503500_801468B0[2];
/// Two 360-entry X/Z paths `func_actor_503500_8013223C` walks the model along,
/// selected by `_Actor503500SliderWork::path` (1 or 2).
extern DVECTOR_XZ D_actor_503500_80147D90[];
extern DVECTOR_XZ D_actor_503500_80148330[];

static u32     _gActor503500Model15820PartVerts[1];
static SVECTOR _gActor503500Model15820Verts[92];
static TmdBone _gActor503500Model15820Skeleton[1];
static u32     _gActor503500Model15820Stream[459];

s32 func_actor_503500_80132584(Task*, s32, s32, s32);
s32 func_actor_503500_80132664(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

TaskMessageEntry D_actor_503500_80146888[4] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_503500_80132584 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_503500_80132664 },
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

static void func_actor_503500_8013223C(Task* arg0);
static void func_actor_503500_80132430(Task* arg0);

static void func_actor_503500_8013223C(Task* arg0)
{
    TmdObject*              ext;
    _Actor503500SliderWork* work;
    Enemy*                  enemy;
    GfxCoord*               coord;
    DVECTOR_XZ*             p;
    VECTOR                  pos;

    ext   = arg0->extra.tmd;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = ext->coords;
    if (work->path != 0) {
        if (work->timer < ARRAY_SIZE(D_actor_503500_80147D90)) {
            if (work->path == 1) {
                p = &D_actor_503500_80147D90[work->timer];
            } else {
                p = &D_actor_503500_80148330[work->timer];
            }
            coord->coord.t[0] = p->vx;
            coord->coord.t[2] = p->vz;
            if (!(enemy->placeKey & 0xF)) {
                if (work->timer & 1) {
                    padScriptSpawn(D_actor_503500_801468A8, D_actor_503500_801468B0);
                    displaySetShakeY(-1);
                } else {
                    displaySetShakeY(0);
                }
            }
            work->timer++;
        } else {
            work->timer = 0;
            work->path  = 0;
            if (!(enemy->placeKey & 0xF)) {
                displaySetShakeY(0);
            }
        }
    } else if (work->timer > 0) {
        if (!(enemy->placeKey & 0xF)) {
            if (work->timer & 1) {
                padScriptSpawn(D_actor_503500_801468A8, D_actor_503500_801468B0);
                displaySetShakeY(-1);
            } else {
                displaySetShakeY(0);
            }
        }
        work->timer--;
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        // Filled and never read: the original passes the matrix's own
        // translation instead, but the stores are still emitted.
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        worldCoordSetModelLighting(ext, coord->workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

static void func_actor_503500_80132430(Task* arg0)
{
    TmdObject*              ext;
    _Actor503500SliderWork* work;

    ext  = arg0->extra.tmd;
    work = memCalloc(sizeof(_Actor503500SliderWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    ext->flags         |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    work->freeCountdown = 0;
    func_actor_503500_801324EC(arg0);
    arg0->msgTable     = D_actor_503500_80146888;
    arg0->exitCallback = func_actor_503500_801324C4;
    arg0->state       += 1;
}

/// `Task::exitCallback` of the actor's main task, and the third entry of its
/// state table: hands the `Enemy` the spawn left in `Task::spawnArg2` back to
/// `enemyDestroy`.
static void func_actor_503500_801324C4(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

static void func_actor_503500_801324EC(Task* arg0)
{
    TmdObject*              ext;
    _Actor503500SliderWork* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

#include "../../shared/actor_messages_place_euler.inc.c"

s32 func_actor_503500_80132584(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*              obj;
    _Actor503500SliderWork* work;
    s32                     ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_80132664(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor503500SliderWork* work;

    work = task->work;
    switch (msg->command) {
        case 0:
            work->path  = 0;
            work->timer = 0;
            displaySetShakeY(0);
            break;
        case 1:
            work->path                = 1;
            work->timer               = 0;
            task->extra.tmd->otOffset = 0x15;
            break;
        case 2:
            work->path                = 2;
            work->timer               = 0;
            task->extra.tmd->otOffset = 0x14;
            break;
        case 3:
            work->path  = 0;
            work->timer = 10000;
            break;
    }
    return 0;
}

/// `Task::state` handlers `func_actor_503500_8013270C` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131E24 = {
    {
        func_actor_503500_80132430,
        func_actor_503500_8013223C,
        func_actor_503500_801324C4,
    },
};

void func_actor_503500_8013270C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}
