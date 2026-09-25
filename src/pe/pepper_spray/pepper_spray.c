#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/pepper_spray.h"

extern s8 D_80114C0B;

/// Runs one frame of the pepper spray. State 0 parks the room light slot on
/// the nozzle coordinate, seeds the spray yaw / spread / brightness from
/// `Gp_LcgState`, refills the six cone yaws and plays the spray sound; state 1
/// just decays the yaw and the brightness by a sixteenth each, scaled by how
/// long the spray has run. Either state then redraws the nozzle, flashes the
/// screen at the current brightness and draws the six cone quads. The effect
/// ends after nine frames, or immediately if the player is dying
/// (`D_80114C0B`) or the room is fading (`Gp_State1C`).
/// Scratch the nozzle spray walks while the cloud is alive.
/// lists an object in the linker script at its first subsegment, and this has
s16 D_pepper_spray_8012FB9C[6] = { 0, 0, 0, 0, 0, 0 };

void func_pepper_spray_8012EF34(Task* arg0)
{
    GpEffWork*    mem;
    GpCoord*      coord;
    GpCoord64*    base;
    GpPointLight* slot;
    s32           i;
    s32           age;
    s32           tz;
    s32           yaw;
    s32           spread;
    s32           pan;
    u8            rgb[3];

    base  = Gp_RoomCoords;
    slot  = &base->data.light;
    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if ((D_80114C0B == -2) || (Gp_State1C->fadeState != 0)) {
        SndEvt_EnqueueType7(0xE03F0001, 1);
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    age      = (u16)mem->age + 1;
    mem->age = age;
    switch (arg0->state) {
        case 0:
            slot->head.u.coord.coord.t[0] = coord->coord.t[0];
            Gp_LcgState                   = Gp_LcgState * 5 + 0x71357911;
            yaw                           = (((u32)Gp_LcgState >> 16) & 0x3FF) + 0xA00;
            slot->head.u.coord.coord.t[1] = coord->coord.t[1];
            Gp_LcgState                   = Gp_LcgState * 5 + 0x71357911;
            spread                        = ((u32)Gp_LcgState >> 16) & 0xFFF;
            tz                            = coord->coord.t[2];
            base->data.coord.flg          = 0;
            slot->head.r                  = 0x1000;
            slot->head.g                  = 0x1000;
            slot->head.b                  = 0x1000;
            slot->inner                   = 0xFA0;
            slot->outer                   = 0x12C0;
            base->framesLeft              = 6;
            slot->head.u.coord.coord.t[2] = tz;
            mem->period                   = 0xE0;
            mem->scale                    = yaw;
            mem->angle                    = spread;
            arg0->state                   = 1;
            for (i = 0; i < 6; i++) {
                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                D_pepper_spray_8012FB9C[i] = ((i & 3) << 10) + (((u32)Gp_LcgState >> 16) & 0x3FF);
            }
            Gp_StateC08.field_6 |= 8;
            pan                  = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0xE03F0001, pan, (s8)gpGetObjDepth(coord));
            break;
        case 1:
            mem->scale  = mem->scale - age * (mem->scale >> 4);
            mem->period = mem->period - mem->age * (mem->period >> 4);
            break;
    }
    func_pepper_spray_8012F21C(coord, mem->scale, mem->angle);
    rgb[0] = rgb[1] = rgb[2] = mem->period;
    Gp_DrawFadeQuad(rgb, 1);
    for (i = 0; i < 6; i++) {
        func_pepper_spray_8012F634(coord, D_pepper_spray_8012FB9C[i], mem->period);
    }
    if (slot->inner >= 0x191) {
        slot->inner -= 0x190;
    }
    if (mem->age >= 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_pepper_spray_8012F21C(GpCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                head;
    GpEffFlareScratch* blk;
    GpEffFlareScratch* copy;
    POLY_FT4*          prim;
    s32                ang;

    head                            = SCRATCH_HEAD(u8);
    blk                             = (GpEffFlareScratch*)(head - 0x1C);
    copy                            = blk;
    blk->vec.vx                     = *(u16*)&arg0->workm.t[0];
    blk->vec.vy                     = *(u16*)&arg0->workm.t[1];
    blk->vec.vz                     = *(u16*)&arg0->workm.t[2];
    SCRATCH_HEAD(GpEffFlareScratch) = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((GpEffFlareScratch*)(head - 0x1C))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&((GpEffFlareScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpEffFlareScratch*)(head - 0x1C))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(copy);
        ((GpEffFlareScratch*)(head - 0x1C))->otz++;
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        prim->code |= 3;
        ang         = arg2;
        blk->dx     = (((arg1 * 0x37) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(ang)) >> 12;
        blk->dy     = (((arg1 * 0x37) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
        prim->x0    = blk->sx + *(u16*)&blk->dx;
        prim->x3    = blk->sx - *(u16*)&blk->dx;
        prim->y0    = blk->sy - *(u16*)&blk->dy;
        prim->y3    = blk->sy + *(u16*)&blk->dy;
        ang         = ang + 0x400;
        blk->dx     = (((arg1 * 0x37) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(ang)) >> 12;
        blk->dy     = (((arg1 * 0x37) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
        prim->x1    = blk->sx + *(u16*)&blk->dx;
        prim->x2    = blk->sx - *(u16*)&blk->dx;
        prim->y1    = blk->sy - *(u16*)&blk->dy;
        prim->y2    = blk->sy + *(u16*)&blk->dy;
        addPrim((u_long*)(((((u32)((GpEffFlareScratch*)(head - 0x1C))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/* Every scratch vector address is computed off `head`, not off `blk`, so the
   loads and stores keep spelling the block out from `head` rather than reusing
   the `blk` register the way CSE off `blk` would. */
void func_pepper_spray_8012F634(GpCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                        head;
    OverlayFlaggedQuadScratch* blk;
    OverlayFlaggedQuadScratch* copy;
    POLY_G4*                   prim;
    MATRIX*                    wm;
    s32                        ang;
    s32                        back;
    s32                        depth;
    s16                        color;

    depth                                   = -0x200;
    head                                    = SCRATCH_HEAD(u8);
    blk                                     = (OverlayFlaggedQuadScratch*)(head - sizeof(OverlayFlaggedQuadScratch));
    SCRATCH_HEAD(OverlayFlaggedQuadScratch) = blk;
    copy                                    = blk;
    color                                   = arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    ang = arg1;

    back         = ang - 0xC0;
    blk->v[0].vx = (u32)rsin(back) >> 4;
    blk->v[0].vy = (u32)rcos(back) >> 4;
    blk->v[0].vz = 0;
    wm           = &arg0->workm;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[0]);
    gte_rtv0();
    gte_stsv(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[0]);
    (u16) blk->v[0].vx = (u16)blk->v[0].vx + *(u16*)&arg0->workm.t[0];
    (u16) blk->v[0].vy = (u16)blk->v[0].vy + *(u16*)&arg0->workm.t[1];
    (u16) blk->v[0].vz = (u16)blk->v[0].vz + *(u16*)&arg0->workm.t[2];

    blk->v[1].vx = (u32)rsin(ang) >> 1;
    blk->v[1].vy = (u32)rcos(ang) >> 1;
    blk->v[1].vz = depth;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[1]);
    gte_rtv0();
    gte_stsv(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[1]);
    (u16) blk->v[1].vx = (u16)blk->v[1].vx + *(u16*)&arg0->workm.t[0];
    (u16) blk->v[1].vy = (u16)blk->v[1].vy + *(u16*)&arg0->workm.t[1];
    (u16) blk->v[1].vz = (u16)blk->v[1].vz + *(u16*)&arg0->workm.t[2];

    blk->v[2].vx = (u32)rsin(ang) >> 4;
    blk->v[2].vy = (u32)rcos(ang) >> 4;
    blk->v[2].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[2]);
    gte_rtv0();
    gte_stsv(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[2]);
    (u16) blk->v[2].vx = (u16)blk->v[2].vx + *(u16*)&arg0->workm.t[0];
    ang                = ang + 0xC0;
    (u16) blk->v[2].vy = (u16)blk->v[2].vy + *(u16*)&arg0->workm.t[1];
    (u16) blk->v[2].vz = (u16)blk->v[2].vz + *(u16*)&arg0->workm.t[2];

    blk->v[3].vx = (u32)rsin(ang) >> 4;
    blk->v[3].vy = (u32)rcos(ang) >> 4;
    blk->v[3].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[3]);
    gte_rtv0();
    gte_stsv(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[3]);
    (u16) blk->v[3].vx = (u16)blk->v[3].vx + *(u16*)&arg0->workm.t[0];
    (u16) blk->v[3].vy = (u16)blk->v[3].vy + *(u16*)&arg0->workm.t[1];
    (u16) blk->v[3].vz = (u16)blk->v[3].vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_stflg(&((OverlayFlaggedQuadScratch*)(head - 0x28))->flag);
    if (blk->flag >= 0) {
        gte_ldv3(&((OverlayFlaggedQuadScratch*)(head - 0x28))->v[1],
                 &((OverlayFlaggedQuadScratch*)(head - 0x28))->v[2],
                 &((OverlayFlaggedQuadScratch*)(head - 0x28))->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&((OverlayFlaggedQuadScratch*)(head - 0x28))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(copy);
            ((OverlayFlaggedQuadScratch*)(head - 0x28))->otz++;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2 >> 1, arg2 >> 1, color);
            setRGB3(prim, 0, 0, 0);
            addPrim((u_long*)(((((u32)((OverlayFlaggedQuadScratch*)(head - 0x28))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, ((OverlayFlaggedQuadScratch*)(head - 0x28))->otz);
        }
    }
    SCRATCH_POP_BYTES(sizeof(OverlayFlaggedQuadScratch));
}
