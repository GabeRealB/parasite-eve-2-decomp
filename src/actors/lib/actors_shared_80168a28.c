#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_80168a28.h"
#include "actors/actors_shared_8016945c.h"

void ActorsShared80168a28(Task* arg0)
{
    ActorsShared80168d3cWork* objs;
    GpEnemy*                  enemy;
    TmdObject*                tmd;
    ActorsShared80168d3cWork* work;
    s32                       soundId;
    s32                       pan;

    work            = (ActorsShared80168d3cWork*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    tmd             = (TmdObject*)arg0->extra;
    work->field_438 = 1;
    if (enemy->hp >= 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->recs = 0;
    objs        = (ActorsShared80168d3cWork*)arg0->work;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    ActorsShared_SetTaskState(arg0, 5);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->flags |= 0x80;
}
