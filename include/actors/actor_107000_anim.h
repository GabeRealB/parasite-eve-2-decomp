#ifndef ACTOR_107000_ANIM_H
#define ACTOR_107000_ANIM_H

#include "common.h"
#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_801153F4;
void      ActorsShared801349d8(Task*);
void      func_actor_107000_80134810(Task*, GsCOORDINATE2*);

static __inline__ void Actor107000_TickAnim(Task* task)
{
    Actor107000Work* work = (Actor107000Work*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)work, i);
            }
        }
    }
}

static __inline__ void Actor107000_UpdateColor(GpEnemy* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block         = (VECTOR*)(*(u8**)0x1F8003FC - 0x10);
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    *(VECTOR**)0x1F8003FC = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}

#endif
