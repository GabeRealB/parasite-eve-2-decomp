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

/// A spark emitter. For 0x14 ticks it turns its heading by a random
/// 0x200..0x3FF and spawns the effect `gRoomEffectMoteId` names at its frame, moving
/// outwards along that heading at 3/16 speed with a vertical velocity of -0x80
/// per tick of age, then releases its work block. It pauses while the room's
/// event state is set and releases the block when that state reaches 4.
static inline void RoomFx_SparkEmitterTask(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
        return;
    } else {
        actorRenderComposeCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
            effectKillTask(mem, arg0);
            return;
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        ang             = mem->scale + (((gRandomLcgState >> 16) & 0x1FF) + 0x200);
        mem->scale      = ang;
        mem->move.vx    = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy    = -mem->age * 128;
        mem->move.vz    = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(gRoomEffectMoteId, coord, 0x30080201, &mem->move);
    }
}
