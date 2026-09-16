#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"

#include "rooms/dryfield_warehouse.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_2", func_dryfield_warehouse_8017DA58);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_2", func_dryfield_warehouse_8017DBB0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_2", func_dryfield_warehouse_8017E090);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_2", func_dryfield_warehouse_8017E22C);

/// Screen-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block and seeds all three channels to 0, then every frame it draws the fade
/// overlay and steps each channel up by `Task::spawnArg1`. `r` is the one the
/// end-of-fade test watches, so once it has reached 0x100 the display is
/// switched back on, the room's fade-task handle is cleared and the task kills
/// itself.
void func_dryfield_warehouse_8017E308(Task* arg0)
{
    DwhFadeWork* fade;
    DwhFadeWork* alloc;

    fade = (DwhFadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (DwhFadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r + (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g + (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b + (u16)arg0->spawnArg1);
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            D_dryfield_warehouse_801821C0 = NULL;
            Task_Kill(arg0);
            break;
    }
}
