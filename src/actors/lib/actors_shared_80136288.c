#include "common.h"

#include "actors/actors_shared_80136288.h"
#include "actors/actors_shared_80137e18.h"
#include "actors/actors_shared_80137ea8.h"

#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/tmd.h"

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void ActorsShared80136288(GpEnemy* arg0, Task* arg1)
{
    ActorShared80136288Work* work;
    ActorShared80136288Work* work2;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           part;
    u16                      ticks;
    s32                      state;
    s32                      one;
    s32                      i;

    obj   = (TmdObject*)arg1->extra;
    state = D_801153F4;
    work  = (ActorShared80136288Work*)arg1->work;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case2:
    obj->flags       = obj->flags | 0x80;
    arg0->node.flags = one;
    return;
default_body:
    switch (work->field_36C) {
        case 0:
            if (work->field_394 == 0) {
                SndEvt_EnqueueType6(0xD, 0, 0);
                obj->flags = 2;
                ActorsShared80137e18((ActorShared80137e18*)arg1);
            }
            arg0->recs = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_1DC);
            Gp_UnlinkObj(&work->field_22C);
            Gp_UnlinkObj(&work->field_2AC);
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x2A);
            work->field_370 = 0xC;
            work->field_36E = 0;
            work->field_36C = 1;
            break;
        case 1:
            if (work->field_394 == 0) {
                ActorsShared80137ea8((ActorShared80137ea8*)arg1);
            } else {
                obj->flags = 0x80;
            }
            ticks           = work->field_36E + 1;
            work->field_36E = ticks;
            if ((s16)ticks >= 0x3D) {
                work->field_36C = 2;
            }
            break;
        case 2:
            SndEvt_EnqueueType7(0xD, 1);
            obj->flags  = 0x80;
            part->sub   = coord;
            arg1->state = 3;
            break;
    }
    Gp_SetLightMode((GpObj4C*)arg0, 1);
    work2 = (ActorShared80136288Work*)arg1->work;
    i     = 1;
    if (work2->field_370 != (s16)work2->field_372) {
        work2->field_372 = work2->field_370;
        work2->field_374 = 0;
        do {
            func_800B4114((GpAnimCtx*)work2, i, work2->field_370, 0, 8);
            i++;
        } while (i < 7);
        return;
    }
    TOUCH_REG(i);
    work2->field_374 = (u16)(work2->field_374 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work2, i);
        i++;
    } while (i < 7);
case1:
    return;
}
