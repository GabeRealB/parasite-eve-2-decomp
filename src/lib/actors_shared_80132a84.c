#include "common.h"

#include "actors/actor_101900.h"
#include "actors/actor_101900_facing.h"
#include "actors/actors_shared_80132808.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "psyq/abs.h"
#include "rooms/rooms_shared_80182078.h"

void ActorsShared80132a84(Actor01900* arg0)
{
    GpAnimPose          pose;
    GpAnimPose          blendPose;
    GpAnimCtx*          anim;
    s16                 weight;
    s16                 i;
    Actor01900AnimWork* work;

    work   = (Actor01900AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_8AA;
            work->slots[i].rate      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}
