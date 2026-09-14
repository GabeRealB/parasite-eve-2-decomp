#include "common.h"

#include "actors/actor_361100.h"

#include "main/task.h"
#include "main/tmd.h"
#include "main/mem.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern GpMsgEntry D_actor_361100_8016BAF0[];

void func_actor_361100_80162B0C(void)
{
    D_actor_361100_80171BE0 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_3", func_actor_361100_80162B18);

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

    arg0->idMap     = (TaskIdMap*)work;
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

    arg0->field_24     = D_actor_361100_8016BAF0;
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

    work          = (Actor361100Work*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_3", func_actor_361100_80162E20);

s32 func_actor_361100_80162F58(Task* task, s32 arg1, Actor361100Placement* placement)
{
    Actor361100Coord* coord;
    Actor361100Work*  work;

    work              = (Actor361100Work*)task->idMap;
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
            obj->field_C                              |= 0x80;
            ((Actor361100Work*)task->idMap)->field_4A2 = mode;
            obj->field_C                              |= 4;
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

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_3", func_actor_361100_801630D4);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_3", func_actor_361100_801631C4);

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

    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4A2 = -1;
    func_actor_361100_801634B4(arg0);
    arg0->field_24     = D_actor_361100_80171BB8;
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
    work          = (Actor361100Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

extern void* D_actor_361100_80171BA8[];

s32 func_actor_361100_801634D0(Task* task, s32 arg1, Actor361100AnimPreset* msg)
{
    Actor361100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor361100Work*)task->idMap;
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
