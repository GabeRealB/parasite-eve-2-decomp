#include "common.h"

#include "actors/actor_323000.h"
#include "actors/actors_shared_801366fc.h"

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000_2", func_actor_323000_80163448);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000_2", func_actor_323000_80163A30);

void func_actor_323000_80163EA0(GpEnemy* enemy, Task* task)
{
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR           pos;
    TmdObject*       obj;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    Actor323000Work* work;
    Actor323000Work* work2;
    Actor323000Work* mem;

    obj         = (TmdObject*)task->extra;
    coord       = obj->field_8;
    mem         = (Actor323000Work*)Mem_Calloc(0x934, 0);
    work        = mem;
    task->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback = ActorsShared801366fc;
    work2              = (Actor323000Work*)task->idMap;
    tmd                = (TmdObject*)task->extra;
    tmd->field_1C      = &work2->light;
    tmd->field_20      = &work2->color;
    enemy->field_4     = &((TmdObject*)task->extra)->field_8->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_50     = &D_actor_323000_80164D54;
    enemy->field_4C     = 0;
    enemy->field_40     = 0;
    enemy->field_54     = 0;
    func_800B3F84(&work->anim, D_actor_323000_8017387C, (GpAnimObj*)obj, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, D_actor_323000_8017387C, (GpAnimObj*)obj, work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82E = 1;
    work->field_82A = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_323000_80163A30(task);
    task->field_24 = &D_actor_323000_801739D0;
    coord->sub     = &Gfx_ViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_323000_80173A24.field_0 = ((TmdObject*)task->extra)->field_8;
    D_actor_323000_80173A24.field_4 = 0x100;
    D_actor_323000_80173A24.field_6 = 2;
    work->field_0                   = 0;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000_2", func_actor_323000_8016409C);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000_2", func_actor_323000_8016420C);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000_2", func_actor_323000_801645A4);

void func_actor_323000_8016483C(void)
{
}
