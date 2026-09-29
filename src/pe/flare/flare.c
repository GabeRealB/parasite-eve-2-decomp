#include "pe/flare.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// This overlay's id, the `u16` every package opens with.

static void flareDrawSparkQuad(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);

/// PROVISIONAL: written before `Task` was processed, so the statements
/// about `Task` fields rest on unverified names. Rewrite once `Task` is done.
/// Emits the flare's shower of sparks.
///
/// Starts the sound cue panned to the object, then spawns one spark a frame for
/// the first 20 frames, each in a random direction, reparenting itself to the
/// last one spawned. Releases at frame 36, by which time the sparks it created
/// are running on their own.
///
/// A cancelled or interrupted cast stops the cue and releases immediately.
void flareEffectTask(Task* arg0)
{
    GpEffWork*  mem;
    GfxCoord*   coord;
    GpStateC08* state;
    s32         pan;
    s16         tick;
    GpEffWork*  spawned;
    s32         rng;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if ((state->field_3 == -2) || (Gp_State1C->fadeState != 0)) {
        SndEvt_EnqueueType7(0xE03E0001, 1);
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        pan = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(0xE03E0001, pan, (s8)gpGetObjDepth(coord));
        arg0->state = 1;
    }
    tick = mem->age;
    if (tick < 0x14) {
        if (tick == 8) {
            state->field_6 |= 8;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        spawned     = Gp_SpawnEff(0x6019E, coord, (((u32)rng >> 16) & 0x1FF) + 0x680, 0);
        if (spawned != NULL) {
            Task_Reparent(arg0, spawned->task);
        }
        return;
    }
    if (tick == 0x24) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// PROVISIONAL: written before `Task` was processed, so the statements
/// about `Task` fields rest on unverified names. Rewrite once `Task` is done.
/// Flies one spark away from the player and draws it.
///
/// On the first frame it starts from the player's position, picks a random
/// heading and pitch, and turns those into a velocity in the player's frame of
/// reference. Every frame after that it advances by that velocity and draws the
/// next sprite frame, stepping the frame on every second tick. Releases once
/// all eight frames have been drawn.
void flareSparkTask(Task* arg0)
{
    GpEffWork*  mem;
    GfxCoord*   coord;
    GfxCoord*   player;
    GpMtxWords* dstm;
    GpMtxWords* srcm;
    u32         rng;
    s32         temp_lo;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.tmd->coords;
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        player              = (gameGetPtrSlot(3))->extra.tmd->coords;
        dstm                = (GpMtxWords*)&coord->coord;
        srcm                = (GpMtxWords*)&player->coord;
        dstm->m00_m01       = srcm->m00_m01;
        dstm->m02_m10       = srcm->m02_m10;
        dstm->m11_m12       = srcm->m11_m12;
        dstm->m20_m21       = srcm->m20_m21;
        dstm->m22           = srcm->m22;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        rng          = Gp_LcgState * 5 + 0x71357911;
        mem->period  = arg0->spawnArg1.value & 0xFFF;
        mem->scale   = (rng >> 16) & 0xFFF;
        Gp_LcgState  = rng;
        mem->angle   = mem->period >> 5;
        mem->move.vx = (rsin(mem->scale) * mem->angle) >> 12;
        temp_lo      = rcos(mem->scale) * mem->angle;
        mem->move.vz = 0x100;
        mem->move.vy = temp_lo >> 12;
        gte_SetRotMatrix((MATRIX*)srcm);
        gte_ldv0(&mem->move);
        gte_rtv0();
        gte_stsv(&mem->move);
        arg0->state = 1;
    }
    coord->coord.t[0]  += mem->move.vx;
    coord->coord.t[1]  += mem->move.vy;
    coord->coord.t[2]  += mem->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    if (!(mem->age & 1)) {
        mem->index = mem->index + 1;
    }
    if (mem->index < 8) {
        flareDrawSparkQuad(coord, mem->index, mem->period, mem->scale);
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws one frame of a spark's sprite at a world position.
///
/// The position is projected to the screen and the quad dropped if it lands
/// behind the camera. `arg1` picks one of the eight texture frames, `arg3`
/// spins the quad and `arg2` sizes it, with the size divided by depth so the
/// sprite shrinks into the distance.
static void flareDrawSparkQuad(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    block->vec.vz                             = vz;
    SCRATCH_HEAD(GpFxQuadScratch)             = block;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        prim->clut  = 0x4311;
        u0          = arg1 << 5;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}
