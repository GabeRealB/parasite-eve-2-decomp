#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "rooms/room_common.h"

#include "gameplay/display.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc. Each
/// is reached by its own address, so the compiler materialises it whole into
/// `$a0` rather than indexing one base.
extern SVECTOR D_dryfield_night_motel_room_3_8017DA84;
extern SVECTOR D_dryfield_night_motel_room_3_8017DA8C;
extern SVECTOR D_dryfield_night_motel_room_3_8017DA94;

/// Queues a flickering disc at the world point `arg0`: a semi-transparent
/// `POLY_FT4` square centred on the point's projection, with half-width
/// `arg2 * 39 / otz`, textured from the 40-texel cell `arg1` of tpage 0x2B
/// and shaded 0x20 or 0x30 on alternate frames. Points closer than OTZ 0x11
/// are skipped.
static void func_dryfield_night_motel_room_3_8017D738(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw25Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz > 0x10) {
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// Per-frame effect: queues the room's glowing discs for the visit
/// `gGameSession->at4.loc.view` selects - visit 3 the second point, visit 4 the
/// second point then the first, visits 10 and 11 the first alone, visit 8 the
/// third with a wider UV column and half-extent. Visits outside those draw
/// nothing.
static void func_dryfield_night_motel_room_3_8017D9B4(void)
{
    switch (gGameSession->at4.loc.view) {
        case 3:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA8C, 1, 0x240);
            break;
        case 4:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA8C, 1, 0x240);
            /* fallthrough */
        case 10:
        case 11:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA84, 1, 0x200);
            break;
        case 8:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA94, 2, 0x180);
            break;
    }
}
