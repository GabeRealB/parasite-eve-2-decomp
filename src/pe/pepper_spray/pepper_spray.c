#include "pe/pepper_spray.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/display.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

static void func_pepper_spray_8012F21C(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_pepper_spray_8012F634(GfxCoord* arg0, s16 arg1, s16 arg2);

/// The six spray-cone yaws, refilled once per cast by
/// `func_pepper_spray_8012EF34` from `gRandomLcgState`: entry `i` is a 0x400-wide
/// draw offset into the quadrant `i & 3`, so the six quads fan around the
/// nozzle. `func_pepper_spray_8012F634` draws one quad per entry every frame.
static s16 D_pepper_spray_8012FB9C[6] = { 0, 0, 0, 0, 0, 0 };

/// Runs one frame of the pepper spray. State 0 enables transient point-light
/// slot 0 at the nozzle's position, seeds the spray yaw / spread / brightness from
/// `gRandomLcgState`, refills the six cone yaws and plays the spray sound; state 1
/// just decays the yaw and the brightness by a sixteenth each, scaled by how
/// long the spray has run. Either state then redraws the nozzle, flashes the
/// screen at the current brightness and draws the six cone quads. The effect
/// ends after nine frames, or immediately if the player is dying
/// (`Gp_StateC08.effectPhase`) or parasite-energy effects are cancelled
/// (`gRoomEffectState->peEffectControl`).

void func_pepper_spray_8012EF34(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            i;
    s32                            age;
    s32                            tz;
    s32                            yaw;
    s32                            spread;
    s32                            pan;
    u8                             rgb[3];

    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        SndEvt_EnqueueType7(SOUND_PEPPER_SPRAY_USE, 1);
        effectKillTask(mem, arg0);
        return;
    }
    age      = (u16)mem->age + 1;
    mem->age = age;
    switch (arg0->state) {
        case 0:
            slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
            gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            yaw                                                = ((gRandomLcgState >> 16) & 0x3FF) + 0xA00;
            slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
            gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            spread                                             = (gRandomLcgState >> 16) & 0xFFF;
            tz                                                 = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            slot->head.color.r                                 = 0x1000;
            slot->head.color.g                                 = 0x1000;
            slot->head.color.b                                 = 0x1000;
            slot->inner                                        = 0xFA0;
            slot->outer                                        = 0x12C0;
            lightSlot->framesLeft                              = 6;
            slot->head.transform.coord.coord.t[2]              = tz;
            mem->period                                        = 0xE0;
            mem->scale                                         = yaw;
            mem->angle                                         = spread;
            arg0->state                                        = 1;
            for (i = 0; i < 6; i++) {
                gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_pepper_spray_8012FB9C[i] = ((i & 3) << 10) + ((gRandomLcgState >> 16) & 0x3FF);
            }
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(SOUND_PEPPER_SPRAY_USE, pan, (s8)worldCoordGetOriginAudioDepth(coord));
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
        effectKillTask(mem, arg0);
    }
}

/// Links the pepper-spray nozzle quad at `arg0`'s world position. The position
/// is projected through `GsWSMATRIX` by a single `RTPS` and the quad is
/// dropped when that sets a negative `gte_stflg`. `arg1` sizes it and `arg2`
/// spins it: the corners sit `arg1 * 0x37 / depth` from the projected centre
/// along `arg2` and `arg2 + 0x400`, so the nozzle shrinks with depth. The
/// texture is the fixed 0x37 x 0x37 patch at (0x70, 0xC8) on tpage 0x29, drawn
/// semi-transparent and unshaded.
static void func_pepper_spray_8012F21C(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    EffectBillboardScratch* scratchHead;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               prim;
    s32                     ang;

    scratchHead                                  = SCRATCH_STACK_CURSOR(EffectBillboardScratch);
    block                                        = scratchHead - 1;
    depthOutput                                  = &block->depth;
    block->worldPoint.vx                         = (u16)arg0->workm.t[0];
    block->worldPoint.vy                         = (u16)arg0->workm.t[1];
    block->worldPoint.vz                         = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectBillboardScratch) = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        block->depth++;
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        prim->code          |= 3;
        ang                  = arg2;
        block->cornerOffsetX = (((arg1 * 0x37) / block->depth) * rsin(ang)) >> 12;
        block->cornerOffsetY = (((arg1 * 0x37) / block->depth) * rcos(ang)) >> 12;
        prim->x0             = block->screenX + (u16)block->cornerOffsetX;
        prim->x3             = block->screenX - (u16)block->cornerOffsetX;
        prim->y0             = block->screenY - (u16)block->cornerOffsetY;
        prim->y3             = block->screenY + (u16)block->cornerOffsetY;
        ang                  = ang + 0x400;
        block->cornerOffsetX = (((arg1 * 0x37) / block->depth) * rsin(ang)) >> 12;
        block->cornerOffsetY = (((arg1 * 0x37) / block->depth) * rcos(ang)) >> 12;
        prim->x1             = block->screenX + (u16)block->cornerOffsetX;
        prim->x2             = block->screenX - (u16)block->cornerOffsetX;
        prim->y1             = block->screenY - (u16)block->cornerOffsetY;
        prim->y2             = block->screenY + (u16)block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}

/* The GTE loads and stores address the block as `head[-1]`, from the cursor
   read before the reservation, while the plain field writes go through `blk`:
   spelling them all off `blk` lets CSE reuse its register, and the original
   does not. */
/// Draws the pepper-spray cone as one Gouraud quad: three corners on a 0x100
/// circle around `arg1` (at `-0xC0`, `0`, `+0xC0`) and one tip twice as far
/// out and 0x200 towards the camera, all in `arg0`'s `workm` frame. `arg2` is
/// the spray brightness; only the corner along `arg1` is lit, with half of
/// `arg2` in red and green and all of it in blue.
static void func_pepper_spray_8012F634(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    OverlayFlaggedQuadScratch* head;
    OverlayFlaggedQuadScratch* blk;
    OverlayFlaggedQuadScratch* copy;
    POLY_G4*                   prim;
    MATRIX*                    wm;
    s32                        ang;
    s32                        back;
    s32                        depth;
    s16                        color;

    depth                                           = -0x200;
    head                                            = SCRATCH_STACK_CURSOR(OverlayFlaggedQuadScratch);
    blk                                             = head - 1;
    SCRATCH_STACK_CURSOR(OverlayFlaggedQuadScratch) = blk;
    copy                                            = blk;
    color                                           = arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    ang = arg1;

    back               = ang - 0xC0;
    blk->corners[0].vx = (u32)rsin(back) >> 4;
    blk->corners[0].vy = (u32)rcos(back) >> 4;
    blk->corners[0].vz = 0;
    wm                 = &arg0->workm;
    gte_SetRotMatrix(wm);
    gte_ldv0(&head[-1].corners[0]);
    gte_rtv0();
    gte_stsv(&head[-1].corners[0]);
    (u16) blk->corners[0].vx = (u16)blk->corners[0].vx + (u16)arg0->workm.t[0];
    (u16) blk->corners[0].vy = (u16)blk->corners[0].vy + (u16)arg0->workm.t[1];
    (u16) blk->corners[0].vz = (u16)blk->corners[0].vz + (u16)arg0->workm.t[2];

    blk->corners[1].vx = (u32)rsin(ang) >> 1;
    blk->corners[1].vy = (u32)rcos(ang) >> 1;
    blk->corners[1].vz = depth;
    gte_SetRotMatrix(wm);
    gte_ldv0(&head[-1].corners[1]);
    gte_rtv0();
    gte_stsv(&head[-1].corners[1]);
    (u16) blk->corners[1].vx = (u16)blk->corners[1].vx + (u16)arg0->workm.t[0];
    (u16) blk->corners[1].vy = (u16)blk->corners[1].vy + (u16)arg0->workm.t[1];
    (u16) blk->corners[1].vz = (u16)blk->corners[1].vz + (u16)arg0->workm.t[2];

    blk->corners[2].vx = (u32)rsin(ang) >> 4;
    blk->corners[2].vy = (u32)rcos(ang) >> 4;
    blk->corners[2].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&head[-1].corners[2]);
    gte_rtv0();
    gte_stsv(&head[-1].corners[2]);
    (u16) blk->corners[2].vx = (u16)blk->corners[2].vx + (u16)arg0->workm.t[0];
    ang                      = ang + 0xC0;
    (u16) blk->corners[2].vy = (u16)blk->corners[2].vy + (u16)arg0->workm.t[1];
    (u16) blk->corners[2].vz = (u16)blk->corners[2].vz + (u16)arg0->workm.t[2];

    blk->corners[3].vx = (u32)rsin(ang) >> 4;
    blk->corners[3].vy = (u32)rcos(ang) >> 4;
    blk->corners[3].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&head[-1].corners[3]);
    gte_rtv0();
    gte_stsv(&head[-1].corners[3]);
    (u16) blk->corners[3].vx = (u16)blk->corners[3].vx + (u16)arg0->workm.t[0];
    (u16) blk->corners[3].vy = (u16)blk->corners[3].vy + (u16)arg0->workm.t[1];
    (u16) blk->corners[3].vz = (u16)blk->corners[3].vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&head[-1].corners[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_stflg(&head[-1].flag);
    if (blk->flag >= 0) {
        gte_ldv3(&head[-1].corners[1], &head[-1].corners[2], &head[-1].corners[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&head[-1].flag);
        if (blk->flag >= 0) {
            gte_stszotz(&copy->otz);
            head[-1].otz++;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2 >> 1, arg2 >> 1, color);
            setRGB3(prim, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)head[-1].otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, head[-1].otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}
