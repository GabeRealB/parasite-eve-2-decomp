#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actors_shared_80167e78.h"

void ActorsShared80167e78(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    ActorsShared80168d3cWork* next;
    ActorsShared80168d3cWork* next2;
    s32                       soundId;
    s32                       pan;
    s32                       cond;
    s16                       angle;
    s16                       speed;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = -0x14;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work2                                           = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (ActorsShared80168d3cWork*)arg0->idMap;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (ActorsShared80168d3cWork*)arg0->idMap;
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}
