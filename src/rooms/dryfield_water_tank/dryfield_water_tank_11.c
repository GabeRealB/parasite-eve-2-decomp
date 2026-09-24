#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"

#include "rooms/dryfield_water_tank.h"

/// Fade the water tank up from white, then tear the task down.
///
/// State 0 allocates the ramp at `Task::work` and saturates all three channels
/// at 0xFF; a failed allocation kills the task outright. State 1 runs every
/// frame: it links a semi-transparent full-screen `TILE` (`-0xA0,-0x78`,
/// `0x140x0xF0`) plus the `0xE1000240` `DR_TPAGE` into `gGpuCurrentOt[-16]`,
/// tinting the tile `r`/`g`/`r`, then steps all three channels down by
/// `Task::spawnArg1`. Once `r` drops below 0 the screen is clear, so the task
/// kills itself. The fade-out half is `func_dryfield_water_tank_8017E3C4`.
void func_dryfield_water_tank_8017E220(Task* arg0)
{
    DwtFadeWork* fade;
    DwtFadeWork* alloc;
    u8           r;
    u8           g;
    TILE*        tile;
    DR_TPAGE*    dr;

    fade = (DwtFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (DwtFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt - 16, dr);

            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
            kill:
                taskKill(arg0);
            }
            break;
    }
}
