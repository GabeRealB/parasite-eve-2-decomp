#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_341700.h"

extern u32 Gp_LcgState;

/// Plays sound 4 on the first frame and, once the hit flags are set, turns
/// the enemy around, draws a 0x5A..0xD9 cooldown into `field_44A`, requests
/// animation 0xD and moves the task to state 1.
void func_actor_341700_801639C0(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* work3;
    s32              soundId;
    s32              pan;
    u32              rand;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (func_actor_341700_80168468(arg0) != 0) {
        work->field_438  = 0;
        rand             = Gp_LcgState * 5 + 0x71357911;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work->field_7A  += 0x800;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 2;
        work3            = (Actor341700Work*)arg0->work;
        Gp_LcgState      = rand;
        arg0->state      = 1;
        work3->field_420 = 0;
        work3->field_422 = 0;
    }
}

/// Falls along the heading in `field_40C`: moves the root 0xC8 units a frame,
/// adds the accelerating drop `field_42A` to its Y and to the second hit
/// body, and once the root reaches the ground height saved in `field_92`
/// requests landing animation 0x13 and advances the sub-state.
void func_actor_341700_80163AF0(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor341700Work*)arg0->work;
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
        anim                 = (Actor341700Work*)arg0->work;
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
