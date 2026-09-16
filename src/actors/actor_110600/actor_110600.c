#include "common.h"

#include "actors/actor_110600.h"
#include "actors/actors_shared_8013411c.h"
#include "actors/actors_shared_80135a60.h"

#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801322CC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132470);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132654);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801327EC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132958);

INCLUDE_RODATA("actors/nonmatchings/actor_110600/actor_110600", D_actor_110600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132D54);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132FE0);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133550);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133778);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133A94);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it uniformly: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `scale` on all three axes. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_ShrinkRotation` in `actor_444000_5.c`.
static __inline__ void Actor110600_ScaleRotation(Task* task, s16 scale)
{
    ActorShared80135a60Scratch* blk;
    GsCOORDINATE2*              coord;
    u8*                         head;
    s16                         ang;
    u16                         m22;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    coord                                         = ((TmdObject*)task->extra)->field_8;
    blk                                           = (ActorShared80135a60Scratch*)(head - 0x34);
    *(ActorShared80135a60Scratch**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]                    = *(u16*)&((ActorShared80135a60Scratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]                    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]                    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]                    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]                    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]                    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]                    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]                    = *(u16*)&blk->m.m[2][1];
    m22                                     = *(u16*)&blk->m.m[2][2];
    coord->flg                              = 0;
    coord->coord.m[2][2]                    = m22;
    ((TmdObject*)task->extra)->field_8->flg = 0;
    *(u8**)G_SCRATCH_HEAD                   = *(u8**)G_SCRATCH_HEAD + 0x34;
}

/// Placement opcode: drops the model's root coordinate onto `placement` (the
/// three longs become its translation, the Euler angles go through
/// `Gfx_RotMatrixX` / `Y` / `Z`), then rebuilds and rescales that coordinate
/// from the actor's own heading and caches the resulting yaw in the work
/// block's `field_8`. Same placement as `ActorsShared80169f74`, with the
/// rescale of `ActorsShared80135a60` folded in behind it.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    Actor110600Work* work;

    work = (Actor110600Work*)task->idMap;

    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    Actor110600_ScaleRotation(task, (s16)work->field_B7C);
    work->field_8 = ratan2(-((TmdObject*)task->extra)->field_8->coord.m[2][0],
                           ((TmdObject*)task->extra)->field_8->coord.m[2][2]);
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134040);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801341A4);

/// Per-tick animation pass: for each clip id 1..0x12, the first ten (`i < 0xB`)
/// write the two clip ids into their slot's `field_9` and tick the primary and
/// blend contexts through `func_800B3448`, then hand both poses to
/// `Gp_AnimWritePoseCopy` with `weight` at 0x8A0 and its complement; the rest
/// only rewrite the primary slot and `Gp_AnimTickIndex` it. Same body as
/// `func_actor_403000_801336B4`, which walks 24 slots instead of 19.
void func_actor_110600_80134438(Actor110600* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor110600AnimWork* work;

    work   = (Actor110600AnimWork*)arg0->field_1C;
    weight = work->field_8A0;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_89E;
            work->slots[i].field_9      = (u8)(work->field_896 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_896 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134564);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134728);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134AB4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135194);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135454);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135A18);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135B84);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135E20);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136210);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136888);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801369D8);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136B20);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136ECC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801372CC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137684);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801377FC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137980);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137AF4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137DB0);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137F2C);

void func_actor_110600_80138394(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_110600/actor_110600", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_8013839C);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80138448);

s32 func_actor_110600_80138538(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor110600Work*)arg0->idMap;
    if (enemy->field_40 > 0) {
        return 1;
    }
    work->field_BE4 = 0;
    enemy->field_4C = 0;
    work->field_BE6 = 0;
    return 0;
}
