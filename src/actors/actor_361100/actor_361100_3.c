#include "common.h"

#include "actors/actor_361100.h"

#include "main/task.h"
#include "main/tmd.h"
#include "main/mem.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern void* D_actor_361100_8016BAE0[];

extern GpMsgEntry D_actor_361100_8016BAF0[];

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_361100_80162B0C(void)
{
    D_actor_361100_80171BE0 = 0;
}

/// Per-frame tick of the armed variant: integrates the 16.16 accumulator at
/// 0x480 three words at a time into the root part's local translation -- whole
/// part onto `coord.t`, then it is truncated back to its fraction -- runs the
/// `field_4A0` countdown that zeroes the 0x490 step while it is at 0, ticks the
/// animation slots once `field_43C` has latched, and while the part is visible
/// rebuilds its world matrix and hands the result to `Gp_UpdateActorColor`.
/// `field_4A2` counts the root part's buffers down to the free.
///
/// The twin of `func_actor_361100_801631C4` with the two groups swapped: that
/// one integrates the 0x490 group `func_actor_361100_801630D4` arms onto
/// `field_8[1]` and draws a ground shadow, this one the 0x480 group
/// `func_actor_361100_80163750` arms, onto `field_8[0]`.
void func_actor_361100_80162B18(Task* task)
{
    TmdObject*       ext  = task->extra;
    Actor361100Work* work = (Actor361100Work*)task->work;
    GsCOORDINATE2*   coord;
    VECTOR           pos;
    s32              i;

    coord              = ext->field_8;
    work->field_480   += work->field_490;
    work->field_484   += work->field_494;
    work->field_488   += work->field_498;
    coord->coord.t[0] += (s16)(work->field_480 >> 16);
    coord->coord.t[1] += (s16)(work->field_484 >> 16);
    coord->coord.t[2] += (s16)(work->field_488 >> 16);
    coord->flg         = 0;
    work->field_480    = (u16)work->field_480;
    work->field_484    = (u16)work->field_484;
    work->field_488    = (u16)work->field_488;
    if (work->field_4A0 >= 0) {
        if (work->field_4A0 == 0) {
            work->field_490 = 0;
            work->field_494 = 0;
            work->field_498 = 0;
        }
        work->field_4A0--;
    }
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->spawnArg2, &pos, 0, 0);
    }
    if (work->field_4A2 >= 0) {
        if (work->field_4A2 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4A2--;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_3", func_actor_361100_80162CBC);

void func_actor_361100_80162D28(Task* arg0)
{
    Actor361100Work*  work;
    Actor361100Coord* coord;
    GpEnemy*          enemy;

    enemy = arg0->spawnArg2;
    coord = (Actor361100Coord*)((TmdObject*)arg0->extra)->field_8;

    work = (Actor361100Work*)Mem_Calloc(sizeof(Actor361100Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4A2 = -1;
    work->field_480 = 0;
    work->field_484 = 0;
    work->field_488 = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->field_54 = 0;

    func_actor_361100_80162E04(arg0);
    TOUCH_REG(enemy);

    arg0->msgTable     = D_actor_361100_8016BAF0;
    arg0->exitCallback = func_actor_361100_80162DE4;
    arg0->state       += 1;
}

void func_actor_361100_80162DE4(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_361100_80162E04(Task* arg0)
{
    TmdObject*       ext;
    Actor361100Work* work;

    work          = (Actor361100Work*)arg0->work;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

s32 func_actor_361100_80162E20(Task* task, s32 arg1, Actor361100AnimPreset* msg)
{
    Actor361100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor361100Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_361100_8016BAE0[work->field_43E], (GpAnimObj*)ext, work->field_30C,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

s32 func_actor_361100_80162F58(Task* task, s32 arg1, Actor361100Placement* placement)
{
    Actor361100Coord* coord;
    Actor361100Work*  work;

    work              = (Actor361100Work*)task->work;
    coord             = (Actor361100Coord*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrixZYX(&coord->rot, &coord->coord);
    coord->flg      = 0;
    work->field_480 = 0;
    work->field_484 = 0;
    work->field_488 = 0;
    work->field_490 = 0;
    work->field_494 = 0;
    work->field_498 = 0;
    return 0;
}

s32 func_actor_361100_80162FF4(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C                             |= 0x80;
            ((Actor361100Work*)task->work)->field_4A2 = mode;
            obj->field_C                             |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_361100_801630D4(Task* task, s32 arg1, Actor361100Msg* msg)
{
    Actor361100Work* work;

    work = (Actor361100Work*)task->work;
    switch (msg->field_2) {
        case 0:
            work->field_490 = 0;
            work->field_494 = 0;
            work->field_498 = 0;
            break;
        case 1:
            work->field_490 = 0xFFF6CCCD;
            work->field_494 = 0xFEC13334;
            work->field_498 = 0xB9999;
            work->field_4A0 = 0x19;
            break;
        case 2:
            work->field_490 = 0xFFCC13B2;
            work->field_494 = 0xFF559D8A;
            work->field_498 = 0x1C7627;
            work->field_4A0 = 0x1A;
            break;
        case 3:
            work->field_490 = 0x606DB6;
            work->field_494 = 0x1C4DB6D;
            work->field_498 = 0xFED84925;
            work->field_4A0 = 0xE;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// Per-frame tick of the actor: integrates the 16.16 accumulator at 0x490
/// three words at a time into the root part's local translation -- whole part
/// onto `coord.t`, then it is truncated back to its fraction -- runs the
/// `field_4A0` countdown that zeroes the 0x480 step while it is at 1, ticks the
/// animation slots once `field_43C` has latched, and while the part is visible
/// draws its ground shadow, rebuilds the second part's world matrix from it and
/// re-ranks it through `func_800D7A9C`. `field_4A2` counts the second part's
/// buffers down to the free. The body is the same tick the family's sibling
/// packages run (`func_actor_335800_80163568`, `func_actor_141000_801332A0`),
/// which is why its `field_8[1]` uses are spelled out from `task` at each site
/// rather than cached.
void func_actor_361100_801631C4(Task* task)
{
    TmdObject*       ext  = task->extra;
    Actor361100Work* work = (Actor361100Work*)task->work;
    GsCOORDINATE2*   coord;
    VECTOR3          pos;
    s32              i;

    coord              = ext->field_8;
    work->field_490   += work->field_480;
    work->field_494   += work->field_484;
    work->field_498   += work->field_488;
    coord->coord.t[0] += (s16)(work->field_490 >> 16);
    coord->coord.t[1] += (s16)(work->field_494 >> 16);
    coord->coord.t[2] += (s16)(work->field_498 >> 16);
    coord->flg         = 0;
    work->field_490    = (u16)work->field_490;
    work->field_494    = (u16)work->field_494;
    work->field_498    = (u16)work->field_498;
    if (work->field_4A0 > 0) {
        if (work->field_4A0 == 1) {
            work->field_480 = 0;
            work->field_484 = 0;
            work->field_488 = 0;
        }
        work->field_4A0--;
    }
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->field_8);
        }
        ((TmdObject*)task->extra)->field_8[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->field_8[1].workm.t, 0, 3);
    }
    if (work->field_4A2 >= 0) {
        if (work->field_4A2 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4A2--;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_3", func_actor_361100_801633A4);

extern u8 D_actor_361100_80171BB8[];

void func_actor_361100_80163410(Task* arg0)
{
    Actor361100Work* work;

    work = (Actor361100Work*)Mem_Calloc(sizeof(Actor361100Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4A2 = -1;
    func_actor_361100_801634B4(arg0);
    arg0->msgTable     = D_actor_361100_80171BB8;
    arg0->exitCallback = func_actor_361100_80163494;
    arg0->state       += 1;
}

void func_actor_361100_80163494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_361100_801634B4(Task* arg0)
{
    TmdObject*       ext;
    Actor361100Work* work;

    ext           = arg0->extra;
    work          = (Actor361100Work*)arg0->work;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

extern void* D_actor_361100_80171BA8[];

s32 func_actor_361100_801634D0(Task* task, s32 arg1, Actor361100AnimPreset* msg)
{
    Actor361100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor361100Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_361100_80171BA8[work->field_43E], (GpAnimObj*)ext, work->field_30C,
                      work->slots);
    }
    work->field_43D = msg->field_4;
    if (msg->field_8 != 0 && work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_43D);
        }
    }
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_43C = 1;
    return 0;
}
