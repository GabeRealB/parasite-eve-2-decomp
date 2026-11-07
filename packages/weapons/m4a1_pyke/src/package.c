#include "m4a1_pyke.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include <core/pe2.h>

#include <gameplay/display.h>
#include <gameplay/actor.h>
#include <gameplay/actor_render.h>
#include <gameplay/effect_tasks.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/items.h>
#include <gameplay/light.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>

#include <engine/coord.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

#include <projectiles/flamethrower_flame/flamethrower_flame.h>

#include <weapons/weapon/weapon.h>
#include <weapons/weapon/sounds.h>
#include <weapons/attack/attack.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(BURST_ROUNDS, int, 3)
PE2_OPTION_DEF(BURST_CANCEL_FRAMES, int, 9)
PE2_OPTION_DEF(SECONDARY_CANCEL_FRAMES, int, 0x1C)
PE2_OPTION_DEF(BURST_DELAY_FRAMES, int, 3)
PE2_OPTION_DEF(RECOVERY_COOLDOWN_FRAMES, int, 0xC)
PE2_OPTION_DEF(FUEL_INTERVAL_FRAMES, int, 0x14)
PE2_OPTION_DEF(FLAME_COOLDOWN_FRAMES, int, 0x28)
PE2_OPTION_DEF(BURST_IMPACT_FRAMES_LEFT, int, 2)
/// The nozzle's glow, flame emission and light (`m4a1PykeNozzleTask`): times in dispatch ticks, light radii in game units.
PE2_OPTION_DEF(NOZZLE_LIGHT_FRAMES, int, 4)
PE2_OPTION_DEF(NOZZLE_SPEED_STEP, int, 64)
PE2_OPTION_DEF(NOZZLE_SPEED_MAX, int, 384)
PE2_OPTION_DEF(NOZZLE_IDLE_LIGHT_INNER, int, 128)
PE2_OPTION_DEF(NOZZLE_IDLE_LIGHT_OUTER, int, 1024)
PE2_OPTION_DEF(NOZZLE_FIRE_LIGHT_INNER, int, 1024)
PE2_OPTION_DEF(NOZZLE_FIRE_LIGHT_OUTER, int, 16384)
PE2_OPTION_DEF(NOZZLE_IDLE_RED_MIN, int, 0x400)
PE2_OPTION_DEF(NOZZLE_FIRE_RED_MIN, int, 0x800)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gM4a1PykeWeaponModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gM4a1PykePlayerAnimationBank)

/// Attack tuning and ids; times in dispatch ticks.
enum {
    SECONDARY_TURN_RATE_INDEX = 2,
};

/// The nozzle's glow, flame emission and light (`m4a1PykeNozzleTask`): times in dispatch ticks, light radii in game units.
enum {
    NOZZLE_LIGHT_RANDOM_MASK = 0x700,
};

/// Nozzle glow and flame-launch origin relative to the held weapon's root.
///
/// XYZ are signed integer model-coordinate units, copied into the effect's
/// local translation beneath the weapon coordinate; this is not a world offset.
static SVECTOR _gNozzleOffset = { 0, 512, 64, 0 };

void m4a1PykeNozzleTask(Task* task)
{
    enum {
        NOZZLE_STATE_INIT   = 0,
        NOZZLE_STATE_UPDATE = 1,
    };
    EffectWork*                    effectWork;
    GfxCoord*                      nozzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    GfxCoord*                      lightCoord;
    EffectWork*                    flameWork;
    u32                            lightRandom;

    effectWork     = task->spawnArg2.pointer;
    nozzleCoord    = task->extra.coordBody->coord;
    transientLight = &gWorldCoordTransientPointLights[1];
    lightCoord     = &transientLight->light.head.transform.coord;
    pointLight     = &transientLight->light;
    if ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return;
    }
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }
    effectWork->age++;
    switch (task->state) {
        case NOZZLE_STATE_INIT:
            nozzleCoord->parent = effectWork->parent;
            gfxSetRotIdentity(&nozzleCoord->coord);
            nozzleCoord->coord.t[0]   = _gNozzleOffset.vx;
            nozzleCoord->coord.t[1]   = _gNozzleOffset.vy;
            nozzleCoord->coord.t[2]   = _gNozzleOffset.vz;
            nozzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(nozzleCoord);
            task->state = NOZZLE_STATE_UPDATE;
            break;
        case NOZZLE_STATE_UPDATE:
            switch (task->spawnArg1.value) {
                case M4A1_PYKE_NOZZLE_OFF:
                    break;
                case M4A1_PYKE_NOZZLE_IDLE:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        effectWork->age--;
                        _flamethrowerFlameDrawNozzle(
                            GRAPHICS_MATRIX_TRANSLATION(&nozzleCoord->workm), effectWork->age, FLAMETHROWER_FLAME_NOZZLE_SIZE_SCALE);
                        break;
                    }
                    actorRenderComposeCoord(nozzleCoord);
                    _flamethrowerFlameDrawNozzle(GRAPHICS_MATRIX_TRANSLATION(&nozzleCoord->workm), effectWork->age, FLAMETHROWER_FLAME_NOZZLE_SIZE_SCALE);
                    transientLight->framesLeft = PE2_OPTION(NOZZLE_LIGHT_FRAMES);
                    pointLight->inner          = PE2_OPTION(NOZZLE_IDLE_LIGHT_INNER);
                    pointLight->outer          = PE2_OPTION(NOZZLE_IDLE_LIGHT_OUTER);
                    lightRandom                = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState            = lightRandom;
                    // Green halves the unsigned red halfword; blue quarters its signed value.
                    pointLight->head.color.r = ((lightRandom >> 16) & NOZZLE_LIGHT_RANDOM_MASK) + PE2_OPTION(NOZZLE_IDLE_RED_MIN);
                    pointLight->head.color.g = (u16)pointLight->head.color.r >> 1;
                    pointLight->head.color.b = pointLight->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &nozzleCoord->workm, &lightCoord->coord);
                    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    effectWork->scale        = PE2_OPTION(NOZZLE_SPEED_STEP);
                    break;
                case M4A1_PYKE_NOZZLE_FIRE:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        effectWork->age--;
                        break;
                    }
                    actorRenderComposeCoord(nozzleCoord);
                    if (effectWork->scale < PE2_OPTION(NOZZLE_SPEED_MAX)) {
                        effectWork->scale = effectWork->scale + PE2_OPTION(NOZZLE_SPEED_STEP);
                    }
                    flameWork = effectSpawn(EFFECT_M4A1_PYKE_FLAME, nozzleCoord, (s32)effectWork->scale, NULL);
                    if (flameWork != NULL) {
                        taskReparent(task, flameWork->task);
                    }
                    transientLight->framesLeft = PE2_OPTION(NOZZLE_LIGHT_FRAMES);
                    pointLight->inner          = PE2_OPTION(NOZZLE_FIRE_LIGHT_INNER);
                    pointLight->outer          = PE2_OPTION(NOZZLE_FIRE_LIGHT_OUTER);
                    lightRandom                = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState            = lightRandom;
                    pointLight->head.color.r   = ((lightRandom >> 16) & NOZZLE_LIGHT_RANDOM_MASK) + PE2_OPTION(NOZZLE_FIRE_RED_MIN);
                    pointLight->head.color.g   = (u16)pointLight->head.color.r >> 1;
                    pointLight->head.color.b   = pointLight->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &nozzleCoord->workm, &lightCoord->coord);
                    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                case M4A1_PYKE_NOZZLE_RESET_IDLE:
                    task->spawnArg1.value = M4A1_PYKE_NOZZLE_IDLE;
                    break;
                case M4A1_PYKE_NOZZLE_RESET_OFF:
                    task->spawnArg1.value = M4A1_PYKE_NOZZLE_OFF;
                    break;
                case M4A1_PYKE_NOZZLE_RELEASE:
                    effectKillTask(effectWork, task);
                    break;
            }
            break;
    }
}

/// The flying flame's collision key: the player's, so player-only weapon damage bonuses apply.
enum { FLAMETHROWER_FLAME_ATTACK_KEY = FLAMETHROWER_FLAME_KEY };

/// Retains this weapon's discarded splash-depth increment before the GTE store.
///
/// The splash fragment consumes this 0/1 binding; its default selects the
/// companion's effective increment after the store instead.
#define FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE 1
#include <projectiles/flamethrower_flame/flamethrower_flame.inc.c>

DEFINE_ALIAS(_flamethrowerFlameTask, m4a1PykeFlameTask);

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

void m4a1PykeAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE       = 0,
        PHASE_WAIT_READY    = 1,
        PHASE_SELECT_ATTACK = 2,
        PHASE_BURST         = 3,
        PHASE_IMPACT        = 4,
        PHASE_FLAME         = 5,
        PHASE_RECOVER       = 6,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    Task*      nozzleTask;
    s32        readyBlendFrames;
    s32        burstDelay;
    s32        remainingFuel;

    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            readyBlendFrames                                      = PE2_OPTION(READY_BLEND_FRAMES);
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PE2_OPTION(MOVING_READY_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase        = PHASE_BURST;
                actor->turnRateIndex     = 0;
                actor->stateTimer        = 0;
                actor->attackCancelTicks = PE2_OPTION(BURST_CANCEL_FRAMES);
                actor->actionValue       = PE2_OPTION(BURST_ROUNDS);
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                nozzleTask                         = actor->weaponEffectTask;
                actor->statePhase                  = PHASE_FLAME;
                actor->turnRateIndex               = SECONDARY_TURN_RATE_INDEX;
                actor->attackControl.cooldownTicks = PE2_OPTION(FLAME_COOLDOWN_FRAMES);
                actor->attackCancelTicks           = PE2_OPTION(SECONDARY_CANCEL_FRAMES);
                actor->actionValue                 = PE2_OPTION(FUEL_INTERVAL_FRAMES);
                if (nozzleTask != NULL) {
                    nozzleTask->spawnArg1.value = M4A1_PYKE_NOZZLE_FIRE;
                }
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_PYKE), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_PYKE_SOUND(M4A1_PYKE_SOUND_FLAME), 1);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 2);
                break;
            }
            /* fallthrough */
        case PHASE_BURST:
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = PE2_OPTION(BURST_DELAY_FRAMES);
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_PYKE), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_PYKE_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX_M4A1_PYKE, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                    break;
                }
                actor->stateTimer = burstDelay - 1;
                if (burstDelay - 1 == PE2_OPTION(BURST_IMPACT_FRAMES_LEFT)) {
                    _weaponResolveShotImpact(actor, rootCoord, impactCoord);
                }
                break;
            }
            /* fallthrough */
        case PHASE_IMPACT:
            actor->statePhase = PHASE_RECOVER;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            break;
        case PHASE_FLAME:
            // Sustain the nozzle in fuel intervals until secondary input or fuel runs out.
            if (actor->actionValue == 0) {
                remainingFuel = playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_SECONDARY);
                if (playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY && remainingFuel != 0) {
                    actor->actionValue = PE2_OPTION(FUEL_INTERVAL_FRAMES);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_PYKE), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                } else {
                    nozzleTask        = actor->weaponEffectTask;
                    actor->statePhase = PHASE_RECOVER;
                    if (nozzleTask != NULL) {
                        nozzleTask->spawnArg1.value = remainingFuel != 0 ? M4A1_PYKE_NOZZLE_RESET_IDLE : M4A1_PYKE_NOZZLE_RESET_OFF;
                    }
                    sndEvtRequestScriptStop(M4A1_PYKE_SOUND(M4A1_PYKE_SOUND_FLAME), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_RELOAD, 0, 2);
                }
            } else {
                actor->actionValue = (u16)actor->actionValue - 1;
            }
            break;
        case PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = PE2_OPTION(RECOVERY_COOLDOWN_FRAMES);
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
