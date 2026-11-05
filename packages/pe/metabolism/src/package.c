#include "metabolism.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effects.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/gfx_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

PE2_EDITOR_LABEL("Cast")
/// How far above the player's root the cast sits, and its radius as it starts.
PE2_OPTION_DEF(CAST_HEIGHT, int, 0x400)
PE2_OPTION_DEF(START_RADIUS, int, 0x80)
/// Brightness gained each frame up to the level's, and lost each frame as the cast fades.
PE2_OPTION_DEF(BRIGHTNESS_STEP, int, 0x10)
/// Sparkles thrown off each frame of growth.
PE2_OPTION_DEF(SPARKLES_PER_FRAME, int, 3)

PE2_EDITOR_LABEL("Wedges")
/// How far apart the wedges are, and the range of the random bearing added to each, a power of two, in a 4096-unit
/// turn.
PE2_OPTION_DEF(WEDGE_SPACING, int, 1024)
PE2_OPTION_DEF(WEDGE_JITTER, int, 1024)
/// A wedge's length as a multiple of the cast's radius in perspective, and half its width, in a 4096-unit turn.
PE2_OPTION_DEF(WEDGE_LENGTH, int, 128)
PE2_OPTION_DEF(WEDGE_HALF_WIDTH, int, 0x20)

PE2_EDITOR_LABEL("Glow")
/// How much dimmer red and blue are than green, as shifts.
PE2_OPTION_DEF(RED_SHIFT, int, 2)
PE2_OPTION_DEF(BLUE_SHIFT, int, 1)
/// The central disc's radius, as the cast's shifted down.
PE2_OPTION_DEF(DISC_RADIUS_SHIFT, int, 1)
/// The glow bands' width, how much dimmer each is than what it surrounds, as a shift, and how far out from the cast's
/// radius the outer band (from level 2) sits.
PE2_OPTION_DEF(BAND_WIDTH, int, 0x80)
PE2_OPTION_DEF(BAND_DIM_SHIFT, int, 1)
PE2_OPTION_DEF(OUTER_BAND_OFFSET, int, 0x200)

PE2_EDITOR_LABEL("Sparkles")
/// How fast a sparkle sinks.
PE2_OPTION_DEF(SPARKLE_SINK_SPEED, int, 8)
/// From this level, one sparkle in this many is drawn plain and fading instead of spinning: its brightness as it
/// starts, and what it loses each drawn frame.
PE2_OPTION_DEF(FADING_SPARKLE_LEVEL, int, 3)
PE2_OPTION_DEF(FADING_SPARKLE_ODDS, int, 3)
PE2_OPTION_DEF(FADING_SPARKLE_BRIGHTNESS, int, 0xC0)
PE2_OPTION_DEF(FADING_SPARKLE_FADE_STEP, int, 0x18)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(WEDGE_JITTER)) && PE2_OPTION(WEDGE_JITTER) <= 4096, wedge_jitter_is_a_mask);
STATIC_ASSERT(PE2_OPTION(BRIGHTNESS_STEP) > 0 && PE2_OPTION(FADING_SPARKLE_ODDS) > 0, steps_and_odds);

/// The cast's tuning at one Parasite Energy level, by the level digit of the attachment id less one. Brightness is the
/// green channel's; the radius sizes the fan and the bands, twice the disc's, and is how far out sparkles spawn.
typedef struct {
    s16 wedgeCount;      // Glow wedges in the fan, one random angle each; the angle table holds 16
    s16 brightnessLimit; // Brightness the cast rises to
    s16 radiusStep;      // Radius gained per frame, while the cast grows and while it fades
    s16 radiusLimit;     // Radius that ends the growth; also the sprite radius each spark is spawned with
} _LevelTuning;

enum { MAX_CAST_WEDGES = 16 };

/// Levels 1 to 3.
static _LevelTuning _gLevelTuning[] = {
    { 8, 0x0080, 0x0020, 0x0400 },
    { 12, 0x00B0, 0x0030, 0x0500 },
    { MAX_CAST_WEDGES, 0x00E0, 0x0040, 0x0600 },
};

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[ARRAY_SIZE(_gLevelTuning)] = { 0xE01F0001, 0xE0220001, 0xE0250001 };

/// The wedges' bearings, with a little jitter, chosen when a cast starts.
static s16 _gFanWedgeBearings[MAX_CAST_WEDGES];

static void _drawFanWedge(const GfxCoord* coord, s16 radius, s16 bearing, s16 brightness);

/// Draws the cast's glow: a disc, drawn twice to brighten it, inside a glow band that flickers bluer, and from level 2
/// a fainter one.
static inline void _drawGlow(const GfxCoord* coord, const EffectWork* castWork)
{
    u8 rgb[3];

    rgb[0] = castWork->scale >> PE2_OPTION(RED_SHIFT);
    rgb[1] = castWork->scale;
    rgb[2] = castWork->scale >> PE2_OPTION(BLUE_SHIFT);
    effectDrawGouraudDisc(coord, castWork->angle >> PE2_OPTION(DISC_RADIUS_SHIFT), rgb);
    effectDrawGouraudDisc(coord, castWork->angle >> PE2_OPTION(DISC_RADIUS_SHIFT), rgb);
    rgb[0] >>= PE2_OPTION(BAND_DIM_SHIFT);
    rgb[1] >>= PE2_OPTION(BAND_DIM_SHIFT);
    rgb[2] >>= PE2_OPTION(BAND_DIM_SHIFT);
    effectDrawOuterGlowBand(coord, castWork->angle, PE2_OPTION(BAND_WIDTH), rgb);
    if (castWork->age & 1) {
        rgb[1] >>= 1;
        rgb[2] <<= 1;
        effectDrawOuterGlowBand(coord, PE2_OPTION(BAND_WIDTH), castWork->angle, rgb);
    }
    if (castWork->index != 0) {
        rgb[0] >>= PE2_OPTION(BAND_DIM_SHIFT);
        rgb[1] >>= PE2_OPTION(BAND_DIM_SHIFT);
        rgb[2] >>= PE2_OPTION(BAND_DIM_SHIFT);
        effectDrawOuterGlowBand(coord, (s16)(castWork->angle + PE2_OPTION(OUTER_BAND_OFFSET)), PE2_OPTION(BAND_WIDTH), rgb);
    }
}

/// Chooses the level's wedge bearings.
#define SEED_FAN_BEARINGS(levelIndex, wedgeIndex)                                                                                                        \
    {                                                                                                                                                    \
        s32 bearingRoll;                                                                                                                                 \
                                                                                                                                                         \
        for ((wedgeIndex) = 0; (wedgeIndex) < _gLevelTuning[(levelIndex)].wedgeCount; (wedgeIndex)++) {                                                  \
            bearingRoll                      = RANDOM_LCG_NEXT(gRandomLcgState);                                                                         \
            _gFanWedgeBearings[(wedgeIndex)] = ((wedgeIndex) * PE2_OPTION(WEDGE_SPACING)) + (((u32)bearingRoll >> 16) & (PE2_OPTION(WEDGE_JITTER) - 1)); \
            gRandomLcgState                  = bearingRoll;                                                                                              \
        }                                                                                                                                                \
    }

void metabolismCastTask(Task* task)
{
    enum {
        STATE_INITIALIZE   = 0,
        STATE_GROWING      = 1,
        STATE_FADING       = 2,
        STATE_RELEASE      = 3,
        ANGLE_MASK         = 0xFFF,
        TRIG_FRACTION_BITS = 12,
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
    if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(effect, task);
        return;
    }

    effect->age = effect->age + 1;
    switch (task->state) {
        case STATE_INITIALIZE:
            coord->parent = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = -PE2_OPTION(CAST_HEIGHT);
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state   = STATE_GROWING;
            effect->index = (gAttachmentState.attachId % 10) - 1;
            effect->angle = PE2_OPTION(START_RADIUS);
            SEED_FAN_BEARINGS(effect->index, particleIndex);
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                     = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gCastSoundScripts[effect->index], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case STATE_GROWING:
            actorRenderComposeCoord(coord);
            brightness = effect->scale;
            if (brightness < _gLevelTuning[effect->index].brightnessLimit) {
                brightness += PE2_OPTION(BRIGHTNESS_STEP);
            }
            effect->scale = brightness;
            effect->angle = effect->angle + _gLevelTuning[effect->index].radiusStep;
            {
                s32 bearingRoll;
                s32 yawRoll;

                for (particleIndex = 0; particleIndex < PE2_OPTION(SPARKLES_PER_FRAME); particleIndex++) {
                    bearingRoll     = RANDOM_LCG_NEXT(gRandomLcgState);
                    yawRoll         = RANDOM_LCG_NEXT(bearingRoll);
                    gRandomLcgState = bearingRoll;
                    effect->step    = ((u32)bearingRoll >> 16) & ANGLE_MASK;
                    gRandomLcgState = yawRoll;
                    gfxRotMatrixY(&coord->coord, ((u32)yawRoll >> 16) & ANGLE_MASK, GRAPHICS_ROTATION_COMPOSE);
                    // Retain the transform before overwriting the sparkle offset.
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&effect->move);
                    gte_rtv0();
                    gte_stsv(&effect->move);
                    effect->move.vx = (rcos(effect->step) * effect->angle) >> TRIG_FRACTION_BITS;
                    verticalProduct = rsin(effect->step) * effect->angle;
                    effect->move.vz = 0;
                    effect->move.vy = verticalProduct >> TRIG_FRACTION_BITS;
                    spawned         = effectSpawn(EFFECT_METABOLISM_SPARKLE, coord,
                                                  (s32)_gLevelTuning[effect->index].radiusLimit,
                                                  &effect->move);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
            }
            if (effect->angle >= _gLevelTuning[effect->index].radiusLimit) {
                task->state = STATE_FADING;
            }
            for (particleIndex = 0; particleIndex < _gLevelTuning[effect->index].wedgeCount; particleIndex++) {
                _drawFanWedge(coord, effect->angle, _gFanWedgeBearings[particleIndex],
                              effect->scale);
            }
            _drawGlow(coord, effect);
            return;
        case STATE_FADING:
            actorRenderComposeCoord(coord);
            for (particleIndex = 0; particleIndex < _gLevelTuning[effect->index].wedgeCount; particleIndex++) {
                _drawFanWedge(coord, effect->angle, _gFanWedgeBearings[particleIndex],
                              effect->scale);
            }
            effect->scale = effect->scale - PE2_OPTION(BRIGHTNESS_STEP);
            effect->angle = effect->angle + _gLevelTuning[effect->index].radiusStep;
            if (effect->scale < (PE2_OPTION(BRIGHTNESS_STEP) + 1)) {
                task->state = STATE_RELEASE;
            }
            _drawGlow(coord, effect);
            return;
        case STATE_RELEASE:
            effectKillTask(effect, task);
            return;
    }
}
#undef SEED_FAN_BEARINGS

/// Moves a sparkle by `deltaY` and composes it.
static inline void _moveSparkleCoord(GfxCoord* coord, s16 deltaY)
{
    coord->coord.t[1]  += deltaY;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void metabolismSparkleTask(Task* task)
{
    enum {
        SPARKLE_STATE_INITIALIZE = 0,
        SPARKLE_STATE_SPINNING   = 1,
        SPARKLE_STATE_FADING     = 2,
        SPARKLE_SIZE_MASK        = 0xFFF,
        SPARKLE_ROTATION_MASK    = 0xFFF,
        SPARKLE_PALETTE_SHIFT    = 12,
        SPARKLE_SPINNING_PALETTE = 1,
        SPARKLE_FADING_PALETTE   = 3,
        SPARKLE_FRAME_COUNT      = 8,
    };
    EffectWork* work;
    GfxCoord*   coord;
    u16         peLevel;
    u16         variantRoll;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case SPARKLE_STATE_INITIALIZE:
            work->move.vx = 0;
            work->move.vy = PE2_OPTION(SPARKLE_SINK_SPEED);
            work->move.vz = 0;
            work->angle   = task->spawnArg1.value & SPARKLE_SIZE_MASK;
            // Only PE level 3 can select the fading variant; lower levels skip that random draw.
            peLevel = gAttachmentState.attachId % 10U;
            if (peLevel - 1 < (PE2_OPTION(FADING_SPARKLE_LEVEL) - 1) ||
                (RANDOM_LCG_ADVANCE(),
                 variantRoll = RANDOM_LCG_DRAW() % (u32)PE2_OPTION(FADING_SPARKLE_ODDS), variantRoll != 0)) {
                task->state  = SPARKLE_STATE_SPINNING;
                work->period = SPARKLE_SPINNING_PALETTE << SPARKLE_PALETTE_SHIFT;
                RANDOM_LCG_ADVANCE();
                work->scale = RANDOM_LCG_DRAW() & SPARKLE_ROTATION_MASK;
            } else {
                task->state  = SPARKLE_STATE_FADING;
                work->scale  = PE2_OPTION(FADING_SPARKLE_BRIGHTNESS);
                work->period = SPARKLE_FADING_PALETTE << SPARKLE_PALETTE_SHIFT;
            }
            return;
        case SPARKLE_STATE_SPINNING:
            _moveSparkleCoord(coord, work->move.vy);
            // Advance on even ages, draw on odd ages, and retire at frame eight.
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < SPARKLE_FRAME_COUNT) {
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
        case SPARKLE_STATE_FADING:
            _moveSparkleCoord(coord, work->move.vy);
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < SPARKLE_FRAME_COUNT) {
                if (work->age & 1) {
                    effectDrawModulatedBillboard(coord, work->index, work->angle,
                                                 work->scale | work->period);
                    work->scale = work->scale - PE2_OPTION(FADING_SPARKLE_FADE_STEP);
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
    }
}

/// Reserves a scratch block and projects the coordinate's center into it; the caller releases it.
static inline EffectCentreScratch* _projectFanCentre(const GfxCoord* coord, void** cursorSlot)
{
    EffectCentreScratch* scratch;

    // Project the composed centre once, narrowing its translation to signed halfwords.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK_AT(cursorSlot, EffectCentreScratch);
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

/// Draws an additive wedge of the fan at `bearing`, from a lit center to a black rim, its blue flickering; a point
/// failing to project draws nothing.
static void _drawFanWedge(const GfxCoord* coord, s16 radius, s16 bearing, s16 brightness)
{
    enum {
        FAN_TRIG_FRACTION_BITS = 12,
    };
    void**               cursorSlot;
    EffectCentreScratch* scratch;
    POLY_G3*             triangle;
    s32                  rightRimAngle;
    s32                  leftRimAngle;

    cursorSlot = SCRATCH_STACK_CURSOR_SLOT;
    scratch    = _projectFanCentre(coord, cursorSlot);
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
        scratch->screenExtent = (radius * PE2_OPTION(WEDGE_LENGTH)) / scratch->depth;
        rightRimAngle         = bearing;
        leftRimAngle          = rightRimAngle - PE2_OPTION(WEDGE_HALF_WIDTH);
        triangle->x0          = scratch->screenX;
        triangle->y0          = scratch->screenY;
        triangle->x1          = scratch->screenX + ((scratch->screenExtent * rsin(leftRimAngle)) >> FAN_TRIG_FRACTION_BITS);
        triangle->y1          = scratch->screenY + ((scratch->screenExtent * rcos(leftRimAngle)) >> FAN_TRIG_FRACTION_BITS);
        rightRimAngle        += PE2_OPTION(WEDGE_HALF_WIDTH);
        triangle->x2          = scratch->screenX + ((scratch->screenExtent * rsin(rightRimAngle)) >> FAN_TRIG_FRACTION_BITS);
        triangle->y2          = scratch->screenY + ((scratch->screenExtent * rcos(rightRimAngle)) >> FAN_TRIG_FRACTION_BITS);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                triangle);
        gpuSetPrimitiveBlendMode(triangle, GPU_BLEND_ADD, scratch->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK_AT(cursorSlot, EffectCentreScratch);
}
