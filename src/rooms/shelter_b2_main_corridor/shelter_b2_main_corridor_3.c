#include "common.h"
#include <psyq/inline_c.h>

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/shelter_b2_main_corridor.h"

extern GpMsgEntry D_shelter_b2_main_corridor_80182C14[];
extern TaskDesc   D_shelter_b2_main_corridor_80182DE0[];
extern s32        D_8007107C;
extern s8         D_8007217B;

/// Draws each surface in `D_shelter_b2_main_corridor_80182DEC` at height
/// `D_shelter_b2_main_corridor_80182E28` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// The outer edges are coloured (0x80, 0, 0) and the seam (0x20, 0x20, 0x20);
/// each quad is followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. The primitive cursor is reset
/// to the current buffer's half of the primitive area first, and nothing is
/// drawn in views 10 and 11 of stage 4, area 0x21. The per-surface values live
/// in a work block pushed on the scratchpad stack for the duration of the call.
void func_shelter_b2_main_corridor_8017E390(void)
{
    SVECTOR                         v0, v1, v2, v3;
    s32                             sxy0, sxy1, sxy2, sxy3;
    s32                             p, flag;
    s32                             phase;
    ShelterB2MainCorridorSurface*   e;
    ShelterB2MainCorridorWaterWork* w;
    u8*                             head;
    POLY_G4*                        poly;
    DR_MODE*                        dr;
    s32                             otz;
    s32                             i;
    GpAreaKey*                      k;

    e     = D_shelter_b2_main_corridor_80182DEC;
    phase = -(gDisplayState.animFrame * 16);
    k     = &gGameSession->at4.loc;
    if (k->stage == 4) {
        if (k->area == 0x21) {
            if ((u32)(gGameSession->at4.loc.view - 0xA) < 2) {
                return;
            }
        }
    }
    if (D_8007217B == 0) {
        D_shelter_b2_main_corridor_80189660 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b2_main_corridor_80189660 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB2MainCorridorWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b2_main_corridor_80182E28;
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
                poly                                = (POLY_G4*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(poly + 1);
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
                dr                                  = (DR_MODE*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(dr + 1);
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
                poly                                = (POLY_G4*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(poly + 1);
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
                dr                                  = (DR_MODE*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// The water task: runs its state, first
/// `func_shelter_b2_main_corridor_8017EBF4` and then the surface drawer above,
/// and each tick publishes the room's water height to the session.
void func_shelter_b2_main_corridor_8017EB8C(Task* task)
{
    TaskFunc states[2] = { func_shelter_b2_main_corridor_8017EBF4, func_shelter_b2_main_corridor_8017E390 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b2_main_corridor_80182E28;
}
