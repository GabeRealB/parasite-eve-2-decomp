#include "weapons/m4a1_pyke.h"

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
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"

#include "../../shared/pyke_flame.h"

/// Translation of the Pyke's effect coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x200, 0x40)`.
static SVECTOR D_m4a1_pyke_8011E90C = { 0, 0x200, 0x40, 0 };

void m4a1PykeNozzleTask(Task* task)
{
    enum {
        M4A1_PYKE_NOZZLE_STATE_INIT        = 0,
        M4A1_PYKE_NOZZLE_STATE_UPDATE      = 1,
        M4A1_PYKE_NOZZLE_LIGHT_FRAMES      = 4,
        M4A1_PYKE_NOZZLE_SPEED_STEP        = 64,
        M4A1_PYKE_NOZZLE_SPEED_MAX         = 384,
        M4A1_PYKE_NOZZLE_IDLE_LIGHT_INNER  = 128,
        M4A1_PYKE_NOZZLE_IDLE_LIGHT_OUTER  = 1024,
        M4A1_PYKE_NOZZLE_FIRE_LIGHT_INNER  = 1024,
        M4A1_PYKE_NOZZLE_FIRE_LIGHT_OUTER  = 16384,
        M4A1_PYKE_NOZZLE_LIGHT_RANDOM_MASK = 0x700,
        M4A1_PYKE_NOZZLE_IDLE_RED_MIN      = 0x400,
        M4A1_PYKE_NOZZLE_FIRE_RED_MIN      = 0x800,
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
            nozzleCoord->coord.t[0]   = D_m4a1_pyke_8011E90C.vx;
            nozzleCoord->coord.t[1]   = D_m4a1_pyke_8011E90C.vy;
            nozzleCoord->coord.t[2]   = D_m4a1_pyke_8011E90C.vz;
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
                        _pykeFlameDrawNozzle(
                            MATRIX_TRANS(&nozzleCoord->workm), effectWork->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                        break;
                    }
                    actorRenderComposeCoord(nozzleCoord);
                    _pykeFlameDrawNozzle(MATRIX_TRANS(&nozzleCoord->workm), effectWork->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                    transientLight->framesLeft = M4A1_PYKE_NOZZLE_LIGHT_FRAMES;
                    pointLight->inner          = M4A1_PYKE_NOZZLE_IDLE_LIGHT_INNER;
                    pointLight->outer          = M4A1_PYKE_NOZZLE_IDLE_LIGHT_OUTER;
                    lightRandom                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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
                    lightRandom                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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

#include "../../shared/pyke_flame_nozzle.inc.c"

#define PYKE_FLAME_KEY 0x21C1E
#include "../../shared/pyke_flame_task.inc.c"

void m4a1PykeFlameTask(Task* task)
{
    _pykeFlameTask(task);
}

#include "../../shared/pyke_flame_blob.inc.c"

#define PYKE_FLAME_SPLASH_DEAD_BIAS 1
#include "../../shared/pyke_flame_splash.inc.c"

#include "../../shared/pyke_flame_release.inc.c"

void m4a1PykeAttackState(Task* playerTask)
{
    enum {
        M4A1_PYKE_PHASE_PREPARE             = 0,
        M4A1_PYKE_PHASE_WAIT_READY          = 1,
        M4A1_PYKE_PHASE_SELECT_ATTACK       = 2,
        M4A1_PYKE_PHASE_BURST               = 3,
        M4A1_PYKE_PHASE_IMPACT              = 4,
        M4A1_PYKE_PHASE_FLAME               = 5,
        M4A1_PYKE_PHASE_RECOVER             = 6,
        M4A1_PYKE_PLAYER_ATTACK_STATE       = 4,
        M4A1_PYKE_ANIMATION_READY           = 9,
        M4A1_PYKE_ANIMATION_PRIMARY         = 0xA,
        M4A1_PYKE_ANIMATION_SECONDARY       = 0xB,
        M4A1_PYKE_ANIMATION_RELEASE         = 0xF,
        M4A1_PYKE_READY_BLEND_FRAMES        = 1,
        M4A1_PYKE_MOVING_READY_BLEND_FRAMES = 8,
        M4A1_PYKE_IMPACT_SOUND              = SOUND_COMMON(0x17),
        M4A1_PYKE_BURST_ROUNDS              = 3,
        M4A1_PYKE_BURST_CANCEL_FRAMES       = 9,
        M4A1_PYKE_SECONDARY_CANCEL_FRAMES   = 0x1C,
        M4A1_PYKE_BURST_DELAY_FRAMES        = 3,
        M4A1_PYKE_RECOVERY_COOLDOWN_FRAMES  = 0xC,
        M4A1_PYKE_FUEL_INTERVAL_FRAMES      = 0x14,
        M4A1_PYKE_FLAME_COOLDOWN_FRAMES     = 0x28,
        M4A1_PYKE_WEAPON_ID                 = 28,
        M4A1_PYKE_PRIMARY_SOUND             = SOUND_WEAPON(SOUND_BANK_M4A1_PYKE, 4),
        M4A1_PYKE_FLAME_SOUND               = SOUND_PYKE_FIRE_TAIL,
        M4A1_PYKE_BURST_IMPACT_TICKS_LEFT   = 2,
        M4A1_PYKE_SECONDARY_TURN_RATE_INDEX = 2,
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
            actor->state                                          = M4A1_PYKE_PLAYER_ATTACK_STATE;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_PYKE_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M4A1_PYKE_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
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
                equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_PYKE_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_PYKE_FLAME_SOUND, 1);
                playerActorPlayChildSlotsWithBlend(playerTask, M4A1_PYKE_ANIMATION_SECONDARY, 0, 2);
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
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_PYKE_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_PYKE_PRIMARY_SOUND, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                M4A1_PYKE_WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_PYKE_ANIMATION_PRIMARY, 0, 2);
                    break;
                }
                actor->stateTimer = burstDelay - 1;
                if (burstDelay - 1 == M4A1_PYKE_BURST_IMPACT_TICKS_LEFT) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                        worldCoordPlaySound(impactCoord, M4A1_PYKE_IMPACT_SOUND, 1);
                    }
                }
                break;
            }
            /* fallthrough */
        case M4A1_PYKE_PHASE_IMPACT:
            actor->statePhase                                     = M4A1_PYKE_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, M4A1_PYKE_IMPACT_SOUND, 1);
            }
            break;
        case M4A1_PYKE_PHASE_FLAME:
            // Sustain the nozzle in fuel intervals until secondary input or fuel runs out.
            if (actor->actionValue == 0) {
                remainingFuel = playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_SECONDARY);
                if (playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY && remainingFuel != 0) {
                    actor->actionValue = M4A1_PYKE_FUEL_INTERVAL_FRAMES;
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_PYKE_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                } else {
                    nozzleTask        = actor->weaponEffectTask;
                    actor->statePhase = M4A1_PYKE_PHASE_RECOVER;
                    if (nozzleTask != NULL) {
                        nozzleTask->spawnArg1.value = remainingFuel != 0 ? M4A1_PYKE_NOZZLE_RESET_IDLE : M4A1_PYKE_NOZZLE_RESET_OFF;
                    }
                    sndEvtRequestScriptStop(SOUND_PYKE_FIRE_TAIL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_PYKE_ANIMATION_RELEASE, 0, 2);
                }
            } else {
                actor->actionValue = (u16)actor->actionValue - 1;
            }
            break;
        case M4A1_PYKE_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_PYKE_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

static TmdBone _gM4a1PykeModel01BCCSkeleton[1] = {
#include "assets/m4a1_pyke_model_01BCC_skeleton.inc"
};

static u32 _gM4a1PykeModel01BCCPartVerts[1] = {
#include "assets/m4a1_pyke_model_01BCC_partVerts.inc"
};

static SVECTOR _gM4a1PykeModel01BCCVerts[74] = {
#include "assets/m4a1_pyke_model_01BCC_verts.inc"
};

static SVECTOR _gM4a1PykeModel01BCCNormals[64] = {
#include "assets/m4a1_pyke_model_01BCC_normals.inc"
};

static u32 _gM4a1PykeModel01BCCStream[504] = {
#include "assets/m4a1_pyke_model_01BCC_stream.inc"
};

TmdSource D_m4a1_pyke_8011F56C = {
    0,
    3668,
    0,
    1,
    _gM4a1PykeModel01BCCPartVerts,
    _gM4a1PykeModel01BCCVerts,
    _gM4a1PykeModel01BCCNormals,
    _gM4a1PykeModel01BCCSkeleton,
    _gM4a1PykeModel01BCCStream,
};

static AnimationPackedPose _gM4a1PykeAnimation02560Bank1[2] = {
#include "assets/m4a1_pyke_animation_02560_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation02560Bank4[8] = {
#include "assets/m4a1_pyke_animation_02560_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation02560Records[76] = {
#include "assets/m4a1_pyke_animation_02560_records.inc"
};

static u16 _gM4a1PykeAnimation02560Indices[20] = {
#include "assets/m4a1_pyke_animation_02560_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation02560 = {
    _gM4a1PykeAnimation02560Records,
    _gM4a1PykeAnimation02560Indices,
    { NULL, _gM4a1PykeAnimation02560Bank1, NULL, NULL, _gM4a1PykeAnimation02560Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation02C04Bank1[12] = {
#include "assets/m4a1_pyke_animation_02C04_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation02C04Bank4[151] = {
#include "assets/m4a1_pyke_animation_02C04_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation02C04Records[218] = {
#include "assets/m4a1_pyke_animation_02C04_records.inc"
};

static u16 _gM4a1PykeAnimation02C04Indices[20] = {
#include "assets/m4a1_pyke_animation_02C04_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation02C04 = {
    _gM4a1PykeAnimation02C04Records,
    _gM4a1PykeAnimation02C04Indices,
    { NULL, _gM4a1PykeAnimation02C04Bank1, NULL, NULL, _gM4a1PykeAnimation02C04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation03464Bank1[19] = {
#include "assets/m4a1_pyke_animation_03464_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation03464Bank4[169] = {
#include "assets/m4a1_pyke_animation_03464_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation03464Records[290] = {
#include "assets/m4a1_pyke_animation_03464_records.inc"
};

static u16 _gM4a1PykeAnimation03464Indices[20] = {
#include "assets/m4a1_pyke_animation_03464_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation03464 = {
    _gM4a1PykeAnimation03464Records,
    _gM4a1PykeAnimation03464Indices,
    { NULL, _gM4a1PykeAnimation03464Bank1, NULL, NULL, _gM4a1PykeAnimation03464Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation03CC8Bank1[19] = {
#include "assets/m4a1_pyke_animation_03CC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation03CC8Bank4[170] = {
#include "assets/m4a1_pyke_animation_03CC8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation03CC8Records[290] = {
#include "assets/m4a1_pyke_animation_03CC8_records.inc"
};

static u16 _gM4a1PykeAnimation03CC8Indices[20] = {
#include "assets/m4a1_pyke_animation_03CC8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation03CC8 = {
    _gM4a1PykeAnimation03CC8Records,
    _gM4a1PykeAnimation03CC8Indices,
    { NULL, _gM4a1PykeAnimation03CC8Bank1, NULL, NULL, _gM4a1PykeAnimation03CC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation03FDCBank1[3] = {
#include "assets/m4a1_pyke_animation_03FDC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation03FDCBank4[69] = {
#include "assets/m4a1_pyke_animation_03FDC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation03FDCRecords[99] = {
#include "assets/m4a1_pyke_animation_03FDC_records.inc"
};

static u16 _gM4a1PykeAnimation03FDCIndices[20] = {
#include "assets/m4a1_pyke_animation_03FDC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation03FDC = {
    _gM4a1PykeAnimation03FDCRecords,
    _gM4a1PykeAnimation03FDCIndices,
    { NULL, _gM4a1PykeAnimation03FDCBank1, NULL, NULL, _gM4a1PykeAnimation03FDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation04738Bank1[14] = {
#include "assets/m4a1_pyke_animation_04738_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation04738Bank4[156] = {
#include "assets/m4a1_pyke_animation_04738_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation04738Records[253] = {
#include "assets/m4a1_pyke_animation_04738_records.inc"
};

static u16 _gM4a1PykeAnimation04738Indices[20] = {
#include "assets/m4a1_pyke_animation_04738_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation04738 = {
    _gM4a1PykeAnimation04738Records,
    _gM4a1PykeAnimation04738Indices,
    { NULL, _gM4a1PykeAnimation04738Bank1, NULL, NULL, _gM4a1PykeAnimation04738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation04EC0Bank1[16] = {
#include "assets/m4a1_pyke_animation_04EC0_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation04EC0Bank4[167] = {
#include "assets/m4a1_pyke_animation_04EC0_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation04EC0Records[247] = {
#include "assets/m4a1_pyke_animation_04EC0_records.inc"
};

static u16 _gM4a1PykeAnimation04EC0Indices[20] = {
#include "assets/m4a1_pyke_animation_04EC0_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation04EC0 = {
    _gM4a1PykeAnimation04EC0Records,
    _gM4a1PykeAnimation04EC0Indices,
    { NULL, _gM4a1PykeAnimation04EC0Bank1, NULL, NULL, _gM4a1PykeAnimation04EC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05194Bank1[6] = {
#include "assets/m4a1_pyke_animation_05194_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05194Bank4[52] = {
#include "assets/m4a1_pyke_animation_05194_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05194Records[91] = {
#include "assets/m4a1_pyke_animation_05194_records.inc"
};

static u16 _gM4a1PykeAnimation05194Indices[20] = {
#include "assets/m4a1_pyke_animation_05194_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05194 = {
    _gM4a1PykeAnimation05194Records,
    _gM4a1PykeAnimation05194Indices,
    { NULL, _gM4a1PykeAnimation05194Bank1, NULL, NULL, _gM4a1PykeAnimation05194Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05524Bank1[7] = {
#include "assets/m4a1_pyke_animation_05524_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05524Bank4[73] = {
#include "assets/m4a1_pyke_animation_05524_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05524Records[114] = {
#include "assets/m4a1_pyke_animation_05524_records.inc"
};

static u16 _gM4a1PykeAnimation05524Indices[20] = {
#include "assets/m4a1_pyke_animation_05524_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05524 = {
    _gM4a1PykeAnimation05524Records,
    _gM4a1PykeAnimation05524Indices,
    { NULL, _gM4a1PykeAnimation05524Bank1, NULL, NULL, _gM4a1PykeAnimation05524Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation059ACBank1[9] = {
#include "assets/m4a1_pyke_animation_059AC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation059ACBank4[104] = {
#include "assets/m4a1_pyke_animation_059AC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation059ACRecords[139] = {
#include "assets/m4a1_pyke_animation_059AC_records.inc"
};

static u16 _gM4a1PykeAnimation059ACIndices[20] = {
#include "assets/m4a1_pyke_animation_059AC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation059AC = {
    _gM4a1PykeAnimation059ACRecords,
    _gM4a1PykeAnimation059ACIndices,
    { NULL, _gM4a1PykeAnimation059ACBank1, NULL, NULL, _gM4a1PykeAnimation059ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05BA8Bank1[3] = {
#include "assets/m4a1_pyke_animation_05BA8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05BA8Bank4[22] = {
#include "assets/m4a1_pyke_animation_05BA8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05BA8Records[76] = {
#include "assets/m4a1_pyke_animation_05BA8_records.inc"
};

static u16 _gM4a1PykeAnimation05BA8Indices[20] = {
#include "assets/m4a1_pyke_animation_05BA8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05BA8 = {
    _gM4a1PykeAnimation05BA8Records,
    _gM4a1PykeAnimation05BA8Indices,
    { NULL, _gM4a1PykeAnimation05BA8Bank1, NULL, NULL, _gM4a1PykeAnimation05BA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05E80Bank1[6] = {
#include "assets/m4a1_pyke_animation_05E80_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05E80Bank4[57] = {
#include "assets/m4a1_pyke_animation_05E80_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05E80Records[87] = {
#include "assets/m4a1_pyke_animation_05E80_records.inc"
};

static u16 _gM4a1PykeAnimation05E80Indices[20] = {
#include "assets/m4a1_pyke_animation_05E80_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05E80 = {
    _gM4a1PykeAnimation05E80Records,
    _gM4a1PykeAnimation05E80Indices,
    { NULL, _gM4a1PykeAnimation05E80Bank1, NULL, NULL, _gM4a1PykeAnimation05E80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06124Bank1[4] = {
#include "assets/m4a1_pyke_animation_06124_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06124Bank4[55] = {
#include "assets/m4a1_pyke_animation_06124_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06124Records[82] = {
#include "assets/m4a1_pyke_animation_06124_records.inc"
};

static u16 _gM4a1PykeAnimation06124Indices[20] = {
#include "assets/m4a1_pyke_animation_06124_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06124 = {
    _gM4a1PykeAnimation06124Records,
    _gM4a1PykeAnimation06124Indices,
    { NULL, _gM4a1PykeAnimation06124Bank1, NULL, NULL, _gM4a1PykeAnimation06124Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06324Bank1[3] = {
#include "assets/m4a1_pyke_animation_06324_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06324Bank4[23] = {
#include "assets/m4a1_pyke_animation_06324_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06324Records[76] = {
#include "assets/m4a1_pyke_animation_06324_records.inc"
};

static u16 _gM4a1PykeAnimation06324Indices[20] = {
#include "assets/m4a1_pyke_animation_06324_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06324 = {
    _gM4a1PykeAnimation06324Records,
    _gM4a1PykeAnimation06324Indices,
    { NULL, _gM4a1PykeAnimation06324Bank1, NULL, NULL, _gM4a1PykeAnimation06324Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06678Bank1[8] = {
#include "assets/m4a1_pyke_animation_06678_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06678Bank4[68] = {
#include "assets/m4a1_pyke_animation_06678_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06678Records[101] = {
#include "assets/m4a1_pyke_animation_06678_records.inc"
};

static u16 _gM4a1PykeAnimation06678Indices[20] = {
#include "assets/m4a1_pyke_animation_06678_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06678 = {
    _gM4a1PykeAnimation06678Records,
    _gM4a1PykeAnimation06678Indices,
    { NULL, _gM4a1PykeAnimation06678Bank1, NULL, NULL, _gM4a1PykeAnimation06678Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0692CBank1[5] = {
#include "assets/m4a1_pyke_animation_0692C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0692CBank4[55] = {
#include "assets/m4a1_pyke_animation_0692C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0692CRecords[83] = {
#include "assets/m4a1_pyke_animation_0692C_records.inc"
};

static u16 _gM4a1PykeAnimation0692CIndices[20] = {
#include "assets/m4a1_pyke_animation_0692C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0692C = {
    _gM4a1PykeAnimation0692CRecords,
    _gM4a1PykeAnimation0692CIndices,
    { NULL, _gM4a1PykeAnimation0692CBank1, NULL, NULL, _gM4a1PykeAnimation0692CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06C4CBank1[6] = {
#include "assets/m4a1_pyke_animation_06C4C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06C4CBank4[66] = {
#include "assets/m4a1_pyke_animation_06C4C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06C4CRecords[96] = {
#include "assets/m4a1_pyke_animation_06C4C_records.inc"
};

static u16 _gM4a1PykeAnimation06C4CIndices[20] = {
#include "assets/m4a1_pyke_animation_06C4C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06C4C = {
    _gM4a1PykeAnimation06C4CRecords,
    _gM4a1PykeAnimation06C4CIndices,
    { NULL, _gM4a1PykeAnimation06C4CBank1, NULL, NULL, _gM4a1PykeAnimation06C4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation07410Bank1[18] = {
#include "assets/m4a1_pyke_animation_07410_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation07410Bank4[184] = {
#include "assets/m4a1_pyke_animation_07410_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation07410Records[239] = {
#include "assets/m4a1_pyke_animation_07410_records.inc"
};

static u16 _gM4a1PykeAnimation07410Indices[20] = {
#include "assets/m4a1_pyke_animation_07410_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation07410 = {
    _gM4a1PykeAnimation07410Records,
    _gM4a1PykeAnimation07410Indices,
    { NULL, _gM4a1PykeAnimation07410Bank1, NULL, NULL, _gM4a1PykeAnimation07410Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation086A8Bank1[29] = {
#include "assets/m4a1_pyke_animation_086A8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation086A8Bank4[450] = {
#include "assets/m4a1_pyke_animation_086A8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation086A8Records[633] = {
#include "assets/m4a1_pyke_animation_086A8_records.inc"
};

static u16 _gM4a1PykeAnimation086A8Indices[20] = {
#include "assets/m4a1_pyke_animation_086A8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation086A8 = {
    _gM4a1PykeAnimation086A8Records,
    _gM4a1PykeAnimation086A8Indices,
    { NULL, _gM4a1PykeAnimation086A8Bank1, NULL, NULL, _gM4a1PykeAnimation086A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation09220Bank1[12] = {
#include "assets/m4a1_pyke_animation_09220_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation09220Bank4[266] = {
#include "assets/m4a1_pyke_animation_09220_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation09220Records[412] = {
#include "assets/m4a1_pyke_animation_09220_records.inc"
};

static u16 _gM4a1PykeAnimation09220Indices[20] = {
#include "assets/m4a1_pyke_animation_09220_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation09220 = {
    _gM4a1PykeAnimation09220Records,
    _gM4a1PykeAnimation09220Indices,
    { NULL, _gM4a1PykeAnimation09220Bank1, NULL, NULL, _gM4a1PykeAnimation09220Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0993CBank1[9] = {
#include "assets/m4a1_pyke_animation_0993C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0993CBank4[144] = {
#include "assets/m4a1_pyke_animation_0993C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0993CRecords[264] = {
#include "assets/m4a1_pyke_animation_0993C_records.inc"
};

static u16 _gM4a1PykeAnimation0993CIndices[20] = {
#include "assets/m4a1_pyke_animation_0993C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0993C = {
    _gM4a1PykeAnimation0993CRecords,
    _gM4a1PykeAnimation0993CIndices,
    { NULL, _gM4a1PykeAnimation0993CBank1, NULL, NULL, _gM4a1PykeAnimation0993CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation09DBCBank1[6] = {
#include "assets/m4a1_pyke_animation_09DBC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation09DBCBank4[107] = {
#include "assets/m4a1_pyke_animation_09DBC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation09DBCRecords[143] = {
#include "assets/m4a1_pyke_animation_09DBC_records.inc"
};

static u16 _gM4a1PykeAnimation09DBCIndices[20] = {
#include "assets/m4a1_pyke_animation_09DBC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation09DBC = {
    _gM4a1PykeAnimation09DBCRecords,
    _gM4a1PykeAnimation09DBCIndices,
    { NULL, _gM4a1PykeAnimation09DBCBank1, NULL, NULL, _gM4a1PykeAnimation09DBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation09F94Bank1[3] = {
#include "assets/m4a1_pyke_animation_09F94_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation09F94Bank4[32] = {
#include "assets/m4a1_pyke_animation_09F94_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation09F94Records[57] = {
#include "assets/m4a1_pyke_animation_09F94_records.inc"
};

static u16 _gM4a1PykeAnimation09F94Indices[20] = {
#include "assets/m4a1_pyke_animation_09F94_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation09F94 = {
    _gM4a1PykeAnimation09F94Records,
    _gM4a1PykeAnimation09F94Indices,
    { NULL, _gM4a1PykeAnimation09F94Bank1, NULL, NULL, _gM4a1PykeAnimation09F94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0A4F8Bank1[11] = {
#include "assets/m4a1_pyke_animation_0A4F8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0A4F8Bank4[125] = {
#include "assets/m4a1_pyke_animation_0A4F8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0A4F8Records[167] = {
#include "assets/m4a1_pyke_animation_0A4F8_records.inc"
};

static u16 _gM4a1PykeAnimation0A4F8Indices[20] = {
#include "assets/m4a1_pyke_animation_0A4F8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0A4F8 = {
    _gM4a1PykeAnimation0A4F8Records,
    _gM4a1PykeAnimation0A4F8Indices,
    { NULL, _gM4a1PykeAnimation0A4F8Bank1, NULL, NULL, _gM4a1PykeAnimation0A4F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0A6ECBank1[3] = {
#include "assets/m4a1_pyke_animation_0A6EC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0A6ECBank4[20] = {
#include "assets/m4a1_pyke_animation_0A6EC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0A6ECRecords[76] = {
#include "assets/m4a1_pyke_animation_0A6EC_records.inc"
};

static u16 _gM4a1PykeAnimation0A6ECIndices[20] = {
#include "assets/m4a1_pyke_animation_0A6EC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0A6EC = {
    _gM4a1PykeAnimation0A6ECRecords,
    _gM4a1PykeAnimation0A6ECIndices,
    { NULL, _gM4a1PykeAnimation0A6ECBank1, NULL, NULL, _gM4a1PykeAnimation0A6ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0AB6CBank1[8] = {
#include "assets/m4a1_pyke_animation_0AB6C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0AB6CBank4[105] = {
#include "assets/m4a1_pyke_animation_0AB6C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0AB6CRecords[139] = {
#include "assets/m4a1_pyke_animation_0AB6C_records.inc"
};

static u16 _gM4a1PykeAnimation0AB6CIndices[20] = {
#include "assets/m4a1_pyke_animation_0AB6C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0AB6C = {
    _gM4a1PykeAnimation0AB6CRecords,
    _gM4a1PykeAnimation0AB6CIndices,
    { NULL, _gM4a1PykeAnimation0AB6CBank1, NULL, NULL, _gM4a1PykeAnimation0AB6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0AD44Bank1[2] = {
#include "assets/m4a1_pyke_animation_0AD44_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0AD44Bank4[16] = {
#include "assets/m4a1_pyke_animation_0AD44_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0AD44Records[76] = {
#include "assets/m4a1_pyke_animation_0AD44_records.inc"
};

static u16 _gM4a1PykeAnimation0AD44Indices[20] = {
#include "assets/m4a1_pyke_animation_0AD44_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0AD44 = {
    _gM4a1PykeAnimation0AD44Records,
    _gM4a1PykeAnimation0AD44Indices,
    { NULL, _gM4a1PykeAnimation0AD44Bank1, NULL, NULL, _gM4a1PykeAnimation0AD44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0B584Bank1[14] = {
#include "assets/m4a1_pyke_animation_0B584_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0B584Bank4[194] = {
#include "assets/m4a1_pyke_animation_0B584_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0B584Records[272] = {
#include "assets/m4a1_pyke_animation_0B584_records.inc"
};

static u16 _gM4a1PykeAnimation0B584Indices[20] = {
#include "assets/m4a1_pyke_animation_0B584_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0B584 = {
    _gM4a1PykeAnimation0B584Records,
    _gM4a1PykeAnimation0B584Indices,
    { NULL, _gM4a1PykeAnimation0B584Bank1, NULL, NULL, _gM4a1PykeAnimation0B584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0C02CBank1[19] = {
#include "assets/m4a1_pyke_animation_0C02C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0C02CBank4[269] = {
#include "assets/m4a1_pyke_animation_0C02C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0C02CRecords[336] = {
#include "assets/m4a1_pyke_animation_0C02C_records.inc"
};

static u16 _gM4a1PykeAnimation0C02CIndices[20] = {
#include "assets/m4a1_pyke_animation_0C02C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0C02C = {
    _gM4a1PykeAnimation0C02CRecords,
    _gM4a1PykeAnimation0C02CIndices,
    { NULL, _gM4a1PykeAnimation0C02CBank1, NULL, NULL, _gM4a1PykeAnimation0C02CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0CEC8Bank1[24] = {
#include "assets/m4a1_pyke_animation_0CEC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0CEC8Bank4[390] = {
#include "assets/m4a1_pyke_animation_0CEC8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0CEC8Records[453] = {
#include "assets/m4a1_pyke_animation_0CEC8_records.inc"
};

static u16 _gM4a1PykeAnimation0CEC8Indices[20] = {
#include "assets/m4a1_pyke_animation_0CEC8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0CEC8 = {
    _gM4a1PykeAnimation0CEC8Records,
    _gM4a1PykeAnimation0CEC8Indices,
    { NULL, _gM4a1PykeAnimation0CEC8Bank1, NULL, NULL, _gM4a1PykeAnimation0CEC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0D36CBank1[8] = {
#include "assets/m4a1_pyke_animation_0D36C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0D36CBank4[102] = {
#include "assets/m4a1_pyke_animation_0D36C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0D36CRecords[151] = {
#include "assets/m4a1_pyke_animation_0D36C_records.inc"
};

static u16 _gM4a1PykeAnimation0D36CIndices[20] = {
#include "assets/m4a1_pyke_animation_0D36C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0D36C = {
    _gM4a1PykeAnimation0D36CRecords,
    _gM4a1PykeAnimation0D36CIndices,
    { NULL, _gM4a1PykeAnimation0D36CBank1, NULL, NULL, _gM4a1PykeAnimation0D36CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0D7F4Bank1[7] = {
#include "assets/m4a1_pyke_animation_0D7F4_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0D7F4Bank4[91] = {
#include "assets/m4a1_pyke_animation_0D7F4_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0D7F4Records[158] = {
#include "assets/m4a1_pyke_animation_0D7F4_records.inc"
};

static u16 _gM4a1PykeAnimation0D7F4Indices[20] = {
#include "assets/m4a1_pyke_animation_0D7F4_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0D7F4 = {
    _gM4a1PykeAnimation0D7F4Records,
    _gM4a1PykeAnimation0D7F4Indices,
    { NULL, _gM4a1PykeAnimation0D7F4Bank1, NULL, NULL, _gM4a1PykeAnimation0D7F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0DC68Bank1[8] = {
#include "assets/m4a1_pyke_animation_0DC68_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0DC68Bank4[104] = {
#include "assets/m4a1_pyke_animation_0DC68_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0DC68Records[137] = {
#include "assets/m4a1_pyke_animation_0DC68_records.inc"
};

static u16 _gM4a1PykeAnimation0DC68Indices[20] = {
#include "assets/m4a1_pyke_animation_0DC68_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0DC68 = {
    _gM4a1PykeAnimation0DC68Records,
    _gM4a1PykeAnimation0DC68Indices,
    { NULL, _gM4a1PykeAnimation0DC68Bank1, NULL, NULL, _gM4a1PykeAnimation0DC68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0E374Bank1[13] = {
#include "assets/m4a1_pyke_animation_0E374_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0E374Bank4[175] = {
#include "assets/m4a1_pyke_animation_0E374_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0E374Records[217] = {
#include "assets/m4a1_pyke_animation_0E374_records.inc"
};

static u16 _gM4a1PykeAnimation0E374Indices[20] = {
#include "assets/m4a1_pyke_animation_0E374_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0E374 = {
    _gM4a1PykeAnimation0E374Records,
    _gM4a1PykeAnimation0E374Indices,
    { NULL, _gM4a1PykeAnimation0E374Bank1, NULL, NULL, _gM4a1PykeAnimation0E374Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0E8ECBank1[10] = {
#include "assets/m4a1_pyke_animation_0E8EC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0E8ECBank4[131] = {
#include "assets/m4a1_pyke_animation_0E8EC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0E8ECRecords[169] = {
#include "assets/m4a1_pyke_animation_0E8EC_records.inc"
};

static u16 _gM4a1PykeAnimation0E8ECIndices[20] = {
#include "assets/m4a1_pyke_animation_0E8EC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0E8EC = {
    _gM4a1PykeAnimation0E8ECRecords,
    _gM4a1PykeAnimation0E8ECIndices,
    { NULL, _gM4a1PykeAnimation0E8ECBank1, NULL, NULL, _gM4a1PykeAnimation0E8ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0F1A4Bank1[18] = {
#include "assets/m4a1_pyke_animation_0F1A4_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0F1A4Bank4[201] = {
#include "assets/m4a1_pyke_animation_0F1A4_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0F1A4Records[283] = {
#include "assets/m4a1_pyke_animation_0F1A4_records.inc"
};

static u16 _gM4a1PykeAnimation0F1A4Indices[20] = {
#include "assets/m4a1_pyke_animation_0F1A4_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0F1A4 = {
    _gM4a1PykeAnimation0F1A4Records,
    _gM4a1PykeAnimation0F1A4Indices,
    { NULL, _gM4a1PykeAnimation0F1A4Bank1, NULL, NULL, _gM4a1PykeAnimation0F1A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0F944Bank1[16] = {
#include "assets/m4a1_pyke_animation_0F944_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0F944Bank4[157] = {
#include "assets/m4a1_pyke_animation_0F944_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0F944Records[263] = {
#include "assets/m4a1_pyke_animation_0F944_records.inc"
};

static u16 _gM4a1PykeAnimation0F944Indices[20] = {
#include "assets/m4a1_pyke_animation_0F944_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0F944 = {
    _gM4a1PykeAnimation0F944Records,
    _gM4a1PykeAnimation0F944Indices,
    { NULL, _gM4a1PykeAnimation0F944Bank1, NULL, NULL, _gM4a1PykeAnimation0F944Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation10318Bank1[25] = {
#include "assets/m4a1_pyke_animation_10318_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation10318Bank4[223] = {
#include "assets/m4a1_pyke_animation_10318_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation10318Records[311] = {
#include "assets/m4a1_pyke_animation_10318_records.inc"
};

static u16 _gM4a1PykeAnimation10318Indices[20] = {
#include "assets/m4a1_pyke_animation_10318_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation10318 = {
    _gM4a1PykeAnimation10318Records,
    _gM4a1PykeAnimation10318Indices,
    { NULL, _gM4a1PykeAnimation10318Bank1, NULL, NULL, _gM4a1PykeAnimation10318Bank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_pyke_8012D500 = { { {
    NULL,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation0F1A4,
    &_gM4a1PykeAnimation0F944,
    &_gM4a1PykeAnimation10318,
    &_gM4a1PykeAnimation03464,
    &_gM4a1PykeAnimation03CC8,
    &_gM4a1PykeAnimation0E374,
    &_gM4a1PykeAnimation0E8EC,
    &_gM4a1PykeAnimation0AD44,
    &_gM4a1PykeAnimation0D36C,
    &_gM4a1PykeAnimation0D7F4,
    &_gM4a1PykeAnimation0C02C,
    &_gM4a1PykeAnimation0B584,
    &_gM4a1PykeAnimation0CEC8,
    &_gM4a1PykeAnimation0DC68,
    &_gM4a1PykeAnimation0692C,
    &_gM4a1PykeAnimation06C4C,
    &_gM4a1PykeAnimation07410,
    &_gM4a1PykeAnimation02C04,
    &_gM4a1PykeAnimation0CEC8,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation086A8,
    &_gM4a1PykeAnimation0993C,
    &_gM4a1PykeAnimation09220,
    &_gM4a1PykeAnimation059AC,
    &_gM4a1PykeAnimation05BA8,
    &_gM4a1PykeAnimation05E80,
    &_gM4a1PykeAnimation06124,
    &_gM4a1PykeAnimation06324,
    &_gM4a1PykeAnimation06678,
    &_gM4a1PykeAnimation09DBC,
    &_gM4a1PykeAnimation09F94,
    &_gM4a1PykeAnimation09DBC,
    &_gM4a1PykeAnimation09F94,
    &_gM4a1PykeAnimation04738,
    &_gM4a1PykeAnimation04EC0,
    &_gM4a1PykeAnimation05524,
    &_gM4a1PykeAnimation05194,
    &_gM4a1PykeAnimation03FDC,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation0A4F8,
    &_gM4a1PykeAnimation0A6EC,
    &_gM4a1PykeAnimation0AB6C,
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
