#include "common.h"

#include "actors/actor_323000.h"
#include "gameplay/3CD8.h"

void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2)
{
    SVECTOR    sp10;
    TmdObject* obj;

    switch (arg1) {
        case 0:
        case 1:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            sp10.vx = -0x1F4;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 7:
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 14:
        case 17:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
    }

    obj = (TmdObject*)task->extra;
    Gp_SpawnEff(0x60054, &obj->coords[arg1], arg2 | 0x80000000, &sp10);
}
