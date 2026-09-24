#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_garden.h"
#include "rooms/room_common.h"

/// `rtps` / `rtpt` / `mvmva`. The `inline_c.h` macros of those names assemble
/// to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 D_80070F70;
extern s32 D_80115730;
extern s32 Gp_LcgState;

/// Per-channel right shifts turning a glow's brightness into its colour,
/// indexed by the tint in `Task::spawnArg1`.
extern s16 D_neo_ark_garden_80181400[][3];

void func_neo_ark_garden_8017F42C(SVECTOR* arg0)
{
    MATRIX         m;
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;

    Gfx_RotMatrixX(&m, D_80070F70 << 7, 1);
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &m;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - 0x38;
    SOFT_TOUCH_REG(head);
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
    do {
        v->vx = 0;
        TOUCH_REG(v);
        v->vy = (s16)tbl->x * 250;
        TOUCH_REG(v);
        v->vz = (s16)tbl->y * 250;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->vx;
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->vy;
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->vz;
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2D);
        prim->tpage = 0xAC;
        prim->clut  = 0x43C0;
        prim->u0    = 0;
        prim->v0    = 0;
        prim->u1    = 0x3F;
        prim->v1    = 0;
        prim->u2    = 0;
        prim->v2    = 0x3F;
        prim->u3    = 0x3F;
        prim->v3    = 0x3F;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

/// Glow effect task. It does nothing while the event state is 1 to 3 and
/// releases its work block once it reaches 4. State 0 parents the coordinate to `GpEffWork::parent`
/// with no rotation at `GpEffWork::pos`. States 1 and 2 grow the brightness
/// to 0xC0 and the size to 0x200 and draw a glow disc tinted through
/// `D_neo_ark_garden_80181400`; state 1 also spawns effect `D_80115730` on a
/// random bone of the player every fourth tick, and state 2 adds a larger
/// half-bright disc on odd ticks. State 3 drifts the coordinate away along a
/// rotated step, draws an expanding ring and fades the glow, releasing the
/// work block once it has faded. State 4 releases it at once.
void func_neo_ark_garden_8017F790(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_neo_ark_garden_801805B4(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_neo_ark_garden_80180190(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}
