#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actors_shared_80167b70.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "actors/actors_shared_80167cdc.h"
#include "actors/actors_shared_80167e78.h"
#include "actors/actors_shared_80168010.h"
#include "actors/actors_shared_80168174.h"

void ActorsShared80167b70(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s16                       angle;
    GsCOORDINATE2*            coord;
    ActorsShared80168d3cWork* anim;
    s32                       speed;
    s32                       dx;

    work                                           = (ActorsShared80168d3cWork*)arg0->work;
    angle                                          = work->field_7A;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = -0x8C;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work->field_78                                += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 2;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        anim              = (ActorsShared80168d3cWork*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

void ActorsShared80167cdc(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    GsCOORDINATE2*            coord;
    s32                       soundId;
    s32                       pan;
    s16                       angle;
    s16                       speed;

    work  = (ActorsShared80168d3cWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_412++;
    work->field_78 += -work->field_78 >> 5;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = -0x50;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 4;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

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

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = -0x14;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work2                                          = (ActorsShared80168d3cWork*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (ActorsShared80168d3cWork*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (ActorsShared80168d3cWork*)arg0->work;
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}

void ActorsShared80168010(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s16                       angle;
    GsCOORDINATE2*            coord;
    ActorsShared80168d3cWork* anim;
    s32                       speed;
    s32                       dx;

    work                                           = (ActorsShared80168d3cWork*)arg0->work;
    angle                                          = work->field_7A;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = -0x8C;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work->field_78                                += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 2;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        anim              = (ActorsShared80168d3cWork*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x12C;
        work->field_420++;
    }
}

void ActorsShared80168174(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* anim;
    GsCOORDINATE2*            coord;
    s32                       soundId;
    s32                       pan;
    s32                       soundId2;
    s32                       pan2;
    s16                       angle;
    s16                       speed;

    work  = (ActorsShared80168d3cWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_412++;
    work->field_78 += (0x800 - work->field_78) >> 3;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan2     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = -0x5A;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 4;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (ActorsShared80168d3cWork*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_412   = 0;
        work->field_420++;
    }
}
