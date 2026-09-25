#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawns effect 0x60054 at coordinate `arg1` of the actor's model while the
/// room's effect set is live, with the offset that limb uses (none at the
/// root and part 1, 0x2BC up at parts 9 and 7, 0x258 up at 14 and 17); the
/// spawn argument is `arg2` with bit 31 set. Other coordinates spawn nothing.
/// Nothing in this package calls it.
void func_actor_323400_80164AA0(Task* task, s16 arg1, s16 arg2)
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
        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[arg1], arg2 | 0x80000000, &sp10);
    }
}
