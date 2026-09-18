#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

/// Fade block the task keeps at `Task::work` -- that slot is not a
/// `TaskIdMap` here. The allocation below is `Mem_Malloc(8, 0)`, so the size is
/// the allocation and not a guess. `field_0` is never touched.
typedef struct {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  r;
    /* 0x4 */ u16  g;
    /* 0x6 */ u16  b;
} RoomsShared8017ff5cWork;
STATIC_ASSERT_SIZEOF(RoomsShared8017ff5cWork, 0x8);

/// Fade a room up from white over the `Fade_DrawOverlay` screen tint.
///
/// State 0 allocates the block and saturates all three channels at 0xFF; a
/// failed allocation kills the task outright. State 1 runs every frame: it
/// draws the overlay tinted `r`/`g`/`r`, so only the green channel reads as a
/// colour and the blue one is stepped without ever being shown, then lowers all
/// three by `Task::spawnArg1` -- the fade rate, not a colour. Once `r`
/// underflows past 0 the screen is fully clear and the task kills itself.
///
/// Shared by the Dryfield water tower and the Dryfield warehouse. The fall is
/// signed on a `u16` channel, so the test is `(s16)r < 0` and not a bound: the
/// fade-out counterpart that ramps the same block the other way, from 0 up to
/// 0x100, stays private to each room.
void RoomsShared8017ff5c(Task* arg0)
{
    RoomsShared8017ff5cWork* work;
    RoomsShared8017ff5cWork* alloc;

    work = (RoomsShared8017ff5cWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (RoomsShared8017ff5cWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if ((s16)work->r < 0) {
                Task_Kill(arg0);
            }
            break;
    }
}
