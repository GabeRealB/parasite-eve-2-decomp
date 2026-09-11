#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_8016974c.h"

s16 func_actor_342400_80169728(Task* arg0, s32 arg1);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_18", func_actor_342400_8016A240);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_18", func_actor_342400_8016A280);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_18", func_actor_342400_8016A2FC);

void func_actor_342400_8016A370(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;
    s16              angle;
    s16              speed;

    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        speed                                           = func_actor_342400_80169728(arg0, 0x1E);
        angle                                           = work->field_7A + 0x400;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    if (ActorsShared8016974c(arg0)) {
        Actor342400Work* next;

        work->field_438 = 0;
        next            = (Actor342400Work*)arg0->idMap;
        next->field_420 = 3;
        next->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_18", func_actor_342400_8016A494);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_18", func_actor_342400_8016A4FC);
