#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_8016a184.h"

void ActorsShared8016a184(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    GpEnemy*                  enemy;
    s32                       soundId;
    s32                       pan;

    work            = (ActorsShared80168d3cWork*)arg0->idMap;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->field_40 > 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
}
