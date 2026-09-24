#include "common.h"
#include <psyq/inline_c.h>

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/session.h"

#include "rooms/shelter_b4_upper_sewer.h"

extern u8   D_8007216C;
extern u8   D_shelter_b4_upper_sewer_80188D2C;
extern void func_shelter_b4_upper_sewer_8017E59C(s32);

extern s32                        D_8007107C;
extern s8                         D_8007217B;
extern s16                        D_shelter_b4_upper_sewer_80186438;
extern ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186448[];
extern ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186454[];
extern u8*                        D_shelter_b4_upper_sewer_80188D30;

void func_shelter_b4_upper_sewer_8017DD98(Task* task, ShelterB4UpperSewerSurface* e, s16 y, u8 c);

/// Water state of the room's water task: clamps the water level
/// `D_shelter_b4_upper_sewer_80186438` to -0x640..0, publishes it as the
/// session's water height, and draws the surfaces of the current camera view at
/// that height, in a blue that brightens as the level moves away from 0.
void func_shelter_b4_upper_sewer_8017DC88(Task* task)
{
    s16 w;
    u8  c;
    s32 h;

    if (D_8007217B == 0) {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    if (D_shelter_b4_upper_sewer_80186438 < -0x640) {
        D_shelter_b4_upper_sewer_80186438 = -0x640;
    } else if (D_shelter_b4_upper_sewer_80186438 > 0) {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    w                    = D_shelter_b4_upper_sewer_80186438;
    gGameSession->waterY = w;
    h                    = w;
    c                    = (-h * 16) / 225;
    if (gGameSession->at4.loc.view != 0xC) {
        func_shelter_b4_upper_sewer_8017DD98(task, D_shelter_b4_upper_sewer_80186448, h, c);
    } else {
        func_shelter_b4_upper_sewer_8017DD98(task, D_shelter_b4_upper_sewer_80186454, h, c);
    }
}

/// Draws each surface in `e` at height `y` as a strip of `count` flat
/// semi-transparent quads coloured (0, `c` / 4, `c`), projected through the
/// view matrix and each followed by a draw-mode packet selecting blend mode 2.
/// Quads are linked 0x60 deeper in the ordering table than their projected
/// depth, and those the projection flags as invalid are skipped. The
/// per-surface values live in a work block pushed on the scratchpad stack for
/// the duration of the call. `task` is unused.
void func_shelter_b4_upper_sewer_8017DD98(Task* task, ShelterB4UpperSewerSurface* e, s16 y, u8 c)
{
    SVECTOR                       v0, v1, v2, v3;
    s32                           sxy0, sxy1, sxy2, sxy3;
    s32                           p, flag;
    ShelterB4UpperSewerWaterWork* w;
    u8*                           head;
    POLY_F4*                      poly;
    DR_MODE*                      dr;
    s32                           otz;
    s32                           i;

    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB4UpperSewerWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = y;
    for (; e->count != -1; e++) {
        if (e->alongZ == 0) {
            w->dx = e->width / e->count;
            w->dz = e->depth;
            w->x  = e->x;
            w->z  = e->z;
            for (i = 0; i < e->count; i++) {
                v0.vx   = w->x + w->dx * i;
                v0.vy   = w->y;
                v0.vz   = w->z;
                v1.vx   = w->x + w->dx * (i + 1);
                v1.vy   = w->y;
                v1.vz   = w->z;
                w->wave = 0;
                v2.vx   = w->x + w->dx * i;
                v2.vy   = w->y + w->wave;
                v2.vz   = w->z + w->dz;
                w->wave = 0;
                v3.vx   = w->x + w->dx * (i + 1);
                v3.vy   = w->y + w->wave;
                v3.vz   = w->z + w->dz;
                otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    *(s32*)&poly->x0 = sxy0;
                    *(s32*)&poly->x1 = sxy1;
                    *(s32*)&poly->x2 = sxy2;
                    *(s32*)&poly->x3 = sxy3;
                    poly->r0         = 0;
                    poly->g0         = c >> 2;
                    poly->b0         = c;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            poly);
                    dr                                = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(dr + 1);
                    setlen(dr, 1);
                    dr->code[0] = 0xE100004A;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            dr);
                }
            }
        } else {
            w->dx = e->width;
            w->dz = e->depth / e->count;
            w->x  = e->x;
            w->z  = e->z;
            for (i = 0; i < e->count; i++) {
                v0.vx   = w->x;
                v0.vy   = w->y;
                v0.vz   = w->z + w->dz * i;
                v1.vx   = w->x;
                v1.vy   = w->y;
                v1.vz   = w->z + w->dz * (i + 1);
                w->wave = 0;
                v2.vx   = w->x + w->dx;
                v2.vy   = w->y + w->wave;
                v2.vz   = w->z + w->dz * i;
                w->wave = 0;
                v3.vx   = w->x + w->dx;
                v3.vy   = w->y + w->wave;
                v3.vz   = w->z + w->dz * (i + 1);
                otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    *(s32*)&poly->x0 = sxy0;
                    *(s32*)&poly->x1 = sxy1;
                    *(s32*)&poly->x2 = sxy2;
                    *(s32*)&poly->x3 = sxy3;
                    poly->r0         = 0;
                    poly->g0         = c >> 2;
                    poly->b0         = c;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            poly);
                    dr                                = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(dr + 1);
                    setlen(dr, 1);
                    dr->code[0] = 0xE100004A;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            dr);
                }
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// The room's water task: runs its state (`func_shelter_b4_upper_sewer_8017E55C`
/// once, then `func_shelter_b4_upper_sewer_8017DC88` every frame) and publishes
/// the water level as the session's water height.
void func_shelter_b4_upper_sewer_8017E4F4(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_upper_sewer_8017E55C, func_shelter_b4_upper_sewer_8017DC88 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_upper_sewer_80186438;
}
