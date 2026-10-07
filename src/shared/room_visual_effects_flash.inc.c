#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws an additive four-spike star over two concentric shaded discs.
///
/// `coord` must have a composed world matrix and `rgb` supplies three colour
/// bytes. The signed 16-bit `radius` in world units is scaled by
/// 64 / (SZ3 / 4 + 1). The outer disc and spikes use half brightness; the inner
/// disc uses full brightness at half radius. Two spikes reach the disc radius
/// and two reach twice it. Shoulders use one eighth of the disc projection
/// scale. Negative GTE flags suppress drawing. Scratch storage is released
/// after the draw; the twenty Gouraud quads live for the current frame.
static void _roomVisualEffectsDrawHaloStar(const GfxCoord* coord, s16 radius, const u8 rgb[3])
{
    /// Initializes a star wedge with three black rim vertices and a centre tint.
    ///
    /// `quad` must have no side effects because it is evaluated repeatedly;
    /// each colour expression is evaluated once, with its supplied type.
    /// Use only as a standalone statement list with a terminating semicolon.
#define ROOM_VISUAL_EFFECTS_INIT_HALO_STAR_WEDGE(quad, red, green, blue) \
    setPolyG4(quad);                                                     \
    setRGB0(quad, 0, 0, 0);                                              \
    setRGB1(quad, 0, 0, 0);                                              \
    setRGB2(quad, (red), (green), (blue));                               \
    setRGB3(quad, 0, 0, 0)

    RoomFxRadialScratch* projection;
    POLY_G4*             quad;
    s32                  angle;
    s32                  midAngle;
    s32                  nextAngle;
    s32                  shoulderAngle;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxRadialScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        projection->depth++;
        projection->radii.star.disc  = (radius * ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE) / projection->depth;
        projection->radii.star.spike = (radius * ROOM_VISUAL_EFFECTS_STAR_SHOULDER_PROJECTION_SCALE) / projection->depth;

        // Layer a full-bright inner disc over the half-bright outer disc.
        angle = 0;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            ROOM_VISUAL_EFFECTS_INIT_HALO_STAR_WEDGE(quad, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            quad->x0  = projection->screenX + ((projection->radii.star.disc * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            midAngle  = angle + 0x100;
            quad->y0  = projection->screenY + ((projection->radii.star.disc * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x1  = projection->screenX + ((projection->radii.star.disc * rsin(midAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1  = projection->screenY + ((projection->radii.star.disc * rcos(midAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            nextAngle = angle + 0x200;
            quad->x2  = projection->screenX;
            quad->y2  = projection->screenY;
            quad->x3  = projection->screenX + ((projection->radii.star.disc * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3  = projection->screenY + ((projection->radii.star.disc * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            ROOM_VISUAL_EFFECTS_INIT_HALO_STAR_WEDGE(quad, rgb[0], rgb[1], rgb[2]);
            quad->x0 = projection->screenX + ((projection->radii.star.disc * rsin(angle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->y0 = projection->screenY + ((projection->radii.star.disc * rcos(angle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->x1 = projection->screenX + ((projection->radii.star.disc * rsin(midAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->y1 = projection->screenY + ((projection->radii.star.disc * rcos(midAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->x2 = projection->screenX;
            quad->y2 = projection->screenY;
            quad->x3 = projection->screenX + ((projection->radii.star.disc * rsin(nextAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->y3 = projection->screenY + ((projection->radii.star.disc * rcos(nextAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        } while (angle < ROOM_VISUAL_EFFECTS_FULL_TURN);

        // Alternate short and long spikes around the projected centre.
        angle = 0x200;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            ROOM_VISUAL_EFFECTS_INIT_HALO_STAR_WEDGE(quad, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            shoulderAngle = angle - 0x400;
            quad->x0      = projection->screenX + ((projection->radii.star.spike * rsin(shoulderAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->y0      = projection->screenY + ((projection->radii.star.spike * rcos(shoulderAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->x1      = projection->screenX + ((projection->radii.star.disc * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y1      = projection->screenY + ((projection->radii.star.disc * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            shoulderAngle = angle + 0x400;
            quad->x2      = projection->screenX;
            quad->y2      = projection->screenY;
            quad->x3      = projection->screenX + ((projection->radii.star.spike * rsin(shoulderAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            quad->y3      = projection->screenY + ((projection->radii.star.spike * rcos(shoulderAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            ROOM_VISUAL_EFFECTS_INIT_HALO_STAR_WEDGE(quad, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            quad->x0      = projection->screenX + ((projection->radii.star.spike * rsin(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y0      = projection->screenY + ((projection->radii.star.spike * rcos(angle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->x1      = projection->screenX + ((projection->radii.star.disc * rsin(shoulderAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS - 1));
            quad->y1      = projection->screenY + ((projection->radii.star.disc * rcos(shoulderAngle)) >> (ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS - 1));
            shoulderAngle = angle + 0x800;
            quad->x2      = projection->screenX;
            quad->y2      = projection->screenY;
            quad->x3      = projection->screenX + ((projection->radii.star.spike * rsin(shoulderAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            quad->y3      = projection->screenY + ((projection->radii.star.spike * rcos(shoulderAngle)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS);
            angle         = shoulderAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->depth);
        } while (angle < ROOM_VISUAL_EFFECTS_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxRadialScratch);
#undef ROOM_VISUAL_EFFECTS_INIT_HALO_STAR_WEDGE
}

/// Emits twenty motes at turning, successively higher offsets around its spawn coordinate.
///
/// `task` owns a coordinate body and a zero-aged `EffectWork` in
/// `spawnArg2.pointer`; the emitter ignores `spawnArg1`. Each active age 1..20
/// advances a signed halfword heading by 512..1023 (4096 units per turn) and
/// emits at a local radial offset of approximately 768 coordinate units and
/// Y = -128 * age. `effectSpawn` copies that offset; it is not a velocity.
/// The mote argument selects steady downward motion at eight parent-axis
/// units per active tick, a half-extent of 513, default palette and lifetime 48.
/// The room must have installed `gRoomEffectMoteId` and keep its callback loaded.
/// Age 21 releases the emitter's work; motes are independent tasks and outlive
/// it. Room control pauses at nonzero values below four and cancels at four or above.
static inline void _roomVisualEffectsSparkEmitterTask(Task* task)
{
    enum { EMITTER_EMISSION_TICKS           = 20,
           EMITTER_HEADING_STEP_MIN         = 0x200,
           EMITTER_HEADING_STEP_RANDOM_MASK = 0x1FF,
           EMITTER_OFFSET_HEIGHT_STEP       = 128,
           // Steady down, half-extent 0x201, speed 8, lifetime 48, palette 0.
           EMITTER_MOTE_ARGUMENT = 0x30080201 };

    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s16         heading;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    } else {
        actorRenderComposeCoord(coord);
        work->age++;
        if (work->age >= EMITTER_EMISSION_TICKS + 1) {
            effectKillTask(work, task);
            return;
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        // scale accumulates the halfword heading; move holds the next local spawn offset.
        heading     = work->scale + (((gRandomLcgState >> 16) & EMITTER_HEADING_STEP_RANDOM_MASK) + EMITTER_HEADING_STEP_MIN);
        work->scale = heading;
        // Keep the unsigned shifts and signed-halfword narrowing of the radial offset.
        work->move.vx = (u32)(rcos(heading) * 3) >> 4;
        work->move.vy = -work->age * EMITTER_OFFSET_HEIGHT_STEP;
        work->move.vz = (u32)(rsin(work->scale) * 3) >> 4;
        effectSpawn(gRoomEffectMoteId, coord, EMITTER_MOTE_ARGUMENT, &work->move);
    }
}
