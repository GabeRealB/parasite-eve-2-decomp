#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Channel block the fade task keeps at `Task::work`, sized by its own
/// `Mem_Malloc(8, 0)`. The leading halfword is never touched.
typedef struct {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  r;
    /* 0x4 */ u16  g;
    /* 0x6 */ u16  b;
} Actor120300FadeWork;
STATIC_ASSERT_SIZEOF(Actor120300FadeWork, 0x8);

/// Fade task, entry 4 of the actor's task table: darkens the screen to black.
///
/// State 0 allocates the channel block and clears it; a failed allocation
/// kills the task. State 1 runs every frame: it draws a subtractive
/// `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never drawn),
/// then raises all three channels by `Task::spawnArg1`, the fade rate. Once
/// `r` reaches 0x100 the task kills itself.
void func_actor_120300_80133B5C(Task* arg0)
{
    Actor120300FadeWork* work;
    Actor120300FadeWork* alloc;

    work = (Actor120300FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor120300FadeWork*)Mem_Malloc(8, 0);
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
            if ((s16)work->r >= 0x100) {
                taskKill(arg0);
            }
            break;
    }
}
