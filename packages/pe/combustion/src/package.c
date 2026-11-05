#include "combustion.h"

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
#include <engine/gfx.h>
#include <engine/gfx_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <pe/effects/pyro_flame/pyro_flame.h>
#include <effects/sprite_quad/sprite_quad.h>

PE2_EDITOR_LABEL("Lines")
/// How far above the player each line starts, and how far out; each level's table adds its steps every frame.
PE2_OPTION_DEF(EMITTER_HEIGHT, int, 1024)
PE2_OPTION_DEF(START_REACH, int, 0x200)
/// How far each line turns to its side, and how fast it turns back once laid, in a 4096-unit turn.
PE2_OPTION_DEF(SIDE_YAW, int, 512)
PE2_OPTION_DEF(UNWIND_YAW_STEP, int, 80)
/// The screen's tint as the lines ignite, a color word.
PE2_OPTION_DEF(IGNITION_COLOR, int, 0x3F7FFF)
/// The pad's rumble under each line, for the level's emitter life, from start to end.
PE2_OPTION_DEF(RUMBLE_START, int, 0xFF)
PE2_OPTION_DEF(RUMBLE_END, int, 8)

PE2_EDITOR_LABEL("Flames")
/// The size of a line's first flame, and what each later flame adds.
PE2_OPTION_DEF(FLAME_SIZE, int, 512)
PE2_OPTION_DEF(FLAME_SIZE_GROWTH, int, 32)
/// The range of a flame's random starting frame, a power of two, and the frame it goes on: a flame starting later burns
/// shorter.
PE2_OPTION_DEF(FLAME_START_PHASE, int, 16)
PE2_OPTION_DEF(FLAME_END_FRAME, int, 33)
/// The wide flame that ends a line, as a multiple of a flame's size: in halves below the large level, whole at it.
PE2_OPTION_DEF(WIDE_FLAME_HALVES, int, 3)
PE2_OPTION_DEF(LARGE_WIDE_FLAME_SCALE, int, 4)
/// The wide flame's tint on its odd frames, a color word.
PE2_OPTION_DEF(WIDE_FLAME_TINT, int, 0x4070C0)
/// The level from which flames are drawn from the large sheet, and embers may take either animation.
PE2_OPTION_DEF(LARGE_LEVEL, int, 3)
/// One frame in this many, a flame sheds an ember. A power of two.
PE2_OPTION_DEF(EMBER_ODDS, int, 4)

PE2_EDITOR_LABEL("Embers")
/// An ember's size, and what each level adds.
PE2_OPTION_DEF(EMBER_SIZE, int, 0x300)
PE2_OPTION_DEF(EMBER_LEVEL_SIZE, int, 256)
/// The range of an ember's rising speed, a power of two, and what each level adds.
PE2_OPTION_DEF(EMBER_RISE_JITTER, int, 256)
PE2_OPTION_DEF(EMBER_LEVEL_RISE, int, 64)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(IS_COLOR(PE2_OPTION(IGNITION_COLOR)) && IS_COLOR(PE2_OPTION(WIDE_FLAME_TINT)), colors);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(FLAME_START_PHASE)) && POWER_OF_TWO(PE2_OPTION(EMBER_ODDS)) &&
                  POWER_OF_TWO(PE2_OPTION(EMBER_RISE_JITTER)),
              masks);
STATIC_ASSERT(PE2_OPTION(FLAME_END_FRAME) > PE2_OPTION(FLAME_START_PHASE), every_flame_burns);

/// The flame lines' tuning at one Parasite Energy level, by the level digit of the attachment id less one, in the
/// emitter's own axes and its frames.
typedef struct {
    s16 flameDropStep;     // Added each frame to the Y of the offset the next flame is laid at; positive is down
    s16 flameReachStep;    // Added each frame to the Z of that offset, which starts START_REACH out
    s16 narrowFlameFrames; // Frames of narrow flames; the next, wide, ends the line and applies the spell
    s16 emitterFrames;     // Frames an emitter lives, and its rumble lasts
} _LevelTuning;

/// Strip lengths shared by the drawers and the ember task's stopping tests.
enum {
    SMALL_FLAME_FRAME_COUNT = 6,
    EMBER_FRAME_COUNT       = 8,
};

/// Added to SZ3 / 4 before perspective sizing and ordering to avoid a zero divisor.
enum { SPRITE_DEPTH_BIAS = 1 };

static void _drawSmallFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor);
static void _drawEmber(const GfxCoord* coord, s32 animationFrame, s16 sizeFactor);
static void _drawLargeFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor);

/// Levels 1 to 3.
static _LevelTuning _gLevelTuning[] = {
    { 0x0060, 0x0120, 7, 21 },
    { 0x0055, 0x0187, 8, 23 },
    { 0x004C, 0x01F3, 9, 25 },
};

/// The cue each level's lines play as they ignite.
static s32 _gIgnitionSoundScripts[ARRAY_SIZE(_gLevelTuning)] = { 0xE00C0002, 0xE00F0002, 0xE0120002 };

/// The last emitter's height where it was spawned, which nothing reads.
static s32 _gSpawnComposedY = 0;

/// Places an emitter above the player, turned toward its side (`spawnArg1`, +1 or -1).
static inline void _placeEmitterCoord(GfxCoord* coord, const Task* emitterTask)
{
    coord->parent = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0] = 0;
    coord->coord.t[1] = -PE2_OPTION(EMITTER_HEIGHT);
    coord->coord.t[2] = 0;
    gfxRotMatrixY(&coord->coord, emitterTask->spawnArg1.value * PE2_OPTION(SIDE_YAW), 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void combustionFlameEmitterTask(Task* task)
{
    enum {
        EMITTER_STATE_INITIALIZE = 0,
        EMITTER_STATE_EMITTING   = 1,
        EMITTER_STATE_UNWINDING  = 2,
    };
    EffectWork* work;
    GfxCoord*   coord;
    EffectWork* spawned;
    s32         pan;
    u8          rgb[3];

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case EMITTER_STATE_INITIALIZE:
            if (task->spawnArg1.value == 0) {
                task->spawnArg1.value = 1;
            }
            // Save the spawn height before placing the line in the player's frame.
            _gSpawnComposedY = coord->workm.t[1];
            _placeEmitterCoord(coord, task);
            work->move.vz = PE2_OPTION(START_REACH);
            pan           = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gIgnitionSoundScripts[(u16)(gAttachmentState.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            rgb[0] = COLOR_RED(PE2_OPTION(IGNITION_COLOR));
            rgb[1] = COLOR_GREEN(PE2_OPTION(IGNITION_COLOR));
            rgb[2] = COLOR_BLUE(PE2_OPTION(IGNITION_COLOR));
            effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            task->state = EMITTER_STATE_EMITTING;
            work->index = gAttachmentState.attachId % 10 - 1;
            padScriptSpawnVariableMotorRamp(_gLevelTuning[work->index].emitterFrames, PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            /* fallthrough */
        case EMITTER_STATE_EMITTING:
            actorRenderComposeCoord(coord);
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                effectKillTask(work, task);
                return;
            }
            // move is the next flame's local placement, rather than this task's velocity.
            work->move.vy = work->move.vy + _gLevelTuning[work->index].flameDropStep;
            work->move.vz = work->move.vz + _gLevelTuning[work->index].flameReachStep;
            spawned       = effectSpawn((EFFECT_COMBUSTION_FLAME | EFFECT_SPAWN_UNLIMITED), coord, (s32)(work->age), &work->move);
            if (spawned != NULL) {
                taskReparent(task, spawned->task);
            }
            if (_gLevelTuning[work->index].narrowFlameFrames < work->age) {
                gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
                task->state             = EMITTER_STATE_UNWINDING;
                return;
            }
            return;
        case EMITTER_STATE_UNWINDING:
            actorRenderComposeCoord(coord);
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) ||
                (work->age > _gLevelTuning[work->index].emitterFrames)) {
                effectKillTask(work, task);
                return;
            }
            // Compose first; the next tick observes this unwinding rotation.
            gfxRotMatrixY(&coord->coord, -(task->spawnArg1.value * PE2_OPTION(UNWIND_YAW_STEP)), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

/// Now and then sheds an ember from a flame, in one of its two animations; the ember is freed with the flame.
static inline void _trySpawnEmber(Task* parentTask, GfxCoord* parentCoord)
{
    enum {
        FLAME_EMBER_VARIANT_MASK = 1,
    };
    s32         emissionRng;
    s32         variantRng;
    EffectWork* spawned;

    emissionRng     = RANDOM_LCG_NEXT(gRandomLcgState);
    gRandomLcgState = emissionRng;
    if ((((u32)emissionRng >> 16) & (PE2_OPTION(EMBER_ODDS) - 1)) == 0) {
        variantRng      = RANDOM_LCG_NEXT(emissionRng);
        gRandomLcgState = variantRng;
        spawned         = effectSpawn(EFFECT_COMBUSTION_EMBER, parentCoord, ((u32)variantRng >> 16) & FLAME_EMBER_VARIANT_MASK, 0);
        if (spawned != NULL) {
            taskReparent(parentTask, spawned->task);
        }
    }
}

void combustionFlameTask(Task* task)
{
    enum {
        FLAME_STATE_INITIALIZE = 0,
        FLAME_STATE_NARROW     = 1,
        FLAME_STATE_WIDE       = 2,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         phaseRng;
    s32         narrowFlameFrames;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case FLAME_STATE_INITIALIZE:
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);

            coord->coord.t[0]   = work->pos.vx;
            coord->coord.t[1]   = work->pos.vy;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);

            // Animation starts at a random phase; the emission age determines size.
            phaseRng          = RANDOM_LCG_NEXT(gRandomLcgState);
            work->age         = ((u32)phaseRng >> 16) & (PE2_OPTION(FLAME_START_PHASE) - 1);
            work->scale       = task->spawnArg1.value * PE2_OPTION(FLAME_SIZE_GROWTH) + PE2_OPTION(FLAME_SIZE);
            work->index       = gAttachmentState.attachId % 10 - 1;
            narrowFlameFrames = _gLevelTuning[work->index].narrowFlameFrames;
            gRandomLcgState   = phaseRng;
            if (narrowFlameFrames < task->spawnArg1.value) {
                task->state = FLAME_STATE_WIDE;
                return;
            }
            task->state = FLAME_STATE_NARROW;
            return;
        case FLAME_STATE_NARROW:
            actorRenderComposeCoord(coord);
            if (work->index < (PE2_OPTION(LARGE_LEVEL) - 1)) {
                _drawSmallFlame(coord, work->age, work->scale);
            } else {
                _drawLargeFlame(coord, work->age, work->scale);
            }
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (work->age >= PE2_OPTION(FLAME_END_FRAME))) {
                effectKillTask(work, task);
                return;
            }
            _trySpawnEmber(task, coord);
            return;
        case FLAME_STATE_WIDE:
            actorRenderComposeCoord(coord);
            if (work->index < (PE2_OPTION(LARGE_LEVEL) - 1)) {
                _spriteQuadDrawFlicker(coord, work->age, work->scale * PE2_OPTION(WIDE_FLAME_HALVES) / 2, 0);
            } else {
                _spriteQuadDrawFlicker(coord, work->age, work->scale * PE2_OPTION(LARGE_WIDE_FLAME_SCALE), 0);
            }
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (work->age >= PE2_OPTION(FLAME_END_FRAME))) {
                effectKillTask(work, task);
                return;
            }
            _trySpawnEmber(task, coord);
            return;
    }
}

/// Sets a square billboard's corners around its projected center, `screenExtent` pixels out.
static inline void _setBillboardCorners(POLY_FT4* quad, const EffectCentreScratch* scratch)
{
    s16 cornerX;
    s16 cornerY;

    cornerX  = scratch->screenX - (u16)scratch->screenExtent;
    quad->x2 = cornerX;
    quad->x0 = cornerX;
    cornerX  = scratch->screenX + (u16)scratch->screenExtent;
    quad->x3 = cornerX;
    quad->x1 = cornerX;
    cornerY  = scratch->screenY - (u16)scratch->screenExtent;
    quad->y1 = cornerY;
    quad->y0 = cornerY;
    cornerY  = scratch->screenY + (u16)scratch->screenExtent;
    quad->y3 = cornerY;
    quad->y2 = cornerY;
}

/// Draws an additive square billboard from the six-cell small-flame strip; a point failing to project draws nothing.
static void _drawSmallFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor)
{
    // UV coordinates and the perspective numerator use the texel span of one cell.
    enum {
        SMALL_FLAME_CELL_WIDTH = 32,
        SMALL_FLAME_UV_SPAN    = SMALL_FLAME_CELL_WIDTH - 1,
        SMALL_FLAME_TOP_V      = 0x98,
        SMALL_FLAME_BOTTOM_V   = SMALL_FLAME_TOP_V + SMALL_FLAME_UV_SPAN,
    };
    EffectCentreScratch* scratchTop;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  leftU;
    s32                  rightU;
    u16                  worldZ;

    // Narrow the composed centre into the GTE vector and project it.
    scratchTop                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchTop - 1)->worldPoint.vx           = (u16)coord->workm.t[0];
    scratch                                   = scratchTop - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
        scratch->depth += SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
        quad->clut  = getClut(32, 266);
        quad->v0    = SMALL_FLAME_TOP_V;
        quad->v1    = SMALL_FLAME_TOP_V;
        quad->v2    = SMALL_FLAME_BOTTOM_V;
        quad->v3    = SMALL_FLAME_BOTTOM_V;
        leftU       = (s16)(animationFrame % SMALL_FLAME_FRAME_COUNT) * SMALL_FLAME_CELL_WIDTH;
        rightU      = leftU + SMALL_FLAME_UV_SPAN;
        quad->u1    = rightU;
        quad->u3    = rightU;
        quad->u0    = leftU;
        quad->u2    = leftU;
        // Size the axis-aligned quad in pixels using the biased projection depth.
        scratch->screenExtent = (sizeFactor * SMALL_FLAME_UV_SPAN) / scratch->depth;
        _setBillboardCorners(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Moves an ember up by `riseStep` (negative is up) and composes it.
static inline void _riseEmber(GfxCoord* coord, s16 riseStep)
{
    s32 nextY;

    nextY               = coord->coord.t[1] + riseStep;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
}

void combustionEmberTask(Task* task)
{
    enum {
        EMBER_STATE_INIT        = 0,
        EMBER_STATE_PYRO_FLAME  = 1,
        EMBER_STATE_EMBER       = 2,
        EMBER_STATE_SMALL_FLAME = 3,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         riseRng;
    s32         angleRng;
    s16         riseStep;
    s16         levelIndex;
    s16         animationFrame;
    s32         riseLevelIndex;
    s32         randomRiseStep;
    s32         sizeLevelIndex;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case EMBER_STATE_INIT:
            // Retain the PE level, fixed orientation, rise speed and size for this ember.
            levelIndex      = (gAttachmentState.attachId % 10U) - 1;
            riseRng         = RANDOM_LCG_NEXT(gRandomLcgState);
            angleRng        = RANDOM_LCG_NEXT(riseRng);
            work->scale     = ((u32)angleRng >> 16) & (PYRO_FLAME_FULL_TURN - 1);
            randomRiseStep  = ((u32)riseRng >> 16) & (PE2_OPTION(EMBER_RISE_JITTER) - 1);
            work->step      = levelIndex;
            gRandomLcgState = riseRng;
            riseLevelIndex  = work->step;
            work->move.vy   = -randomRiseStep - (riseLevelIndex * PE2_OPTION(EMBER_LEVEL_RISE));
            gRandomLcgState = angleRng;
            task->state     = task->spawnArg1.value + EMBER_STATE_PYRO_FLAME;
            sizeLevelIndex  = work->step;
            work->angle     = (sizeLevelIndex * PE2_OPTION(EMBER_LEVEL_SIZE)) + PE2_OPTION(EMBER_SIZE);
            if (work->step >= (PE2_OPTION(LARGE_LEVEL) - 1)) {
                gRandomLcgState = RANDOM_LCG_NEXT(angleRng);
                task->state    += RANDOM_LCG_DRAW() & 1;
            }
            // The first tick always uses pyro-flame frame zero, regardless of the selected state.
            /* fallthrough */
        case EMBER_STATE_PYRO_FLAME:
            riseStep = work->move.vy;
            _riseEmber(coord, riseStep);
            // Advance on even ages and flash the retained cell on odd ages.
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            animationFrame = work->index;
            if (animationFrame < PYRO_FLAME_FRAME_COUNT) {
                if (work->age & 1) {
                    _pyroFlameDrawSprite(coord, animationFrame, work->angle, work->scale);
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
        case EMBER_STATE_EMBER:
            riseStep = work->move.vy;
            _riseEmber(coord, riseStep);
            animationFrame = work->index + 1;
            work->index    = animationFrame;
            if (animationFrame < EMBER_FRAME_COUNT) {
                _drawEmber(coord, animationFrame, work->angle);
                return;
            }
            effectKillTask(work, task);
            return;
        case EMBER_STATE_SMALL_FLAME:
            riseStep = work->move.vy;
            _riseEmber(coord, riseStep);
            animationFrame = work->index + 1;
            work->index    = animationFrame;
            if (animationFrame < SMALL_FLAME_FRAME_COUNT) {
                _drawSmallFlame(coord, animationFrame, work->angle);
                return;
            }
            effectKillTask(work, task);
            return;
    }
}

#include <pe/effects/pyro_flame/draw_sprite.inc.c>

/// Draws an additive square billboard from the eight-cell ember strip.
static void _drawEmber(const GfxCoord* coord, s32 animationFrame, s16 sizeFactor)
{
    enum {
        EMBER_CELL_WIDTH = 24,
        EMBER_UV_SPAN    = EMBER_CELL_WIDTH - 1,
        EMBER_TOP_V      = 0xA0,
        EMBER_BOTTOM_V   = EMBER_TOP_V + EMBER_UV_SPAN,
    };
    EffectCentreScratch* scratchTop;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  leftU;
    u16                  worldZ;

    // Narrow the composed centre into the GTE vector and project it.
    scratchTop                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchTop - 1)->worldPoint.vx           = (u16)coord->workm.t[0];
    scratch                                   = scratchTop - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
        scratch->depth += SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
        quad->clut  = getClut(208, 268);
        leftU       = (animationFrame & (EMBER_FRAME_COUNT - 1)) * EMBER_CELL_WIDTH;
        setUV4(quad, leftU, EMBER_TOP_V, leftU + EMBER_UV_SPAN, EMBER_TOP_V, leftU, EMBER_BOTTOM_V, leftU + EMBER_UV_SPAN, EMBER_BOTTOM_V);
        // Size the axis-aligned quad in pixels using the biased projection depth.
        scratch->screenExtent = (sizeFactor * EMBER_UV_SPAN) / scratch->depth;
        _setBillboardCorners(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// The wide flame's copy of the flicker drawer, sized by its 56-texel cell's span.
#define SPRITE_QUAD_SCALE (56 - 1)
/// Odd frames draw the tinted cell, semitransparent.
#define SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET(quad)                                                    \
    {                                                                                                     \
        setRGB0((quad), COLOR_RED(PE2_OPTION(WIDE_FLAME_TINT)), COLOR_GREEN(PE2_OPTION(WIDE_FLAME_TINT)), \
                COLOR_BLUE(PE2_OPTION(WIDE_FLAME_TINT)));                                                 \
        SPRITE_QUAD_SET_FLICKER_ODD_CELL((quad));                                                         \
        setSemiTrans((quad), 1);                                                                          \
    }
/// Even frames draw the raw cell, additive.
#define SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET(quad) \
    {                                                   \
        setSemiTrans((quad), 1);                        \
        setShadeTex((quad), 1);                         \
        SPRITE_QUAD_SET_FLICKER_EVEN_CELL((quad));      \
    }
#include <effects/sprite_quad/draw_flicker.inc.c>

/// Draws an additive square billboard from the twelve-cell large-flame sheet, each cell with its own palette.
static void _drawLargeFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor)
{
    enum {
        LARGE_FLAME_COLUMNS     = 6,
        LARGE_FLAME_FRAME_COUNT = 12,
        LARGE_FLAME_CELL_WIDTH  = 40,
        LARGE_FLAME_UV_SPAN     = LARGE_FLAME_CELL_WIDTH - 1,
        // Signed origins wrap to byte UVs starting at V=0x88.
        LARGE_FLAME_TOP_V    = -0x78,
        LARGE_FLAME_BOTTOM_V = LARGE_FLAME_TOP_V + LARGE_FLAME_UV_SPAN,
    };
    EffectCentreScratch* scratchTop;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  wrappedFrame;
    u32                  cellIndex;
    u16                  textureColumn;
    u16                  textureRow;
    s32                  leftU;
    s32                  rightU;
    s32                  topV;
    s32                  bottomV;
    u16                  worldZ;

    // Narrow the composed centre into the GTE vector and project it.
    scratchTop                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchTop - 1)->worldPoint.vx           = (u16)coord->workm.t[0];
    scratch                                   = scratchTop - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    wrappedFrame = animationFrame;
    wrappedFrame = wrappedFrame % LARGE_FLAME_FRAME_COUNT;
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
        scratch->depth += SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage   = getTPage(0, GPU_BLEND_ADD, 640, 0);
        cellIndex     = (u16)wrappedFrame;
        quad->clut    = getClut(cellIndex << 4, 268);
        textureColumn = cellIndex % LARGE_FLAME_COLUMNS;
        textureRow    = cellIndex / LARGE_FLAME_COLUMNS;
        leftU         = textureColumn * LARGE_FLAME_CELL_WIDTH;
        rightU        = leftU + LARGE_FLAME_UV_SPAN;
        topV          = textureRow * LARGE_FLAME_CELL_WIDTH + LARGE_FLAME_TOP_V;
        bottomV       = textureRow * LARGE_FLAME_CELL_WIDTH + LARGE_FLAME_BOTTOM_V;
        setUV4(quad, leftU, topV, rightU, topV, leftU, bottomV, rightU, bottomV);
        // Size the axis-aligned quad in pixels using the biased projection depth.
        scratch->screenExtent = (sizeFactor * LARGE_FLAME_UV_SPAN) / scratch->depth;
        _setBillboardCorners(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void combustionCastTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE = 0,
    };
    GfxCoord* coord;

    if (task->state != CAST_STATE_INITIALIZE) {
        effectKillTask(task->spawnArg2.pointer, task);
        return;
    }
    coord = task->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    // Independent emitters outlive this two-tick controller.
    effectSpawn((EFFECT_COMBUSTION_FLAME_EMITTER | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
    effectSpawn((EFFECT_COMBUSTION_FLAME_EMITTER | EFFECT_SPAWN_UNLIMITED), coord, -1, 0);
    task->state = task->state + 1;
}
