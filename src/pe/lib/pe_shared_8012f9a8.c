#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

/// Shared Life Drain / Healing billboard. State 0 seeds the spin and colour
/// from the spawn argument and the LCG; state 1 lifts the frame and draws the
/// additive quad on odd ticks until the animation runs out.
void PeShared8012f9a8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;
    s32            state;
    s16            step;
    u16            spawn;

    mem      = arg0->spawnArg2;
    coord    = ((TmdObject*)arg0->extra)->coords;
    mem->age = (u16)mem->age + 1;
    state    = arg0->state;
    switch (state) {
        case 0:
            mem->move.vy = 4;
            mem->move.vx = 0;
            mem->move.vz = 0;
            arg0->state  = 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            spawn        = (u16)arg0->spawnArg1;
            mem->period  = 0x1000;
            mem->angle   = spawn & 0xFFF;
            return;
        case 1:
            step              = mem->move.vy;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->age & 1)) {
                mem->index = (u16)mem->index + 1;
            }
            if (mem->index < 8) {
                if ((u16)mem->age & 1) {
                    Gp_DrawFxQuad(coord, (u16)mem->index, mem->angle,
                                  (u16)mem->scale | (u16)mem->period);
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
    }
}
