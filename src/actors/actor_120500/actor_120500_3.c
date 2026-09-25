#include "common.h"

#include "actors/actor_120500.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Fade from black, entry 1 of the actor's task table.
///
/// State 0 allocates the channel block and seeds all three channels at 0xFF;
/// a failed allocation kills the task. State 1 runs every frame: it draws a
/// subtractive `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never
/// drawn), then lowers all three channels by `Task::spawnArg1`, the fade rate.
/// Once `r` has gone negative the screen is clear and the task kills itself.
void func_actor_120500_80132708(Task* arg0)
{
    Actor120500FadeWork* fade;
    Actor120500FadeWork* alloc;

    fade = (Actor120500FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor120500FadeWork*)Mem_Malloc(8, 0);
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
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}
