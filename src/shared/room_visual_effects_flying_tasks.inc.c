#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Runs an attached charge disc with player-joint sparks, flicker and a fading release ring.
///
/// `task` owns a coordinate body and the zeroed `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` indexes the two `RoomFx_DiscShades`
/// rows (0 or 1). The work's borrowed parent coordinate and its ancestors must
/// remain live. ATTACH replaces the spawn placement with an identity rotation
/// and the copied offset in that parent's axes; this first active tick does not draw.
/// GROW emits a child flying spark every fourth age from player model parts
/// 3..18, so that model and the installed flying-spark callback must be live.
/// Each spark borrows this disc's composed coordinate as its initial target;
/// adopting its task ensures it is torn down with the disc.
/// FLICKER stops emission and adds a half-bright larger disc on odd ages.
/// RELEASE draws the shrinking disc before offsetting its composed cache for
/// an expanding orange ring, fading by 16 per active tick until teardown.
/// The local attachment remains intact. Its owner requests these states using
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`; room control pauses at nonzero values
/// below four and cancels at four or above, releasing work and child tasks.
static inline void _roomVisualEffectsGlowDiscTask(Task* task)
{
    /// Brightens and enlarges the disc before applying its selected tint.
    ///
    /// Captures this task's `work`, `task` and three-byte `rgb` locals; the
    /// tint index must be 0 or 1. Use as a standalone statement list in a block.
#define ROOM_VISUAL_EFFECTS_GROW_AND_TINT_GLOW_DISC()                        \
    if (work->scale < GLOW_DISC_FULL_BRIGHTNESS) {                           \
        work->scale += GLOW_DISC_BRIGHTEN_STEP;                              \
    }                                                                        \
    if (work->angle < GLOW_DISC_FULL_RADIUS) {                               \
        work->angle += GLOW_DISC_RADIUS_STEP;                                \
    }                                                                        \
    rgb[0] = work->scale >> RoomFx_DiscShades[task->spawnArg1.value].rShift; \
    rgb[1] = work->scale >> RoomFx_DiscShades[task->spawnArg1.value].gShift; \
    rgb[2] = work->scale >> RoomFx_DiscShades[task->spawnArg1.value].bShift

    enum { GLOW_DISC_FULL_BRIGHTNESS      = 0xC0,
           GLOW_DISC_BRIGHTEN_STEP        = 8,
           GLOW_DISC_FULL_RADIUS          = 0x200,
           GLOW_DISC_RADIUS_STEP          = 0x10,
           GLOW_DISC_SPARK_INTERVAL_TICKS = 4,
           GLOW_DISC_FIRST_PLAYER_PART    = 3,
           GLOW_DISC_PLAYER_PART_COUNT    = 16,
           GLOW_DISC_FLICKER_RADIUS_DELTA = 0x100,
           GLOW_DISC_RELEASE_OFFSET       = 0x100,
           GLOW_DISC_RING_RADIUS_STEP     = 8,
           GLOW_DISC_RING_INITIAL_RADIUS  = 0x80,
           GLOW_DISC_RING_TINT_DELTA      = 0x100,
           GLOW_DISC_FADE_STEP            = 0x10 };

    EffectWork* work;
    GfxCoord*   coord;
    EffectWork* sparkWork;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // scale is brightness, angle is disc radius, period is release-ring growth.
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        case ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH:
            // Attach in the borrowed anchor's axes rather than keeping the spawn placement.
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = work->pos.vx;
            coord->coord.t[1]   = work->pos.vy;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = ROOM_VISUAL_EFFECTS_GLOW_DISC_GROW;
            break;
        case ROOM_VISUAL_EFFECTS_GLOW_DISC_GROW:
            actorRenderComposeCoord(coord);
            if (!(work->age & (GLOW_DISC_SPARK_INTERVAL_TICKS - 1))) {
                Task* playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                sparkWork        = effectSpawn(gRoomEffectFlyingSparkId, &playerTask->extra.tmd->coords[((gRandomLcgState >> 16) & (GLOW_DISC_PLAYER_PART_COUNT - 1)) + GLOW_DISC_FIRST_PLAYER_PART], coord, NULL);
                if (sparkWork != NULL) {
                    taskReparent(task, sparkWork->task);
                }
            }
            ROOM_VISUAL_EFFECTS_GROW_AND_TINT_GLOW_DISC();
            _roomVisualEffectsDrawFlyingDisc(coord, work->angle, rgb);
            break;
        case ROOM_VISUAL_EFFECTS_GLOW_DISC_FLICKER:
            actorRenderComposeCoord(coord);
            ROOM_VISUAL_EFFECTS_GROW_AND_TINT_GLOW_DISC();
            _roomVisualEffectsDrawFlyingDisc(coord, work->angle, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            if (work->age & 1) {
                _roomVisualEffectsDrawFlyingDisc(coord, (s16)(work->angle + GLOW_DISC_FLICKER_RADIUS_DELTA), rgb);
            }
            break;
        case ROOM_VISUAL_EFFECTS_GLOW_DISC_RELEASE:
            actorRenderComposeCoord(coord);
            rgb[0] = work->scale >> RoomFx_DiscShades[task->spawnArg1.value].rShift;
            rgb[1] = work->scale >> RoomFx_DiscShades[task->spawnArg1.value].gShift;
            rgb[2] = work->scale >> RoomFx_DiscShades[task->spawnArg1.value].bShift;
            _roomVisualEffectsDrawFlyingDisc(coord, work->angle, rgb);
            ROOM_VISUAL_EFFECTS_SET_ORANGE_TINT(rgb, work->scale);
            // Freeze a local release offset in composed view axes on entering this phase.
            if (work->period == 0) {
                work->move.vy = -GLOW_DISC_RELEASE_OFFSET;
                work->move.vz = GLOW_DISC_RELEASE_OFFSET;
                work->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            // Only the ring uses this shifted cache; the disc above used the attached centre.
            work->period      += GLOW_DISC_RING_RADIUS_STEP;
            coord->workm.t[0] += work->move.vx;
            coord->workm.t[1] += work->move.vy;
            coord->workm.t[2] += work->move.vz;
            _roomVisualEffectsDrawFlyingRing(coord, (s16)(work->period + GLOW_DISC_RING_INITIAL_RADIUS), GLOW_DISC_RING_TINT_DELTA, rgb);
            work->angle -= GLOW_DISC_FADE_STEP;
            if (work->scale > GLOW_DISC_FADE_STEP) {
                work->scale -= GLOW_DISC_FADE_STEP;
                break;
            }
            effectKillTask(work, task);
            break;
        case ROOM_VISUAL_EFFECTS_GLOW_DISC_CANCEL:
            effectKillTask(work, task);
            break;
    }
}
#undef ROOM_VISUAL_EFFECTS_GROW_AND_TINT_GLOW_DISC

/// Runs an animated spark along a fixed step derived from an initial target offset.
///
/// `spawnArg1.pointer` is a live target `GfxCoord` with a composed world matrix
/// for the first active tick. The task's own world matrix must also be composed.
/// That tick converts the initial displacement into parent axes, narrows it to
/// 16-bit components, and stores 204/4096 of it as the per-tick step in `pos`.
/// The target is not sampled again. Subsequent active ticks move by that step
/// and draw on odd ages. At age 20 the task releases its owned `EffectWork` in
/// `spawnArg2`. Room effect control pauses at nonzero and cancels at four or above.
static inline void _roomVisualEffectsFlyingSparkTask(Task* task)
{
    enum { SPARK_INITIALIZE,
           SPARK_FLY,
           SPARK_STEP_Q12       = 0xCC,
           SPARK_LIFETIME_TICKS = 20 };

    EffectWork*     work;
    GfxCoord*       coord;
    const GfxCoord* targetCoord;
    VECTOR          targetOffset;

    work        = task->spawnArg2.pointer;
    coord       = task->extra.coordBody->coord;
    targetCoord = task->spawnArg1.pointer;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        switch (task->state) {
            case SPARK_INITIALIZE:
                // Fix the flight step in parent axes from the initial world-space separation.
                targetOffset.vx = targetCoord->workm.t[0] - coord->workm.t[0];
                targetOffset.vy = targetCoord->workm.t[1] - coord->workm.t[1];
                targetOffset.vz = targetCoord->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &targetOffset, &targetOffset);
                work->pos.vx = targetOffset.vx;
                work->pos.vy = targetOffset.vy;
                work->pos.vz = targetOffset.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(SPARK_STEP_Q12);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = SPARK_FLY;
                break;
            case SPARK_FLY:
                coord->coord.t[0]  += work->pos.vx;
                coord->coord.t[1]  += work->pos.vy;
                coord->coord.t[2]  += work->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    _roomVisualEffectsDrawFlyingSpark(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= SPARK_LIFETIME_TICKS) {
                    effectKillTask(work, task);
                }
                break;
        }
    } else if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
    }
}
