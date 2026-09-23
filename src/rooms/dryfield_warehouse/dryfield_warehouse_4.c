#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_dryfield_warehouse_8017E414(GsCOORDINATE2* coord, s16 arg1);
void func_dryfield_warehouse_8017ED34(GsCOORDINATE2* coord, s16 arg1, s16 arg2);

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
/// `mvmva` rotating V0 by the rotation matrix with no translation.
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Scratch block one quad is built in: the GTE depth of its last three
/// corners, then the four corners after they are placed in world space.
typedef struct _DryfieldWarehouseBandScratch {
    s32     otz;
    SVECTOR v[4];
} _DryfieldWarehouseBandScratch;

/// Points in the space of the coordinate drawn under: ring centres, one per
/// circle, for the ring drawer, and prism corners for the prism drawer, which
/// the room's only caller points at `[8..15]`.
extern SVECTOR D_dryfield_warehouse_8017FB2C[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_warehouse_8017FBAC[];

/// Draws one prism from `D_dryfield_warehouse_8017FB2C[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring `[4..7]`,
/// then a cap over the lit ring. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The lit
/// corners share a grey of 0x18 plus a small pulse; the far corners are black.
void func_dryfield_warehouse_8017E414(GsCOORDINATE2* coord, s16 arg1)
{
    _DryfieldWarehouseBandScratch* blk;
    POLY_G4*                       prim;
    s32                            i;
    s32                            next;
    s32                            far;
    s32                            farNext;
    u8                             shade;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(_DryfieldWarehouseBandScratch);
    blk                     = (_DryfieldWarehouseBandScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + i]);
        gte_mvmva_real();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
        blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
        blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + next]);
        gte_mvmva_real();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
        blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
        blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + far]);
        gte_mvmva_real();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
        blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
        blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + farNext]);
        gte_mvmva_real();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
        blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
        blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
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
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
    blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
    blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
    blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
    blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 3]);
    gte_mvmva_real();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
    blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
    blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 2]);
    gte_mvmva_real();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
    blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
    blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
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
    setRGB0(prim, shade, shade, shade);
    setRGB1(prim, shade, shade, shade);
    setRGB2(prim, shade, shade, shade);
    setRGB3(prim, shade, shade, shade);
    addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(_DryfieldWarehouseBandScratch);
}

/// Draws one ring of gouraud `POLY_G4` segments between two circles in the XZ
/// plane of `coord`: circle `arg1` of the room's centre/radius tables forms the
/// lit edge and circle `arg1 + 1` the black one. `arg2` segments cover the full
/// turn, starting at a phase that advances with the frame counter. Each corner
/// is placed in `coord`'s space through its `workm`, then projected through
/// `GsWSMATRIX`; the lit edge glows at 0x14 plus a small pulse.
void func_dryfield_warehouse_8017ED34(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    _DryfieldWarehouseBandScratch* blk;
    POLY_G4*                       prim;
    s16                            level;
    s16                            step;
    s16                            start;
    s32                            angle;
    s32                            next;

    level                  = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x14;
    *(u8**)G_SCRATCH_HEAD -= sizeof(_DryfieldWarehouseBandScratch);
    blk                    = *(_DryfieldWarehouseBandScratch**)G_SCRATCH_HEAD;
    start                  = gDisplayState.animFrame & 0xFFF;
    step                   = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->v[0].vx = D_dryfield_warehouse_8017FB2C[arg1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->v[0].vy = D_dryfield_warehouse_8017FB2C[arg1].vy;
        blk->v[0].vz = D_dryfield_warehouse_8017FB2C[arg1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[0]);
        gte_mvmva_real();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        next          = angle + step;
        blk->v[0].vz += coord->workm.t[2];

        blk->v[1].vx = D_dryfield_warehouse_8017FB2C[arg1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->v[1].vy = D_dryfield_warehouse_8017FB2C[arg1].vy;
        blk->v[1].vz = D_dryfield_warehouse_8017FB2C[arg1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[1]);
        gte_mvmva_real();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];

        blk->v[2].vx = D_dryfield_warehouse_8017FB2C[arg1 + 1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->v[2].vy = D_dryfield_warehouse_8017FB2C[arg1 + 1].vy;
        blk->v[2].vz = D_dryfield_warehouse_8017FB2C[arg1 + 1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[2]);
        gte_mvmva_real();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];

        blk->v[3].vx = D_dryfield_warehouse_8017FB2C[arg1 + 1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->v[3].vy = D_dryfield_warehouse_8017FB2C[arg1 + 1].vy;
        blk->v[3].vz = D_dryfield_warehouse_8017FB2C[arg1 + 1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
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
        setRGB0(prim, level, level, level);
        setRGB1(prim, level, level, level);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(_DryfieldWarehouseBandScratch);
}

/// Per-frame effect on the room's model task: re-poses the model for the
/// current stage visit, then publishes variant 2 as the room's
/// `Gp_State1C->roomEffectMode` index. `Task::extra` is the task's `TmdObject`, so
/// `field_8` is the coordinate every pose shares. The stage-visit byte
/// `gGameSession->at4.loc.view` is used as a bit index: bits 2, 3, 6 and 9 (`0x24C`)
/// pose through `func_dryfield_warehouse_8017E414`, bit 2 (`4`) also drives
/// `func_dryfield_warehouse_8017ED34` to step 0, those same `0x24C` visits also
/// drive it to step 2, and bits 2, 3, 4 and 6-9 (`0x3DC`) drive it to steps 4
/// and 6.
void func_dryfield_warehouse_8017F494(Task* arg0)
{
    s32            mask;
    s32            poseMask;
    GsCOORDINATE2* coord;

    mask     = 1 << gGameSession->at4.loc.view;
    poseMask = mask & 0x24C;
    coord    = ((TmdObject*)arg0->extra)->coords;
    if (poseMask != 0) {
        func_dryfield_warehouse_8017E414(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_warehouse_8017ED34(coord, 0, 8);
    }
    if (poseMask != 0) {
        func_dryfield_warehouse_8017ED34(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_warehouse_8017ED34(coord, 4, 8);
        func_dryfield_warehouse_8017ED34(coord, 6, 8);
    }
    Gp_State1C->roomEffectMode = 2;
}
