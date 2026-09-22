#include "common.h"
#include "actors/actor_110300.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared80132368(GpActorWork* arg0)
{
    if (ActorsShared80131f9cWork->field_474 == 1) {
        ActorsShared80132208();
        ActorsShared80131f9cWork->field_474 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_474 == 2) {
        ActorsShared80132180();
        ActorsShared80131f9cWork->field_474 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_474 == 3) {
        ActorsShared80132138();
    }
}
