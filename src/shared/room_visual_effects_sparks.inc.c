#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
static void RoomFx_DrawTwinTrail(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomFxTwinTrailScratch* block;
    GfxCoord*               a;
    GfxCoord*               b;
    POLY_G4*                prim;
    s32                     i;
    s32                     j;
    s32                     i0;
    s32                     i1;
    s32                     hi;
    s32                     lo;
    s32                     fade;
    s32                     r;
    s32                     g;
    s32                     bl;
    s32                     r2;
    s32                     g2;
    s32                     b2;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomFxTwinTrailScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    // Seven quads, newest edge first. Each joins the two trails at one slot and the slot before it.
    i = 0;
    do {
        j                         = arg2 - i;
        i0                        = j & 7;
        a                         = &arg0[i0];
        block->worldCorners[0].vx = (u16)a->workm.t[0];
        j                         = j - 1;
        block->worldCorners[0].vy = (u16)a->workm.t[1];
        i1                        = j & 7;
        block->worldCorners[0].vz = (u16)a->workm.t[2];
        b                         = &arg1[i0];
        block->worldCorners[1].vx = (u16)b->workm.t[0];
        block->worldCorners[1].vy = (u16)b->workm.t[1];
        block->worldCorners[1].vz = (u16)b->workm.t[2];
        a                         = &arg0[i1];
        block->worldCorners[2].vx = (u16)a->workm.t[0];
        block->worldCorners[2].vy = (u16)a->workm.t[1];
        block->worldCorners[2].vz = (u16)a->workm.t[2];
        b                         = &arg1[i1];
        block->worldCorners[3].vx = (u16)b->workm.t[0];
        block->worldCorners[3].vy = (u16)b->workm.t[1];
        block->worldCorners[3].vz = (u16)b->workm.t[2];
        // Corner 0 is projected alone. The flag word belongs to the transform of the other three.
        gte_ldv0(&block->worldCorners[0]);
        gte_rtps();
        gte_stsxy(&block->screenX0);
        gte_ldv3(&block->worldCorners[1], &block->worldCorners[2], &block->worldCorners[3]);
        gte_rtpt();
        gte_stsxy3(&block->screenX1, &block->screenX2, &block->screenX3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            block->depth   = block->depth + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = block->screenX0;
            prim->y0 = block->screenY0;
            prim->x1 = block->screenX1;
            prim->y1 = block->screenY1;
            prim->x2 = block->screenX2;
            prim->y2 = block->screenY2;
            prim->x3 = block->screenX3;
            prim->y3 = block->screenY3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxTwinTrailScratch);
}

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
static inline void RoomFx_SparkBurstTask(Task* task)
{
    GfxCoord*   objCoord;
    EffectWork* work;
    u8          rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (EffectWork*)task->spawnArg2.pointer;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    actorRenderComposeCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(EFFECT_IMPACT_FLASH, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(EFFECT_BOUNCING_SPARK, objCoord, 0x100, NULL);
                Gp_SpawnEff(EFFECT_BOUNCING_SPARK, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, objCoord, ((gRandomLcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            RoomFx_DrawFlashRing(objCoord, 0x100, 0x100, rgb);
            RoomFx_DrawFlashRing(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            effectKillTask(work, task);
            break;
    }
}
