#include "common.h"

#include "actors/actor_503500.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_503500_80132430`
/// (`memCalloc(0x48)`) and parked in that task's `Task::work` slot.
/// `func_actor_503500_801324EC` republishes the two matrices onto
/// `TmdObject::lightMtx` / `field_20` -- the colour/light matrix pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`
/// -- so the allocation is exactly two `MATRIX`es plus a small tail.
/// `func_actor_503500_80132664` sets `field_45` / `field_40` from the message
/// mode (0..3).
typedef struct Actor503500ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ s16    field_40;
    /* 0x42 */ byte   pad_42[0x2];
    /* 0x44 */ s8     field_44;
    /* 0x45 */ s8     field_45;
    /* 0x46 */ byte   pad_46[0x2];
} Actor503500ColorMtx;
STATIC_ASSERT_SIZEOF(Actor503500ColorMtx, 0x48);

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];
void              func_actor_503500_801324C4(Task* task);
void              func_actor_503500_801324EC(Task* arg0);
/// Script pair handed to `Gp_SpawnScript18` on every odd pulse frame.
extern u8 D_actor_503500_801468A8[];
extern u8 D_actor_503500_801468B0[];
/// Two 360-entry X/Z paths `func_actor_503500_8013223C` walks the model along,
/// selected by `Actor503500ColorMtx::field_45` (1 or 2).
extern DVECTOR_XZ D_actor_503500_80147D90[];
extern DVECTOR_XZ D_actor_503500_80148330[];

void func_actor_503500_8013223C(Task* arg0)
{
    TmdObject*           ext;
    Actor503500ColorMtx* work;
    GpEnemy*             enemy;
    GsCOORDINATE2*       coord;
    DVECTOR_XZ*          p;
    VECTOR               pos;

    ext   = arg0->extra;
    work  = (Actor503500ColorMtx*)arg0->work;
    enemy = arg0->spawnArg2;
    coord = ext->coords;
    if (work->field_45 != 0) {
        if (work->field_40 < 360) {
            if (work->field_45 == 1) {
                p = &D_actor_503500_80147D90[work->field_40];
            } else {
                p = &D_actor_503500_80148330[work->field_40];
            }
            coord->coord.t[0] = p->vx;
            coord->coord.t[2] = p->vz;
            if (!(enemy->placeKey & 0xF)) {
                if (work->field_40 & 1) {
                    Gp_SpawnScript18((s32)D_actor_503500_801468A8, (s32)D_actor_503500_801468B0);
                    Display_ClampField126(-1);
                } else {
                    Display_ClampField126(0);
                }
            }
            work->field_40++;
        } else {
            work->field_40 = 0;
            work->field_45 = 0;
            if (!(enemy->placeKey & 0xF)) {
                Display_ClampField126(0);
            }
        }
    } else if (work->field_40 > 0) {
        if (!(enemy->placeKey & 0xF)) {
            if (work->field_40 & 1) {
                Gp_SpawnScript18((s32)D_actor_503500_801468A8, (s32)D_actor_503500_801468B0);
                Display_ClampField126(-1);
            } else {
                Display_ClampField126(0);
            }
        }
        work->field_40--;
    }
    if (!(ext->flags & 0x80)) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        // Filled and never read: the original passes the matrix's own
        // translation instead, but the stores are still emitted.
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        func_800D7A9C(ext, (VECTOR*)coord->workm.t, 0, 3);
    }
    if (work->field_44 >= 0) {
        if (work->field_44 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_44--;
    }
}

void func_actor_503500_80132430(Task* arg0)
{
    TmdObject*           ext;
    Actor503500ColorMtx* work;

    ext  = arg0->extra;
    work = memCalloc(sizeof(Actor503500ColorMtx), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work     = (TaskIdMap*)work;
    ext->flags    |= 0x84;
    work->field_44 = 0;
    func_actor_503500_801324EC(arg0);
    arg0->msgTable     = D_actor_503500_80146888;
    arg0->exitCallback = func_actor_503500_801324C4;
    arg0->state       += 1;
}

/// `Task::exitCallback` of the actor's main task, and the third entry of its
/// state table: hands the `GpEnemy` the spawn left in `Task::spawnArg2` back to
/// `Gp_DestroyEnemy`.
void func_actor_503500_801324C4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

void func_actor_503500_801324EC(Task* arg0)
{
    TmdObject*           ext;
    Actor503500ColorMtx* work;

    ext           = arg0->extra;
    work          = (Actor503500ColorMtx*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Message-0x7D4 handler of the main task's table (`D_actor_503500_80146888`):
/// places the actor at `args` - the translation goes straight into the root
/// coordinate's local matrix, the Euler angles into the coordinate's `rot`
/// slot, from which the rotation is rebuilt. Clearing `flg` has the world
/// matrix recomputed. Returns 0.
s32 func_actor_503500_80132508(Task* task, s32 arg1, GpPlaceArg* args)
{
    GpCoordExt* coord;

    coord               = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

s32 func_actor_503500_80132584(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                                  |= 0x80;
            ((Actor503500ColorMtx*)task->work)->field_44 = mode;
            obj->flags                                  |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_80132664(Task* task, s32 arg1, Actor503500ModeMsg* msg)
{
    Actor503500ColorMtx* work;

    work = (Actor503500ColorMtx*)task->work;
    switch (msg->mode) {
        case 0:
            work->field_45 = 0;
            work->field_40 = 0;
            Display_ClampField126(0);
            break;
        case 1:
            work->field_45                      = 1;
            work->field_40                      = 0;
            ((TmdObject*)task->extra)->otOffset = 0x15;
            break;
        case 2:
            work->field_45                      = 2;
            work->field_40                      = 0;
            ((TmdObject*)task->extra)->otOffset = 0x14;
            break;
        case 3:
            work->field_45 = 0;
            work->field_40 = 10000;
            break;
    }
    return 0;
}

/// `Task::state` handlers `func_actor_503500_8013270C` dispatches through.
const TaskFuncTable3 D_actor_503500_80131E24 = {
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
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}
