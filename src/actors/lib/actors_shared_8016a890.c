#include "common.h"

#include "actors/actors_shared_8016a890.h"

#include "gameplay/1BC.h"

void ActorsShared8016a890(Task* arg0)
{
    ActorsShared8016a890Work* work2;
    ActorsShared8016a890Work* work;

    work                                  = (ActorsShared8016a890Work*)arg0->idMap;
    ((GpEnemy*)arg0->spawnArg2)->field_54 = 0;
    work2                                 = (ActorsShared8016a890Work*)arg0->idMap;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}
