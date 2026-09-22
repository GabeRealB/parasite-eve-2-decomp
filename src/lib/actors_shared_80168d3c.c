#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actors_shared_80168d3c.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "actors/actors_shared_8016974c.h"
#include "actors/actors_shared_8016a084.h"
#include "actors/actors_shared_8016a184.h"

extern u32 Gp_LcgState;

void ActorsShared80168d3c(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s16                       tmp;

    work             = (ActorsShared80168d3cWork*)arg0->work;
    work->field_92   = *(u16*)&((TmdObject*)arg0->extra)->coords->coord.t[1];
    work2            = (ActorsShared80168d3cWork*)arg0->work;
    tmp              = 8;
    work2->field_426 = tmp;
    work2->field_418 = tmp;
    work2->field_41C = 0x10;
    tmp              = 1;
    work2->field_414 = tmp;
    work->field_412  = 0;
    work->field_428  = 0;
    work->field_42A  = -0x12C;
    work->field_440  = tmp;
    work->field_438  = 0;
    work->field_432  = 0;
    work->field_422  = work->field_422 + 1;
}

void ActorsShared8016a084(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       soundId;
    s32                       pan;
    u32                       rand;

    work            = (ActorsShared80168d3cWork*)arg0->work;
    work->field_438 = 0;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (ActorsShared8016974c(arg0) != 0) {
        rand             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = rand;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work2            = (ActorsShared80168d3cWork*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

void ActorsShared8016a184(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    GpEnemy*                  enemy;
    s32                       soundId;
    s32                       pan;

    work            = (ActorsShared80168d3cWork*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->hp > 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0002;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
}
