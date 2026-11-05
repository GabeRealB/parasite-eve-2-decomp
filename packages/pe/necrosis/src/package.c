#include "necrosis.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor.h>
#include <gameplay/actor_render.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effects.h>
#include <gameplay/pad_script.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/gfx_types.h>
#include <engine/mem.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
/// Signed texture-frame selector; the caller reduces the effect age modulo six.
#define SPRITE_QUAD_FRAME_T s16
#include <effects/sprite_quad/sprite_quad.h>

PE2_EDITOR_LABEL("Cloud")
/// The cloud's speed as it launches along the caster's facing, and what it keeps of it each frame, in 4096ths: above
/// 4096 it speeds up.
PE2_OPTION_DEF(LAUNCH_SPEED, int, 0x90)
PE2_OPTION_DEF(ACCELERATION, int, 0x1100)
/// The radius of the sphere that stops the cloud at a wall, and what the damage sphere gains each frame.
PE2_OPTION_DEF(GRID_PROBE_RADIUS, int, 0x80)
PE2_OPTION_DEF(DAMAGE_GROWTH, int, 0x20)
/// What each frame adds to the size of the trail puff the cloud sheds, and the frames the cast waits for its trail
/// after the cloud stops.
PE2_OPTION_DEF(TRAIL_GROWTH, int, 0x60)
PE2_OPTION_DEF(TRAIL_WAIT_FRAMES, int, 0x10)
/// The pad's rumble lasts the level's travel and this many frames more, from start to end strength.
PE2_OPTION_DEF(RUMBLE_TAIL_FRAMES, int, 0xC)
PE2_OPTION_DEF(RUMBLE_START, int, 0xFF)
PE2_OPTION_DEF(RUMBLE_END, int, 8)

PE2_EDITOR_LABEL("Trail")
/// A trail puff shrinks each frame by its first size shifted down by this, and every this many frames sheds a mist
/// puff that much smaller than its first size.
PE2_OPTION_DEF(TRAIL_SHRINK_SHIFT, int, 4)
PE2_OPTION_DEF(MIST_INTERVAL, int, 3)
PE2_OPTION_DEF(MIST_SIZE_SHORTFALL, int, 0x100)

PE2_EDITOR_LABEL("Mist")
/// A mist puff drifts each frame by its size over this.
PE2_OPTION_DEF(MIST_DRIFT_DIVISOR, int, 20)
/// A mist puff is additive rather than subtractive at a chance of its level less one in this, a power of two.
PE2_OPTION_DEF(MIST_BLEND_ROLL, int, 4)
/// From this level, subtractive mist puffs are drawn from the large strip.
PE2_OPTION_DEF(LARGE_MIST_LEVEL, int, 3)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(MIST_BLEND_ROLL)), mist_blend_roll_is_a_mask);
STATIC_ASSERT(PE2_OPTION(MIST_INTERVAL) > 0 && PE2_OPTION(MIST_DRIFT_DIVISOR) > 0, divisors);

/// The cloud's tuning at one Parasite Energy level, by the level digit of the attachment id less one. Its frames are
/// counted while the room's PE runs.
typedef struct {
    s16 startRadius;  // The damage sphere's radius as it launches, and the trail puffs' size before their growth
    s16 travelFrames; // Frames the cloud travels and does damage for
} _LevelTuning;
PE2_TYPE(_LevelTuning)

/// The cloud's collision, which the cast allocates and frees: a damage sphere keyed with the spell's damage id, and a
/// sphere the room's grid stops, both on the cloud and sharing one contact.
typedef struct {
    WorldCollisionBody    damageBody;
    WorldCollisionBody    gridBody;    // Unlinked when the grid stops the cloud, which leaves the damage sphere growing in place
    WorldCollisionContact contacts[1]; // Cleared each frame of travel
} _CloudCollision;

PE2_EDITOR_LABEL("Levels")
/// The cloud at levels 1 to 3.
PE2_OPTION_DEF(LEVELS, _LevelTuning[3], {
    { 0x03C0, 0x000A },
    { 0x0480, 0x000F },
    { 0x0540, 0x0014 },
})
PE2_EDITOR_LABEL("")

static _LevelTuning _gLevelTuning[ATTACHMENT_LEVEL_COUNT] = PE2_OPTION(LEVELS);

enum {
    CAST_SOUND_LEVEL_1 = (s32)0xE0150001,
    CAST_SOUND_LEVEL_2 = (s32)0xE0180001,
    CAST_SOUND_LEVEL_3 = (s32)0xE01B0001,
};

/// The cue each level's cloud plays as it launches.
static s32 _gCastSoundScripts[ARRAY_SIZE(_gLevelTuning)] = {
    CAST_SOUND_LEVEL_1,
    CAST_SOUND_LEVEL_2,
    CAST_SOUND_LEVEL_3,
};

/// A mist puff's frame and blend, packed, and its strips' cells.
enum {
    PUFF_BLEND_ADD_FLAG     = 0x1000, // Set: additive blend and its palette; clear: subtractive blend and its palette
    PUFF_FRAME_MASK         = 0xF,
    PUFF_SMALL_CELL_WIDTH   = 32,
    PUFF_LARGE_CELL_WIDTH   = 40,
    PUFF_ANGLE_MASK         = 0xFFF, // One turn is 0x1000 angle units
    PUFF_QUARTER_TURN       = 0x400,
    PUFF_TRIG_FRACTION_BITS = 12,
};

static void _drawMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle);
static void _drawLargeMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle);

/// Moves a mist puff by its drift and composes it.
static inline void _advanceMistPuff(GfxCoord* coord, const EffectWork* effect)
{
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

/// Sets a small mist puff's corners, turned by `cornerAngle` around its projected center and sized by `size`.
static inline void _setMistPuffCorners(EffectShapeScratch* scratch, POLY_FT4* quad, s16 size, s16 cornerAngle)
{
    s32 perpendicularAngle;

    // Opposite corners share an offset; the other pair lies a quarter turn away.
    scratch->extent.corner.x = (((size * (PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rsin(cornerAngle)) >> PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rcos(cornerAngle)) >> PUFF_TRIG_FRACTION_BITS;
    quad->x0                 = scratch->screenX + scratch->extent.corner.x;
    quad->x3                 = scratch->screenX - scratch->extent.corner.x;
    quad->y0                 = scratch->screenY - scratch->extent.corner.y;
    quad->y3                 = scratch->screenY + scratch->extent.corner.y;
    perpendicularAngle       = cornerAngle + PUFF_QUARTER_TURN;
    scratch->extent.corner.x = (((size * (PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rsin(perpendicularAngle)) >> PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rcos(perpendicularAngle)) >> PUFF_TRIG_FRACTION_BITS;
    quad->x1                 = scratch->screenX + scratch->extent.corner.x;
    quad->x2                 = scratch->screenX - scratch->extent.corner.x;
    quad->y1                 = scratch->screenY - scratch->extent.corner.y;
    quad->y2                 = scratch->screenY + scratch->extent.corner.y;
}

/// Sets a large mist puff's corners, as `_setMistPuffCorners` does.
static inline void _setLargeMistPuffCorners(EffectShapeScratch* scratch, POLY_FT4* quad, s16 size, s16 cornerAngle)
{
    s32 perpendicularAngle;

    // Opposite corners share an offset; the other pair lies a quarter turn away.
    scratch->extent.corner.x = (((size * (PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rsin(cornerAngle)) >> PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rcos(cornerAngle)) >> PUFF_TRIG_FRACTION_BITS;
    quad->x0                 = scratch->screenX + scratch->extent.corner.x;
    quad->x3                 = scratch->screenX - scratch->extent.corner.x;
    quad->y0                 = scratch->screenY - scratch->extent.corner.y;
    quad->y3                 = scratch->screenY + scratch->extent.corner.y;
    perpendicularAngle       = cornerAngle + PUFF_QUARTER_TURN;
    scratch->extent.corner.x = (((size * (PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rsin(perpendicularAngle)) >> PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rcos(perpendicularAngle)) >> PUFF_TRIG_FRACTION_BITS;
    quad->x1                 = scratch->screenX + scratch->extent.corner.x;
    quad->x2                 = scratch->screenX - scratch->extent.corner.x;
    quad->y1                 = scratch->screenY - scratch->extent.corner.y;
    quad->y2                 = scratch->screenY + scratch->extent.corner.y;
}

/// Moves the cloud by its velocity and composes it.
static inline void _advanceCastCoord(GfxCoord* coord, const EffectWork* effect)
{
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void necrosisCastTask(Task* task)
{
    enum {
        CAST_INITIALIZE       = 0,
        CAST_TRAVELLING       = 1,
        CAST_WAIT_FOR_TRAIL   = 2,
        SPELL_DAMAGE_KEY_BASE = WORLD_COLLISION_CONTACT_ATTACK | 0x8000,
    };
    _CloudCollision*       collision;
    EffectWork*            effect;
    GfxCoord*              coord;
    GfxCoord*              playerCoord;
    WorldCollisionContact* contacts;
    EffectWork*            spawned;
    s32                    pan;
    u16                    previousAge;
    s32                    nextAge;
    s16                    peEffectControl;

    collision   = task->work;
    effect      = task->spawnArg2.pointer;
    coord       = task->extra.coordBody->coord;
    previousAge = effect->age;
    nextAge     = previousAge + 1;
    effect->age = nextAge;
    switch (task->state) {
        case CAST_INITIALIZE:
            if (gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) {
                break;
            }
            peEffectControl = gRoomEffectState->peEffectControl;
            if (peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                break;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = previousAge;
                return;
            }
            collision = memCalloc(sizeof(_CloudCollision), 0);
            if (collision == NULL) {
                effect->age = 0;
                return;
            }
            playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            // Borrow only the player's rotation; this cloud retains its own translation.
            gfxCopyRotationWords(&coord->coord, &playerCoord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            effect->move.vx = 0;
            effect->move.vy = 0;
            effect->move.vz = PE2_OPTION(LAUNCH_SPEED);
            gte_SetRotMatrix(&playerCoord->coord);
            gte_ldv0(&effect->move);
            gte_rtv0();
            gte_stsv(&effect->move);
            contacts                               = collision->contacts;
            effect->index                          = (gAttachmentState.attachId % 10) - 1;
            task->work                             = collision;
            collision->damageBody.coord            = coord;
            collision->damageBody.context.contacts = contacts;
            collision->damageBody.key =
                ((u16)(gAttachmentState.attachId / 100) - 1) * 9 + ((u16)((u16)(gAttachmentState.attachId % 100) / 10) - 1) * 3 + (u16)(gAttachmentState.attachId % 10) + SPELL_DAMAGE_KEY_BASE;
            collision->damageBody.radius = _gLevelTuning[effect->index].startRadius;
            collision->damageBody.flags  = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &collision->damageBody);
            contacts->flags                      = WORLD_COLLISION_CONTACT_LAST;
            collision->gridBody.coord            = coord;
            collision->gridBody.context.contacts = contacts;
            collision->gridBody.key              = 0;
            collision->gridBody.radius           = PE2_OPTION(GRID_PROBE_RADIUS);
            collision->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            collision->damageBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_GRID_ONLY, &collision->gridBody);
            collision->gridBody.flags = (collision->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            pan                       = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gCastSoundScripts[(u16)(gAttachmentState.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            padScriptSpawnVariableMotorRamp(_gLevelTuning[effect->index].travelFrames + PE2_OPTION(RUMBLE_TAIL_FRAMES), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            task->state = CAST_TRAVELLING;
            /* fallthrough */
        case CAST_TRAVELLING:
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                // Accelerate before moving and emitting the growing trail, including launch.
                gte_lddp(PE2_OPTION(ACCELERATION));
                gte_ldsv(&effect->move);
                gte_gpf12();
                gte_stsv(&effect->move);
                _advanceCastCoord(coord, effect);
                spawned = effectSpawn((EFFECT_NECROSIS_TRAIL_PUFF | EFFECT_SPAWN_UNLIMITED), coord,
                                      _gLevelTuning[effect->index].startRadius + (effect->age * PE2_OPTION(TRAIL_GROWTH)),
                                      NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                collision->damageBody.radius = collision->damageBody.radius + PE2_OPTION(DAMAGE_GROWTH);
            } else {
                effect->age = effect->age - 1;
            }
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                break;
            }
            if (effect->age > _gLevelTuning[effect->index].travelFrames) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = CAST_WAIT_FOR_TRAIL;
                return;
            }
            if (worldCollisionFindContactIndex(collision->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                effect->move.vx = 0;
                effect->move.vy = 0;
                effect->move.vz = 0;
                worldCollisionUnlinkBody(&collision->gridBody);
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case CAST_WAIT_FOR_TRAIL:
            // Bodies are unlinked; the final wait continues aging even while PE is paused.
            if (gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) {
                break;
            }
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                break;
            }
            nextAge = (s16)nextAge;
            if ((_gLevelTuning[effect->index].travelFrames + PE2_OPTION(TRAIL_WAIT_FRAMES)) < nextAge) {
                break;
            }
            return;
        default:
            return;
    }
    effectKillTask(effect, task);
}

void necrosisTrailPuffTask(Task* task)
{
    enum {
        TRAIL_INITIALIZE      = 0,
        TRAIL_SHRINKING       = 1,
        TRAIL_SIZE_ANGLE_MASK = 0xFFF,
        TRAIL_TEXTURE_FRAMES  = 6,
    };
    EffectWork* effect;
    GfxCoord*   coord;
    EffectWork* spawned;

    effect = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    effect->age = effect->age + 1;
    if (task->state == TRAIL_INITIALIZE) {
        effect->scale = task->spawnArg1.value & TRAIL_SIZE_ANGLE_MASK;
        RANDOM_LCG_ADVANCE();
        effect->angle  = RANDOM_LCG_DRAW() & TRAIL_SIZE_ANGLE_MASK;
        effect->period = effect->scale - PE2_OPTION(MIST_SIZE_SHORTFALL);
        effect->step   = effect->scale >> PE2_OPTION(TRAIL_SHRINK_SHIFT);
        task->state    = TRAIL_SHRINKING;
    }
    actorRenderComposeCoord(coord);
    spriteQuadDraw(coord, effect->age % TRAIL_TEXTURE_FRAMES, effect->scale, effect->angle);
    // Retire before the every-third-age child emission when the final shrink expires.
    effect->scale -= effect->step;
    if (effect->scale < effect->step) {
        effectKillTask(effect, task);
        return;
    }
    if (effect->age % PE2_OPTION(MIST_INTERVAL) == 0) {
        spawned = effectSpawn(EFFECT_NECROSIS_MIST_PUFF, coord, (s32)(effect->period), 0);
        if (spawned != NULL) {
            taskReparent(task, spawned->task);
        }
    }
}

/// The trail puff's sprite: six 40-texel cells.
#define SPRITE_QUAD_CLUT       getClut(240, 266)
#define SPRITE_QUAD_CELL_WIDTH 40
#define SPRITE_QUAD_TOP_V      0x38
#define SPRITE_QUAD_BOTTOM_V   (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
#define SPRITE_QUAD_SCALE      (SPRITE_QUAD_CELL_WIDTH - 1)
#include <effects/sprite_quad/draw.inc.c>

void necrosisMistPuffTask(Task* task)
{
    enum {
        PUFF_STATE_INIT        = 0,
        PUFF_STATE_SMALL       = 1,
        PUFF_STATE_LARGE       = 2,
        PUFF_SMALL_FRAME_LIMIT = 8, // Frames 1..7 draw; frame 8 moves once more, then releases
        PUFF_LARGE_FRAME_LIMIT = 6, // Frames 1..5 draw; frame 6 moves once more, then releases
        PUFF_SPAWN_SIZE_MASK   = 0xFFF,
    };
    EffectWork* effect;
    GfxCoord*   coord;
    s16         frame;
    s32         bearingRoll;
    s32         depthRoll;
    s32         blendRoll;
    s32         verticalProduct;
    s32         nextState;
    u16         spawnSizeBits;

    effect = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    effect->age = effect->age + 1;
    switch (task->state) {
        case PUFF_STATE_INIT:
            // Retain size in period, sprite bearing in scale, and drift magnitude in angle.
            // Divide the stored signed size before narrowing the Q12 displacement components.
            effect->age     = 0;
            spawnSizeBits   = task->spawnArg1.value;
            effect->period  = spawnSizeBits & PUFF_SPAWN_SIZE_MASK;
            bearingRoll     = RANDOM_LCG_NEXT(gRandomLcgState);
            effect->scale   = ((u32)bearingRoll >> 16) & PUFF_ANGLE_MASK;
            gRandomLcgState = bearingRoll;
            effect->angle   = effect->period / PE2_OPTION(MIST_DRIFT_DIVISOR);
            effect->move.vx = (rsin(effect->scale) * effect->angle) >> PUFF_TRIG_FRACTION_BITS;
            verticalProduct = rcos(effect->scale) * effect->angle;
            depthRoll       = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = depthRoll;
            effect->move.vy = verticalProduct >> PUFF_TRIG_FRACTION_BITS;
            effect->move.vz = (rsin(((u32)depthRoll >> 16) & PUFF_ANGLE_MASK) * effect->move.vx) >> PUFF_TRIG_FRACTION_BITS;
            blendRoll       = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = blendRoll;
            // The spawner clears step; only selected puffs switch to additive blending.
            if ((s32)(((u32)blendRoll >> 16) & (PE2_OPTION(MIST_BLEND_ROLL) - 1)) < ((u16)(gAttachmentState.attachId % 10U) - 1)) {
                effect->step = PUFF_BLEND_ADD_FLAG;
            }
            if ((u16)(gAttachmentState.attachId % 10U) - 1 < (PE2_OPTION(LARGE_MIST_LEVEL) - 1)) {
                task->state = PUFF_STATE_SMALL;
                return;
            }
            nextState = PUFF_STATE_LARGE;
            if (effect->step != 0) {
                nextState = PUFF_STATE_SMALL;
            }
            task->state = nextState;
            return;
        case PUFF_STATE_SMALL:
            // Move before testing expiry, including the final update that draws no frame.
            _advanceMistPuff(coord, effect);
            frame         = effect->index + 1;
            effect->index = frame;
            if (frame < PUFF_SMALL_FRAME_LIMIT) {
                _drawMistPuff(coord, (s16)(frame | effect->step), effect->period,
                              effect->scale);
                return;
            }
            effectKillTask(effect, task);
            return;
        case PUFF_STATE_LARGE:
            _advanceMistPuff(coord, effect);
            frame         = effect->index + 1;
            effect->index = frame;
            if (frame < PUFF_LARGE_FRAME_LIMIT) {
                _drawLargeMistPuff(coord, (s16)(frame | effect->step), effect->period,
                                   effect->scale);
                return;
            }
            effectKillTask(effect, task);
            return;
    }
}

/// Draws a small mist puff, a turned camera-facing quad from the 32-texel strip, additive or subtractive by
/// `frameAndBlend`; a point failing to project draws nothing.
static void _drawMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle)
{
    enum { PUFF_TOP_V = 0x18 };
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s32                 uStart;
    s32                 uEnd;

    // Project the narrowed view-space centre and reject GTE projection errors.
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
        scratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        if (frameAndBlend & PUFF_BLEND_ADD_FLAG) {
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(240, 266);
        } else {
            quad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 640, 0);
            quad->clut  = getClut(32, 267);
        }
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        uStart = (frameAndBlend & PUFF_FRAME_MASK) * PUFF_SMALL_CELL_WIDTH;
        uEnd   = uStart + (PUFF_SMALL_CELL_WIDTH - 1);
        setUV4(quad, uStart, PUFF_TOP_V, uEnd, PUFF_TOP_V,
               uStart, PUFF_TOP_V + (PUFF_SMALL_CELL_WIDTH - 1),
               uEnd, PUFF_TOP_V + (PUFF_SMALL_CELL_WIDTH - 1));
        // Scale the rotated corners by projection depth, then queue the raw textured quad.
        _setMistPuffCorners(scratch, quad, size, angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws a large mist puff from the 40-texel strip, as `_drawMistPuff` does.
static void _drawLargeMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle)
{
    enum { PUFF_TOP_V = 0x50 };
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s32                 uStart;
    s32                 uEnd;

    // Project the narrowed view-space centre and reject GTE projection errors.
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
        scratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        if (frameAndBlend & PUFF_BLEND_ADD_FLAG) {
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
            quad->clut  = getClut(240, 266);
        } else {
            quad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 576, 0);
            quad->clut  = getClut(32, 267);
        }
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        uStart = (frameAndBlend & PUFF_FRAME_MASK) * PUFF_LARGE_CELL_WIDTH;
        uEnd   = uStart + (PUFF_LARGE_CELL_WIDTH - 1);
        setUV4(quad, uStart, PUFF_TOP_V, uEnd, PUFF_TOP_V,
               uStart, PUFF_TOP_V + (PUFF_LARGE_CELL_WIDTH - 1),
               uEnd, PUFF_TOP_V + (PUFF_LARGE_CELL_WIDTH - 1));
        // Scale the rotated corners by projection depth, then queue the raw textured quad.
        _setLargeMistPuffCorners(scratch, quad, size, angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
