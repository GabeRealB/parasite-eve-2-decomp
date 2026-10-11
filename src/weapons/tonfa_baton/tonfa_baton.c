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

#include "../../shared/blade_trail.h"
#include "gameplay/animation.h"
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

#define BLADE_TRAIL_BASE_HISTORY gBladeTrailBase
#define BLADE_TRAIL_TIP_HISTORY  gBladeTrailTip
#include "../../shared/blade_trail_draw.inc.c"
#undef BLADE_TRAIL_BASE_HISTORY
#undef BLADE_TRAIL_TIP_HISTORY

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

enum {
    TONFA_BATON_WEAPON_MODEL_QUAD_COUNT = 16,
    TONFA_BATON_SHAFT_MODEL_QUAD_COUNT  = 6
};

/// Initial identity transform and self-parented root for the weapon model.
static TmdBone _gTonfaBatonWeaponModelSkeleton[1] = {
#include "assets/tonfa_baton_model_010C8_skeleton.inc"
};

/// Vertex count of the model's sole part: 24.
static u32 _gTonfaBatonWeaponModelPartVerts[ARRAY_SIZE(_gTonfaBatonWeaponModelSkeleton)] = {
#include "assets/tonfa_baton_model_010C8_partVerts.inc"
};

/// Writable weapon vertices in integer part-local coordinates.
static SVECTOR _gTonfaBatonWeaponModelVerts[24] = {
#include "assets/tonfa_baton_model_010C8_verts.inc"
};

/// Writable lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gTonfaBatonWeaponModelNormals[20] = {
#include "assets/tonfa_baton_model_010C8_normals.inc"
};

/// Writable commands for 16 gouraud textured quads; creation resolves their handler slot.
///
/// One skeletal group is followed by an empty final group and `TMD_STREAM_END`.
static u32 _gTonfaBatonWeaponModelStream[118] = {
#include "assets/tonfa_baton_model_010C8_stream.inc"
};

TmdSource gTonfaBatonWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = TONFA_BATON_WEAPON_MODEL_QUAD_COUNT * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gTonfaBatonWeaponModelSkeleton),
    .partVertexCounts    = _gTonfaBatonWeaponModelPartVerts,
    .verts               = _gTonfaBatonWeaponModelVerts,
    .normals             = _gTonfaBatonWeaponModelNormals,
    .skeleton            = _gTonfaBatonWeaponModelSkeleton,
    .stream              = _gTonfaBatonWeaponModelStream,
};

/// Initial identity transform and self-parented root for the shaft model.
static TmdBone _gTonfaBatonShaftModelSkeleton[1] = {
#include "assets/tonfa_baton_model_0136C_skeleton.inc"
};

/// Vertex count of the model's sole part: 8.
static u32 _gTonfaBatonShaftModelPartVerts[ARRAY_SIZE(_gTonfaBatonShaftModelSkeleton)] = {
#include "assets/tonfa_baton_model_0136C_partVerts.inc"
};

/// Writable shaft vertices in integer part-local coordinates.
static SVECTOR _gTonfaBatonShaftModelVerts[8] = {
#include "assets/tonfa_baton_model_0136C_verts.inc"
};

/// Writable lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gTonfaBatonShaftModelNormals[8] = {
#include "assets/tonfa_baton_model_0136C_normals.inc"
};

/// Writable commands for 6 gouraud textured quads; creation resolves their handler slot.
///
/// One skeletal group is followed by an empty final group and `TMD_STREAM_END`.
static u32 _gTonfaBatonShaftModelStream[48] = {
#include "assets/tonfa_baton_model_0136C_stream.inc"
};

TmdSource gTonfaBatonShaftModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = TONFA_BATON_SHAFT_MODEL_QUAD_COUNT * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gTonfaBatonShaftModelSkeleton),
    .partVertexCounts    = _gTonfaBatonShaftModelPartVerts,
    .verts               = _gTonfaBatonShaftModelVerts,
    .normals             = _gTonfaBatonShaftModelNormals,
    .skeleton            = _gTonfaBatonShaftModelSkeleton,
    .stream              = _gTonfaBatonShaftModelStream,
};

/// Encoding-1 poses: integer local XYZ and Euler angles in 1/4096 turns.
///
/// Each pose spans three four-byte words addressed by the animation records.
static AnimationPackedPose _gTonfaBatonIdleAnimationBank1[2] = {
#include "assets/tonfa_baton_animation_015E0_bank1.inc"
};

/// Encoding-4 Euler rotations, eight angle units per packed step (4096 per turn).
static AnimationPackedRotation _gTonfaBatonIdleAnimationBank4[8] = {
#include "assets/tonfa_baton_animation_015E0_bank4.inc"
};

/// Whole-frame keyframe durations and absolute record-index jumps for 19 tracks.
static AnimationRecord _gTonfaBatonIdleAnimationRecords[76] = {
#include "assets/tonfa_baton_animation_015E0_records.inc"
};

/// Record starts for model-part tracks 0..18; the final halfword is retained serialized data.
static u16 _gTonfaBatonIdleAnimationIndices[20] = {
#include "assets/tonfa_baton_animation_015E0_indices.inc"
};

/// Standing idle clip for the player while the tonfa baton bank is installed.
///
/// Native bank slots 1, 21, 22 and 41. Loops the same standing pose on all 19 model-part tracks.
/// Borrows read-only, word-aligned pose banks and records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonIdleAnimation = {
    .records           = _gTonfaBatonIdleAnimationRecords,
    .trackStartIndices = _gTonfaBatonIdleAnimationIndices,
    .poseBanks         = { NULL, _gTonfaBatonIdleAnimationBank1, NULL, NULL, _gTonfaBatonIdleAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 poses: integer local XYZ and Euler angles in 1/4096 turns.
///
/// Each pose spans three four-byte words addressed by the animation records.
static AnimationPackedPose _gTonfaBatonWalkAnimationBank1[12] = {
#include "assets/tonfa_baton_animation_01C84_bank1.inc"
};

/// Encoding-4 Euler rotations, eight angle units per packed step (4096 per turn).
static AnimationPackedRotation _gTonfaBatonWalkAnimationBank4[151] = {
#include "assets/tonfa_baton_animation_01C84_bank4.inc"
};

/// Whole-frame keyframe durations and absolute record-index jumps for 19 tracks.
static AnimationRecord _gTonfaBatonWalkAnimationRecords[218] = {
#include "assets/tonfa_baton_animation_01C84_records.inc"
};

/// Record starts for model-part tracks 0..18; the final halfword is retained serialized data.
static u16 _gTonfaBatonWalkAnimationIndices[20] = {
#include "assets/tonfa_baton_animation_01C84_indices.inc"
};

/// Forward-walk clip for the player while the tonfa baton bank is installed.
///
/// Native bank slots 2 and 19. Used for forward walking with or without an attached weapon model.
/// Borrows read-only, word-aligned pose banks and records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonWalkAnimation = {
    .records           = _gTonfaBatonWalkAnimationRecords,
    .trackStartIndices = _gTonfaBatonWalkAnimationIndices,
    .poseBanks         = { NULL, _gTonfaBatonWalkAnimationBank1, NULL, NULL, _gTonfaBatonWalkAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 poses: integer local XYZ and Euler angles in 1/4096 turns.
///
/// Each pose spans three four-byte words addressed by the animation records.
static AnimationPackedPose _gTonfaBatonTurnNegativeAnimationBank1[19] = {
#include "assets/tonfa_baton_animation_024E4_bank1.inc"
};

/// Encoding-4 Euler rotations, eight angle units per packed step (4096 per turn).
static AnimationPackedRotation _gTonfaBatonTurnNegativeAnimationBank4[169] = {
#include "assets/tonfa_baton_animation_024E4_bank4.inc"
};

/// Whole-frame keyframe durations and absolute record-index jumps for 19 tracks.
static AnimationRecord _gTonfaBatonTurnNegativeAnimationRecords[290] = {
#include "assets/tonfa_baton_animation_024E4_records.inc"
};

/// Record starts for model-part tracks 0..18; the final halfword is retained serialized data.
static u16 _gTonfaBatonTurnNegativeAnimationIndices[20] = {
#include "assets/tonfa_baton_animation_024E4_indices.inc"
};

/// Negative-direction stationary-turn clip for the player while the tonfa baton bank is installed.
///
/// Native bank slot 5 selects this when stationary turning has a negative sign.
/// Borrows read-only, word-aligned pose banks and records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonTurnNegativeAnimation = {
    .records           = _gTonfaBatonTurnNegativeAnimationRecords,
    .trackStartIndices = _gTonfaBatonTurnNegativeAnimationIndices,
    .poseBanks         = { NULL, _gTonfaBatonTurnNegativeAnimationBank1, NULL, NULL, _gTonfaBatonTurnNegativeAnimationBank4, NULL, NULL, NULL },
};

/// Encoding indices for the pose banks of these tonfa baton player clips.
enum {
    TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Translation/rotation poses for `_gTonfaBatonTurnPositiveAnimation`.
///
/// XYZ use integer model coordinates; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gTonfaBatonTurnPositiveAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_02D48_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonTurnPositiveAnimation`, in 1/512-turn steps.
static const AnimationPackedRotation _gTonfaBatonTurnPositiveAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_02D48_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonTurnPositiveAnimation`.
///
/// Durations count normal-rate frames; control jumps use absolute record indices.
static const AnimationRecord _gTonfaBatonTurnPositiveAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_02D48_records.inc"
};

/// Track-start record indices for `_gTonfaBatonTurnPositiveAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonTurnPositiveAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_02D48_indices.inc"
};

/// Looping positive-direction stationary turn for the player in the tonfa baton bank.
///
/// Native bank slot 6; scripted turning also selects it for a nonnegative yaw delta.
/// Supplies 19 model-part tracks. Borrows read-only, word-aligned pose banks and
/// records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonTurnPositiveAnimation = {
    .records           = _gTonfaBatonTurnPositiveAnimationRecords,
    .trackStartIndices = _gTonfaBatonTurnPositiveAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonTurnPositiveAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonTurnPositiveAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonItemUseAnimation`.
///
/// XYZ use integer model coordinates; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gTonfaBatonItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_0305C_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonItemUseAnimation`, in 1/512-turn steps.
static const AnimationPackedRotation _gTonfaBatonItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_0305C_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonItemUseAnimation`.
///
/// Durations count normal-rate frames; stop controls hold the final pose.
static const AnimationRecord _gTonfaBatonItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_0305C_records.inc"
};

/// Track-start record indices for `_gTonfaBatonItemUseAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_0305C_indices.inc"
};

/// Player item-use gesture in the tonfa baton bank.
///
/// Native bank slot 40 supplies 19 model-part tracks that stop on their final pose.
/// Borrows read-only, word-aligned pose banks and records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonItemUseAnimation = {
    .records           = _gTonfaBatonItemUseAnimationRecords,
    .trackStartIndices = _gTonfaBatonItemUseAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonItemUseAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonItemUseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonStairAscentAnimation`.
///
/// XYZ use integer model coordinates; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gTonfaBatonStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_037B8_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonStairAscentAnimation`, in 1/512-turn steps.
static const AnimationPackedRotation _gTonfaBatonStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_037B8_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonStairAscentAnimation`.
///
/// Durations count normal-rate frames; track 1 carries the ascent footstep cues.
/// Control jumps use absolute record indices to repeat each track.
static const AnimationRecord _gTonfaBatonStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_037B8_records.inc"
};

/// Track-start record indices for `_gTonfaBatonStairAscentAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_037B8_indices.inc"
};

/// Looping player stair-ascent clip in the tonfa baton bank.
///
/// Native bank slot 36 supplies 19 model-part tracks. Track 1's footstep cues
/// pace ascent movement. Borrows read-only, word-aligned pose banks and records
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonStairAscentAnimation = {
    .records           = _gTonfaBatonStairAscentAnimationRecords,
    .trackStartIndices = _gTonfaBatonStairAscentAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonStairAscentAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonStairAscentAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonStairDescentAnimation`.
///
/// XYZ use integer model coordinates; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gTonfaBatonStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_03F40_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonStairDescentAnimation`, in 1/512-turn steps.
static const AnimationPackedRotation _gTonfaBatonStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_03F40_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonStairDescentAnimation`.
///
/// Durations count normal-rate frames; track 1 carries the descent footstep cues.
/// Control jumps use absolute record indices to repeat each track.
static const AnimationRecord _gTonfaBatonStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_03F40_records.inc"
};

/// Track-start record indices for `_gTonfaBatonStairDescentAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_03F40_indices.inc"
};

/// Looping player stair-descent clip in the tonfa baton bank.
///
/// Native bank slot 37 supplies 19 model-part tracks. Track 1's footstep cues
/// pace descent movement. Borrows read-only, word-aligned pose banks and records
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonStairDescentAnimation = {
    .records           = _gTonfaBatonStairDescentAnimationRecords,
    .trackStartIndices = _gTonfaBatonStairDescentAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonStairDescentAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonStairDescentAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonStairAscentFinishOddAnimation`.
///
/// XYZ use integer model coordinates; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gTonfaBatonStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04214_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonStairAscentFinishOddAnimation`, in 1/512-turn steps.
static const AnimationPackedRotation _gTonfaBatonStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04214_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonStairAscentFinishOddAnimation`.
///
/// Durations count normal-rate frames; track 1 carries the finishing footstep cue.
/// Stop controls hold each track's final pose.
static const AnimationRecord _gTonfaBatonStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04214_records.inc"
};

/// Track-start record indices for `_gTonfaBatonStairAscentFinishOddAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04214_indices.inc"
};

/// Player finishing stride for a stair ascent with an odd requested step count.
///
/// Tonfa baton bank slot 39 supplies 19 model-part tracks that stop on their final
/// pose, with a finishing footstep cue on track 1. Borrows read-only, word-aligned
/// pose banks and records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonStairAscentFinishOddAnimation = {
    .records           = _gTonfaBatonStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gTonfaBatonStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonStairAscentFinishOddAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonStairAscentFinishOddAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonStairAscentFinishEvenAnimation`.
///
/// XYZ use integer model coordinates; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gTonfaBatonStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_045A4_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonStairAscentFinishEvenAnimation`, in 1/512-turn steps.
static const AnimationPackedRotation _gTonfaBatonStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_045A4_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonStairAscentFinishEvenAnimation`.
///
/// Durations count normal-rate frames; track 1 carries the finishing footstep cue.
/// Stop controls hold each track's final pose.
static const AnimationRecord _gTonfaBatonStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_045A4_records.inc"
};

/// Track-start record indices for `_gTonfaBatonStairAscentFinishEvenAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_045A4_indices.inc"
};

/// Player finishing stride for a stair ascent with an even requested step count.
///
/// Tonfa baton bank slot 38 supplies 19 model-part tracks that stop on their final
/// pose, with a finishing footstep cue on track 1. Borrows read-only, word-aligned
/// pose banks and records for the loaded package's lifetime.
static AnimationSet _gTonfaBatonStairAscentFinishEvenAnimation = {
    .records           = _gTonfaBatonStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gTonfaBatonStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonStairAscentFinishEvenAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonStairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily0ReleaseAnimation`.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04A2C_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily0ReleaseAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04A2C_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonParasiteEnergyFamily0ReleaseAnimation`.
///
/// Durations count normal-rate frames; stop controls hold each track's final pose.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04A2C_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily0ReleaseAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04A2C_indices.inc"
};

/// Parasite Energy family 0 release clip in the tonfa baton player bank.
///
/// Bank slot 26 enters the action with a six-frame blend on child tracks 1..18.
/// Tracks stop on their final pose, allowing the action to advance to charging.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily0ReleaseAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily0ReleaseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily0ChargeAnimation`.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily0ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04C28_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily0ChargeAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily0ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04C28_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonParasiteEnergyFamily0ChargeAnimation`.
///
/// Durations count normal-rate frames; each track loops its two keyframes.
/// The following stop record is retained but bypassed by the loop.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily0ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04C28_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily0ChargeAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily0ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04C28_indices.inc"
};

/// Parasite Energy family 0 charge clip in the tonfa baton player bank.
///
/// Bank slot 27 loops child tracks 1..18 while the attachment countdown runs.
/// The cast clip replaces it when the countdown reaches zero.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily0ChargeAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily0ChargeAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily0ChargeAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily0ChargeAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily0ChargeAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily0CastAnimation`.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily0CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04F00_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily0CastAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily0CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04F00_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonParasiteEnergyFamily0CastAnimation`.
///
/// Durations count normal-rate frames; stop controls hold each track's final pose.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily0CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04F00_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily0CastAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily0CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_04F00_indices.inc"
};

/// Parasite Energy family 0 cast clip in the tonfa baton player bank.
///
/// Bank slot 28 plays child tracks 1..18 when the attachment countdown expires.
/// Tracks stop on their final pose, allowing the interrupted player state to resume.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily0CastAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily0CastAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily0CastAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily0CastAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily0CastAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily1ReleaseAnimation`.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_051A4_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily1ReleaseAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_051A4_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonParasiteEnergyFamily1ReleaseAnimation`.
///
/// Durations count normal-rate frames; stop controls hold each track's final pose.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_051A4_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily1ReleaseAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_051A4_indices.inc"
};

/// Parasite Energy family 1 release clip in the tonfa baton player bank.
///
/// Bank slot 29 enters the action with a six-frame blend on child tracks 1..18.
/// Tracks stop on their final pose, allowing the action to advance to charging.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily1ReleaseAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily1ReleaseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily1ChargeAnimation`.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily1ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_053A4_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily1ChargeAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily1ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_053A4_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonParasiteEnergyFamily1ChargeAnimation`.
///
/// Durations count normal-rate frames; each track loops its two keyframes.
/// The following stop record is retained but bypassed by the loop.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily1ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_053A4_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily1ChargeAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily1ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_053A4_indices.inc"
};

/// Parasite Energy family 1 charge clip in the tonfa baton player bank.
///
/// Bank slot 30 loops child tracks 1..18 while the attachment countdown runs.
/// The cast clip replaces it when the countdown reaches zero.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily1ChargeAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily1ChargeAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily1ChargeAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily1ChargeAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily1ChargeAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily1CastAnimation`.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily1CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_056F8_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily1CastAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily1CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_056F8_bank4.inc"
};

/// Keyframes and control records for `_gTonfaBatonParasiteEnergyFamily1CastAnimation`.
///
/// Durations count normal-rate frames; stop controls hold each track's final pose.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily1CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_056F8_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily1CastAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily1CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_056F8_indices.inc"
};

/// Parasite Energy family 1 cast clip in the tonfa baton player bank.
///
/// Bank slot 31 plays child tracks 1..18 when the attachment countdown expires.
/// Tracks stop on their final pose, allowing the interrupted player state to resume.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily1CastAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily1CastAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily1CastAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily1CastAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily1CastAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonHitPart4Animation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonHitPart4AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_059AC_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonHitPart4Animation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonHitPart4AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_059AC_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonHitPart4Animation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
static const AnimationRecord _gTonfaBatonHitPart4AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_059AC_records.inc"
};

/// Track-start record indices for `_gTonfaBatonHitPart4Animation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonHitPart4AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_059AC_indices.inc"
};

/// Player hit-reaction clip for the model-part-4 collision body in the tonfa baton bank.
///
/// Bank slot 16 blends child tracks 1..18 over three normal-rate frames.
/// Tracks stop on their final pose and carry no embedded action cues.
/// Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonHitPart4Animation = {
    .records           = _gTonfaBatonHitPart4AnimationRecords,
    .trackStartIndices = _gTonfaBatonHitPart4AnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonHitPart4AnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonHitPart4AnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonHitOtherBodyAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonHitOtherBodyAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_05CCC_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonHitOtherBodyAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonHitOtherBodyAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_05CCC_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonHitOtherBodyAnimation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
static const AnimationRecord _gTonfaBatonHitOtherBodyAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_05CCC_records.inc"
};

/// Track-start record indices for `_gTonfaBatonHitOtherBodyAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonHitOtherBodyAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_05CCC_indices.inc"
};

/// Player hit-reaction clip for other collision bodies in the tonfa baton bank.
///
/// Bank slot 17 blends child tracks 1..18 over three normal-rate frames.
/// Tracks stop on their final pose and carry no embedded action cues.
/// Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonHitOtherBodyAnimation = {
    .records           = _gTonfaBatonHitOtherBodyAnimationRecords,
    .trackStartIndices = _gTonfaBatonHitOtherBodyAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonHitOtherBodyAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonHitOtherBodyAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonDeathAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonDeathAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_06490_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonDeathAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonDeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_06490_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonDeathAnimation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
static const AnimationRecord _gTonfaBatonDeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_06490_records.inc"
};

/// Track-start record indices for `_gTonfaBatonDeathAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonDeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_06490_indices.inc"
};

/// Player death-pose clip in the tonfa baton bank.
///
/// Bank slot 18 plays child tracks 1..18 when HP reaches zero, leaving the root
/// track unchanged. Tracks stop on their final pose and carry no action cues.
/// Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonDeathAnimation = {
    .records           = _gTonfaBatonDeathAnimationRecords,
    .trackStartIndices = _gTonfaBatonDeathAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonDeathAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonDeathAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonHighHealthIdleAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonHighHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_07728_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonHighHealthIdleAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_07728_bank4.inc"
};

/// Keyframes and idle-loop controls for `_gTonfaBatonHighHealthIdleAnimation`.
///
/// Durations count normal-rate frames; jumps use absolute record indices.
static const AnimationRecord _gTonfaBatonHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_07728_records.inc"
};

/// Track-start record indices for `_gTonfaBatonHighHealthIdleAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_07728_indices.inc"
};

/// Player long-idle clip above half maximum HP in the tonfa baton bank.
///
/// After 300 direction-free ticks, bank slot 23 blends child tracks 1..18 over
/// five normal-rate frames. An introduction leads into repeating idle poses.
/// Tracks carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonHighHealthIdleAnimation = {
    .records           = _gTonfaBatonHighHealthIdleAnimationRecords,
    .trackStartIndices = _gTonfaBatonHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonHighHealthIdleAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonHighHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonLowHealthIdleAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonLowHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_082A0_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonLowHealthIdleAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_082A0_bank4.inc"
};

/// Keyframes and idle-loop controls for `_gTonfaBatonLowHealthIdleAnimation`.
///
/// Durations count normal-rate frames; jumps use absolute record indices.
static const AnimationRecord _gTonfaBatonLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_082A0_records.inc"
};

/// Track-start record indices for `_gTonfaBatonLowHealthIdleAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_082A0_indices.inc"
};

/// Player low-health long-idle clip, also used for paralysis, in the tonfa baton bank.
///
/// At or below quarter maximum HP after 300 direction-free ticks, bank slot 25
/// blends child tracks 1..18 over five normal-rate frames. Paralysis selects it at any HP and
/// blends the same child tracks over six frames. An introduction leads into
/// repeating poses; tracks carry no embedded action cues. Borrows read-only,
/// word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonLowHealthIdleAnimation = {
    .records           = _gTonfaBatonLowHealthIdleAnimationRecords,
    .trackStartIndices = _gTonfaBatonLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonLowHealthIdleAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonLowHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonMidHealthIdleAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonMidHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_089BC_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonMidHealthIdleAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_089BC_bank4.inc"
};

/// Keyframes and idle-loop controls for `_gTonfaBatonMidHealthIdleAnimation`.
///
/// Durations count normal-rate frames; jumps use absolute record indices.
static const AnimationRecord _gTonfaBatonMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_089BC_records.inc"
};

/// Track-start record indices for `_gTonfaBatonMidHealthIdleAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/tonfa_baton_animation_089BC_indices.inc"
};

/// Player long-idle clip above quarter and at or below half maximum HP in the tonfa baton bank.
///
/// After 300 direction-free ticks, bank slot 24 blends child tracks 1..18 over
/// five normal-rate frames. An introduction leads into repeating idle poses.
/// Tracks carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonMidHealthIdleAnimation = {
    .records           = _gTonfaBatonMidHealthIdleAnimationRecords,
    .trackStartIndices = _gTonfaBatonMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonMidHealthIdleAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonMidHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonScriptedPresentationEntryAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_08E3C_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonScriptedPresentationEntryAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_08E3C_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonScriptedPresentationEntryAnimation`.
///
/// Durations count normal-rate frames; each track holds its final keyframe.
static const AnimationRecord _gTonfaBatonScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_08E3C_records.inc"
};

/// Track-start record indices for `_gTonfaBatonScriptedPresentationEntryAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_08E3C_indices.inc"
};

/// Scripted presentation entry clip in the tonfa baton player bank.
///
/// Bank slot 32 (also stored in slot 34) plays for a zero presentation selector.
/// Child tracks 1..18 stop at their final poses while the scripted command runs.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonScriptedPresentationEntryAnimation = {
    .records           = _gTonfaBatonScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gTonfaBatonScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonScriptedPresentationEntryAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonScriptedPresentationEntryAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonScriptedPresentationExitAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09014_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonScriptedPresentationExitAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09014_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonScriptedPresentationExitAnimation`.
///
/// Each track has two keyframes followed by a stop; durations count normal-rate frames.
static const AnimationRecord _gTonfaBatonScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09014_records.inc"
};

/// Track-start record indices for `_gTonfaBatonScriptedPresentationExitAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09014_indices.inc"
};

/// Scripted presentation exit clip in the tonfa baton player bank.
///
/// Bank slot 33 (also stored in slot 35) plays for a nonzero presentation selector.
/// Child tracks 1..18 transition between two keyframes and stop; scripted callers
/// wait for completion before returning control. Records carry no embedded action
/// cues. Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonScriptedPresentationExitAnimation = {
    .records           = _gTonfaBatonScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gTonfaBatonScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonScriptedPresentationExitAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonScriptedPresentationExitAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily2ReleaseAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09578_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily2ReleaseAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09578_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonParasiteEnergyFamily2ReleaseAnimation`.
///
/// Durations count normal-rate frames; each track holds its final keyframe.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09578_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily2ReleaseAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09578_indices.inc"
};

/// Parasite Energy family 2 release clip in the tonfa baton player bank.
///
/// Bank slot 42 enters the action with a six-frame blend on child tracks 1..18.
/// Family 2 selects spell IDs whose tens digit is 3; playback also uses this clip
/// for any family selector other than 0 or 1. Tracks stop at their final poses
/// before charging. Records carry no embedded action cues. Borrows read-only,
/// word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily2ReleaseAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily2ReleaseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily2ChargeAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily2ChargeAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0976C_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily2ChargeAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily2ChargeAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0976C_bank4.inc"
};

/// Keyframes and loop controls for `_gTonfaBatonParasiteEnergyFamily2ChargeAnimation`.
///
/// Each track loops two one-frame keyframes through an absolute record-index jump.
/// The following stop record is retained but bypassed by the loop.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily2ChargeAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0976C_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily2ChargeAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily2ChargeAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0976C_indices.inc"
};

/// Parasite Energy family 2 charge clip in the tonfa baton player bank.
///
/// Bank slot 43 loops child tracks 1..18 while the attachment countdown runs.
/// The cast clip replaces it when the countdown reaches zero. Playback selects
/// this family for any selector other than 0 or 1. Records carry no embedded
/// action cues. Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily2ChargeAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily2ChargeAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily2ChargeAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily2ChargeAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily2ChargeAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonParasiteEnergyFamily2CastAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonParasiteEnergyFamily2CastAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09BEC_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonParasiteEnergyFamily2CastAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonParasiteEnergyFamily2CastAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09BEC_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonParasiteEnergyFamily2CastAnimation`.
///
/// Durations count normal-rate frames; each track holds its final keyframe.
static const AnimationRecord _gTonfaBatonParasiteEnergyFamily2CastAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09BEC_records.inc"
};

/// Track-start record indices for `_gTonfaBatonParasiteEnergyFamily2CastAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonParasiteEnergyFamily2CastAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09BEC_indices.inc"
};

/// Parasite Energy family 2 cast clip in the tonfa baton player bank.
///
/// Bank slot 44 replaces charging when the attachment countdown reaches zero.
/// Child tracks 1..18 stop at their final poses before normal locomotion or aiming
/// resumes. Playback selects this family for any selector other than 0 or 1.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonParasiteEnergyFamily2CastAnimation = {
    .records           = _gTonfaBatonParasiteEnergyFamily2CastAnimationRecords,
    .trackStartIndices = _gTonfaBatonParasiteEnergyFamily2CastAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonParasiteEnergyFamily2CastAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonParasiteEnergyFamily2CastAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonAimIdleAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonAimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09D78_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonAimIdleAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonAimIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09D78_bank4.inc"
};

/// Keyframes and return controls for `_gTonfaBatonAimIdleAnimation`.
///
/// Each one-frame keyframe is followed by a jump back to its own record index.
/// Returning to the same keyframe settles the pose.
/// The following stop record is retained but bypassed by the return.
static const AnimationRecord _gTonfaBatonAimIdleAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09D78_records.inc"
};

/// Track-start record indices for `_gTonfaBatonAimIdleAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonAimIdleAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_09D78_indices.inc"
};

/// Stationary aim pose in the tonfa baton player bank.
///
/// Bank slot 9 holds child tracks 1..18 after aim entry and while aiming without
/// movement or turning. Scripted grab recovery can borrow the same clip through
/// a replacement set table. Each track returns to its single keyframe and settles.
/// Records carry no embedded action cues. Borrows read-only, word-aligned data
/// for the loaded package's lifetime.
static AnimationSet _gTonfaBatonAimIdleAnimation = {
    .records           = _gTonfaBatonAimIdleAnimationRecords,
    .trackStartIndices = _gTonfaBatonAimIdleAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonAimIdleAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonAimIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonAimBackwardOrTurnAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0A594_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonAimBackwardOrTurnAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonAimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0A594_bank4.inc"
};

/// Keyframes and loop controls for `_gTonfaBatonAimBackwardOrTurnAnimation`.
///
/// Durations count normal-rate frames. Child-track jumps return past the entry
/// keyframes, except track 12's two-keyframe loop. Track 1 carries both footstep
/// cues; stop records after the jumps are retained but bypassed during looping.
static const AnimationRecord _gTonfaBatonAimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0A594_records.inc"
};

/// Track-start record indices for `_gTonfaBatonAimBackwardOrTurnAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonAimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0A594_indices.inc"
};

/// Backward aim locomotion and stationary turning clip in the tonfa baton player bank.
///
/// Bank slot 13 supplies child tracks 1..18 when aiming with backward input or
/// turning without movement. Repeats the child tracks and carries footstep cues
/// on track 1. Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonAimBackwardOrTurnAnimation = {
    .records           = _gTonfaBatonAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gTonfaBatonAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonAimBackwardOrTurnAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonAimBackwardOrTurnAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonAimForwardAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonAimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0AE50_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonAimForwardAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonAimForwardAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0AE50_bank4.inc"
};

/// Keyframes and loop controls for `_gTonfaBatonAimForwardAnimation`.
///
/// Durations count normal-rate frames. Child-track jumps skip the four entry
/// keyframes. Track 1 carries both footstep cues; stop records after the jumps
/// are retained but bypassed during looping.
static const AnimationRecord _gTonfaBatonAimForwardAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0AE50_records.inc"
};

/// Track-start record indices for `_gTonfaBatonAimForwardAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonAimForwardAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0AE50_indices.inc"
};

/// Forward aim locomotion clip in the tonfa baton player bank.
///
/// Bank slot 12 supplies child tracks 1..18 while aiming with forward input.
/// Repeats the child tracks after entry and carries footstep cues on track 1.
/// Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonAimForwardAnimation = {
    .records           = _gTonfaBatonAimForwardAnimationRecords,
    .trackStartIndices = _gTonfaBatonAimForwardAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonAimForwardAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonAimForwardAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonBattleEndAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonBattleEndAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0B5C8_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonBattleEndAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonBattleEndAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0B5C8_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonBattleEndAnimation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
/// Records carry no embedded action cues.
static const AnimationRecord _gTonfaBatonBattleEndAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0B5C8_records.inc"
};

/// Track-start record indices for `_gTonfaBatonBattleEndAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonBattleEndAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0B5C8_indices.inc"
};

/// Battle-end action clip in the tonfa baton player bank.
///
/// Bank slot 20 supplies child tracks 1..18 for the action after combat ends.
/// Tracks stop at their final poses. The baton performs no ammunition load or
/// reload sound during this action. Borrows read-only, word-aligned data for the
/// loaded package's lifetime.
static AnimationSet _gTonfaBatonBattleEndAnimation = {
    .records           = _gTonfaBatonBattleEndAnimationRecords,
    .trackStartIndices = _gTonfaBatonBattleEndAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonBattleEndAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonBattleEndAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonFirstStrikeAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonFirstStrikeAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0BC08_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonFirstStrikeAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonFirstStrikeAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0BC08_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonFirstStrikeAnimation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
/// Records carry no embedded action cues; strike timing belongs to the attack state.
static const AnimationRecord _gTonfaBatonFirstStrikeAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0BC08_records.inc"
};

/// Track-start record indices for `_gTonfaBatonFirstStrikeAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonFirstStrikeAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0BC08_indices.inc"
};

/// First baton strike clip in the tonfa baton player bank.
///
/// Bank slot 10 supplies child tracks 1..18, entered with a three-frame blend
/// from each track's start. Finishing track 1 selects either the queued combo strike
/// or single-strike recovery. Borrows read-only, word-aligned data for the
/// loaded package's lifetime.
static AnimationSet _gTonfaBatonFirstStrikeAnimation = {
    .records           = _gTonfaBatonFirstStrikeAnimationRecords,
    .trackStartIndices = _gTonfaBatonFirstStrikeAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonFirstStrikeAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonFirstStrikeAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonStrikeRecoveryAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonStrikeRecoveryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0C424_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonStrikeRecoveryAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonStrikeRecoveryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0C424_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonStrikeRecoveryAnimation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
/// Records carry no embedded action cues.
static const AnimationRecord _gTonfaBatonStrikeRecoveryAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0C424_records.inc"
};

/// Track-start record indices for `_gTonfaBatonStrikeRecoveryAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonStrikeRecoveryAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0C424_indices.inc"
};

/// Recovery clip after a single baton strike in the tonfa baton player bank.
///
/// Bank slot 14 supplies child tracks 1..18 when no second strike was queued;
/// finishing track 1 returns control to the ordinary weapon-attack completion.
/// Slot 15 aliases this same set in the generic reload table positions.
/// Borrows read-only, word-aligned data for the loaded package's lifetime.
static AnimationSet _gTonfaBatonStrikeRecoveryAnimation = {
    .records           = _gTonfaBatonStrikeRecoveryAnimationRecords,
    .trackStartIndices = _gTonfaBatonStrikeRecoveryAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonStrikeRecoveryAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonStrikeRecoveryAnimationBank4,
    },
};

/// Translation/rotation poses for `_gTonfaBatonComboStrikeAnimation`.
///
/// Tracks 0 and 1 use integer local coordinates and 4096 angle units per turn.
static const AnimationPackedPose _gTonfaBatonComboStrikeAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0D42C_bank1.inc"
};

/// Rotation-only poses for `_gTonfaBatonComboStrikeAnimation`, in 1/512-turn steps.
///
/// Tracks 2..18 preserve each model part's local translation.
static const AnimationPackedRotation _gTonfaBatonComboStrikeAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0D42C_bank4.inc"
};

/// Keyframes and stop controls for `_gTonfaBatonComboStrikeAnimation`.
///
/// Durations count normal-rate frames; each track holds its final pose.
/// Records carry no embedded action cues; strike timing belongs to the attack state.
static const AnimationRecord _gTonfaBatonComboStrikeAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0D42C_records.inc"
};

/// Track-start record indices for `_gTonfaBatonComboStrikeAnimation`.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gTonfaBatonComboStrikeAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/tonfa_baton_animation_0D42C_indices.inc"
};

/// Queued second strike and its recovery in the tonfa baton player bank.
///
/// Bank slot 11 restarts child tracks 1..18 when the first strike ends with a
/// second strike queued. Finishing track 1 completes the attack without a
/// separate recovery clip. Borrows read-only, word-aligned data for the loaded
/// package's lifetime.
static AnimationSet _gTonfaBatonComboStrikeAnimation = {
    .records           = _gTonfaBatonComboStrikeAnimationRecords,
    .trackStartIndices = _gTonfaBatonComboStrikeAnimationIndices,
    .poseBanks         = {
        [TONFA_BATON_ANIMATION_TRANSLATION_ROTATION_BANK] = _gTonfaBatonComboStrikeAnimationBank1,
        [TONFA_BATON_ANIMATION_PACKED_ROTATION_BANK]      = _gTonfaBatonComboStrikeAnimationBank4,
    },
};

static AnimationPackedPose _gTonfaBatonAnimation0D6F4Bank1[5] = {
#include "assets/tonfa_baton_animation_0D6F4_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0D6F4Bank4[54] = {
#include "assets/tonfa_baton_animation_0D6F4_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0D6F4Records[89] = {
#include "assets/tonfa_baton_animation_0D6F4_records.inc"
};

static u16 _gTonfaBatonAnimation0D6F4Indices[20] = {
#include "assets/tonfa_baton_animation_0D6F4_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0D6F4 = {
    _gTonfaBatonAnimation0D6F4Records,
    _gTonfaBatonAnimation0D6F4Indices,
    { NULL, _gTonfaBatonAnimation0D6F4Bank1, NULL, NULL, _gTonfaBatonAnimation0D6F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0D8BCBank1[3] = {
#include "assets/tonfa_baton_animation_0D8BC_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0D8BCBank4[28] = {
#include "assets/tonfa_baton_animation_0D8BC_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0D8BCRecords[57] = {
#include "assets/tonfa_baton_animation_0D8BC_records.inc"
};

static u16 _gTonfaBatonAnimation0D8BCIndices[20] = {
#include "assets/tonfa_baton_animation_0D8BC_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0D8BC = {
    _gTonfaBatonAnimation0D8BCRecords,
    _gTonfaBatonAnimation0D8BCIndices,
    { NULL, _gTonfaBatonAnimation0D8BCBank1, NULL, NULL, _gTonfaBatonAnimation0D8BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0E194Bank1[19] = {
#include "assets/tonfa_baton_animation_0E194_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0E194Bank4[193] = {
#include "assets/tonfa_baton_animation_0E194_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0E194Records[296] = {
#include "assets/tonfa_baton_animation_0E194_records.inc"
};

static u16 _gTonfaBatonAnimation0E194Indices[20] = {
#include "assets/tonfa_baton_animation_0E194_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0E194 = {
    _gTonfaBatonAnimation0E194Records,
    _gTonfaBatonAnimation0E194Indices,
    { NULL, _gTonfaBatonAnimation0E194Bank1, NULL, NULL, _gTonfaBatonAnimation0E194Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0E8C8Bank1[13] = {
#include "assets/tonfa_baton_animation_0E8C8_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0E8C8Bank4[167] = {
#include "assets/tonfa_baton_animation_0E8C8_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0E8C8Records[235] = {
#include "assets/tonfa_baton_animation_0E8C8_records.inc"
};

static u16 _gTonfaBatonAnimation0E8C8Indices[20] = {
#include "assets/tonfa_baton_animation_0E8C8_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0E8C8 = {
    _gTonfaBatonAnimation0E8C8Records,
    _gTonfaBatonAnimation0E8C8Indices,
    { NULL, _gTonfaBatonAnimation0E8C8Bank1, NULL, NULL, _gTonfaBatonAnimation0E8C8Bank4, NULL, NULL, NULL },
};

AnimationBank D_tonfa_baton_8012BAB0 = { { {
    NULL,
    &_gTonfaBatonIdleAnimation,
    &_gTonfaBatonWalkAnimation,
    &_gTonfaBatonAnimation0E194,
    &_gTonfaBatonAnimation0E8C8,
    &_gTonfaBatonTurnNegativeAnimation,
    &_gTonfaBatonTurnPositiveAnimation,
    &_gTonfaBatonAnimation0D6F4,
    &_gTonfaBatonAnimation0D8BC,
    &_gTonfaBatonAimIdleAnimation,
    &_gTonfaBatonFirstStrikeAnimation,
    &_gTonfaBatonComboStrikeAnimation,
    &_gTonfaBatonAimForwardAnimation,
    &_gTonfaBatonAimBackwardOrTurnAnimation,
    &_gTonfaBatonStrikeRecoveryAnimation,
    &_gTonfaBatonStrikeRecoveryAnimation,
    &_gTonfaBatonHitPart4Animation,
    &_gTonfaBatonHitOtherBodyAnimation,
    &_gTonfaBatonDeathAnimation,
    &_gTonfaBatonWalkAnimation,
    &_gTonfaBatonBattleEndAnimation,
    &_gTonfaBatonIdleAnimation,
    &_gTonfaBatonIdleAnimation,
    &_gTonfaBatonHighHealthIdleAnimation,
    &_gTonfaBatonMidHealthIdleAnimation,
    &_gTonfaBatonLowHealthIdleAnimation,
    &_gTonfaBatonParasiteEnergyFamily0ReleaseAnimation,
    &_gTonfaBatonParasiteEnergyFamily0ChargeAnimation,
    &_gTonfaBatonParasiteEnergyFamily0CastAnimation,
    &_gTonfaBatonParasiteEnergyFamily1ReleaseAnimation,
    &_gTonfaBatonParasiteEnergyFamily1ChargeAnimation,
    &_gTonfaBatonParasiteEnergyFamily1CastAnimation,
    &_gTonfaBatonScriptedPresentationEntryAnimation,
    &_gTonfaBatonScriptedPresentationExitAnimation,
    &_gTonfaBatonScriptedPresentationEntryAnimation,
    &_gTonfaBatonScriptedPresentationExitAnimation,
    &_gTonfaBatonStairAscentAnimation,
    &_gTonfaBatonStairDescentAnimation,
    &_gTonfaBatonStairAscentFinishEvenAnimation,
    &_gTonfaBatonStairAscentFinishOddAnimation,
    &_gTonfaBatonItemUseAnimation,
    &_gTonfaBatonIdleAnimation,
    &_gTonfaBatonParasiteEnergyFamily2ReleaseAnimation,
    &_gTonfaBatonParasiteEnergyFamily2ChargeAnimation,
    &_gTonfaBatonParasiteEnergyFamily2CastAnimation,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

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
