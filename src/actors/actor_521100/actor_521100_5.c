#include "common.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_801326b4.h"
#include "actors/actors_shared_801366fc.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_actor_521100_8016A358;
extern u8 D_actor_521100_8016A3A0;

void func_800D7A9C(TmdObject* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_actor_521100_80135DDC(void* spawnArg2, Task* task);
void func_actor_521100_801360C4(void* spawnArg2, Task* task);
void func_actor_521100_80136680(void* arg0, Task* task);

s32 func_actor_521100_80135D10(Actor521100* arg0, s32 arg1, s32 arg2)
{
    Actor521100Obj2C* obj;
    Actor521100Work*  work;

    obj  = arg0->field_2C;
    work = arg0->field_1C;
    if (!(arg2 & 1)) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    if (arg2 & 2) {
        obj->field_C |= 4;
    }
    work->field_692 = arg2;
    return 0;
}

s32 func_actor_521100_80135D58(Actor521100* arg0, s32 arg1, Actor521100Msg* arg2)
{
    Actor521100Work* work;

    work = arg0->field_1C;
    switch (arg2->field_2) {
        case 0:
            work->field_68C = 1;
            work->field_68E = 0;
            work->field_690 = 0;
            break;
        case 1:
            work->field_694 = arg2->field_2;
            break;
    }
    return 0;
}

s32 func_actor_521100_80135D9C(Actor521100* arg0)
{
    arg0->field_1C->field_6B0 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    return 0;
}

s16 func_actor_521100_80135DC8(Actor521100* arg0)
{
    return arg0->field_1C->field_6B2;
}

void func_actor_521100_80135DDC(void* spawnArg2, Task* task)
{
    VECTOR           vec;
    Actor521100Work* mem;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    enemy                   = (GpEnemy*)spawnArg2;
    obj                     = task->extra;
    coord                   = obj->field_8;
    mem                     = Mem_Calloc(0x4B4, 0);
    D_actor_521100_8016A3D8 = mem;
    task->idMap             = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = ActorsShared801366fc;
    coord->sub               = &Gfx_ViewCoord;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->node.field_5      = 0;
    enemy->node.field_4      = 1;
    obj->field_E             = 1;
    obj->field_1C            = (MATRIX*)D_actor_521100_8016A3D8;
    obj->field_20            = (MATRIX*)D_actor_521100_8016A3D8 + 1;
    vec.vx                   = coord->workm.t[0];
    vec.vy                   = coord->workm.t[1] - 0x320;
    ActorsShared801326b4Task = task;
    vec.vz                   = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3A0, (GpAnimObj*)obj, D_actor_521100_8016A3D8->pad_34C);
    D_actor_521100_8016A3D8->animId    = 1;
    D_actor_521100_8016A3D8->field_47C = 2;
    task->field_24                     = &D_actor_521100_8016A358;
    func_actor_521100_80135F2C(task);
    task->state += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80135F2C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_801360C4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80136290);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_5", func_actor_521100_80136404);

/// splat migrates this table into `func_actor_521100_80136604`'s own `.s`, so
/// there is no standalone rodata file to `INCLUDE_RODATA`; it is defined here.
/// The word of 0 after the three handlers is the `.align 3` pad before
/// `jtbl_actor_521100_80131E78` in the next unit; splat grouped it into this
/// symbol.
const Actor521100StateFuncTable3 D_actor_521100_80131E68 = { {
    func_actor_521100_80135DDC,
    func_actor_521100_80136680,
    func_actor_521100_801360C4,
} };
const s32                        D_actor_521100_80131E74 = 0;

/// State dispatcher: copies the overlay's 3-entry state table onto a 20-byte
/// stack record, fills the trailing context bytes, caches the work pointer,
/// and calls the entry `Task::state` selects.
void func_actor_521100_80136604(Task* arg0)
{
    Actor521100DispatchCtx sp;

    sp.table                = D_actor_521100_80131E68;
    sp.field_10             = 2;
    sp.field_11             = 9;
    sp.field_12             = 1;
    D_actor_521100_8016A3D8 = (Actor521100Work*)arg0->idMap;
    sp.table.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_521100_80136680(void* arg0, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->field_8;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_521100_80135F2C(task);
}
