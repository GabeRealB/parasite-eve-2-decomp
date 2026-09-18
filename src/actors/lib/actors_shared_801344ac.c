#include "common.h"

#include "actors/actors_shared_801344ac.h"
#include "main/gameflow.h"
#include "main/mem.h"

/// Fade-in task: on its first tick it allocates the 8-byte `r`/`g`/`b` block
/// and seeds all three channels to 0xFF, then every frame it draws the fade
/// overlay and steps each channel down by `Task::spawnArg1`.  With the channels
/// starting saturated and `r` the one the end-of-fade test watches, the task
/// kills itself once `r` has gone negative.
void ActorsShared801344ac(Task* arg0)
{
    ActorShared801344acWork* fade;
    ActorShared801344acWork* alloc;

    fade = (ActorShared801344acWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (ActorShared801344acWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
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
            Task_Kill(arg0);
            break;
    }
}
