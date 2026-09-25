#include "common.h"

#include "actors/actor_120500.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Fade to black, entry 2 of the actor's task table.
///
/// State 0 allocates the channel block and clears it; a failed allocation
/// kills the task. State 1 runs every frame: it draws a subtractive
/// `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never drawn),
/// then raises all three channels by `Task::spawnArg1`, the fade rate. Once
/// `r` reaches 0x100 the screen is black and the task kills itself.
void func_actor_120500_801327E4(Task* arg0)
{
    Actor120500FadeWork* work;
    Actor120500FadeWork* alloc;

    work = (Actor120500FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor120500FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
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
            if (work->r >= 0x100) {
                taskKill(arg0);
            }
            break;
    }
}
