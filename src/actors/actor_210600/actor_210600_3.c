#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_210600.h"

extern u8 D_actor_210600_8015A4B4[];
extern u8 D_actor_210600_8015A4CC[];

void func_actor_210600_8014B2C0(Task* task);

/// Spawn body: allocates the actor's 0x8D8-byte `Actor210600Work`, stores it in
/// `Task::work`, and hands the task's `TmdObject` its light / colour matrices
/// from the block's tail. The enemy object then takes the model's root
/// coordinate (`TmdObject::field_8`) as `field_4` and its third part coordinate
/// (`field_8[2]`, used by `Gp_UpdateLinkXforms`) as `field_18`; `field_1C` is
/// zeroed because `Gp_UpdateLinkXforms` fills it.
///
/// The animation context at the front of the block is started with
/// `D_actor_210600_8015A4B4` and the pose buffer at 0x30C, and the model
/// coordinate is parented to `Gfx_ViewCoord` and rebuilt once before the three
/// matrix translations are copied to `func_800D7A9C` (start 0, count 3).
void func_actor_210600_8014B8C8(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor210600Work* work;
    Actor210600Work* mem;
    TmdObject*       tmd;

    obj        = task->extra;
    coord      = obj->field_8;
    mem        = (Actor210600Work*)Mem_Calloc(0x8D8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    tmd                = task->extra;
    tmd->field_1C      = &work->light;
    tmd->field_20      = &work->color;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_4D     = 0;
    enemy->field_4C     = 0;
    enemy->field_4D     = 0;
    func_800B3F84(&work->anim, D_actor_210600_8015A4B4, (GpAnimObj*)obj, work->field_30C, work->slots);
    work->field_87C = 2;
    work->field_882 = 1;
    func_actor_210600_8014B2C0(task);
    task->field_24 = D_actor_210600_8015A4CC;
    coord->sub     = &Gfx_ViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, &vec, 0, 3);
    task->state++;
}

/// State dispatcher: copies the overlay's 3-entry state table onto the stack
/// and calls the entry `Task::state` selects, handing it the task's 0x20 spawn
/// argument and the task itself.
void func_actor_210600_8014BA3C(Task* arg0)
{
    Actor210600DispatchCtx sp;

    sp.table = D_actor_210600_80149E24;
    sp.table.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014BA98);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014BDA4);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014C0EC);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014C638);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014C7DC);
