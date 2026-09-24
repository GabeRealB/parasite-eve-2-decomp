#include "common.h"
#include <psyq/inline_c.h>

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/shelter_b2_septic_tank.h"

extern GpMsgEntry D_shelter_b2_septic_tank_80182F4C[];
extern TaskDesc   D_shelter_b2_septic_tank_801832C0[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank_2", func_shelter_b2_septic_tank_8017DB68);

/// Draws each surface in `D_shelter_b2_septic_tank_801832F0` at height
/// `D_shelter_b2_septic_tank_801832BC` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// In both strips the quad's lower-X edge is coloured (0, 0x40, 0x80) and its
/// upper-X edge (0, 0x10, 0x20); each quad is followed by a draw-mode packet
/// selecting blend mode 2. Quads the projection flags as invalid are skipped.
/// The per-surface values live in a work block pushed on the scratchpad stack
/// for the duration of the call.
void func_shelter_b2_septic_tank_8017E2DC(void)
{
    SVECTOR                       v0, v1, v2, v3;
    s32                           sxy0, sxy1, sxy2, sxy3;
    s32                           p, flag;
    s32                           phase;
    ShelterB2SepticTankSurface*   e;
    ShelterB2SepticTankWaterWork* w;
    u8*                           head;
    POLY_G4*                      poly;
    DR_MODE*                      dr;
    s32                           otz;
    s32                           i;

    e                 = D_shelter_b2_septic_tank_801832F0;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB2SepticTankWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b2_septic_tank_801832BC;
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
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
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
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank_2", func_shelter_b2_septic_tank_8017EA50);
