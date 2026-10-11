#include "weapons/m4a1.h"

#include <psyq/libgpu.h>

#include "common.h"
#include "types.h"

#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"

/// The weapon's index: 0x10 for the M4A1, 0x14 and 0x15 for its two upgrades.
/// The three packages are this source built once each, and each declares its
/// index in the manifest. It keys the firing sound, the shot effect and the item.
#ifndef WEAPON_ID
#error "WEAPON_ID is a per-package build parameter"
#endif

enum { M4A1_IMPACT_SOUND = SOUND_COMMON(0x17) };

/// Encoding-indexed banks used by the running clip's player-part tracks.
enum {
    M4A1_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M4A1_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Ends an M4A1-family shot's contact tests and requests the chosen surface's impact effect and sound.
///
/// Clears only the weapon body's grid/pair enables; keeps it linked and its
/// complete six-entry contact table intact. Requires live player/room state,
/// initialized contacts and `rootCoord->workm` in the contacts' view frame.
/// Enemy-body hits suppress surface selection. A selected eligible grid point
/// receives 0..7 units of XYZ jitter; only `impactCoord`'s three cached
/// translation words are written. Failure leaves that temporary node untouched.
/// On success, requests the common impact sound synchronously and latches
/// combat noise. Scratch/GTE requirements follow the picker and sound APIs;
/// this helper neither releases nor retains the caller's writable impact node.
static inline void _m4a1ResolveShotImpact(GameActor* actor, const GfxCoord* rootCoord, GfxCoord* impactCoord)
{
    enum { M4A1_IMPACT_SIGNAL_NOISE = 1 };

    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, M4A1_IMPACT_SOUND, M4A1_IMPACT_SIGNAL_NOISE);
    }
}

void m4a1AttackState(Task* playerTask)
{
    enum {
        M4A1_PHASE_PREPARE             = 0,
        M4A1_PHASE_WAIT_READY          = 1,
        M4A1_PHASE_BURST               = 2,
        M4A1_PHASE_RECOVER             = 3,
        M4A1_PLAYER_ATTACK_STATE       = 4,
        M4A1_ANIMATION_READY           = 9,
        M4A1_ANIMATION_PRIMARY         = 0xA,
        M4A1_READY_BLEND_FRAMES        = 1,
        M4A1_MOVING_READY_BLEND_FRAMES = 6,
        M4A1_BURST_ROUNDS              = 3,
        M4A1_SHOT_DELAY_FRAMES         = 3,
        M4A1_IMPACT_DELAY_FRAMES       = 2,
        M4A1_CANCEL_FRAMES             = 9,
        M4A1_RECOVERY_COOLDOWN_FRAMES  = 0xC,
        M4A1_FIRE_SOUND_BASE           = SOUND_WEAPON(0, 4),
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        shotDelay;
    // Preserve the halfword shot count before storing it in actionValue.
    s16 burstRounds;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case M4A1_PHASE_PREPARE:
            actor->state             = M4A1_PLAYER_ATTACK_STATE;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = M4A1_PHASE_WAIT_READY;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = M4A1_CANCEL_FRAMES;
            burstRounds              = 1;
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                burstRounds = M4A1_BURST_ROUNDS;
            }
            actor->actionValue = burstRounds;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            readyBlendFrames                                      = M4A1_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M4A1_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M4A1_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_PHASE_BURST:
            // Enable contacts for one dispatch and resolve each round on the next.
            if (actor->actionValue != 0) {
                shotDelay = actor->stateTimer;
                if (shotDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M4A1_SHOT_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_FIRE_SOUND_BASE | (WEAPON_ID << 16), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_ANIMATION_PRIMARY, 0, 2);
                    break;
                }
                actor->stateTimer = shotDelay - 1;
                if (shotDelay - 1 == M4A1_IMPACT_DELAY_FRAMES) {
                    _m4a1ResolveShotImpact(actor, rootCoord, impactCoord);
                }
            } else {
                // Resolve the final impact before entering recovery.
                actor->statePhase = M4A1_PHASE_RECOVER;
                _m4a1ResolveShotImpact(actor, rootCoord, impactCoord);
            }
            break;
        case M4A1_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// Initial identity transform and self-parented root of the held rifle.
///
/// Read-only asset tables retain their serialized positions in .data.
static const TmdBone _gM4a1RifleModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/m4a1_model_006AC_skeleton.inc"
};

/// Vertex count of the rifle's sole part: 58.
static const u32 _gM4a1RifleModelPartVerts[ARRAY_SIZE(_gM4a1RifleModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m4a1_model_006AC_partVerts.inc"
};

/// Writable rifle vertices in integer coordinates local to the model root.
static SVECTOR _gM4a1RifleModelVerts[58] = {
#include "assets/m4a1_model_006AC_verts.inc"
};

/// Writable rifle lighting normals, indexed independently of the vertices.
static SVECTOR _gM4a1RifleModelNormals[58] = {
#include "assets/m4a1_model_006AC_normals.inc"
};

/// Writable commands for two textured triangles and 55 textured quads in one part.
///
/// Draw-handler slots resolve in place; the final word is `TMD_STREAM_END`.
static u32 _gM4a1RifleModelStream[406] = {
#include "assets/m4a1_model_006AC_stream.inc"
};

TmdSource gM4a1RifleModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (55 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1RifleModelSkeleton),
    .partVertexCounts    = _gM4a1RifleModelPartVerts,
    .verts               = _gM4a1RifleModelVerts,
    .normals             = _gM4a1RifleModelNormals,
    .skeleton            = _gM4a1RifleModelSkeleton,
    .stream              = _gM4a1RifleModelStream,
};

/// Idle translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1IdleAnimationBank1[2] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_00EB8_bank1.inc"
};

/// Idle rotation-only keyframes, with each packed angle step equal to 1/512 turn.
static const AnimationPackedRotation _gM4a1IdleAnimationBank4[8] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_00EB8_bank4.inc"
};

/// Keyframes and track controls for the idle pose; durations count normal-rate frames.
static const AnimationRecord _gM4a1IdleAnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_00EB8_records.inc"
};

/// Nineteen idle track-start record indices plus the exporter's trailing halfword.
static const u16 _gM4a1IdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_00EB8_indices.inc"
};

/// Player-rig idle pose in the M4A1-family bank.
///
/// Nineteen tracks; bank indices 1, 21, 22 and 41 share this set. All clip data
/// remains borrowed from the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1IdleAnimation = {
    .records           = _gM4a1IdleAnimationRecords,
    .trackStartIndices = _gM4a1IdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1IdleAnimationBank1, NULL, NULL, _gM4a1IdleAnimationBank4, NULL, NULL, NULL },
};

/// Unarmed-walk translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1UnarmedWalkAnimationBank1[12] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0155C_bank1.inc"
};

/// Unarmed-walk rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1UnarmedWalkAnimationBank4[151] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0155C_bank4.inc"
};

/// Unarmed-walk keyframes and track controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1UnarmedWalkAnimationRecords[218] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0155C_records.inc"
};

/// Nineteen unarmed-walk track starts plus the exporter's trailing halfword.
static const u16 _gM4a1UnarmedWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0155C_indices.inc"
};

/// Player-rig forward walk when no weapon model is attached, in M4A1-family bank entry 19.
///
/// Nineteen tracks borrowing the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1UnarmedWalkAnimation = {
    .records           = _gM4a1UnarmedWalkAnimationRecords,
    .trackStartIndices = _gM4a1UnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1UnarmedWalkAnimationBank1, NULL, NULL, _gM4a1UnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Negative-turn translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1NegativeTurnAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_01DBC_bank1.inc"
};

/// Negative-turn rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1NegativeTurnAnimationBank4[169] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_01DBC_bank4.inc"
};

/// Negative-turn keyframes and track controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1NegativeTurnAnimationRecords[290] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_01DBC_records.inc"
};

/// Nineteen negative-turn track starts plus the exporter's trailing halfword.
static const u16 _gM4a1NegativeTurnAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_01DBC_indices.inc"
};

/// Player-rig turn toward decreasing yaw, in M4A1-family bank entry 5.
///
/// Nineteen tracks borrowing the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1NegativeTurnAnimation = {
    .records           = _gM4a1NegativeTurnAnimationRecords,
    .trackStartIndices = _gM4a1NegativeTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1NegativeTurnAnimationBank1, NULL, NULL, _gM4a1NegativeTurnAnimationBank4, NULL, NULL, NULL },
};

/// Positive-turn translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1PositiveTurnAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02620_bank1.inc"
};

/// Positive-turn rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1PositiveTurnAnimationBank4[170] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02620_bank4.inc"
};

/// Positive-turn keyframes and track controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1PositiveTurnAnimationRecords[290] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02620_records.inc"
};

/// Nineteen positive-turn track starts plus the exporter's trailing halfword.
static const u16 _gM4a1PositiveTurnAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02620_indices.inc"
};

/// Player-rig turn toward increasing yaw, in M4A1-family bank entry 6.
///
/// Nineteen tracks borrowing the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1PositiveTurnAnimation = {
    .records           = _gM4a1PositiveTurnAnimationRecords,
    .trackStartIndices = _gM4a1PositiveTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1PositiveTurnAnimationBank1, NULL, NULL, _gM4a1PositiveTurnAnimationBank4, NULL, NULL, NULL },
};

/// Item-use translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ItemUseAnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02934_bank1.inc"
};

/// Item-use rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ItemUseAnimationBank4[69] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02934_bank4.inc"
};

/// Item-use keyframes and track controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ItemUseAnimationRecords[99] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02934_records.inc"
};

/// Nineteen item-use track starts plus the exporter's trailing halfword.
static const u16 _gM4a1ItemUseAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_02934_indices.inc"
};

/// Player-rig item-use gesture in M4A1-family bank entry 40.
///
/// Nineteen tracks borrowing the loaded weapon overlay throughout playback;
/// this clip presents the action and does not apply the item's effect.
static AnimationSet _gM4a1ItemUseAnimation = {
    .records           = _gM4a1ItemUseAnimationRecords,
    .trackStartIndices = _gM4a1ItemUseAnimationIndices,
    .poseBanks         = { NULL, _gM4a1ItemUseAnimationBank1, NULL, NULL, _gM4a1ItemUseAnimationBank4, NULL, NULL, NULL },
};

/// Stair-ascent translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1StairAscentAnimationBank1[14] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03090_bank1.inc"
};

/// Stair-ascent rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1StairAscentAnimationBank4[156] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03090_bank4.inc"
};

/// Stair-ascent keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1StairAscentAnimationRecords[253] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03090_records.inc"
};

/// Stair-ascent record starts for tracks 0..18, followed by a retained exporter halfword.
static const u16 _gM4a1StairAscentAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03090_indices.inc"
};

/// Player-rig stair ascent loop in M4A1-family bank entry 36.
///
/// Nineteen tracks; slot-1 footstep cues gate the scripted stair strides.
/// Keep the selected M4A1-family overlay loaded throughout playback.
static AnimationSet _gM4a1StairAscentAnimation = {
    .records           = _gM4a1StairAscentAnimationRecords,
    .trackStartIndices = _gM4a1StairAscentAnimationIndices,
    .poseBanks         = { NULL, _gM4a1StairAscentAnimationBank1, NULL, NULL, _gM4a1StairAscentAnimationBank4, NULL, NULL, NULL },
};

/// Stair-descent translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1StairDescentAnimationBank1[16] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03818_bank1.inc"
};

/// Stair-descent rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1StairDescentAnimationBank4[167] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03818_bank4.inc"
};

/// Stair-descent keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1StairDescentAnimationRecords[247] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03818_records.inc"
};

/// Stair-descent record starts for tracks 0..18, followed by a retained exporter halfword.
static const u16 _gM4a1StairDescentAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03818_indices.inc"
};

/// Player-rig stair descent loop in M4A1-family bank entry 37.
///
/// Nineteen tracks; slot-1 footstep cues gate the scripted stair strides.
/// Keep the selected M4A1-family overlay loaded throughout playback.
static AnimationSet _gM4a1StairDescentAnimation = {
    .records           = _gM4a1StairDescentAnimationRecords,
    .trackStartIndices = _gM4a1StairDescentAnimationIndices,
    .poseBanks         = { NULL, _gM4a1StairDescentAnimationBank1, NULL, NULL, _gM4a1StairDescentAnimationBank4, NULL, NULL, NULL },
};

/// Odd-count stair-finish translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1StairAscentFinishOddAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03AEC_bank1.inc"
};

/// Odd-count stair-finish rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1StairAscentFinishOddAnimationBank4[52] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03AEC_bank4.inc"
};

/// Odd-count stair-finish keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1StairAscentFinishOddAnimationRecords[91] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03AEC_records.inc"
};

/// Odd-count stair-finish record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1StairAscentFinishOddAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03AEC_indices.inc"
};

/// Player-rig stair-ascent finish after an odd step count, in M4A1-family bank entry 39.
///
/// Selected by the initial stair count, before returning to the idle pose.
/// Nineteen tracks borrowing the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1StairAscentFinishOddAnimation = {
    .records           = _gM4a1StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM4a1StairAscentFinishOddAnimationIndices,
    .poseBanks         = { NULL, _gM4a1StairAscentFinishOddAnimationBank1, NULL, NULL, _gM4a1StairAscentFinishOddAnimationBank4, NULL, NULL, NULL },
};

/// Even-count stair-finish translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1StairAscentFinishEvenAnimationBank1[7] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03E7C_bank1.inc"
};

/// Even-count stair-finish rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1StairAscentFinishEvenAnimationBank4[73] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03E7C_bank4.inc"
};

/// Even-count stair-finish keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1StairAscentFinishEvenAnimationRecords[114] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03E7C_records.inc"
};

/// Even-count stair-finish record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1StairAscentFinishEvenAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_03E7C_indices.inc"
};

/// Player-rig stair-ascent finish after an even step count, in M4A1-family bank entry 38.
///
/// Selected by the initial stair count, before returning to the idle pose.
/// Nineteen tracks borrowing the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1StairAscentFinishEvenAnimation = {
    .records           = _gM4a1StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM4a1StairAscentFinishEvenAnimationIndices,
    .poseBanks         = { NULL, _gM4a1StairAscentFinishEvenAnimationBank1, NULL, NULL, _gM4a1StairAscentFinishEvenAnimationBank4, NULL, NULL, NULL },
};

/// PE family-0 release translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyReleaseFamily0AnimationBank1[9] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04304_bank1.inc"
};

/// PE family-0 release rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyReleaseFamily0AnimationBank4[104] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04304_bank4.inc"
};

/// PE family-0 release keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyReleaseFamily0AnimationRecords[139] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04304_records.inc"
};

/// PE family-0 release record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1ParasiteEnergyReleaseFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04304_indices.inc"
};

/// Player-rig Parasite Energy family-0 release gesture in M4A1-family bank entry 26.
///
/// Starts the release/charge/cast sequence while the attachment countdown begins.
/// Family 0 covers spell IDs 300..600 with tens digit other than 3, and item IDs
/// at least 601 with tens digit 1. Nineteen tracks borrow the loaded overlay
/// throughout playback.
static AnimationSet _gM4a1ParasiteEnergyReleaseFamily0Animation = {
    .records           = _gM4a1ParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyReleaseFamily0AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyReleaseFamily0AnimationBank4, NULL, NULL, NULL },
};

/// PE family-0 charge translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyChargeFamily0AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04500_bank1.inc"
};

/// PE family-0 charge rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyChargeFamily0AnimationBank4[22] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04500_bank4.inc"
};

/// PE family-0 charge keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyChargeFamily0AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04500_records.inc"
};

/// PE family-0 charge record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1ParasiteEnergyChargeFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04500_indices.inc"
};

/// Player-rig Parasite Energy family-0 charge loop in M4A1-family bank entry 27.
///
/// Runs after `_gM4a1ParasiteEnergyReleaseFamily0Animation` until the attachment
/// countdown reaches zero and the cast starts, unless the action is cancelled.
/// Nineteen tracks borrowing the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyChargeFamily0Animation = {
    .records           = _gM4a1ParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyChargeFamily0AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyChargeFamily0AnimationBank4, NULL, NULL, NULL },
};

/// PE family-0 cast translation/rotation keyframes in integer model units and 1/4096 turns.
///
/// Read-only clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM4a1ParasiteEnergyCastFamily0AnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_047D8_bank1.inc"
};

/// PE family-0 cast rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyCastFamily0AnimationBank4[57] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_047D8_bank4.inc"
};

/// PE family-0 cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyCastFamily0AnimationRecords[87] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_047D8_records.inc"
};

/// PE family-0 cast record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1ParasiteEnergyCastFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_047D8_indices.inc"
};

/// Player-rig Parasite Energy family-0 cast gesture in M4A1-family bank entry 28.
///
/// Plays when the family-0 attachment countdown reaches zero, after its charge.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyCastFamily0Animation = {
    .records           = _gM4a1ParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyCastFamily0AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyCastFamily0AnimationBank4, NULL, NULL, NULL },
};

/// PE family-1 release translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyReleaseFamily1AnimationBank1[4] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04A7C_bank1.inc"
};

/// PE family-1 release rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyReleaseFamily1AnimationBank4[55] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04A7C_bank4.inc"
};

/// PE family-1 release keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyReleaseFamily1AnimationRecords[82] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04A7C_records.inc"
};

/// PE family-1 release record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1ParasiteEnergyReleaseFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04A7C_indices.inc"
};

/// Player-rig Parasite Energy family-1 release gesture in M4A1-family bank entry 29.
///
/// Starts the release/charge/cast sequence while the attachment countdown begins.
/// Family 1 covers IDs below 300 with tens digit other than 3, and item IDs
/// at least 601 with tens digit other than 1. Nineteen tracks borrow the loaded
/// M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyReleaseFamily1Animation = {
    .records           = _gM4a1ParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyReleaseFamily1AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyReleaseFamily1AnimationBank4, NULL, NULL, NULL },
};

/// PE family-1 charge translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyChargeFamily1AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04C7C_bank1.inc"
};

/// PE family-1 charge rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyChargeFamily1AnimationBank4[23] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04C7C_bank4.inc"
};

/// PE family-1 charge keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyChargeFamily1AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04C7C_records.inc"
};

/// PE family-1 charge record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1ParasiteEnergyChargeFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04C7C_indices.inc"
};

/// Player-rig Parasite Energy family-1 charge loop in M4A1-family bank entry 30.
///
/// Runs after `_gM4a1ParasiteEnergyReleaseFamily1Animation` until the attachment
/// countdown reaches zero and the cast starts, unless the action is cancelled.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyChargeFamily1Animation = {
    .records           = _gM4a1ParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyChargeFamily1AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyChargeFamily1AnimationBank4, NULL, NULL, NULL },
};

/// PE family-1 cast translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyCastFamily1AnimationBank1[8] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04FD0_bank1.inc"
};

/// PE family-1 cast rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyCastFamily1AnimationBank4[68] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04FD0_bank4.inc"
};

/// PE family-1 cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyCastFamily1AnimationRecords[101] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04FD0_records.inc"
};

/// PE family-1 cast record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1ParasiteEnergyCastFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_04FD0_indices.inc"
};

/// Player-rig Parasite Energy family-1 cast gesture in M4A1-family bank entry 31.
///
/// Plays when the family-1 attachment countdown reaches zero, after its charge.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyCastFamily1Animation = {
    .records           = _gM4a1ParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyCastFamily1AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyCastFamily1AnimationBank4, NULL, NULL, NULL },
};

/// Part-4 hit reaction translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1Part4HitReactionAnimationBank1[5] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05284_bank1.inc"
};

/// Part-4 hit reaction rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1Part4HitReactionAnimationBank4[55] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05284_bank4.inc"
};

/// Part-4 hit reaction keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1Part4HitReactionAnimationRecords[83] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05284_records.inc"
};

/// Part-4 hit reaction record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1Part4HitReactionAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05284_indices.inc"
};

/// Player-rig hit reaction for the model-part-4 collision sphere in M4A1-family bank entry 16.
///
/// Selected for hit region 1, with a three-frame blend across child slots 1..18.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1Part4HitReactionAnimation = {
    .records           = _gM4a1Part4HitReactionAnimationRecords,
    .trackStartIndices = _gM4a1Part4HitReactionAnimationIndices,
    .poseBanks         = { NULL, _gM4a1Part4HitReactionAnimationBank1, NULL, NULL, _gM4a1Part4HitReactionAnimationBank4, NULL, NULL, NULL },
};

/// Other-body hit reaction translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1OtherBodyHitReactionAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_055A4_bank1.inc"
};

/// Other-body hit reaction rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1OtherBodyHitReactionAnimationBank4[66] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_055A4_bank4.inc"
};

/// Other-body hit reaction keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1OtherBodyHitReactionAnimationRecords[96] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_055A4_records.inc"
};

/// Other-body hit reaction record starts for tracks 0..18, then a retained exporter halfword.
static const u16 _gM4a1OtherBodyHitReactionAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_055A4_indices.inc"
};

/// Player-rig hit reaction for other body regions in M4A1-family bank entry 17.
///
/// Selected when the hit region differs from the model-part-4 region, with a
/// three-frame blend across child slots 1..18. Nineteen tracks borrow the loaded
/// M4A1-family overlay throughout playback.
static AnimationSet _gM4a1OtherBodyHitReactionAnimation = {
    .records           = _gM4a1OtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gM4a1OtherBodyHitReactionAnimationIndices,
    .poseBanks         = { NULL, _gM4a1OtherBodyHitReactionAnimationBank1, NULL, NULL, _gM4a1OtherBodyHitReactionAnimationBank4, NULL, NULL, NULL },
};

/// Death translation/rotation keyframes in integer model units and 1/4096 turns.
///
/// Read-only clip payloads retain their serialized positions in .data.
static const AnimationPackedPose _gM4a1DeathAnimationBank1[18] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05D68_bank1.inc"
};

/// Death rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1DeathAnimationBank4[184] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05D68_bank4.inc"
};

/// Death keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1DeathAnimationRecords[239] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05D68_records.inc"
};

/// Death record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1DeathAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_05D68_indices.inc"
};

/// Player-rig death clip in M4A1-family bank entry 18.
///
/// Played at zero HP with stopped movement and turning; its final poses hold.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1DeathAnimation = {
    .records           = _gM4a1DeathAnimationRecords,
    .trackStartIndices = _gM4a1DeathAnimationIndices,
    .poseBanks         = { NULL, _gM4a1DeathAnimationBank1, NULL, NULL, _gM4a1DeathAnimationBank4, NULL, NULL, NULL },
};

/// Healthy idle translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HealthyIdleAnimationBank1[29] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07000_bank1.inc"
};

/// Healthy idle rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1HealthyIdleAnimationBank4[450] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07000_bank4.inc"
};

/// Healthy idle keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1HealthyIdleAnimationRecords[633] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07000_records.inc"
};

/// Healthy idle record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1HealthyIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07000_indices.inc"
};

/// Player-rig healthy idle loop in M4A1-family bank entry 23.
///
/// Selected after 300 direction-free idle ticks when HP exceeds half maximum HP.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1HealthyIdleAnimation = {
    .records           = _gM4a1HealthyIdleAnimationRecords,
    .trackStartIndices = _gM4a1HealthyIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1HealthyIdleAnimationBank1, NULL, NULL, _gM4a1HealthyIdleAnimationBank4, NULL, NULL, NULL },
};

/// Critical idle translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1CriticalIdleAnimationBank1[12] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07B78_bank1.inc"
};

/// Critical idle rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1CriticalIdleAnimationBank4[266] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07B78_bank4.inc"
};

/// Critical idle keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1CriticalIdleAnimationRecords[412] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07B78_records.inc"
};

/// Critical idle record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1CriticalIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_07B78_indices.inc"
};

/// Player-rig critical-health idle loop in M4A1-family bank entry 25.
///
/// Selected after 300 direction-free idle ticks at or below quarter maximum HP.
/// Also used during paralysis. Nineteen tracks borrow the loaded M4A1-family
/// overlay throughout playback.
static AnimationSet _gM4a1CriticalIdleAnimation = {
    .records           = _gM4a1CriticalIdleAnimationRecords,
    .trackStartIndices = _gM4a1CriticalIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1CriticalIdleAnimationBank1, NULL, NULL, _gM4a1CriticalIdleAnimationBank4, NULL, NULL, NULL },
};

/// Injured idle translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1InjuredIdleAnimationBank1[9] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08294_bank1.inc"
};

/// Injured idle rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1InjuredIdleAnimationBank4[144] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08294_bank4.inc"
};

/// Injured idle keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1InjuredIdleAnimationRecords[264] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08294_records.inc"
};

/// Injured idle record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1InjuredIdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08294_indices.inc"
};

/// Player-rig injured idle loop in M4A1-family bank entry 24.
///
/// Selected after 300 direction-free idle ticks above quarter maximum HP and
/// at or below half maximum HP. Nineteen tracks borrow the loaded M4A1-family
/// overlay throughout playback.
static AnimationSet _gM4a1InjuredIdleAnimation = {
    .records           = _gM4a1InjuredIdleAnimationRecords,
    .trackStartIndices = _gM4a1InjuredIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1InjuredIdleAnimationBank1, NULL, NULL, _gM4a1InjuredIdleAnimationBank4, NULL, NULL, NULL },
};

/// Event-entry translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1EventEntryAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08714_bank1.inc"
};

/// Event-entry rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1EventEntryAnimationBank4[107] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08714_bank4.inc"
};

/// Event-entry keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1EventEntryAnimationRecords[143] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08714_records.inc"
};

/// Event-entry record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1EventEntryAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08714_indices.inc"
};

/// Player-rig scripted-event entry clip in M4A1-family bank entries 32 and 34.
///
/// A zero stored presentation selector plays entry 32 before a scripted interaction;
/// its final poses hold until further playback is requested. Nineteen tracks
/// borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1EventEntryAnimation = {
    .records           = _gM4a1EventEntryAnimationRecords,
    .trackStartIndices = _gM4a1EventEntryAnimationIndices,
    .poseBanks         = { NULL, _gM4a1EventEntryAnimationBank1, NULL, NULL, _gM4a1EventEntryAnimationBank4, NULL, NULL, NULL },
};

/// Event-exit translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1EventExitAnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_088EC_bank1.inc"
};

/// Event-exit rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1EventExitAnimationBank4[32] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_088EC_bank4.inc"
};

/// Event-exit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1EventExitAnimationRecords[57] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_088EC_records.inc"
};

/// Event-exit record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1EventExitAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_088EC_indices.inc"
};

/// Player-rig scripted-event exit clip in M4A1-family bank entries 33 and 35.
///
/// A nonzero stored presentation selector plays entry 33 after a scripted interaction,
/// before ordinary control resumes. Nineteen tracks borrow the loaded
/// M4A1-family overlay throughout playback.
static AnimationSet _gM4a1EventExitAnimation = {
    .records           = _gM4a1EventExitAnimationRecords,
    .trackStartIndices = _gM4a1EventExitAnimationIndices,
    .poseBanks         = { NULL, _gM4a1EventExitAnimationBank1, NULL, NULL, _gM4a1EventExitAnimationBank4, NULL, NULL, NULL },
};

/// PE family-2 release translation/rotation keyframes in model units and 1/4096 turns.
///
/// Read-only clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM4a1ParasiteEnergyReleaseFamily2AnimationBank1[11] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08E50_bank1.inc"
};

/// PE family-2 release rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyReleaseFamily2AnimationBank4[125] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08E50_bank4.inc"
};

/// PE family-2 release keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyReleaseFamily2AnimationRecords[167] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08E50_records.inc"
};

/// PE family-2 release record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1ParasiteEnergyReleaseFamily2AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_08E50_indices.inc"
};

/// Player-rig Parasite Energy family-2 release gesture in M4A1-family bank entry 42.
///
/// Starts the release/charge/cast sequence while the attachment countdown begins.
/// Family 2 covers spell IDs below 601 whose decimal tens digit is 3.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyReleaseFamily2Animation = {
    .records           = _gM4a1ParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyReleaseFamily2AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyReleaseFamily2AnimationBank4, NULL, NULL, NULL },
};

/// PE family-2 charge translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyChargeFamily2AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09044_bank1.inc"
};

/// PE family-2 charge rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyChargeFamily2AnimationBank4[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09044_bank4.inc"
};

/// PE family-2 charge keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyChargeFamily2AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09044_records.inc"
};

/// PE family-2 charge record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1ParasiteEnergyChargeFamily2AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09044_indices.inc"
};

/// Player-rig Parasite Energy family-2 charge loop in M4A1-family bank entry 43.
///
/// Runs after `_gM4a1ParasiteEnergyReleaseFamily2Animation` until the attachment
/// countdown reaches zero and the cast starts, unless the action is cancelled.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyChargeFamily2Animation = {
    .records           = _gM4a1ParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyChargeFamily2AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyChargeFamily2AnimationBank4, NULL, NULL, NULL },
};

/// PE family-2 cast translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ParasiteEnergyCastFamily2AnimationBank1[8] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_094C4_bank1.inc"
};

/// PE family-2 cast rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ParasiteEnergyCastFamily2AnimationBank4[105] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_094C4_bank4.inc"
};

/// PE family-2 cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ParasiteEnergyCastFamily2AnimationRecords[139] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_094C4_records.inc"
};

/// PE family-2 cast record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1ParasiteEnergyCastFamily2AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_094C4_indices.inc"
};

/// Player-rig Parasite Energy family-2 cast gesture in M4A1-family bank entry 44.
///
/// Plays when the family-2 attachment countdown reaches zero, after its charge.
/// Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1ParasiteEnergyCastFamily2Animation = {
    .records           = _gM4a1ParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gM4a1ParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = { NULL, _gM4a1ParasiteEnergyCastFamily2AnimationBank1, NULL, NULL, _gM4a1ParasiteEnergyCastFamily2AnimationBank4, NULL, NULL, NULL },
};

/// Aim-hold translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1AimHoldAnimationBank1[2] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_096A0_bank1.inc"
};

/// Aim-hold rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1AimHoldAnimationBank4[17] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_096A0_bank4.inc"
};

/// Aim-hold keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1AimHoldAnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_096A0_records.inc"
};

/// Aim-hold record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1AimHoldAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_096A0_indices.inc"
};

/// Player-rig stationary rifle aim hold in M4A1-family bank entry 9.
///
/// Holds aim after entry and prepares each firing sequence. Each track loops
/// over two references to the same pose. Nineteen tracks borrow the loaded
/// M4A1-family overlay throughout playback.
static AnimationSet _gM4a1AimHoldAnimation = {
    .records           = _gM4a1AimHoldAnimationRecords,
    .trackStartIndices = _gM4a1AimHoldAnimationIndices,
    .poseBanks         = { NULL, _gM4a1AimHoldAnimationBank1, NULL, NULL, _gM4a1AimHoldAnimationBank4, NULL, NULL, NULL },
};

/// Backward/turn aim translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1AimBackwardOrTurnAnimationBank1[14] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09E88_bank1.inc"
};

/// Backward/turn aim rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1AimBackwardOrTurnAnimationBank4[189] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09E88_bank4.inc"
};

/// Backward/turn aim keyframes, footstep cues and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1AimBackwardOrTurnAnimationRecords[255] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09E88_records.inc"
};

/// Backward/turn aim record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1AimBackwardOrTurnAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_09E88_indices.inc"
};

/// Player-rig rifle-aim backward walk or stationary turn in M4A1-family bank entry 13.
///
/// The same clip serves backward movement and turning without forward movement.
/// Child tracks loop after their lead-in; track 1 supplies the paired footstep
/// cues. Nineteen tracks borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1AimBackwardOrTurnAnimation = {
    .records           = _gM4a1AimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM4a1AimBackwardOrTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1AimBackwardOrTurnAnimationBank1, NULL, NULL, _gM4a1AimBackwardOrTurnAnimationBank4, NULL, NULL, NULL },
};

/// Forward aim-walk translation/rotation keyframes in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1AimForwardWalkAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0A930_bank1.inc"
};

/// Forward aim-walk rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1AimForwardWalkAnimationBank4[269] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0A930_bank4.inc"
};

/// Forward aim-walk keyframes, footstep cues and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1AimForwardWalkAnimationRecords[336] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0A930_records.inc"
};

/// Forward aim-walk record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1AimForwardWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0A930_indices.inc"
};

/// Player-rig forward walk while aiming the rifle, in M4A1-family bank entry 12.
///
/// Child tracks loop after their lead-in; track 1 supplies the paired footstep
/// cues. Root displacement comes from the actor's movement step. Nineteen tracks
/// borrow the loaded M4A1-family overlay throughout playback.
static AnimationSet _gM4a1AimForwardWalkAnimation = {
    .records           = _gM4a1AimForwardWalkAnimationRecords,
    .trackStartIndices = _gM4a1AimForwardWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1AimForwardWalkAnimationBank1, NULL, NULL, _gM4a1AimForwardWalkAnimationBank4, NULL, NULL, NULL },
};

/// Reload translation/rotation keyframes in integer model units and 1/4096 turns.
///
/// Read-only clip data retains its serialized position in .data.
static const AnimationPackedPose _gM4a1ReloadAnimationBank1[26] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0B894_bank1.inc"
};

/// Reload rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ReloadAnimationBank4[416] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0B894_bank4.inc"
};

/// Reload keyframes, loading cues and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ReloadAnimationRecords[471] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0B894_records.inc"
};

/// Reload record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1ReloadAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0B894_indices.inc"
};

/// Player-rig rifle reload shared by M4A1-family bank entries 14, 15 and 20.
///
/// Child tracks stop at the final pose; track 1 supplies two loading cues.
/// Nineteen tracks borrow the loaded weapon overlay throughout playback,
/// including while a caller retains a cue-record pointer.
static AnimationSet _gM4a1ReloadAnimation = {
    .records           = _gM4a1ReloadAnimationRecords,
    .trackStartIndices = _gM4a1ReloadAnimationIndices,
    .poseBanks         = { NULL, _gM4a1ReloadAnimationBank1, NULL, NULL, _gM4a1ReloadAnimationBank4, NULL, NULL, NULL },
};

/// Firing translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1FireAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0BC60_bank1.inc"
};

/// Firing rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1FireAnimationBank4[80] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0BC60_bank4.inc"
};

/// Firing keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1FireAnimationRecords[125] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0BC60_records.inc"
};

/// Firing record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1FireAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0BC60_indices.inc"
};

/// Player-rig rifle firing pose shared by M4A1-family bank entries 10 and 11.
///
/// Each fired round blends the child tracks into this clip; they stop at the
/// final pose. Shot timing belongs to the attack state rather than clip cues.
/// Nineteen tracks borrow the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1FireAnimation = {
    .records           = _gM4a1FireAnimationRecords,
    .trackStartIndices = _gM4a1FireAnimationIndices,
    .poseBanks         = { NULL, _gM4a1FireAnimationBank1, NULL, NULL, _gM4a1FireAnimationBank4, NULL, NULL, NULL },
};

/// Aim-entry translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1AimEntryAnimationBank1[13] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0C3A0_bank1.inc"
};

/// Aim-entry rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1AimEntryAnimationBank4[182] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0C3A0_bank4.inc"
};

/// Aim-entry keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1AimEntryAnimationRecords[223] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0C3A0_records.inc"
};

/// Aim-entry record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1AimEntryAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0C3A0_indices.inc"
};

/// Player-rig transition into rifle aim, in M4A1-family bank entry 7.
///
/// Child tracks stop at the final pose. Enemy grab and hit recovery can also
/// borrow this set into their player-animation tables. All nineteen tracks
/// remain borrowed from the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1AimEntryAnimation = {
    .records           = _gM4a1AimEntryAnimationRecords,
    .trackStartIndices = _gM4a1AimEntryAnimationIndices,
    .poseBanks         = { NULL, _gM4a1AimEntryAnimationBank1, NULL, NULL, _gM4a1AimEntryAnimationBank4, NULL, NULL, NULL },
};

/// Aim-exit translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1AimExitAnimationBank1[18] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0CDD0_bank1.inc"
};

/// Aim-exit rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1AimExitAnimationBank4[266] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0CDD0_bank4.inc"
};

/// Aim-exit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1AimExitAnimationRecords[312] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0CDD0_records.inc"
};

/// Aim-exit record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1AimExitAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0CDD0_indices.inc"
};

/// Player-rig transition out of rifle aim, in M4A1-family bank entry 8.
///
/// Normal and scripted attacks use this clip when leaving aim. Child tracks
/// stop at the final pose; all nineteen tracks borrow the loaded weapon overlay
/// throughout playback.
static AnimationSet _gM4a1AimExitAnimation = {
    .records           = _gM4a1AimExitAnimationRecords,
    .trackStartIndices = _gM4a1AimExitAnimationIndices,
    .poseBanks         = { NULL, _gM4a1AimExitAnimationBank1, NULL, NULL, _gM4a1AimExitAnimationBank4, NULL, NULL, NULL },
};

/// Forward-walk translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1ForwardWalkAnimationBank1[18] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0D688_bank1.inc"
};

/// Forward-walk rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1ForwardWalkAnimationBank4[201] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0D688_bank4.inc"
};

/// Forward-walk keyframes, footstep cues and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1ForwardWalkAnimationRecords[283] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0D688_records.inc"
};

/// Forward-walk record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1ForwardWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0D688_indices.inc"
};

/// Player-rig forward walk with the rifle attached, in M4A1-family bank entry 2.
///
/// Child tracks loop; track 1 supplies the paired footstep cues. Root
/// displacement comes from the actor's movement step. Nineteen tracks borrow
/// the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1ForwardWalkAnimation = {
    .records           = _gM4a1ForwardWalkAnimationRecords,
    .trackStartIndices = _gM4a1ForwardWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1ForwardWalkAnimationBank1, NULL, NULL, _gM4a1ForwardWalkAnimationBank4, NULL, NULL, NULL },
};

/// Backward-walk translation/rotation keyframes in integer model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BackwardWalkAnimationBank1[16] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0DE28_bank1.inc"
};

/// Backward-walk rotation-only keyframes, with packed angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BackwardWalkAnimationBank4[157] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0DE28_bank4.inc"
};

/// Backward-walk keyframes, footstep cues and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BackwardWalkAnimationRecords[263] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0DE28_records.inc"
};

/// Backward-walk record starts for tracks 0..18; the final serialized halfword is unused.
static const u16 _gM4a1BackwardWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0DE28_indices.inc"
};

/// Player-rig backward walk in the M4A1-family bank, entry 3.
///
/// Child tracks loop; track 1 supplies the paired footstep cues. Root
/// displacement comes from the actor's movement step. Nineteen tracks borrow
/// the loaded weapon overlay throughout playback.
static AnimationSet _gM4a1BackwardWalkAnimation = {
    .records           = _gM4a1BackwardWalkAnimationRecords,
    .trackStartIndices = _gM4a1BackwardWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1BackwardWalkAnimationBank1, NULL, NULL, _gM4a1BackwardWalkAnimationBank4, NULL, NULL, NULL },
};

/// Translation/Euler keyframes for `_gM4a1RunAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1RunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0E7FC_bank1.inc"
};

/// Rotation-only keyframes for `_gM4a1RunAnimation`, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1RunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0E7FC_bank4.inc"
};

/// Keyframes, paired footstep cues and loop controls for `_gM4a1RunAnimation`.
static const AnimationRecord _gM4a1RunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0E7FC_records.inc"
};

/// Record starts for `_gM4a1RunAnimation`'s tracks 0..18, followed by an unused serialized halfword.
static const u16 _gM4a1RunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_animation_0E7FC_indices.inc"
};

/// Player running clip in M4A1-family native-bank slot 4.
///
/// The base rifle and both upgrades supply this nineteen-track clip for normal
/// locomotion and scripted run-to actions. Moving tracks repeat a 20-frame
/// loop after their initial keyframes; track 1 carries paired footstep cues.
/// Root displacement is applied by the actor separately. Keep the selected
/// package's borrowed clip data loaded throughout playback.
static AnimationSet _gM4a1RunAnimation = {
    .records           = _gM4a1RunAnimationRecords,
    .trackStartIndices = _gM4a1RunAnimationIndices,
    .poseBanks         = {
        [M4A1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1RunAnimationBank1,
        [M4A1_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1RunAnimationBank4,
    },
};

AnimationBank gM4a1PlayerAnimationBank = {
    .table = {
        .sets = {
            NULL,
            &_gM4a1IdleAnimation,
            &_gM4a1ForwardWalkAnimation,
            &_gM4a1BackwardWalkAnimation,
            &_gM4a1RunAnimation,
            &_gM4a1NegativeTurnAnimation,
            &_gM4a1PositiveTurnAnimation,
            &_gM4a1AimEntryAnimation,
            &_gM4a1AimExitAnimation,
            &_gM4a1AimHoldAnimation,
            &_gM4a1FireAnimation,
            &_gM4a1FireAnimation,
            &_gM4a1AimForwardWalkAnimation,
            &_gM4a1AimBackwardOrTurnAnimation,
            &_gM4a1ReloadAnimation,
            &_gM4a1ReloadAnimation,
            &_gM4a1Part4HitReactionAnimation,
            &_gM4a1OtherBodyHitReactionAnimation,
            &_gM4a1DeathAnimation,
            &_gM4a1UnarmedWalkAnimation,
            &_gM4a1ReloadAnimation,
            &_gM4a1IdleAnimation,
            &_gM4a1IdleAnimation,
            &_gM4a1HealthyIdleAnimation,
            &_gM4a1InjuredIdleAnimation,
            &_gM4a1CriticalIdleAnimation,
            &_gM4a1ParasiteEnergyReleaseFamily0Animation,
            &_gM4a1ParasiteEnergyChargeFamily0Animation,
            &_gM4a1ParasiteEnergyCastFamily0Animation,
            &_gM4a1ParasiteEnergyReleaseFamily1Animation,
            &_gM4a1ParasiteEnergyChargeFamily1Animation,
            &_gM4a1ParasiteEnergyCastFamily1Animation,
            &_gM4a1EventEntryAnimation,
            &_gM4a1EventExitAnimation,
            &_gM4a1EventEntryAnimation,
            &_gM4a1EventExitAnimation,
            &_gM4a1StairAscentAnimation,
            &_gM4a1StairDescentAnimation,
            &_gM4a1StairAscentFinishEvenAnimation,
            &_gM4a1StairAscentFinishOddAnimation,
            &_gM4a1ItemUseAnimation,
            &_gM4a1IdleAnimation,
            &_gM4a1ParasiteEnergyReleaseFamily2Animation,
            &_gM4a1ParasiteEnergyChargeFamily2Animation,
            &_gM4a1ParasiteEnergyCastFamily2Animation,
            // Unused base slots and all raw extension words start cleared.
        },
    },
};

PACKAGE_ALIASES
