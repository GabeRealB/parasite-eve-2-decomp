#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/gfx.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80163354.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_80165cc0.h"
#include "actors/actors_shared_801662ec.h"
#include "actors/actors_shared_80164b68.h"
#include "actors/actors_shared_801692e8.h"
#include "actors/actors_shared_8016a538.h"

/// Eleven state handlers, indexed by `Actor342400Work::field_420`; copied to
/// the stack before dispatch.
extern TaskFuncTable11 D_actor_342400_80161EA8;

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
    obj                = task->extra;
    w                  = (Actor342400Work*)task->idMap;
    e                  = task->spawnArg2;
    coord              = obj->field_8;
    task->field_24     = D_actor_342400_80173A3C;
    obj->field_1C      = &w->lightMtx;
    obj->field_20      = &w->colorMtx;
    e->field_50        = &D_actor_342400_80170588;
    e->field_54        = (s32)w->rec_2EC;
    w->eff_3FC.field_0 = &((TmdObject*)task->extra)->field_8[1];
    w->eff_3FC.field_4 = 0x140;
    w->eff_3FC.field_6 = 2;
    e->field_40 = e->field_42 = D_actor_342400_80170588.field_4;
    func_800B3F84(&w->anim, D_actor_342400_801739E8, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor342400Work*)task->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = 2;
    ActorsShared80165cc0(task);
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
    obj                = task->extra;
    w                  = (Actor342400Work*)task->idMap;
    e                  = task->spawnArg2;
    coord              = obj->field_8;
    task->field_24     = D_actor_342400_80173A3C;
    obj->field_1C      = &w->lightMtx;
    obj->field_20      = &w->colorMtx;
    e->field_50        = &D_actor_342400_80170588;
    e->field_54        = (s32)w->rec_2EC;
    w->eff_3FC.field_0 = &((TmdObject*)task->extra)->field_8[1];
    w->eff_3FC.field_4 = 0x140;
    w->eff_3FC.field_6 = two;
    e->field_40 = e->field_42 = D_actor_342400_80170588.field_4;
    func_800B3F84(&w->anim, D_actor_342400_801739E8, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor342400Work*)task->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = two;
    ActorsShared80165cc0(task);
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

/// Moves the task to `state` with a fresh state machine.
static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor342400Work* w = (Actor342400Work*)arg0->idMap;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Same helper as in `actor_342400_9.c`: colours `enemy` from `coord`'s world
/// position through a 0x10-byte `VECTOR` taken off `G_SCRATCH_HEAD`.
static __inline__ void update_color(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 take_hit(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;
    Actor342400Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            set_state(arg0, 3);
            w2            = (Actor342400Work*)arg0->idMap;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        set_state(arg0, 7);
        return 1;
    }
    return 0;
}

/// `actor_342400_9.c`'s `update_rotation`: wraps the pitch / heading / roll at
/// 0x78..0x7C to 12 bits and rebuilds the model root's rotation from them
/// (Z, then X, then the heading) in a matrix taken off `G_SCRATCH_HEAD`.
static __inline__ void update_rotation(Task* arg0)
{
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    MATRIX*          m     = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          dst;

    work->field_78           &= 0xFFF;
    work->field_7A           &= 0xFFF;
    work->field_7C           &= 0xFFF;
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    func_8004BFF8(work->field_7A, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// Per-frame callback for the main enemy, the eleven-state counterpart of
/// `func_actor_342400_801670C0`. In mode 0 it aims at the nearest actor
/// (`ActorsShared801662ec`), lets a pending hit (`take_hit`) replace the state
/// handler, eases `field_424` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `field_448` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `field_438` is clear.
void func_actor_342400_801640B0(Task* arg0)
{
    GpEnemy*         enemy = arg0->spawnArg2;
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable11  sp    = D_actor_342400_80161EA8;
    s32              cur;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            ActorsShared801662ec(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            ActorsShared80165cc0(arg0);
            cur             = work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            ActorsShared801639a8(arg0);
            if (work->field_432 == 1) {
                func_actor_342400_80169654(arg0, 6, (u16*)&work->field_98);
            }
            update_rotation(arg0);
            func_actor_342400_801653DC(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->field_40 <= 0) {
                set_state(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->field_40 <= 0) {
                set_state(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                set_state(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                set_state(arg0, 9);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
            ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
            ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}
