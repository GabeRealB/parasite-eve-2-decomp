#include "inferno.h"

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
#include <gameplay/pad_script.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/mem.h>
#include <engine/scratch.h>
#include <engine/sound.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

PE2_EDITOR_LABEL("Fan rings")
/// Level 2's ring of fans around the center one: how many, and how far apart in a 4096-unit turn, starting half a
/// spacing in.
PE2_OPTION_DEF(LEVEL_2_FANS, int, 4)
PE2_OPTION_DEF(LEVEL_2_FAN_SPACING, int, 0x400)
/// Level 3's two bursts: how many fans each, and how far apart, the inner starting half a spacing in.
PE2_OPTION_DEF(BURST_FANS, int, 6)
PE2_OPTION_DEF(BURST_FAN_SPACING, int, 0x2AA)
/// How far out the inner ring (and level 2's) and the outer burst's ring stand.
PE2_OPTION_DEF(INNER_FANS_RADIUS, int, 0x600)
PE2_OPTION_DEF(OUTER_FANS_RADIUS, int, 0x900)
/// The frames level 3's inner and outer bursts come on.
PE2_OPTION_DEF(INNER_BURST_FRAME, int, 12)
PE2_OPTION_DEF(OUTER_BURST_FRAME, int, 24)

PE2_EDITOR_LABEL("Screen wash")
/// The wash's brightness at its start and after the last burst, and how much dimmer green and blue are than red, as
/// shifts.
PE2_OPTION_DEF(WASH_BRIGHTNESS, int, 0xFF)
PE2_OPTION_DEF(WASH_GREEN_SHIFT, int, 1)
PE2_OPTION_DEF(WASH_BLUE_SHIFT, int, 2)
/// Between level 3's bursts it pulses by this each frame, up to the ceiling, and it fades by the fade step.
PE2_OPTION_DEF(WASH_PULSE_STEP, int, 0x10)
PE2_OPTION_DEF(WASH_PULSE_CEILING, int, 0xF0)
PE2_OPTION_DEF(WASH_FADE_STEP, int, 8)

PE2_EDITOR_LABEL("Flame fans")
/// A fan's brightness as it starts, and how high the center fan of level 3 rises before it fades.
PE2_OPTION_DEF(FAN_BRIGHTNESS, int, 0x80)
PE2_OPTION_DEF(FAN_LIFT_LIMIT, int, 0xC00)
/// A drifting fan moves this much of its offset from the cast each frame, in 4096ths.
PE2_OPTION_DEF(FAN_DRIFT_SPEED, int, 0x80)

PE2_EDITOR_LABEL("Rumble")
/// The pad's rumble as level 1, level 2 and level 3 start, at level 3's inner burst too, and as its outer burst comes,
/// in frames, and its strength from start to end.
PE2_OPTION_DEF(LEVEL_1_RUMBLE_FRAMES, int, 16)
PE2_OPTION_DEF(LEVEL_2_RUMBLE_FRAMES, int, 20)
PE2_OPTION_DEF(LEVEL_3_RUMBLE_FRAMES, int, 12)
PE2_OPTION_DEF(OUTER_BURST_RUMBLE_FRAMES, int, 24)
PE2_OPTION_DEF(RUMBLE_START, int, 0xFF)
PE2_OPTION_DEF(RUMBLE_END, int, 8)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(PE2_OPTION(LEVEL_2_FANS) > 0 && PE2_OPTION(BURST_FANS) > 0 && PE2_OPTION(LEVEL_2_FAN_SPACING) > 0 &&
                  PE2_OPTION(BURST_FAN_SPACING) > 0,
              the_rings_have_fans);
STATIC_ASSERT(PE2_OPTION(INNER_BURST_FRAME) < PE2_OPTION(OUTER_BURST_FRAME), the_bursts_come_in_order);
STATIC_ASSERT(PE2_OPTION(WASH_FADE_STEP) > 0, the_wash_fades);

enum {
    /// Vertices of a fan's rims, and quads joining them.
    FAN_SEGMENT_COUNT = 6,

    /// A sixth of a turn, truncated: the last segment closes the gap of 686.
    FAN_SEGMENT_YAW_STEP = 0x2AA,

    /// The fan's two bands: one whose top rises as it grows, one at its shape's lift.
    FAN_RISING_BAND = 0,

    FAN_CONSTANT_LIFT_BAND = 1,

    FAN_BAND_COUNT = 2,

    FAN_TEXTURE_FRAME_COUNT = 6,
};

/// Each band segment's random offset into the fan's animation, which the fan task allocates and frees.
typedef union {
    struct {
        u8 risingBand[FAN_SEGMENT_COUNT];         // Cell phase of the band whose lift is period + lift
        u8 constantLiftBand[FAN_SEGMENT_COUNT];   // Cell phase of the band whose lift is the shape's lift alone
    } band;
    u8 byBand[FAN_BAND_COUNT][FAN_SEGMENT_COUNT]; // [0] risingBand, [1] constantLiftBand
    u8 byte[FAN_BAND_COUNT * FAN_SEGMENT_COUNT];  // risingBand, then constantLiftBand
} _FanTexturePhase;

/// A band's rims, built in the world, and the current quad's screen corners (X low, Y high).
typedef struct {
    SVECTOR topRing[FAN_SEGMENT_COUNT];    // Wider rim, lifted along local -Y; quad vertices 0 and 1
    SVECTOR bottomRing[FAN_SEGMENT_COUNT]; // Narrower rim in the local XZ plane; quad vertices 2 and 3
    s32     sxy0;                          // Packed screen position of the current quad's vertex 0
    s32     sxy1;                          // Packed screen position of vertex 1
    s32     sxy2;                          // Packed screen position of vertex 2
    s32     sxy3;                          // Packed screen position of vertex 3
} _FanScratch;

PE2_EDITOR_LABEL("Bands")
/// A fan's two bands' base radius, lift and spread.
PE2_OPTION_DEF(BANDS, EffectBandShape[2], {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
})
PE2_EDITOR_LABEL("")

static EffectBandShape _gFanBandShapes[FAN_BAND_COUNT] = PE2_OPTION(BANDS);

/// How a fan of each variant grows and fades, each frame: the variants are in the order the fan task takes them.
typedef struct {
    s16 fade;   // Brightness lost; the fan goes once it has less than this left
    s16 radius; // Radius gained by both bands
    s16 lift;   // Height the rising band's top gains; the center fan of level 3 rises to the lift limit, then fades
    s16 spread; // Spread both tops gain
} _FanVariant;
PE2_TYPE(_FanVariant)

PE2_EDITOR_LABEL("Fan variants")
/// Grow then fade, stationary burst, drifting burst, wide ignition, shallow ring, fast ignition.
PE2_OPTION_DEF(VARIANTS, _FanVariant[6], {
    { 4, 0x20, 0xC0, 0x18 },
    { 8, 0x20, 0xC0, 0x18 },
    { 8, 0x20, 0xC0, 0x18 },
    { 6, 0x40, 0xC0, 0x10 },
    { 6, 0x40, 0x40, 0x18 },
    { 6, 0x80, 0x20, 0x20 },
})
PE2_EDITOR_LABEL("")

/// A variant's growth and fade, read as constants: no table of them is stored.
#define FAN_VARIANT(variant, field) PE2_OPTION_AT(VARIANTS, variant, field)

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[] = { 0xE0100001, 0xE0130001, 0xE00D0001 };

/// The fan's texture: six 40-texel cells.
enum {
    FAN_TRIG_FRACTION_BITS = 12,
    FAN_TEXTURE_DEPTH_4BIT = 0,
    FAN_TEXTURE_PAGE_X     = 640, // VRAM words
    FAN_TEXTURE_PAGE_Y     = 0,   // VRAM rows
    FAN_PALETTE_X          = 32,  // VRAM words
    FAN_PALETTE_Y          = 266, // VRAM rows
    FAN_TEXTURE_CELL_SIZE  = 40,  // Texels per cell edge
    FAN_TEXTURE_V          = 96   // First row within the texture page
};

static void _drawScreenWash(s16 intensity);
static void _drawRisingFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _FanTexturePhase* texturePhase);
static void _drawConstantLiftFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _FanTexturePhase* texturePhase);

/// Builds a band's rims around the coordinate: the lifted one at `liftedRadius`, the base one at `baseRadius`.
#define BUILD_FAN_RIMS(scratch, coord, baseRadius, liftedRadius, lift, segmentIndex, yaw, bottomVertex)                          \
    gte_SetTransMatrix(&GsWSMATRIX);                                                                                             \
    /* Wider lifted rim, then the narrower rim in the local XZ plane. */                                                         \
    for ((segmentIndex) = 0; (segmentIndex) < FAN_SEGMENT_COUNT; (segmentIndex)++) {                                             \
        (yaw)                                 = (segmentIndex) * FAN_SEGMENT_YAW_STEP;                                           \
        (scratch)->topRing[(segmentIndex)].vx = (rsin((yaw)) * (liftedRadius)) >> FAN_TRIG_FRACTION_BITS;                        \
        (scratch)->topRing[(segmentIndex)].vy = -(lift);                                                                         \
        (scratch)->topRing[(segmentIndex)].vz = (rcos((yaw)) * (liftedRadius)) >> FAN_TRIG_FRACTION_BITS;                        \
        gte_SetRotMatrix(&(coord)->workm);                                                                                       \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                           \
        gte_rtv0();                                                                                                              \
        gte_stsv(&(scratch)->topRing[(segmentIndex)]);                                                                           \
        (scratch)->topRing[(segmentIndex)].vx    = (u16)(scratch)->topRing[(segmentIndex)].vx + (u16)(coord)->workm.t[0];        \
        (scratch)->topRing[(segmentIndex)].vy    = (u16)(scratch)->topRing[(segmentIndex)].vy + (u16)(coord)->workm.t[1];        \
        (scratch)->topRing[(segmentIndex)].vz    = (u16)(scratch)->topRing[(segmentIndex)].vz + (u16)(coord)->workm.t[2];        \
        (scratch)->bottomRing[(segmentIndex)].vx = (rsin((yaw)) * (baseRadius)) >> FAN_TRIG_FRACTION_BITS;                       \
        /* Address the lower rim within the complete scratch block's byte extent. */                                             \
        (bottomVertex)     = (SVECTOR*)((u8*)(scratch) + (segmentIndex) * sizeof(SVECTOR) + OFFSET_OF(_FanScratch, bottomRing)); \
        (bottomVertex)->vy = 0;                                                                                                  \
        (bottomVertex)->vz = (rcos((yaw)) * (baseRadius)) >> FAN_TRIG_FRACTION_BITS;                                             \
        gte_SetRotMatrix(&(coord)->workm);                                                                                       \
        gte_ldv0(&(scratch)->bottomRing[(segmentIndex)]);                                                                        \
        gte_rtv0();                                                                                                              \
        gte_stsv(&(scratch)->bottomRing[(segmentIndex)]);                                                                        \
        (scratch)->bottomRing[(segmentIndex)].vx = (u16)(scratch)->bottomRing[(segmentIndex)].vx + (u16)(coord)->workm.t[0];     \
        (bottomVertex)->vy                       = (u16)(bottomVertex)->vy + (u16)(coord)->workm.t[1];                           \
        (bottomVertex)->vz                       = (u16)(bottomVertex)->vz + (u16)(coord)->workm.t[2];                           \
    }

/// Spawns a ring of fans `work->scale` out, from `yaw` by `yawStep` until `yawEnd`.
#define SPAWN_OFFSET_FANS(coord, work, yaw, yawStep, yawEnd, fanVariant)                                    \
    do {                                                                                                    \
        (work)->move.vx = (rsin((yaw)) * (work)->scale) >> CAST_TRIG_FRACTION_BITS;                         \
        (work)->move.vz = (rcos((yaw)) * (work)->scale) >> CAST_TRIG_FRACTION_BITS;                         \
        (yaw)          += (yawStep);                                                                        \
        effectSpawn((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), (coord), (fanVariant), &(work)->move); \
    } while ((yaw) < (yawEnd))

void infernoCastTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE     = 0,
        CAST_STATE_LEVEL_ONE      = 1,
        CAST_STATE_LEVEL_TWO      = 5,
        CAST_STATE_LEVEL_THREE    = 9,
        CAST_STATE_INNER_BURST    = 10,
        CAST_STATE_OUTER_BURST    = 11,
        CAST_STATE_FADING         = 12,
        CAST_LEVEL_STATE_STRIDE   = 4,
        CAST_INITIAL_RADIUS       = 0x200, // Replaced before any ring uses it
        CAST_LEVEL_TWO_YAW_START  = PE2_OPTION(LEVEL_2_FAN_SPACING) / 2,
        CAST_LEVEL_TWO_YAW_END    = CAST_LEVEL_TWO_YAW_START + PE2_OPTION(LEVEL_2_FANS) * PE2_OPTION(LEVEL_2_FAN_SPACING),
        CAST_INNER_YAW_START      = PE2_OPTION(BURST_FAN_SPACING) / 2,
        CAST_INNER_YAW_END        = CAST_INNER_YAW_START + PE2_OPTION(BURST_FANS) * PE2_OPTION(BURST_FAN_SPACING),
        CAST_OUTER_YAW_END        = PE2_OPTION(BURST_FANS) * PE2_OPTION(BURST_FAN_SPACING),
        CAST_TRIG_FRACTION_BITS   = 12,
        CAST_FAN_GROW_THEN_FADE   = 0,
        CAST_FAN_STATIONARY_BURST = 1,
        CAST_FAN_DRIFTING_BURST   = 2,
        CAST_FAN_WIDE_IGNITION    = 3,
        CAST_FAN_SHALLOW_RING     = 4,
        CAST_FAN_FAST_IGNITION    = 5,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         yaw;
    s32         pan;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(work, task);
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    work->age = work->age + 1;
    switch (task->state) {
        case CAST_STATE_INITIALIZE:
            work->scale = CAST_INITIAL_RADIUS;
            work->angle = PE2_OPTION(WASH_BRIGHTNESS);
            pan         = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gCastSoundScripts[(u16)(gAttachmentState.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            // The PE level chooses one of three state chains, starting at 1, 5 or 9.
            task->state = ((u16)(gAttachmentState.attachId % 10) - 1) * CAST_LEVEL_STATE_STRIDE + CAST_STATE_LEVEL_ONE;
            return;
        case CAST_STATE_LEVEL_ONE:
            effectSpawn((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, CAST_FAN_WIDE_IGNITION, NULL);
            effectSpawn((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, CAST_FAN_FAST_IGNITION, NULL);
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            padScriptSpawnVariableMotorRamp(PE2_OPTION(LEVEL_1_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            task->state = CAST_STATE_FADING;
            return;
        case CAST_STATE_LEVEL_TWO:
            yaw = CAST_LEVEL_TWO_YAW_START;
            _drawScreenWash(work->angle);
            effectSpawn((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, CAST_FAN_WIDE_IGNITION, NULL);
            work->scale = PE2_OPTION(INNER_FANS_RADIUS);
            SPAWN_OFFSET_FANS(coord, work, yaw, PE2_OPTION(LEVEL_2_FAN_SPACING), CAST_LEVEL_TWO_YAW_END, CAST_FAN_SHALLOW_RING);
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            padScriptSpawnVariableMotorRamp(PE2_OPTION(LEVEL_2_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            task->state = CAST_STATE_FADING;
            return;
        case CAST_STATE_LEVEL_THREE:
            effectSpawn((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, CAST_FAN_GROW_THEN_FADE, NULL);
            padScriptSpawnVariableMotorRamp(PE2_OPTION(LEVEL_3_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            task->state = CAST_STATE_INNER_BURST;
            return;
        case CAST_STATE_INNER_BURST:
            _drawScreenWash(work->angle);
            work->angle = work->angle - PE2_OPTION(WASH_PULSE_STEP);
            if (work->age != PE2_OPTION(INNER_BURST_FRAME)) {
                return;
            }
            // The level-three bursts share the cast's absolute age counter.
            work->scale = PE2_OPTION(INNER_FANS_RADIUS);
            yaw         = CAST_INNER_YAW_START;
            SPAWN_OFFSET_FANS(coord, work, yaw, PE2_OPTION(BURST_FAN_SPACING), CAST_INNER_YAW_END, CAST_FAN_STATIONARY_BURST);
            padScriptSpawnVariableMotorRamp(PE2_OPTION(LEVEL_3_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            task->state = CAST_STATE_OUTER_BURST;
            return;
        case CAST_STATE_OUTER_BURST:
            _drawScreenWash(work->angle);
            if (work->angle < PE2_OPTION(WASH_PULSE_CEILING)) {
                work->angle = work->angle + PE2_OPTION(WASH_PULSE_STEP);
            }
            if (work->age != PE2_OPTION(OUTER_BURST_FRAME)) {
                return;
            }
            work->scale = PE2_OPTION(OUTER_FANS_RADIUS);
            yaw         = 0;
            SPAWN_OFFSET_FANS(coord, work, yaw, PE2_OPTION(BURST_FAN_SPACING), CAST_OUTER_YAW_END, CAST_FAN_DRIFTING_BURST);
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            padScriptSpawnVariableMotorRamp(PE2_OPTION(OUTER_BURST_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            task->state = CAST_STATE_FADING;
            work->angle = PE2_OPTION(WASH_BRIGHTNESS);
            return;
        case CAST_STATE_FADING:
            _drawScreenWash(work->angle);
            if (work->angle >= (PE2_OPTION(WASH_FADE_STEP) + 1)) {
                work->angle = work->angle - PE2_OPTION(WASH_FADE_STEP);
                return;
            }
            break;
        default:
            return;
    }
    effectKillTask(work, task);
}

#undef SPAWN_OFFSET_FANS

/// Draws the cast's additive red-amber wash over the screen, steady through its shake.
static void _drawScreenWash(s16 intensity)
{
    enum {
        SCREEN_HALF_WIDTH         = 160, // Pixels from the centred screen origin
        SCREEN_HALF_HEIGHT        = 120,
        SCREEN_WASH_SORTING_DEPTH = 0x30 // Unscaled depth passed to both OT insertions
    };
    POLY_F4*      quad;
    DisplayState* display;
    s32           left;
    s32           right;
    s32           top;
    s32           bottom;
    s32           sortingDepth;

    display      = &gDisplayState;
    left         = -SCREEN_HALF_WIDTH;
    right        = SCREEN_HALF_WIDTH;
    top          = -SCREEN_HALF_HEIGHT;
    bottom       = SCREEN_HALF_HEIGHT;
    sortingDepth = SCREEN_WASH_SORTING_DEPTH;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setRGB0(quad, intensity, intensity >> PE2_OPTION(WASH_GREEN_SHIFT), intensity >> PE2_OPTION(WASH_BLUE_SHIFT));
    quad->x0 = left;
    quad->y0 = top - display->vramYOffset;
    quad->x1 = right;
    quad->y1 = top - display->vramYOffset;
    quad->x2 = left;
    quad->y2 = bottom - display->vramYOffset;
    quad->x3 = right;
    quad->y3 = bottom - display->vramYOffset;
    // Prepending the draw-mode packet makes additive blending active before the wash.
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(sortingDepth, display->otDepthShift), quad);
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, sortingDepth);
}

/// Fades a fan by `brightnessDecay`, grows its radius, lift and spread, and draws both bands.
static inline void _fadeAndDrawFanBands(EffectWork* work, const GfxCoord* coord, const _FanTexturePhase* texturePhase,
                                        s32 brightnessDecay, s32 radiusGrowth, s32 risingLiftGrowth, s32 upperRimSpreadGrowth)
{
    work->scale  = work->scale - brightnessDecay;
    work->angle  = work->angle + radiusGrowth;
    work->period = work->period + risingLiftGrowth;
    work->step   = work->step + upperRimSpreadGrowth;
    _drawRisingFanBand(work, coord, FAN_RISING_BAND, texturePhase);
    _drawConstantLiftFanBand(work, coord, FAN_CONSTANT_LIFT_BAND, texturePhase);
}

void infernoFlameFanTask(Task* task)
{
    enum {
        FAN_STATE_INITIALIZE       = 0,
        FAN_STATE_GROW_THEN_FADE   = 1,
        FAN_STATE_STATIONARY_BURST = 2,
        FAN_STATE_DRIFTING_BURST   = 3,
        FAN_STATE_WIDE_IGNITION    = 4,
        FAN_STATE_SHALLOW_RING     = 5,
        FAN_STATE_FAST_IGNITION    = 6,
        FAN_RANDOM_PHASE_SHIFT     = 16
    };
    EffectWork*       work;
    GfxCoord*         coord;
    _FanTexturePhase* texturePhase;
    u8*               segmentPhases;
    s32               segmentIndex;
    u32               randomDraw;

    texturePhase = task->work;
    work         = task->spawnArg2.pointer;
    coord        = task->extra.coordBody->coord;
    if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(work, task);
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    work->age = work->age + 1;
    switch (task->state) {
        case FAN_STATE_INITIALIZE:
            texturePhase = memCalloc(sizeof(*texturePhase), false);
            if (texturePhase == NULL) {
                work->age = 0;
                return;
            }
            task->work  = texturePhase;
            work->scale = PE2_OPTION(FAN_BRIGHTNESS);
            // Seed each segment in both bands, preserving the interleaved random draws.
            segmentIndex = 0;
            do {
                segmentPhases    = &texturePhase->byte[segmentIndex];
                randomDraw       = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState  = randomDraw;
                segmentPhases[0] = randomDraw >> FAN_RANDOM_PHASE_SHIFT;
                segmentIndex++;
                randomDraw                       = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState                  = randomDraw;
                segmentPhases[FAN_SEGMENT_COUNT] = randomDraw >> FAN_RANDOM_PHASE_SHIFT;
            } while (segmentIndex < FAN_SEGMENT_COUNT);
            task->state = task->spawnArg1.value + FAN_STATE_GROW_THEN_FADE;
            // Cache a 1/32-offset step rotated into the coordinate's parent frame.
            gte_lddp(PE2_OPTION(FAN_DRIFT_SPEED));
            gte_ldsv(&work->pos);
            gte_gpf12();
            gte_stsv(&work->move);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            return;
        case FAN_STATE_GROW_THEN_FADE:
            // The centered fan reaches its lift limit before brightness starts fading.
            if (work->scale >= FAN_VARIANT(0, fade) + 1) {
                if (work->period < PE2_OPTION(FAN_LIFT_LIMIT)) {
                    work->period = work->period + FAN_VARIANT(0, lift);
                } else {
                    work->scale = work->scale - FAN_VARIANT(0, fade);
                }
                work->angle = work->angle + FAN_VARIANT(0, radius);
                work->step  = work->step + FAN_VARIANT(0, spread);
                _drawRisingFanBand(work, coord, FAN_RISING_BAND, texturePhase);
                _drawConstantLiftFanBand(work, coord, FAN_CONSTANT_LIFT_BAND, texturePhase);
                return;
            }
            break;
        case FAN_STATE_STATIONARY_BURST:
            if (work->scale >= FAN_VARIANT(1, fade) + 1) {
                _fadeAndDrawFanBands(work, coord, texturePhase, FAN_VARIANT(1, fade), FAN_VARIANT(1, radius), FAN_VARIANT(1, lift), FAN_VARIANT(1, spread));
                return;
            }
            break;
        case FAN_STATE_DRIFTING_BURST:
            // Leave the composed matrix at this frame's old position; refresh next frame.
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->scale >= FAN_VARIANT(2, fade) + 1) {
                _fadeAndDrawFanBands(work, coord, texturePhase, FAN_VARIANT(2, fade), FAN_VARIANT(2, radius), FAN_VARIANT(2, lift), FAN_VARIANT(2, spread));
                return;
            }
            break;
        case FAN_STATE_WIDE_IGNITION:
            if (work->scale >= FAN_VARIANT(3, fade) + 1) {
                _fadeAndDrawFanBands(work, coord, texturePhase, FAN_VARIANT(3, fade), FAN_VARIANT(3, radius), FAN_VARIANT(3, lift), FAN_VARIANT(3, spread));
                return;
            }
            break;
        case FAN_STATE_SHALLOW_RING:
            if (work->scale >= FAN_VARIANT(4, fade) + 1) {
                _fadeAndDrawFanBands(work, coord, texturePhase, FAN_VARIANT(4, fade), FAN_VARIANT(4, radius), FAN_VARIANT(4, lift), FAN_VARIANT(4, spread));
                return;
            }
            break;
        case FAN_STATE_FAST_IGNITION:
            if (work->scale >= FAN_VARIANT(5, fade) + 1) {
                _fadeAndDrawFanBands(work, coord, texturePhase, FAN_VARIANT(5, fade), FAN_VARIANT(5, radius), FAN_VARIANT(5, lift), FAN_VARIANT(5, spread));
                return;
            }
            break;
        default:
            return;
    }
    effectKillTask(work, task);
}

/// Draws the band whose top rises with the fan (`period`): additive textured quads between its rims, each segment
/// animating at its own offset; a segment failing to project is dropped.
static void _drawRisingFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _FanTexturePhase* texturePhase)
{
    _FanScratch*           scratch;
    const EffectBandShape* shape;
    const EffectBandShape* shapes;
    SVECTOR*               bottomVertex;
    POLY_FT4*              quad;
    s32                    projectionFlags;
    s32                    sortingDepth;
    s32                    segmentIndex;
    s32                    nextSegmentIndex;
    s32                    yaw;
    s32                    textureU;
    s16                    baseRadius;
    s16                    liftedRadius;
    u16                    lift;
    u16                    textureCell;

    shapes       = _gFanBandShapes;
    shape        = &shapes[bandIndex];
    lift         = work->period + shape->lift;
    baseRadius   = work->angle + shape->baseRadius;
    liftedRadius = shape->spread + (baseRadius + work->step);
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_FanScratch);
    BUILD_FAN_RIMS(scratch, coord, baseRadius, liftedRadius, lift, segmentIndex, yaw, bottomVertex);
    // Project adjacent rim pairs, then queue each valid textured segment.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < FAN_SEGMENT_COUNT; segmentIndex++) {
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtps();
        textureCell = (texturePhase->byBand[bandIndex][segmentIndex] + work->age) % FAN_TEXTURE_FRAME_COUNT;
        gte_stsxy(&scratch->sxy0);
        nextSegmentIndex = segmentIndex + 1;
        gte_ldv3(&scratch->topRing[nextSegmentIndex % FAN_SEGMENT_COUNT], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex % FAN_SEGMENT_COUNT]);
        gte_rtpt();
        gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
        gte_stflg(&projectionFlags);
        if (projectionFlags >= 0) {
            gte_stszotz(&sortingDepth);
            sortingDepth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setRGB0(quad, work->scale, work->scale, work->scale);
            quad->tpage = getTPage(FAN_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, FAN_TEXTURE_PAGE_X, FAN_TEXTURE_PAGE_Y);
            quad->clut  = getClut(FAN_PALETTE_X, FAN_PALETTE_Y);
            textureU    = textureCell * FAN_TEXTURE_CELL_SIZE;
            setUV4(quad, textureU, FAN_TEXTURE_V, textureU + FAN_TEXTURE_CELL_SIZE - 1, FAN_TEXTURE_V,
                   textureU, FAN_TEXTURE_V + FAN_TEXTURE_CELL_SIZE - 1,
                   textureU + FAN_TEXTURE_CELL_SIZE - 1, FAN_TEXTURE_V + FAN_TEXTURE_CELL_SIZE - 1);
            quad->x0 = scratch->sxy0;
            quad->y0 = scratch->sxy0 >> 16;
            quad->x1 = scratch->sxy1;
            quad->y1 = scratch->sxy1 >> 16;
            quad->x2 = scratch->sxy2;
            quad->y2 = scratch->sxy2 >> 16;
            quad->x3 = scratch->sxy3;
            quad->y3 = scratch->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_FanScratch);
}

/// Draws the band whose top stays at its shape's lift, as `_drawRisingFanBand` does.
static void _drawConstantLiftFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _FanTexturePhase* texturePhase)
{
    _FanScratch*           scratch;
    const EffectBandShape* shape;
    const EffectBandShape* shapes;
    SVECTOR*               bottomVertex;
    POLY_FT4*              quad;
    s32                    projectionFlags;
    s32                    sortingDepth;
    s32                    segmentIndex;
    s32                    nextSegmentIndex;
    s32                    yaw;
    s32                    textureU;
    s16                    baseRadius;
    s16                    liftedRadius;
    u16                    lift;
    u16                    textureCell;

    shapes       = _gFanBandShapes;
    shape        = &shapes[bandIndex];
    baseRadius   = work->angle + shape->baseRadius;
    liftedRadius = shape->spread + (baseRadius + work->step);
    lift         = shape->lift;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_FanScratch);
    BUILD_FAN_RIMS(scratch, coord, baseRadius, liftedRadius, lift, segmentIndex, yaw, bottomVertex);
    // Project adjacent rim pairs, then queue each valid textured segment.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < FAN_SEGMENT_COUNT; segmentIndex++) {
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtps();
        textureCell = (texturePhase->byBand[bandIndex][segmentIndex] + work->age) % FAN_TEXTURE_FRAME_COUNT;
        gte_stsxy(&scratch->sxy0);
        nextSegmentIndex = segmentIndex + 1;
        gte_ldv3(&scratch->topRing[nextSegmentIndex % FAN_SEGMENT_COUNT], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex % FAN_SEGMENT_COUNT]);
        gte_rtpt();
        gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
        gte_stflg(&projectionFlags);
        if (projectionFlags >= 0) {
            gte_stszotz(&sortingDepth);
            sortingDepth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setRGB0(quad, work->scale, work->scale, work->scale);
            quad->tpage = getTPage(FAN_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, FAN_TEXTURE_PAGE_X, FAN_TEXTURE_PAGE_Y);
            quad->clut  = getClut(FAN_PALETTE_X, FAN_PALETTE_Y);
            textureU    = textureCell * FAN_TEXTURE_CELL_SIZE;
            setUV4(quad, textureU, FAN_TEXTURE_V, textureU + FAN_TEXTURE_CELL_SIZE - 1, FAN_TEXTURE_V,
                   textureU, FAN_TEXTURE_V + FAN_TEXTURE_CELL_SIZE - 1,
                   textureU + FAN_TEXTURE_CELL_SIZE - 1, FAN_TEXTURE_V + FAN_TEXTURE_CELL_SIZE - 1);
            quad->x0 = scratch->sxy0;
            quad->y0 = scratch->sxy0 >> 16;
            quad->x1 = scratch->sxy1;
            quad->y1 = scratch->sxy1 >> 16;
            quad->x2 = scratch->sxy2;
            quad->y2 = scratch->sxy2 >> 16;
            quad->x3 = scratch->sxy3;
            quad->y3 = scratch->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_FanScratch);
}

#undef BUILD_FAN_RIMS

#undef FAN_VARIANT
