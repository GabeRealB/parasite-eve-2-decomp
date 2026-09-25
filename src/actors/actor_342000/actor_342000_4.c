#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Channel block of the overlay's fade task, sized by its own
/// `memCalloc(8, 0)` and parked in that task's `Task::work` slot. The three
/// channels start at 0xFF and fall by the task's `spawnArg1` each frame.
typedef struct Actor342000Fade {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ s16  r;
    /* 0x4 */ s16  g;
    /* 0x6 */ s16  b;
} Actor342000Fade;
STATIC_ASSERT_SIZEOF(Actor342000Fade, 0x8);

/// Fade task of the overlay's task table: its first tick allocates the
/// channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`, killing
/// the task once `r` has gone negative.
void func_actor_342000_80163EAC(Task* arg0)
{
    Actor342000Fade* fade;
    Actor342000Fade* alloc;

    fade = (Actor342000Fade*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342000Fade*)memCalloc(8, 0);
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
