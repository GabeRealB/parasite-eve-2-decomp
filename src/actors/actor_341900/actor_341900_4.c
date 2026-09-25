#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

#include "actors/actor_341900.h"

/// Fade task, entry 1 of the overlay's task table: its first tick allocates
/// the channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`,
/// killing the task once `r` has gone negative.
void func_actor_341900_80163148(Task* arg0)
{
    Actor341900Fade* fade;
    Actor341900Fade* alloc;

    fade = (Actor341900Fade*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor341900Fade*)memCalloc(8, 0);
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
