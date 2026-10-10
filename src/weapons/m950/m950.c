#include "weapons/m950.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"

#include "weapons/weapon.h"

void m950AttackState(Task* playerTask)
{
    enum {
        M950_PHASE_PREPARE             = 0,
        M950_PHASE_WAIT_READY          = 1,
        M950_PHASE_ARM_SHOT            = 2,
        M950_PHASE_SHOT_DELAY          = 3,
        M950_PHASE_IMPACT              = 4,
        M950_PHASE_RECOVER             = 5,
        M950_PLAYER_ATTACK_STATE       = 4,
        M950_ANIMATION_READY           = 9,
        M950_ANIMATION_PRIMARY         = 0xA,
        M950_READY_BLEND_FRAMES        = 1,
        M950_MOVING_READY_BLEND_FRAMES = 5,
        M950_IMPACT_SOUND              = SOUND_COMMON(0x17),
        M950_SHOT_DELAY_FRAMES         = 4,
        M950_CANCEL_FRAMES             = 9,
        M950_WEAPON_ID                 = 3,
        M950_FIRE_SOUND                = SOUND_WEAPON(3, 4),
    };
    GameActor* actor;
    GfxCoord*  weaponCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    weaponCoord = actor->equipmentTasks[1]->extra.tmd->coords;
    switch (actor->statePhase) {
        case M950_PHASE_PREPARE:
            actor->state                                          = M950_PLAYER_ATTACK_STATE;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            readyBlendFrames = M950_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M950_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M950_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M950_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M950_PHASE_ARM_SHOT:
        fire:
            // Rearming falls through so this dispatch counts toward the four-tick delay.
            actor->statePhase                  = M950_PHASE_SHOT_DELAY;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = M950_SHOT_DELAY_FRAMES;
            /* fallthrough */
        case M950_PHASE_SHOT_DELAY:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                equipmentConsumeWeaponLoad(WEAPON_ITEM(M950_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M950_FIRE_SOUND, 1);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, weaponCoord, 3, NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, M950_ANIMATION_PRIMARY, 0, 2);
            }
            break;
        case M950_PHASE_IMPACT:
            actor->attackCancelTicks = M950_CANCEL_FRAMES;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, weaponCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, M950_IMPACT_SOUND, 1);
            }
            /* fallthrough */
        case M950_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// Initial identity transform and self-parented root of the rigid M950 model.
///
/// Read-only source tables retain their serialized positions in .data.
static const TmdBone _gM950WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/m950_model_00548_skeleton.inc"
};

/// Vertex count of the weapon's sole skeletal part: all 36 vertices.
static const u32 _gM950WeaponModelPartVerts[ARRAY_SIZE(_gM950WeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m950_model_00548_partVerts.inc"
};

/// Writable M950 vertices in integer coordinates local to the weapon root.
static SVECTOR _gM950WeaponModelVerts[] = {
#include "assets/m950_model_00548_verts.inc"
};

/// Writable M950 lighting normals in part-local axes, with twelve fractional bits.
static SVECTOR _gM950WeaponModelNormals[] = {
#include "assets/m950_model_00548_normals.inc"
};

/// Writable commands for four textured triangles and 28 textured quads.
///
/// Handler slots resolve in place. The skeletal group and empty final group
/// each end with `TMD_STREAM_GROUP_END`; `TMD_STREAM_END` closes the stream.
static u32 _gM950WeaponModelStream[] = {
#include "assets/m950_model_00548_stream.inc"
};

TmdSource gM950WeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (28 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM950WeaponModelSkeleton),
    .partVertexCounts    = _gM950WeaponModelPartVerts,
    .verts               = _gM950WeaponModelVerts,
    .normals             = _gM950WeaponModelNormals,
    .skeleton            = _gM950WeaponModelSkeleton,
    .stream              = _gM950WeaponModelStream,
};

/// Root and body translation/rotation keys for the M950 player's idle pose.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_bank1.inc"
};

/// Packed joint rotations for the M950 player's idle pose, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_bank4.inc"
};

/// Keyframes and loop/stop controls holding the M950 player's idle pose.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_records.inc"
};

/// Record starts for the idle pose's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_indices.inc"
};

/// Stationary idle pose in the M950 player's native animation bank.
///
/// Bank ids 1, 21, 22 and 41 share this nineteen-track pose. The descriptor and
/// its read-only records and banks must remain loaded throughout playback.
static AnimationSet _gM950PlayerIdleAnimation = {
    .records           = _gM950PlayerIdleAnimationRecords,
    .trackStartIndices = _gM950PlayerIdleAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerIdleAnimationBank1, NULL, NULL, _gM950PlayerIdleAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for the M950 bank's unarmed forward walk.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_bank1.inc"
};

/// Packed joint rotations for the unarmed forward walk, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_bank4.inc"
};

/// Keyframes and loop/stop controls for the M950 bank's unarmed forward walk.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_records.inc"
};

/// Record starts for the unarmed forward walk's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_indices.inc"
};

/// Unarmed forward-walk clip at id 19 in the M950 player's native animation bank.
///
/// Selected while the weapon child is absent. Its nineteen player model-part
/// tracks and all borrowed clip data must remain loaded throughout playback.
static AnimationSet _gM950PlayerUnarmedWalkAnimation = {
    .records           = _gM950PlayerUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM950PlayerUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerUnarmedWalkAnimationBank1, NULL, NULL, _gM950PlayerUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for forward walking while holding the M950.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_bank1.inc"
};

/// Packed joint rotations for the M950 forward walk, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_bank4.inc"
};

/// Keyframes and loop/stop controls for forward walking while holding the M950.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_records.inc"
};

/// Record starts for the armed forward walk's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_indices.inc"
};

/// Armed forward-walk clip at id 2 in the M950 player's native animation bank.
///
/// Its nineteen player model-part tracks and all borrowed clip data must remain
/// loaded throughout playback.
static AnimationSet _gM950PlayerWalkAnimation = {
    .records           = _gM950PlayerWalkAnimationRecords,
    .trackStartIndices = _gM950PlayerWalkAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerWalkAnimationBank1, NULL, NULL, _gM950PlayerWalkAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for backward walking in the M950 bank.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_bank1.inc"
};

/// Packed joint rotations for the backward walk, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_bank4.inc"
};

/// Keyframes and loop/stop controls for backward walking in the M950 bank.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_records.inc"
};

/// Record starts for the backward walk's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_indices.inc"
};

/// Backward-walk clip at id 3 in the M950 player's native animation bank.
///
/// Its nineteen player model-part tracks and all borrowed clip data must remain
/// loaded throughout playback.
static AnimationSet _gM950PlayerBackwardWalkAnimation = {
    .records           = _gM950PlayerBackwardWalkAnimationRecords,
    .trackStartIndices = _gM950PlayerBackwardWalkAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerBackwardWalkAnimationBank1, NULL, NULL, _gM950PlayerBackwardWalkAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for running in the M950 bank.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerRunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_bank1.inc"
};

/// Packed joint rotations for the run, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_bank4.inc"
};

/// Keyframes and loop/stop controls for running in the M950 bank.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_records.inc"
};

/// Record starts for the run's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_indices.inc"
};

/// Run clip at id 4 in the M950 player's native animation bank.
///
/// Its nineteen player model-part tracks and all borrowed clip data must remain
/// loaded throughout playback.
static AnimationSet _gM950PlayerRunAnimation = {
    .records           = _gM950PlayerRunAnimationRecords,
    .trackStartIndices = _gM950PlayerRunAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerRunAnimationBank1, NULL, NULL, _gM950PlayerRunAnimationBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation032ECBank1[19] = {
#include "assets/m950_animation_032EC_bank1.inc"
};

static AnimationPackedRotation _gM950Animation032ECBank4[169] = {
#include "assets/m950_animation_032EC_bank4.inc"
};

static AnimationRecord _gM950Animation032ECRecords[290] = {
#include "assets/m950_animation_032EC_records.inc"
};

static u16 _gM950Animation032ECIndices[20] = {
#include "assets/m950_animation_032EC_indices.inc"
};

static AnimationSet _gM950Animation032EC = {
    _gM950Animation032ECRecords,
    _gM950Animation032ECIndices,
    { NULL, _gM950Animation032ECBank1, NULL, NULL, _gM950Animation032ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation03B50Bank1[19] = {
#include "assets/m950_animation_03B50_bank1.inc"
};

static AnimationPackedRotation _gM950Animation03B50Bank4[170] = {
#include "assets/m950_animation_03B50_bank4.inc"
};

static AnimationRecord _gM950Animation03B50Records[290] = {
#include "assets/m950_animation_03B50_records.inc"
};

static u16 _gM950Animation03B50Indices[20] = {
#include "assets/m950_animation_03B50_indices.inc"
};

static AnimationSet _gM950Animation03B50 = {
    _gM950Animation03B50Records,
    _gM950Animation03B50Indices,
    { NULL, _gM950Animation03B50Bank1, NULL, NULL, _gM950Animation03B50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation03E64Bank1[3] = {
#include "assets/m950_animation_03E64_bank1.inc"
};

static AnimationPackedRotation _gM950Animation03E64Bank4[69] = {
#include "assets/m950_animation_03E64_bank4.inc"
};

static AnimationRecord _gM950Animation03E64Records[99] = {
#include "assets/m950_animation_03E64_records.inc"
};

static u16 _gM950Animation03E64Indices[20] = {
#include "assets/m950_animation_03E64_indices.inc"
};

static AnimationSet _gM950Animation03E64 = {
    _gM950Animation03E64Records,
    _gM950Animation03E64Indices,
    { NULL, _gM950Animation03E64Bank1, NULL, NULL, _gM950Animation03E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation045C0Bank1[14] = {
#include "assets/m950_animation_045C0_bank1.inc"
};

static AnimationPackedRotation _gM950Animation045C0Bank4[156] = {
#include "assets/m950_animation_045C0_bank4.inc"
};

static AnimationRecord _gM950Animation045C0Records[253] = {
#include "assets/m950_animation_045C0_records.inc"
};

static u16 _gM950Animation045C0Indices[20] = {
#include "assets/m950_animation_045C0_indices.inc"
};

static AnimationSet _gM950Animation045C0 = {
    _gM950Animation045C0Records,
    _gM950Animation045C0Indices,
    { NULL, _gM950Animation045C0Bank1, NULL, NULL, _gM950Animation045C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation04D48Bank1[16] = {
#include "assets/m950_animation_04D48_bank1.inc"
};

static AnimationPackedRotation _gM950Animation04D48Bank4[167] = {
#include "assets/m950_animation_04D48_bank4.inc"
};

static AnimationRecord _gM950Animation04D48Records[247] = {
#include "assets/m950_animation_04D48_records.inc"
};

static u16 _gM950Animation04D48Indices[20] = {
#include "assets/m950_animation_04D48_indices.inc"
};

static AnimationSet _gM950Animation04D48 = {
    _gM950Animation04D48Records,
    _gM950Animation04D48Indices,
    { NULL, _gM950Animation04D48Bank1, NULL, NULL, _gM950Animation04D48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0501CBank1[6] = {
#include "assets/m950_animation_0501C_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0501CBank4[52] = {
#include "assets/m950_animation_0501C_bank4.inc"
};

static AnimationRecord _gM950Animation0501CRecords[91] = {
#include "assets/m950_animation_0501C_records.inc"
};

static u16 _gM950Animation0501CIndices[20] = {
#include "assets/m950_animation_0501C_indices.inc"
};

static AnimationSet _gM950Animation0501C = {
    _gM950Animation0501CRecords,
    _gM950Animation0501CIndices,
    { NULL, _gM950Animation0501CBank1, NULL, NULL, _gM950Animation0501CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation053ACBank1[7] = {
#include "assets/m950_animation_053AC_bank1.inc"
};

static AnimationPackedRotation _gM950Animation053ACBank4[73] = {
#include "assets/m950_animation_053AC_bank4.inc"
};

static AnimationRecord _gM950Animation053ACRecords[114] = {
#include "assets/m950_animation_053AC_records.inc"
};

static u16 _gM950Animation053ACIndices[20] = {
#include "assets/m950_animation_053AC_indices.inc"
};

static AnimationSet _gM950Animation053AC = {
    _gM950Animation053ACRecords,
    _gM950Animation053ACIndices,
    { NULL, _gM950Animation053ACBank1, NULL, NULL, _gM950Animation053ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation05834Bank1[9] = {
#include "assets/m950_animation_05834_bank1.inc"
};

static AnimationPackedRotation _gM950Animation05834Bank4[104] = {
#include "assets/m950_animation_05834_bank4.inc"
};

static AnimationRecord _gM950Animation05834Records[139] = {
#include "assets/m950_animation_05834_records.inc"
};

static u16 _gM950Animation05834Indices[20] = {
#include "assets/m950_animation_05834_indices.inc"
};

static AnimationSet _gM950Animation05834 = {
    _gM950Animation05834Records,
    _gM950Animation05834Indices,
    { NULL, _gM950Animation05834Bank1, NULL, NULL, _gM950Animation05834Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation05A30Bank1[3] = {
#include "assets/m950_animation_05A30_bank1.inc"
};

static AnimationPackedRotation _gM950Animation05A30Bank4[22] = {
#include "assets/m950_animation_05A30_bank4.inc"
};

static AnimationRecord _gM950Animation05A30Records[76] = {
#include "assets/m950_animation_05A30_records.inc"
};

static u16 _gM950Animation05A30Indices[20] = {
#include "assets/m950_animation_05A30_indices.inc"
};

static AnimationSet _gM950Animation05A30 = {
    _gM950Animation05A30Records,
    _gM950Animation05A30Indices,
    { NULL, _gM950Animation05A30Bank1, NULL, NULL, _gM950Animation05A30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation05D08Bank1[6] = {
#include "assets/m950_animation_05D08_bank1.inc"
};

static AnimationPackedRotation _gM950Animation05D08Bank4[57] = {
#include "assets/m950_animation_05D08_bank4.inc"
};

static AnimationRecord _gM950Animation05D08Records[87] = {
#include "assets/m950_animation_05D08_records.inc"
};

static u16 _gM950Animation05D08Indices[20] = {
#include "assets/m950_animation_05D08_indices.inc"
};

static AnimationSet _gM950Animation05D08 = {
    _gM950Animation05D08Records,
    _gM950Animation05D08Indices,
    { NULL, _gM950Animation05D08Bank1, NULL, NULL, _gM950Animation05D08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation05FACBank1[4] = {
#include "assets/m950_animation_05FAC_bank1.inc"
};

static AnimationPackedRotation _gM950Animation05FACBank4[55] = {
#include "assets/m950_animation_05FAC_bank4.inc"
};

static AnimationRecord _gM950Animation05FACRecords[82] = {
#include "assets/m950_animation_05FAC_records.inc"
};

static u16 _gM950Animation05FACIndices[20] = {
#include "assets/m950_animation_05FAC_indices.inc"
};

static AnimationSet _gM950Animation05FAC = {
    _gM950Animation05FACRecords,
    _gM950Animation05FACIndices,
    { NULL, _gM950Animation05FACBank1, NULL, NULL, _gM950Animation05FACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation061ACBank1[3] = {
#include "assets/m950_animation_061AC_bank1.inc"
};

static AnimationPackedRotation _gM950Animation061ACBank4[23] = {
#include "assets/m950_animation_061AC_bank4.inc"
};

static AnimationRecord _gM950Animation061ACRecords[76] = {
#include "assets/m950_animation_061AC_records.inc"
};

static u16 _gM950Animation061ACIndices[20] = {
#include "assets/m950_animation_061AC_indices.inc"
};

static AnimationSet _gM950Animation061AC = {
    _gM950Animation061ACRecords,
    _gM950Animation061ACIndices,
    { NULL, _gM950Animation061ACBank1, NULL, NULL, _gM950Animation061ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation06500Bank1[8] = {
#include "assets/m950_animation_06500_bank1.inc"
};

static AnimationPackedRotation _gM950Animation06500Bank4[68] = {
#include "assets/m950_animation_06500_bank4.inc"
};

static AnimationRecord _gM950Animation06500Records[101] = {
#include "assets/m950_animation_06500_records.inc"
};

static u16 _gM950Animation06500Indices[20] = {
#include "assets/m950_animation_06500_indices.inc"
};

static AnimationSet _gM950Animation06500 = {
    _gM950Animation06500Records,
    _gM950Animation06500Indices,
    { NULL, _gM950Animation06500Bank1, NULL, NULL, _gM950Animation06500Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation067B4Bank1[5] = {
#include "assets/m950_animation_067B4_bank1.inc"
};

static AnimationPackedRotation _gM950Animation067B4Bank4[55] = {
#include "assets/m950_animation_067B4_bank4.inc"
};

static AnimationRecord _gM950Animation067B4Records[83] = {
#include "assets/m950_animation_067B4_records.inc"
};

static u16 _gM950Animation067B4Indices[20] = {
#include "assets/m950_animation_067B4_indices.inc"
};

static AnimationSet _gM950Animation067B4 = {
    _gM950Animation067B4Records,
    _gM950Animation067B4Indices,
    { NULL, _gM950Animation067B4Bank1, NULL, NULL, _gM950Animation067B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation06AD4Bank1[6] = {
#include "assets/m950_animation_06AD4_bank1.inc"
};

static AnimationPackedRotation _gM950Animation06AD4Bank4[66] = {
#include "assets/m950_animation_06AD4_bank4.inc"
};

static AnimationRecord _gM950Animation06AD4Records[96] = {
#include "assets/m950_animation_06AD4_records.inc"
};

static u16 _gM950Animation06AD4Indices[20] = {
#include "assets/m950_animation_06AD4_indices.inc"
};

static AnimationSet _gM950Animation06AD4 = {
    _gM950Animation06AD4Records,
    _gM950Animation06AD4Indices,
    { NULL, _gM950Animation06AD4Bank1, NULL, NULL, _gM950Animation06AD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation07298Bank1[18] = {
#include "assets/m950_animation_07298_bank1.inc"
};

static AnimationPackedRotation _gM950Animation07298Bank4[184] = {
#include "assets/m950_animation_07298_bank4.inc"
};

static AnimationRecord _gM950Animation07298Records[239] = {
#include "assets/m950_animation_07298_records.inc"
};

static u16 _gM950Animation07298Indices[20] = {
#include "assets/m950_animation_07298_indices.inc"
};

static AnimationSet _gM950Animation07298 = {
    _gM950Animation07298Records,
    _gM950Animation07298Indices,
    { NULL, _gM950Animation07298Bank1, NULL, NULL, _gM950Animation07298Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation08530Bank1[29] = {
#include "assets/m950_animation_08530_bank1.inc"
};

static AnimationPackedRotation _gM950Animation08530Bank4[450] = {
#include "assets/m950_animation_08530_bank4.inc"
};

static AnimationRecord _gM950Animation08530Records[633] = {
#include "assets/m950_animation_08530_records.inc"
};

static u16 _gM950Animation08530Indices[20] = {
#include "assets/m950_animation_08530_indices.inc"
};

static AnimationSet _gM950Animation08530 = {
    _gM950Animation08530Records,
    _gM950Animation08530Indices,
    { NULL, _gM950Animation08530Bank1, NULL, NULL, _gM950Animation08530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation090A8Bank1[12] = {
#include "assets/m950_animation_090A8_bank1.inc"
};

static AnimationPackedRotation _gM950Animation090A8Bank4[266] = {
#include "assets/m950_animation_090A8_bank4.inc"
};

static AnimationRecord _gM950Animation090A8Records[412] = {
#include "assets/m950_animation_090A8_records.inc"
};

static u16 _gM950Animation090A8Indices[20] = {
#include "assets/m950_animation_090A8_indices.inc"
};

static AnimationSet _gM950Animation090A8 = {
    _gM950Animation090A8Records,
    _gM950Animation090A8Indices,
    { NULL, _gM950Animation090A8Bank1, NULL, NULL, _gM950Animation090A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation097C4Bank1[9] = {
#include "assets/m950_animation_097C4_bank1.inc"
};

static AnimationPackedRotation _gM950Animation097C4Bank4[144] = {
#include "assets/m950_animation_097C4_bank4.inc"
};

static AnimationRecord _gM950Animation097C4Records[264] = {
#include "assets/m950_animation_097C4_records.inc"
};

static u16 _gM950Animation097C4Indices[20] = {
#include "assets/m950_animation_097C4_indices.inc"
};

static AnimationSet _gM950Animation097C4 = {
    _gM950Animation097C4Records,
    _gM950Animation097C4Indices,
    { NULL, _gM950Animation097C4Bank1, NULL, NULL, _gM950Animation097C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation09C44Bank1[6] = {
#include "assets/m950_animation_09C44_bank1.inc"
};

static AnimationPackedRotation _gM950Animation09C44Bank4[107] = {
#include "assets/m950_animation_09C44_bank4.inc"
};

static AnimationRecord _gM950Animation09C44Records[143] = {
#include "assets/m950_animation_09C44_records.inc"
};

static u16 _gM950Animation09C44Indices[20] = {
#include "assets/m950_animation_09C44_indices.inc"
};

static AnimationSet _gM950Animation09C44 = {
    _gM950Animation09C44Records,
    _gM950Animation09C44Indices,
    { NULL, _gM950Animation09C44Bank1, NULL, NULL, _gM950Animation09C44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation09E1CBank1[3] = {
#include "assets/m950_animation_09E1C_bank1.inc"
};

static AnimationPackedRotation _gM950Animation09E1CBank4[32] = {
#include "assets/m950_animation_09E1C_bank4.inc"
};

static AnimationRecord _gM950Animation09E1CRecords[57] = {
#include "assets/m950_animation_09E1C_records.inc"
};

static u16 _gM950Animation09E1CIndices[20] = {
#include "assets/m950_animation_09E1C_indices.inc"
};

static AnimationSet _gM950Animation09E1C = {
    _gM950Animation09E1CRecords,
    _gM950Animation09E1CIndices,
    { NULL, _gM950Animation09E1CBank1, NULL, NULL, _gM950Animation09E1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0A380Bank1[11] = {
#include "assets/m950_animation_0A380_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0A380Bank4[125] = {
#include "assets/m950_animation_0A380_bank4.inc"
};

static AnimationRecord _gM950Animation0A380Records[167] = {
#include "assets/m950_animation_0A380_records.inc"
};

static u16 _gM950Animation0A380Indices[20] = {
#include "assets/m950_animation_0A380_indices.inc"
};

static AnimationSet _gM950Animation0A380 = {
    _gM950Animation0A380Records,
    _gM950Animation0A380Indices,
    { NULL, _gM950Animation0A380Bank1, NULL, NULL, _gM950Animation0A380Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0A574Bank1[3] = {
#include "assets/m950_animation_0A574_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0A574Bank4[20] = {
#include "assets/m950_animation_0A574_bank4.inc"
};

static AnimationRecord _gM950Animation0A574Records[76] = {
#include "assets/m950_animation_0A574_records.inc"
};

static u16 _gM950Animation0A574Indices[20] = {
#include "assets/m950_animation_0A574_indices.inc"
};

static AnimationSet _gM950Animation0A574 = {
    _gM950Animation0A574Records,
    _gM950Animation0A574Indices,
    { NULL, _gM950Animation0A574Bank1, NULL, NULL, _gM950Animation0A574Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0A9F4Bank1[8] = {
#include "assets/m950_animation_0A9F4_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0A9F4Bank4[105] = {
#include "assets/m950_animation_0A9F4_bank4.inc"
};

static AnimationRecord _gM950Animation0A9F4Records[139] = {
#include "assets/m950_animation_0A9F4_records.inc"
};

static u16 _gM950Animation0A9F4Indices[20] = {
#include "assets/m950_animation_0A9F4_indices.inc"
};

static AnimationSet _gM950Animation0A9F4 = {
    _gM950Animation0A9F4Records,
    _gM950Animation0A9F4Indices,
    { NULL, _gM950Animation0A9F4Bank1, NULL, NULL, _gM950Animation0A9F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0B49CBank1[19] = {
#include "assets/m950_animation_0B49C_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0B49CBank4[269] = {
#include "assets/m950_animation_0B49C_bank4.inc"
};

static AnimationRecord _gM950Animation0B49CRecords[336] = {
#include "assets/m950_animation_0B49C_records.inc"
};

static u16 _gM950Animation0B49CIndices[20] = {
#include "assets/m950_animation_0B49C_indices.inc"
};

static AnimationSet _gM950Animation0B49C = {
    _gM950Animation0B49CRecords,
    _gM950Animation0B49CIndices,
    { NULL, _gM950Animation0B49CBank1, NULL, NULL, _gM950Animation0B49CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0B674Bank1[2] = {
#include "assets/m950_animation_0B674_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0B674Bank4[16] = {
#include "assets/m950_animation_0B674_bank4.inc"
};

static AnimationRecord _gM950Animation0B674Records[76] = {
#include "assets/m950_animation_0B674_records.inc"
};

static u16 _gM950Animation0B674Indices[20] = {
#include "assets/m950_animation_0B674_indices.inc"
};

static AnimationSet _gM950Animation0B674 = {
    _gM950Animation0B674Records,
    _gM950Animation0B674Indices,
    { NULL, _gM950Animation0B674Bank1, NULL, NULL, _gM950Animation0B674Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0BDA8Bank1[12] = {
#include "assets/m950_animation_0BDA8_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0BDA8Bank4[165] = {
#include "assets/m950_animation_0BDA8_bank4.inc"
};

static AnimationRecord _gM950Animation0BDA8Records[240] = {
#include "assets/m950_animation_0BDA8_records.inc"
};

static u16 _gM950Animation0BDA8Indices[20] = {
#include "assets/m950_animation_0BDA8_indices.inc"
};

static AnimationSet _gM950Animation0BDA8 = {
    _gM950Animation0BDA8Records,
    _gM950Animation0BDA8Indices,
    { NULL, _gM950Animation0BDA8Bank1, NULL, NULL, _gM950Animation0BDA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0D914Bank1[50] = {
#include "assets/m950_animation_0D914_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0D914Bank4[712] = {
#include "assets/m950_animation_0D914_bank4.inc"
};

static AnimationRecord _gM950Animation0D914Records[873] = {
#include "assets/m950_animation_0D914_records.inc"
};

static u16 _gM950Animation0D914Indices[20] = {
#include "assets/m950_animation_0D914_indices.inc"
};

static AnimationSet _gM950Animation0D914 = {
    _gM950Animation0D914Records,
    _gM950Animation0D914Indices,
    { NULL, _gM950Animation0D914Bank1, NULL, NULL, _gM950Animation0D914Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0DE14Bank1[9] = {
#include "assets/m950_animation_0DE14_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0DE14Bank4[120] = {
#include "assets/m950_animation_0DE14_bank4.inc"
};

static AnimationRecord _gM950Animation0DE14Records[153] = {
#include "assets/m950_animation_0DE14_records.inc"
};

static u16 _gM950Animation0DE14Indices[20] = {
#include "assets/m950_animation_0DE14_indices.inc"
};

static AnimationSet _gM950Animation0DE14 = {
    _gM950Animation0DE14Records,
    _gM950Animation0DE14Indices,
    { NULL, _gM950Animation0DE14Bank1, NULL, NULL, _gM950Animation0DE14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0E304Bank1[9] = {
#include "assets/m950_animation_0E304_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0E304Bank4[116] = {
#include "assets/m950_animation_0E304_bank4.inc"
};

static AnimationRecord _gM950Animation0E304Records[153] = {
#include "assets/m950_animation_0E304_records.inc"
};

static u16 _gM950Animation0E304Indices[20] = {
#include "assets/m950_animation_0E304_indices.inc"
};

static AnimationSet _gM950Animation0E304 = {
    _gM950Animation0E304Records,
    _gM950Animation0E304Indices,
    { NULL, _gM950Animation0E304Bank1, NULL, NULL, _gM950Animation0E304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation0E890Bank1[10] = {
#include "assets/m950_animation_0E890_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0E890Bank4[136] = {
#include "assets/m950_animation_0E890_bank4.inc"
};

static AnimationRecord _gM950Animation0E890Records[169] = {
#include "assets/m950_animation_0E890_records.inc"
};

static u16 _gM950Animation0E890Indices[20] = {
#include "assets/m950_animation_0E890_indices.inc"
};

static AnimationSet _gM950Animation0E890 = {
    _gM950Animation0E890Records,
    _gM950Animation0E890Indices,
    { NULL, _gM950Animation0E890Bank1, NULL, NULL, _gM950Animation0E890Bank4, NULL, NULL, NULL },
};

AnimationBank D_m950_8012BA78 = { { {
    NULL,
    &_gM950PlayerIdleAnimation,
    &_gM950PlayerWalkAnimation,
    &_gM950PlayerBackwardWalkAnimation,
    &_gM950PlayerRunAnimation,
    &_gM950Animation032EC,
    &_gM950Animation03B50,
    &_gM950Animation0E304,
    &_gM950Animation0E890,
    &_gM950Animation0B674,
    &_gM950Animation0DE14,
    &_gM950Animation0DE14,
    &_gM950Animation0B49C,
    &_gM950Animation0BDA8,
    &_gM950Animation0D914,
    &_gM950Animation0D914,
    &_gM950Animation067B4,
    &_gM950Animation06AD4,
    &_gM950Animation07298,
    &_gM950PlayerUnarmedWalkAnimation,
    &_gM950Animation0D914,
    &_gM950PlayerIdleAnimation,
    &_gM950PlayerIdleAnimation,
    &_gM950Animation08530,
    &_gM950Animation097C4,
    &_gM950Animation090A8,
    &_gM950Animation05834,
    &_gM950Animation05A30,
    &_gM950Animation05D08,
    &_gM950Animation05FAC,
    &_gM950Animation061AC,
    &_gM950Animation06500,
    &_gM950Animation09C44,
    &_gM950Animation09E1C,
    &_gM950Animation09C44,
    &_gM950Animation09E1C,
    &_gM950Animation045C0,
    &_gM950Animation04D48,
    &_gM950Animation053AC,
    &_gM950Animation0501C,
    &_gM950Animation03E64,
    &_gM950PlayerIdleAnimation,
    &_gM950Animation0A380,
    &_gM950Animation0A574,
    &_gM950Animation0A9F4,
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
