#include "common.h"
#include "actors/actor_107000.h"
#include "actors/actors_shared_80137220.h"
#include "actors/actors_shared_8013777c.h"
#include "actors/actors_shared_80138640.h"
#include "main/task.h"
#include "main/tmd.h"
#include <psyq/inline_c.h>
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")
extern u8 D_801153F4;

#include "main/sound.h"
#include "gameplay/gameplay.h"
static __inline__ void update_color(GpEnemy* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block;
    block                 = (VECTOR*)((u8*)*(void**)0x1F8003FC - 0x10);
    *(VECTOR**)0x1F8003FC = block;
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}
static __inline__ void update_animation(Task* task)
{
    Actor107000Spawn2Work* work;
    s32                    i;
    work = (Actor107000Spawn2Work*)task->work;
    if (work->field_370 != (s16)work->field_372) {
        work->field_372 = work->field_370;
        work->field_374 = 0;
        for (i = 1; i < 7; i++)
            func_800B4114((GpAnimCtx*)work, i, work->field_370, 0, 8);
    } else {
        work->field_374++;
        for (i = 1; i < 7; i++)
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
}
static __inline__ void rotate_parts(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                  = (Actor107000Spawn2Work*)arg0->work;
    head                  = *(u8**)0x1F8003FC;
    *(MATRIX**)0x1F8003FC = (MATRIX*)(head - 0x20);
    scratch               = (MATRIX*)(head - 0x20);
    coord                 = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir_real();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    *(MATRIX**)0x1F8003FC                  = (MATRIX*)((u8*)*(MATRIX**)0x1F8003FC + 0x20);
}

void ActorsShared80137220(Actor107000Ctx* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    s32                    sound;
    s32                    pan;
    work = (Actor107000Spawn2Work*)arg1->work;
    switch (D_801153F4) {
        case 1:
            update_color((GpEnemy*)arg1->spawnArg2, &((TmdObject*)arg1->extra)->coords[1]);
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->field_14                   = 1;
            return;
        case 0:
        default:
            if (work->field_396 != 0) {
                ActorsShared80138640((ActorShared80138640*)arg1);
                ActorsShared8013777c(arg1);
                update_animation(arg1);
                rotate_parts(arg1);
                Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
                update_color((GpEnemy*)arg1->spawnArg2, &((TmdObject*)arg1->extra)->coords[1]);
                work->field_378 -= 2;
                if (work->field_378 < 0)
                    work->field_378 = 0;
                coord = ((TmdObject*)arg1->extra)->coords;
                if (coord->coord.t[1] > 0) {
                    sound = (((u16)((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x402C0009;
                    pan   = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
                    work->field_38C                               = 1;
                    work->rotation.vx                             = 0x400;
                    work->field_398                               = 0;
                    work->field_378                               = 0;
                    ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
                    arg1->state                                   = 1;
                    return;
                }
                if ((u16)work->field_39A == 0)
                    work->field_398 += 10;
                else
                    work->field_398 += 20;
            }
            break;
    }
}
