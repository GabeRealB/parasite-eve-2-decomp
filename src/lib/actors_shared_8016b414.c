#include "common.h"
#include "main/task.h"
#include "actors/actors_shared_8016b414.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "actors/actor_341700.h"
#include "actors/actors_shared_8016945c.h"
#include "actors/actors_shared_8016b500.h"

void ActorsShared8016b414(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (ActorsShared80168d3cWork*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void ActorsShared8016b48c(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if (ActorsShared8016945c(arg0) == 0) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

void ActorsShared8016b500(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    ActorsShared80168d3cWork* work3;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((ActorsShared8016945c(arg0) << 0x10) == 0) {
        work2 = (ActorsShared80168d3cWork*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (ActorsShared80168d3cWork*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}
