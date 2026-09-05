#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"

#include <psyq/libgpu.h>

/// Colour ramp the task keeps at `Task::idMap` -- that slot is not a
/// `TaskIdMap` here. The allocation below is `Mem_Malloc(8, 0)`, so the size is
/// the allocation and not a guess. All three channels start saturated and step
/// down together, but the tile takes its blue from `r`, so `b` is only ever
/// stepped and never read.
typedef struct {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ s16  r;
    /* 0x4 */ s16  g;
    /* 0x6 */ s16  b;
} RoomsShared8017da58Work;
STATIC_ASSERT_SIZEOF(RoomsShared8017da58Work, 0x8);

/// Fade a room up from white, then tear the task down.
///
/// State 0 allocates the ramp and saturates all three channels at 0xFF; a
/// failed allocation kills the task outright. State 1 runs every frame: it
/// links a semi-transparent full-screen `TILE` (`-0xA0,-0x78`, `0x140x0xF0`)
/// plus the `0xE1000240` `DR_TPAGE` into `Gpu_CurrentOt[-16]`, tinting the tile
/// `r`/`g`/`r`, then steps all three channels down by `Task::spawnArg1`. Once
/// `r` underflows past 0 the screen is fully clear, so the task kills itself.
/// Shared by the Acropolis Plaza and the Dryfield water tank; the fade-out
/// counterpart stays private to each room.
void RoomsShared8017da58(Task* arg0)
{
    RoomsShared8017da58Work* fade;
    RoomsShared8017da58Work* alloc;
    u8                       r;
    u8                       g;
    TILE*                    tile;
    DR_TPAGE*                dr;

    fade = (RoomsShared8017da58Work*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (RoomsShared8017da58Work*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = (TILE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(Gpu_CurrentOt - 16, tile);

            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(Gpu_CurrentOt - 16, dr);

            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
            kill:
                Task_Kill(arg0);
            }
            break;
    }
}
