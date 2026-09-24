#include "common.h"

#include "actors/actor_136100.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Fade-in task, entry 4 of the actor's task table: on its first tick it
/// allocates the 8-byte channel block and seeds all three channels to 0xFF,
/// then every frame draws the fade overlay and steps each channel down by
/// `spawnArg1`, killing itself once `r` has gone negative.
void func_actor_136100_801344AC(Task* arg0)
{
    Actor136100FadeWork* fade;
    Actor136100FadeWork* alloc;

    fade = (Actor136100FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor136100FadeWork*)Mem_Malloc(8, 0);
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
