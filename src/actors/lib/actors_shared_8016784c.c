#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actors_shared_8016784c.h"

void ActorsShared8016784c(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    GsCOORDINATE2*            coord;
    s32                       soundId;
    s32                       pan;
    s16                       angle;
    s16                       speed;

    work  = (ActorsShared80168d3cWork*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = 0x50;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 4;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}
