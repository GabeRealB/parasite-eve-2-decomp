#include "tonfa_baton.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include <core/pe2.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/message.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_collision.h>
#include <gameplay/player_animation.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/gfx.h>
#include <engine/pad.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/weapon.h>
#include <weapons/effects/blade_trail/blade_trail.h>
#include <weapons/weapon/weapon_index.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(WINDUP_FRAMES, int, 8)
PE2_OPTION_DEF(COMBO_ADVANCE_FRAMES, int, 0xC)
PE2_OPTION_DEF(RECOVERY_ADVANCE_FRAMES, int, 0xA)
PE2_OPTION_DEF(COMBO_CONTACT_FRAMES, int, 9)
PE2_OPTION_DEF(COMBO_CONTACT_START_FRAMES, int, 3)
/// Divisor that turns the model's forward axis into the distance a baton
/// strike carries the actor per frame: 4096 / 84, about 48 coordinate units.
PE2_OPTION_DEF(ATTACK_ADVANCE_DIVISOR, int, 84)
/// The swing's ribbon trail (`tonfaBatonSwingTrailTask`): times in dispatch ticks, tints as RGB444.
PE2_OPTION_DEF(TRAIL_LIFETIME_FRAMES, int, 31)
/// RGB multipliers (0, 0, 1).
PE2_OPTION_DEF(TRAIL_TINT_BLUE, int, 0x001)
/// RGB multipliers (0, 1, 3).
PE2_OPTION_DEF(TRAIL_TINT_CYAN, int, 0x013)
/// The baton's swing pose (`tonfaBatonModelTask`): steps in 4096 per turn, offsets in game units.
PE2_OPTION_DEF(POSE_RETURN_STEP, int, 0x100)
PE2_OPTION_DEF(POSE_STRIKE_STEP, int, 0x1C0)
PE2_OPTION_DEF(GRIP_OFFSET_Y, int, 0x60)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gTonfaBatonWeaponModel)

PE2_BIND(shaft_model, model)
PE2_BIND_ARG(shaft_model, _symbol, gTonfaBatonShaftModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gTonfaBatonPlayerAnimationBank)

/// A sound of the Tonfa Baton's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_TONFA_BATON, (slot))

/// The Tonfa Baton's own slots.
enum {
    SOUND_FIRST_SWING   = 1,
    SOUND_COMBO_SWING   = 2,
    SOUND_FIRST_CONTACT = 3,
    SOUND_COMBO_CONTACT = 4,
};

/// The contacts' keys and what else is wiring.
enum {
    FIRST_STRIKE_KEY = 0x21317,
    COMBO_STRIKE_KEY = 0x21315,
};

// Defined at the end of the file, after the package's model and bank.
/// The eight-segment swing trails, one array per end of the baton. Every entry
/// is parented to `gGfxViewCoord`.

/// One-based weapon index passed to the resident use counter.

/// Scratch-stack block for the tonfa baton's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages the forward step a strike
/// moves the actor by.
///
/// `forward` is read from the root coordinate's local matrix and is not
/// normalized. `advance` is each component of it divided by
/// `PE2_OPTION(ATTACK_ADVANCE_DIVISOR)` and multiplied by 1 on a frame a
/// strike carries the actor forward, 0 on any other. It is added to the root
/// coordinate's local translation in every state. Neither vector's `pad` is
/// written.
typedef struct {
    VECTOR  advance; // This frame's displacement of the model's root coordinate, in coordinate units; zero while no strike carries the actor
    SVECTOR forward; // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _AttackScratch;
STATIC_ASSERT_SIZEOF(_AttackScratch, 0x18);

enum {
    TRAIL_ENDPOINT_BASE  = 0,
    TRAIL_ENDPOINT_TIP   = 1,
    TRAIL_ENDPOINT_COUNT = 2
};

/// Base and tip translations for the baton swing ribbon, in integer game coordinates.
///
/// The base is (0, 128, 0) in the weapon frame. Seeding chains the tip's
/// (0, -512, 0) translation below that base; subsequent samples apply it in
/// the weapon frame directly. Only XYZ are read; each vector retains its pad.
static SVECTOR _gTrailEndpointOffsets[TRAIL_ENDPOINT_COUNT] = {
    { 0, 0x0080, 0, 0 },
    { 0, -0x0200, 0, 0 },
};

/// Its model, its shaft's and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(shaft_model)
PE2_VALUE(bank)

// Pose requests shared by the argument reset and the angle step.
enum {
    POSE_REST   = 0,
    POSE_STRIKE = 1
};

static GfxCoord _gBladeTrailFirstHistory[8]  = { 0 };
static GfxCoord _gBladeTrailSecondHistory[8] = { 0 };

/// Packed RGB multipliers retained for the tonfa baton's swing ribbon.
///
/// Red occupies bits 8..9, green 4..5 and blue 0..1; all other bits are zero.
/// Initially zero when the overlay loads. Seeding selects 0x001 (blue:
/// 0, 0, 1) when `Task::spawnArg1.value` is zero or 0x013 (cyan: 0, 1, 3)
/// otherwise. The tint persists between dispatches and is shared by
/// simultaneous trail tasks until another seed replaces it.
static s16 _gTrailTint = 0;

static void _killModelTask(Task* task);

void tonfaBatonSwingTrailTask(Task* task)
{
    enum {
        TRAIL_SEED   = 0,
        TRAIL_RECORD = 1,
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
            case TRAIL_SEED:
                baseCoord->parent       = effectWork->parent;
                baseCoord->coord.t[0]   = _gTrailEndpointOffsets[TRAIL_ENDPOINT_BASE].vx;
                baseCoord->coord.t[1]   = _gTrailEndpointOffsets[TRAIL_ENDPOINT_BASE].vy;
                baseCoord->coord.t[2]   = _gTrailEndpointOffsets[TRAIL_ENDPOINT_BASE].vz;
                baseCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(baseCoord);
                task->state = TRAIL_RECORD;

                // The seed chains the tip below the base; later samples use the weapon parent.
                // Only translations feed the ribbon; the temporary rotation is left uninitialized.
                seedTipOffset            = &_gTrailEndpointOffsets[TRAIL_ENDPOINT_TIP];
                tipEndpoint.parent       = baseCoord;
                tipEndpoint.coord.t[0]   = seedTipOffset->vx;
                tipEndpoint.coord.t[1]   = seedTipOffset->vy;
                tipEndpoint.coord.t[2]   = seedTipOffset->vz;
                tipEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&tipEndpoint);

                // Seed every slot with the same pose, detached from the moving baton.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gBladeTrailFirstHistory); historyIndex++) {
                    historyFrame = &_gBladeTrailFirstHistory[historyIndex];
                    _bladeTrailStoreFrame(historyFrame, baseCoord);
                    historyFrame = &_gBladeTrailSecondHistory[historyIndex];
                    _bladeTrailStoreFrame(historyFrame, &tipEndpoint);
                }
                packedTint = PE2_OPTION(TRAIL_TINT_CYAN);
                if (task->spawnArg1.value == 0) {
                    packedTint = PE2_OPTION(TRAIL_TINT_BLUE);
                }
                _gTrailTint = packedTint;
                break;
            case TRAIL_RECORD:
                baseCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(baseCoord);
                tipEndpoint.parent       = effectWork->parent;
                recordTipOffset          = &_gTrailEndpointOffsets[TRAIL_ENDPOINT_TIP];
                tipEndpoint.coord.t[0]   = recordTipOffset->vx;
                tipEndpoint.coord.t[1]   = recordTipOffset->vy;
                tipEndpoint.coord.t[2]   = recordTipOffset->vz;
                tipEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&tipEndpoint);
                historyFrame = &_gBladeTrailFirstHistory[effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1)];
                _bladeTrailStoreFrame(historyFrame, baseCoord);
                historyFrame = &_gBladeTrailSecondHistory[effectWork->age & (ARRAY_SIZE(_gBladeTrailSecondHistory) - 1)];
                _bladeTrailStoreFrame(historyFrame, &tipEndpoint);

                // Recompose retained world poses against the current camera before drawing.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gBladeTrailFirstHistory); historyIndex++) {
                    historyFrame               = &_gBladeTrailFirstHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &_gBladeTrailSecondHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1), _gTrailTint);
                break;
        }
        if (effectWork->age >= PE2_OPTION(TRAIL_LIFETIME_FRAMES)) {
            effectKillTask(effectWork, task);
        }
    }
}

#include <weapons/effects/blade_trail/draw.inc.c>

/// Starts the attached baton model's pose updates and installs default teardown.
///
/// State 0 requires a live TMD body with an initialized root coordinate whose
/// parent was supplied by the spawner. Clears the model flags on this first
/// dispatch; the next dispatch mirrors the player's flags.
static void _initModelTask(Task* task)
{
    TmdObject* model;
    GfxCoord*  rootCoord;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    task->state++;
    task->exitCallback      = _killModelTask;
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
static inline void _stepModelPose(GfxCoord* rootCoord, s32 poseRequest)
{

    switch (poseRequest) {
        case POSE_REST:
            if (rootCoord->param.rot.vz > 0) {
                rootCoord->param.rot.vz -= PE2_OPTION(POSE_RETURN_STEP);
            }
            break;
        case POSE_STRIKE:
            if (rootCoord->param.rot.vz < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                rootCoord->param.rot.vz += PE2_OPTION(POSE_STRIKE_STEP);
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
static void _updateModelPose(Task* task)
{
    enum {
        POSE_MODE_MASK = 0xF,
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
    rootCoord->coord.t[1] = PE2_OPTION(GRIP_OFFSET_Y);
    rootCoord->coord.t[2] = 0;

    if (playerActor->mode != GAME_ACTOR_MODE_NORMAL || playerActor->state != PLAYER_ACTOR_STATE_ATTACK) {
        task->spawnArg1.value = POSE_REST;
    }

    poseMode = task->spawnArg1.value & POSE_MODE_MASK;
    _stepModelPose(rootCoord, poseMode);
}

/// Advances state 2 to teardown on the next model-task dispatch.
static void _deferModelTaskRemoval(Task* task)
{
    enum { MODEL_STATE_KILL = 3 };

    task->state = MODEL_STATE_KILL;
}

/// Hands the live baton model task to default teardown, as state 3 or on exit.
static void _killModelTask(Task* task)
{
    taskKill(task);
}

void tonfaBatonModelTask(Task* task)
{
    TaskFunc stateHandlers[] = {
        _initModelTask,
        _updateModelPose,
        _deferModelTaskRemoval,
        _killModelTask,
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
static inline void _advanceAttack(GfxCoord* rootCoord, _AttackScratch* scratch, s32 advanceThisFrame)
{
    gfxReadMatrixZAxis(&rootCoord->coord, &scratch->forward);
    scratch->advance.vx    = (s16)(scratch->forward.vx / PE2_OPTION(ATTACK_ADVANCE_DIVISOR)) * advanceThisFrame;
    scratch->advance.vy    = (s16)(scratch->forward.vy / PE2_OPTION(ATTACK_ADVANCE_DIVISOR)) * advanceThisFrame;
    scratch->advance.vz    = (s16)(scratch->forward.vz / PE2_OPTION(ATTACK_ADVANCE_DIVISOR)) * advanceThisFrame;
    rootCoord->coord.t[0] += scratch->advance.vx;
    rootCoord->coord.t[1] += scratch->advance.vy;
    rootCoord->coord.t[2] += scratch->advance.vz;
}

void tonfaBatonAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE       = 0,
        PHASE_FIRST_STRIKE  = 1,
        PHASE_COMBO_QUEUED  = 2,
        PHASE_COMBO_STRIKE  = 3,
        PHASE_COMBO_RECOVER = 4,
        PHASE_RECOVER       = 5,
    };
    GameActor*      actor;
    GfxCoord*       rootCoord;
    _AttackScratch* scratch;
    EffectWork*     trailWork;
    s32             windupTicksLeft;
    s32             comboTicksLeft;
    s32             contactTicksLeft;
    s32             advanceThisFrame;

    advanceThisFrame = 0;
    actor            = playerTask->work;
    scratch          = SCRATCH_STACK_RESERVE_BLOCK(_AttackScratch);
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->statePhase     = PHASE_FIRST_STRIKE;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = GAME_ACTOR_MOVEMENT_STOPPED;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->stateTimer     = PE2_OPTION(WINDUP_FRAMES);
            actor->actionValue    = 0;
            weaponRecordUse(WEAPON_INDEX_TONFA_BATON);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key   = FIRST_STRIKE_KEY;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT)) | WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 1, 3);
            break;
        case PHASE_FIRST_STRIKE:
        case PHASE_COMBO_QUEUED:
            windupTicksLeft = actor->stateTimer;
            if (windupTicksLeft == 0) {
                actor->movementSign = 1;
                advanceThisFrame    = 1;
                if (actor->padPressed & (PAD_BUTTON_R1 | PAD_BUTTON_R2)) {
                    actor->statePhase = PHASE_COMBO_QUEUED;
                }
            } else {
                windupTicksLeft--;
                actor->stateTimer = windupTicksLeft;
                if (windupTicksLeft == 0) {
                    actor->equipmentTasks[1]->spawnArg1.value             = POSE_STRIKE;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_FIRST_SWING), 0);
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
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_FIRST_CONTACT), 0);
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, 1, 0, 0) == 0) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (actor->statePhase == PHASE_COMBO_QUEUED) {
                    actor->statePhase = PHASE_COMBO_STRIKE;
                    actor->stateTimer = PE2_OPTION(COMBO_ADVANCE_FRAMES);
                    weaponRecordUse(WEAPON_INDEX_TONFA_BATON);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = COMBO_STRIKE_KEY;
                    trailWork                                          = effectSpawn(
                        EFFECT_TONFA_BATON_SWING_TRAIL, actor->equipmentTasks[1]->extra.tmd->coords, 1,
                        NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                    playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE);
                } else {
                    actor->statePhase                         = PHASE_RECOVER;
                    actor->stateTimer                         = PE2_OPTION(RECOVERY_ADVANCE_FRAMES);
                    actor->equipmentTasks[1]->spawnArg1.value = POSE_REST;
                    playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_RELOAD);
                }
            }
            break;
        case PHASE_COMBO_STRIKE:
            if (actor->stateTimer != 0) {
                actor->movementSign = 1;
                advanceThisFrame    = 1;
                comboTicksLeft      = actor->stateTimer - 1;
                actor->stateTimer   = comboTicksLeft;
                if (comboTicksLeft == PE2_OPTION(COMBO_CONTACT_START_FRAMES)) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_COMBO_SWING), 0);
                } else if (comboTicksLeft == 0) {
                    actor->statePhase                         = PHASE_COMBO_RECOVER;
                    actor->stateTimer                         = PE2_OPTION(COMBO_CONTACT_FRAMES);
                    actor->equipmentTasks[1]->spawnArg1.value = POSE_REST;
                }
            }
            /* fallthrough */
        case PHASE_COMBO_RECOVER:
            if (actor->statePhase == PHASE_COMBO_RECOVER) {
                contactTicksLeft = actor->stateTimer;
                contactTicksLeft--;
                actor->stateTimer = contactTicksLeft;
                if (contactTicksLeft == 0) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                }
            }
            if (actor->actionValue != 2 && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = 2;
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_COMBO_CONTACT), 0);
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, 1, 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
        case PHASE_RECOVER:
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
    _advanceAttack(rootCoord, scratch, advanceThisFrame);
    SCRATCH_STACK_RELEASE_BLOCK(_AttackScratch);
}
