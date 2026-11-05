#include "energyshot.h"

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
#include <engine/sound.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <effects/glow/glow.h>

PE2_EDITOR_LABEL("Rings")
/// The three rings' radii, as multiples of the cast's scale; they are drawn at each level's ring height.
PE2_OPTION_DEF(RING_1_RADIUS, int, 4)
PE2_OPTION_DEF(RING_2_RADIUS, int, 8)
PE2_OPTION_DEF(RING_3_RADIUS, int, 12)
/// The glow wedges' length as a multiple of the scale, and the range of the random yaw added to each, a power of two in
/// a 4096-unit turn.
PE2_OPTION_DEF(WEDGE_LENGTH, int, 6)
PE2_OPTION_DEF(WEDGE_JITTER, int, 512)
/// How much dimmer green is than red and blue, as a shift.
PE2_OPTION_DEF(GREEN_SHIFT, int, 1)
/// Brightness lost each frame while the cast fades.
PE2_OPTION_DEF(FADE_STEP, int, 16)

PE2_EDITOR_LABEL("Beams")
/// The main beam's radius as a multiple of the scale, and how far short of the ring height it stops.
PE2_OPTION_DEF(BEAM_RADIUS, int, 6)
PE2_OPTION_DEF(BEAM_SHORTFALL, int, 0x100)
/// From level 2, a second beam: its radius, and its height as a multiple of the ring height.
PE2_OPTION_DEF(BEAM_2_RADIUS, int, 4)
PE2_OPTION_DEF(BEAM_2_HEIGHT, int, 2)
/// At level 3, a third: its radius, and its height as the ring height shifted down.
PE2_OPTION_DEF(BEAM_3_RADIUS, int, 8)
PE2_OPTION_DEF(BEAM_3_HEIGHT_SHIFT, int, 1)
/// What a beam's top and bottom radii add to its growth (the bottom's halved).
PE2_OPTION_DEF(BEAM_TOP_RADIUS, int, 1024)
PE2_OPTION_DEF(BEAM_BOTTOM_RADIUS, int, 256)

PE2_EDITOR_LABEL("Sparks")
/// How far out the cast sheds its spark each frame, in halves of its scale.
PE2_OPTION_DEF(SPARK_RADIUS_HALVES, int, 3)

PE2_EDITOR_LABEL("Rising billboard")
/// How fast a billboard rises, plus a random part under the jitter, a power of two.
PE2_OPTION_DEF(BILLBOARD_RISE_SPEED, int, 16)
PE2_OPTION_DEF(BILLBOARD_RISE_JITTER, int, 64)
/// Frames each of its eight cells shows, a power of two, and its size.
PE2_OPTION_DEF(BILLBOARD_FRAME_TICKS, int, 4)
PE2_OPTION_DEF(BILLBOARD_SIZE, int, 0x400)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(WEDGE_JITTER)) && PE2_OPTION(WEDGE_JITTER) <= 4096, wedge_jitter_is_a_mask);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(BILLBOARD_RISE_JITTER)) && POWER_OF_TWO(PE2_OPTION(BILLBOARD_FRAME_TICKS)), billboard_masks);

/// The cast's tuning at one Parasite Energy level, by the level digit of the attachment id less one. A scale is at once
/// the drawing's brightness (its low byte the red and blue) and, multiplied up, each ring's, wedge's and beam's radius.
typedef struct {
    s16 wedgeCount; // Glow wedges fanned around the rings
    s16 scaleLimit; // Scale that ends the growth; the fading rings are drawn at this scale
    s16 scaleStep;  // Scale gained per frame of growth; at level 3 the fading wedges and beams keep gaining it
    u16 ringHeight; // Height of the rings and wedges, which the beams' heights follow; also each spark's size
} _LevelTuning;
PE2_TYPE(_LevelTuning)

PE2_EDITOR_LABEL("Levels")
/// The cast at levels 1 to 3.
PE2_OPTION_DEF(LEVELS, _LevelTuning[3], {
    { 8, 0x0090, 5, 0x0400 },
    { 12, 0x00C0, 6, 0x0500 },
    { 16, 0x00F0, 7, 0x0600 },
})
PE2_EDITOR_LABEL("")

/// The most any level has, which its table holds.
enum { MAX_CAST_WEDGES = LARGER(LARGER(PE2_OPTION_AT(LEVELS, 0, wedgeCount), PE2_OPTION_AT(LEVELS, 1, wedgeCount)), PE2_OPTION_AT(LEVELS, 2, wedgeCount)) };
STATIC_ASSERT(PE2_OPTION_AT(LEVELS, 0, wedgeCount) > 0 && PE2_OPTION_AT(LEVELS, 1, wedgeCount) > 0 && PE2_OPTION_AT(LEVELS, 2, wedgeCount) > 0, every_wedgeCount_is_positive);

static _LevelTuning _gLevelTuning[] = PE2_OPTION(LEVELS);

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[ARRAY_SIZE(_gLevelTuning)] = { 0xE02A0001, 0xE02D0001, 0xE0300001 };

static void _drawBeamBand(const GfxCoord* coord, s16 radiusGrowth, s16 height, const u8* rgb);

/// Each beam segment's random offset into the beam's animation, chosen when a cast starts.
static s16 _gBeamBandFrameOffsets[EFFECT_BAND_SEGMENT_COUNT];

/// The wedges' yaws, spaced evenly with a little jitter when a cast starts.
static s16 _gWedgeYaws[MAX_CAST_WEDGES];

/// Draws the level's wedges at `radiusGrowth` times their length.
#define DRAW_CAST_WEDGES(coord, work, radiusGrowth, levelTuning, rgb, wedgeIndex)                           \
    {                                                                                                       \
        const _LevelTuning* wedgeTuning;                                                                    \
        const s16*          wedgeYaw;                                                                       \
        s16                 wedgeCount;                                                                     \
        (wedgeIndex) = 0;                                                                                   \
        wedgeCount   = (levelTuning)[(work)->index].wedgeCount;                                             \
        if (wedgeCount > 0) {                                                                               \
            wedgeTuning = (levelTuning);                                                                    \
            wedgeYaw    = _gWedgeYaws;                                                                      \
            do {                                                                                            \
                glowDrawWedge((coord), (s16)((radiusGrowth) * PE2_OPTION(WEDGE_LENGTH)), *wedgeYaw, (rgb)); \
                wedgeYaw += 1;                                                                              \
            } while (++(wedgeIndex) < wedgeTuning[(work)->index].wedgeCount);                               \
        }                                                                                                   \
    }

void energyshotCastTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE     = 0,
        CAST_STATE_GROWING        = 1,
        CAST_STATE_FADING         = 2,
        CAST_ANGLE_TURN           = 0x1000,
        CAST_TEXTURE_PHASE_MASK   = 0xFF,
        CAST_SPARK_ANGLE_MASK     = 0xFFF,
        CAST_SPARK_RANDOM_PALETTE = 0x8000,
        CAST_LEVEL_THREE_INDEX    = 2,
        CAST_SPARK_RADIUS_SHIFT   = 11, // Halves of a scale, in the sine's 4096ths
    };
    EffectWork*      work;
    GfxCoord*        coord;
    AttachmentState* attachment;
    s32              segmentIndex;
    u8               rgb[3];

    attachment = &gAttachmentState;
    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    if ((attachment->effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case CAST_STATE_INITIALIZE: {
                RoomEffectState* effectState;
                s16              wedgeCount;
                u16              levelIndex;

                coord->parent = work->parent;
                gfxSetRotIdentity(&coord->coord);
                coord->coord.t[2]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[0]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                attachment->flags        |= ATTACHMENT_FLAG_APPLY_STATS;
                effectState               = gRoomEffectState;
                effectState->burstRequest = false;
                effectState->peFxFlags   &= (u16)~ROOM_EFFECT_PE_ENERGY_SHOT_AURA;
                task->state               = CAST_STATE_GROWING;
                work->index               = (gAttachmentState.attachId % 10) - 1;
                // Replace the complete beam phase table, then seed the active wedge prefix.
                segmentIndex = 0;
                {
                    s16* texturePhase;

                    texturePhase = _gBeamBandFrameOffsets;
                    do {
                        s32 randomState;

                        segmentIndex   += 1;
                        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                        *texturePhase   = ((u32)randomState >> 16) & CAST_TEXTURE_PHASE_MASK;
                        texturePhase   += 1;
                        gRandomLcgState = randomState;
                    } while (segmentIndex < ARRAY_SIZE(_gBeamBandFrameOffsets));
                }
                segmentIndex = 0;
                {
                    _LevelTuning* levelTuning;

                    levelTuning = _gLevelTuning;
                    wedgeCount  = levelTuning[work->index].wedgeCount;
                    levelIndex  = work->index;
                    if (wedgeCount > 0) {
                        do {
                            s32 baseYaw;
                            s32 randomState;

                            baseYaw                   = segmentIndex * (CAST_ANGLE_TURN / _gLevelTuning[(s16)levelIndex].wedgeCount);
                            randomState               = RANDOM_LCG_NEXT(gRandomLcgState);
                            _gWedgeYaws[segmentIndex] = baseYaw + (((u32)randomState >> 16) & (PE2_OPTION(WEDGE_JITTER) - 1));
                            segmentIndex             += 1;
                            gRandomLcgState           = randomState;
                            wedgeCount                = _gLevelTuning[work->index].wedgeCount;
                            levelIndex                = work->index;
                        } while (segmentIndex < wedgeCount);
                    }
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(_gCastSoundScripts[work->index], pan,
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case CAST_STATE_GROWING: {
                _LevelTuning* levelTuning;
                s16           ringHeight;
                s32           randomState;
                s16           spawnYaw;

                levelTuning = _gLevelTuning;
                work->scale = work->scale + levelTuning[work->index].scaleStep;
                rgb[0]      = (u8)work->scale;
                rgb[1]      = work->scale >> PE2_OPTION(GREEN_SHIFT);
                rgb[2]      = (u8)work->scale;
                // Lift the discs and wedges; beams and sparks use the origin below.
                ringHeight          = levelTuning[work->index].ringHeight;
                coord->coord.t[1]   = -ringHeight;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                effectDrawGouraudDisc(coord, (s16)(work->scale * PE2_OPTION(RING_1_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(work->scale * PE2_OPTION(RING_2_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(work->scale * PE2_OPTION(RING_3_RADIUS)), rgb);
                DRAW_CAST_WEDGES(coord, work, work->scale, levelTuning, rgb, segmentIndex);
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->index != 0) {
                    if (work->index == CAST_LEVEL_THREE_INDEX) {
                        ringHeight = _gLevelTuning[2].ringHeight;
                        _drawBeamBand(coord, (s16)(work->scale * PE2_OPTION(BEAM_3_RADIUS)), ringHeight >> PE2_OPTION(BEAM_3_HEIGHT_SHIFT), rgb);
                    }
                    _drawBeamBand(
                        coord, (s16)(work->scale * PE2_OPTION(BEAM_2_RADIUS)),
                        _gLevelTuning[work->index].ringHeight * PE2_OPTION(BEAM_2_HEIGHT), rgb);
                }
                _drawBeamBand(
                    coord, (s16)(work->scale * PE2_OPTION(BEAM_RADIUS)),
                    _gLevelTuning[work->index].ringHeight - PE2_OPTION(BEAM_SHORTFALL), rgb);
                randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                spawnYaw        = ((u32)randomState >> 16) & CAST_SPARK_ANGLE_MASK;
                gRandomLcgState = randomState;
                work->angle     = spawnYaw;
                work->move.vx   = (u32)(rsin(spawnYaw) * work->scale * PE2_OPTION(SPARK_RADIUS_HALVES)) >> CAST_SPARK_RADIUS_SHIFT;
                work->move.vz   = (u32)(rcos(work->angle) * work->scale * PE2_OPTION(SPARK_RADIUS_HALVES)) >> CAST_SPARK_RADIUS_SHIFT;
                ringHeight      = _gLevelTuning[work->index].ringHeight;
                effectSpawn(EFFECT_RISING_ENERGY_SPARK, coord,
                            ringHeight | CAST_SPARK_RANDOM_PALETTE,
                            &work->move);
                if (_gLevelTuning[work->index].scaleLimit < work->scale) {
                    effectSpawn((EFFECT_ENERGY_SHOT_AURA | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                    work->period = work->scale;
                    task->state  = CAST_STATE_FADING;
                }
                return;
            }
            case CAST_STATE_FADING: {
                _LevelTuning* levelTuning;
                s16           ringHeight;

                if (work->scale < (PE2_OPTION(FADE_STEP) + 1)) {
                    effectKillTask(work, task);
                    return;
                }
                work->scale         = work->scale - PE2_OPTION(FADE_STEP);
                rgb[0]              = (u8)work->scale;
                rgb[1]              = work->scale >> PE2_OPTION(GREEN_SHIFT);
                rgb[2]              = (u8)work->scale;
                levelTuning         = _gLevelTuning;
                ringHeight          = levelTuning[work->index].ringHeight;
                coord->coord.t[1]   = -ringHeight;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                // Discs hold their capped radius; level-three wedges and beams keep growing.
                effectDrawGouraudDisc(coord, (s16)(levelTuning[work->index].scaleLimit * PE2_OPTION(RING_1_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(levelTuning[work->index].scaleLimit * PE2_OPTION(RING_2_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(levelTuning[work->index].scaleLimit * PE2_OPTION(RING_3_RADIUS)), rgb);
                DRAW_CAST_WEDGES(coord, work, work->period, levelTuning, rgb, segmentIndex);
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->index != 0) {
                    if (work->index == CAST_LEVEL_THREE_INDEX) {
                        work->period =
                            work->period + _gLevelTuning[2].scaleStep;
                        _drawBeamBand(
                            coord, (s16)(work->period * PE2_OPTION(BEAM_3_RADIUS)),
                            (s16)_gLevelTuning[work->index].ringHeight >> PE2_OPTION(BEAM_3_HEIGHT_SHIFT),
                            rgb);
                    }
                    _drawBeamBand(
                        coord, (s16)(work->period * PE2_OPTION(BEAM_2_RADIUS)),
                        _gLevelTuning[work->index].ringHeight * PE2_OPTION(BEAM_2_HEIGHT), rgb);
                }
                _drawBeamBand(
                    coord, (s16)(work->period * PE2_OPTION(BEAM_RADIUS)),
                    _gLevelTuning[work->index].ringHeight - PE2_OPTION(BEAM_SHORTFALL), rgb);
                return;
            }
        }
        return;
    }
    effectKillTask(work, task);
}

#undef DRAW_CAST_WEDGES

#include <effects/glow/wedge.inc.c>

/// Projects a beam segment's four corners and picks its animation cell.
#define PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame)                                                                          \
    {                                                                                                                                      \
        enum { BAND_FRAME_COUNT = 6 };                                                                                                     \
        s32 nextSegmentIndex;                                                                                                              \
                                                                                                                                           \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                     \
        gte_rtps();                                                                                                                        \
        (textureFrame) = (_gBeamBandFrameOffsets[(segmentIndex)] + gDisplayState.animFrame) % BAND_FRAME_COUNT;                            \
        gte_stsxy(&(scratch)->sxy0);                                                                                                       \
        nextSegmentIndex = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                         \
        gte_ldv3(&(scratch)->topRing[nextSegmentIndex], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[nextSegmentIndex]); \
        gte_rtpt();                                                                                                                        \
        gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                  \
        gte_stflg(&(scratch)->projectionFlags);                                                                                            \
    }

/// Draws a beam, an additive textured sleeve flaring from the coordinate up to `height`, its radii following
/// `radiusGrowth`, its segments animating at their own offsets; a segment failing to project is dropped.
static void _drawBeamBand(const GfxCoord* coord, s16 radiusGrowth, s16 height, const u8* rgb)
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
    s32                segmentIndex;
    s32                rimAngle;
    s32                textureU;
    s16                textureFrame;
    s16                topRadius;
    s16                bottomRadius;

    bottomRadius = radiusGrowth / 2 + PE2_OPTION(BEAM_BOTTOM_RADIUS);
    topRadius    = radiusGrowth + PE2_OPTION(BEAM_TOP_RADIUS);
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rims in local XZ, then transform their narrowed vertices.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        rimAngle                          = segmentIndex * BAND_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (rsin(rimAngle) * topRadius) >> BAND_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vy = -height;
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
        PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setRGB0(quad, rgb[0], rgb[1], rgb[2]);
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

/// Gives a billboard its rising speed and its spin, which also picks palette 0.
static inline void _initRisingBillboard(EffectWork* work)
{
    enum {
        BILLBOARD_ROTATION_MASK = 0xFFF,
    };

    work->move.vx = 0;
    work->move.vz = 0;
    RANDOM_LCG_ADVANCE();
    // Interpret the unsigned subtraction's low halfword as a signed Y displacement.
    work->move.vy = (s16)(-PE2_OPTION(BILLBOARD_RISE_SPEED) -
                          (RANDOM_LCG_DRAW() & (PE2_OPTION(BILLBOARD_RISE_JITTER) - 1)));
    RANDOM_LCG_ADVANCE();
    work->scale = RANDOM_LCG_DRAW() & BILLBOARD_ROTATION_MASK;
}

void energyshotRisingBillboardTask(Task* task)
{
    enum {
        BILLBOARD_STATE_INITIALIZE = 0,
        BILLBOARD_STATE_ANIMATE    = 1,
        BILLBOARD_FRAME_COUNT      = 8,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         nextY;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    if (task->state == BILLBOARD_STATE_INITIALIZE) {
        _initRisingBillboard(work);
        task->state = BILLBOARD_STATE_ANIMATE;
    }

    // Initialization falls through to movement and drawing on the first tick.
    nextY               = coord->coord.t[1] + work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
    if ((work->age & (PE2_OPTION(BILLBOARD_FRAME_TICKS) - 1)) == 0) {
        work->index = work->index + 1;
    }
    if (work->index < BILLBOARD_FRAME_COUNT) {
        effectDrawSpinningBillboard(coord, work->index, PE2_OPTION(BILLBOARD_SIZE), work->scale);
        return;
    }
    effectKillTask(work, task);
}
