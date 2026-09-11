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

/* The animation-player step reads the caller's `Task*` straight out of `$a0`
 * (the call passes no argument), so it stays unprototyped. */
void func_actor_341700_801649DC();

void func_actor_341700_80169380(Task* arg0)
{
    GpEnemy*         enemy;
    Actor341700Work* work;
    TmdObject*       model;
    Actor341700Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2;
    model = (TmdObject*)arg0->extra;
    work  = (Actor341700Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    func_actor_341700_801681C4(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->field_C   = model->field_C | 0x80;
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_17", func_actor_341700_80169440);

void func_actor_341700_80169520(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->idMap;
    func_actor_341700_801649DC();
    work2 = (Actor341700Work*)arg0->idMap;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}
