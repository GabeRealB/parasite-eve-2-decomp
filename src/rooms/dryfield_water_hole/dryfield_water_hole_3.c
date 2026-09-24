#include "common.h"
#include <psyq/inline_c.h>
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_water_hole.h"

extern s8 D_8007217B;
extern u8 D_801153F4;

/// Draws each surface in `D_dryfield_water_hole_8017FC98` as two strips of 64
/// semi-transparent Gouraud quads laid side by side along Z, projected through
/// the view matrix. The seam between the strips is lifted by a sine wave that
/// runs along X and scrolls with `D_dryfield_water_hole_801828D0`, which only
/// advances while `D_801153F4` is clear. The outer edges are coloured
/// (0xFF, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. `task` is unused.
void func_dryfield_water_hole_8017D898(Task* task)
{
    SVECTOR                   v0, v1, v2, v3;
    s32                       sxy0, sxy1, sxy2, sxy3;
    s32                       p, flag;
    s32                       step;
    s32                       phase;
    DryfieldWaterHoleSurface* e;
    POLY_G4*                  poly;
    DR_MODE*                  dr;
    s32                       otz;
    s32                       i;
    s32                       half;
    s32                       wave;

    e = D_dryfield_water_hole_8017FC98;
    if (D_8007217B == 0) {
        D_dryfield_water_hole_801828CC = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_dryfield_water_hole_801828CC = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (D_801153F4 == 0) {
        D_dryfield_water_hole_801828D0++;
    }
    phase             = -(D_dryfield_water_hole_801828D0 * 16);
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    for (; e->y != -1; e++) {
        step = e->width / 64;
        half = (s16)e->depth / 2;
        for (i = 0; i < 64; i++) {
            v0.vx = e->x + step * i;
            v0.vy = e->y;
            v0.vz = e->z;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y;
            v1.vz = e->z;
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v2.vx = e->x + step * i;
            v2.vy = e->y + wave;
            v2.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y + wave;
            v3.vz = e->z + half;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                           = (POLY_G4*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0xFF;
                poly->r1         = 0xFF;
                poly->g0         = 0;
                poly->b0         = 0;
                poly->g1         = 0;
                poly->b1         = 0;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                             = (DR_MODE*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 64; i++) {
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v0.vx = e->x + step * i;
            v0.vy = e->y + wave;
            v0.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y + wave;
            v1.vz = e->z + half;
            v2.vx = e->x + step * i;
            v2.vy = e->y;
            v2.vz = e->z + half * 2;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y;
            v3.vz = e->z + half * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                           = (POLY_G4*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0xFF;
                poly->r3         = 0xFF;
                poly->g2         = 0;
                poly->b2         = 0;
                poly->g3         = 0;
                poly->b3         = 0;
                poly->r0         = 0x20;
                poly->g0         = 0x20;
                poly->b0         = 0x20;
                poly->r1         = 0x20;
                poly->g1         = 0x20;
                poly->b1         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                             = (DR_MODE*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
}

/// The room's water task: runs its current state -
/// `func_dryfield_water_hole_8017E000` once, then
/// `func_dryfield_water_hole_8017D898`, which draws the surfaces - and each
/// tick sets the session's water height to -0x1A4.
void func_dryfield_water_hole_8017DFA0(Task* task)
{
    TaskFunc states[2] = { func_dryfield_water_hole_8017E000, func_dryfield_water_hole_8017D898 };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}
