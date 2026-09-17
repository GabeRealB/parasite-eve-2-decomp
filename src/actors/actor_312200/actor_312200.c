#include "common.h"

#include "actors/actor_312200.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/tmd.h"

extern u8 D_actor_312200_80169F44[];
extern u8 D_actor_312200_80169F5C[];

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80161E30);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162178);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_801626C4);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162868);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162FB4);

/// Spawn body: allocates the actor's 0x984-byte `Actor312200Work`, stores it in
/// `Task::idMap` and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. The enemy takes the
/// root coordinate's matrix as `field_4` and the model's third part coordinate
/// as `field_18`; the display node the body builds in place points its `field_C`
/// at the block's three-entry `GpRec18` table and its `field_8` at the model's
/// fourth part coordinate. The model coordinate is parented to `Gfx_ViewCoord`
/// and rebuilt once before the three matrix translations are copied to
/// `func_800D7A9C` (start 0, count 3).
void func_actor_312200_80163178(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    TmdObject*       tmd;
    Actor312200Work* mem;
    Actor312200Work* work;
    GpObj*           node;

    obj         = (TmdObject*)task->extra;
    coord       = obj->field_8;
    mem         = (Actor312200Work*)Mem_Calloc(sizeof(Actor312200Work), 0);
    work        = mem;
    task->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    tmd                = (TmdObject*)task->extra;
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
    func_800B3F84(&work->anim, D_actor_312200_80169F44, (GpAnimObj*)obj, work->poses, work->slots);
    work->field_88C = 2;
    work->field_892 = 1;
    work->field_896 = 0x10;
    func_actor_312200_80162FB4(task);
    node           = &work->field_8BC;
    node->field_8  = &((TmdObject*)task->extra)->field_8[3];
    node->field_C  = work->recs;
    node->field_10 = 0;
    node->field_12 = 0;
    node->field_14 = 0;
    node->field_18 = 0x3000A;
    node->field_1C = 0x180;
    node->flags    = 1;
    Gp_LinkObj(2, node);
    node->flags |= 0x8000;
    Gp_InitRec18Table(node->field_C, 3, 0);
    task->field_24  = D_actor_312200_80169F5C;
    work->field_8AC = 0;
    work->field_8AD = 1;
    coord->sub      = &Gfx_ViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, &vec, 0, 3);
    work->field_0 = 0;
    work->field_2 = -1;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80163370);

INCLUDE_RODATA("actors/nonmatchings/actor_312200/actor_312200", D_actor_312200_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_312200/actor_312200", ActorsShared80135df4Table);
