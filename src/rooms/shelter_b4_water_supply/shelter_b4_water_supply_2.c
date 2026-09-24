#include "common.h"
#include <psyq/inline_c.h>

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b4_water_supply.h"

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc   D_shelter_b4_water_supply_80182620[];
extern GpMsgEntry D_shelter_b4_water_supply_801825F0[];
extern TaskDesc   D_shelter_b4_water_supply_8018263C[];
void              func_shelter_b4_water_supply_8017DB18(void);

/// Message 0x20: unless a report-only query, answers in `field_3` from
/// nibbles 0x51 (1 when set, 2 when clear) and 0x53 (adds 2 when set).
/// Always returns 1 (not consumed).
s32 func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->msgId == 0x20 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    return 1;
}

/// Draws each surface in `D_shelter_b4_water_supply_80182648` at height
/// `D_shelter_b4_water_supply_80182638` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// The outer edges are coloured (0x80, 0, 0) and the seam (0x20, 0x20, 0x20);
/// each quad is followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. The per-surface values live in
/// a work block pushed on the scratchpad stack for the duration of the call.
/// `arg0` is unused.
void func_shelter_b4_water_supply_8017DE74(s32 arg0)
{
    SVECTOR                        v0, v1, v2, v3;
    s32                            sxy0, sxy1, sxy2, sxy3;
    s32                            p, flag;
    s32                            phase;
    ShelterB4WaterSupplySurface*   e;
    ShelterB4WaterSupplyWaterWork* w;
    u8*                            head;
    POLY_G4*                       poly;
    DR_MODE*                       dr;
    s32                            otz;
    s32                            i;

    e                 = D_shelter_b4_water_supply_80182648;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB4WaterSupplyWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b4_water_supply_80182638;
    for (; e->end != -1; e++) {
        w->dx = e->width / 2;
        w->dz = e->depth / 16;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x;
            v0.vy   = w->y;
            v0.vz   = w->z + w->dz * i;
            v1.vx   = w->x;
            v1.vy   = w->y;
            v1.vz   = w->z + w->dz * (i + 1);
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->r1         = 0x80;
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
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx   = w->x + w->dx;
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz * (i + 1);
            v2.vx   = w->x + w->dx * 2;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * i;
            v3.vx   = w->x + w->dx * 2;
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0x80;
                poly->r3         = 0x80;
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
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// Draws each surface in `D_shelter_b4_water_supply_8018265C` at height
/// `D_shelter_b4_water_supply_80182638` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along Z, projected through the view matrix.
/// The seam between the strips is lifted by a sine wave that runs along X and
/// scrolls with the display frame counter. The outer edges are coloured
/// (0x80, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. The per-surface values live in a work block pushed on
/// the scratchpad stack for the duration of the call. `arg0` is unused.
void func_shelter_b4_water_supply_8017E5D8(s32 arg0)
{
    SVECTOR                        v0, v1, v2, v3;
    s32                            sxy0, sxy1, sxy2, sxy3;
    s32                            p, flag;
    s32                            phase;
    ShelterB4WaterSupplySurface*   e;
    ShelterB4WaterSupplyWaterWork* w;
    u8*                            head;
    POLY_G4*                       poly;
    DR_MODE*                       dr;
    s32                            otz;
    s32                            i;

    e                 = D_shelter_b4_water_supply_8018265C;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB4WaterSupplyWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b4_water_supply_80182638;
    for (; e->end != -1; e++) {
        w->dx = e->width / 16;
        w->dz = e->depth / 2;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x + w->dx * i;
            v0.vy   = w->y;
            v0.vz   = w->z;
            v1.vx   = w->x + w->dx * (i + 1);
            v1.vy   = w->y;
            v1.vz   = w->z;
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx * i;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx * (i + 1);
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz;
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->r1         = 0x80;
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
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx * i;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx   = w->x + w->dx * (i + 1);
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz;
            v2.vx   = w->x + w->dx * i;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * 2;
            v3.vx   = w->x + w->dx * (i + 1);
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * 2;
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0x80;
                poly->r3         = 0x80;
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
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017ED28);
