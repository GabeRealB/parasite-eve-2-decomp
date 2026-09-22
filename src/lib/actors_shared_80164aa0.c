#include "common.h"

#include "actors/actor_421600.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

void ActorsShared80164aa0(Actor421600* arg0, s16 arg1, s16 arg2)
{
    SVECTOR sp10;
    s32     spawn;

    switch (arg1) {
        case 0:
        case 1:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 7:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (Gp_State1C->roomEffectMode == 2 && spawn == 1) {
        Gp_SpawnEff(0x60054, &arg0->field_2C->coords[arg1], arg2 | 0x80000000, &sp10);
    }
}
