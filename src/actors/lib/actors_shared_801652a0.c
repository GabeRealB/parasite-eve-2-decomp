#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_8016974c.h"
#include "actors/actors_shared_801652a0.h"

void ActorsShared801652a0(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* next;
    ActorsShared80168d3cWork* next2;
    u32                       soundId;
    s32                       pan;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->field_8;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->field_8;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (ActorsShared8016974c(arg0)) {
        next             = (ActorsShared80168d3cWork*)arg0->idMap;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (ActorsShared80168d3cWork*)arg0->idMap;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}
