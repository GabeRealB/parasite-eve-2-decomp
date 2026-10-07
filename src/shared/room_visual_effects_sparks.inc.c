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

/// Chooses a smoke spawn offset, advancing the LCG separately for each axis.
///
/// Writes the three signed halfwords of `work->move` in -255..256 coordinate
/// units. `work` is borrowed and writable; no other work field is changed.
static inline void _roomVisualEffectsChooseSparkBurstOffset(EffectWork* work)
{
    enum { BURST_OFFSET_CENTRE      = 0x100,
           BURST_OFFSET_RANDOM_MASK = 0x1FF };

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = BURST_OFFSET_CENTRE - ((gRandomLcgState >> 16) & BURST_OFFSET_RANDOM_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = BURST_OFFSET_CENTRE - ((gRandomLcgState >> 16) & BURST_OFFSET_RANDOM_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vz   = BURST_OFFSET_CENTRE - ((gRandomLcgState >> 16) & BURST_OFFSET_RANDOM_MASK);
}

/// Runs an impact flash followed by smoke puffs or fading orange rings and bouncing sparks.
///
/// `task` owns a coordinate body and a zero-aged `EffectWork` in
/// `spawnArg2.pointer`. A nonzero `spawnArg1.value` selects smoke: the first
/// active tick emits one puff, then ages 2..7 emit a puff at an independently
/// jittered XYZ offset in -255..256 coordinate units. Zero selects two bouncing
/// sparks on the first tick, followed by a fixed ring and an expanding ring.
/// Their brightness falls by 32 and the expanding radius grows by 48 per tick.
/// Both variants enter the release state at age seven and free work on the
/// next active tick. Spawned effects are independent tasks. Room control pauses
/// at nonzero values below four and cancels at four or above.
static inline void _roomVisualEffectsSparkBurstTask(Task* task)
{
    enum { SPARK_BURST_INITIALIZE,
           SPARK_BURST_SMOKE,
           SPARK_BURST_RINGS,
           SPARK_BURST_RELEASE,
           SPARK_BURST_RELEASE_AGE       = 7,
           SPARK_BURST_FLASH_HALF_EXTENT = 0x400,
           SPARK_BURST_SPARK_HALF_EXTENT = 0x100,
           SPARK_BURST_FIXED_RADIUS      = 0x100,
           SPARK_BURST_INITIAL_LEVEL     = 0xC0,
           SPARK_BURST_FADE_STEP         = 0x20,
           SPARK_BURST_RADIUS_STEP       = 0x30,
           SPARK_BURST_RANDOM_MASK       = 0x1FF,
           // Additive smoke, half-extent 0x600, four ticks per atlas frame, default rise.
           SPARK_BURST_INITIAL_SMOKE_ARG = 0x80004600,
           // Additive smoke, random direction mode 2, half-extent 0x400..0x5FF, three ticks/frame.
           SPARK_BURST_JITTERED_SMOKE_ARG = 0x82003400 };

    GfxCoord*   coord;
    EffectWork* work;
    u8          rgb[3];

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->state) {
        case SPARK_BURST_INITIALIZE:
            effectSpawn(EFFECT_IMPACT_FLASH, coord, SPARK_BURST_FLASH_HALF_EXTENT, NULL);
            if (task->spawnArg1.value != 0) {
                effectSpawn(EFFECT_SMOKE_PUFF, coord, SPARK_BURST_INITIAL_SMOKE_ARG, NULL);
                task->state = SPARK_BURST_SMOKE;
            } else {
                effectSpawn(EFFECT_BOUNCING_SPARK, coord, SPARK_BURST_SPARK_HALF_EXTENT, NULL);
                effectSpawn(EFFECT_BOUNCING_SPARK, coord, SPARK_BURST_SPARK_HALF_EXTENT, NULL);
                work->scale = SPARK_BURST_FIXED_RADIUS;
                work->angle = SPARK_BURST_INITIAL_LEVEL;
                task->state = SPARK_BURST_RINGS;
            }
            break;

        case SPARK_BURST_SMOKE:
            // The vector is a spawn offset; the child chooses its own random motion.
            _roomVisualEffectsChooseSparkBurstOffset(work);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ((gRandomLcgState >> 16) & SPARK_BURST_RANDOM_MASK) | SPARK_BURST_JITTERED_SMOKE_ARG,
                        &work->move);
            if (work->age >= SPARK_BURST_RELEASE_AGE) {
                task->state = SPARK_BURST_RELEASE;
            }
            break;

        case SPARK_BURST_RINGS:
            // Reused work fields hold ring radius in scale and byte brightness in angle.
            work->angle -= SPARK_BURST_FADE_STEP;
            work->scale += SPARK_BURST_RADIUS_STEP;
            ROOM_VISUAL_EFFECTS_SET_ORANGE_TINT(rgb, work->angle);
            _roomVisualEffectsDrawFlashRing(coord, SPARK_BURST_FIXED_RADIUS, SPARK_BURST_FIXED_RADIUS, rgb);
            _roomVisualEffectsDrawFlashRing(coord, work->scale, work->scale, rgb);
            if (work->age >= SPARK_BURST_RELEASE_AGE) {
                task->state = SPARK_BURST_RELEASE;
            }
            break;

        case SPARK_BURST_RELEASE:
            effectKillTask(work, task);
            break;
    }
}
