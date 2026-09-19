#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

#include "actors/actors_shared_80163148.h"

/// Two-state fade driver: on the first call it allocates the 8-byte channel
/// block, seeds `r`/`g`/`b` at 0xFF and steps the task to state 1; on every call
/// it draws the block as a full-screen overlay and decays the three channels by
/// `Task::spawnArg1`, killing the task once red has gone negative.
void ActorsShared80163148(Task* arg0)
{
    ActorsShared80163148Fade* fade;
    ActorsShared80163148Fade* alloc;

    fade = (ActorsShared80163148Fade*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (ActorsShared80163148Fade*)Mem_Calloc(8, 0);
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
            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}
