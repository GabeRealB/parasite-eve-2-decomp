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

/// Initial identity transform and self-parented root of the equipped weapon.
///
/// Read-only pose and count tables retain their overlay positions in .data.
static const TmdBone _gM4a1PykeWeaponModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_model_01BCC_skeleton.inc"
};

/// Part-local vertex counts in skeleton order; the sole part owns all 74 vertices.
static const u32 _gM4a1PykeWeaponModelPartVerts[ARRAY_SIZE(_gM4a1PykeWeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_model_01BCC_partVerts.inc"
};

/// Equipped weapon vertices in integer coordinates local to its single part.
static SVECTOR _gM4a1PykeWeaponModelVerts[74] = {
#include "assets/m4a1_pyke_model_01BCC_verts.inc"
};

/// Weapon lighting normals with twelve fractional bits, indexed separately from vertices.
static SVECTOR _gM4a1PykeWeaponModelNormals[64] = {
#include "assets/m4a1_pyke_model_01BCC_normals.inc"
};

/// Writable commands for two Gouraud textured triangles and 69 textured quads.
///
/// Creation resolves handler slots in place. The skeletal group is followed
/// by an empty final group and `TMD_STREAM_END`.
static u32 _gM4a1PykeWeaponModelStream[504] = {
#include "assets/m4a1_pyke_model_01BCC_stream.inc"
};

TmdSource gM4a1PykeWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (69 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1PykeWeaponModelSkeleton),
    .partVertexCounts    = _gM4a1PykeWeaponModelPartVerts,
    .verts               = _gM4a1PykeWeaponModelVerts,
    .normals             = _gM4a1PykeWeaponModelNormals,
    .skeleton            = _gM4a1PykeWeaponModelSkeleton,
    .stream              = _gM4a1PykeWeaponModelStream,
};

/// Translation-and-rotation poses for the player's idle pose in the Pyke bank.
///
/// Encoding 1 uses integer local translations and 4096 angle units per turn.
/// Read-only clip arrays retain their overlay positions in .data.
static const AnimationPackedPose _gM4a1PykeIdleAnimationBank1[2] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02560_bank1.inc"
};

/// Packed Euler rotations for the player's idle pose in the Pyke bank.
///
/// Encoding 4 stores 11/10/11 signed components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeIdleAnimationBank4[8] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02560_bank4.inc"
};

/// Keyframe durations and loop/stop controls for the player's idle pose.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeIdleAnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02560_records.inc"
};

/// Absolute starting record indices for the player's idle pose tracks.
///
/// Native playback uses tracks 0..18; the twentieth stored index is zero.
static const u16 _gM4a1PykeIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02560_indices.inc"
};

/// Player idle pose supplied by the M4A1 Pyke animation bank.
///
/// Native-bank sets 1, 21, 22 and 41 share this pose.
/// Borrows nineteen part tracks and their pose banks from this overlay; keep
/// the package loaded throughout playback. The weapon model is a separate asset.
static AnimationSet _gM4a1PykeIdleAnimation = {
    .records           = _gM4a1PykeIdleAnimationRecords,
    .trackStartIndices = _gM4a1PykeIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeIdleAnimationBank1, NULL, NULL, _gM4a1PykeIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for the player's unarmed forward walk in the Pyke bank.
///
/// Encoding 1 uses integer local translations and 4096 angle units per turn.
/// Read-only clip arrays retain their overlay positions in .data.
static const AnimationPackedPose _gM4a1PykeUnarmedWalkAnimationBank1[12] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02C04_bank1.inc"
};

/// Packed Euler rotations for the player's unarmed forward walk in the Pyke bank.
///
/// Encoding 4 stores 11/10/11 signed components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeUnarmedWalkAnimationBank4[151] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02C04_bank4.inc"
};

/// Keyframe durations and loop/stop controls for the player's unarmed forward walk.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeUnarmedWalkAnimationRecords[218] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02C04_records.inc"
};

/// Absolute starting record indices for the player's unarmed forward walk tracks.
///
/// Native playback uses tracks 0..18; the twentieth stored index is zero.
static const u16 _gM4a1PykeUnarmedWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_02C04_indices.inc"
};

/// Player unarmed forward walk supplied by the M4A1 Pyke animation bank.
///
/// Native-bank set 19 is selected while the weapon model is absent.
/// Borrows nineteen part tracks and their pose banks from this overlay; keep
/// the package loaded throughout playback. The weapon model is a separate asset.
static AnimationSet _gM4a1PykeUnarmedWalkAnimation = {
    .records           = _gM4a1PykeUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM4a1PykeUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeUnarmedWalkAnimationBank1, NULL, NULL, _gM4a1PykeUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for the player's stationary left turn in the Pyke bank.
///
/// Encoding 1 uses integer local translations and 4096 angle units per turn.
/// Read-only clip arrays retain their overlay positions in .data.
static const AnimationPackedPose _gM4a1PykeTurnLeftAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03464_bank1.inc"
};

/// Packed Euler rotations for the player's stationary left turn in the Pyke bank.
///
/// Encoding 4 stores 11/10/11 signed components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeTurnLeftAnimationBank4[169] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03464_bank4.inc"
};

/// Keyframe durations and loop/stop controls for the player's stationary left turn.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeTurnLeftAnimationRecords[290] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03464_records.inc"
};

/// Absolute starting record indices for the player's stationary left turn tracks.
///
/// Native playback uses tracks 0..18; the twentieth stored index is zero.
static const u16 _gM4a1PykeTurnLeftAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03464_indices.inc"
};

/// Player stationary left turn supplied by the M4A1 Pyke animation bank.
///
/// Native-bank set 5 accompanies negative heading steps.
/// Borrows nineteen part tracks and their pose banks from this overlay; keep
/// the package loaded throughout playback. The weapon model is a separate asset.
static AnimationSet _gM4a1PykeTurnLeftAnimation = {
    .records           = _gM4a1PykeTurnLeftAnimationRecords,
    .trackStartIndices = _gM4a1PykeTurnLeftAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeTurnLeftAnimationBank1, NULL, NULL, _gM4a1PykeTurnLeftAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for the player's stationary right turn in the Pyke bank.
///
/// Encoding 1 uses integer local translations and 4096 angle units per turn.
/// Read-only clip arrays retain their overlay positions in .data.
static const AnimationPackedPose _gM4a1PykeTurnRightAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03CC8_bank1.inc"
};

/// Packed Euler rotations for the player's stationary right turn in the Pyke bank.
///
/// Encoding 4 stores 11/10/11 signed components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeTurnRightAnimationBank4[170] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03CC8_bank4.inc"
};

/// Keyframe durations and loop/stop controls for the player's stationary right turn.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeTurnRightAnimationRecords[290] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03CC8_records.inc"
};

/// Absolute starting record indices for the player's stationary right turn tracks.
///
/// Native playback uses tracks 0..18; the twentieth stored index is zero.
static const u16 _gM4a1PykeTurnRightAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03CC8_indices.inc"
};

/// Player stationary right turn supplied by the M4A1 Pyke animation bank.
///
/// Native-bank set 6 accompanies positive heading steps.
/// Borrows nineteen part tracks and their pose banks from this overlay; keep
/// the package loaded throughout playback. The weapon model is a separate asset.
static AnimationSet _gM4a1PykeTurnRightAnimation = {
    .records           = _gM4a1PykeTurnRightAnimationRecords,
    .trackStartIndices = _gM4a1PykeTurnRightAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeTurnRightAnimationBank1, NULL, NULL, _gM4a1PykeTurnRightAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for the player's item-use motion in the Pyke bank.
///
/// Encoding 1 uses integer local translations and 4096 angle units per turn.
/// Read-only clip arrays retain their overlay positions in .data.
static const AnimationPackedPose _gM4a1PykeItemUseAnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03FDC_bank1.inc"
};

/// Packed Euler rotations for the player's item-use motion in the Pyke bank.
///
/// Encoding 4 stores 11/10/11 signed components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeItemUseAnimationBank4[69] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03FDC_bank4.inc"
};

/// Keyframe durations and stop controls for the player's item-use motion.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeItemUseAnimationRecords[99] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03FDC_records.inc"
};

/// Absolute starting record indices for the player's item-use motion tracks.
///
/// Native playback uses tracks 0..18; the twentieth stored index is zero.
static const u16 _gM4a1PykeItemUseAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_03FDC_indices.inc"
};

/// Player item-use motion supplied by the M4A1 Pyke animation bank.
///
/// Native-bank set 40 plays once before aim locomotion resumes.
/// Borrows nineteen part tracks and their pose banks from this overlay; keep
/// the package loaded throughout playback. The weapon model is a separate asset.
static AnimationSet _gM4a1PykeItemUseAnimation = {
    .records           = _gM4a1PykeItemUseAnimationRecords,
    .trackStartIndices = _gM4a1PykeItemUseAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeItemUseAnimationBank1, NULL, NULL, _gM4a1PykeItemUseAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for the player's stair ascent in the Pyke bank.
///
/// Tracks 0 and 1 use local integer translations and 4096 angle units per turn.
/// Immutable clip payload retains its overlay position in .data.
static const AnimationPackedPose _gM4a1PykeStairAscentAnimationBank1[14] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04738_bank1.inc"
};

/// Packed part rotations for the player's stair ascent in the Pyke bank.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeStairAscentAnimationBank4[156] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04738_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for the player's stair ascent.
///
/// Durations count normal-rate frames; pose offsets count four-byte words,
/// while control jumps use absolute record indices.
static const AnimationRecord _gM4a1PykeStairAscentAnimationRecords[253] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04738_records.inc"
};

/// Absolute track-start record indices for the player's stair ascent.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeStairAscentAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04738_indices.inc"
};

/// Player stair-ascent motion supplied by the M4A1 Pyke animation bank.
///
/// Native-bank set 36 loops through a scripted stair flight; track 1 carries
/// the footstep cues that pace its strides. Borrows nineteen part tracks and
/// their pose banks; keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeStairAscentAnimation = {
    .records           = _gM4a1PykeStairAscentAnimationRecords,
    .trackStartIndices = _gM4a1PykeStairAscentAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeStairAscentAnimationBank1, NULL, NULL, _gM4a1PykeStairAscentAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for the player's stair descent in the Pyke bank.
///
/// Tracks 0 and 1 use local integer translations and 4096 angle units per turn.
/// Immutable clip payload retains its overlay position in .data.
static const AnimationPackedPose _gM4a1PykeStairDescentAnimationBank1[16] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04EC0_bank1.inc"
};

/// Packed part rotations for the player's stair descent in the Pyke bank.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeStairDescentAnimationBank4[167] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04EC0_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for the player's stair descent.
///
/// Durations count normal-rate frames; pose offsets count four-byte words,
/// while control jumps use absolute record indices.
static const AnimationRecord _gM4a1PykeStairDescentAnimationRecords[247] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04EC0_records.inc"
};

/// Absolute track-start record indices for the player's stair descent.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeStairDescentAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_04EC0_indices.inc"
};

/// Player stair-descent motion supplied by the M4A1 Pyke animation bank.
///
/// Native-bank set 37 loops through a scripted stair flight; track 1 carries
/// the footstep cues that pace its strides. Borrows nineteen part tracks and
/// their pose banks; keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeStairDescentAnimation = {
    .records           = _gM4a1PykeStairDescentAnimationRecords,
    .trackStartIndices = _gM4a1PykeStairDescentAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeStairDescentAnimationBank1, NULL, NULL, _gM4a1PykeStairDescentAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for finishing an odd-step stair ascent.
///
/// Tracks 0 and 1 use local integer translations and 4096 angle units per turn.
/// Immutable clip payload retains its overlay position in .data.
static const AnimationPackedPose _gM4a1PykeStairAscentFinishOddAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05194_bank1.inc"
};

/// Packed part rotations for finishing an odd-step stair ascent.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeStairAscentFinishOddAnimationBank4[52] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05194_bank4.inc"
};

/// Keyframes, footstep cues and stop controls for the odd-step stair-ascent finish.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeStairAscentFinishOddAnimationRecords[91] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05194_records.inc"
};

/// Absolute track-start record indices for the odd-step stair-ascent finish.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeStairAscentFinishOddAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05194_indices.inc"
};

/// Player finish motion for a scripted stair ascent with an odd initial step count.
///
/// Native-bank set 39 plays once, with a three-frame entry blend, before idle
/// resumes. Borrows nineteen part tracks and their pose banks from the Pyke
/// package; keep it loaded throughout playback.
static AnimationSet _gM4a1PykeStairAscentFinishOddAnimation = {
    .records           = _gM4a1PykeStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM4a1PykeStairAscentFinishOddAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeStairAscentFinishOddAnimationBank1, NULL, NULL, _gM4a1PykeStairAscentFinishOddAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for finishing an even-step stair ascent.
///
/// Tracks 0 and 1 use local integer translations and 4096 angle units per turn.
/// Immutable clip payload retains its overlay position in .data.
static const AnimationPackedPose _gM4a1PykeStairAscentFinishEvenAnimationBank1[7] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05524_bank1.inc"
};

/// Packed part rotations for finishing an even-step stair ascent.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeStairAscentFinishEvenAnimationBank4[73] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05524_bank4.inc"
};

/// Keyframes, footstep cues and stop controls for the even-step stair-ascent finish.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeStairAscentFinishEvenAnimationRecords[114] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05524_records.inc"
};

/// Absolute track-start record indices for the even-step stair-ascent finish.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeStairAscentFinishEvenAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05524_indices.inc"
};

/// Player finish motion for a scripted stair ascent with an even initial step count.
///
/// Native-bank set 38 plays once, with a three-frame entry blend, before idle
/// resumes. Borrows nineteen part tracks and their pose banks from the Pyke
/// package; keep it loaded throughout playback.
static AnimationSet _gM4a1PykeStairAscentFinishEvenAnimation = {
    .records           = _gM4a1PykeStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM4a1PykeStairAscentFinishEvenAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeStairAscentFinishEvenAnimationBank1, NULL, NULL, _gM4a1PykeStairAscentFinishEvenAnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for family-0 Parasite Energy release.
///
/// Tracks 0 and 1 use local integer translations and 4096 angle units per turn.
/// Immutable clip payload retains its overlay position in .data.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyReleaseFamily0AnimationBank1[9] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_059AC_bank1.inc"
};

/// Packed part rotations for family-0 Parasite Energy release.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyReleaseFamily0AnimationBank4[104] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_059AC_bank4.inc"
};

/// Keyframe durations and stop controls for family-0 Parasite Energy release.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyReleaseFamily0AnimationRecords[139] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_059AC_records.inc"
};

/// Absolute track-start record indices for family-0 Parasite Energy release.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyReleaseFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_059AC_indices.inc"
};

/// Player release motion that starts family-0 Parasite Energy charging.
///
/// Native-bank set 26 plays once with a six-frame entry blend when the
/// attachment selects clip family 0. Borrows nineteen part tracks and their
/// pose banks from the Pyke package; keep it loaded throughout playback.
static AnimationSet _gM4a1PykeParasiteEnergyReleaseFamily0Animation = {
    .records           = _gM4a1PykeParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyReleaseFamily0AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyReleaseFamily0AnimationBank4, NULL, NULL, NULL },
};

/// Translation-and-rotation poses for family-0 Parasite Energy charging.
///
/// Tracks 0 and 1 use local integer translations and 4096 angle units per turn.
/// Immutable clip payload retains its overlay position in .data.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyChargeFamily0AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05BA8_bank1.inc"
};

/// Packed part rotations for family-0 Parasite Energy charging.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyChargeFamily0AnimationBank4[22] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05BA8_bank4.inc"
};

/// Keyframe durations and loop controls for family-0 Parasite Energy charging.
///
/// Durations count normal-rate frames; pose offsets count four-byte words,
/// while control jumps use absolute record indices.
static const AnimationRecord _gM4a1PykeParasiteEnergyChargeFamily0AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05BA8_records.inc"
};

/// Absolute track-start record indices for family-0 Parasite Energy charging.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyChargeFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05BA8_indices.inc"
};

/// Player charge motion for family-0 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 27 loops while the attachment countdown runs, then the
/// cast motion replaces it. Borrows nineteen part tracks and their pose banks;
/// keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeParasiteEnergyChargeFamily0Animation = {
    .records           = _gM4a1PykeParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyChargeFamily0AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyChargeFamily0AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-0 Parasite Energy casting.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyCastFamily0AnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05E80_bank1.inc"
};

/// Packed rotation keyframes for family-0 Parasite Energy casting.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyCastFamily0AnimationBank4[57] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05E80_bank4.inc"
};

/// Keyframe durations and stop controls for family-0 Parasite Energy casting.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyCastFamily0AnimationRecords[87] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05E80_records.inc"
};

/// Absolute track-start record indices for family-0 Parasite Energy casting.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyCastFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_05E80_indices.inc"
};

/// Player cast motion for family-0 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 28 starts when the attachment countdown reaches zero;
/// its stop controls finish the cast phase. Borrows nineteen part tracks and
/// their pose banks; keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeParasiteEnergyCastFamily0Animation = {
    .records           = _gM4a1PykeParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyCastFamily0AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyCastFamily0AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-1 Parasite Energy release.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyReleaseFamily1AnimationBank1[4] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06124_bank1.inc"
};

/// Packed rotation keyframes for family-1 Parasite Energy release.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyReleaseFamily1AnimationBank4[55] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06124_bank4.inc"
};

/// Keyframe durations and stop controls for family-1 Parasite Energy release.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyReleaseFamily1AnimationRecords[82] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06124_records.inc"
};

/// Absolute track-start record indices for family-1 Parasite Energy release.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyReleaseFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06124_indices.inc"
};

/// Player release motion for family-1 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 29 starts the attachment countdown; its stop controls
/// hand playback to the charge phase. Borrows nineteen part tracks and their
/// pose banks; keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeParasiteEnergyReleaseFamily1Animation = {
    .records           = _gM4a1PykeParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyReleaseFamily1AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyReleaseFamily1AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-1 Parasite Energy charging.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyChargeFamily1AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06324_bank1.inc"
};

/// Packed rotation keyframes for family-1 Parasite Energy charging.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyChargeFamily1AnimationBank4[23] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06324_bank4.inc"
};

/// Keyframe durations and loop controls for family-1 Parasite Energy charging.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track loops through two keyframes via an absolute record index;
/// the trailing stop record remains stored after that jump.
static const AnimationRecord _gM4a1PykeParasiteEnergyChargeFamily1AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06324_records.inc"
};

/// Absolute track-start record indices for family-1 Parasite Energy charging.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyChargeFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06324_indices.inc"
};

/// Player charge motion for family-1 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 30 loops while the attachment countdown runs, then the
/// cast motion replaces it. Borrows nineteen part tracks and their pose banks;
/// keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeParasiteEnergyChargeFamily1Animation = {
    .records           = _gM4a1PykeParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyChargeFamily1AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyChargeFamily1AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-1 Parasite Energy casting.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyCastFamily1AnimationBank1[8] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06678_bank1.inc"
};

/// Packed rotation keyframes for family-1 Parasite Energy casting.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyCastFamily1AnimationBank4[68] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06678_bank4.inc"
};

/// Keyframe durations and stop controls for family-1 Parasite Energy casting.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyCastFamily1AnimationRecords[101] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06678_records.inc"
};

/// Absolute track-start record indices for family-1 Parasite Energy casting.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyCastFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06678_indices.inc"
};

/// Player cast motion for family-1 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 31 starts when the attachment countdown reaches zero;
/// its stop controls finish the cast phase. Borrows nineteen part tracks and
/// their pose banks; keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeParasiteEnergyCastFamily1Animation = {
    .records           = _gM4a1PykeParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyCastFamily1AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyCastFamily1AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for a hit to player model part 4.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeHitPart4AnimationBank1[5] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0692C_bank1.inc"
};

/// Packed rotation keyframes for a hit to player model part 4.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeHitPart4AnimationBank4[55] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0692C_bank4.inc"
};

/// Keyframe durations and stop controls for a hit to player model part 4.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeHitPart4AnimationRecords[83] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0692C_records.inc"
};

/// Absolute track-start record indices for a hit to player model part 4.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeHitPart4AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0692C_indices.inc"
};

/// Player hit reaction for contact with model part 4 in the Pyke bank.
///
/// Native-bank set 16 is selected for collision-body index 1, the sphere
/// attached to model part 4. Playback blends child tracks over three frames.
/// Borrows nineteen part tracks and their pose banks; keep this package loaded
/// throughout playback.
static AnimationSet _gM4a1PykeHitPart4Animation = {
    .records           = _gM4a1PykeHitPart4AnimationRecords,
    .trackStartIndices = _gM4a1PykeHitPart4AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeHitPart4AnimationBank1, NULL, NULL, _gM4a1PykeHitPart4AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for a hit to another player body region.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeHitOtherBodyAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06C4C_bank1.inc"
};

/// Packed rotation keyframes for a hit to another player body region.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeHitOtherBodyAnimationBank4[66] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06C4C_bank4.inc"
};

/// Keyframe durations and stop controls for a hit to another player body region.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeHitOtherBodyAnimationRecords[96] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06C4C_records.inc"
};

/// Absolute track-start record indices for a hit to another player body region.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeHitOtherBodyAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_06C4C_indices.inc"
};

/// Player hit reaction for other body contacts in the Pyke bank.
///
/// Native-bank set 17 is selected for hits outside the model-part-4 sphere.
/// Playback blends child tracks over three frames. Borrows nineteen part
/// tracks and their pose banks; keep this package loaded throughout playback.
static AnimationSet _gM4a1PykeHitOtherBodyAnimation = {
    .records           = _gM4a1PykeHitOtherBodyAnimationRecords,
    .trackStartIndices = _gM4a1PykeHitOtherBodyAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeHitOtherBodyAnimationBank1, NULL, NULL, _gM4a1PykeHitOtherBodyAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for the player's death pose.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeDeathAnimationBank1[18] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_07410_bank1.inc"
};

/// Packed rotation keyframes for the player's death pose.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeDeathAnimationBank4[184] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_07410_bank4.inc"
};

/// Keyframe durations and stop controls for the player's death pose.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeDeathAnimationRecords[239] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_07410_records.inc"
};

/// Absolute track-start record indices for the player's death pose.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeDeathAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_07410_indices.inc"
};

/// Player death pose in the Pyke bank, ending with each part held in place.
///
/// Native-bank set 18 is selected at zero HP with movement and turning stopped.
/// Borrows nineteen part tracks and their pose banks; keep this package loaded
/// throughout playback. Child tracks may be reset or blended into this set.
static AnimationSet _gM4a1PykeDeathAnimation = {
    .records           = _gM4a1PykeDeathAnimationRecords,
    .trackStartIndices = _gM4a1PykeDeathAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeDeathAnimationBank1, NULL, NULL, _gM4a1PykeDeathAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for the high-health idle.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeHighHealthIdleAnimationBank1[29] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_086A8_bank1.inc"
};

/// Packed rotation keyframes for the high-health idle.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeHighHealthIdleAnimationBank4[450] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_086A8_bank4.inc"
};

/// Keyframe durations, loop jumps and stop controls for the high-health idle.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices within the same part track.
static const AnimationRecord _gM4a1PykeHighHealthIdleAnimationRecords[633] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_086A8_records.inc"
};

/// Absolute track-start record indices for the high-health idle.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeHighHealthIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_086A8_indices.inc"
};

/// Looping player idle for HP above half maximum in the Pyke bank.
///
/// Native-bank set 23 follows 300 idle ticks, with a five-frame child-track blend.
/// Borrows nineteen part tracks and their pose banks; keep this package loaded
/// throughout playback.
static AnimationSet _gM4a1PykeHighHealthIdleAnimation = {
    .records           = _gM4a1PykeHighHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1PykeHighHealthIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeHighHealthIdleAnimationBank1, NULL, NULL, _gM4a1PykeHighHealthIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for the low-health idle and paralysis.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeLowHealthIdleAnimationBank1[12] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09220_bank1.inc"
};

/// Packed rotation keyframes for the low-health idle and paralysis.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeLowHealthIdleAnimationBank4[266] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09220_bank4.inc"
};

/// Keyframe durations, loop jumps and stop controls for the low-health idle.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices within the same part track.
static const AnimationRecord _gM4a1PykeLowHealthIdleAnimationRecords[412] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09220_records.inc"
};

/// Absolute track-start record indices for the low-health idle and paralysis.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeLowHealthIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09220_indices.inc"
};

/// Looping player idle for HP at or below one quarter maximum in the Pyke bank.
///
/// Native-bank set 25 follows 300 idle ticks, with a five-frame child-track blend.
/// Paralysis also selects this set, blending all child tracks over six frames.
/// Borrows nineteen part tracks and their pose banks; keep this package loaded
/// throughout playback.
static AnimationSet _gM4a1PykeLowHealthIdleAnimation = {
    .records           = _gM4a1PykeLowHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1PykeLowHealthIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeLowHealthIdleAnimationBank1, NULL, NULL, _gM4a1PykeLowHealthIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for the middle-health idle.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeMidHealthIdleAnimationBank1[9] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0993C_bank1.inc"
};

/// Packed rotation keyframes for the middle-health idle.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeMidHealthIdleAnimationBank4[144] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0993C_bank4.inc"
};

/// Keyframe durations, loop jumps and stop controls for the middle-health idle.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices within the same part track.
static const AnimationRecord _gM4a1PykeMidHealthIdleAnimationRecords[264] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0993C_records.inc"
};

/// Absolute track-start record indices for the middle-health idle.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeMidHealthIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0993C_indices.inc"
};

/// Looping player idle for HP above one quarter and at most half maximum.
///
/// Native Pyke-bank set 24 follows 300 idle ticks, with a five-frame child-track
/// blend. Borrows nineteen part tracks and their pose banks; keep this package
/// loaded throughout playback.
static AnimationSet _gM4a1PykeMidHealthIdleAnimation = {
    .records           = _gM4a1PykeMidHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1PykeMidHealthIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeMidHealthIdleAnimationBank1, NULL, NULL, _gM4a1PykeMidHealthIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for the player's CAP event entry.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeEventEntryAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09DBC_bank1.inc"
};

/// Packed rotation keyframes for the player's CAP event entry.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeEventEntryAnimationBank4[107] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09DBC_bank4.inc"
};

/// Keyframe durations and stop controls for the player's CAP event entry.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeEventEntryAnimationRecords[143] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09DBC_records.inc"
};

/// Absolute track-start record indices for the player's CAP event entry.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeEventEntryAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09DBC_indices.inc"
};

/// Player presentation before a CAP event in the Pyke bank.
///
/// Native-bank set 32 starts on the next scripted tick for presentation variant
/// zero, then holds each part's final pose while the event runs. Slot 34 aliases
/// the same clip. Borrows nineteen part tracks and their pose banks; keep this
/// package loaded throughout playback.
static AnimationSet _gM4a1PykeEventEntryAnimation = {
    .records           = _gM4a1PykeEventEntryAnimationRecords,
    .trackStartIndices = _gM4a1PykeEventEntryAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeEventEntryAnimationBank1, NULL, NULL, _gM4a1PykeEventEntryAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for the player's CAP event exit.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeEventExitAnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09F94_bank1.inc"
};

/// Packed rotation keyframes for the player's CAP event exit.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeEventExitAnimationBank4[32] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09F94_bank4.inc"
};

/// Keyframe durations and stop controls for the player's CAP event exit.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeEventExitAnimationRecords[57] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09F94_records.inc"
};

/// Absolute track-start record indices for the player's CAP event exit.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeEventExitAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_09F94_indices.inc"
};

/// Player presentation after a CAP event in the Pyke bank.
///
/// Native-bank set 33 starts on the next scripted tick for a nonzero presentation
/// variant; player control resumes after it finishes. Slot 35 aliases the same
/// clip. Borrows nineteen part tracks and their pose banks; keep this package
/// loaded throughout playback.
static AnimationSet _gM4a1PykeEventExitAnimation = {
    .records           = _gM4a1PykeEventExitAnimationRecords,
    .trackStartIndices = _gM4a1PykeEventExitAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeEventExitAnimationBank1, NULL, NULL, _gM4a1PykeEventExitAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-2 Parasite Energy release.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyReleaseFamily2AnimationBank1[11] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A4F8_bank1.inc"
};

/// Packed rotation keyframes for family-2 Parasite Energy release.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyReleaseFamily2AnimationBank4[125] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A4F8_bank4.inc"
};

/// Keyframe durations and stop controls for family-2 Parasite Energy release.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyReleaseFamily2AnimationRecords[167] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A4F8_records.inc"
};

/// Absolute track-start record indices for family-2 Parasite Energy release.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyReleaseFamily2AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A4F8_indices.inc"
};

/// Player release motion for family-2 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 42 starts the release phase with a six-frame child-track blend;
/// its stop controls allow the charge phase to begin.
/// Borrows nineteen part tracks and their word-aligned pose banks; keep this
/// package loaded while any slot or cached record refers to the clip.
static AnimationSet _gM4a1PykeParasiteEnergyReleaseFamily2Animation = {
    .records           = _gM4a1PykeParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyReleaseFamily2AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyReleaseFamily2AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-2 Parasite Energy charging.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyChargeFamily2AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A6EC_bank1.inc"
};

/// Packed rotation keyframes for family-2 Parasite Energy charging.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyChargeFamily2AnimationBank4[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A6EC_bank4.inc"
};

/// Keyframe durations and loop controls for family-2 Parasite Energy charging.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyChargeFamily2AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A6EC_records.inc"
};

/// Absolute track-start record indices for family-2 Parasite Energy charging.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyChargeFamily2AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0A6EC_indices.inc"
};

/// Player charge hold for family-2 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 43 loops while the attachment countdown is nonzero;
/// set 44 replaces it when the cast begins.
/// Borrows nineteen part tracks and their word-aligned pose banks; keep this
/// package loaded while any slot or cached record refers to the clip.
static AnimationSet _gM4a1PykeParasiteEnergyChargeFamily2Animation = {
    .records           = _gM4a1PykeParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyChargeFamily2AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyChargeFamily2AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for family-2 Parasite Energy casting.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeParasiteEnergyCastFamily2AnimationBank1[8] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AB6C_bank1.inc"
};

/// Packed rotation keyframes for family-2 Parasite Energy casting.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeParasiteEnergyCastFamily2AnimationBank4[105] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AB6C_bank4.inc"
};

/// Keyframe durations and stop controls for family-2 Parasite Energy casting.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeParasiteEnergyCastFamily2AnimationRecords[139] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AB6C_records.inc"
};

/// Absolute track-start record indices for family-2 Parasite Energy casting.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeParasiteEnergyCastFamily2AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AB6C_indices.inc"
};

/// Player cast motion for family-2 Parasite Energy actions in the Pyke bank.
///
/// Native-bank set 44 starts when the attachment countdown reaches zero;
/// its stop controls finish the cast phase.
/// Borrows nineteen part tracks and their word-aligned pose banks; keep this
/// package loaded while any slot or cached record refers to the clip.
static AnimationSet _gM4a1PykeParasiteEnergyCastFamily2Animation = {
    .records           = _gM4a1PykeParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gM4a1PykeParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeParasiteEnergyCastFamily2AnimationBank1, NULL, NULL, _gM4a1PykeParasiteEnergyCastFamily2AnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for stationary aiming.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeAimIdleAnimationBank1[2] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AD44_bank1.inc"
};

/// Packed rotation keyframes for stationary aiming.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeAimIdleAnimationBank4[16] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AD44_bank4.inc"
};

/// Keyframe durations and loop controls for stationary aiming.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1PykeAimIdleAnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AD44_records.inc"
};

/// Absolute track-start record indices for stationary aiming.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeAimIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0AD44_indices.inc"
};

/// Player stationary aiming pose in the Pyke bank.
///
/// Native-bank set 9 loops while neither movement nor turning is requested.
/// Attack preparation also blends into this pose before selecting a weapon attack.
/// Borrows nineteen part tracks and their word-aligned pose banks; keep this
/// package loaded while any slot or cached record refers to the clip.
static AnimationSet _gM4a1PykeAimIdleAnimation = {
    .records           = _gM4a1PykeAimIdleAnimationRecords,
    .trackStartIndices = _gM4a1PykeAimIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeAimIdleAnimationBank1, NULL, NULL, _gM4a1PykeAimIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for backward aiming movement and stationary turning.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeAimBackwardOrTurnAnimationBank1[14] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0B584_bank1.inc"
};

/// Packed rotation keyframes for backward aiming movement and stationary turning.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeAimBackwardOrTurnAnimationBank4[194] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0B584_bank4.inc"
};

/// Keyframe durations and loop controls for backward aiming movement and stationary turning.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Track 1 also carries the paired footstep cue bits.
static const AnimationRecord _gM4a1PykeAimBackwardOrTurnAnimationRecords[272] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0B584_records.inc"
};

/// Absolute track-start record indices for backward aiming movement and stationary turning.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeAimBackwardOrTurnAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0B584_indices.inc"
};

/// Player backward aiming movement and stationary turn cycle in the Pyke bank.
///
/// Native-bank set 13 loops for backward movement or turning without movement;
/// slot-1 cues select the paired footstep sounds and effects.
/// Borrows nineteen part tracks and their word-aligned pose banks; keep this
/// package loaded while any slot or cached record refers to the clip.
static AnimationSet _gM4a1PykeAimBackwardOrTurnAnimation = {
    .records           = _gM4a1PykeAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM4a1PykeAimBackwardOrTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeAimBackwardOrTurnAnimationBank1, NULL, NULL, _gM4a1PykeAimBackwardOrTurnAnimationBank4, NULL, NULL, NULL },
};

/// Translation and rotation keyframes for forward aiming movement.
///
/// Tracks 0 and 1 use signed halfwords: translation in model units and Euler
/// angles in 4096 units per turn.
static const AnimationPackedPose _gM4a1PykeAimForwardAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0C02C_bank1.inc"
};

/// Packed rotation keyframes for forward aiming movement.
///
/// Tracks 2..18 use signed 11/10/11-bit components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1PykeAimForwardAnimationBank4[269] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0C02C_bank4.inc"
};

/// Keyframe durations and loop controls for forward aiming movement.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Track 1 also carries the paired footstep cue bits.
static const AnimationRecord _gM4a1PykeAimForwardAnimationRecords[336] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0C02C_records.inc"
};

/// Absolute track-start record indices for forward aiming movement.
///
/// Tracks 0..18 are followed by an unused stored zero halfword.
static const u16 _gM4a1PykeAimForwardAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_pyke_animation_0C02C_indices.inc"
};

/// Player forward aiming movement cycle in the Pyke bank.
///
/// Native-bank set 12 loops for forward movement while aiming;
/// slot-1 cues select the paired footstep sounds and effects.
/// Borrows nineteen part tracks and their word-aligned pose banks; keep this
/// package loaded while any slot or cached record refers to the clip.
static AnimationSet _gM4a1PykeAimForwardAnimation = {
    .records           = _gM4a1PykeAimForwardAnimationRecords,
    .trackStartIndices = _gM4a1PykeAimForwardAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PykeAimForwardAnimationBank1, NULL, NULL, _gM4a1PykeAimForwardAnimationBank4, NULL, NULL, NULL },
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
    &_gM4a1PykeIdleAnimation,
    &_gM4a1PykeAnimation0F1A4,
    &_gM4a1PykeAnimation0F944,
    &_gM4a1PykeAnimation10318,
    &_gM4a1PykeTurnLeftAnimation,
    &_gM4a1PykeTurnRightAnimation,
    &_gM4a1PykeAnimation0E374,
    &_gM4a1PykeAnimation0E8EC,
    &_gM4a1PykeAimIdleAnimation,
    &_gM4a1PykeAnimation0D36C,
    &_gM4a1PykeAnimation0D7F4,
    &_gM4a1PykeAimForwardAnimation,
    &_gM4a1PykeAimBackwardOrTurnAnimation,
    &_gM4a1PykeAnimation0CEC8,
    &_gM4a1PykeAnimation0DC68,
    &_gM4a1PykeHitPart4Animation,
    &_gM4a1PykeHitOtherBodyAnimation,
    &_gM4a1PykeDeathAnimation,
    &_gM4a1PykeUnarmedWalkAnimation,
    &_gM4a1PykeAnimation0CEC8,
    &_gM4a1PykeIdleAnimation,
    &_gM4a1PykeIdleAnimation,
    &_gM4a1PykeHighHealthIdleAnimation,
    &_gM4a1PykeMidHealthIdleAnimation,
    &_gM4a1PykeLowHealthIdleAnimation,
    &_gM4a1PykeParasiteEnergyReleaseFamily0Animation,
    &_gM4a1PykeParasiteEnergyChargeFamily0Animation,
    &_gM4a1PykeParasiteEnergyCastFamily0Animation,
    &_gM4a1PykeParasiteEnergyReleaseFamily1Animation,
    &_gM4a1PykeParasiteEnergyChargeFamily1Animation,
    &_gM4a1PykeParasiteEnergyCastFamily1Animation,
    &_gM4a1PykeEventEntryAnimation,
    &_gM4a1PykeEventExitAnimation,
    &_gM4a1PykeEventEntryAnimation,
    &_gM4a1PykeEventExitAnimation,
    &_gM4a1PykeStairAscentAnimation,
    &_gM4a1PykeStairDescentAnimation,
    &_gM4a1PykeStairAscentFinishEvenAnimation,
    &_gM4a1PykeStairAscentFinishOddAnimation,
    &_gM4a1PykeItemUseAnimation,
    &_gM4a1PykeIdleAnimation,
    &_gM4a1PykeParasiteEnergyReleaseFamily2Animation,
    &_gM4a1PykeParasiteEnergyChargeFamily2Animation,
    &_gM4a1PykeParasiteEnergyCastFamily2Animation,
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
