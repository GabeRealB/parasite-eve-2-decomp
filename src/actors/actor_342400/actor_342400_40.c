#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actor_342400.h"

/// Flies backwards off the heading, pitching toward 0x200, under the
/// accelerating drop; on landing levels out, requests animation 0xC,
/// launches again and advances the state.
void func_actor_342400_80167B70(Task* arg0)
{
    Actor342400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor342400Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor342400Work*)arg0->work;
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
        anim              = (Actor342400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Plays sound 9 on the first frame and hops backwards 0x50 units a frame,
/// easing the pitch back to zero, under the accelerating drop; on landing
/// advances the state.
void func_actor_342400_80167CDC(Task* arg0)
{
    Actor342400Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor342400Work*)arg0->work;
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

/// Plays sound 9 on the first frame and backs off 0x14 units a frame; once
/// the hit flags are set, disarms the outer hit body and moves the task to
/// state 3 with the state machine at state 3.
void func_actor_342400_80167E78(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    Actor342400Work* next;
    Actor342400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor342400Work*)arg0->work;
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
    work2                                          = (Actor342400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor342400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor342400Work*)arg0->work;
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}

/// Flies backwards off the heading, pitching toward 0x200, under the
/// accelerating drop; on landing requests animation 0xC, launches high
/// (-0x12C) and advances the state.
void func_actor_342400_80168010(Task* arg0)
{
    Actor342400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor342400Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor342400Work*)arg0->work;
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
        anim              = (Actor342400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x12C;
        work->field_420++;
    }
}

/// Plays sounds 9 and 3 on the first frame and backs off 0x5A units a frame,
/// pitching toward 0x800, under the accelerating drop; on landing levels
/// out, turns around, requests animation 0x11 and advances the state.
void func_actor_342400_80168174(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* anim;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s16              angle;
    s16              speed;

    work  = (Actor342400Work*)arg0->work;
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
        anim              = (Actor342400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_412   = 0;
        work->field_420++;
    }
}
