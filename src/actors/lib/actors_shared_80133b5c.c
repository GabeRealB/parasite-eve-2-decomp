#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Fade block the task keeps at `Task::work` -- that slot is not a
/// `TaskIdMap` here. The allocation below is `Mem_Malloc(8, 0)`, so the size is
/// the allocation and not a guess. The leading halfword is never touched.
typedef struct {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  r;
    /* 0x4 */ u16  g;
    /* 0x6 */ u16  b;
} ActorShared80133b5cWork;
STATIC_ASSERT_SIZEOF(ActorShared80133b5cWork, 0x8);

/// Fade a screen up from black over the `Fade_DrawOverlay` tint.
///
/// State 0 allocates the block and clears all three channels; a failed
/// allocation kills the task outright. State 1 runs every frame: it draws the
/// overlay tinted `r`/`g`/`r`, so only the green channel reads as a colour and
/// the blue one is stepped without ever being shown, then raises all three by
/// `Task::spawnArg1` -- the fade rate, not a colour. Once `r` has passed 0x100
/// the screen is fully dark and the task kills itself.
///
/// Shared by the actor_120300 and actor_120500 overlays, which are two loadouts
/// of the same enemy and spawn this task identically. The rise saturates on a
/// `u16` channel, so the test is `(s16)r >= 0x100` and not an underflow: the
/// fade-*in* counterpart that walks the same block the other way, from 0xFF
/// down past zero, is `RoomsShared8017ff5c` over in `src/rooms/lib/`, and it
/// carries its own copy of these three halfwords.
void ActorsShared80133b5c(Task* arg0)
{
    ActorShared80133b5cWork* work;
    ActorShared80133b5cWork* alloc;

    work = (ActorShared80133b5cWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (ActorShared80133b5cWork*)Mem_Malloc(8, 0);
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
