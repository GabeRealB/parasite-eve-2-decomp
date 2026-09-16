#include "common.h"

#include "actors/actors_shared_80136614.h"
#include "actors/actor_207000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

extern TaskDesc D_actor_207000_801575F0;

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_8", func_actor_207000_8014FF1C);

void func_actor_207000_801500C8(Actor207000* arg0)
{
    SVECTOR        sp10;
    s32            sp18;
    s32            arg2;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dst;
    Task*          spawned;

    coord   = arg0->field_2C->field_8;
    dst     = coord + 1;
    arg2    = ActorsShared80136614(coord, &sp18);
    sp10.vz = 0;
    sp10.vy = 0;
    sp10.vx = 0;
    spawned = Task_SpawnFromTable(&D_actor_207000_801575F0, 1, arg2, 0);
    if (spawned != 0) {
        Gp_CopyCoordOffset(spawned, dst, &sp10);
        Task_Reparent((Task*)arg0, spawned);
    }
}
