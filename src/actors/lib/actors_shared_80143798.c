#include "common.h"

#include "actors/actors_shared_80143798.h"

#include "gameplay/1BC.h"

void ActorsShared80143798(void* arg0, Task* arg1)
{
    VECTOR sp10;

    ((TmdObject*)arg1->extra)->field_8->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->field_8);
    sp10.vx = ((TmdObject*)arg1->extra)->field_8->workm.t[0];
    sp10.vy = ((TmdObject*)arg1->extra)->field_8->workm.t[1];
    sp10.vz = ((TmdObject*)arg1->extra)->field_8->workm.t[2];
}
