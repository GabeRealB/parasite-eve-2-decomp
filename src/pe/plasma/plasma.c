#include "pe/plasma.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/effects/glow_draw.h"

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.

static void _plasmaDrawRingBand(const EffectWork* effect, const GfxCoord* coord, s32 bandIndex);

/// Shapes of the three concurrent Plasma bands, ordered by increasing base radius.
///
/// All three rows are drawn at every PE level. Distances use local coordinate
/// units: base radius is added to radial growth, lift to animated upper-rim
/// height, and spread to the upper rim's extra radius.
static EffectBandShape _gPlasmaBandShapes[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
    { 0x0300, 0x0400, 0x0400 },
};

enum {
    PLASMA_CAST_SOUND_LEVEL_1 = (s32)0xE0160001,
    PLASMA_CAST_SOUND_LEVEL_2 = (s32)0xE0190001,
    PLASMA_CAST_SOUND_LEVEL_3 = (s32)0xE01C0001,
};

/// Plasma startup sound-script requests, indexed by PE level minus one (0..2).
///
/// Each signed word packs a type-14 PE bank id, zero instance tag and entry 1.
/// This selection is independent of the three concurrent visual bands.
/// Startup copies the selected value into the sound queue; no table pointer
/// survives unloading this overlay.
static s32 _gPlasmaCastSoundScripts[ATTACHMENT_LEVEL_COUNT] = {
    PLASMA_CAST_SOUND_LEVEL_1,
    PLASMA_CAST_SOUND_LEVEL_2,
    PLASMA_CAST_SOUND_LEVEL_3,
};

/// Retained texture-frame offsets for each Plasma band's sixteen segments.
///
/// The first index is the shape row (0..2), the second the segment (0..15).
/// Startup replaces all 48 offsets with random values 0..255 in segment-first
/// order. Each segment adds its offset to the effect age modulo six to select
/// a texture cell. A later cast replaces this shared package storage.
static s16 _gPlasmaBandFrameOffsets[ARRAY_SIZE(_gPlasmaBandShapes)][EFFECT_BAND_SEGMENT_COUNT] = { 0 };

/// Seeds the three Plasma texture-phase columns in per-segment LCG order.
///
/// Takes 48 draws from the shared generator, interleaving columns 0, 1, 2
/// at each of the sixteen segment indices. Stores bits 16..23 in each signed
/// halfword phase slot, yielding 0..255. Replaces the complete package table;
/// a later cast therefore replaces an earlier cast's phases. No pointers are
/// retained and GTE state is untouched.
static inline void _plasmaSeedBandPhases(void)
{
    s32 segmentIndex;
    enum { PLASMA_PHASE_JITTER_MASK = 0xFF };
    segmentIndex = 0;
    do {
        RANDOM_LCG_ADVANCE();
        _gPlasmaBandFrameOffsets[0][segmentIndex] = RANDOM_LCG_DRAW() & PLASMA_PHASE_JITTER_MASK;
        RANDOM_LCG_ADVANCE();
        _gPlasmaBandFrameOffsets[1][segmentIndex] = RANDOM_LCG_DRAW() & PLASMA_PHASE_JITTER_MASK;
        RANDOM_LCG_ADVANCE();
        _gPlasmaBandFrameOffsets[2][segmentIndex] = RANDOM_LCG_DRAW() & PLASMA_PHASE_JITTER_MASK;
        segmentIndex++;
    } while (segmentIndex < ARRAY_SIZE(_gPlasmaBandFrameOffsets[0]));
}

void plasmaCastTask(Task* task)
{
    enum {
        PLASMA_RADIUS_STEP_BASE           = 0x60,
        PLASMA_RADIUS_STEP_PER_LEVEL      = 0x30,
        PLASMA_LEVEL_THREE_RADIUS_STEP    = 0xC0,
        PLASMA_HALO_RADIUS_STEP           = 64,
        PLASMA_HALO_WIDTH_BASE            = 0x100,
        PLASMA_HALO_WIDTH_PER_LEVEL       = 128,
        PLASMA_LEVEL_THREE_HALO_LIFT_STEP = 128,
        PLASMA_MOTOR_BASE_FRAMES          = 16,
        PLASMA_MOTOR_FRAMES_PER_LEVEL     = 4,
        PLASMA_MOTOR_START_INTENSITY      = 0xFF,
        PLASMA_MOTOR_END_INTENSITY        = 8,
        PLASMA_STATE_INITIALIZE           = 0,
        PLASMA_STATE_LEVEL_ONE_OR_TWO     = 1,
        PLASMA_STATE_LEVEL_THREE          = 2,
        PLASMA_INITIAL_BRIGHTNESS         = 0xA0,
        PLASMA_FADE_END_BRIGHTNESS        = 9,
        PLASMA_APPLY_STATS_AGE            = 8,
        PLASMA_BRIGHTNESS_STEP            = 8,
        PLASMA_HEIGHT_STEP                = 0x20,
        PLASMA_SPREAD_STEP                = 0x20,
    };
    EffectWork*      effect;
    GfxCoord*        coord;
    AttachmentState* attachment;
    s32              pan;
    s32              nextState;
    u16              previousAge;
    u16              nextAge;
    u8               rgb[3];
    s16              haloRadius;

    attachment = &gAttachmentState;
    effect     = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(effect, task);
        return;
    }

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    previousAge = effect->age;
    nextAge     = previousAge + 1;
    effect->age = nextAge;
    switch (task->state) {
        case PLASMA_STATE_INITIALIZE:
            effect->scale = PLASMA_INITIAL_BRIGHTNESS;
            effect->index = (gAttachmentState.attachId % 10) - 1;
            _plasmaSeedBandPhases();
            nextState = PLASMA_STATE_LEVEL_THREE;
            if (effect->index < 2) {
                nextState = PLASMA_STATE_LEVEL_ONE_OR_TWO;
            }
            task->state = nextState;
            pan         = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gPlasmaCastSoundScripts[(u16)(gAttachmentState.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            padScriptSpawnVariableMotorRamp((s16)(effect->index * PLASMA_MOTOR_FRAMES_PER_LEVEL + PLASMA_MOTOR_BASE_FRAMES), PLASMA_MOTOR_START_INTENSITY, PLASMA_MOTOR_END_INTENSITY);
            return;
        case PLASMA_STATE_LEVEL_ONE_OR_TWO:
            if (effect->scale < PLASMA_FADE_END_BRIGHTNESS) {
                effectKillTask(effect, task);
                return;
            }
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if ((s16)nextAge == PLASMA_APPLY_STATS_AGE) {
                    attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                effect->scale  -= PLASMA_BRIGHTNESS_STEP;
                effect->angle  += PLASMA_RADIUS_STEP_BASE + effect->index * PLASMA_RADIUS_STEP_PER_LEVEL;
                effect->period -= PLASMA_HEIGHT_STEP;
                effect->step   += PLASMA_SPREAD_STEP;
            } else {
                effect->age = previousAge;
            }
            // Shape rows are bands, so all three draw independently of the PE level.
            _plasmaDrawRingBand(effect, coord, 0);
            _plasmaDrawRingBand(effect, coord, 1);
            _plasmaDrawRingBand(effect, coord, 2);
            rgb[0] = rgb[1]    = effect->scale;
            rgb[2]             = effect->scale * 3 / 2;
            coord->workm.t[1] -= effect->age * PLASMA_HALO_RADIUS_STEP;
            _glowDrawHalo(coord, (s16)(effect->age * PLASMA_HALO_RADIUS_STEP), (s16)(effect->index * PLASMA_HALO_WIDTH_PER_LEVEL + PLASMA_HALO_WIDTH_BASE), rgb);
            rgb[0]           >>= 1;
            rgb[1]           >>= 1;
            rgb[2]           >>= 1;
            coord->workm.t[1] -= effect->age * PLASMA_HALO_RADIUS_STEP;
            _glowDrawHalo(coord, (s16)(effect->age * (2 * PLASMA_HALO_RADIUS_STEP)), (s16)(effect->index * PLASMA_HALO_WIDTH_PER_LEVEL + PLASMA_HALO_WIDTH_BASE), rgb);
            if (effect->index != 0) {
                rgb[0]           >>= 1;
                rgb[1]           >>= 1;
                rgb[2]           >>= 1;
                coord->workm.t[1] -= effect->age * PLASMA_HALO_RADIUS_STEP;
                _glowDrawHalo(coord, (s16)(effect->age * (3 * PLASMA_HALO_RADIUS_STEP)), (s16)(effect->index * PLASMA_HALO_WIDTH_PER_LEVEL + PLASMA_HALO_WIDTH_BASE), rgb);
            }
            return;
        case PLASMA_STATE_LEVEL_THREE:
            if (effect->scale < PLASMA_FADE_END_BRIGHTNESS) {
                break;
            }
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if ((s16)nextAge == PLASMA_APPLY_STATS_AGE) {
                    attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                effect->scale  -= PLASMA_BRIGHTNESS_STEP;
                effect->angle  += PLASMA_LEVEL_THREE_RADIUS_STEP;
                effect->period -= PLASMA_HEIGHT_STEP;
                effect->step   += PLASMA_SPREAD_STEP;
            } else {
                effect->age = previousAge;
            }
            // Shape rows are bands, so all three draw independently of the PE level.
            _plasmaDrawRingBand(effect, coord, 0);
            _plasmaDrawRingBand(effect, coord, 1);
            _plasmaDrawRingBand(effect, coord, 2);
            rgb[0] = rgb[1]    = effect->scale;
            rgb[2]             = effect->scale * 3 / 2;
            coord->workm.t[1] -= effect->age * PLASMA_LEVEL_THREE_HALO_LIFT_STEP;
            haloRadius         = effect->age * PLASMA_HALO_RADIUS_STEP;
            _glowDrawHalo(coord, haloRadius, haloRadius, rgb);
            rgb[0]           >>= 1;
            rgb[1]           >>= 1;
            rgb[2]           >>= 1;
            coord->workm.t[1] -= effect->age * PLASMA_LEVEL_THREE_HALO_LIFT_STEP;
            haloRadius         = effect->age * (2 * PLASMA_HALO_RADIUS_STEP);
            _glowDrawHalo(coord, haloRadius, haloRadius, rgb);
            if (effect->index != 0) {
                rgb[0]           >>= 1;
                rgb[1]           >>= 1;
                rgb[2]           >>= 1;
                coord->workm.t[1] -= effect->age * PLASMA_LEVEL_THREE_HALO_LIFT_STEP;
                haloRadius         = effect->age * (3 * PLASMA_HALO_RADIUS_STEP);
                _glowDrawHalo(coord, haloRadius, haloRadius, rgb);
            }
            return;
        default:
            return;
    }
    effectKillTask(effect, task);
}

/// Projects one ring-band quad and selects its six-cell texture frame.
///
/// Borrows a complete word-aligned EffectBandScratch with both sixteen-vertex
/// rings initialized in the camera matrix's input frame. segmentIndex is 0..15;
/// the next index wraps with the power-of-two EFFECT_BAND_SEGMENT_COUNT. Inputs are
/// stable, side-effect-free values used repeatedly; textureFrame is a writable
/// s16 local. The caller must have installed the camera projection matrices.
/// Stores RTPS corner zero before RTPT shifts the screen FIFO, then corners
/// one through three and only the final RTPT FLAG. The last corner's SZ3
/// remains available for the caller's depth calculation.
/// bandIndex is 0..2 and selects the package phase row. Adds the read-only
/// effect's signed age to that phase with signed arithmetic, then takes the
/// remainder modulo six; frame selection must not be converted to unsigned.
/// The texture lookup stays between RTPS and its screen store. The next-index
/// local belongs to the expansion; no caller-local identifiers are captured.
/// Invoke inside a braced caller block.
#define PLASMA_PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame, effect, bandIndex)                                                \
    {                                                                                                                                      \
        enum { PLASMA_BAND_FRAME_COUNT = 6 };                                                                                              \
        s32 nextSegmentIndex;                                                                                                              \
                                                                                                                                           \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                     \
        gte_rtps();                                                                                                                        \
        (textureFrame) = (_gPlasmaBandFrameOffsets[(bandIndex)][(segmentIndex)] + (effect)->age) % PLASMA_BAND_FRAME_COUNT;                \
        gte_stsxy(&(scratch)->sxy0);                                                                                                       \
        nextSegmentIndex = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                         \
        gte_ldv3(&(scratch)->topRing[nextSegmentIndex], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[nextSegmentIndex]); \
        gte_rtpt();                                                                                                                        \
        gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                  \
        gte_stflg(&(scratch)->projectionFlags);                                                                                            \
    }

/// Draws one of the Plasma cast's three rising textured ring bands.
///
/// Borrows composed `coord` and cast `effect`; `bandIndex` is 0..2 and
/// selects both the shape row and texture-phase column, independently of PE
/// level. In coordinate units, the bottom radius is angle + baseRadius, the
/// top radius adds step + spread, and top Y is -(period + lift). Radius
/// sums narrow to signed halfwords; height and translations retain low
/// halfwords. Six 40-texel frames use column jitter plus signed effect age.
/// RGB reads the little-endian low byte of scale. The final RTPT FLAG rejects
/// a segment; sorting uses its last corner's SZ3 / 4 + 1.
/// Reserves/releases one complete scratch block and appends at most sixteen
/// additive modulated `POLY_FT4` packets. Inputs must stay clear of scratch
/// and the unchecked primitive arena; queued packets live through drawing.
static void _plasmaDrawRingBand(const EffectWork* effect, const GfxCoord* coord, s32 bandIndex)
{
    enum {
        PLASMA_BAND_ANGLE_STEP         = 256,
        PLASMA_BAND_TRIG_FRACTION_BITS = 12,
        PLASMA_BAND_CELL_WIDTH         = 40,
        PLASMA_BAND_TOP_V              = 0x60,
        PLASMA_BAND_UV_SPAN            = 39,
    };
    EffectBandScratch* scratch;
    SVECTOR*           bottomVertex;
    POLY_FT4*          quad;
    EffectBandShape*   shape;
    s32                segmentIndex;
    s32                rimAngle;
    s32                textureU;
    s16                textureFrame;
    s16                topRadius;
    s16                bottomRadius;
    u16                heightBits;
    u16                heightOffsetBits;

    shape            = &_gPlasmaBandShapes[bandIndex];
    heightOffsetBits = effect->period;
    bottomRadius     = effect->angle;
    heightBits       = heightOffsetBits + shape->lift;
    bottomRadius    += shape->baseRadius;
    topRadius        = bottomRadius + effect->step + shape->spread;
    scratch          = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rims in local XZ, then transform their narrowed vertices.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        rimAngle                          = segmentIndex * PLASMA_BAND_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (rsin(rimAngle) * topRadius) >> PLASMA_BAND_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vy = -heightBits;
        scratch->topRing[segmentIndex].vz = (rcos(rimAngle) * topRadius) >> PLASMA_BAND_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx    = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy    = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz    = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(rimAngle) * bottomRadius) >> PLASMA_BAND_TRIG_FRACTION_BITS;
        // Address the lower rim through the complete scratch block's byte view.
        bottomVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        bottomVertex->vy = 0;
        bottomVertex->vz = (rcos(rimAngle) * bottomRadius) >> PLASMA_BAND_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx = (u16)scratch->bottomRing[segmentIndex].vx + (u16)coord->workm.t[0];
        bottomVertex->vy                     = (u16)bottomVertex->vy + (u16)coord->workm.t[1];
        bottomVertex->vz                     = (u16)bottomVertex->vz + (u16)coord->workm.t[2];
    }
    // Project sixteen wrapped segments; only accepted quads consume packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        PLASMA_PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame, effect, bandIndex);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setRGB0(quad, *(const u8*)&effect->scale, *(const u8*)&effect->scale, *(const u8*)&effect->scale);
            setSemiTrans(quad, true);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(16, 267);
            textureU    = textureFrame * PLASMA_BAND_CELL_WIDTH;
            setUV4(quad, textureU, PLASMA_BAND_TOP_V, textureU + PLASMA_BAND_UV_SPAN, PLASMA_BAND_TOP_V, textureU, PLASMA_BAND_TOP_V + PLASMA_BAND_UV_SPAN, textureU + PLASMA_BAND_UV_SPAN, PLASMA_BAND_TOP_V + PLASMA_BAND_UV_SPAN);
            quad->x0 = (u16)scratch->sxy0.vx;
            quad->y0 = (u16)scratch->sxy0.vy;
            quad->x1 = (u16)scratch->sxy1.vx;
            quad->y1 = (u16)scratch->sxy1.vy;
            quad->x2 = (u16)scratch->sxy2.vx;
            quad->y2 = (u16)scratch->sxy2.vy;
            quad->x3 = (u16)scratch->sxy3.vx;
            quad->y3 = (u16)scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}
#undef PLASMA_PROJECT_BAND_SEGMENT

#include "../../shared/effects/glow_draw_halo.inc.c"
