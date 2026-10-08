#include "pe/necrosis.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Signed texture-frame selector; the caller reduces the effect age modulo six.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Size and travel time of the necrosis cloud at one Parasite Energy level.
///
/// The cast launches a cloud along the caster's facing. The cloud carries a
/// collision sphere keyed with the spell's identity and sheds one trail puff
/// per frame, both of which grow as it travels. The cast selects its row by the
/// level digit of the spell being cast (`AttachmentState::attachId % 10 - 1`),
/// so a higher level launches a larger cloud that travels for longer.
///
/// The radius is in game-coordinate units. The frame count is compared with
/// the cast's `EffectWork::age`, which is 1 on the launch frame and is held
/// while parasite-energy effects are paused for as long as the cloud travels.
typedef struct {
    s16 startRadius;  // Radius the keyed collision sphere is launched with, gaining 0x20 a frame from the launch frame on; a trail puff's sprite size is this plus 0x60 for each frame of the cast's age
    s16 travelFrames; // Frames the cloud keeps its collision spheres for. On the frame after, it sheds its last puff and drops them, and the cast ends 0x10 frames later; the controller vibration that fades out under the cast lasts 0xC frames longer than this
} _NecrosisLevelTuning;
STATIC_ASSERT_SIZEOF(_NecrosisLevelTuning, 4);

/// Collision block of the travelling necrosis cloud, allocated zeroed on the
/// cast's first running frame and kept at `Task::work`.
///
/// Both bodies are spheres centred on the origin of the coordinate the cloud
/// travels on, and both borrow the one contact entry. The damage sphere is what
/// the cloud hurts with: it takes pair tests only, and its key is the damage id
/// of the spell being cast - contact category 2 with bit 0x8000, which selects
/// the attachment level table, plus that spell and level's row number. The
/// grid sphere is what stops the cloud: collision list 7 is walked only by the
/// room-grid pass, and a contact with a class-0 room surface
/// (`WORLD_COLLISION_CONTACT_GRID` alone) zeroes the cloud's velocity and
/// unlinks that sphere, leaving the damage sphere growing in place. Both are
/// unlinked when the travel time runs out or the cast ends early.
typedef struct {
    WorldCollisionBody    damageBody;  // Sphere linked on list 1; pair tests are enabled after the link. Launched with the level's `startRadius` and gains 0x20 each travelling frame
    WorldCollisionBody    gridBody;    // Sphere of radius 0x80 linked on list 7 with a zero key; grid tests are enabled after the link, together with `WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT`, which sphere tests do not read
    WorldCollisionContact contacts[1]; // One-entry table both bodies borrow. The entry is marked LAST; occupied contacts are cleared each travelling frame, and only a grid contact is ever looked for
} _NecrosisWork;
STATIC_ASSERT_SIZEOF(_NecrosisWork, 0x58);

/// Per-level tuning for the necrosis cloud, one row per PE level 1-3, weakest
/// first.
static _NecrosisLevelTuning D_necrosis_801306BC[] = {
    { 0x03C0, 0x000A },
    { 0x0480, 0x000F },
    { 0x0540, 0x0014 },
};

/// The `sndEvtRequestScriptStart` id for each `D_necrosis_801306BC` row.
static s32 D_necrosis_801306C8[] = { 0xE0150001, 0xE0180001, 0xE01B0001 };

/// Packed sprite-frame bits and fixed-point units shared by the mist puff task and drawers.
enum {
    NECROSIS_PUFF_BLEND_ADD_FLAG     = 0x1000, // Set: additive blend and its palette; clear: subtractive blend and its palette
    NECROSIS_PUFF_FRAME_MASK         = 0xF,
    NECROSIS_PUFF_SMALL_CELL_WIDTH   = 32,
    NECROSIS_PUFF_LARGE_CELL_WIDTH   = 40,
    NECROSIS_PUFF_ANGLE_MASK         = 0xFFF, // One turn is 0x1000 angle units
    NECROSIS_PUFF_QUARTER_TURN       = 0x400,
    NECROSIS_PUFF_TRIG_FRACTION_BITS = 12,
};

static void _necrosisDrawMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle);
static void _necrosisDrawLargeMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle);

/// Moves a Necrosis mist puff by its stored displacement and refreshes its view-space transform.
///
/// `effect->move` contains signed integer game-coordinate units per update in
/// the space of `coord->parent`; its trigonometric Q12 conversion is complete.
/// The displacement is added directly to the local translation. The puff's
/// parent is the view coordinate, and its ancestor chain must remain live
/// during composition. Both arguments are borrowed; `effect` is read-only.
static inline void _necrosisAdvanceMistPuff(GfxCoord* coord, const EffectWork* effect)
{
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

/// Writes the screen corners of a rotated 32-texel Necrosis mist puff.
///
/// `scratch` must hold the projected centre and SZ3 / 4 + 1 depth (1..16384).
/// `size` is 0..4095 game-coordinate units; `size * 31 / depth` is the screen
/// half-diagonal in pixels. `cornerAngle` is the bearing of corner 0 in 0x1000
/// units per turn: zero places it above the centre, increasing clockwise.
/// Signed division truncates before the Q12 trigonometric products are shifted.
/// Only the quad's XY fields are written, narrowing each result to 16 bits;
/// `scratch->extent.corner` retains the quarter-turned offset on return.
/// Both objects are borrowed and must remain writable throughout the call.
static inline void _necrosisSetMistPuffCorners(EffectShapeScratch* scratch, POLY_FT4* quad, s16 size, s16 cornerAngle)
{
    s32 perpendicularAngle;

    // Opposite corners share an offset; the other pair lies a quarter turn away.
    scratch->extent.corner.x = (((size * (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rsin(cornerAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rcos(cornerAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    quad->x0                 = scratch->screenX + scratch->extent.corner.x;
    quad->x3                 = scratch->screenX - scratch->extent.corner.x;
    quad->y0                 = scratch->screenY - scratch->extent.corner.y;
    quad->y3                 = scratch->screenY + scratch->extent.corner.y;
    perpendicularAngle       = cornerAngle + NECROSIS_PUFF_QUARTER_TURN;
    scratch->extent.corner.x = (((size * (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rsin(perpendicularAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1)) / scratch->depth) * rcos(perpendicularAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    quad->x1                 = scratch->screenX + scratch->extent.corner.x;
    quad->x2                 = scratch->screenX - scratch->extent.corner.x;
    quad->y1                 = scratch->screenY - scratch->extent.corner.y;
    quad->y2                 = scratch->screenY + scratch->extent.corner.y;
}

/// Writes the screen corners of a rotated 40-texel Necrosis mist puff.
///
/// `scratch` must hold the projected centre and SZ3 / 4 + 1 depth (1..16384).
/// `size` is 0..4095 game-coordinate units; `size * 39 / depth` is the screen
/// half-diagonal in pixels. `cornerAngle` is the bearing of corner 0 in 0x1000
/// units per turn: zero places it above the centre, increasing clockwise.
/// Signed division truncates before the Q12 trigonometric products are shifted.
/// Only the quad's XY fields are written, narrowing each result to 16 bits;
/// `scratch->extent.corner` retains the quarter-turned offset on return.
/// Both objects are borrowed and must remain writable throughout the call.
static inline void _necrosisSetLargeMistPuffCorners(EffectShapeScratch* scratch, POLY_FT4* quad, s16 size, s16 cornerAngle)
{
    s32 perpendicularAngle;

    // Opposite corners share an offset; the other pair lies a quarter turn away.
    scratch->extent.corner.x = (((size * (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rsin(cornerAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rcos(cornerAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    quad->x0                 = scratch->screenX + scratch->extent.corner.x;
    quad->x3                 = scratch->screenX - scratch->extent.corner.x;
    quad->y0                 = scratch->screenY - scratch->extent.corner.y;
    quad->y3                 = scratch->screenY + scratch->extent.corner.y;
    perpendicularAngle       = cornerAngle + NECROSIS_PUFF_QUARTER_TURN;
    scratch->extent.corner.x = (((size * (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rsin(perpendicularAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1)) / scratch->depth) * rcos(perpendicularAngle)) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
    quad->x1                 = scratch->screenX + scratch->extent.corner.x;
    quad->x2                 = scratch->screenX - scratch->extent.corner.x;
    quad->y1                 = scratch->screenY - scratch->extent.corner.y;
    quad->y2                 = scratch->screenY + scratch->extent.corner.y;
}

/// Copies the player's nine Q12 coefficients without changing translation.
///
/// Borrows word-aligned matrices, with `destination` writable. Transfers all
/// 18 coefficient bytes through `GfxRotationWords`, including any scale in
/// the player's basis; preserves the alignment halfword and translation.
/// A whole word-view assignment would also overwrite the alignment halfword.
/// The caller invalidates and composes the coordinate afterward. Retains no
/// pointers and changes no GTE state.
static inline void _necrosisCopyCastRotation(MATRIX* destination, const MATRIX* source)
{
    GfxRotationWords*       destinationRotation = (GfxRotationWords*)destination;
    const GfxRotationWords* sourceRotation      = (const GfxRotationWords*)source;

    destinationRotation->m00M01 = sourceRotation->m00M01;
    destinationRotation->m02M10 = sourceRotation->m02M10;
    destinationRotation->m11M12 = sourceRotation->m11M12;
    destinationRotation->m20M21 = sourceRotation->m20M21;
    destinationRotation->m22    = sourceRotation->m22;
}

/// Applies one velocity step in the parent frame and refreshes the composed coordinate.
///
/// `effect->move` supplies signed 16-bit XYZ distances per tick in the parent
/// coordinate's units; no rotation or fixed-point scaling occurs here. Borrows
/// writable `coord` and read-only `effect`; each translation sum must fit s32.
/// Marks the composition dirty before refreshing it. The parent chain stays
/// live through composition, which clobbers GTE state. Retains no pointers.
static inline void _necrosisAdvanceCastCoord(GfxCoord* coord, const EffectWork* effect)
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
        NECROSIS_MOTOR_START_INTENSITY = 0xFF,
        NECROSIS_MOTOR_END_INTENSITY   = 8,
        NECROSIS_CAST_INITIALIZE       = 0,
        NECROSIS_CAST_TRAVELLING       = 1,
        NECROSIS_CAST_WAIT_FOR_TRAIL   = 2,
        NECROSIS_LAUNCH_SPEED          = 0x90,
        NECROSIS_SPELL_DAMAGE_KEY_BASE = WORLD_COLLISION_CONTACT_ATTACK | 0x8000,
        NECROSIS_GRID_PROBE_RADIUS     = 0x80,
        NECROSIS_VELOCITY_SCALE_Q12    = 0x1100,
        NECROSIS_TRAIL_SIZE_STEP       = 0x60,
        NECROSIS_DAMAGE_RADIUS_STEP    = 0x20,
        NECROSIS_MOTOR_TAIL_FRAMES     = 0xC,
        NECROSIS_TRAIL_WAIT_FRAMES     = 0x10,
    };
    _NecrosisWork*         collision;
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
        case NECROSIS_CAST_INITIALIZE:
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) {
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
            collision = memCalloc(sizeof(_NecrosisWork), 0);
            if (collision == NULL) {
                effect->age = 0;
                return;
            }
            playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            // Borrow only the player's rotation; this cloud retains its own translation.
            _necrosisCopyCastRotation(&coord->coord, &playerCoord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            effect->move.vx = 0;
            effect->move.vy = 0;
            effect->move.vz = NECROSIS_LAUNCH_SPEED;
            gte_SetRotMatrix(&playerCoord->coord);
            gte_ldv0(&effect->move);
            gte_rtv0();
            gte_stsv(&effect->move);
            contacts                               = collision->contacts;
            effect->index                          = (Gp_StateC08.attachId % 10) - 1;
            task->work                             = collision;
            collision->damageBody.coord            = coord;
            collision->damageBody.context.contacts = contacts;
            collision->damageBody.key =
                ((u16)(Gp_StateC08.attachId / 100) - 1) * 9 + ((u16)((u16)(Gp_StateC08.attachId % 100) / 10) - 1) * 3 + (u16)(Gp_StateC08.attachId % 10) + NECROSIS_SPELL_DAMAGE_KEY_BASE;
            collision->damageBody.radius = D_necrosis_801306BC[effect->index].startRadius;
            collision->damageBody.flags  = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &collision->damageBody);
            contacts->flags                      = WORLD_COLLISION_CONTACT_LAST;
            collision->gridBody.coord            = coord;
            collision->gridBody.context.contacts = contacts;
            collision->gridBody.key              = 0;
            collision->gridBody.radius           = NECROSIS_GRID_PROBE_RADIUS;
            collision->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            collision->damageBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_GRID_ONLY, &collision->gridBody);
            collision->gridBody.flags = (collision->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            pan                       = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_necrosis_801306C8[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            padScriptSpawnVariableMotorRamp(D_necrosis_801306BC[effect->index].travelFrames + NECROSIS_MOTOR_TAIL_FRAMES, NECROSIS_MOTOR_START_INTENSITY, NECROSIS_MOTOR_END_INTENSITY);
            task->state = NECROSIS_CAST_TRAVELLING;
            /* fallthrough */
        case NECROSIS_CAST_TRAVELLING:
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                // Accelerate before moving and emitting the growing trail, including launch.
                gte_lddp(NECROSIS_VELOCITY_SCALE_Q12);
                gte_ldsv(&effect->move);
                gte_gpf12();
                gte_stsv(&effect->move);
                _necrosisAdvanceCastCoord(coord, effect);
                spawned = effectSpawn((EFFECT_NECROSIS_TRAIL_PUFF | EFFECT_SPAWN_UNLIMITED), coord,
                                      D_necrosis_801306BC[effect->index].startRadius + (effect->age * NECROSIS_TRAIL_SIZE_STEP),
                                      NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                collision->damageBody.radius = collision->damageBody.radius + NECROSIS_DAMAGE_RADIUS_STEP;
            } else {
                effect->age = effect->age - 1;
            }
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                break;
            }
            if (effect->age > D_necrosis_801306BC[effect->index].travelFrames) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = NECROSIS_CAST_WAIT_FOR_TRAIL;
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
        case NECROSIS_CAST_WAIT_FOR_TRAIL:
            // Bodies are unlinked; the final wait continues aging even while PE is paused.
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) {
                break;
            }
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                break;
            }
            nextAge = (s16)nextAge;
            if ((D_necrosis_801306BC[effect->index].travelFrames + NECROSIS_TRAIL_WAIT_FRAMES) < nextAge) {
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
        NECROSIS_TRAIL_INITIALIZE           = 0,
        NECROSIS_TRAIL_SHRINKING            = 1,
        NECROSIS_TRAIL_SIZE_ANGLE_MASK      = 0xFFF,
        NECROSIS_TRAIL_MIST_SIZE_OFFSET     = 0x100,
        NECROSIS_TRAIL_SHRINK_FRACTION_BITS = 4,
        NECROSIS_TRAIL_TEXTURE_FRAMES       = 6,
        NECROSIS_TRAIL_MIST_INTERVAL        = 3,
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
    if (task->state == NECROSIS_TRAIL_INITIALIZE) {
        effect->scale   = task->spawnArg1.value & NECROSIS_TRAIL_SIZE_ANGLE_MASK;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effect->angle   = (gRandomLcgState >> 16) & NECROSIS_TRAIL_SIZE_ANGLE_MASK;
        effect->period  = effect->scale - NECROSIS_TRAIL_MIST_SIZE_OFFSET;
        effect->step    = effect->scale >> NECROSIS_TRAIL_SHRINK_FRACTION_BITS;
        task->state     = NECROSIS_TRAIL_SHRINKING;
    }
    actorRenderComposeCoord(coord);
    spriteQuadDraw(coord, effect->age % NECROSIS_TRAIL_TEXTURE_FRAMES, effect->scale, effect->angle);
    // Retire before the every-third-age child emission when the final shrink expires.
    effect->scale -= effect->step;
    if (effect->scale < effect->step) {
        effectKillTask(effect, task);
        return;
    }
    if (effect->age % NECROSIS_TRAIL_MIST_INTERVAL == 0) {
        spawned = effectSpawn(EFFECT_NECROSIS_MIST_PUFF, coord, (s32)(effect->period), 0);
        if (spawned != NULL) {
            taskReparent(task, spawned->task);
        }
    }
}

/// Trail-puff palette: VRAM X=240 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(240, 266)
/// Texel width and horizontal stride of each of the six frames selected by the caller.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Inclusive top texel row of the necrosis strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the necrosis sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

void necrosisMistPuffTask(Task* task)
{
    enum {
        NECROSIS_PUFF_STATE_INIT            = 0,
        NECROSIS_PUFF_STATE_SMALL           = 1,
        NECROSIS_PUFF_STATE_LARGE           = 2,
        NECROSIS_PUFF_SMALL_FRAME_LIMIT     = 8, // Frames 1..7 draw; frame 8 moves once more, then releases
        NECROSIS_PUFF_LARGE_FRAME_LIMIT     = 6, // Frames 1..5 draw; frame 6 moves once more, then releases
        NECROSIS_PUFF_LARGE_MIN_LEVEL_INDEX = 2, // Level digit minus one
        NECROSIS_PUFF_BLEND_RANDOM_MASK     = 3, // Two-bit draw compared with the level index
        NECROSIS_PUFF_SIZE_TO_DRIFT_DIVISOR = 20,
        NECROSIS_PUFF_SPAWN_SIZE_MASK       = 0xFFF,
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
        case NECROSIS_PUFF_STATE_INIT:
            // Retain size in period, sprite bearing in scale, and drift magnitude in angle.
            // Divide the stored signed size before narrowing the Q12 displacement components.
            effect->age     = 0;
            spawnSizeBits   = task->spawnArg1.value;
            effect->period  = spawnSizeBits & NECROSIS_PUFF_SPAWN_SIZE_MASK;
            bearingRoll     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            effect->scale   = ((u32)bearingRoll >> 16) & NECROSIS_PUFF_ANGLE_MASK;
            gRandomLcgState = bearingRoll;
            effect->angle   = effect->period / NECROSIS_PUFF_SIZE_TO_DRIFT_DIVISOR;
            effect->move.vx = (rsin(effect->scale) * effect->angle) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
            verticalProduct = rcos(effect->scale) * effect->angle;
            depthRoll       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = depthRoll;
            effect->move.vy = verticalProduct >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
            effect->move.vz = (rsin(((u32)depthRoll >> 16) & NECROSIS_PUFF_ANGLE_MASK) * effect->move.vx) >> NECROSIS_PUFF_TRIG_FRACTION_BITS;
            blendRoll       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = blendRoll;
            // The spawner clears step; only selected puffs switch to additive blending.
            if ((s32)(((u32)blendRoll >> 16) & NECROSIS_PUFF_BLEND_RANDOM_MASK) < ((u16)(Gp_StateC08.attachId % 10U) - 1)) {
                effect->step = NECROSIS_PUFF_BLEND_ADD_FLAG;
            }
            if ((u16)(Gp_StateC08.attachId % 10U) - 1 < NECROSIS_PUFF_LARGE_MIN_LEVEL_INDEX) {
                task->state = NECROSIS_PUFF_STATE_SMALL;
                return;
            }
            nextState = NECROSIS_PUFF_STATE_LARGE;
            if (effect->step != 0) {
                nextState = NECROSIS_PUFF_STATE_SMALL;
            }
            task->state = nextState;
            return;
        case NECROSIS_PUFF_STATE_SMALL:
            // Move before testing expiry, including the final update that draws no frame.
            _necrosisAdvanceMistPuff(coord, effect);
            frame         = effect->index + 1;
            effect->index = frame;
            if (frame < NECROSIS_PUFF_SMALL_FRAME_LIMIT) {
                _necrosisDrawMistPuff(coord, (s16)(frame | effect->step), effect->period,
                                      effect->scale);
                return;
            }
            effectKillTask(effect, task);
            return;
        case NECROSIS_PUFF_STATE_LARGE:
            _necrosisAdvanceMistPuff(coord, effect);
            frame         = effect->index + 1;
            effect->index = frame;
            if (frame < NECROSIS_PUFF_LARGE_FRAME_LIMIT) {
                _necrosisDrawLargeMistPuff(coord, (s16)(frame | effect->step), effect->period,
                                           effect->scale);
                return;
            }
            effectKillTask(effect, task);
            return;
    }
}

/// Draws a 32-texel Necrosis mist cell as a rotated camera-facing quad.
///
/// `coord->workm` must contain the composed view-space transform. Translation
/// components are narrowed to signed 16-bit coordinate units before projection.
/// The caller supplies frames 1..7 in the low nibble of `frameAndBlend` and
/// `NECROSIS_PUFF_BLEND_ADD_FLAG` selects additive blending and its palette;
/// clear selects subtractive blending and its palette. `size` is 0..4095
/// game-coordinate units and `angle` uses 0x1000 units per turn.
/// Requires one available scratch block and space for one GPU `POLY_FT4`.
/// A negative GTE FLAG rejects the quad; otherwise its corner distance is
/// `size * 31 / (SZ3 / 4 + 1)`. Scratch is released before return; the queued
/// packet remains live until GPU drawing completes.
static void _necrosisDrawMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle)
{
    enum { NECROSIS_PUFF_TOP_V = 0x18 };
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
        if (frameAndBlend & NECROSIS_PUFF_BLEND_ADD_FLAG) {
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(240, 266);
        } else {
            quad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 640, 0);
            quad->clut  = getClut(32, 267);
        }
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        uStart = (frameAndBlend & NECROSIS_PUFF_FRAME_MASK) * NECROSIS_PUFF_SMALL_CELL_WIDTH;
        uEnd   = uStart + (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1);
        setUV4(quad, uStart, NECROSIS_PUFF_TOP_V, uEnd, NECROSIS_PUFF_TOP_V,
               uStart, NECROSIS_PUFF_TOP_V + (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1),
               uEnd, NECROSIS_PUFF_TOP_V + (NECROSIS_PUFF_SMALL_CELL_WIDTH - 1));
        // Scale the rotated corners by projection depth, then queue the raw textured quad.
        _necrosisSetMistPuffCorners(scratch, quad, size, angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws a 40-texel Necrosis mist cell as a rotated camera-facing quad.
///
/// `coord->workm` must contain the composed view-space transform. Translation
/// components are narrowed to signed 16-bit coordinate units before projection.
/// The caller supplies frames 1..5 in the low nibble of `frameAndBlend`.
/// `NECROSIS_PUFF_BLEND_ADD_FLAG` selects additive blending and its palette;
/// clear selects subtractive blending and its palette. The task uses this
/// larger strip only for subtractive puffs at PE level 3. `size` is 0..4095
/// game-coordinate units and `angle` uses 0x1000 units per turn.
/// Requires one available scratch block and space for one GPU `POLY_FT4`.
/// A negative GTE FLAG rejects the quad; otherwise its corner distance is
/// `size * 39 / (SZ3 / 4 + 1)`. Scratch is released before return; the queued
/// packet remains live until GPU drawing completes.
static void _necrosisDrawLargeMistPuff(const GfxCoord* coord, s16 frameAndBlend, s16 size, s16 angle)
{
    enum { NECROSIS_PUFF_TOP_V = 0x50 };
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
        if (frameAndBlend & NECROSIS_PUFF_BLEND_ADD_FLAG) {
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
            quad->clut  = getClut(240, 266);
        } else {
            quad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 576, 0);
            quad->clut  = getClut(32, 267);
        }
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        uStart = (frameAndBlend & NECROSIS_PUFF_FRAME_MASK) * NECROSIS_PUFF_LARGE_CELL_WIDTH;
        uEnd   = uStart + (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1);
        setUV4(quad, uStart, NECROSIS_PUFF_TOP_V, uEnd, NECROSIS_PUFF_TOP_V,
               uStart, NECROSIS_PUFF_TOP_V + (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1),
               uEnd, NECROSIS_PUFF_TOP_V + (NECROSIS_PUFF_LARGE_CELL_WIDTH - 1));
        // Scale the rotated corners by projection depth, then queue the raw textured quad.
        _necrosisSetLargeMistPuffCorners(scratch, quad, size, angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
