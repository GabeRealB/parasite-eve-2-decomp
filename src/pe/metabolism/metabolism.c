#include "pe/metabolism.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Visual tuning of the metabolism cast for one Parasite Energy level.
///
/// The cast is a fan of glow wedges, two rings and up to three arcs that
/// brighten and widen around the caster while it throws off sparks. It selects
/// its row with the level digit of the attachment id, less one, which it keeps
/// in `EffectWork::index`.
///
/// The brightness is the cast's `EffectWork::scale`: the green channel of the
/// drawing, which red and blue are shifted down from. The radius is its
/// `EffectWork::angle`: the extent of the fan and the arcs, twice the radius
/// of the rings, and the distance from the caster each spark is spawned at.
typedef struct {
    s16 wedgeCount;      // Glow wedges in the fan, one random angle each; the angle table holds 16
    s16 brightnessLimit; // Brightness the growth stops at; below it the cast gains 0x10 a frame
    s16 radiusStep;      // Radius gained per frame, while the cast grows and while it fades
    s16 radiusLimit;     // Radius that ends the growth; also the sprite radius each spark is spawned with
} _MetabolismLevelTuning;
STATIC_ASSERT_SIZEOF(_MetabolismLevelTuning, 8);

/// Per-level tuning for the metabolism drain, one row per PE level 1-3,
/// weakest first.
static _MetabolismLevelTuning D_metabolism_8012FB54[] = {
    { 0x0008, 0x0080, 0x0020, 0x0400 },
    { 0x000C, 0x00B0, 0x0030, 0x0500 },
    { 0x0010, 0x00E0, 0x0040, 0x0600 },
};

/// The `sndEvtRequestScriptStart` id for each `D_metabolism_8012FB54` row.
static s32 D_metabolism_8012FB6C[] = { 0xE01F0001, 0xE0220001, 0xE0250001 };

/// Scratch angles for the fan, one per wedge: `(i << 10)` plus a 10-bit
/// random offset, seeded by state 0 and drawn by `_metabolismDrawFanWedge`.
static s16 D_metabolism_8012FB78[16];

static void _metabolismDrawFanWedge(const GfxCoord* coord, s16 radius, s16 bearing, s16 brightness);

/// Draws the metabolism cast's doubled disc and concentric outer glow bands.
///
/// Borrows a composed centre coordinate and the cast's work for this call.
/// `angle` is the radius in game-coordinate units, `scale` is green brightness
/// (red / 4, blue / 2), `age` selects the extra band on odd ticks, and a
/// nonzero `index` selects the additional outer band for PE levels 2 and 3.
/// RGB arithmetic narrows to bytes after each operation. The drawers borrow
/// scratch storage and append packets to the unchecked frame primitive arena.
static inline void _metabolismDrawGlow(const GfxCoord* coord, const EffectWork* castWork)
{
    enum {
        METABOLISM_GLOW_BAND_WIDTH        = 0x80,
        METABOLISM_GLOW_OUTER_BAND_OFFSET = 0x200,
    };
    u8 rgb[3];

    rgb[0] = castWork->scale >> 2;
    rgb[1] = castWork->scale;
    rgb[2] = castWork->scale >> 1;
    effectDrawGouraudDisc(coord, castWork->angle >> 1, rgb);
    effectDrawGouraudDisc(coord, castWork->angle >> 1, rgb);
    rgb[0] >>= 1;
    rgb[1] >>= 1;
    rgb[2] >>= 1;
    effectDrawOuterGlowBand(coord, castWork->angle, METABOLISM_GLOW_BAND_WIDTH, rgb);
    if (castWork->age & 1) {
        rgb[1] >>= 1;
        rgb[2] <<= 1;
        effectDrawOuterGlowBand(coord, METABOLISM_GLOW_BAND_WIDTH, castWork->angle, rgb);
    }
    if (castWork->index != 0) {
        rgb[0] >>= 1;
        rgb[1] >>= 1;
        rgb[2] >>= 1;
        effectDrawOuterGlowBand(coord, (s16)(castWork->angle + METABOLISM_GLOW_OUTER_BAND_OFFSET), METABOLISM_GLOW_BAND_WIDTH, rgb);
    }
}

/// Seeds the selected level's 8, 12 or 16 fixed fan bearings.
///
/// levelIndex is a side-effect-free 0..2 expression and wedgeIndex a writable
/// scalar local; both are evaluated repeatedly. Captures the level tuning,
/// complete sixteen-angle table and shared LCG; 4096 angle units form a turn.
#define METABOLISM_SEED_FAN_BEARINGS(levelIndex, wedgeIndex)                                                                        \
    {                                                                                                                               \
        enum { METABOLISM_WEDGE_JITTER_MASK = 0x3FF };                                                                              \
        s32 bearingRoll;                                                                                                            \
                                                                                                                                    \
        for ((wedgeIndex) = 0; (wedgeIndex) < D_metabolism_8012FB54[((levelIndex))].wedgeCount; (wedgeIndex)++) {                   \
            bearingRoll                         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                   \
            D_metabolism_8012FB78[(wedgeIndex)] = ((wedgeIndex) << 10) + (((u32)bearingRoll >> 16) & METABOLISM_WEDGE_JITTER_MASK); \
            gRandomLcgState                     = bearingRoll;                                                                      \
        }                                                                                                                           \
    }

void metabolismCastTask(Task* task)
{
    enum {
        METABOLISM_STATE_INITIALIZE    = 0,
        METABOLISM_STATE_GROWING       = 1,
        METABOLISM_STATE_FADING        = 2,
        METABOLISM_STATE_RELEASE       = 3,
        METABOLISM_CENTRE_LIFT         = 0x400,
        METABOLISM_INITIAL_RADIUS      = 0x80,
        METABOLISM_BRIGHTNESS_STEP     = 0x10,
        METABOLISM_FADE_END_BRIGHTNESS = 0x11,
        METABOLISM_SPARKLES_PER_TICK   = 3,
        METABOLISM_ANGLE_MASK          = 0xFFF,
        METABOLISM_TRIG_FRACTION_BITS  = 12,
    };
    EffectWork* effect;
    GfxCoord*   coord;
    EffectWork* spawned;
    s32         pan;
    s32         brightness;
    s32         particleIndex;
    s32         verticalProduct;

    effect = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(effect, task);
        return;
    }

    effect->age = effect->age + 1;
    switch (task->state) {
        case METABOLISM_STATE_INITIALIZE:
            coord->parent = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = -METABOLISM_CENTRE_LIFT;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state   = METABOLISM_STATE_GROWING;
            effect->index = (Gp_StateC08.attachId % 10) - 1;
            effect->angle = METABOLISM_INITIAL_RADIUS;
            METABOLISM_SEED_FAN_BEARINGS(effect->index, particleIndex);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_metabolism_8012FB6C[effect->index], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case METABOLISM_STATE_GROWING:
            actorRenderComposeCoord(coord);
            brightness = effect->scale;
            if (brightness < D_metabolism_8012FB54[effect->index].brightnessLimit) {
                brightness += METABOLISM_BRIGHTNESS_STEP;
            }
            effect->scale = brightness;
            effect->angle = effect->angle + D_metabolism_8012FB54[effect->index].radiusStep;
            {
                s32 bearingRoll;
                s32 yawRoll;

                for (particleIndex = 0; particleIndex < METABOLISM_SPARKLES_PER_TICK; particleIndex++) {
                    bearingRoll     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    yawRoll         = bearingRoll * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = bearingRoll;
                    effect->step    = ((u32)bearingRoll >> 16) & METABOLISM_ANGLE_MASK;
                    gRandomLcgState = yawRoll;
                    gfxRotMatrixY(&coord->coord, ((u32)yawRoll >> 16) & METABOLISM_ANGLE_MASK, GRAPHICS_ROTATION_COMPOSE);
                    // Retain the transform before overwriting the sparkle offset.
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&effect->move);
                    gte_rtv0();
                    gte_stsv(&effect->move);
                    effect->move.vx = (rcos(effect->step) * effect->angle) >> METABOLISM_TRIG_FRACTION_BITS;
                    verticalProduct = rsin(effect->step) * effect->angle;
                    effect->move.vz = 0;
                    effect->move.vy = verticalProduct >> METABOLISM_TRIG_FRACTION_BITS;
                    spawned         = effectSpawn(EFFECT_METABOLISM_SPARKLE, coord,
                                                  (s32)D_metabolism_8012FB54[effect->index].radiusLimit,
                                                  &effect->move);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
            }
            if (effect->angle >= D_metabolism_8012FB54[effect->index].radiusLimit) {
                task->state = METABOLISM_STATE_FADING;
            }
            for (particleIndex = 0; particleIndex < D_metabolism_8012FB54[effect->index].wedgeCount; particleIndex++) {
                _metabolismDrawFanWedge(coord, effect->angle, D_metabolism_8012FB78[particleIndex],
                                        effect->scale);
            }
            _metabolismDrawGlow(coord, effect);
            return;
        case METABOLISM_STATE_FADING:
            actorRenderComposeCoord(coord);
            for (particleIndex = 0; particleIndex < D_metabolism_8012FB54[effect->index].wedgeCount; particleIndex++) {
                _metabolismDrawFanWedge(coord, effect->angle, D_metabolism_8012FB78[particleIndex],
                                        effect->scale);
            }
            effect->scale = effect->scale - METABOLISM_BRIGHTNESS_STEP;
            effect->angle = effect->angle + D_metabolism_8012FB54[effect->index].radiusStep;
            if (effect->scale < METABOLISM_FADE_END_BRIGHTNESS) {
                task->state = METABOLISM_STATE_RELEASE;
            }
            _metabolismDrawGlow(coord, effect);
            return;
        case METABOLISM_STATE_RELEASE:
            effectKillTask(effect, task);
            return;
    }
}
#undef METABOLISM_SEED_FAN_BEARINGS

/// Advances the sparkle's parent-space Y position and composes it for drawing.
///
/// `deltaY` is a signed displacement in game-coordinate units; adding it to
/// the local translation must fit s32. Borrows a non-NULL writable `coord`
/// and its live, writable, acyclic parent chain. Refreshes `workm` through
/// that chain using the current composition pass and clobbers GTE registers.
static inline void _metabolismMoveSparkleCoord(GfxCoord* coord, s16 deltaY)
{
    coord->coord.t[1]  += deltaY;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void metabolismSparkleTask(Task* task)
{
    enum {
        METABOLISM_SPARKLE_STATE_INITIALIZE   = 0,
        METABOLISM_SPARKLE_STATE_SPINNING     = 1,
        METABOLISM_SPARKLE_STATE_FADING       = 2,
        METABOLISM_SPARKLE_Y_STEP             = 8,
        METABOLISM_SPARKLE_SIZE_MASK          = 0xFFF,
        METABOLISM_SPARKLE_ROTATION_MASK      = 0xFFF,
        METABOLISM_SPARKLE_PALETTE_SHIFT      = 12,
        METABOLISM_SPARKLE_SPINNING_PALETTE   = 1,
        METABOLISM_SPARKLE_FADING_PALETTE     = 3,
        METABOLISM_SPARKLE_INITIAL_BRIGHTNESS = 0xC0,
        METABOLISM_SPARKLE_BRIGHTNESS_STEP    = 0x18,
        METABOLISM_SPARKLE_FRAME_COUNT        = 8,
        METABOLISM_SPARKLE_FADING_ROLL_COUNT  = 3,
        METABOLISM_SPARKLE_ALWAYS_SPIN_LEVELS = 2,
    };
    EffectWork* work;
    GfxCoord*   coord;
    u16         peLevel;
    u16         variantRoll;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case METABOLISM_SPARKLE_STATE_INITIALIZE:
            work->move.vx = 0;
            work->move.vy = METABOLISM_SPARKLE_Y_STEP;
            work->move.vz = 0;
            work->angle   = task->spawnArg1.value & METABOLISM_SPARKLE_SIZE_MASK;
            // Only PE level 3 can select the fading variant; lower levels skip that random draw.
            peLevel = Gp_StateC08.attachId % 10U;
            if (peLevel - 1 < METABOLISM_SPARKLE_ALWAYS_SPIN_LEVELS ||
                (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT,
                 variantRoll     = (gRandomLcgState >> 16) % (u32)METABOLISM_SPARKLE_FADING_ROLL_COUNT, variantRoll != 0)) {
                task->state     = METABOLISM_SPARKLE_STATE_SPINNING;
                work->period    = METABOLISM_SPARKLE_SPINNING_PALETTE << METABOLISM_SPARKLE_PALETTE_SHIFT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = (gRandomLcgState >> 16) & METABOLISM_SPARKLE_ROTATION_MASK;
            } else {
                task->state  = METABOLISM_SPARKLE_STATE_FADING;
                work->scale  = METABOLISM_SPARKLE_INITIAL_BRIGHTNESS;
                work->period = METABOLISM_SPARKLE_FADING_PALETTE << METABOLISM_SPARKLE_PALETTE_SHIFT;
            }
            return;
        case METABOLISM_SPARKLE_STATE_SPINNING:
            _metabolismMoveSparkleCoord(coord, work->move.vy);
            // Advance on even ages, draw on odd ages, and retire at frame eight.
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < METABOLISM_SPARKLE_FRAME_COUNT) {
                if (work->age & 1) {
                    effectDrawSpinningBillboard(coord, work->index, work->angle,
                                                work->scale | work->period);
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
        case METABOLISM_SPARKLE_STATE_FADING:
            _metabolismMoveSparkleCoord(coord, work->move.vy);
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < METABOLISM_SPARKLE_FRAME_COUNT) {
                if (work->age & 1) {
                    effectDrawModulatedBillboard(coord, work->index, work->angle,
                                                 work->scale | work->period);
                    work->scale = work->scale - METABOLISM_SPARKLE_BRIGHTNESS_STEP;
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
    }
}

/// Reserves and projects the centre of a metabolism fan wedge.
///
/// `cursorSlot` points at the initialized scratch-stack cursor. Returns one
/// live complete block; the caller releases it through the same cursor slot.
/// Borrows the composed coordinate and `GsWSMATRIX`, and clobbers GTE registers.
/// The returned FLAG is from RTPS; SZ3 remains available for the caller to
/// capture before another transform. Negative FLAG values are rejected by the caller.
static inline EffectCentreScratch* _metabolismProjectFanCentre(const GfxCoord* coord, void** cursorSlot)
{
    EffectCentreScratch* scratch;

    // Project the composed centre once, narrowing its translation to signed halfwords.
    scratch                = SCRATCH_PUSH_AT(cursorSlot, EffectCentreScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    return scratch;
}

/// Draws one additive Gouraud wedge of the metabolism cast's screen-space fan.
///
/// `coord` supplies a composed translation in the input space of `GsWSMATRIX`;
/// its rotation is unused and XYZ narrows to signed 16-bit coordinate units.
/// The rim radius in pixels is `radius * 128 / (SZ3 / 4 + 1)`, with signed
/// arithmetic and division toward zero. `bearing` uses 4096 units per turn,
/// counterclockwise from screen-down; the rim corners are at bearing +/- 32.
/// `brightness` supplies green (0..255), with red / 2 and blue alternating
/// between full and half brightness on display-frame parity. The rim is black.
/// A negative GTE FLAG rejects the triangle. Reserves/releases one complete
/// scratch block and appends a triangle/blend-command pair to the unchecked
/// frame arena; inputs must stay clear of both, and packets live through GPU drawing.
static void _metabolismDrawFanWedge(const GfxCoord* coord, s16 radius, s16 bearing, s16 brightness)
{
    enum {
        METABOLISM_FAN_PERSPECTIVE_SCALE  = 128,
        METABOLISM_FAN_HALF_ANGLE         = 0x20,
        METABOLISM_FAN_TRIG_FRACTION_BITS = 12,
    };
    void**               cursorSlot;
    EffectCentreScratch* scratch;
    POLY_G3*             triangle;
    s32                  rightRimAngle;
    s32                  leftRimAngle;

    cursorSlot = SCRATCH_HEAD_ADDR;
    scratch    = _metabolismProjectFanCentre(coord, cursorSlot);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        triangle       = gGpuPrimCursor;
        gGpuPrimCursor = triangle + 1;
        setPolyG3(triangle);
        // Fade from the coloured centre to a black rim spanning 64 angle units.
        setRGB0(triangle, brightness >> 1, brightness, brightness >> (gDisplayState.animFrame & 1));
        setRGB1(triangle, 0, 0, 0);
        setRGB2(triangle, 0, 0, 0);
        scratch->screenExtent = (radius * METABOLISM_FAN_PERSPECTIVE_SCALE) / scratch->depth;
        rightRimAngle         = bearing;
        leftRimAngle          = rightRimAngle - METABOLISM_FAN_HALF_ANGLE;
        triangle->x0          = scratch->screenX;
        triangle->y0          = scratch->screenY;
        triangle->x1          = scratch->screenX + ((scratch->screenExtent * rsin(leftRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        triangle->y1          = scratch->screenY + ((scratch->screenExtent * rcos(leftRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        rightRimAngle        += METABOLISM_FAN_HALF_ANGLE;
        triangle->x2          = scratch->screenX + ((scratch->screenExtent * rsin(rightRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        triangle->y2          = scratch->screenY + ((scratch->screenExtent * rcos(rightRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                triangle);
        gpuSetPrimitiveBlendMode(triangle, GPU_BLEND_ADD, scratch->depth);
    }
    SCRATCH_POP_AT(cursorSlot, EffectCentreScratch);
}
