#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

#include "rooms/dryfield_warehouse.h"

/// Screen-fade task running the other way from `func_dryfield_warehouse_8017E308`:
/// on its first tick it allocates the 8-byte block and saturates all three
/// channels at 0xFF, then every frame it draws the fade overlay and lowers each
/// channel by `Task::spawnArg1`. Once `r` falls below 0 the screen is clear and
/// the task kills itself.
void func_dryfield_warehouse_8017E22C(Task* arg0)
{
    DwhFadeWork* fade;
    DwhFadeWork* alloc;

    fade = (DwhFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (DwhFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}
