#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_341700.h"
#include "actors/actors_shared_8016a538.h"

/// Drops the model back to the ground: eases the pitch latched in
/// `field_434` back to zero while keeping the heading, adds the accelerating
/// drop to the root Y, and on landing requests animation 0xC and advances the
/// sub-state.
void func_actor_341700_80163E58(Task* arg0)
{
    Actor341700Work*         work;
    GsCOORDINATE2*           coord;
    ActorsShared8016a538Mat  rot;
    ActorsShared8016a538Mat* src;
    MATRIX*                  dst;
    Actor341700Work*         anim;

    work               = (Actor341700Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->coords;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    work->field_434   += -work->field_434 >> 2;
    RotMatrixX(work->field_434, &src->mat);
    func_8004BFF8(work->field_7A, &src->mat);
    dst                = &coord->coord;
    dst->m[0][0]       = src->mat.m[0][0];
    dst->m[0][1]       = src->mat.m[0][1];
    dst->m[0][2]       = src->mat.m[0][2];
    dst->m[1][0]       = src->mat.m[1][0];
    dst->m[1][1]       = src->mat.m[1][1];
    dst->m[1][2]       = src->mat.m[1][2];
    dst->m[2][0]       = src->mat.m[2][0];
    dst->m[2][1]       = src->mat.m[2][1];
    dst->m[2][2]       = src->mat.m[2][2];
    work->field_428   += 2;
    work->field_42A   += work->field_428;
    coord->coord.t[1] += work->field_42A;
    if (coord->coord.t[1] > 0) {
        work->field_412   = 0;
        coord->coord.t[1] = 0;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x20;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_422++;
    }
}

/// Plays sounds 4 and 3 on the first two frames; once the hit flags are set,
/// moves the task to state 3 with the state machine at state 3.
void func_actor_341700_80163FBC(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* next;
    Actor341700Work* next2;
    u32              soundId;
    s32              pan;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (func_actor_341700_80168468(arg0)) {
        next             = (Actor341700Work*)arg0->work;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (Actor341700Work*)arg0->work;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}
