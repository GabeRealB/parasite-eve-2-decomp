#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_801666F0(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    Actor341700Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = 0x14;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work2                                          = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor341700Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor341700Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}
