#include "plasma.h"

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
#include <engine/scratch.h>
#include <engine/sound.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <effects/glow/glow.h>

PE2_EDITOR_LABEL("Cast")
/// The cast's brightness as it starts, what it loses each frame, and the frame the spell applies on.
PE2_OPTION_DEF(BRIGHTNESS, int, 0xA0)
PE2_OPTION_DEF(FADE_STEP, int, 8)
PE2_OPTION_DEF(APPLY_FRAME, int, 8)
/// Blue, in halves of red and green.
PE2_OPTION_DEF(BLUE_HALVES, int, 3)

PE2_EDITOR_LABEL("Bands")
/// What the bands' radius gains each frame, what each level adds, and what it gains at level 3 instead.
PE2_OPTION_DEF(GROWTH, int, 0x60)
PE2_OPTION_DEF(LEVEL_GROWTH, int, 0x30)
PE2_OPTION_DEF(LEVEL_3_GROWTH, int, 0xC0)
/// How much higher the bands' tops rise, and how much wider they spread, each frame.
PE2_OPTION_DEF(BAND_RISE, int, 0x20)
PE2_OPTION_DEF(BAND_SPREAD, int, 0x20)

PE2_EDITOR_LABEL("Halos")
/// The stacked halos' radii grow each frame by one, two and three times this, and each rises by the lift above the one
/// below (by level 3's at level 3); the third is from level 2.
PE2_OPTION_DEF(HALO_GROWTH, int, 64)
PE2_OPTION_DEF(HALO_LIFT, int, 64)
PE2_OPTION_DEF(LEVEL_3_HALO_LIFT, int, 128)
/// The halos' width, and what each level adds; at level 3 it is their radius.
PE2_OPTION_DEF(HALO_WIDTH, int, 0x100)
PE2_OPTION_DEF(LEVEL_HALO_WIDTH, int, 128)
/// How much dimmer each halo is than the one below, as a shift.
PE2_OPTION_DEF(HALO_DIM_SHIFT, int, 1)

PE2_EDITOR_LABEL("Rumble")
/// The pad's rumble as the cast starts, in frames, what each level adds, and its strength from start to end.
PE2_OPTION_DEF(RUMBLE_FRAMES, int, 16)
PE2_OPTION_DEF(LEVEL_RUMBLE_FRAMES, int, 4)
PE2_OPTION_DEF(RUMBLE_START, int, 0xFF)
PE2_OPTION_DEF(RUMBLE_END, int, 8)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(PE2_OPTION(FADE_STEP) > 0, the_cast_fades);

static void _drawRingBand(const EffectWork* effect, const GfxCoord* coord, s32 bandIndex);

/// The three bands' base radius, lift and spread, all drawn at every level.
static EffectBandShape _gBandShapes[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
    { 0x0300, 0x0400, 0x0400 },
};

enum {
    CAST_SOUND_LEVEL_1 = (s32)0xE0160001,
    CAST_SOUND_LEVEL_2 = (s32)0xE0190001,
    CAST_SOUND_LEVEL_3 = (s32)0xE01C0001,
};

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[ATTACHMENT_LEVEL_COUNT] = {
    CAST_SOUND_LEVEL_1,
    CAST_SOUND_LEVEL_2,
    CAST_SOUND_LEVEL_3,
};

/// Each band segment's random offset into the band's animation, chosen when a cast starts.
static s16 _gBandFrameOffsets[ARRAY_SIZE(_gBandShapes)][EFFECT_BAND_SEGMENT_COUNT] = { 0 };

/// Chooses the band segments' offsets.
static inline void _seedBandPhases(void)
{
    s32 segmentIndex;
    enum { PHASE_JITTER_MASK = 0xFF };
    segmentIndex = 0;
    do {
        RANDOM_LCG_ADVANCE();
        _gBandFrameOffsets[0][segmentIndex] = RANDOM_LCG_DRAW() & PHASE_JITTER_MASK;
        RANDOM_LCG_ADVANCE();
        _gBandFrameOffsets[1][segmentIndex] = RANDOM_LCG_DRAW() & PHASE_JITTER_MASK;
        RANDOM_LCG_ADVANCE();
        _gBandFrameOffsets[2][segmentIndex] = RANDOM_LCG_DRAW() & PHASE_JITTER_MASK;
        segmentIndex++;
    } while (segmentIndex < ARRAY_SIZE(_gBandFrameOffsets[0]));
}

void plasmaCastTask(Task* task)
{
    enum {
        STATE_INITIALIZE       = 0,
        STATE_LEVEL_ONE_OR_TWO = 1,
        STATE_LEVEL_THREE      = 2,
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
        case STATE_INITIALIZE:
            effect->scale = PE2_OPTION(BRIGHTNESS);
            effect->index = (gAttachmentState.attachId % 10) - 1;
            _seedBandPhases();
            nextState = STATE_LEVEL_THREE;
            if (effect->index < 2) {
                nextState = STATE_LEVEL_ONE_OR_TWO;
            }
            task->state = nextState;
            pan         = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gCastSoundScripts[(u16)(gAttachmentState.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            padScriptSpawnVariableMotorRamp((s16)(effect->index * PE2_OPTION(LEVEL_RUMBLE_FRAMES) + PE2_OPTION(RUMBLE_FRAMES)), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            return;
        case STATE_LEVEL_ONE_OR_TWO:
            if (effect->scale < (PE2_OPTION(FADE_STEP) + 1)) {
                effectKillTask(effect, task);
                return;
            }
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if ((s16)nextAge == PE2_OPTION(APPLY_FRAME)) {
                    attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                effect->scale  -= PE2_OPTION(FADE_STEP);
                effect->angle  += PE2_OPTION(GROWTH) + effect->index * PE2_OPTION(LEVEL_GROWTH);
                effect->period -= PE2_OPTION(BAND_RISE);
                effect->step   += PE2_OPTION(BAND_SPREAD);
            } else {
                effect->age = previousAge;
            }
            // Shape rows are bands, so all three draw independently of the PE level.
            _drawRingBand(effect, coord, 0);
            _drawRingBand(effect, coord, 1);
            _drawRingBand(effect, coord, 2);
            rgb[0] = rgb[1]    = effect->scale;
            rgb[2]             = effect->scale * PE2_OPTION(BLUE_HALVES) / 2;
            coord->workm.t[1] -= effect->age * PE2_OPTION(HALO_LIFT);
            _glowDrawHalo(coord, (s16)(effect->age * PE2_OPTION(HALO_GROWTH)), (s16)(effect->index * PE2_OPTION(LEVEL_HALO_WIDTH) + PE2_OPTION(HALO_WIDTH)), rgb);
            rgb[0]           >>= PE2_OPTION(HALO_DIM_SHIFT);
            rgb[1]           >>= PE2_OPTION(HALO_DIM_SHIFT);
            rgb[2]           >>= PE2_OPTION(HALO_DIM_SHIFT);
            coord->workm.t[1] -= effect->age * PE2_OPTION(HALO_LIFT);
            _glowDrawHalo(coord, (s16)(effect->age * (2 * PE2_OPTION(HALO_GROWTH))), (s16)(effect->index * PE2_OPTION(LEVEL_HALO_WIDTH) + PE2_OPTION(HALO_WIDTH)), rgb);
            if (effect->index != 0) {
                rgb[0]           >>= PE2_OPTION(HALO_DIM_SHIFT);
                rgb[1]           >>= PE2_OPTION(HALO_DIM_SHIFT);
                rgb[2]           >>= PE2_OPTION(HALO_DIM_SHIFT);
                coord->workm.t[1] -= effect->age * PE2_OPTION(HALO_LIFT);
                _glowDrawHalo(coord, (s16)(effect->age * (3 * PE2_OPTION(HALO_GROWTH))), (s16)(effect->index * PE2_OPTION(LEVEL_HALO_WIDTH) + PE2_OPTION(HALO_WIDTH)), rgb);
            }
            return;
        case STATE_LEVEL_THREE:
            if (effect->scale < (PE2_OPTION(FADE_STEP) + 1)) {
                break;
            }
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if ((s16)nextAge == PE2_OPTION(APPLY_FRAME)) {
                    attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                effect->scale  -= PE2_OPTION(FADE_STEP);
                effect->angle  += PE2_OPTION(LEVEL_3_GROWTH);
                effect->period -= PE2_OPTION(BAND_RISE);
                effect->step   += PE2_OPTION(BAND_SPREAD);
            } else {
                effect->age = previousAge;
            }
            // Shape rows are bands, so all three draw independently of the PE level.
            _drawRingBand(effect, coord, 0);
            _drawRingBand(effect, coord, 1);
            _drawRingBand(effect, coord, 2);
            rgb[0] = rgb[1]    = effect->scale;
            rgb[2]             = effect->scale * PE2_OPTION(BLUE_HALVES) / 2;
            coord->workm.t[1] -= effect->age * PE2_OPTION(LEVEL_3_HALO_LIFT);
            haloRadius         = effect->age * PE2_OPTION(HALO_GROWTH);
            _glowDrawHalo(coord, haloRadius, haloRadius, rgb);
            rgb[0]           >>= PE2_OPTION(HALO_DIM_SHIFT);
            rgb[1]           >>= PE2_OPTION(HALO_DIM_SHIFT);
            rgb[2]           >>= PE2_OPTION(HALO_DIM_SHIFT);
            coord->workm.t[1] -= effect->age * PE2_OPTION(LEVEL_3_HALO_LIFT);
            haloRadius         = effect->age * (2 * PE2_OPTION(HALO_GROWTH));
            _glowDrawHalo(coord, haloRadius, haloRadius, rgb);
            if (effect->index != 0) {
                rgb[0]           >>= PE2_OPTION(HALO_DIM_SHIFT);
                rgb[1]           >>= PE2_OPTION(HALO_DIM_SHIFT);
                rgb[2]           >>= PE2_OPTION(HALO_DIM_SHIFT);
                coord->workm.t[1] -= effect->age * PE2_OPTION(LEVEL_3_HALO_LIFT);
                haloRadius         = effect->age * (3 * PE2_OPTION(HALO_GROWTH));
                _glowDrawHalo(coord, haloRadius, haloRadius, rgb);
            }
            return;
        default:
            return;
    }
    effectKillTask(effect, task);
}

/// Projects a band segment's four corners and picks its animation cell.
#define PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame, effect, bandIndex)                                                       \
    {                                                                                                                                      \
        enum { BAND_FRAME_COUNT = 6 };                                                                                                     \
        s32 nextSegmentIndex;                                                                                                              \
                                                                                                                                           \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                     \
        gte_rtps();                                                                                                                        \
        (textureFrame) = (_gBandFrameOffsets[(bandIndex)][(segmentIndex)] + (effect)->age) % BAND_FRAME_COUNT;                             \
        gte_stsxy(&(scratch)->sxy0);                                                                                                       \
        nextSegmentIndex = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                         \
        gte_ldv3(&(scratch)->topRing[nextSegmentIndex], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[nextSegmentIndex]); \
        gte_rtpt();                                                                                                                        \
        gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                  \
        gte_stflg(&(scratch)->projectionFlags);                                                                                            \
    }

/// Draws a band, an additive textured sleeve from its base radius up to its rising, spreading top, its segments
/// animating at their own offsets; a segment failing to project is dropped.
static void _drawRingBand(const EffectWork* effect, const GfxCoord* coord, s32 bandIndex)
{
    enum {
        BAND_ANGLE_STEP         = 256,
        BAND_TRIG_FRACTION_BITS = 12,
        BAND_CELL_WIDTH         = 40,
        BAND_TOP_V              = 0x60,
        BAND_UV_SPAN            = 39,
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

    shape            = &_gBandShapes[bandIndex];
    heightOffsetBits = effect->period;
    bottomRadius     = effect->angle;
    heightBits       = heightOffsetBits + shape->lift;
    bottomRadius    += shape->baseRadius;
    topRadius        = bottomRadius + effect->step + shape->spread;
    scratch          = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rims in local XZ, then transform their narrowed vertices.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        rimAngle                          = segmentIndex * BAND_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (rsin(rimAngle) * topRadius) >> BAND_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vy = -heightBits;
        scratch->topRing[segmentIndex].vz = (rcos(rimAngle) * topRadius) >> BAND_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx    = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy    = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz    = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(rimAngle) * bottomRadius) >> BAND_TRIG_FRACTION_BITS;
        // Address the lower rim through the complete scratch block's byte view.
        bottomVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        bottomVertex->vy = 0;
        bottomVertex->vz = (rcos(rimAngle) * bottomRadius) >> BAND_TRIG_FRACTION_BITS;
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
        PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame, effect, bandIndex);
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
            textureU    = textureFrame * BAND_CELL_WIDTH;
            setUV4(quad, textureU, BAND_TOP_V, textureU + BAND_UV_SPAN, BAND_TOP_V, textureU, BAND_TOP_V + BAND_UV_SPAN, textureU + BAND_UV_SPAN, BAND_TOP_V + BAND_UV_SPAN);
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
#undef PROJECT_BAND_SEGMENT

#include <effects/glow/halo.inc.c>
