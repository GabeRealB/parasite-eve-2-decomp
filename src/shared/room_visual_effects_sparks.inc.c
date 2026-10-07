#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws seven fading additive quads between two eight-frame trail histories.
///
/// Each array contains eight composed `GfxCoord` frames. `newestSlot` is 0..7;
/// the drawer walks backwards with modulo-eight indexing. The three packed
/// colour multipliers are red at bits 8..15 (signed shift), green at bits 4..5,
/// and blue at bits 0..1; callers use 0x123 for multipliers 1, 2 and 3. Levels
/// descend by nine from 64 on the newest edge. A negative GTE flag from the
/// last three corners skips that quad; corner zero's flags are not tested.
static void _roomVisualEffectsDrawTwinTrail(const GfxCoord firstTrail[ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT], const GfxCoord secondTrail[ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT], s16 newestSlot, s16 packedColorMultipliers)
{
    RoomFxTwinTrailScratch* projection;
    const GfxCoord*         firstCoord;
    const GfxCoord*         secondCoord;
    POLY_G4*                quad;
    s32                     segmentAge;
    s32                     historySlot;
    s32                     newerSlot;
    s32                     olderSlot;
    s32                     newerLevel;
    s32                     olderLevel;
    s32                     unwrappedLevel;
    s32                     newerRed;
    s32                     newerGreen;
    s32                     newerBlue;
    s32                     olderRed;
    s32                     olderGreen;
    s32                     olderBlue;

    projection = SCRATCH_STACK_RESERVE_BLOCK(RoomFxTwinTrailScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    // Seven quads, newest edge first. Each joins the two trails at one slot and the slot before it.
    segmentAge = 0;
    do {
        historySlot                    = newestSlot - segmentAge;
        newerSlot                      = historySlot & (ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT - 1);
        firstCoord                     = &firstTrail[newerSlot];
        projection->worldCorners[0].vx = (u16)firstCoord->workm.t[0];
        historySlot                    = historySlot - 1;
        projection->worldCorners[0].vy = (u16)firstCoord->workm.t[1];
        olderSlot                      = historySlot & (ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT - 1);
        projection->worldCorners[0].vz = (u16)firstCoord->workm.t[2];
        secondCoord                    = &secondTrail[newerSlot];
        projection->worldCorners[1].vx = (u16)secondCoord->workm.t[0];
        projection->worldCorners[1].vy = (u16)secondCoord->workm.t[1];
        projection->worldCorners[1].vz = (u16)secondCoord->workm.t[2];
        firstCoord                     = &firstTrail[olderSlot];
        projection->worldCorners[2].vx = (u16)firstCoord->workm.t[0];
        projection->worldCorners[2].vy = (u16)firstCoord->workm.t[1];
        projection->worldCorners[2].vz = (u16)firstCoord->workm.t[2];
        secondCoord                    = &secondTrail[olderSlot];
        projection->worldCorners[3].vx = (u16)secondCoord->workm.t[0];
        projection->worldCorners[3].vy = (u16)secondCoord->workm.t[1];
        projection->worldCorners[3].vz = (u16)secondCoord->workm.t[2];
        // Corner 0 is projected alone. The flag word belongs to the transform of the other three.
        gte_ldv0(&projection->worldCorners[0]);
        gte_rtps();
        gte_stsxy(&projection->screenX0);
        gte_ldv3(&projection->worldCorners[1], &projection->worldCorners[2], &projection->worldCorners[3]);
        gte_rtpt();
        gte_stsxy3(&projection->screenX1, &projection->screenX2, &projection->screenX3);
        gte_stflg(&projection->projectionFlags);
        if (projection->projectionFlags >= 0) {
            gte_stszotz(&projection->depth);
            unwrappedLevel    = 0x40 - segmentAge * ROOM_VISUAL_EFFECTS_TRAIL_LEVEL_STEP;
            newerLevel        = unwrappedLevel & 0xFF;
            newerRed          = newerLevel * (packedColorMultipliers >> 8);
            newerGreen        = newerLevel * ((packedColorMultipliers >> 4) & 3);
            newerBlue         = newerLevel * (packedColorMultipliers & 3);
            olderLevel        = (unwrappedLevel - ROOM_VISUAL_EFFECTS_TRAIL_LEVEL_STEP) & 0xFF;
            olderRed          = olderLevel * (packedColorMultipliers >> 8);
            olderGreen        = olderLevel * ((packedColorMultipliers >> 4) & 3);
            quad              = gGpuPrimCursor;
            projection->depth = projection->depth + 1;
            gGpuPrimCursor    = quad + 1;
            setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
            olderBlue = olderLevel * (packedColorMultipliers & 3);
            setcode(quad, ROOM_VISUAL_EFFECTS_GOURAUD_QUAD);
            quad->r0 = newerRed;
            quad->r1 = newerRed;
            quad->g0 = newerGreen;
            quad->g1 = newerGreen;
            quad->b0 = newerBlue;
            quad->b1 = newerBlue;
            quad->r2 = olderRed;
            quad->r3 = olderRed;
            quad->g2 = olderGreen;
            quad->g3 = olderGreen;
            quad->b2 = olderBlue;
            quad->b3 = olderBlue;
            quad->x0 = projection->screenX0;
            quad->y0 = projection->screenY0;
            quad->x1 = projection->screenX1;
            quad->y1 = projection->screenY1;
            quad->x2 = projection->screenX2;
            quad->y2 = projection->screenY2;
            quad->x3 = projection->screenX3;
            quad->y3 = projection->screenY3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        }
        segmentAge += 1;
    } while (segmentAge < ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT - 1);
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
            effectSpawn(EFFECT_IMPACT_FLASH, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                effectSpawn(EFFECT_SMOKE_PUFF, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                effectSpawn(EFFECT_BOUNCING_SPARK, objCoord, 0x100, NULL);
                effectSpawn(EFFECT_BOUNCING_SPARK, objCoord, 0x100, NULL);
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
            effectSpawn(EFFECT_SMOKE_PUFF, objCoord, ((gRandomLcgState >> 16) & 0x1FF) | 0x82003400,
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
            _roomVisualEffectsDrawFlashRing(objCoord, 0x100, 0x100, rgb);
            _roomVisualEffectsDrawFlashRing(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            effectKillTask(work, task);
            break;
    }
}
