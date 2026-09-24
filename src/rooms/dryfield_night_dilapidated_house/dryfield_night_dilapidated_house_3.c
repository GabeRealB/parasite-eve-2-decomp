#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
/// `mvmva` rotating V0 by the rotation matrix with no translation.
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Scratch block one quad is built in: the GTE depth of its last three
/// corners, then the four corners after they are placed in world space.
typedef struct _DryfieldNightDilapidatedHouseBandScratch {
    s32     otz;
    SVECTOR v[4];
} _DryfieldNightDilapidatedHouseBandScratch;

/// The prism corners, eight per prism: a lit ring of four, then the far ring.
extern SVECTOR D_dryfield_night_dilapidated_house_801872CC[];

/// Draws a four-sided prism from the eight corners at
/// `D_dryfield_night_dilapidated_house_801872CC[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring
/// `[4..7]`, then a cap over the lit ring. Each corner is rotated by `coord`'s
/// `workm` and moved by its translation before projection through
/// `GsWSMATRIX`. The lit corners share a pulsing colour whose red is three
/// quarters of its green and blue; the far corners are black.
void func_dryfield_night_dilapidated_house_8017DD30(GsCOORDINATE2* coord, s16 arg1)
{
    _DryfieldNightDilapidatedHouseBandScratch* blk;
    POLY_G4*                                   prim;
    s32                                        i;
    s32                                        next;
    s32                                        far;
    s32                                        farNext;
    s16                                        pulse;
    s16                                        red;
    s16                                        blue;
    s16                                        green;

    pulse                  = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    *(u8**)G_SCRATCH_HEAD -= sizeof(_DryfieldNightDilapidatedHouseBandScratch);
    blk                    = *(_DryfieldNightDilapidatedHouseBandScratch**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + i]);
        gte_mvmva_real();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        blk->v[0].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + next]);
        gte_mvmva_real();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + far]);
        gte_mvmva_real();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + farNext]);
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
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx += coord->workm.t[0];
    blk->v[0].vy += coord->workm.t[1];
    blk->v[0].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx += coord->workm.t[0];
    blk->v[1].vy += coord->workm.t[1];
    blk->v[1].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 3]);
    gte_mvmva_real();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx += coord->workm.t[0];
    blk->v[2].vy += coord->workm.t[1];
    blk->v[2].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 2]);
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
    setRGB2(prim, red, green, blue);
    setRGB3(prim, red, green, blue);
    addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    *(u8**)G_SCRATCH_HEAD += sizeof(_DryfieldNightDilapidatedHouseBandScratch);
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix and then re-poses three of its parts. The current visit is the
/// stage-visit byte `gGameSession->at4.loc.view` taken as a bit index, and each pose
/// is gated on that bit being one of a fixed set of visits; the second set is
/// contained in the other two, so a visit in it runs all three poses.
void func_dryfield_night_dilapidated_house_8017E670(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x99C) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0);
    }
    if (mask & 0x998) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 8);
    }
    if (mask & 0x9F8) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0x10);
    }
}
