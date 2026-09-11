#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/gfx.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80164b68.h"
#include "actors/actors_shared_801692e8.h"

/// Main enemy init. Allocates the 0x454-byte `Actor342400Work`, points the
/// model at the light / color matrices inside it, runs the animation context,
/// links the collision objects and the enemy's list node, and enters state 2
/// for spawn kind 1 (low nibble of `spawnArg1`), state 1 otherwise. The root
/// coord is lifted by 0x3C and its translation kept as the spawn position.
///
/// `one` is a separate variable set before `Gp_IncStateF0Ref`: the ROM holds
/// the constant in `$s0`, which GCC only picks for a pseudo that crosses a
/// call (sched2 then sinks the `li` below the `jal`).
void func_actor_342400_80163C58(Task* task)
{
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor342400Work* work;
    TmdObject*       obj;
    Actor342400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor342400Work* w2;
    Actor342400Work* w3;
    Actor342400Work* w4;
    s32              one;

    enemy       = task->spawnArg2;
    root        = ((TmdObject*)task->extra)->field_8;
    task->idMap = Mem_Calloc(0x454, 0);
    work        = (Actor342400Work*)task->idMap;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    ActorsShared801692e8();
    obj            = task->extra;
    w              = (Actor342400Work*)task->idMap;
    e              = task->spawnArg2;
    coord          = obj->field_8;
    task->field_24 = D_actor_342400_80173A3C;
    obj->field_1C  = &w->lightMtx;
    obj->field_20  = &w->colorMtx;
    e->field_50    = &D_actor_342400_80170588;
    e->field_54    = (s32)w->rec_2EC;
    w->field_3FC   = &((TmdObject*)task->extra)->field_8[1];
    w->field_400   = 0x140;
    w->field_402   = 2;
    e->field_40 = e->field_42 = D_actor_342400_80170588.field_4;
    func_800B3F84(&w->anim, D_actor_342400_801739E8, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor342400Work*)task->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = 2;
    func_actor_342400_80165CC0(task);
    coord->sub = &Gfx_ViewCoord;
    ActorsShared80164b68(task);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    enemy       = task->spawnArg2;
    Gp_LinkNode(&enemy->node);
    enemy->field_4      = &((TmdObject*)task->extra)->field_8->coord;
    enemy->field_48     = 0;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vy  = 0;
    enemy->field_1C.vz  = 0;
    enemy->field_18     = &((TmdObject*)task->extra)->field_8[1];
    enemy->node.field_4 = 4;
    one                 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if ((task->spawnArg1 & 0xF) == one) {
        w3            = (Actor342400Work*)task->idMap;
        task->state   = 2;
        w3->field_420 = 0;
        w3->field_422 = 0;
    } else {
        w4            = (Actor342400Work*)task->idMap;
        task->state   = one;
        w4->field_420 = 0;
        w4->field_422 = 0;
    }
    work->field_80    = root->coord.t[0];
    root->coord.t[1] -= 0x3C;
    work->field_82    = root->coord.t[1];
    work->field_84    = root->coord.t[2];
}

/// Variant of `func_actor_342400_80163C58`'s init: also destroys the enemy
/// when bit 16 of `spawnArg1` is set,
/// sets bit 0x80 of the model's `field_C` for spawn kind 2, and enters state 6
/// with `field_451` set and the collision flags 0x8000 / 0x4000 cleared on
/// `obj_2AC` / `obj_2CC`.
///
/// `two` is a variable for the same reason as `one` in the sibling: the ROM
/// keeps the constant in `$s5` across the calls. `kind` has to be its own
/// variable too - masking `flags` in place reuses `$v1` for the result.
void func_actor_342400_80163E70(Task* task)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor342400Work* work;
    TmdObject*       obj;
    Actor342400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor342400Work* w2;
    Actor342400Work* w3;
    GpEnemy*         e2;
    s32              flags;
    s32              kind;
    s32              two;

    model       = task->extra;
    enemy       = task->spawnArg2;
    root        = model->field_8;
    task->idMap = Mem_Calloc(0x454, 0);
    work        = (Actor342400Work*)task->idMap;
    if (work == NULL) {
        goto destroy;
    }
    ActorsShared801692e8();
    flags = task->spawnArg1;
    if ((flags >> 16) & 1) {
    destroy:
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    kind = flags & 0xF;
    two  = 2;
    if (kind == two) {
        model->field_C |= 0x80;
    }
    obj            = task->extra;
    w              = (Actor342400Work*)task->idMap;
    e              = task->spawnArg2;
    coord          = obj->field_8;
    task->field_24 = D_actor_342400_80173A3C;
    obj->field_1C  = &w->lightMtx;
    obj->field_20  = &w->colorMtx;
    e->field_50    = &D_actor_342400_80170588;
    e->field_54    = (s32)w->rec_2EC;
    w->field_3FC   = &((TmdObject*)task->extra)->field_8[1];
    w->field_400   = 0x140;
    w->field_402   = two;
    e->field_40 = e->field_42 = D_actor_342400_80170588.field_4;
    func_800B3F84(&w->anim, D_actor_342400_801739E8, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor342400Work*)task->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = two;
    func_actor_342400_80165CC0(task);
    coord->sub = &Gfx_ViewCoord;
    ActorsShared80164b68(task);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    e2 = task->spawnArg2;
    Gp_LinkNode(&e2->node);
    e2->field_4          = &((TmdObject*)task->extra)->field_8->coord;
    e2->field_48         = 0;
    e2->field_1C.vx      = 0;
    e2->field_1C.vy      = 0;
    e2->field_1C.vz      = 0;
    e2->field_18         = &((TmdObject*)task->extra)->field_8[1];
    e2->node.field_4     = 1;
    work->field_80       = root->coord.t[0];
    root->coord.t[1]    -= 0x3C;
    work->field_82       = root->coord.t[1];
    work->field_84       = root->coord.t[2];
    work->field_451      = 1;
    work->obj_2AC.flags &= 0x7FFF;
    work->obj_2CC.flags &= 0xBFFF;
    w3                   = (Actor342400Work*)task->idMap;
    task->state          = 6;
    w3->field_420        = 0;
    w3->field_422        = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_2", func_actor_342400_801640B0);
