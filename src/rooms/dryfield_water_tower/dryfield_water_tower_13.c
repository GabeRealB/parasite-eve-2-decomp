#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

#include "rooms/dryfield_water_tower.h"

/// The room's fade-in task, entry 2 of `D_dryfield_water_tower_8018277C`: the
/// reverse of the fade-out `func_dryfield_water_tower_80180038`. State 0
/// allocates the `DwtwFadeWork` block into `Task::work` and saturates its
/// three channels at 0xFF; a failed allocation kills the task. Every state-1
/// frame draws the overlay tinted `r`/`g`/`r` with `Fade_DrawOverlay` and
/// lowers each channel by `Task::spawnArg1`, the fade rate; once `r` falls
/// below zero the task kills itself.
void func_dryfield_water_tower_8017FF5C(Task* arg0)
{
    DwtwFadeWork* work;
    DwtwFadeWork* alloc;

    work = (DwtwFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (DwtwFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if ((s16)work->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}
