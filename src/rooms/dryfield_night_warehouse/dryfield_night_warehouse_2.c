#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_dryfield_night_warehouse_8017D6B4(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_night_warehouse_8017DFF4(GsCOORDINATE2* coord, s16 arg1, s16 arg2);

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
/// `mvmva` rotating V0 by the rotation matrix with no translation.
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Scratch block one quad is built in: the GTE depth of its last three
/// corners, then the four corners after they are placed in world space.
typedef struct _DryfieldNightWarehouseBandScratch {
    s32     otz;
    SVECTOR v[4];
} _DryfieldNightWarehouseBandScratch;

/// Ring centres in the space of the coordinate drawn under, one per circle.
extern SVECTOR D_dryfield_night_warehouse_8017E858[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_night_warehouse_8017E8D8[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_warehouse/dryfield_night_warehouse_2", func_dryfield_night_warehouse_8017D6B4);

/// Draws the band joining ring `arg1` to ring `arg1 + 1` as `arg2` gouraud
/// `POLY_G4` segments, starting at an angle that turns with
/// `gDisplayState.animFrame`. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The corners
/// on ring `arg1` share a pulsing colour whose red is three quarters of its
/// green and blue; the corners on ring `arg1 + 1` are black.
void func_dryfield_night_warehouse_8017DFF4(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    _DryfieldNightWarehouseBandScratch* blk;
    POLY_G4*                            prim;
    s16                                 red;
    s16                                 blue;
    s16                                 green;
    s16                                 step;
    s16                                 start;
    s16                                 pulse;
    s32                                 angle;
    s32                                 next;

    pulse                  = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    *(u8**)G_SCRATCH_HEAD -= sizeof(_DryfieldNightWarehouseBandScratch);
    blk                    = *(_DryfieldNightWarehouseBandScratch**)G_SCRATCH_HEAD;
    start                  = gDisplayState.animFrame & 0xFFF;
    step                   = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->v[0].vx = D_dryfield_night_warehouse_8017E858[arg1].vx +
                       ((rsin(angle) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        blk->v[0].vy = D_dryfield_night_warehouse_8017E858[arg1].vy;
        blk->v[0].vz = D_dryfield_night_warehouse_8017E858[arg1].vz +
                       ((rcos(angle) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[0]);
        gte_mvmva_real();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        next          = angle + step;
        blk->v[0].vz += coord->workm.t[2];

        blk->v[1].vx = D_dryfield_night_warehouse_8017E858[arg1].vx +
                       ((rsin(next) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        blk->v[1].vy = D_dryfield_night_warehouse_8017E858[arg1].vy;
        blk->v[1].vz = D_dryfield_night_warehouse_8017E858[arg1].vz +
                       ((rcos(next) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[1]);
        gte_mvmva_real();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];

        blk->v[2].vx = D_dryfield_night_warehouse_8017E858[arg1 + 1].vx +
                       ((rsin(angle) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        blk->v[2].vy = D_dryfield_night_warehouse_8017E858[arg1 + 1].vy;
        blk->v[2].vz = D_dryfield_night_warehouse_8017E858[arg1 + 1].vz +
                       ((rcos(angle) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[2]);
        gte_mvmva_real();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];

        blk->v[3].vx = D_dryfield_night_warehouse_8017E858[arg1 + 1].vx +
                       ((rsin(next) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        blk->v[3].vy = D_dryfield_night_warehouse_8017E858[arg1 + 1].vy;
        blk->v[3].vz = D_dryfield_night_warehouse_8017E858[arg1 + 1].vz +
                       ((rcos(next) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[3]);
        gte_mvmva_real();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps_real();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(_DryfieldNightWarehouseBandScratch);
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix and then re-poses it. The current visit is the stage-visit byte
/// `gGameSession->at4.loc.view` taken as a bit index, and each pose is gated on that
/// bit being one of a fixed set of visits.
void func_dryfield_night_warehouse_8017E778(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017D6B4(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_night_warehouse_8017DFF4(coord, 0, 8);
    }
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017DFF4(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_night_warehouse_8017DFF4(coord, 4, 8);
        func_dryfield_night_warehouse_8017DFF4(coord, 6, 8);
    }
}
