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
#include "actors/actors_shared_8016945c.h"

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
