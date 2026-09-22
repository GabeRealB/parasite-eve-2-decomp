#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_8016974c.h"
#include "actors/actors_shared_801698d4.h"
#include "actors/actors_shared_8016bef0.h"

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
