#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
/// `mvmva` rotating V0 by the rotation matrix with no translation.
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Scratch block one face is built in: the GTE depth of its last three
/// corners, then the four corners after they are placed in world space.
typedef struct _DssPrismScratch {
    s32     otz;
    SVECTOR v[4];
} _DssPrismScratch;

/// Prism corners in the space of the coordinate drawn under, eight per prism:
/// the lit ring, then the far ring. The room draws the prisms at `[0..7]` and
/// `[8..15]`.
extern SVECTOR D_dryfield_souvenir_shop_8017E03C[];

/// Draws one prism from `D_dryfield_souvenir_shop_8017E03C[arg1..]` as five
/// gouraud `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring
/// `[4..7]`, then a cap over the lit ring. Each corner is rotated by `coord`'s
/// `workm` and moved by its translation before projection through
/// `GsWSMATRIX`. The lit corners share a grey of 0x18 plus a small pulse; the
/// far corners are black.
void func_dryfield_souvenir_shop_8017D6B4(GsCOORDINATE2* coord, s16 arg1)
{
    _DssPrismScratch* blk;
    POLY_G4*          prim;
    s32               i;
    s32               next;
    s32               far;
    s32               farNext;
    u8                shade;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(_DssPrismScratch);
    blk                     = (_DssPrismScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + i]);
        gte_mvmva_real();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
        blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
        blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + next]);
        gte_mvmva_real();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
        blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
        blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + far]);
        gte_mvmva_real();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
        blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
        blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + farNext]);
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
    gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
    blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
    blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + 1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
    blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
    blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + 3]);
    gte_mvmva_real();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
    blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
    blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_souvenir_shop_8017E03C[arg1 + 2]);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(_DssPrismScratch);
}

/// Per-frame effect on the room's model task: `Task::extra` is the task's
/// `TmdObject`, and both of the room's prisms are drawn under its first
/// coordinate.
void func_dryfield_souvenir_shop_8017DFD4(Task* task)
{
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;

    func_dryfield_souvenir_shop_8017D6B4(coord, 0);
    func_dryfield_souvenir_shop_8017D6B4(coord, 8);
}
