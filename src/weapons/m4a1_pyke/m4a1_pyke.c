#include "weapons/m4a1_pyke.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"
#include "weapons/sounds.h"

#include "../../shared/projectiles/flamethrower_flame.h"
#include "../shared/attack.h"

/// Nozzle glow and flame-launch origin relative to the held weapon's root.
///
/// XYZ are signed integer model-coordinate units, copied into the effect's
/// local translation beneath the weapon coordinate; this is not a world offset.
static SVECTOR _gM4a1PykeNozzleOffset = { 0, 512, 64, 0 };

void m4a1PykeNozzleTask(Task* task)
{
    enum {
        M4A1_PYKE_NOZZLE_STATE_INIT   = 0,
        M4A1_PYKE_NOZZLE_STATE_UPDATE = 1,
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
        case M4A1_PYKE_NOZZLE_STATE_INIT:
            nozzleCoord->parent = effectWork->parent;
            gfxSetRotIdentity(&nozzleCoord->coord);
            nozzleCoord->coord.t[0]   = _gM4a1PykeNozzleOffset.vx;
            nozzleCoord->coord.t[1]   = _gM4a1PykeNozzleOffset.vy;
            nozzleCoord->coord.t[2]   = _gM4a1PykeNozzleOffset.vz;
            nozzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(nozzleCoord);
            task->state = M4A1_PYKE_NOZZLE_STATE_UPDATE;
            break;
        case M4A1_PYKE_NOZZLE_STATE_UPDATE:
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
                    transientLight->framesLeft = M4A1_PYKE_NOZZLE_LIGHT_FRAMES;
                    pointLight->inner          = M4A1_PYKE_NOZZLE_IDLE_LIGHT_INNER;
                    pointLight->outer          = M4A1_PYKE_NOZZLE_IDLE_LIGHT_OUTER;
                    lightRandom                = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState            = lightRandom;
                    // Green halves the unsigned red halfword; blue quarters its signed value.
                    pointLight->head.color.r = ((lightRandom >> 16) & M4A1_PYKE_NOZZLE_LIGHT_RANDOM_MASK) + M4A1_PYKE_NOZZLE_IDLE_RED_MIN;
                    pointLight->head.color.g = (u16)pointLight->head.color.r >> 1;
                    pointLight->head.color.b = pointLight->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &nozzleCoord->workm, &lightCoord->coord);
                    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    effectWork->scale        = M4A1_PYKE_NOZZLE_SPEED_STEP;
                    break;
                case M4A1_PYKE_NOZZLE_FIRE:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        effectWork->age--;
                        break;
                    }
                    actorRenderComposeCoord(nozzleCoord);
                    if (effectWork->scale < M4A1_PYKE_NOZZLE_SPEED_MAX) {
                        effectWork->scale = effectWork->scale + M4A1_PYKE_NOZZLE_SPEED_STEP;
                    }
                    flameWork = effectSpawn(EFFECT_M4A1_PYKE_FLAME, nozzleCoord, (s32)effectWork->scale, NULL);
                    if (flameWork != NULL) {
                        taskReparent(task, flameWork->task);
                    }
                    transientLight->framesLeft = M4A1_PYKE_NOZZLE_LIGHT_FRAMES;
                    pointLight->inner          = M4A1_PYKE_NOZZLE_FIRE_LIGHT_INNER;
                    pointLight->outer          = M4A1_PYKE_NOZZLE_FIRE_LIGHT_OUTER;
                    lightRandom                = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState            = lightRandom;
                    pointLight->head.color.r   = ((lightRandom >> 16) & M4A1_PYKE_NOZZLE_LIGHT_RANDOM_MASK) + M4A1_PYKE_NOZZLE_FIRE_RED_MIN;
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
#include "../../shared/projectiles/flamethrower_flame.inc.c"

DEFINE_ALIAS(_flamethrowerFlameTask, m4a1PykeFlameTask);

#include "assets/engine/animation/player_schema.h"

void m4a1PykeAttackState(Task* playerTask)
{
    enum {
        M4A1_PYKE_PHASE_PREPARE       = 0,
        M4A1_PYKE_PHASE_WAIT_READY    = 1,
        M4A1_PYKE_PHASE_SELECT_ATTACK = 2,
        M4A1_PYKE_PHASE_BURST         = 3,
        M4A1_PYKE_PHASE_IMPACT        = 4,
        M4A1_PYKE_PHASE_FLAME         = 5,
        M4A1_PYKE_PHASE_RECOVER       = 6,
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
        case M4A1_PYKE_PHASE_PREPARE:
            readyBlendFrames                                      = M4A1_PYKE_READY_BLEND_FRAMES;
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_PYKE_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case M4A1_PYKE_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_PYKE_PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase        = M4A1_PYKE_PHASE_BURST;
                actor->turnRateIndex     = 0;
                actor->stateTimer        = 0;
                actor->attackCancelTicks = M4A1_PYKE_BURST_CANCEL_FRAMES;
                actor->actionValue       = M4A1_PYKE_BURST_ROUNDS;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                nozzleTask                         = actor->weaponEffectTask;
                actor->statePhase                  = M4A1_PYKE_PHASE_FLAME;
                actor->turnRateIndex               = M4A1_PYKE_SECONDARY_TURN_RATE_INDEX;
                actor->attackControl.cooldownTicks = M4A1_PYKE_FLAME_COOLDOWN_FRAMES;
                actor->attackCancelTicks           = M4A1_PYKE_SECONDARY_CANCEL_FRAMES;
                actor->actionValue                 = M4A1_PYKE_FUEL_INTERVAL_FRAMES;
                if (nozzleTask != NULL) {
                    nozzleTask->spawnArg1.value = M4A1_PYKE_NOZZLE_FIRE;
                }
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_PYKE), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_PYKE_SOUND(M4A1_PYKE_SOUND_FLAME), 1);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 2);
                break;
            }
            /* fallthrough */
        case M4A1_PYKE_PHASE_BURST:
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M4A1_PYKE_BURST_DELAY_FRAMES;
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
                if (burstDelay - 1 == M4A1_PYKE_BURST_IMPACT_TICKS_LEFT) {
                    _weaponResolveShotImpact(actor, rootCoord, impactCoord);
                }
                break;
            }
            /* fallthrough */
        case M4A1_PYKE_PHASE_IMPACT:
            actor->statePhase = M4A1_PYKE_PHASE_RECOVER;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            break;
        case M4A1_PYKE_PHASE_FLAME:
            // Sustain the nozzle in fuel intervals until secondary input or fuel runs out.
            if (actor->actionValue == 0) {
                remainingFuel = playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_SECONDARY);
                if (playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY && remainingFuel != 0) {
                    actor->actionValue = M4A1_PYKE_FUEL_INTERVAL_FRAMES;
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_PYKE), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                } else {
                    nozzleTask        = actor->weaponEffectTask;
                    actor->statePhase = M4A1_PYKE_PHASE_RECOVER;
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
        case M4A1_PYKE_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_PYKE_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The model `gM4a1PykeWeaponModel`: generated from its asset (weapons/m4a1_pyke/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m4a1_pyke/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m4a1_pyke/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m4a1_pyke/clips.inc"
#include "assets/weapons/m4a1_pyke/bank.inc"
