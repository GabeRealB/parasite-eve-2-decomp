#include "pe/apobiosis.h"

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
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"

/// Size and pace of the apobiosis effect at one Parasite Energy level.
///
/// The cast and every shard it spawns select a row by the level digit of the
/// spell being cast (`AttachmentState::attachId % 10 - 1`), so a higher level
/// radiates more strips, faster and larger. A strip is a textured quad laid
/// between two projected points; a sprite is a camera-facing animated quad.
///
/// A scale sizes its primitive before the perspective divide: the on-screen
/// half-extent in pixels is the scale times the texture's extent in texels,
/// over the view depth.
typedef struct {
    s16 stripCount;        // Strips the cast radiates each frame; it seeds two angles for each
    s16 playerSpriteScale; // Scale of the sprite the cast draws on the player
    s16 radiusStep;        // Added each frame to the cast's radius, which is its halo's size and its strips' length. Also the distance from a strip's first angle to its second in the angle table
    u16 stripScale;        // Scale of every strip, and of a shard's own sprite; a shard pinned to its coordinate doubles it
} _ApobiosisLevelParams;
STATIC_ASSERT_SIZEOF(_ApobiosisLevelParams, 0x8);

static void _apobiosisDrawScreenFlash(s16 brightness);

/// Spell-id level selection and colour variation used by apobiosis's drawers and shards.
enum {
    APOBIOSIS_LEVEL_ID_RADIX      = 10,
    APOBIOSIS_LEVEL_THREE_ROW     = 2,
    APOBIOSIS_ALT_COLOR_ROLL_MASK = 3, // One alternate colour in four draws at level 3
    APOBIOSIS_FULL_TURN           = 4096,
    APOBIOSIS_QUARTER_TURN        = APOBIOSIS_FULL_TURN / 4,
    APOBIOSIS_TRIG_SHIFT          = 12, // rsin/rcos return Q12 values
};

/// Per-level tuning for the apobiosis pulse, one row per PE level 1-3,
/// weakest first.
static _ApobiosisLevelParams D_apobiosis_80130B5C[] = {
    { 0x0004, 0x0400, 0x00C0, 0x0280 },
    { 0x0006, 0x0500, 0x0100, 0x0300 },
    { 0x0008, 0x0600, 0x0140, 0x0400 },
};

/// The `sndEvtRequestScriptStart` id the cast plays, one per `D_apobiosis_80130B5C`
/// row, so the sound follows the cast's level like the burst does.
static s32 D_apobiosis_80130B74[] = { 0xE0170001, 0xE01A0001, 0xE01D0001 };

static void _apobiosisDrawShardSprite(const GfxCoord* coord, s16 textureFrame, s16 sizeScale, s16 screenAngle);
static void _apobiosisDrawShardStrip(const GfxCoord* coord, const SVECTOR* endOffset, s16 textureFrame, s16 widthScale);

/// Ring azimuths, two rows of up to eight. `apobiosisCastTask` lays out
/// `_ApobiosisLevelParams::stripCount * 2` of them at `(i << 10) + rand()` in state 0 and
/// then jitters each by +-0x80 a frame; the first row is the shard's own angle
/// and the row `_ApobiosisLevelParams::radiusStep` entries later is its elevation.
static s16 D_apobiosis_80130B80[16];

/// The running cast task, cached by `apobiosisCastTask` so each shard
/// can reparent itself onto the cast when it starts.
static Task* D_apobiosis_80130BA0;

/// Attempts one drifting Apobiosis shard at a random local polar offset.
///
/// `castWork` and `castCoord` must be non-NULL and writable, with a live,
/// writable, acyclic coordinate parent chain and an initialized effect controller.
/// `radiusMask` is 1023, 2047 or 4095; it selects a radius in 0..radiusMask
/// game-coordinate units. Overwrites `scale` with that radius and `angle` with
/// yaw in 0..4095 (4096 units per turn), consuming two shared random draws in
/// that order with unsigned 32-bit wraparound.
///
/// Replaces `move.vx/vz` with signed Q12-trigonometric offsets in the cast's
/// local space, preserving `move.vy`. Placement applies the cast's orientation
/// and snapshots the offset before return, so later bursts may reuse `move`;
/// the shard never reads the retained offset pointer. Composition and placement
/// change GTE working registers. The cast task must stay live through the shard's
/// first tick, when it joins the cast's teardown tree; the Apobiosis overlay must
/// stay loaded for the shard's lifetime. Spawn failure is ignored and still
/// leaves the draws and work writes.
static inline void _apobiosisSpawnDriftingShard(EffectWork* castWork, GfxCoord* castCoord, s32 radiusMask)
{
    enum {
        APOBIOSIS_SHARD_SPAWN_DRIFTING = 0 // Keeps the spawned coordinate parented to the view
    };

    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    castWork->scale   = (gRandomLcgState >> 16) & radiusMask;
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    castWork->angle   = (gRandomLcgState >> 16) & (APOBIOSIS_FULL_TURN - 1);
    castWork->move.vx = castWork->scale * rsin(castWork->angle) >> APOBIOSIS_TRIG_SHIFT;
    castWork->move.vz = castWork->scale * rcos(castWork->angle) >> APOBIOSIS_TRIG_SHIFT;
    effectSpawn(EFFECT_APOBIOSIS_SHARD, castCoord, APOBIOSIS_SHARD_SPAWN_DRIFTING, &castWork->move);
}

void apobiosisCastTask(Task* task)
{
    enum {
        APOBIOSIS_CAST_STATE_INITIALIZE         = 0,
        APOBIOSIS_CAST_STATE_RING               = 1,
        APOBIOSIS_CAST_STATE_SPARSE_SHARDS      = 2,
        APOBIOSIS_CAST_STATE_SINGLE_SHARDS      = 3,
        APOBIOSIS_CAST_STATE_DOUBLE_SHARDS      = 4,
        APOBIOSIS_CAST_STATE_FADING             = 5,
        APOBIOSIS_CAST_LEVEL_ONE_ROW            = 0,
        APOBIOSIS_CAST_INITIAL_RADIUS           = 512,
        APOBIOSIS_CAST_UNUSED_PERIOD            = 128,
        APOBIOSIS_CAST_INITIAL_BRIGHTNESS       = 240,
        APOBIOSIS_CAST_RING_LIFT                = 1024,
        APOBIOSIS_CAST_HALO_THICKNESS           = 128,
        APOBIOSIS_CAST_APPLY_STATS_AGE          = 4,
        APOBIOSIS_CAST_SPARSE_END_AGE           = 20,
        APOBIOSIS_CAST_SINGLE_END_AGE           = 30,
        APOBIOSIS_CAST_DOUBLE_END_AGE           = 40,
        APOBIOSIS_CAST_RING_FADE_STEP           = 24,
        APOBIOSIS_CAST_SPARSE_FADE_STEP         = 16,
        APOBIOSIS_CAST_SPARSE_FADE_FLOOR        = 64,
        APOBIOSIS_CAST_SINGLE_FADE_STEP         = 12,
        APOBIOSIS_CAST_SINGLE_FADE_FLOOR        = 32,
        APOBIOSIS_CAST_FINAL_FADE_STEP          = 8,
        APOBIOSIS_CAST_QUARTER_TURN_SHIFT       = 10,
        APOBIOSIS_CAST_ANGLE_JITTER_MASK        = 255,
        APOBIOSIS_CAST_ANGLE_JITTER_HALF_SPAN   = 128,
        APOBIOSIS_CAST_SPARSE_SHARD_CHANCE_MASK = 3,
        APOBIOSIS_CAST_SPARSE_RADIUS_MASK       = 1023,
        APOBIOSIS_CAST_SINGLE_RADIUS_MASK       = 2047,
        APOBIOSIS_CAST_DOUBLE_RADIUS_MASK       = 4095,
        APOBIOSIS_CAST_FLASH_JITTER_MASK        = 127,
        APOBIOSIS_CAST_FLASH_JITTER_BASE        = 96,
        APOBIOSIS_CAST_SEEDED_ANGLES_PER_STRIP  = 2,
        APOBIOSIS_CAST_DOUBLE_SHARDS_PER_TICK   = 2,
        APOBIOSIS_CAST_RING_MOTOR_FRAMES        = 10,
        APOBIOSIS_CAST_SHARD_MOTOR_FRAMES       = 20,
        APOBIOSIS_CAST_DOUBLE_MOTOR_FRAMES      = 34,
        APOBIOSIS_CAST_MOTOR_LEVEL_ONE_FRAMES   = 18,
        APOBIOSIS_CAST_MOTOR_LEVEL_STEP         = 8,
        APOBIOSIS_CAST_MOTOR_START              = 255,
        APOBIOSIS_CAST_MOTOR_END                = 8,
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
    if ((Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case APOBIOSIS_CAST_STATE_INITIALIZE:
                // Publish the parent for shards, then seed the level-dependent ring.
                D_apobiosis_80130BA0 = task;
                coord->parent        = work->parent;
                gfxSetRotIdentity(&coord->coord);
                coord->coord.t[0]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[2]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(D_apobiosis_80130B74[(u16)(Gp_StateC08.attachId % APOBIOSIS_LEVEL_ID_RADIX) - 1], pan,
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                task->state  = APOBIOSIS_CAST_STATE_RING;
                work->index  = Gp_StateC08.attachId % APOBIOSIS_LEVEL_ID_RADIX - 1;
                work->scale  = APOBIOSIS_CAST_INITIAL_RADIUS;
                work->period = APOBIOSIS_CAST_UNUSED_PERIOD;
                work->step   = APOBIOSIS_CAST_INITIAL_BRIGHTNESS;
                for (ringIndex = 0; ringIndex < D_apobiosis_80130B5C[work->index].stripCount * APOBIOSIS_CAST_SEEDED_ANGLES_PER_STRIP; ringIndex++) {
                    gRandomLcgState                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_apobiosis_80130B80[ringIndex] = (ringIndex << APOBIOSIS_CAST_QUARTER_TURN_SHIFT) + ((gRandomLcgState >> 16) & (APOBIOSIS_QUARTER_TURN - 1));
                }
                padScriptSpawnVariableMotorRamp(APOBIOSIS_CAST_RING_MOTOR_FRAMES, APOBIOSIS_CAST_MOTOR_START, APOBIOSIS_CAST_MOTOR_END);
                /* fallthrough */
            case APOBIOSIS_CAST_STATE_RING:
                actorRenderComposeCoord(coord);
                if (work->age == APOBIOSIS_CAST_APPLY_STATS_AGE) {
                    Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                _apobiosisDrawScreenFlash(work->step);
                rgb[0] = rgb[1] = work->step >> 2;
                rgb[2]          = work->step >> 1;
                // Lift only the composed draw origin; local placement is unchanged.
                coord->workm.t[1] -= APOBIOSIS_CAST_RING_LIFT;
                work->scale        = work->scale + D_apobiosis_80130B5C[work->index].radiusStep;
                _apobiosisDrawShardSprite(
                    &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1], work->age,
                    D_apobiosis_80130B5C[work->index].playerSpriteScale, 0);
                _glowDrawHalo(coord, work->scale, APOBIOSIS_CAST_HALO_THICKNESS, rgb);
                if (work->age & 1) {
                    _glowDrawHalo(coord, APOBIOSIS_CAST_HALO_THICKNESS, work->scale, rgb);
                }
                for (ringIndex = 0; ringIndex < D_apobiosis_80130B5C[work->index].stripCount; ringIndex++) {
                    gRandomLcgState                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_apobiosis_80130B80[ringIndex] -= ((gRandomLcgState >> 16) & APOBIOSIS_CAST_ANGLE_JITTER_MASK) - APOBIOSIS_CAST_ANGLE_JITTER_HALF_SPAN;
                    // Retained original access: radiusStep is 192/256/320, beyond the 16 seeded halfwords.
                    secondaryAngleIndex                        = ringIndex + D_apobiosis_80130B5C[work->index].radiusStep;
                    gRandomLcgState                            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_apobiosis_80130B80[secondaryAngleIndex] -= ((gRandomLcgState >> 16) & APOBIOSIS_CAST_ANGLE_JITTER_MASK) - APOBIOSIS_CAST_ANGLE_JITTER_HALF_SPAN;
                    work->pos.vx                               = work->scale * rsin(D_apobiosis_80130B80[ringIndex]) >> APOBIOSIS_TRIG_SHIFT;
                    work->pos.vy                               = work->scale * rcos(D_apobiosis_80130B80[ringIndex]) >> APOBIOSIS_TRIG_SHIFT;
                    work->pos.vz =
                        work->pos.vx *
                            rcos(D_apobiosis_80130B80
                                     [ringIndex + D_apobiosis_80130B5C[work->index].radiusStep]) >>
                        APOBIOSIS_TRIG_SHIFT;
                    _apobiosisDrawShardStrip(coord, &work->pos, work->age,
                                             D_apobiosis_80130B5C[work->index].stripScale);
                }
                coord->workm.t[1] += APOBIOSIS_CAST_RING_LIFT;
                if (work->step >= APOBIOSIS_CAST_RING_FADE_STEP + 1) {
                    work->step = work->step - APOBIOSIS_CAST_RING_FADE_STEP;
                    return;
                }
                task->state = APOBIOSIS_CAST_STATE_SPARSE_SHARDS;
                padScriptSpawnVariableMotorRamp(APOBIOSIS_CAST_SHARD_MOTOR_FRAMES, APOBIOSIS_CAST_MOTOR_START, APOBIOSIS_CAST_MOTOR_END);
                return;
            case APOBIOSIS_CAST_STATE_SPARSE_SHARDS:
                // Shard phases reuse scale as a random radius; flash rolls follow each successful chance.
                actorRenderComposeCoord(coord);
                _apobiosisDrawScreenFlash(work->step);
                if (work->step >= APOBIOSIS_CAST_SPARSE_FADE_FLOOR + 1) {
                    work->step = work->step - APOBIOSIS_CAST_SPARSE_FADE_STEP;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & APOBIOSIS_CAST_SPARSE_SHARD_CHANCE_MASK) == 0) {
                    _apobiosisSpawnDriftingShard(work, coord, APOBIOSIS_CAST_SPARSE_RADIUS_MASK);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->step      = ((gRandomLcgState >> 16) & APOBIOSIS_CAST_FLASH_JITTER_MASK) + APOBIOSIS_CAST_FLASH_JITTER_BASE;
                }
                if (work->age == APOBIOSIS_CAST_SPARSE_END_AGE) {
                    work->step  = APOBIOSIS_CAST_INITIAL_BRIGHTNESS;
                    task->state = APOBIOSIS_CAST_STATE_SINGLE_SHARDS;
                }
                return;
            case APOBIOSIS_CAST_STATE_SINGLE_SHARDS:
                _apobiosisDrawScreenFlash(work->step);
                if (work->step >= APOBIOSIS_CAST_SINGLE_FADE_FLOOR + 1) {
                    work->step = work->step - APOBIOSIS_CAST_SINGLE_FADE_STEP;
                }
                _apobiosisSpawnDriftingShard(work, coord, APOBIOSIS_CAST_SINGLE_RADIUS_MASK);
                if (work->age == APOBIOSIS_CAST_SINGLE_END_AGE) {
                    if (work->index <= APOBIOSIS_CAST_LEVEL_ONE_ROW) {
                        task->state = APOBIOSIS_CAST_STATE_FADING;
                        padScriptSpawnVariableMotorRamp(work->index * APOBIOSIS_CAST_MOTOR_LEVEL_STEP + APOBIOSIS_CAST_MOTOR_LEVEL_ONE_FRAMES, APOBIOSIS_CAST_MOTOR_START, APOBIOSIS_CAST_MOTOR_END);
                    } else {
                        work->step  = APOBIOSIS_CAST_INITIAL_BRIGHTNESS;
                        task->state = APOBIOSIS_CAST_STATE_DOUBLE_SHARDS;
                        padScriptSpawnVariableMotorRamp(APOBIOSIS_CAST_DOUBLE_MOTOR_FRAMES, APOBIOSIS_CAST_MOTOR_START, APOBIOSIS_CAST_MOTOR_END);
                    }
                }
                return;
            case APOBIOSIS_CAST_STATE_DOUBLE_SHARDS:
                _apobiosisDrawScreenFlash(work->step);
                if (work->step >= APOBIOSIS_CAST_FINAL_FADE_STEP + 1) {
                    work->step = work->step - APOBIOSIS_CAST_FINAL_FADE_STEP;
                }
                for (shardIndex = 0; shardIndex < APOBIOSIS_CAST_DOUBLE_SHARDS_PER_TICK; shardIndex++) {
                    _apobiosisSpawnDriftingShard(work, coord, APOBIOSIS_CAST_DOUBLE_RADIUS_MASK);
                }
                if (work->age == APOBIOSIS_CAST_DOUBLE_END_AGE) {
                    task->state = APOBIOSIS_CAST_STATE_FADING;
                }
                return;
            case APOBIOSIS_CAST_STATE_FADING:
                _apobiosisDrawScreenFlash(work->step);
                if (work->step >= APOBIOSIS_CAST_FINAL_FADE_STEP + 1) {
                    work->step = work->step - APOBIOSIS_CAST_FINAL_FADE_STEP;
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Queues the additive full-screen apobiosis flash at a fixed ordering depth.
///
/// `brightness` is an RGB intensity in 0-255. The flash is blue, with red and
/// green halved; PE level 3 has a one-in-four chance of yellow instead. The
/// random sequence advances only at level 3. Compensates the applied vertical
/// screen shake and consumes a quad plus its blend-mode packet in the frame arena.
static void _apobiosisDrawScreenFlash(s16 brightness)
{
    enum {
        APOBIOSIS_FLASH_HALF_WIDTH  = 160,
        APOBIOSIS_FLASH_HALF_HEIGHT = 120,
        APOBIOSIS_FLASH_DEPTH       = 48,
    };
    POLY_F4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    if ((u16)(Gp_StateC08.attachId % (u32)APOBIOSIS_LEVEL_ID_RADIX) - 1 == APOBIOSIS_LEVEL_THREE_ROW && (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & APOBIOSIS_ALT_COLOR_ROLL_MASK) == 0) {
        setRGB0(quad, brightness, brightness, brightness >> 1);
    } else {
        setRGB0(quad, brightness >> 1, brightness >> 1, brightness);
    }
    // Cancel the draw environment's shake offset so the flash covers the display.
    setXY4(quad, -APOBIOSIS_FLASH_HALF_WIDTH, -APOBIOSIS_FLASH_HALF_HEIGHT - gDisplayState.vramYOffset, APOBIOSIS_FLASH_HALF_WIDTH,
           -APOBIOSIS_FLASH_HALF_HEIGHT - gDisplayState.vramYOffset, -APOBIOSIS_FLASH_HALF_WIDTH, APOBIOSIS_FLASH_HALF_HEIGHT - gDisplayState.vramYOffset,
           APOBIOSIS_FLASH_HALF_WIDTH, APOBIOSIS_FLASH_HALF_HEIGHT - gDisplayState.vramYOffset);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(APOBIOSIS_FLASH_DEPTH << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            quad);
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, APOBIOSIS_FLASH_DEPTH);
}

#define GLOW_DRAW_HALO_PULL 0x40
#include "../../shared/glow_draw_halo.inc.c"

/// Seeds one apobiosis shard's strip displacement, fixed sprite rotation and PE level row.
///
/// `work` must be a live, writable `EffectWork` whose spawn offset has already
/// positioned its coordinate. Replaces `pos` with a world-space displacement:
/// Y is -4096 and X/Z are each in -2047..2048. The strip adds this displacement
/// to the coordinate's world origin without applying its rotation.
///
/// Advances the shared random sequence three times, for X, Z and then `angle`.
/// The sprite rotation stays fixed in 0..4095, with 4096 units per turn.
/// The active spell must be apobiosis at PE level 1-3; its level digit selects
/// `step`, the zero-based tuning row in 0..2. Rendering sizes come from that
/// row. The retained write of 128 to `scale` has no reader in the shard task.
static inline void _apobiosisInitShardAppearance(EffectWork* work)
{
    enum {
        APOBIOSIS_SHARD_END_Y         = -4096,
        APOBIOSIS_SHARD_UNUSED_SCALE  = 128,
        APOBIOSIS_SHARD_END_SPAN      = 4096,
        APOBIOSIS_SHARD_END_HALF_SPAN = APOBIOSIS_SHARD_END_SPAN / 2,
    };
    // The spawn offset has positioned the shard; pos now describes its strip.
    work->pos.vy    = APOBIOSIS_SHARD_END_Y;
    work->scale     = APOBIOSIS_SHARD_UNUSED_SCALE;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->pos.vx    = APOBIOSIS_SHARD_END_HALF_SPAN - ((gRandomLcgState >> 16) & (APOBIOSIS_SHARD_END_SPAN - 1));
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->pos.vz    = APOBIOSIS_SHARD_END_HALF_SPAN - ((gRandomLcgState >> 16) & (APOBIOSIS_SHARD_END_SPAN - 1));
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->angle     = (gRandomLcgState >> 16) & (APOBIOSIS_FULL_TURN - 1);
    work->step      = Gp_StateC08.attachId % APOBIOSIS_LEVEL_ID_RADIX - 1;
}

void apobiosisShardTask(Task* task)
{
    enum {
        APOBIOSIS_SHARD_INIT            = 0,
        APOBIOSIS_SHARD_PINNED          = 1,
        APOBIOSIS_SHARD_DRIFTING        = 2,
        APOBIOSIS_SHARD_PINNED_FRAMES   = 25,
        APOBIOSIS_SHARD_DRIFTING_FRAMES = 17,
        APOBIOSIS_SHARD_DRIFT_HALF_SPAN = 64,
        APOBIOSIS_SHARD_DRIFT_MASK      = 127,
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case APOBIOSIS_SHARD_INIT:
                // Join the live cast's task tree before choosing a pinned or drifting coordinate.
                taskReparent(D_apobiosis_80130BA0, task);
                if (task->spawnArg1.value != 0) {
                    coord->parent       = work->parent;
                    coord->coord.t[0]   = 0;
                    coord->coord.t[1]   = 0;
                    coord->coord.t[2]   = 0;
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(coord);
                    task->state = APOBIOSIS_SHARD_PINNED;
                } else {
                    work->move.vy   = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = APOBIOSIS_SHARD_DRIFT_HALF_SPAN - ((gRandomLcgState >> 16) & APOBIOSIS_SHARD_DRIFT_MASK);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = APOBIOSIS_SHARD_DRIFT_HALF_SPAN - ((gRandomLcgState >> 16) & APOBIOSIS_SHARD_DRIFT_MASK);
                    task->state     = APOBIOSIS_SHARD_DRIFTING;
                }
                _apobiosisInitShardAppearance(work);
                return;
            case APOBIOSIS_SHARD_PINNED:
                // The work's step is the level row; index advances only on drawn frames.
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index = work->index + 1;
                    _apobiosisDrawShardSprite(coord, work->index,
                                              D_apobiosis_80130B5C[work->step].stripScale * 2,
                                              work->angle);
                    _apobiosisDrawShardStrip(coord, &work->pos, work->index,
                                             D_apobiosis_80130B5C[work->step].stripScale * 2);
                }
                if (work->age < APOBIOSIS_SHARD_PINNED_FRAMES) {
                    return;
                }
                break;
            case APOBIOSIS_SHARD_DRIFTING:
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index = work->index + 1;
                    _apobiosisDrawShardSprite(coord, work->index,
                                              D_apobiosis_80130B5C[work->step].stripScale,
                                              work->angle);
                    _apobiosisDrawShardStrip(coord, &work->pos, work->index,
                                             D_apobiosis_80130B5C[work->step].stripScale);
                }
                if (work->age < APOBIOSIS_SHARD_DRIFTING_FRAMES) {
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Queues the rotating six-frame apobiosis sprite at a composed coordinate's world origin.
///
/// `textureFrame` is nonnegative and wraps every six draws. `sizeScale` is a
/// signed perspective scale: the corner distance is `sizeScale * 39 / depth`
/// pixels, with depth taken from SZ3 / 4 plus one. `screenAngle` uses 4096 units
/// per turn. World translation components narrow to s16 before projection.
/// Uses raw texture colour and additive blending; PE level 3 rolls an alternate
/// palette once in four accepted projections. Rejects a negative GTE FLAG word.
/// Borrows and releases one scratch block, consumes a GPU quad on success, and
/// overwrites the GTE matrix and projection registers.
static void _apobiosisDrawShardSprite(const GfxCoord* coord, s16 textureFrame, s16 sizeScale, s16 screenAngle)
{
    enum {
        APOBIOSIS_SPRITE_FRAME_COUNT = 6,
        APOBIOSIS_SPRITE_CELL_WIDTH  = 40,
        APOBIOSIS_SPRITE_UV_SPAN     = APOBIOSIS_SPRITE_CELL_WIDTH - 1,
        APOBIOSIS_SPRITE_TOP_V       = 56,
        APOBIOSIS_SPRITE_BOTTOM_V    = APOBIOSIS_SPRITE_TOP_V + APOBIOSIS_SPRITE_UV_SPAN,
        APOBIOSIS_SPRITE_DEPTH_BIAS  = 1,
    };
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s16                 cell;
    s32                 uLeft;
    s32                 uRight;
    s32                 quarterTurnAngle;

    /// Resolves one perpendicular corner pair in signed pixels.
    ///
    /// Arguments must be side-effect-free: the scratch pointer is used four
    /// times, and scale and angle twice. Captures the local UV span and Q12 shift.
    /// Expands to two statements; use inside a braced block. Scoped to this drawer.
#define APOBIOSIS_SPRITE_SET_CORNER_OFFSET(scratchBlock, scale, angle)                                                                          \
    (scratchBlock)->extent.corner.x = ((((scale) * APOBIOSIS_SPRITE_UV_SPAN) / (scratchBlock)->depth) * rsin((angle))) >> APOBIOSIS_TRIG_SHIFT; \
    (scratchBlock)->extent.corner.y = ((((scale) * APOBIOSIS_SPRITE_UV_SPAN) / (scratchBlock)->depth) * rcos((angle))) >> APOBIOSIS_TRIG_SHIFT

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
        scratch->depth += APOBIOSIS_SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
        if ((u16)(Gp_StateC08.attachId % APOBIOSIS_LEVEL_ID_RADIX) - 1 == APOBIOSIS_LEVEL_THREE_ROW) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & APOBIOSIS_ALT_COLOR_ROLL_MASK) == 0) {
                quad->clut = getClut(144, 267);
            } else {
                quad->clut = getClut(304, 266);
            }
        } else {
            quad->clut = getClut(304, 266);
        }
        cell   = textureFrame % APOBIOSIS_SPRITE_FRAME_COUNT;
        uLeft  = cell * APOBIOSIS_SPRITE_CELL_WIDTH;
        uRight = uLeft + APOBIOSIS_SPRITE_UV_SPAN;
        setUV4(quad, uLeft, APOBIOSIS_SPRITE_TOP_V, uRight, APOBIOSIS_SPRITE_TOP_V, uLeft, APOBIOSIS_SPRITE_BOTTOM_V, uRight, APOBIOSIS_SPRITE_BOTTOM_V);
        // Resolve two perpendicular corner pairs around the projected centre.
        APOBIOSIS_SPRITE_SET_CORNER_OFFSET(scratch, sizeScale, screenAngle);
        quad->x0         = scratch->screenX + scratch->extent.corner.x;
        quad->x3         = scratch->screenX - scratch->extent.corner.x;
        quad->y0         = scratch->screenY - scratch->extent.corner.y;
        quad->y3         = scratch->screenY + scratch->extent.corner.y;
        quarterTurnAngle = screenAngle + APOBIOSIS_QUARTER_TURN;
        APOBIOSIS_SPRITE_SET_CORNER_OFFSET(scratch, sizeScale, quarterTurnAngle);
        quad->x1 = scratch->screenX + scratch->extent.corner.x;
        quad->x2 = scratch->screenX - scratch->extent.corner.x;
        quad->y1 = scratch->screenY - scratch->extent.corner.y;
        quad->y2 = scratch->screenY + scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef APOBIOSIS_SPRITE_SET_CORNER_OFFSET

/// Queues an additive apobiosis strip from a composed world origin to an offset endpoint.
///
/// `endOffset` is an XYZ displacement in world units, without the coordinate's
/// rotation applied. Both world endpoints narrow to s16 before projection.
/// `textureFrame` selects one of four 128-by-24 texture cells through its low
/// two bits. The signed `widthScale` gives a screen half-width of
/// `widthScale * 23 / (SZ3 / 4 + 1)` pixels using the start point's depth; that
/// same depth sorts the whole strip. Rejects either negative GTE FLAG word.
/// Uses raw texture colour; PE level 3 rolls an alternate palette once in four
/// accepted strips. Borrows and releases one scratch block, consumes one GPU
/// quad on success, and overwrites the GTE matrix and projection registers.
static void _apobiosisDrawShardStrip(const GfxCoord* coord, const SVECTOR* endOffset, s16 textureFrame, s16 widthScale)
{
    enum {
        APOBIOSIS_STRIP_DEPTH_BIAS  = 1,
        APOBIOSIS_STRIP_CELL_WIDTH  = 128,
        APOBIOSIS_STRIP_CELL_HEIGHT = 24,
        APOBIOSIS_STRIP_COLUMN_MASK = 1,
        APOBIOSIS_STRIP_FRAME_MASK  = 3,
        APOBIOSIS_STRIP_TOP_V       = 208,
    };
    EffectStripScratch* scratch;
    POLY_FT4*           quad;
    s32                 uLeft;
    s32                 uRight;
    s32                 vTop;
    s32                 vBottom;
    s16                 screenAngle;

    /// Projects an endpoint using the already-loaded GTE matrices, leaving SZ3 available.
    ///
    /// The complete SVECTOR and both output words are word-aligned live storage.
    /// Writes signed screen pixels and the FLAG word. Each pointer expression
    /// is evaluated once, in order; captures no caller identifiers. Expands to
    /// four statements, so use in a braced block. Scoped to this drawer.
#define APOBIOSIS_STRIP_PROJECT_POINT(worldPoint, screenPoint, projectionFlags) \
    gte_ldv0((worldPoint));                                                     \
    gte_rtps();                                                                 \
    gte_stsxy((screenPoint));                                                   \
    gte_stflg((projectionFlags))

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
    APOBIOSIS_STRIP_PROJECT_POINT(&scratch->worldStart, &scratch->screenStart, &scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth += APOBIOSIS_STRIP_DEPTH_BIAS;
        APOBIOSIS_STRIP_PROJECT_POINT(&scratch->worldEnd, &scratch->screenEnd, &scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            if ((u16)(Gp_StateC08.attachId % (u32)APOBIOSIS_LEVEL_ID_RADIX) - 1 == APOBIOSIS_LEVEL_THREE_ROW &&
                (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & APOBIOSIS_ALT_COLOR_ROLL_MASK) == 0) {
                quad->clut = getClut(128, 267);
            } else {
                quad->clut = getClut(112, 266);
            }
            uLeft  = (textureFrame & APOBIOSIS_STRIP_COLUMN_MASK) * APOBIOSIS_STRIP_CELL_WIDTH;
            uRight = uLeft + (APOBIOSIS_STRIP_CELL_WIDTH - 1);
            // Keep V origins signed until the packet narrows them to bytes.
            vTop    = ((textureFrame & APOBIOSIS_STRIP_FRAME_MASK) >> 1) * APOBIOSIS_STRIP_CELL_HEIGHT - (256 - APOBIOSIS_STRIP_TOP_V);
            vBottom = ((textureFrame & APOBIOSIS_STRIP_FRAME_MASK) >> 1) * APOBIOSIS_STRIP_CELL_HEIGHT -
                      (256 - APOBIOSIS_STRIP_TOP_V - (APOBIOSIS_STRIP_CELL_HEIGHT - 1));
            setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
            // Split the sprite's two corner pairs between the projected segment's ends.
            screenAngle            = ratan2(scratch->screenEnd.vy - scratch->screenStart.vy, scratch->screenEnd.vx - scratch->screenStart.vx);
            scratch->cornerOffsetX = (((widthScale * (APOBIOSIS_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle)) >> APOBIOSIS_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (APOBIOSIS_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle)) >> APOBIOSIS_TRIG_SHIFT;
            quad->x0               = scratch->screenStart.vx + scratch->cornerOffsetX;
            quad->x3               = scratch->screenEnd.vx - scratch->cornerOffsetX;
            quad->y0               = scratch->screenStart.vy - scratch->cornerOffsetY;
            quad->y3               = scratch->screenEnd.vy + scratch->cornerOffsetY;
            scratch->cornerOffsetX = (((widthScale * (APOBIOSIS_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle + APOBIOSIS_QUARTER_TURN)) >> APOBIOSIS_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (APOBIOSIS_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle + APOBIOSIS_QUARTER_TURN)) >> APOBIOSIS_TRIG_SHIFT;
            quad->x1               = scratch->screenEnd.vx + scratch->cornerOffsetX;
            quad->x2               = scratch->screenStart.vx - scratch->cornerOffsetX;
            quad->y1               = scratch->screenEnd.vy - scratch->cornerOffsetY;
            quad->y2               = scratch->screenStart.vy + scratch->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}

#undef APOBIOSIS_STRIP_PROJECT_POINT
