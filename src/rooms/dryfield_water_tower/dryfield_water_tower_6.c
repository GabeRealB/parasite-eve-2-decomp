#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

#include "rooms/dryfield_water_tower.h"

/// The room's fade-out task: state 0 allocates the 8-byte `DwtwFadeWork` block
/// into `Task::work` and clears its three channels, and every state-1 frame
/// draws them with `Fade_DrawOverlay` and raises each by `Task::spawnArg1`, the
/// fade rate. The red channel is the one watched: once it passes 0x100 the fade
/// has run its course and the task kills itself. The task is the second
/// descriptor of `D_dryfield_water_tower_8018277C`, the table whose entry 0 is
/// the room script task.
///
/// The table's entry 2 runs the same body backwards -- saturated channels
/// falling past zero -- and the two rooms that carry it share it as
/// `RoomsShared8017ff5c` out of `src/rooms/lib/`, so this direction is the one
/// that stays private to the water tower.
void func_dryfield_water_tower_80180038(Task* arg0)
{
    DwtwFadeWork* work;
    DwtwFadeWork* alloc;

    work = (DwtwFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (DwtwFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if ((s16)work->r >= 0x100) {
                Task_Kill(arg0);
            }
            break;
    }
}
