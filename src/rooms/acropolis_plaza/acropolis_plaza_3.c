#include "common.h"
#include "main/task.h"
#include "rooms/acropolis_plaza.h"

/// Two four-vertex quads facing each other across the plaza's scene object:
/// one at x - 0xBB8, one at x + 0x7D0, each spanning y .. y + 0x3E8 and
/// z - 0x1000 .. z + 0x3000. The second quad's vertices run in the opposite
/// z order, flipping its winding.
extern SVECTOR D_acropolis_plaza_80198B90[8];

/// Rebuilds the eight box vertices in `D_acropolis_plaza_80198B90` around the
/// scene work's world position.
void func_acropolis_plaza_8017DD90(Task* arg0)
{
    AcropolisPlazaWork* work = (AcropolisPlazaWork*)arg0->idMap;
    s32                 x    = work->pos.vx;
    s32                 y    = work->pos.vy;
    s32                 z    = work->pos.vz;
    s16                 near = x - 0xBB8;
    s16                 top;
    s16                 left;
    s16                 right;
    s16                 far;

    D_acropolis_plaza_80198B90[0].vx = near;
    D_acropolis_plaza_80198B90[1].vx = near;
    D_acropolis_plaza_80198B90[2].vx = near;
    D_acropolis_plaza_80198B90[3].vx = near;

    top   = y + 0x3E8;
    left  = z - 0x1000;
    right = z + 0x3000;
    far   = x + 0x7D0;

    D_acropolis_plaza_80198B90[0].vy = y;
    D_acropolis_plaza_80198B90[1].vy = y;
    D_acropolis_plaza_80198B90[2].vy = top;
    D_acropolis_plaza_80198B90[3].vy = top;

    D_acropolis_plaza_80198B90[0].vz = left;
    D_acropolis_plaza_80198B90[1].vz = right;
    D_acropolis_plaza_80198B90[2].vz = left;
    D_acropolis_plaza_80198B90[3].vz = right;

    D_acropolis_plaza_80198B90[4].vx = far;
    D_acropolis_plaza_80198B90[5].vx = far;
    D_acropolis_plaza_80198B90[6].vx = far;
    D_acropolis_plaza_80198B90[7].vx = far;

    D_acropolis_plaza_80198B90[4].vy = y;
    D_acropolis_plaza_80198B90[5].vy = y;
    D_acropolis_plaza_80198B90[6].vy = top;
    D_acropolis_plaza_80198B90[7].vy = top;

    D_acropolis_plaza_80198B90[4].vz = right;
    D_acropolis_plaza_80198B90[5].vz = left;
    D_acropolis_plaza_80198B90[6].vz = right;
    D_acropolis_plaza_80198B90[7].vz = left;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", D_acropolis_plaza_8017D5E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DE24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DFE0);
