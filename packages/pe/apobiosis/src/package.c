#include "apobiosis.h"

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
#include <effects/glow/glow.h>
#include <pe/effects/strip/strip.h>

PE2_EDITOR_LABEL("Ring")
/// The ring's radius when the cast starts; each level's table adds its step every frame.
PE2_OPTION_DEF(START_RADIUS, int, 512)
/// How far above the caster the ring is drawn, and the thickness of its halo.
PE2_OPTION_DEF(RING_LIFT, int, 1024)
PE2_OPTION_DEF(HALO_THICKNESS, int, 128)
/// How far the halo is pulled toward the camera, in sorting depth.
PE2_OPTION_DEF(HALO_DEPTH_PULL, int, 0x40)
/// The range the strips' angles wander in each frame, a power of two in a 4096-unit turn.
PE2_OPTION_DEF(STRIP_JITTER, int, 256)
/// How much dimmer the ring's red and green, and its blue, are than the flash, as shifts: the ring is blue.
PE2_OPTION_DEF(RING_RED_GREEN_SHIFT, int, 2)
PE2_OPTION_DEF(RING_BLUE_SHIFT, int, 1)
/// The frame of the ring the spell applies on.
PE2_OPTION_DEF(APPLY_FRAME, int, 4)

PE2_EDITOR_LABEL("Flash")
/// The screen flash's brightness when the ring starts and as each later phase starts, and how much dimmer its other
/// channels are, as a shift.
PE2_OPTION_DEF(FLASH_BRIGHTNESS, int, 240)
PE2_OPTION_DEF(FLASH_DIM_SHIFT, int, 1)
/// One flash in this many is yellow rather than blue at level 3, and so are a shard's sprite and strip. A power of two.
PE2_OPTION_DEF(ALT_COLOR_ODDS, int, 4)
/// Brightness the flash loses each frame of the ring.
PE2_OPTION_DEF(RING_FADE_STEP, int, 24)
/// What it loses each frame of the sparse and single shard phases, down to their floors, and in the last phases.
PE2_OPTION_DEF(SPARSE_FADE_STEP, int, 16)
PE2_OPTION_DEF(SPARSE_FADE_FLOOR, int, 64)
PE2_OPTION_DEF(SINGLE_FADE_STEP, int, 12)
PE2_OPTION_DEF(SINGLE_FADE_FLOOR, int, 32)
PE2_OPTION_DEF(FINAL_FADE_STEP, int, 8)
/// A sparse shard relights the flash to this brightness, plus up to the jitter, a power of two.
PE2_OPTION_DEF(SHARD_FLASH_BRIGHTNESS, int, 96)
PE2_OPTION_DEF(SHARD_FLASH_JITTER, int, 128)

PE2_EDITOR_LABEL("Shard phases")
/// The frames the sparse, single and double shard phases end on; the double phase is for levels 2 and 3.
PE2_OPTION_DEF(SPARSE_END_FRAME, int, 20)
PE2_OPTION_DEF(SINGLE_END_FRAME, int, 30)
PE2_OPTION_DEF(DOUBLE_END_FRAME, int, 40)
/// One frame in this many of the sparse phase spawns a shard. A power of two.
PE2_OPTION_DEF(SPARSE_SHARD_ODDS, int, 4)
/// Shards each frame of the double phase.
PE2_OPTION_DEF(DOUBLE_SHARDS, int, 2)
/// How far from the caster each phase's shards spawn, at most; powers of two.
PE2_OPTION_DEF(SPARSE_SHARD_RADIUS, int, 1024)
PE2_OPTION_DEF(SINGLE_SHARD_RADIUS, int, 2048)
PE2_OPTION_DEF(DOUBLE_SHARD_RADIUS, int, 4096)

PE2_EDITOR_LABEL("Shards")
/// Frames a shard pinned to its spawn point lives, and one drifting.
PE2_OPTION_DEF(PINNED_SHARD_FRAMES, int, 25)
PE2_OPTION_DEF(DRIFTING_SHARD_FRAMES, int, 17)
/// The range of a drifting shard's speed on each horizontal axis, a power of two.
PE2_OPTION_DEF(SHARD_DRIFT, int, 128)
/// How far up a shard's strip reaches, and the range its top spreads in on each horizontal axis, a power of two.
PE2_OPTION_DEF(SHARD_STRIP_HEIGHT, int, 4096)
PE2_OPTION_DEF(SHARD_STRIP_SPREAD, int, 4096)
/// How many times larger a pinned shard is drawn.
PE2_OPTION_DEF(PINNED_SHARD_SCALE, int, 2)

PE2_EDITOR_LABEL("Rumble")
/// The pad's rumble as the ring, the shards and the double phase start, and as the cast fades at level 1, in frames,
/// and its strength from start to end.
PE2_OPTION_DEF(RING_RUMBLE_FRAMES, int, 10)
PE2_OPTION_DEF(SHARD_RUMBLE_FRAMES, int, 20)
PE2_OPTION_DEF(DOUBLE_RUMBLE_FRAMES, int, 34)
PE2_OPTION_DEF(FADE_RUMBLE_FRAMES, int, 18)
PE2_OPTION_DEF(RUMBLE_START, int, 255)
PE2_OPTION_DEF(RUMBLE_END, int, 8)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(STRIP_JITTER)) && POWER_OF_TWO(PE2_OPTION(ALT_COLOR_ODDS)), masks);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(SHARD_FLASH_JITTER)) && POWER_OF_TWO(PE2_OPTION(SPARSE_SHARD_ODDS)), shard_masks);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(SPARSE_SHARD_RADIUS)) && POWER_OF_TWO(PE2_OPTION(SINGLE_SHARD_RADIUS)) &&
                  POWER_OF_TWO(PE2_OPTION(DOUBLE_SHARD_RADIUS)),
              shard_radii_are_masks);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(SHARD_DRIFT)) && POWER_OF_TWO(PE2_OPTION(SHARD_STRIP_SPREAD)), shard_spreads_are_masks);
/// The frame the ring ends on: it fades from the flash's brightness, and the phases after it end on given frames.
#define RING_END_FRAME ((PE2_OPTION(FLASH_BRIGHTNESS) + PE2_OPTION(RING_FADE_STEP) - 1) / PE2_OPTION(RING_FADE_STEP))
STATIC_ASSERT(PE2_OPTION(RING_FADE_STEP) > 0 && PE2_OPTION(APPLY_FRAME) >= 1 && PE2_OPTION(APPLY_FRAME) <= RING_END_FRAME,
              the_spell_applies_during_the_ring);
STATIC_ASSERT(RING_END_FRAME < PE2_OPTION(SPARSE_END_FRAME) && PE2_OPTION(SPARSE_END_FRAME) < PE2_OPTION(SINGLE_END_FRAME) &&
                  PE2_OPTION(SINGLE_END_FRAME) < PE2_OPTION(DOUBLE_END_FRAME),
              the_phases_end_in_order);

/// The cast's tuning at one Parasite Energy level, by the level digit of the attachment id less one. A scale sizes a
/// primitive before the perspective divide.
typedef struct {
    s16 stripCount;        // Strips the cast radiates each frame; it seeds two angles for each
    s16 playerSpriteScale; // Scale of the sprite the cast draws on the player
    s16 radiusStep;        // Added each frame to the ring's radius, its halo's size and its strips' length
    u16 stripScale;        // Scale of every strip, and of a shard's own sprite; a shard pinned to its coordinate doubles it
} _LevelParams;
PE2_TYPE(_LevelParams)

static void _drawScreenFlash(s16 brightness);

enum {
    LEVEL_ID_RADIX  = 10,
    LEVEL_THREE_ROW = 2,
    FULL_TURN       = 4096,
    QUARTER_TURN    = FULL_TURN / 4,
    TRIG_SHIFT      = 12, // rsin/rcos return Q12 values
};

PE2_EDITOR_LABEL("Levels")
/// The cast at levels 1 to 3.
PE2_OPTION_DEF(LEVELS, _LevelParams[3], {
    { 4, 0x0400, 192, 0x0280 },
    { 6, 0x0500, 256, 0x0300 },
    { 8, 0x0600, 320, 0x0400 },
})
PE2_EDITOR_LABEL("")

/// The most any level has, which its table holds.
enum { MAX_CAST_STRIPS = LARGER(LARGER(PE2_OPTION_AT(LEVELS, 0, stripCount), PE2_OPTION_AT(LEVELS, 1, stripCount)), PE2_OPTION_AT(LEVELS, 2, stripCount)) };
STATIC_ASSERT(PE2_OPTION_AT(LEVELS, 0, stripCount) > 0 && PE2_OPTION_AT(LEVELS, 1, stripCount) > 0 && PE2_OPTION_AT(LEVELS, 2, stripCount) > 0, every_stripCount_is_positive);

static _LevelParams _gLevelParams[] = PE2_OPTION(LEVELS);

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[ARRAY_SIZE(_gLevelParams)] = { 0xE0170001, 0xE01A0001, 0xE01D0001 };

static void _drawShardSprite(const GfxCoord* coord, s16 textureFrame, s16 sizeScale, s16 screenAngle);
static void _drawShardStrip(const GfxCoord* coord, const SVECTOR* endOffset, s16 textureFrame, s16 widthScale);

/// The strips' angles, two per strip, spaced a quarter turn apart with jitter when a cast starts. The ring also jitters
/// and reads a second angle at `radiusStep` past each strip's, 192 to 327 entries in: past this table, as the original
/// does, into memory nothing here defines.
static s16 _gStripAngles[MAX_CAST_STRIPS * 2];

/// The cast running, which its shards join so that they are freed with it.
static Task* _gCastTask;

/// Spawns a drifting shard at a random point within `radiusMask` of the cast.
static inline void _spawnDriftingShard(EffectWork* castWork, GfxCoord* castCoord, s32 radiusMask)
{
    enum {
        SHARD_SPAWN_DRIFTING = 0 // Keeps the spawned coordinate parented to the view
    };

    RANDOM_LCG_ADVANCE();
    castWork->scale = RANDOM_LCG_DRAW() & radiusMask;
    RANDOM_LCG_ADVANCE();
    castWork->angle   = RANDOM_LCG_DRAW() & (FULL_TURN - 1);
    castWork->move.vx = castWork->scale * rsin(castWork->angle) >> TRIG_SHIFT;
    castWork->move.vz = castWork->scale * rcos(castWork->angle) >> TRIG_SHIFT;
    effectSpawn(EFFECT_APOBIOSIS_SHARD, castCoord, SHARD_SPAWN_DRIFTING, &castWork->move);
}

void apobiosisCastTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE        = 0,
        CAST_STATE_RING              = 1,
        CAST_STATE_SPARSE_SHARDS     = 2,
        CAST_STATE_SINGLE_SHARDS     = 3,
        CAST_STATE_DOUBLE_SHARDS     = 4,
        CAST_STATE_FADING            = 5,
        CAST_LEVEL_ONE_ROW           = 0,
        CAST_UNUSED_PERIOD           = 128,
        CAST_QUARTER_TURN_SHIFT      = 10,
        CAST_SEEDED_ANGLES_PER_STRIP = 2,
        CAST_MOTOR_LEVEL_STEP        = 8, // Times a level row that is always 0 there
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         ringIndex;
    s32         shardIndex;
    s32         secondaryAngleIndex;
    s32         pan;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case CAST_STATE_INITIALIZE:
                // Publish the parent for shards, then seed the level-dependent ring.
                _gCastTask    = task;
                coord->parent = work->parent;
                gfxSetRotIdentity(&coord->coord);
                coord->coord.t[0]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[2]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(_gCastSoundScripts[(u16)(gAttachmentState.attachId % LEVEL_ID_RADIX) - 1], pan,
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                task->state  = CAST_STATE_RING;
                work->index  = gAttachmentState.attachId % LEVEL_ID_RADIX - 1;
                work->scale  = PE2_OPTION(START_RADIUS);
                work->period = CAST_UNUSED_PERIOD;
                work->step   = PE2_OPTION(FLASH_BRIGHTNESS);
                for (ringIndex = 0; ringIndex < _gLevelParams[work->index].stripCount * CAST_SEEDED_ANGLES_PER_STRIP; ringIndex++) {
                    RANDOM_LCG_ADVANCE();
                    _gStripAngles[ringIndex] = (ringIndex << CAST_QUARTER_TURN_SHIFT) + (RANDOM_LCG_DRAW() & (QUARTER_TURN - 1));
                }
                padScriptSpawnVariableMotorRamp(PE2_OPTION(RING_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
                /* fallthrough */
            case CAST_STATE_RING:
                actorRenderComposeCoord(coord);
                if (work->age == PE2_OPTION(APPLY_FRAME)) {
                    gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                _drawScreenFlash(work->step);
                rgb[0] = rgb[1] = work->step >> PE2_OPTION(RING_RED_GREEN_SHIFT);
                rgb[2]          = work->step >> PE2_OPTION(RING_BLUE_SHIFT);
                // Lift only the composed draw origin; local placement is unchanged.
                coord->workm.t[1] -= PE2_OPTION(RING_LIFT);
                work->scale        = work->scale + _gLevelParams[work->index].radiusStep;
                _drawShardSprite(
                    &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1], work->age,
                    _gLevelParams[work->index].playerSpriteScale, 0);
                _glowDrawHalo(coord, work->scale, PE2_OPTION(HALO_THICKNESS), rgb);
                if (work->age & 1) {
                    _glowDrawHalo(coord, PE2_OPTION(HALO_THICKNESS), work->scale, rgb);
                }
                for (ringIndex = 0; ringIndex < _gLevelParams[work->index].stripCount; ringIndex++) {
                    RANDOM_LCG_ADVANCE();
                    _gStripAngles[ringIndex] -= (RANDOM_LCG_DRAW() & (PE2_OPTION(STRIP_JITTER) - 1)) - (PE2_OPTION(STRIP_JITTER) / 2);
                    // Past the table: see `_gStripAngles`.
                    secondaryAngleIndex = ringIndex + _gLevelParams[work->index].radiusStep;
                    RANDOM_LCG_ADVANCE();
                    _gStripAngles[secondaryAngleIndex] -= (RANDOM_LCG_DRAW() & (PE2_OPTION(STRIP_JITTER) - 1)) - (PE2_OPTION(STRIP_JITTER) / 2);
                    work->pos.vx                        = work->scale * rsin(_gStripAngles[ringIndex]) >> TRIG_SHIFT;
                    work->pos.vy                        = work->scale * rcos(_gStripAngles[ringIndex]) >> TRIG_SHIFT;
                    work->pos.vz =
                        work->pos.vx *
                            rcos(_gStripAngles
                                     [ringIndex + _gLevelParams[work->index].radiusStep]) >>
                        TRIG_SHIFT;
                    _drawShardStrip(coord, &work->pos, work->age,
                                    _gLevelParams[work->index].stripScale);
                }
                coord->workm.t[1] += PE2_OPTION(RING_LIFT);
                if (work->step >= PE2_OPTION(RING_FADE_STEP) + 1) {
                    work->step = work->step - PE2_OPTION(RING_FADE_STEP);
                    return;
                }
                task->state = CAST_STATE_SPARSE_SHARDS;
                padScriptSpawnVariableMotorRamp(PE2_OPTION(SHARD_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
                return;
            case CAST_STATE_SPARSE_SHARDS:
                // Shard phases reuse scale as a random radius; flash rolls follow each successful chance.
                actorRenderComposeCoord(coord);
                _drawScreenFlash(work->step);
                if (work->step >= PE2_OPTION(SPARSE_FADE_FLOOR) + 1) {
                    work->step = work->step - PE2_OPTION(SPARSE_FADE_STEP);
                }
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & (PE2_OPTION(SPARSE_SHARD_ODDS) - 1)) == 0) {
                    _spawnDriftingShard(work, coord, (PE2_OPTION(SPARSE_SHARD_RADIUS) - 1));
                    RANDOM_LCG_ADVANCE();
                    work->step = (RANDOM_LCG_DRAW() & (PE2_OPTION(SHARD_FLASH_JITTER) - 1)) + PE2_OPTION(SHARD_FLASH_BRIGHTNESS);
                }
                if (work->age == PE2_OPTION(SPARSE_END_FRAME)) {
                    work->step  = PE2_OPTION(FLASH_BRIGHTNESS);
                    task->state = CAST_STATE_SINGLE_SHARDS;
                }
                return;
            case CAST_STATE_SINGLE_SHARDS:
                _drawScreenFlash(work->step);
                if (work->step >= PE2_OPTION(SINGLE_FADE_FLOOR) + 1) {
                    work->step = work->step - PE2_OPTION(SINGLE_FADE_STEP);
                }
                _spawnDriftingShard(work, coord, (PE2_OPTION(SINGLE_SHARD_RADIUS) - 1));
                if (work->age == PE2_OPTION(SINGLE_END_FRAME)) {
                    if (work->index <= CAST_LEVEL_ONE_ROW) {
                        task->state = CAST_STATE_FADING;
                        padScriptSpawnVariableMotorRamp(work->index * CAST_MOTOR_LEVEL_STEP + PE2_OPTION(FADE_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
                    } else {
                        work->step  = PE2_OPTION(FLASH_BRIGHTNESS);
                        task->state = CAST_STATE_DOUBLE_SHARDS;
                        padScriptSpawnVariableMotorRamp(PE2_OPTION(DOUBLE_RUMBLE_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
                    }
                }
                return;
            case CAST_STATE_DOUBLE_SHARDS:
                _drawScreenFlash(work->step);
                if (work->step >= PE2_OPTION(FINAL_FADE_STEP) + 1) {
                    work->step = work->step - PE2_OPTION(FINAL_FADE_STEP);
                }
                for (shardIndex = 0; shardIndex < PE2_OPTION(DOUBLE_SHARDS); shardIndex++) {
                    _spawnDriftingShard(work, coord, (PE2_OPTION(DOUBLE_SHARD_RADIUS) - 1));
                }
                if (work->age == PE2_OPTION(DOUBLE_END_FRAME)) {
                    task->state = CAST_STATE_FADING;
                }
                return;
            case CAST_STATE_FADING:
                _drawScreenFlash(work->step);
                if (work->step >= PE2_OPTION(FINAL_FADE_STEP) + 1) {
                    work->step = work->step - PE2_OPTION(FINAL_FADE_STEP);
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Draws the additive full-screen flash, blue or now and then yellow at level 3, steady through the screen's shake.
static void _drawScreenFlash(s16 brightness)
{
    enum {
        FLASH_HALF_WIDTH  = 160,
        FLASH_HALF_HEIGHT = 120,
        FLASH_DEPTH       = 48,
    };
    POLY_F4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    if ((u16)(gAttachmentState.attachId % (u32)LEVEL_ID_RADIX) - 1 == LEVEL_THREE_ROW && (((RANDOM_LCG_ADVANCE()) >> 16) & (PE2_OPTION(ALT_COLOR_ODDS) - 1)) == 0) {
        setRGB0(quad, brightness, brightness, brightness >> PE2_OPTION(FLASH_DIM_SHIFT));
    } else {
        setRGB0(quad, brightness >> PE2_OPTION(FLASH_DIM_SHIFT), brightness >> PE2_OPTION(FLASH_DIM_SHIFT), brightness);
    }
    // Cancel the draw environment's shake offset so the flash covers the display.
    setXY4(quad, -FLASH_HALF_WIDTH, -FLASH_HALF_HEIGHT - gDisplayState.vramYOffset, FLASH_HALF_WIDTH,
           -FLASH_HALF_HEIGHT - gDisplayState.vramYOffset, -FLASH_HALF_WIDTH, FLASH_HALF_HEIGHT - gDisplayState.vramYOffset,
           FLASH_HALF_WIDTH, FLASH_HALF_HEIGHT - gDisplayState.vramYOffset);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(FLASH_DEPTH),
            quad);
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, FLASH_DEPTH);
}

/// The halo is pulled toward the camera (see `halo.inc.c`).
#define GLOW_DRAW_HALO_DEPTH_PULL PE2_OPTION(HALO_DEPTH_PULL)
#include <effects/glow/halo.inc.c>

/// Gives a shard its strip's top, its sprite's spin and its level row (`step`).
static inline void _initShardAppearance(EffectWork* work)
{
    enum {
        SHARD_UNUSED_SCALE = 128,
    };
    // The spawn offset has positioned the shard; pos now describes its strip.
    work->pos.vy = -PE2_OPTION(SHARD_STRIP_HEIGHT);
    work->scale  = SHARD_UNUSED_SCALE;
    RANDOM_LCG_ADVANCE();
    work->pos.vx = (PE2_OPTION(SHARD_STRIP_SPREAD) / 2) - (RANDOM_LCG_DRAW() & (PE2_OPTION(SHARD_STRIP_SPREAD) - 1));
    RANDOM_LCG_ADVANCE();
    work->pos.vz = (PE2_OPTION(SHARD_STRIP_SPREAD) / 2) - (RANDOM_LCG_DRAW() & (PE2_OPTION(SHARD_STRIP_SPREAD) - 1));
    RANDOM_LCG_ADVANCE();
    work->angle = RANDOM_LCG_DRAW() & (FULL_TURN - 1);
    work->step  = gAttachmentState.attachId % LEVEL_ID_RADIX - 1;
}

void apobiosisShardTask(Task* task)
{
    enum {
        SHARD_INIT     = 0,
        SHARD_PINNED   = 1,
        SHARD_DRIFTING = 2,
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case SHARD_INIT:
                // Join the live cast's task tree before choosing a pinned or drifting coordinate.
                taskReparent(_gCastTask, task);
                if (task->spawnArg1.value != 0) {
                    coord->parent       = work->parent;
                    coord->coord.t[0]   = 0;
                    coord->coord.t[1]   = 0;
                    coord->coord.t[2]   = 0;
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(coord);
                    task->state = SHARD_PINNED;
                } else {
                    work->move.vy = 0;
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = (PE2_OPTION(SHARD_DRIFT) / 2) - (RANDOM_LCG_DRAW() & (PE2_OPTION(SHARD_DRIFT) - 1));
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = (PE2_OPTION(SHARD_DRIFT) / 2) - (RANDOM_LCG_DRAW() & (PE2_OPTION(SHARD_DRIFT) - 1));
                    task->state   = SHARD_DRIFTING;
                }
                _initShardAppearance(work);
                return;
            case SHARD_PINNED:
                // The work's step is the level row; index advances only on drawn frames.
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index = work->index + 1;
                    _drawShardSprite(coord, work->index,
                                     _gLevelParams[work->step].stripScale * PE2_OPTION(PINNED_SHARD_SCALE),
                                     work->angle);
                    _drawShardStrip(coord, &work->pos, work->index,
                                    _gLevelParams[work->step].stripScale * PE2_OPTION(PINNED_SHARD_SCALE));
                }
                if (work->age < PE2_OPTION(PINNED_SHARD_FRAMES)) {
                    return;
                }
                break;
            case SHARD_DRIFTING:
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index = work->index + 1;
                    _drawShardSprite(coord, work->index,
                                     _gLevelParams[work->step].stripScale,
                                     work->angle);
                    _drawShardStrip(coord, &work->pos, work->index,
                                    _gLevelParams[work->step].stripScale);
                }
                if (work->age < PE2_OPTION(DRIFTING_SHARD_FRAMES)) {
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Draws the additive spinning sprite at a coordinate, cycling its six cells; at level 3 now and then yellow.
static void _drawShardSprite(const GfxCoord* coord, s16 textureFrame, s16 sizeScale, s16 screenAngle)
{
    enum {
        SPRITE_FRAME_COUNT = 6,
        SPRITE_CELL_WIDTH  = 40,
        SPRITE_UV_SPAN     = SPRITE_CELL_WIDTH - 1,
        SPRITE_TOP_V       = 56,
        SPRITE_BOTTOM_V    = SPRITE_TOP_V + SPRITE_UV_SPAN,
        SPRITE_DEPTH_BIAS  = 1,
    };
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s16                 cell;
    s32                 uLeft;
    s32                 uRight;
    s32                 quarterTurnAngle;

    /// A corner's offset from the sprite's center, at `scale` and `angle`.
#define SPRITE_SET_CORNER_OFFSET(scratchBlock, scale, angle)                                                                \
    (scratchBlock)->extent.corner.x = ((((scale) * SPRITE_UV_SPAN) / (scratchBlock)->depth) * rsin((angle))) >> TRIG_SHIFT; \
    (scratchBlock)->extent.corner.y = ((((scale) * SPRITE_UV_SPAN) / (scratchBlock)->depth) * rcos((angle))) >> TRIG_SHIFT

    // Stage the low halves of the cached world origin, then project through the camera.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth += SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
        if ((u16)(gAttachmentState.attachId % LEVEL_ID_RADIX) - 1 == LEVEL_THREE_ROW) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & (PE2_OPTION(ALT_COLOR_ODDS) - 1)) == 0) {
                quad->clut = getClut(144, 267);
            } else {
                quad->clut = getClut(304, 266);
            }
        } else {
            quad->clut = getClut(304, 266);
        }
        cell   = textureFrame % SPRITE_FRAME_COUNT;
        uLeft  = cell * SPRITE_CELL_WIDTH;
        uRight = uLeft + SPRITE_UV_SPAN;
        setUV4(quad, uLeft, SPRITE_TOP_V, uRight, SPRITE_TOP_V, uLeft, SPRITE_BOTTOM_V, uRight, SPRITE_BOTTOM_V);
        // Resolve two perpendicular corner pairs around the projected centre.
        SPRITE_SET_CORNER_OFFSET(scratch, sizeScale, screenAngle);
        quad->x0         = scratch->screenX + scratch->extent.corner.x;
        quad->x3         = scratch->screenX - scratch->extent.corner.x;
        quad->y0         = scratch->screenY - scratch->extent.corner.y;
        quad->y3         = scratch->screenY + scratch->extent.corner.y;
        quarterTurnAngle = screenAngle + QUARTER_TURN;
        SPRITE_SET_CORNER_OFFSET(scratch, sizeScale, quarterTurnAngle);
        quad->x1 = scratch->screenX + scratch->extent.corner.x;
        quad->x2 = scratch->screenX - scratch->extent.corner.x;
        quad->y1 = scratch->screenY - scratch->extent.corner.y;
        quad->y2 = scratch->screenY + scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef SPRITE_SET_CORNER_OFFSET

/// Draws an additive strip from a coordinate up to `endOffset` from it, sorted at its start's depth; either end failing
/// to project drops it. `textureFrame` cycles its four cells; at level 3 now and then yellow.
static void _drawShardStrip(const GfxCoord* coord, const SVECTOR* endOffset, s16 textureFrame, s16 widthScale)
{
    enum {
        STRIP_DEPTH_BIAS  = 1,
        STRIP_CELL_WIDTH  = 128,
        STRIP_CELL_HEIGHT = 24,
        STRIP_COLUMN_MASK = 1,
        STRIP_FRAME_MASK  = 3,
        STRIP_TOP_V       = 208,
    };
    EffectStripScratch* scratch;
    POLY_FT4*           quad;
    s32                 uLeft;
    s32                 uRight;
    s32                 vTop;
    s32                 vBottom;
    s16                 screenAngle;

    // Stage the world-space segment before loading the camera matrices.
    scratch              = SCRATCH_STACK_RESERVE_BLOCK(EffectStripScratch);
    scratch->worldEnd.vx = scratch->worldStart.vx = coord->workm.t[0];
    scratch->worldEnd.vy = scratch->worldStart.vy = coord->workm.t[1];
    scratch->worldEnd.vz = scratch->worldStart.vz = coord->workm.t[2];
    scratch->worldEnd.vx                         += endOffset->vx;
    scratch->worldEnd.vy                         += endOffset->vy;
    scratch->worldEnd.vz                         += endOffset->vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    _effectStripProjectPoint(scratch, false);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth += STRIP_DEPTH_BIAS;
        _effectStripProjectPoint(scratch, true);
        if (scratch->projectionFlags >= 0) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            if ((u16)(gAttachmentState.attachId % (u32)LEVEL_ID_RADIX) - 1 == LEVEL_THREE_ROW &&
                (((RANDOM_LCG_ADVANCE()) >> 16) & (PE2_OPTION(ALT_COLOR_ODDS) - 1)) == 0) {
                quad->clut = getClut(128, 267);
            } else {
                quad->clut = getClut(112, 266);
            }
            uLeft  = (textureFrame & STRIP_COLUMN_MASK) * STRIP_CELL_WIDTH;
            uRight = uLeft + (STRIP_CELL_WIDTH - 1);
            // Keep V origins signed until the packet narrows them to bytes.
            vTop    = ((textureFrame & STRIP_FRAME_MASK) >> 1) * STRIP_CELL_HEIGHT - (256 - STRIP_TOP_V);
            vBottom = ((textureFrame & STRIP_FRAME_MASK) >> 1) * STRIP_CELL_HEIGHT -
                      (256 - STRIP_TOP_V - (STRIP_CELL_HEIGHT - 1));
            setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
            // Split the sprite's two corner pairs between the projected segment's ends.
            screenAngle            = ratan2(scratch->screenEnd.vy - scratch->screenStart.vy, scratch->screenEnd.vx - scratch->screenStart.vx);
            scratch->cornerOffsetX = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle)) >> TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle)) >> TRIG_SHIFT;
            quad->x0               = scratch->screenStart.vx + scratch->cornerOffsetX;
            quad->x3               = scratch->screenEnd.vx - scratch->cornerOffsetX;
            quad->y0               = scratch->screenStart.vy - scratch->cornerOffsetY;
            quad->y3               = scratch->screenEnd.vy + scratch->cornerOffsetY;
            scratch->cornerOffsetX = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle + QUARTER_TURN)) >> TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle + QUARTER_TURN)) >> TRIG_SHIFT;
            quad->x1               = scratch->screenEnd.vx + scratch->cornerOffsetX;
            quad->x2               = scratch->screenStart.vx - scratch->cornerOffsetX;
            quad->y1               = scratch->screenEnd.vy - scratch->cornerOffsetY;
            quad->y2               = scratch->screenStart.vy + scratch->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}
