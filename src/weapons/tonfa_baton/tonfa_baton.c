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

/// The near end of the baton trail inside the weapon frame; the task's own
/// coordinate starts there. The far end follows it directly, and state 0 reaches
/// that as element 1 of this array.
static SVECTOR D_tonfa_baton_8011E0F0[1] = { { 0, 0x0080, 0, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_tonfa_baton_8011E0F0[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
static SVECTOR D_tonfa_baton_8011E0F8 = { 0, -0x0200, 0, 0 };

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
    const SVECTOR* tipOffset;
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
                baseCoord->coord.t[0]   = D_tonfa_baton_8011E0F0[0].vx;
                baseCoord->coord.t[1]   = D_tonfa_baton_8011E0F0[0].vy;
                baseCoord->coord.t[2]   = D_tonfa_baton_8011E0F0[0].vz;
                baseCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(baseCoord);
                task->state = TONFA_BATON_TRAIL_RECORD;

                // The seed chains the tip below the base; later samples use the weapon parent.
                // Only translations feed the ribbon; the temporary rotation is left uninitialized.
                tipOffset                = &D_tonfa_baton_8011E0F0[1];
                tipEndpoint.parent       = baseCoord;
                tipEndpoint.coord.t[0]   = tipOffset->vx;
                tipEndpoint.coord.t[1]   = tipOffset->vy;
                tipEndpoint.coord.t[2]   = tipOffset->vz;
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
                D_tonfa_baton_8012C0EC = packedTint;
                break;
            case TONFA_BATON_TRAIL_RECORD:
                baseCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(baseCoord);
                tipEndpoint.parent       = effectWork->parent;
                tipEndpoint.coord.t[0]   = D_tonfa_baton_8011E0F8.vx;
                tipEndpoint.coord.t[1]   = D_tonfa_baton_8011E0F8.vy;
                tipEndpoint.coord.t[2]   = D_tonfa_baton_8011E0F8.vz;
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
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1), D_tonfa_baton_8012C0EC);
                break;
        }
        if (effectWork->age >= TONFA_BATON_TRAIL_LIFETIME_TICKS) {
            effectKillTask(effectWork, task);
        }
    }
}

#include "../../shared/blade_trail_draw.inc.c"

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

static TmdBone _gTonfaBatonModel010C8Skeleton[1] = {
#include "assets/tonfa_baton_model_010C8_skeleton.inc"
};

static u32 _gTonfaBatonModel010C8PartVerts[1] = {
#include "assets/tonfa_baton_model_010C8_partVerts.inc"
};

static SVECTOR _gTonfaBatonModel010C8Verts[24] = {
#include "assets/tonfa_baton_model_010C8_verts.inc"
};

static SVECTOR _gTonfaBatonModel010C8Normals[20] = {
#include "assets/tonfa_baton_model_010C8_normals.inc"
};

static u32 _gTonfaBatonModel010C8Stream[118] = {
#include "assets/tonfa_baton_model_010C8_stream.inc"
};

TmdSource D_tonfa_baton_8011E460 = {
    0,
    832,
    0,
    1,
    _gTonfaBatonModel010C8PartVerts,
    _gTonfaBatonModel010C8Verts,
    _gTonfaBatonModel010C8Normals,
    _gTonfaBatonModel010C8Skeleton,
    _gTonfaBatonModel010C8Stream,
};

static TmdBone _gTonfaBatonModel0136CSkeleton[1] = {
#include "assets/tonfa_baton_model_0136C_skeleton.inc"
};

static u32 _gTonfaBatonModel0136CPartVerts[1] = {
#include "assets/tonfa_baton_model_0136C_partVerts.inc"
};

static SVECTOR _gTonfaBatonModel0136CVerts[8] = {
#include "assets/tonfa_baton_model_0136C_verts.inc"
};

static SVECTOR _gTonfaBatonModel0136CNormals[8] = {
#include "assets/tonfa_baton_model_0136C_normals.inc"
};

static u32 _gTonfaBatonModel0136CStream[48] = {
#include "assets/tonfa_baton_model_0136C_stream.inc"
};

TmdSource D_tonfa_baton_8011E5EC = {
    0,
    312,
    0,
    1,
    _gTonfaBatonModel0136CPartVerts,
    _gTonfaBatonModel0136CVerts,
    _gTonfaBatonModel0136CNormals,
    _gTonfaBatonModel0136CSkeleton,
    _gTonfaBatonModel0136CStream,
};

static AnimationPackedPose _gTonfaBatonAnimation015E0Bank1[2] = {
#include "assets/tonfa_baton_animation_015E0_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation015E0Bank4[8] = {
#include "assets/tonfa_baton_animation_015E0_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation015E0Records[76] = {
#include "assets/tonfa_baton_animation_015E0_records.inc"
};

static u16 _gTonfaBatonAnimation015E0Indices[20] = {
#include "assets/tonfa_baton_animation_015E0_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation015E0 = {
    _gTonfaBatonAnimation015E0Records,
    _gTonfaBatonAnimation015E0Indices,
    { NULL, _gTonfaBatonAnimation015E0Bank1, NULL, NULL, _gTonfaBatonAnimation015E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation01C84Bank1[12] = {
#include "assets/tonfa_baton_animation_01C84_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation01C84Bank4[151] = {
#include "assets/tonfa_baton_animation_01C84_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation01C84Records[218] = {
#include "assets/tonfa_baton_animation_01C84_records.inc"
};

static u16 _gTonfaBatonAnimation01C84Indices[20] = {
#include "assets/tonfa_baton_animation_01C84_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation01C84 = {
    _gTonfaBatonAnimation01C84Records,
    _gTonfaBatonAnimation01C84Indices,
    { NULL, _gTonfaBatonAnimation01C84Bank1, NULL, NULL, _gTonfaBatonAnimation01C84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation024E4Bank1[19] = {
#include "assets/tonfa_baton_animation_024E4_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation024E4Bank4[169] = {
#include "assets/tonfa_baton_animation_024E4_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation024E4Records[290] = {
#include "assets/tonfa_baton_animation_024E4_records.inc"
};

static u16 _gTonfaBatonAnimation024E4Indices[20] = {
#include "assets/tonfa_baton_animation_024E4_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation024E4 = {
    _gTonfaBatonAnimation024E4Records,
    _gTonfaBatonAnimation024E4Indices,
    { NULL, _gTonfaBatonAnimation024E4Bank1, NULL, NULL, _gTonfaBatonAnimation024E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation02D48Bank1[19] = {
#include "assets/tonfa_baton_animation_02D48_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation02D48Bank4[170] = {
#include "assets/tonfa_baton_animation_02D48_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation02D48Records[290] = {
#include "assets/tonfa_baton_animation_02D48_records.inc"
};

static u16 _gTonfaBatonAnimation02D48Indices[20] = {
#include "assets/tonfa_baton_animation_02D48_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation02D48 = {
    _gTonfaBatonAnimation02D48Records,
    _gTonfaBatonAnimation02D48Indices,
    { NULL, _gTonfaBatonAnimation02D48Bank1, NULL, NULL, _gTonfaBatonAnimation02D48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0305CBank1[3] = {
#include "assets/tonfa_baton_animation_0305C_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0305CBank4[69] = {
#include "assets/tonfa_baton_animation_0305C_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0305CRecords[99] = {
#include "assets/tonfa_baton_animation_0305C_records.inc"
};

static u16 _gTonfaBatonAnimation0305CIndices[20] = {
#include "assets/tonfa_baton_animation_0305C_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0305C = {
    _gTonfaBatonAnimation0305CRecords,
    _gTonfaBatonAnimation0305CIndices,
    { NULL, _gTonfaBatonAnimation0305CBank1, NULL, NULL, _gTonfaBatonAnimation0305CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation037B8Bank1[14] = {
#include "assets/tonfa_baton_animation_037B8_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation037B8Bank4[156] = {
#include "assets/tonfa_baton_animation_037B8_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation037B8Records[253] = {
#include "assets/tonfa_baton_animation_037B8_records.inc"
};

static u16 _gTonfaBatonAnimation037B8Indices[20] = {
#include "assets/tonfa_baton_animation_037B8_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation037B8 = {
    _gTonfaBatonAnimation037B8Records,
    _gTonfaBatonAnimation037B8Indices,
    { NULL, _gTonfaBatonAnimation037B8Bank1, NULL, NULL, _gTonfaBatonAnimation037B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation03F40Bank1[16] = {
#include "assets/tonfa_baton_animation_03F40_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation03F40Bank4[167] = {
#include "assets/tonfa_baton_animation_03F40_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation03F40Records[247] = {
#include "assets/tonfa_baton_animation_03F40_records.inc"
};

static u16 _gTonfaBatonAnimation03F40Indices[20] = {
#include "assets/tonfa_baton_animation_03F40_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation03F40 = {
    _gTonfaBatonAnimation03F40Records,
    _gTonfaBatonAnimation03F40Indices,
    { NULL, _gTonfaBatonAnimation03F40Bank1, NULL, NULL, _gTonfaBatonAnimation03F40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation04214Bank1[6] = {
#include "assets/tonfa_baton_animation_04214_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation04214Bank4[52] = {
#include "assets/tonfa_baton_animation_04214_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation04214Records[91] = {
#include "assets/tonfa_baton_animation_04214_records.inc"
};

static u16 _gTonfaBatonAnimation04214Indices[20] = {
#include "assets/tonfa_baton_animation_04214_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation04214 = {
    _gTonfaBatonAnimation04214Records,
    _gTonfaBatonAnimation04214Indices,
    { NULL, _gTonfaBatonAnimation04214Bank1, NULL, NULL, _gTonfaBatonAnimation04214Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation045A4Bank1[7] = {
#include "assets/tonfa_baton_animation_045A4_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation045A4Bank4[73] = {
#include "assets/tonfa_baton_animation_045A4_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation045A4Records[114] = {
#include "assets/tonfa_baton_animation_045A4_records.inc"
};

static u16 _gTonfaBatonAnimation045A4Indices[20] = {
#include "assets/tonfa_baton_animation_045A4_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation045A4 = {
    _gTonfaBatonAnimation045A4Records,
    _gTonfaBatonAnimation045A4Indices,
    { NULL, _gTonfaBatonAnimation045A4Bank1, NULL, NULL, _gTonfaBatonAnimation045A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation04A2CBank1[9] = {
#include "assets/tonfa_baton_animation_04A2C_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation04A2CBank4[104] = {
#include "assets/tonfa_baton_animation_04A2C_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation04A2CRecords[139] = {
#include "assets/tonfa_baton_animation_04A2C_records.inc"
};

static u16 _gTonfaBatonAnimation04A2CIndices[20] = {
#include "assets/tonfa_baton_animation_04A2C_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation04A2C = {
    _gTonfaBatonAnimation04A2CRecords,
    _gTonfaBatonAnimation04A2CIndices,
    { NULL, _gTonfaBatonAnimation04A2CBank1, NULL, NULL, _gTonfaBatonAnimation04A2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation04C28Bank1[3] = {
#include "assets/tonfa_baton_animation_04C28_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation04C28Bank4[22] = {
#include "assets/tonfa_baton_animation_04C28_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation04C28Records[76] = {
#include "assets/tonfa_baton_animation_04C28_records.inc"
};

static u16 _gTonfaBatonAnimation04C28Indices[20] = {
#include "assets/tonfa_baton_animation_04C28_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation04C28 = {
    _gTonfaBatonAnimation04C28Records,
    _gTonfaBatonAnimation04C28Indices,
    { NULL, _gTonfaBatonAnimation04C28Bank1, NULL, NULL, _gTonfaBatonAnimation04C28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation04F00Bank1[6] = {
#include "assets/tonfa_baton_animation_04F00_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation04F00Bank4[57] = {
#include "assets/tonfa_baton_animation_04F00_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation04F00Records[87] = {
#include "assets/tonfa_baton_animation_04F00_records.inc"
};

static u16 _gTonfaBatonAnimation04F00Indices[20] = {
#include "assets/tonfa_baton_animation_04F00_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation04F00 = {
    _gTonfaBatonAnimation04F00Records,
    _gTonfaBatonAnimation04F00Indices,
    { NULL, _gTonfaBatonAnimation04F00Bank1, NULL, NULL, _gTonfaBatonAnimation04F00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation051A4Bank1[4] = {
#include "assets/tonfa_baton_animation_051A4_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation051A4Bank4[55] = {
#include "assets/tonfa_baton_animation_051A4_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation051A4Records[82] = {
#include "assets/tonfa_baton_animation_051A4_records.inc"
};

static u16 _gTonfaBatonAnimation051A4Indices[20] = {
#include "assets/tonfa_baton_animation_051A4_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation051A4 = {
    _gTonfaBatonAnimation051A4Records,
    _gTonfaBatonAnimation051A4Indices,
    { NULL, _gTonfaBatonAnimation051A4Bank1, NULL, NULL, _gTonfaBatonAnimation051A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation053A4Bank1[3] = {
#include "assets/tonfa_baton_animation_053A4_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation053A4Bank4[23] = {
#include "assets/tonfa_baton_animation_053A4_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation053A4Records[76] = {
#include "assets/tonfa_baton_animation_053A4_records.inc"
};

static u16 _gTonfaBatonAnimation053A4Indices[20] = {
#include "assets/tonfa_baton_animation_053A4_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation053A4 = {
    _gTonfaBatonAnimation053A4Records,
    _gTonfaBatonAnimation053A4Indices,
    { NULL, _gTonfaBatonAnimation053A4Bank1, NULL, NULL, _gTonfaBatonAnimation053A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation056F8Bank1[8] = {
#include "assets/tonfa_baton_animation_056F8_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation056F8Bank4[68] = {
#include "assets/tonfa_baton_animation_056F8_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation056F8Records[101] = {
#include "assets/tonfa_baton_animation_056F8_records.inc"
};

static u16 _gTonfaBatonAnimation056F8Indices[20] = {
#include "assets/tonfa_baton_animation_056F8_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation056F8 = {
    _gTonfaBatonAnimation056F8Records,
    _gTonfaBatonAnimation056F8Indices,
    { NULL, _gTonfaBatonAnimation056F8Bank1, NULL, NULL, _gTonfaBatonAnimation056F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation059ACBank1[5] = {
#include "assets/tonfa_baton_animation_059AC_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation059ACBank4[55] = {
#include "assets/tonfa_baton_animation_059AC_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation059ACRecords[83] = {
#include "assets/tonfa_baton_animation_059AC_records.inc"
};

static u16 _gTonfaBatonAnimation059ACIndices[20] = {
#include "assets/tonfa_baton_animation_059AC_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation059AC = {
    _gTonfaBatonAnimation059ACRecords,
    _gTonfaBatonAnimation059ACIndices,
    { NULL, _gTonfaBatonAnimation059ACBank1, NULL, NULL, _gTonfaBatonAnimation059ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation05CCCBank1[6] = {
#include "assets/tonfa_baton_animation_05CCC_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation05CCCBank4[66] = {
#include "assets/tonfa_baton_animation_05CCC_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation05CCCRecords[96] = {
#include "assets/tonfa_baton_animation_05CCC_records.inc"
};

static u16 _gTonfaBatonAnimation05CCCIndices[20] = {
#include "assets/tonfa_baton_animation_05CCC_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation05CCC = {
    _gTonfaBatonAnimation05CCCRecords,
    _gTonfaBatonAnimation05CCCIndices,
    { NULL, _gTonfaBatonAnimation05CCCBank1, NULL, NULL, _gTonfaBatonAnimation05CCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation06490Bank1[18] = {
#include "assets/tonfa_baton_animation_06490_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation06490Bank4[184] = {
#include "assets/tonfa_baton_animation_06490_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation06490Records[239] = {
#include "assets/tonfa_baton_animation_06490_records.inc"
};

static u16 _gTonfaBatonAnimation06490Indices[20] = {
#include "assets/tonfa_baton_animation_06490_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation06490 = {
    _gTonfaBatonAnimation06490Records,
    _gTonfaBatonAnimation06490Indices,
    { NULL, _gTonfaBatonAnimation06490Bank1, NULL, NULL, _gTonfaBatonAnimation06490Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation07728Bank1[29] = {
#include "assets/tonfa_baton_animation_07728_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation07728Bank4[450] = {
#include "assets/tonfa_baton_animation_07728_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation07728Records[633] = {
#include "assets/tonfa_baton_animation_07728_records.inc"
};

static u16 _gTonfaBatonAnimation07728Indices[20] = {
#include "assets/tonfa_baton_animation_07728_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation07728 = {
    _gTonfaBatonAnimation07728Records,
    _gTonfaBatonAnimation07728Indices,
    { NULL, _gTonfaBatonAnimation07728Bank1, NULL, NULL, _gTonfaBatonAnimation07728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation082A0Bank1[12] = {
#include "assets/tonfa_baton_animation_082A0_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation082A0Bank4[266] = {
#include "assets/tonfa_baton_animation_082A0_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation082A0Records[412] = {
#include "assets/tonfa_baton_animation_082A0_records.inc"
};

static u16 _gTonfaBatonAnimation082A0Indices[20] = {
#include "assets/tonfa_baton_animation_082A0_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation082A0 = {
    _gTonfaBatonAnimation082A0Records,
    _gTonfaBatonAnimation082A0Indices,
    { NULL, _gTonfaBatonAnimation082A0Bank1, NULL, NULL, _gTonfaBatonAnimation082A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation089BCBank1[9] = {
#include "assets/tonfa_baton_animation_089BC_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation089BCBank4[144] = {
#include "assets/tonfa_baton_animation_089BC_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation089BCRecords[264] = {
#include "assets/tonfa_baton_animation_089BC_records.inc"
};

static u16 _gTonfaBatonAnimation089BCIndices[20] = {
#include "assets/tonfa_baton_animation_089BC_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation089BC = {
    _gTonfaBatonAnimation089BCRecords,
    _gTonfaBatonAnimation089BCIndices,
    { NULL, _gTonfaBatonAnimation089BCBank1, NULL, NULL, _gTonfaBatonAnimation089BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation08E3CBank1[6] = {
#include "assets/tonfa_baton_animation_08E3C_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation08E3CBank4[107] = {
#include "assets/tonfa_baton_animation_08E3C_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation08E3CRecords[143] = {
#include "assets/tonfa_baton_animation_08E3C_records.inc"
};

static u16 _gTonfaBatonAnimation08E3CIndices[20] = {
#include "assets/tonfa_baton_animation_08E3C_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation08E3C = {
    _gTonfaBatonAnimation08E3CRecords,
    _gTonfaBatonAnimation08E3CIndices,
    { NULL, _gTonfaBatonAnimation08E3CBank1, NULL, NULL, _gTonfaBatonAnimation08E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation09014Bank1[3] = {
#include "assets/tonfa_baton_animation_09014_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation09014Bank4[32] = {
#include "assets/tonfa_baton_animation_09014_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation09014Records[57] = {
#include "assets/tonfa_baton_animation_09014_records.inc"
};

static u16 _gTonfaBatonAnimation09014Indices[20] = {
#include "assets/tonfa_baton_animation_09014_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation09014 = {
    _gTonfaBatonAnimation09014Records,
    _gTonfaBatonAnimation09014Indices,
    { NULL, _gTonfaBatonAnimation09014Bank1, NULL, NULL, _gTonfaBatonAnimation09014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation09578Bank1[11] = {
#include "assets/tonfa_baton_animation_09578_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation09578Bank4[125] = {
#include "assets/tonfa_baton_animation_09578_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation09578Records[167] = {
#include "assets/tonfa_baton_animation_09578_records.inc"
};

static u16 _gTonfaBatonAnimation09578Indices[20] = {
#include "assets/tonfa_baton_animation_09578_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation09578 = {
    _gTonfaBatonAnimation09578Records,
    _gTonfaBatonAnimation09578Indices,
    { NULL, _gTonfaBatonAnimation09578Bank1, NULL, NULL, _gTonfaBatonAnimation09578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0976CBank1[3] = {
#include "assets/tonfa_baton_animation_0976C_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0976CBank4[20] = {
#include "assets/tonfa_baton_animation_0976C_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0976CRecords[76] = {
#include "assets/tonfa_baton_animation_0976C_records.inc"
};

static u16 _gTonfaBatonAnimation0976CIndices[20] = {
#include "assets/tonfa_baton_animation_0976C_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0976C = {
    _gTonfaBatonAnimation0976CRecords,
    _gTonfaBatonAnimation0976CIndices,
    { NULL, _gTonfaBatonAnimation0976CBank1, NULL, NULL, _gTonfaBatonAnimation0976CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation09BECBank1[8] = {
#include "assets/tonfa_baton_animation_09BEC_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation09BECBank4[105] = {
#include "assets/tonfa_baton_animation_09BEC_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation09BECRecords[139] = {
#include "assets/tonfa_baton_animation_09BEC_records.inc"
};

static u16 _gTonfaBatonAnimation09BECIndices[20] = {
#include "assets/tonfa_baton_animation_09BEC_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation09BEC = {
    _gTonfaBatonAnimation09BECRecords,
    _gTonfaBatonAnimation09BECIndices,
    { NULL, _gTonfaBatonAnimation09BECBank1, NULL, NULL, _gTonfaBatonAnimation09BECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation09D78Bank1[2] = {
#include "assets/tonfa_baton_animation_09D78_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation09D78Bank4[16] = {
#include "assets/tonfa_baton_animation_09D78_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation09D78Records[57] = {
#include "assets/tonfa_baton_animation_09D78_records.inc"
};

static u16 _gTonfaBatonAnimation09D78Indices[20] = {
#include "assets/tonfa_baton_animation_09D78_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation09D78 = {
    _gTonfaBatonAnimation09D78Records,
    _gTonfaBatonAnimation09D78Indices,
    { NULL, _gTonfaBatonAnimation09D78Bank1, NULL, NULL, _gTonfaBatonAnimation09D78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0A594Bank1[15] = {
#include "assets/tonfa_baton_animation_0A594_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0A594Bank4[185] = {
#include "assets/tonfa_baton_animation_0A594_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0A594Records[269] = {
#include "assets/tonfa_baton_animation_0A594_records.inc"
};

static u16 _gTonfaBatonAnimation0A594Indices[20] = {
#include "assets/tonfa_baton_animation_0A594_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0A594 = {
    _gTonfaBatonAnimation0A594Records,
    _gTonfaBatonAnimation0A594Indices,
    { NULL, _gTonfaBatonAnimation0A594Bank1, NULL, NULL, _gTonfaBatonAnimation0A594Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0AE50Bank1[15] = {
#include "assets/tonfa_baton_animation_0AE50_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0AE50Bank4[203] = {
#include "assets/tonfa_baton_animation_0AE50_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0AE50Records[291] = {
#include "assets/tonfa_baton_animation_0AE50_records.inc"
};

static u16 _gTonfaBatonAnimation0AE50Indices[20] = {
#include "assets/tonfa_baton_animation_0AE50_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0AE50 = {
    _gTonfaBatonAnimation0AE50Records,
    _gTonfaBatonAnimation0AE50Indices,
    { NULL, _gTonfaBatonAnimation0AE50Bank1, NULL, NULL, _gTonfaBatonAnimation0AE50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0B5C8Bank1[16] = {
#include "assets/tonfa_baton_animation_0B5C8_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0B5C8Bank4[170] = {
#include "assets/tonfa_baton_animation_0B5C8_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0B5C8Records[240] = {
#include "assets/tonfa_baton_animation_0B5C8_records.inc"
};

static u16 _gTonfaBatonAnimation0B5C8Indices[20] = {
#include "assets/tonfa_baton_animation_0B5C8_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0B5C8 = {
    _gTonfaBatonAnimation0B5C8Records,
    _gTonfaBatonAnimation0B5C8Indices,
    { NULL, _gTonfaBatonAnimation0B5C8Bank1, NULL, NULL, _gTonfaBatonAnimation0B5C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0BC08Bank1[11] = {
#include "assets/tonfa_baton_animation_0BC08_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0BC08Bank4[154] = {
#include "assets/tonfa_baton_animation_0BC08_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0BC08Records[193] = {
#include "assets/tonfa_baton_animation_0BC08_records.inc"
};

static u16 _gTonfaBatonAnimation0BC08Indices[20] = {
#include "assets/tonfa_baton_animation_0BC08_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0BC08 = {
    _gTonfaBatonAnimation0BC08Records,
    _gTonfaBatonAnimation0BC08Indices,
    { NULL, _gTonfaBatonAnimation0BC08Bank1, NULL, NULL, _gTonfaBatonAnimation0BC08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0C424Bank1[15] = {
#include "assets/tonfa_baton_animation_0C424_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0C424Bank4[206] = {
#include "assets/tonfa_baton_animation_0C424_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0C424Records[248] = {
#include "assets/tonfa_baton_animation_0C424_records.inc"
};

static u16 _gTonfaBatonAnimation0C424Indices[20] = {
#include "assets/tonfa_baton_animation_0C424_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0C424 = {
    _gTonfaBatonAnimation0C424Records,
    _gTonfaBatonAnimation0C424Indices,
    { NULL, _gTonfaBatonAnimation0C424Bank1, NULL, NULL, _gTonfaBatonAnimation0C424Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gTonfaBatonAnimation0D42CBank1[30] = {
#include "assets/tonfa_baton_animation_0D42C_bank1.inc"
};

static AnimationPackedRotation _gTonfaBatonAnimation0D42CBank4[424] = {
#include "assets/tonfa_baton_animation_0D42C_bank4.inc"
};

static AnimationRecord _gTonfaBatonAnimation0D42CRecords[492] = {
#include "assets/tonfa_baton_animation_0D42C_records.inc"
};

static u16 _gTonfaBatonAnimation0D42CIndices[20] = {
#include "assets/tonfa_baton_animation_0D42C_indices.inc"
};

static AnimationSet _gTonfaBatonAnimation0D42C = {
    _gTonfaBatonAnimation0D42CRecords,
    _gTonfaBatonAnimation0D42CIndices,
    { NULL, _gTonfaBatonAnimation0D42CBank1, NULL, NULL, _gTonfaBatonAnimation0D42CBank4, NULL, NULL, NULL },
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
    &_gTonfaBatonAnimation015E0,
    &_gTonfaBatonAnimation01C84,
    &_gTonfaBatonAnimation0E194,
    &_gTonfaBatonAnimation0E8C8,
    &_gTonfaBatonAnimation024E4,
    &_gTonfaBatonAnimation02D48,
    &_gTonfaBatonAnimation0D6F4,
    &_gTonfaBatonAnimation0D8BC,
    &_gTonfaBatonAnimation09D78,
    &_gTonfaBatonAnimation0BC08,
    &_gTonfaBatonAnimation0D42C,
    &_gTonfaBatonAnimation0AE50,
    &_gTonfaBatonAnimation0A594,
    &_gTonfaBatonAnimation0C424,
    &_gTonfaBatonAnimation0C424,
    &_gTonfaBatonAnimation059AC,
    &_gTonfaBatonAnimation05CCC,
    &_gTonfaBatonAnimation06490,
    &_gTonfaBatonAnimation01C84,
    &_gTonfaBatonAnimation0B5C8,
    &_gTonfaBatonAnimation015E0,
    &_gTonfaBatonAnimation015E0,
    &_gTonfaBatonAnimation07728,
    &_gTonfaBatonAnimation089BC,
    &_gTonfaBatonAnimation082A0,
    &_gTonfaBatonAnimation04A2C,
    &_gTonfaBatonAnimation04C28,
    &_gTonfaBatonAnimation04F00,
    &_gTonfaBatonAnimation051A4,
    &_gTonfaBatonAnimation053A4,
    &_gTonfaBatonAnimation056F8,
    &_gTonfaBatonAnimation08E3C,
    &_gTonfaBatonAnimation09014,
    &_gTonfaBatonAnimation08E3C,
    &_gTonfaBatonAnimation09014,
    &_gTonfaBatonAnimation037B8,
    &_gTonfaBatonAnimation03F40,
    &_gTonfaBatonAnimation045A4,
    &_gTonfaBatonAnimation04214,
    &_gTonfaBatonAnimation0305C,
    &_gTonfaBatonAnimation015E0,
    &_gTonfaBatonAnimation09578,
    &_gTonfaBatonAnimation0976C,
    &_gTonfaBatonAnimation09BEC,
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
