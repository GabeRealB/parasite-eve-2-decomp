#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "actors/actors_shared_8016974c.h"
#include "actors/actors_shared_80164ca4.h"
#include "actors/actors_shared_80164dd4.h"

extern u32 Gp_LcgState;

void ActorsShared80164ca4(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    ActorsShared80168d3cWork* work3;
    s32                       soundId;
    s32                       pan;
    u32                       rand;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (ActorsShared8016974c(arg0) != 0) {
        work->field_438  = 0;
        rand             = Gp_LcgState * 5 + 0x71357911;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work->field_7A  += 0x800;
        work2            = (ActorsShared80168d3cWork*)arg0->work;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 2;
        work3            = (ActorsShared80168d3cWork*)arg0->work;
        Gp_LcgState      = rand;
        arg0->state      = 1;
        work3->field_420 = 0;
        work3->field_422 = 0;
    }
}

void ActorsShared80164dd4(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s16                       angle;
    GsCOORDINATE2*            coord;
    ActorsShared80168d3cWork* anim;
    s32                       speed;
    s32                       dx;

    work                                           = (ActorsShared80168d3cWork*)arg0->work;
    angle                                          = work->field_40C;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = 0xC8;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->obj_2CC.pos.vy                          += work->field_42A;
    work->field_428                               += 0xE;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] >= (s16)work->field_92) {
        anim                 = (ActorsShared80168d3cWork*)arg0->work;
        anim->field_426      = 2;
        anim->field_41C      = 0x10;
        anim->field_418      = 0x13;
        anim->field_414      = 1;
        coord->coord.t[1]    = (s16)work->field_92;
        work->obj_2CC.pos.vy = 0;
        work->field_412      = 0;
        work->field_422++;
    }
}
