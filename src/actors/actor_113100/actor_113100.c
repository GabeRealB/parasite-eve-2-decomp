#include "common.h"

#include "actors/actor_113100.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_113100_80131E24;
extern TaskFuncTable3 D_actor_113100_80131E30;
extern TaskFuncTable3 D_actor_113100_80131E3C;

/// Declared here rather than taken from `gameplay.h`: the overlays call this
/// with the part index and the owning task as extra arguments that the body
/// never reads, so the shared one-argument prototype does not describe this
/// call site.
extern MATRIX* Gp_GetStageView(u8*, s32, void*);

/// Setup handler (state 0): allocates the 0x540-byte work block, clears the
/// three "no id yet" sentinels and spawns the actor's children from
/// `D_actor_113100_80144308` -- index 1 only in arena mode
/// (`Game_Session->field_9 == 2`), then indices 2 and 3, whose models get the
/// texture page and CLUT of the area record the actor's own location key
/// resolves to. It then builds the work block's display node: `field_C` points
/// at the `GpRec18` table that follows it, the position triple is zeroed, the
/// node is linked and its flags raised to 0x8000 with `field_1C` set to 0x100,
/// and `field_8` is attached to model part 1. Finally it publishes the message
/// table, installs the exit callback and steps to the next state.
void func_actor_113100_80131E58(Task* task)
{
    Actor113100Work* work;
    Task*            child2;
    Task*            child3;
    GpAreaKey        key;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       sessionKey3;
    TmdObject*       model2;
    TmdObject*       model3;
    GpCdRec10*       entry2;
    GpCdRec10*       entry3;
    GpObj*           obj;
    u8               areaByte0;
    u32              raw2;
    u32              raw3;
    u32              index2;
    u32              index3;

    work = Mem_Calloc(0x540, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap     = work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_53D = -1;
    work->field_510 = 0;
    work->field_514 = 0;
    work->field_518 = 0;
    if (Game_Session->field_9 == 2) {
        work->field_534 = Task_SpawnFromTable(&D_actor_113100_80144308, 1, 8, (s32)task);
    }

    child2 = Task_SpawnFromTable(&D_actor_113100_80144308, 2, 4, (s32)task);
    if (child2 != NULL) {
        sessionKey2 = (GpAreaKey*)&Game_Session->field_4;
        raw2        = ((GpEnemy*)task->spawnArg2)->field_8;
        model2      = (TmdObject*)child2->extra;
        key.field_3 = sessionKey2->field_3;
        key.field_2 = sessionKey2->field_2;
        key.field_1 = sessionKey2->field_1;
        areaByte0   = Game_Session->field_4;
        index2      = raw2 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->field_24 = entry2->field_D;
        model2->field_25 = entry2->field_E;
        if (model2->field_18 != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    child3 = Task_SpawnFromTable(&D_actor_113100_80144308, 3, 2, (s32)task);
    if (child3 != NULL) {
        sessionKey3 = (GpAreaKey*)&Game_Session->field_4;
        raw3        = ((GpEnemy*)task->spawnArg2)->field_8;
        model3      = (TmdObject*)child3->extra;
        key.field_3 = sessionKey3->field_3;
        key.field_2 = sessionKey3->field_2;
        key.field_1 = sessionKey3->field_1;
        areaByte0   = Game_Session->field_4;
        index3      = raw3 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3           = (GpCdRec10*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->field_24 = entry3->field_D;
        model3->field_25 = entry3->field_E;
        if (model3->field_18 != NULL) {
            Tmd_ProcessStream(model3);
            Tmd_ProcessStream(model3);
        }
    }

    ActorsShared80132f24(task);

    obj           = &work->obj;
    obj->field_8  = &((TmdObject*)task->extra)->field_8[1];
    obj->field_C  = &work->field_4D8;
    obj->field_18 = 0x30000;
    obj->field_1C = 0x100;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->field_14 = 0;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->field_C, 1, 0);

    task->field_24 = &D_actor_113100_80144338;
    func_80183BAC(1);
    func_actor_113100_80132790(task, 0, 0, 0);
    task->exitCallback = func_actor_113100_80132EF0;
    task->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132104);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_801324DC);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_8013264C);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132790);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_801328EC);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E3C);

void func_actor_113100_80132AD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E24;
    sp.funcs[task->state](task);
}

void func_actor_113100_80132B30(Task* task)
{
    TmdObject*     model;
    Task*          parent;
    s32            index;
    GsCOORDINATE2* node;
    GsCOORDINATE2* part;

    model = task->extra;
    SOFT_BARRIER();
    parent = (Task*)task->spawnArg2;
    index  = task->spawnArg1;
    node   = model->field_8;
    part   = ((TmdObject*)parent->extra)->field_8;

    node->coord.t[1] = 0x64;
    node->coord.t[0] = 0;
    node->coord.t[2] = 0;
    node->flg        = 0;
    node->sub        = &part[index];

    Task_Reparent(parent, task);
    if (GameFlag_GetNibble(0xF1) == 0) {
        model->field_C &= 0xFF7F;
    } else {
        model->field_C |= 0x80;
    }
    task->state += 1;
}

/// Builds the display matrix of the modelled part this actor is posed on.
/// `spawnArg1` indexes the part in the model task's coordinate array: the part's
/// `workm` is transposed into the actor coordinate, the stage view is multiplied
/// in, and the part's X euler angle is applied, after which the coordinate's
/// update flag is cleared so the GTE sees the new matrix.
void func_actor_113100_80132BDC(Task* task)
{
    MATRIX         sp10;
    SVECTOR        sp30;
    MATRIX*        view;
    MATRIX*        coord;
    GsCOORDINATE2* part;
    GsCOORDINATE2* node;
    s32            index;

    index = task->spawnArg1;
    node  = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    part  = &((TmdObject*)((Task*)task->spawnArg2)->extra)->field_8[index];
    view  = Gp_GetStageView(&Game_Session->field_4, index, task);
    coord = &node->coord;
    TransposeMatrix(&part->workm, coord);
    TransposeMatrix(view, &sp10);
    MulMatrix0(coord, &sp10, coord);
    Gp_ExtractEuler(&sp30, &part->coord);
    RotMatrixX(sp30.vy, coord);
    node->flg = 0;
}

void func_actor_113100_80132C9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132CF4);
