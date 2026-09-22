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
#include "actors/actors_shared_8016974c.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_801698d4.h"
#include "actors/actors_shared_8016bef0.h"

void ActorsShared80169e24(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* slow;
    Actor341700Work* fast;

    work = (Actor341700Work*)arg0->work;
    if (ActorsShared8016974c(arg0) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor341700Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor341700Work*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

void ActorsShared80168be0(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    if (ActorsShared8016974c(arg0)) {
        if (work->field_44F == 1) {
            Actor342400Work* w = (Actor342400Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor342400Work* w = (Actor342400Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}
