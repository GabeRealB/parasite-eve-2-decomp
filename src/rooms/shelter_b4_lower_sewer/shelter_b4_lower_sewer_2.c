#include "common.h"
#include <psyq/inline_c.h>
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/shelter_b4_lower_sewer.h"

/// One water surface: its near edge starts at (`x`, `z`) and runs `step` along
/// X; its far edge sits `dz` further along Z. A surface whose `end` is -1
/// terminates the list.
typedef struct {
    s16 x;
    s16 z;
    s16 step;
    s16 dz;
    s16 end;
} _Surface;

/// Working copy of one surface's extents, carved off the scratchpad stack.
typedef struct {
    s16 y;
    s16 step;
    s16 dy;
    s16 dz;
    s16 x;
    s16 z;
} _SurfaceScratch;

extern s16      D_shelter_b4_lower_sewer_80181E6C;
extern _Surface D_shelter_b4_lower_sewer_80181E7C[];
extern _Surface D_shelter_b4_lower_sewer_80181E90[];

/// Draws each surface in `D_shelter_b4_lower_sewer_80181E7C` at height
/// `D_shelter_b4_lower_sewer_80181E6C` as two strips of 32 semi-transparent
/// Gouraud quads laid side by side along Z, each strip running along X. The
/// seam between the strips is lifted by a sine wave whose phase advances with
/// the frame counter. The outer edges are coloured (0, 0x20, 0x80) and the seam
/// (0x20, 0x20, 0x20); each quad is followed by a draw-mode packet selecting
/// blend mode 2. Quads the projection flags as invalid are skipped. Called
/// from the water task's drawing state with the task, which it does not read.
void func_shelter_b4_lower_sewer_8017D72C(Task* task)
{
    SVECTOR          v0, v1, v2, v3;
    s32              sxy0, sxy1, sxy2, sxy3;
    s32              p, flag;
    _Surface*        e;
    u8*              head;
    _SurfaceScratch* s;
    s32              phase;
    POLY_G4*         poly;
    DR_MODE*         dr;
    s32              otz;
    s32              i;

    e                 = D_shelter_b4_lower_sewer_80181E7C;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_lower_sewer_80181E6C;
    for (; e->end != -1; e++) {
        s->step = e->step / 32;
        s->dz   = e->dz / 2;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < 32; i++) {
            v0.vx = s->x + s->step * i;
            v0.vy = s->y;
            v0.vz = s->z;
            v1.vx = s->x + s->step * (i + 1);
            v1.vy = s->y;
            v1.vz = s->z;
            s->dy = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx = s->x + s->step * i;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->dz;
            s->dy = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx = s->x + s->step * (i + 1);
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->dz;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0;
                poly->g0         = 0x20;
                poly->b0         = 0x80;
                poly->r1         = 0;
                poly->g1         = 0x20;
                poly->b1         = 0x80;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        dr);
            }
        }
        for (i = 0; i < 32; i++) {
            s->dy = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx = s->x + s->step * i;
            v0.vy = s->y + s->dy;
            v0.vz = s->z + s->dz;
            s->dy = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx = s->x + s->step * (i + 1);
            v1.vy = s->y + s->dy;
            v1.vz = s->z + s->dz;
            v2.vx = s->x + s->step * i;
            v2.vy = s->y;
            v2.vz = s->z + s->dz * 2;
            v3.vx = s->x + s->step * (i + 1);
            v3.vy = s->y;
            v3.vz = s->z + s->dz * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0;
                poly->g2         = 0x20;
                poly->b2         = 0x80;
                poly->r3         = 0;
                poly->g3         = 0x20;
                poly->b3         = 0x80;
                poly->r0         = 0x20;
                poly->g0         = 0x20;
                poly->b0         = 0x20;
                poly->r1         = 0x20;
                poly->g1         = 0x20;
                poly->b1         = 0x20;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// Draws each surface in `D_shelter_b4_lower_sewer_80181E90` as a strip of 8
/// Gouraud semi-transparent quads laid along X at height
/// `D_shelter_b4_lower_sewer_80181E6C`. The far edge of each quad is lifted by
/// a sine wave whose phase advances with the frame counter, so the surface
/// ripples. Each quad is followed by a draw-mode packet selecting blend mode 2;
/// quads the projection flags as invalid are skipped. Called from the water
/// task's drawing state with the task, which it does not read.
void func_shelter_b4_lower_sewer_8017DE8C(Task* task)
{
    SVECTOR          v0, v1, v2, v3;
    s32              sxy0, sxy1, sxy2, sxy3;
    s32              p, flag;
    s32              phase;
    u8*              head;
    _SurfaceScratch* s;
    _Surface*        e;
    POLY_G4*         poly;
    DR_MODE*         dr;
    s32              otz;
    s32              i;

    e                 = D_shelter_b4_lower_sewer_80181E90;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_lower_sewer_80181E6C;
    for (; e->end != -1; e++) {
        s->step = e->step / 8;
        s->dz   = e->dz;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < 8; i++) {
            v0.vx = s->x + s->step * i;
            v0.vy = s->y;
            v0.vz = s->z;
            v1.vx = s->x + s->step * (i + 1);
            v1.vy = s->y;
            v1.vz = s->z;
            s->dy = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx = s->x + s->step * i;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->dz;
            s->dy = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx = s->x + s->step * (i + 1);
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->dz;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0;
                poly->g0         = 0x20;
                poly->b0         = 0x80;
                poly->r1         = 0;
                poly->g1         = 0x20;
                poly->b1         = 0x80;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// The room's water task: runs its state (`func_shelter_b4_lower_sewer_8017E33C`
/// once, then `func_shelter_b4_lower_sewer_8017E37C` every frame) and publishes
/// `D_shelter_b4_lower_sewer_80181E6C` as the session's water height.
void func_shelter_b4_lower_sewer_8017E2D4(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_lower_sewer_8017E33C, func_shelter_b4_lower_sewer_8017E37C };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_lower_sewer_80181E6C;
}
