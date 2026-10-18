#include "weapons/tonfa_baton.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "tonfa_baton_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"

#include "../../shared/effects/blade_trail.h"
#include "gameplay/world_coords.h"

static s16 _gTonfaBatonTrailTint;

/// Divisor that turns the model's forward axis into the distance a baton
/// strike carries the actor per frame: 4096 / 84, about 48 coordinate units.
enum { TONFA_BATON_ATTACK_ADVANCE_DIVISOR = 84 };

/// One-based weapon index passed to the resident use counter.
enum { TONFA_BATON_WEAPON_ID = 19 };

/// Scratch-stack block for the tonfa baton's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages the forward step a strike
/// moves the actor by.
///
/// `forward` is read from the root coordinate's local matrix and is not
/// normalized. `advance` is each component of it divided by
/// `TONFA_BATON_ATTACK_ADVANCE_DIVISOR` and multiplied by 1 on a frame a
/// strike carries the actor forward, 0 on any other. It is added to the root
/// coordinate's local translation in every state. Neither vector's `pad` is
/// written.
typedef struct {
    VECTOR  advance; // This frame's displacement of the model's root coordinate, in coordinate units; zero while no strike carries the actor
    SVECTOR forward; // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _TonfaBatonAttackScratch;
STATIC_ASSERT_SIZEOF(_TonfaBatonAttackScratch, 0x18);

enum {
    TONFA_BATON_TRAIL_ENDPOINT_BASE  = 0,
    TONFA_BATON_TRAIL_ENDPOINT_TIP   = 1,
    TONFA_BATON_TRAIL_ENDPOINT_COUNT = 2
};

/// Base and tip translations for the baton swing ribbon, in integer game coordinates.
///
/// The base is (0, 128, 0) in the weapon frame. Seeding chains the tip's
/// (0, -512, 0) translation below that base; subsequent samples apply it in
/// the weapon frame directly. Only XYZ are read; each vector retains its pad.
static SVECTOR _gTonfaBatonTrailEndpointOffsets[TONFA_BATON_TRAIL_ENDPOINT_COUNT] = {
    { 0, 0x0080, 0, 0 },
    { 0, -0x0200, 0, 0 },
};

// Pose requests shared by the argument reset and the angle step.
enum {
    TONFA_BATON_POSE_REST   = 0,
    TONFA_BATON_POSE_STRIKE = 1
};

static void _tonfaBatonKillModelTask(Task* task);

/// Stores a composed baton endpoint as a stationary world-space trail pose.
///
/// `composedEndpoint->workm.t` must be a current view-space position, and
/// `gGfxViewCoord.workm` the matching world-to-view transform with an
/// orthonormal rotation. Copies the complete 32-byte endpoint cache into
/// `historyFrame->workm` and removes the view transform into
/// `historyFrame->coord`. Rotation is copied and rebased too, although the
/// ribbon consumes only translation. Rotation elements use `ONE` (4096)
/// for 1.0; translations are signed 32-bit game coordinates.
///
/// The caller-owned nodes and the view must be live, word-aligned and
/// disjoint. Reads only the endpoint's composed cache and retains no pointer
/// to it, so a temporary endpoint may expire after the call. The saved parent
/// is the persistent view node: later composition follows camera movement
/// while preserving the sampled world pose. Leaves the destination's local
/// matrix alignment bytes, composition stamp and parameters untouched; the
/// caller must mark it dirty before recomposing.
///
/// Requires an initialized scratch stack with 48 free bytes disjoint from
/// the nodes and view, released before return. Loads the endpoint's rotation
/// and translation into the GTE before rebasing; changes GTE rotation,
/// translation and arithmetic state.
static inline void _tonfaBatonStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* composedEndpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = composedEndpoint->workm;
    gte_SetRotMatrix(&composedEndpoint->workm);
    gte_SetTransMatrix(&composedEndpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

void tonfaBatonSwingTrailTask(Task* task)
{
    enum {
        TONFA_BATON_TRAIL_SEED           = 0,
        TONFA_BATON_TRAIL_RECORD         = 1,
        TONFA_BATON_TRAIL_LIFETIME_TICKS = 31,
        TONFA_BATON_TRAIL_TINT_BLUE      = 0x001, // RGB multipliers (0, 0, 1)
        TONFA_BATON_TRAIL_TINT_CYAN      = 0x013  // RGB multipliers (0, 1, 3)
    };
    GfxCoord       tipEndpoint;
    GfxCoord*      baseCoord;
    GfxCoord*      historyFrame;
    EffectWork*    effectWork;
    const SVECTOR* seedTipOffset;
    const SVECTOR* recordTipOffset;
    s32            historyIndex;
    s32            packedTint;

    effectWork = task->spawnArg2.pointer;
    baseCoord  = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effectWork, task);
        }
    } else {
        effectWork->age++;
        switch (task->state) {
            case TONFA_BATON_TRAIL_SEED:
                baseCoord->parent       = effectWork->parent;
                baseCoord->coord.t[0]   = _gTonfaBatonTrailEndpointOffsets[TONFA_BATON_TRAIL_ENDPOINT_BASE].vx;
                baseCoord->coord.t[1]   = _gTonfaBatonTrailEndpointOffsets[TONFA_BATON_TRAIL_ENDPOINT_BASE].vy;
                baseCoord->coord.t[2]   = _gTonfaBatonTrailEndpointOffsets[TONFA_BATON_TRAIL_ENDPOINT_BASE].vz;
                baseCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(baseCoord);
                task->state = TONFA_BATON_TRAIL_RECORD;

                // The seed chains the tip below the base; later samples use the weapon parent.
                // Only translations feed the ribbon; the temporary rotation is left uninitialized.
                seedTipOffset            = &_gTonfaBatonTrailEndpointOffsets[TONFA_BATON_TRAIL_ENDPOINT_TIP];
                tipEndpoint.parent       = baseCoord;
                tipEndpoint.coord.t[0]   = seedTipOffset->vx;
                tipEndpoint.coord.t[1]   = seedTipOffset->vy;
                tipEndpoint.coord.t[2]   = seedTipOffset->vz;
                tipEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&tipEndpoint);

                // Seed every slot with the same pose, detached from the moving baton.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(gBladeTrailBase); historyIndex++) {
                    historyFrame = &gBladeTrailBase[historyIndex];
                    _tonfaBatonStoreTrailFrame(historyFrame, baseCoord);
                    historyFrame = &gBladeTrailTip[historyIndex];
                    _tonfaBatonStoreTrailFrame(historyFrame, &tipEndpoint);
                }
                packedTint = TONFA_BATON_TRAIL_TINT_CYAN;
                if (task->spawnArg1.value == 0) {
                    packedTint = TONFA_BATON_TRAIL_TINT_BLUE;
                }
                _gTonfaBatonTrailTint = packedTint;
                break;
            case TONFA_BATON_TRAIL_RECORD:
                baseCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(baseCoord);
                tipEndpoint.parent       = effectWork->parent;
                recordTipOffset          = &_gTonfaBatonTrailEndpointOffsets[TONFA_BATON_TRAIL_ENDPOINT_TIP];
                tipEndpoint.coord.t[0]   = recordTipOffset->vx;
                tipEndpoint.coord.t[1]   = recordTipOffset->vy;
                tipEndpoint.coord.t[2]   = recordTipOffset->vz;
                tipEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&tipEndpoint);
                historyFrame = &gBladeTrailBase[effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1)];
                _tonfaBatonStoreTrailFrame(historyFrame, baseCoord);
                historyFrame = &gBladeTrailTip[effectWork->age & (ARRAY_SIZE(gBladeTrailTip) - 1)];
                _tonfaBatonStoreTrailFrame(historyFrame, &tipEndpoint);

                // Recompose retained world poses against the current camera before drawing.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(gBladeTrailBase); historyIndex++) {
                    historyFrame               = &gBladeTrailBase[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &gBladeTrailTip[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1), _gTonfaBatonTrailTint);
                break;
        }
        if (effectWork->age >= TONFA_BATON_TRAIL_LIFETIME_TICKS) {
            effectKillTask(effectWork, task);
        }
    }
}

/// Binds the baton's base-endpoint ring to the first side of the swing ribbon.
///
/// Supplies a side-effect-free `GfxCoord[8]` array identifier, paired by slot
/// with `BLADE_TRAIL_SECOND_ENDPOINT_HISTORY`. The drawer borrows composed view-space
/// `workm.t` translations in game-coordinate units for corners 0 and 2;
/// the task seeds every slot and refreshes the caches before drawing.
/// This binding exists only around the shared drawer inclusion.
#define BLADE_TRAIL_FIRST_ENDPOINT_HISTORY gBladeTrailBase

/// Binds the baton's tip-endpoint ring to the second side of the swing ribbon.
///
/// Supplies a side-effect-free, complete `GfxCoord[8]` array identifier, paired
/// by slot with `BLADE_TRAIL_FIRST_ENDPOINT_HISTORY`. Seed every slot and
/// recompose in the current view before drawing. The drawer borrows `workm.t`
/// translations in game-coordinate units for corners 1 and 3, narrowing them
/// to signed 16 bits and retaining no pointers. This binding exists only
/// around the shared drawer inclusion.
#define BLADE_TRAIL_SECOND_ENDPOINT_HISTORY gBladeTrailTip
#include "../../shared/effects/blade_trail_draw.inc.c"
#undef BLADE_TRAIL_FIRST_ENDPOINT_HISTORY
#undef BLADE_TRAIL_SECOND_ENDPOINT_HISTORY

/// Starts the attached baton model's pose updates and installs default teardown.
///
/// State 0 requires a live TMD body with an initialized root coordinate whose
/// parent was supplied by the spawner. Clears the model flags on this first
/// dispatch; the next dispatch mirrors the player's flags.
static void _tonfaBatonInitModelTask(Task* task)
{
    TmdObject* model;
    GfxCoord*  rootCoord;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    task->state++;
    task->exitCallback      = _tonfaBatonKillModelTask;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags            = 0;
}

/// Steps the attached baton's rest/strike Z rotation in its parent's frame.
///
/// `rootCoord` must be live and writable with an initialized signed
/// `param.rot.vz` angle, in 4096 units per turn. `poseRequest` is the caller's
/// low-nibble request: 0 rest, 1 strike, 2..15 hold the stored angle.
/// Rest subtracts 256 only while the angle is positive; strike adds 448 only
/// while it is below 2048. The tests precede the steps, so a crossed target
/// stays overshot until another request moves the angle again.
///
/// Always replaces the local 3x3 rotation, including on a hold request, and
/// preserves translation. Other stored Euler components are unused here.
/// The caller owns the coordinate and must invalidate its composition stamp
/// and compose it before using the cached matrix. Requires the initialized
/// scratch stack used by `gfxRotMatrixZ`; retains no pointer.
static inline void _tonfaBatonStepModelPose(GfxCoord* rootCoord, s32 poseRequest)
{
    enum {
        TONFA_BATON_POSE_RETURN_STEP = 0x100,
        TONFA_BATON_POSE_STRIKE_STEP = 0x1C0
    };

    switch (poseRequest) {
        case TONFA_BATON_POSE_REST:
            if (rootCoord->param.rot.vz > 0) {
                rootCoord->param.rot.vz -= TONFA_BATON_POSE_RETURN_STEP;
            }
            break;
        case TONFA_BATON_POSE_STRIKE:
            if (rootCoord->param.rot.vz < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                rootCoord->param.rot.vz += TONFA_BATON_POSE_STRIKE_STEP;
            }
            break;
    }
    // A held angle still replaces the rotation; translation stays at the grip.
    gfxRotMatrixZ(&rootCoord->coord, rootCoord->param.rot.vz, GRAPHICS_ROTATION_REPLACE);
}

/// Updates the baton's grip offset, player display flags and requested strike pose.
///
/// State 1 requires live player task/work/model storage and the baton's TMD
/// root coordinate. The low nibble of `spawnArg1.value` requests the pose
/// (0 rest, 1 strike, others hold); leaving the player's normal-mode attack
/// state clears the entire argument and starts returning to rest.
static void _tonfaBatonUpdateModelPose(Task* task)
{
    enum {
        TONFA_BATON_PLAYER_ATTACK_STATE = 4,
        TONFA_BATON_POSE_MODE_MASK      = 0xF,
        TONFA_BATON_GRIP_OFFSET_Y       = 0x60
    };

    TmdObject* model;
    GfxCoord*  rootCoord;
    GameActor* playerActor;
    s32        poseMode;

    model                   = task->extra.tmd;
    rootCoord               = model->coords;
    playerActor             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->flags;

    rootCoord->coord.t[0] = 0;
    rootCoord->coord.t[1] = TONFA_BATON_GRIP_OFFSET_Y;
    rootCoord->coord.t[2] = 0;

    if (playerActor->mode != GAME_ACTOR_MODE_NORMAL || playerActor->state != TONFA_BATON_PLAYER_ATTACK_STATE) {
        task->spawnArg1.value = TONFA_BATON_POSE_REST;
    }

    poseMode = task->spawnArg1.value & TONFA_BATON_POSE_MODE_MASK;
    _tonfaBatonStepModelPose(rootCoord, poseMode);
}

/// Advances state 2 to teardown on the next model-task dispatch.
static void _tonfaBatonDeferModelTaskRemoval(Task* task)
{
    enum { TONFA_BATON_MODEL_STATE_KILL = 3 };

    task->state = TONFA_BATON_MODEL_STATE_KILL;
}

/// Hands the live baton model task to default teardown, as state 3 or on exit.
static void _tonfaBatonKillModelTask(Task* task)
{
    taskKill(task);
}

void tonfaBatonModelTask(Task* task)
{
    TaskFunc stateHandlers[] = {
        _tonfaBatonInitModelTask,
        _tonfaBatonUpdateModelPose,
        _tonfaBatonDeferModelTaskRemoval,
        _tonfaBatonKillModelTask,
    };

    stateHandlers[task->state](task);
}

/// Adds this strike tick's local forward displacement to the player's root.
///
/// Reads the root's unnormalized Z column (4096 per unit), divides each signed
/// component by 84 and narrows the quotient to s16 before multiplying by
/// `advanceThisFrame`, which must be 0 or 1. Translations stay in the parent's
/// game-coordinate units. Borrows a live writable scratch block disjoint from
/// the root; writes XYZ of `forward` and `advance`, leaving their pad fields
/// and the root's composition stamp untouched. Retains no pointer.
static inline void _tonfaBatonAdvanceAttack(GfxCoord* rootCoord, _TonfaBatonAttackScratch* scratch, s32 advanceThisFrame)
{
    gfxReadMatrixZAxis(&rootCoord->coord, &scratch->forward);
    scratch->advance.vx    = (s16)(scratch->forward.vx / TONFA_BATON_ATTACK_ADVANCE_DIVISOR) * advanceThisFrame;
    scratch->advance.vy    = (s16)(scratch->forward.vy / TONFA_BATON_ATTACK_ADVANCE_DIVISOR) * advanceThisFrame;
    scratch->advance.vz    = (s16)(scratch->forward.vz / TONFA_BATON_ATTACK_ADVANCE_DIVISOR) * advanceThisFrame;
    rootCoord->coord.t[0] += scratch->advance.vx;
    rootCoord->coord.t[1] += scratch->advance.vy;
    rootCoord->coord.t[2] += scratch->advance.vz;
}

void tonfaBatonAttackState(Task* playerTask)
{
    enum {
        TONFA_BATON_PHASE_PREPARE             = 0,
        TONFA_BATON_PHASE_FIRST_STRIKE        = 1,
        TONFA_BATON_PHASE_COMBO_QUEUED        = 2,
        TONFA_BATON_PHASE_COMBO_STRIKE        = 3,
        TONFA_BATON_PHASE_COMBO_RECOVER       = 4,
        TONFA_BATON_PHASE_RECOVER             = 5,
        TONFA_BATON_PLAYER_ATTACK_STATE       = 4,
        TONFA_BATON_ANIMATION_PRIMARY         = 0xA,
        TONFA_BATON_ANIMATION_SECONDARY       = 0xB,
        TONFA_BATON_ANIMATION_RECOVER         = 0xE,
        TONFA_BATON_FIRST_STRIKE_KEY          = 0x21317,
        TONFA_BATON_COMBO_STRIKE_KEY          = 0x21315,
        TONFA_BATON_WINDUP_FRAMES             = 8,
        TONFA_BATON_COMBO_ADVANCE_FRAMES      = 0xC,
        TONFA_BATON_RECOVERY_ADVANCE_FRAMES   = 0xA,
        TONFA_BATON_COMBO_CONTACT_FRAMES      = 9,
        TONFA_BATON_COMBO_CONTACT_START_TICKS = 3,
        TONFA_BATON_FIRST_SWING_SOUND         = SOUND_WEAPON(TONFA_BATON_WEAPON_ID, 1),
        TONFA_BATON_COMBO_SWING_SOUND         = SOUND_WEAPON(TONFA_BATON_WEAPON_ID, 2),
        TONFA_BATON_FIRST_CONTACT_SOUND       = SOUND_WEAPON(TONFA_BATON_WEAPON_ID, 3),
        TONFA_BATON_COMBO_CONTACT_SOUND       = SOUND_WEAPON(TONFA_BATON_WEAPON_ID, 4),
    };
    GameActor*                actor;
    GfxCoord*                 rootCoord;
    _TonfaBatonAttackScratch* scratch;
    EffectWork*               trailWork;
    s32                       windupTicksLeft;
    s32                       comboTicksLeft;
    s32                       contactTicksLeft;
    s32                       advanceThisFrame;

    advanceThisFrame = 0;
    actor            = playerTask->work;
    scratch          = SCRATCH_STACK_RESERVE_BLOCK(_TonfaBatonAttackScratch);
    switch (actor->statePhase) {
        case TONFA_BATON_PHASE_PREPARE:
            actor->state          = TONFA_BATON_PLAYER_ATTACK_STATE;
            actor->statePhase     = TONFA_BATON_PHASE_FIRST_STRIKE;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->stateTimer     = TONFA_BATON_WINDUP_FRAMES;
            actor->actionValue    = 0;
            weaponRecordUse(TONFA_BATON_WEAPON_ID);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key   = TONFA_BATON_FIRST_STRIKE_KEY;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT)) | WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            playerActorPlayChildSlotsWithBlend(playerTask, TONFA_BATON_ANIMATION_PRIMARY, 1, 3);
            break;
        case TONFA_BATON_PHASE_FIRST_STRIKE:
        case TONFA_BATON_PHASE_COMBO_QUEUED:
            windupTicksLeft = actor->stateTimer;
            if (windupTicksLeft == 0) {
                actor->movementSign = 1;
                advanceThisFrame    = 1;
                if (actor->padPressed & (PAD_BUTTON_R1 | PAD_BUTTON_R2)) {
                    actor->statePhase = TONFA_BATON_PHASE_COMBO_QUEUED;
                }
            } else {
                windupTicksLeft--;
                actor->stateTimer = windupTicksLeft;
                if (windupTicksLeft == 0) {
                    actor->equipmentTasks[1]->spawnArg1.value             = TONFA_BATON_POSE_STRIKE;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, TONFA_BATON_FIRST_SWING_SOUND, 0);
                    trailWork = effectSpawn(EFFECT_TONFA_BATON_SWING_TRAIL,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            0, NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                }
            }
            if (actor->actionValue != 1 && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = 1;
                worldCoordPlaySound(playerTask->extra.tmd->coords, TONFA_BATON_FIRST_CONTACT_SOUND, 0);
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, 1, 0, 0) == 0) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (actor->statePhase == TONFA_BATON_PHASE_COMBO_QUEUED) {
                    actor->statePhase = TONFA_BATON_PHASE_COMBO_STRIKE;
                    actor->stateTimer = TONFA_BATON_COMBO_ADVANCE_FRAMES;
                    weaponRecordUse(TONFA_BATON_WEAPON_ID);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = TONFA_BATON_COMBO_STRIKE_KEY;
                    trailWork                                          = effectSpawn(
                        EFFECT_TONFA_BATON_SWING_TRAIL, actor->equipmentTasks[1]->extra.tmd->coords, 1,
                        NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                    playerActorResetChildSlots(playerTask, TONFA_BATON_ANIMATION_SECONDARY);
                } else {
                    actor->statePhase                         = TONFA_BATON_PHASE_RECOVER;
                    actor->stateTimer                         = TONFA_BATON_RECOVERY_ADVANCE_FRAMES;
                    actor->equipmentTasks[1]->spawnArg1.value = TONFA_BATON_POSE_REST;
                    playerActorResetChildSlots(playerTask, TONFA_BATON_ANIMATION_RECOVER);
                }
            }
            break;
        case TONFA_BATON_PHASE_COMBO_STRIKE:
            if (actor->stateTimer != 0) {
                actor->movementSign = 1;
                advanceThisFrame    = 1;
                comboTicksLeft      = actor->stateTimer - 1;
                actor->stateTimer   = comboTicksLeft;
                if (comboTicksLeft == TONFA_BATON_COMBO_CONTACT_START_TICKS) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, TONFA_BATON_COMBO_SWING_SOUND, 0);
                } else if (comboTicksLeft == 0) {
                    actor->statePhase                         = TONFA_BATON_PHASE_COMBO_RECOVER;
                    actor->stateTimer                         = TONFA_BATON_COMBO_CONTACT_FRAMES;
                    actor->equipmentTasks[1]->spawnArg1.value = TONFA_BATON_POSE_REST;
                }
            }
            /* fallthrough */
        case TONFA_BATON_PHASE_COMBO_RECOVER:
            if (actor->statePhase == TONFA_BATON_PHASE_COMBO_RECOVER) {
                contactTicksLeft = actor->stateTimer;
                contactTicksLeft--;
                actor->stateTimer = contactTicksLeft;
                if (contactTicksLeft == 0) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                }
            }
            if (actor->actionValue != 2 && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = 2;
                worldCoordPlaySound(playerTask->extra.tmd->coords, TONFA_BATON_COMBO_CONTACT_SOUND, 0);
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, 1, 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
        case TONFA_BATON_PHASE_RECOVER:
            if (actor->stateTimer != 0) {
                actor->movementSign = 1;
                advanceThisFrame    = 1;
                actor->stateTimer   = actor->stateTimer - 1;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, 1, 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    // Active strike and recovery phases advance along the model's local forward axis.
    rootCoord = playerTask->extra.tmd->coords;
    _tonfaBatonAdvanceAttack(rootCoord, scratch, advanceThisFrame);
    SCRATCH_STACK_RELEASE_BLOCK(_TonfaBatonAttackScratch);
}

/// The model `gTonfaBatonWeaponModel`: generated from its asset (weapons/tonfa_baton/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/tonfa_baton/model.inc"

/// The model `gTonfaBatonShaftModel`: generated from its asset (weapons/tonfa_baton/shaft_model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/tonfa_baton/shaft_model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/tonfa_baton/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/tonfa_baton/clips.inc"
#include "assets/weapons/tonfa_baton/bank.inc"

GfxCoord gBladeTrailBase[8] = { 0 };
GfxCoord gBladeTrailTip[8]  = { 0 };

/// Packed RGB multipliers retained for the tonfa baton's swing ribbon.
///
/// Red occupies bits 8..9, green 4..5 and blue 0..1; all other bits are zero.
/// Initially zero when the overlay loads. Seeding selects 0x001 (blue:
/// 0, 0, 1) when `Task::spawnArg1.value` is zero or 0x013 (cyan: 0, 1, 3)
/// otherwise. The tint persists between dispatches and is shared by
/// simultaneous trail tasks until another seed replaces it.
static s16 _gTonfaBatonTrailTint = 0;
