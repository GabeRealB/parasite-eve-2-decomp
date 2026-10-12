#include "weapons/as12.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

enum { AS12_AMMUNITION_BUCKSHOT  = 13,
       AS12_FIREFLY_IMPACT_SOUND = SOUND_WEAPON(15, 4) };

/// Requests the AS12 Firefly impact sound at the first recorded weapon contact.
///
/// `firstContact` is a readable `const WorldCollisionContact*` supplying signed-halfword
/// XYZ in view-space game coordinates; `impactCoord` is a writable `GfxCoord*`.
/// The contact need not be the surface selected by the impact picker. Requires
/// the Firefly load selected in live player state and a disjoint writable node.
/// Writes only its three cached translation words, with no jitter, then reads
/// that origin synchronously for sound and latches combat noise. The retained
/// cached rotation is loaded into the GTE, but multiplies a zero input point.
/// Leaves the contact, local matrix, parent and composition stamp untouched.
/// Requires initialized projection/scratch state; retains neither pointer.
///
/// Both pointer arguments repeat and must have no side effects. Reads live
/// `gPlayerStatus.weaponSlotItem`; captures no caller locals. The block is one
/// statement with no return/break; invoke it as a standalone statement inside braces.
#define AS12_PLAY_FIREFLY_IMPACT(firstContact, impactCoord)                                                                                                                          \
    {                                                                                                                                                                                \
        enum { AS12_IMPACT_AMMUNITION_BANK_SHIFT = 24,                                                                                                                               \
               AS12_IMPACT_SIGNAL_NOISE          = 1 };                                                                                                                                       \
        (impactCoord)->workm.t[0] = (firstContact)->point.vx;                                                                                                                        \
        (impactCoord)->workm.t[1] = (firstContact)->point.vy;                                                                                                                        \
        (impactCoord)->workm.t[2] = (firstContact)->point.vz;                                                                                                                        \
        worldCoordPlaySound((impactCoord),                                                                                                                                           \
                            ((gPlayerStatus.weaponSlotItem - AS12_AMMUNITION_BUCKSHOT) << AS12_IMPACT_AMMUNITION_BANK_SHIFT) | AS12_FIREFLY_IMPACT_SOUND, AS12_IMPACT_SIGNAL_NOISE); \
    }

void as12AttackState(Task* playerTask)
{
    enum {
        AS12_PHASE_PREPARE             = 0,
        AS12_PHASE_WAIT_READY          = 1,
        AS12_PHASE_ARM_SHOT            = 2,
        AS12_PHASE_FIRE                = 3,
        AS12_PHASE_IMPACT              = 4,
        AS12_PHASE_RECOVER             = 5,
        AS12_PLAYER_ATTACK_STATE       = 4,
        AS12_ANIMATION_READY           = 9,
        AS12_ANIMATION_PRIMARY         = 0xA,
        AS12_READY_BLEND_FRAMES        = 1,
        AS12_IMPACT_SOUND              = SOUND_COMMON(0x17),
        AS12_WEAPON_ID                 = 15,
        AS12_MOVING_READY_BLEND_FRAMES = 5,
        AS12_COOLDOWN_FRAMES           = 0x21,
        AS12_CANCEL_FRAMES             = 0x16,
        AS12_AMMUNITION_FIREFLY        = 14,
        AS12_FIRE_SOUND                = SOUND_WEAPON(15, 5),
        AS12_EFFECT_AMMUNITION_SHIFT   = 16,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        impactFound;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case AS12_PHASE_PREPARE:
            actor->state          = AS12_PLAYER_ATTACK_STATE;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (gPlayerStatus.weaponSlotItem == AS12_AMMUNITION_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~WORLD_COLLISION_BODY_SINGLE_CONTACT;
            }
            readyBlendFrames = AS12_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = AS12_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, AS12_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case AS12_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case AS12_PHASE_ARM_SHOT:
        fire:
            actor->statePhase                  = AS12_PHASE_FIRE;
            actor->attackControl.cooldownTicks = AS12_COOLDOWN_FRAMES;
            actor->rumblePosted                = 0;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            /* fallthrough */
        case AS12_PHASE_FIRE:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(AS12_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                ((gPlayerStatus.weaponSlotItem - AS12_AMMUNITION_BUCKSHOT) << 0x18) | AS12_FIRE_SOUND, 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << AS12_EFFECT_AMMUNITION_SHIFT) | AS12_WEAPON_ID, NULL);
            playerActorResetChildSlots(playerTask, AS12_ANIMATION_PRIMARY);
            break;
        case AS12_PHASE_IMPACT:
            // Firefly uses the first contact position; other non-Buckshot loads use the picked impact.
            actor->attackCancelTicks = AS12_CANCEL_FRAMES;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != AS12_AMMUNITION_BUCKSHOT) {
                impactFound = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord);
                if (gPlayerStatus.weaponSlotItem == AS12_AMMUNITION_FIREFLY) {
                    if (impactFound != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        AS12_PLAY_FIREFLY_IMPACT(&actor->weaponContacts[0], impactCoord);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(impactCoord, AS12_IMPACT_SOUND, 1);
                }
            }
            /* fallthrough */
        case AS12_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0 && actor->attackControl.cooldownTicks == 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
#undef AS12_PLAY_FIREFLY_IMPACT

/// Initial identity transform and self-parented root of the rigid shotgun model.
///
/// Read-only asset arrays retain their serialized positions in .data.
static const TmdBone _gAs12ModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/as12_model_00660_skeleton.inc"
};

/// Vertex count of the model's sole part, covering all 46 local vertices.
static const u32 _gAs12ModelPartVerts[ARRAY_SIZE(_gAs12ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/as12_model_00660_partVerts.inc"
};

/// Shotgun vertices in integer coordinates local to its single root part.
static SVECTOR _gAs12ModelVerts[] = {
#include "assets/as12_model_00660_verts.inc"
};

/// Part-local lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gAs12ModelNormals[] = {
#include "assets/as12_model_00660_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and 41 quads.
///
/// First creation resolves handler slots in place. The single part group ends
/// with `TMD_STREAM_GROUP_END`, followed by `TMD_STREAM_END`; no pre-transformed
/// region is used.
static u32 _gAs12ModelStream[] = {
#include "assets/as12_model_00660_stream.inc"
};

TmdSource gAs12Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (41 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAs12ModelSkeleton),
    .partVertexCounts    = _gAs12ModelPartVerts,
    .verts               = _gAs12ModelVerts,
    .normals             = _gAs12ModelNormals,
    .skeleton            = _gAs12ModelSkeleton,
    .stream              = _gAs12ModelStream,
};

/// Encoding-indexed banks used by the player clips below; all other banks are absent.
enum {
    AS12_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    AS12_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Translation/rotation keyframes for `_gAs12IdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12IdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_00CE4_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12IdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12IdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_00CE4_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12IdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12IdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_00CE4_records.inc"
};

/// Record-array starts of `_gAs12IdleAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12IdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_00CE4_indices.inc"
};

/// Standing player pose in the AS12 bank, shared by slots 1, 21, 22 and 41.
///
/// Locomotion selects slot 1 when movement and turning stop.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12IdleAnimation = {
    .records           = _gAs12IdleAnimationRecords,
    .trackStartIndices = _gAs12IdleAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12IdleAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12IdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12UnarmedWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12UnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01388_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12UnarmedWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12UnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01388_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12UnarmedWalkAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12UnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01388_records.inc"
};

/// Record-array starts of `_gAs12UnarmedWalkAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12UnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01388_indices.inc"
};

/// Player forward-walk clip in AS12 bank slot 19 while the held model is absent.
///
/// The weapon remains selected; an empty equipment-model slot selects this clip.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12UnarmedWalkAnimation = {
    .records           = _gAs12UnarmedWalkAnimationRecords,
    .trackStartIndices = _gAs12UnarmedWalkAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12UnarmedWalkAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12UnarmedWalkAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gAs12UnusedBackwardWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12UnusedBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01C60_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12UnusedBackwardWalkAnimation`, in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12UnusedBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01C60_bank4.inc"
};

/// Keyframes, paired footstep cues and loop controls for `_gAs12UnusedBackwardWalkAnimation`.
static const AnimationRecord _gAs12UnusedBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01C60_records.inc"
};

/// Record starts for `_gAs12UnusedBackwardWalkAnimation`'s tracks 0..18, followed by an unused serialized halfword.
static const u16 _gAs12UnusedBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_01C60_indices.inc"
};

/// Backward-walk clip retained in the AS12 package but absent from its native bank.
///
/// Nineteen player-part tracks borrow the loaded package's read-only payload.
/// The moving tracks repeat a 33-frame loop after their initial keyframes;
/// track 1 carries the paired footstep cues.
static AnimationSet _gAs12UnusedBackwardWalkAnimation = {
    .records           = _gAs12UnusedBackwardWalkAnimationRecords,
    .trackStartIndices = _gAs12UnusedBackwardWalkAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12UnusedBackwardWalkAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12UnusedBackwardWalkAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12NegativeTurnAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12NegativeTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_024C0_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12NegativeTurnAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12NegativeTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_024C0_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12NegativeTurnAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12NegativeTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_024C0_records.inc"
};

/// Record-array starts of `_gAs12NegativeTurnAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12NegativeTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_024C0_indices.inc"
};

/// Stationary player turn for a negative yaw delta, in AS12 bank slot 5.
///
/// Normal turning and scripted turn-to-yaw requests select this clip.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12NegativeTurnAnimation = {
    .records           = _gAs12NegativeTurnAnimationRecords,
    .trackStartIndices = _gAs12NegativeTurnAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12NegativeTurnAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12NegativeTurnAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PositiveTurnAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PositiveTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_02D24_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PositiveTurnAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PositiveTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_02D24_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PositiveTurnAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12PositiveTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_02D24_records.inc"
};

/// Record-array starts of `_gAs12PositiveTurnAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PositiveTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_02D24_indices.inc"
};

/// Stationary player turn for a positive yaw delta, in AS12 bank slot 6.
///
/// Scripted turn-to-yaw requests also select it for a zero delta.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PositiveTurnAnimation = {
    .records           = _gAs12PositiveTurnAnimationRecords,
    .trackStartIndices = _gAs12PositiveTurnAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PositiveTurnAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PositiveTurnAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12ItemUseAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12ItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03038_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12ItemUseAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12ItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03038_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12ItemUseAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12ItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03038_records.inc"
};

/// Record-array starts of `_gAs12ItemUseAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12ItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03038_indices.inc"
};

/// Player item-use pose sequence in AS12 bank slot 40.
///
/// Scripted item use plays this clip before returning to aim locomotion.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12ItemUseAnimation = {
    .records           = _gAs12ItemUseAnimationRecords,
    .trackStartIndices = _gAs12ItemUseAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12ItemUseAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12ItemUseAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12StairAscentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12StairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03794_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12StairAscentAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12StairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03794_bank4.inc"
};

/// Keyframe durations, footstep cues and controls for `_gAs12StairAscentAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12StairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03794_records.inc"
};

/// Record-array starts of `_gAs12StairAscentAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12StairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03794_indices.inc"
};

/// Player stair-ascent clip in AS12 bank slot 36.
///
/// Scripted flights repeat the stride until the requested stair count is exhausted.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12StairAscentAnimation = {
    .records           = _gAs12StairAscentAnimationRecords,
    .trackStartIndices = _gAs12StairAscentAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12StairAscentAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12StairAscentAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12StairDescentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12StairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03F1C_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12StairDescentAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12StairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03F1C_bank4.inc"
};

/// Keyframe durations, footstep cues and controls for `_gAs12StairDescentAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12StairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03F1C_records.inc"
};

/// Record-array starts of `_gAs12StairDescentAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12StairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_03F1C_indices.inc"
};

/// Player stair-descent clip in AS12 bank slot 37.
///
/// Scripted flights repeat the stride until the requested stair count is exhausted.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12StairDescentAnimation = {
    .records           = _gAs12StairDescentAnimationRecords,
    .trackStartIndices = _gAs12StairDescentAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12StairDescentAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12StairDescentAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12StairAscentFinishOddAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12StairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_041F0_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12StairAscentFinishOddAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12StairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_041F0_bank4.inc"
};

/// Keyframe durations, footstep cues and controls for `_gAs12StairAscentFinishOddAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12StairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_041F0_records.inc"
};

/// Record-array starts of `_gAs12StairAscentFinishOddAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12StairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_041F0_indices.inc"
};

/// Player stair-ascent finish for an odd initial stair count, in AS12 bank slot 39.
///
/// Ends the climb with a level stride before returning to scripted idle.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12StairAscentFinishOddAnimation = {
    .records           = _gAs12StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gAs12StairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12StairAscentFinishOddAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12StairAscentFinishOddAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12StairAscentFinishEvenAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12StairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04580_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12StairAscentFinishEvenAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12StairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04580_bank4.inc"
};

/// Keyframe durations, footstep cues and controls for `_gAs12StairAscentFinishEvenAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12StairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04580_records.inc"
};

/// Record-array starts of `_gAs12StairAscentFinishEvenAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12StairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04580_indices.inc"
};

/// Player stair-ascent finish for an even initial stair count, in AS12 bank slot 38.
///
/// Ends the climb with a level stride before returning to scripted idle.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12StairAscentFinishEvenAnimation = {
    .records           = _gAs12StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gAs12StairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12StairAscentFinishEvenAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12StairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeReleaseFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04A08_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeReleaseFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04A08_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeReleaseFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04A08_records.inc"
};

/// Record-array starts of `_gAs12PeReleaseFamily0Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04A08_indices.inc"
};

/// Family-0 Parasite Energy release clip in AS12 bank slot 26.
///
/// Plays before the charge phase when the selected attachment uses clip family 0.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeReleaseFamily0Animation = {
    .records           = _gAs12PeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gAs12PeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeReleaseFamily0AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeReleaseFamily0AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeChargeFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04C04_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeChargeFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04C04_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeChargeFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04C04_records.inc"
};

/// Record-array starts of `_gAs12PeChargeFamily0Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04C04_indices.inc"
};

/// Family-0 Parasite Energy charge clip in AS12 bank slot 27.
///
/// Repeats while the selected attachment's countdown runs, before the cast clip.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeChargeFamily0Animation = {
    .records           = _gAs12PeChargeFamily0AnimationRecords,
    .trackStartIndices = _gAs12PeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeChargeFamily0AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeChargeFamily0AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeCastFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04EDC_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeCastFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04EDC_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeCastFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04EDC_records.inc"
};

/// Record-array starts of `_gAs12PeCastFamily0Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_04EDC_indices.inc"
};

/// Family-0 Parasite Energy cast clip in AS12 bank slot 28.
///
/// Plays when the selected family-0 attachment's charge countdown reaches zero.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeCastFamily0Animation = {
    .records           = _gAs12PeCastFamily0AnimationRecords,
    .trackStartIndices = _gAs12PeCastFamily0AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeCastFamily0AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeCastFamily0AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeReleaseFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05180_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeReleaseFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05180_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeReleaseFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05180_records.inc"
};

/// Record-array starts of `_gAs12PeReleaseFamily1Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05180_indices.inc"
};

/// Family-1 Parasite Energy release clip in AS12 bank slot 29.
///
/// Plays before the charge phase when the selected attachment uses clip family 1.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeReleaseFamily1Animation = {
    .records           = _gAs12PeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gAs12PeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeReleaseFamily1AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeReleaseFamily1AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeChargeFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05380_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeChargeFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05380_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeChargeFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05380_records.inc"
};

/// Record-array starts of `_gAs12PeChargeFamily1Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05380_indices.inc"
};

/// Family-1 Parasite Energy charge clip in AS12 bank slot 30.
///
/// Loops while the selected attachment's countdown runs, before the cast clip.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeChargeFamily1Animation = {
    .records           = _gAs12PeChargeFamily1AnimationRecords,
    .trackStartIndices = _gAs12PeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeChargeFamily1AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeChargeFamily1AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeCastFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_056D4_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeCastFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_056D4_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeCastFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_056D4_records.inc"
};

/// Record-array starts of `_gAs12PeCastFamily1Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_056D4_indices.inc"
};

/// Family-1 Parasite Energy cast clip in AS12 bank slot 31.
///
/// Plays when the selected family-1 attachment's charge countdown reaches zero.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeCastFamily1Animation = {
    .records           = _gAs12PeCastFamily1AnimationRecords,
    .trackStartIndices = _gAs12PeCastFamily1AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeCastFamily1AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeCastFamily1AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12Part4HitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12Part4HitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05988_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12Part4HitAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12Part4HitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05988_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12Part4HitAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12Part4HitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05988_records.inc"
};

/// Record-array starts of `_gAs12Part4HitAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12Part4HitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05988_indices.inc"
};

/// Player hit reaction for the model-part-4 collision body, in AS12 bank slot 16.
///
/// Selected for recorded hit region 1, including attack and hazard contacts.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12Part4HitAnimation = {
    .records           = _gAs12Part4HitAnimationRecords,
    .trackStartIndices = _gAs12Part4HitAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12Part4HitAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12Part4HitAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12OtherBodyHitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12OtherBodyHitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05CA8_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12OtherBodyHitAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12OtherBodyHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05CA8_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12OtherBodyHitAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12OtherBodyHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05CA8_records.inc"
};

/// Record-array starts of `_gAs12OtherBodyHitAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12OtherBodyHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_05CA8_indices.inc"
};

/// Player hit reaction for other collision bodies, in AS12 bank slot 17.
///
/// Recorded hit region 2 covers root and model-part-1 body contacts.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12OtherBodyHitAnimation = {
    .records           = _gAs12OtherBodyHitAnimationRecords,
    .trackStartIndices = _gAs12OtherBodyHitAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12OtherBodyHitAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12OtherBodyHitAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12DeathAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12DeathAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0646C_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12DeathAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12DeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0646C_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12DeathAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12DeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0646C_records.inc"
};

/// Record-array starts of `_gAs12DeathAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12DeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0646C_indices.inc"
};

/// Player death clip in AS12 bank slot 18.
///
/// Stops at its final pose after HP reaches zero in normal or scripted control.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12DeathAnimation = {
    .records           = _gAs12DeathAnimationRecords,
    .trackStartIndices = _gAs12DeathAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12DeathAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12DeathAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12HealthyIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12HealthyIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_07704_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12HealthyIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12HealthyIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_07704_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12HealthyIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12HealthyIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_07704_records.inc"
};

/// Record-array starts of `_gAs12HealthyIdleAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12HealthyIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_07704_indices.inc"
};

/// Repeating player idle clip above half maximum HP, in AS12 bank slot 23.
///
/// Selected at the 300-tick direction-free idle trigger in normal control.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12HealthyIdleAnimation = {
    .records           = _gAs12HealthyIdleAnimationRecords,
    .trackStartIndices = _gAs12HealthyIdleAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12HealthyIdleAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12HealthyIdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12CriticalHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12CriticalHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0827C_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12CriticalHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12CriticalHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0827C_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12CriticalHealthIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12CriticalHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0827C_records.inc"
};

/// Record-array starts of `_gAs12CriticalHealthIdleAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12CriticalHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0827C_indices.inc"
};

/// Repeating critical-health idle and paralysis clip in AS12 bank slot 25.
///
/// Selected at the normal-control idle trigger with HP at most a quarter of
/// maximum; paralysis also reuses this clip regardless of HP.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12CriticalHealthIdleAnimation = {
    .records           = _gAs12CriticalHealthIdleAnimationRecords,
    .trackStartIndices = _gAs12CriticalHealthIdleAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12CriticalHealthIdleAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12CriticalHealthIdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12LowHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12LowHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_08998_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12LowHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12LowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08998_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12LowHealthIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12LowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08998_records.inc"
};

/// Record-array starts of `_gAs12LowHealthIdleAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12LowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08998_indices.inc"
};

/// Repeating player idle clip between quarter and half maximum HP, in AS12 bank slot 24.
///
/// Selected at the 300-tick direction-free idle trigger with HP above a quarter
/// and at most half maximum HP; integer thresholds round down.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12LowHealthIdleAnimation = {
    .records           = _gAs12LowHealthIdleAnimationRecords,
    .trackStartIndices = _gAs12LowHealthIdleAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12LowHealthIdleAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12LowHealthIdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12ScriptedPresentationEntryAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12ScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_08E18_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12ScriptedPresentationEntryAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12ScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08E18_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12ScriptedPresentationEntryAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12ScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08E18_records.inc"
};

/// Record-array starts of `_gAs12ScriptedPresentationEntryAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12ScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08E18_indices.inc"
};

/// Player entry presentation for scripted events, in AS12 bank slots 32 and 34.
///
/// A zero presentation variant selects slot 32 before the event command.
/// Slot 34 retains the same descriptor; playback stops at the final pose.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12ScriptedPresentationEntryAnimation = {
    .records           = _gAs12ScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gAs12ScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12ScriptedPresentationEntryAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12ScriptedPresentationEntryAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12ScriptedPresentationExitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12ScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_08FF0_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12ScriptedPresentationExitAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12ScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08FF0_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12ScriptedPresentationExitAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12ScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08FF0_records.inc"
};

/// Record-array starts of `_gAs12ScriptedPresentationExitAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12ScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_08FF0_indices.inc"
};

/// Player exit presentation for scripted events, in AS12 bank slots 33 and 35.
///
/// A nonzero presentation variant selects slot 33 after the event command.
/// Slot 35 retains the same descriptor; playback stops at the final pose.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12ScriptedPresentationExitAnimation = {
    .records           = _gAs12ScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gAs12ScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12ScriptedPresentationExitAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12ScriptedPresentationExitAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeReleaseFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_09554_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeReleaseFamily2Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09554_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeReleaseFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09554_records.inc"
};

/// Record-array starts of `_gAs12PeReleaseFamily2Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09554_indices.inc"
};

/// Family-2 Parasite Energy release clip in AS12 bank slot 42.
///
/// Plays before the charge phase when the selected attachment uses clip family 2.
/// Playback stops at the final pose.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeReleaseFamily2Animation = {
    .records           = _gAs12PeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gAs12PeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeReleaseFamily2AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeReleaseFamily2AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeChargeFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_09748_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeChargeFamily2Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09748_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeChargeFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09748_records.inc"
};

/// Record-array starts of `_gAs12PeChargeFamily2Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09748_indices.inc"
};

/// Family-2 Parasite Energy charge clip in AS12 bank slot 43.
///
/// Repeats while the selected attachment's countdown runs, before the cast clip.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeChargeFamily2Animation = {
    .records           = _gAs12PeChargeFamily2AnimationRecords,
    .trackStartIndices = _gAs12PeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeChargeFamily2AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeChargeFamily2AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12PeCastFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12PeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_09BC8_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12PeCastFamily2Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12PeCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09BC8_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12PeCastFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gAs12PeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09BC8_records.inc"
};

/// Record-array starts of `_gAs12PeCastFamily2Animation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12PeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09BC8_indices.inc"
};

/// Family-2 Parasite Energy cast clip in AS12 bank slot 44.
///
/// Plays when the selected family-2 attachment's charge countdown reaches zero.
/// Playback stops at the final pose.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12PeCastFamily2Animation = {
    .records           = _gAs12PeCastFamily2AnimationRecords,
    .trackStartIndices = _gAs12PeCastFamily2AnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12PeCastFamily2AnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12PeCastFamily2AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12AimHoldAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12AimHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_09DA0_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12AimHoldAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12AimHoldAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09DA0_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12AimHoldAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12AimHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09DA0_records.inc"
};

/// Record-array starts of `_gAs12AimHoldAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12AimHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_09DA0_indices.inc"
};

/// Repeating stationary aim pose and attack-ready clip in AS12 bank slot 9.
///
/// Selected while aiming without movement or turn input and before firing.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12AimHoldAnimation = {
    .records           = _gAs12AimHoldAnimationRecords,
    .trackStartIndices = _gAs12AimHoldAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12AimHoldAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12AimHoldAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12AimBackwardTurnAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12AimBackwardTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0A5E0_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12AimBackwardTurnAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12AimBackwardTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0A5E0_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12AimBackwardTurnAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12AimBackwardTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0A5E0_records.inc"
};

/// Record-array starts of `_gAs12AimBackwardTurnAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12AimBackwardTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0A5E0_indices.inc"
};

/// Repeating backward-movement and stationary-turn aim clip in AS12 bank slot 13.
///
/// Selected for backward movement while aiming, or turning without displacement.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12AimBackwardTurnAnimation = {
    .records           = _gAs12AimBackwardTurnAnimationRecords,
    .trackStartIndices = _gAs12AimBackwardTurnAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12AimBackwardTurnAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12AimBackwardTurnAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12AimForwardAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12AimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0B088_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12AimForwardAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12AimForwardAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0B088_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12AimForwardAnimation`'s nineteen part tracks.
static const AnimationRecord _gAs12AimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0B088_records.inc"
};

/// Record-array starts of `_gAs12AimForwardAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gAs12AimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0B088_indices.inc"
};

/// Repeating forward-movement aim clip in AS12 bank slot 12.
///
/// Selected for forward movement while aiming; its record cues mark footsteps.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12AimForwardAnimation = {
    .records           = _gAs12AimForwardAnimationRecords,
    .trackStartIndices = _gAs12AimForwardAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12AimForwardAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12AimForwardAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12ReloadAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12ReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0C468_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12ReloadAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12ReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0C468_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12ReloadAnimation`'s nineteen part tracks.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gAs12ReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0C468_records.inc"
};

/// Record-array starts of `_gAs12ReloadAnimation`'s nineteen model-part tracks.
///
/// Playback indexes tracks 0..18; the final serialized halfword is retained
/// before the word-aligned descriptor and is not a twentieth track.
static const u16 _gAs12ReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0C468_indices.inc"
};

/// Reload clip shared by AS12 bank slots 14, 15 and 20.
///
/// The three full-mask record cues time reload sounds and load completion.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12ReloadAnimation = {
    .records           = _gAs12ReloadAnimationRecords,
    .trackStartIndices = _gAs12ReloadAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12ReloadAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12ReloadAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12FireAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12FireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0CAF0_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12FireAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12FireAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0CAF0_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12FireAnimation`'s nineteen part tracks.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gAs12FireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0CAF0_records.inc"
};

/// Record-array starts of `_gAs12FireAnimation`'s nineteen model-part tracks.
///
/// Playback indexes tracks 0..18; the final serialized halfword is retained
/// before the word-aligned descriptor and is not a twentieth track.
static const u16 _gAs12FireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0CAF0_indices.inc"
};

/// Firing clip shared by AS12 bank slots 10 and 11.
///
/// Each automatic shot restarts this clip; it holds its final pose at the end.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gAs12FireAnimation = {
    .records           = _gAs12FireAnimationRecords,
    .trackStartIndices = _gAs12FireAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12FireAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12FireAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12AimEntryAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12AimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0D174_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12AimEntryAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12AimEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0D174_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12AimEntryAnimation`'s nineteen part tracks.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gAs12AimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0D174_records.inc"
};

/// Record-array starts of `_gAs12AimEntryAnimation`'s nineteen model-part tracks.
///
/// Playback indexes tracks 0..18; the final serialized halfword is retained
/// before the word-aligned descriptor and is not a twentieth track.
static const u16 _gAs12AimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0D174_indices.inc"
};

/// Aim-entry clip in AS12 bank slot 7, also borrowed for recovery from enemy grabs and hits.
///
/// Holds the final aim pose on completion. Borrowed tracks and pose data must
/// remain loaded while native or scripted playback refers to this clip.
static AnimationSet _gAs12AimEntryAnimation = {
    .records           = _gAs12AimEntryAnimationRecords,
    .trackStartIndices = _gAs12AimEntryAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12AimEntryAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12AimEntryAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12AimExitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12AimExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0D6EC_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12AimExitAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12AimExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0D6EC_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12AimExitAnimation`'s nineteen part tracks.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gAs12AimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0D6EC_records.inc"
};

/// Record-array starts of `_gAs12AimExitAnimation`'s nineteen model-part tracks.
///
/// Playback indexes tracks 0..18; the final serialized halfword is retained
/// before the word-aligned descriptor and is not a twentieth track.
static const u16 _gAs12AimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0D6EC_indices.inc"
};

/// Aim-exit clip in AS12 bank slot 8 for normal and scripted attack recovery.
///
/// Holds its final pose on completion. Borrowed tracks and pose data must
/// remain loaded throughout playback.
static AnimationSet _gAs12AimExitAnimation = {
    .records           = _gAs12AimExitAnimationRecords,
    .trackStartIndices = _gAs12AimExitAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12AimExitAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12AimExitAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12WalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12WalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0DFA4_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12WalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12WalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0DFA4_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12WalkAnimation`'s nineteen part tracks.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gAs12WalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0DFA4_records.inc"
};

/// Record-array starts of `_gAs12WalkAnimation`'s nineteen model-part tracks.
///
/// Playback indexes tracks 0..18; the final serialized halfword is retained
/// before the word-aligned descriptor and is not a twentieth track.
static const u16 _gAs12WalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0DFA4_indices.inc"
};

/// Repeating equipped forward-walk clip in AS12 bank slot 2.
///
/// Record cues mark footsteps. Borrowed tracks and pose data must remain
/// loaded throughout playback.
static AnimationSet _gAs12WalkAnimation = {
    .records           = _gAs12WalkAnimationRecords,
    .trackStartIndices = _gAs12WalkAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12WalkAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12WalkAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gAs12BackwardWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12BackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/as12_animation_0E744_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12BackwardWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12BackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0E744_bank4.inc"
};

/// Keyframe durations and control records for `_gAs12BackwardWalkAnimation`'s nineteen part tracks.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gAs12BackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0E744_records.inc"
};

/// Record-array starts of `_gAs12BackwardWalkAnimation`'s nineteen model-part tracks.
///
/// Playback indexes tracks 0..18; the final serialized halfword is retained
/// before the word-aligned descriptor and is not a twentieth track.
static const u16 _gAs12BackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0E744_indices.inc"
};

/// Repeating backward-walk clip in AS12 bank slot 3.
///
/// Record cues mark footsteps. Borrowed tracks and pose data must remain
/// loaded throughout playback.
static AnimationSet _gAs12BackwardWalkAnimation = {
    .records           = _gAs12BackwardWalkAnimationRecords,
    .trackStartIndices = _gAs12BackwardWalkAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12BackwardWalkAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12BackwardWalkAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gAs12RunAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gAs12RunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0F118_bank1.inc"
};

/// Rotation-only keyframes for `_gAs12RunAnimation`, in eight-angle-unit steps.
static const AnimationPackedRotation _gAs12RunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0F118_bank4.inc"
};

/// Keyframes, paired footstep cues and loop controls for `_gAs12RunAnimation`.
static const AnimationRecord _gAs12RunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0F118_records.inc"
};

/// Record starts for `_gAs12RunAnimation`'s tracks 0..18, followed by an unused serialized halfword.
static const u16 _gAs12RunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/as12_animation_0F118_indices.inc"
};

/// Player running clip in AS12 native-bank slot 4.
///
/// Normal locomotion and scripted run-to actions select this nineteen-track
/// clip. Moving tracks repeat a 20-frame loop after their initial keyframes;
/// track 1 carries paired footstep cues. Root displacement is applied by the
/// actor separately. Keep the borrowed clip data loaded throughout playback.
static AnimationSet _gAs12RunAnimation = {
    .records           = _gAs12RunAnimationRecords,
    .trackStartIndices = _gAs12RunAnimationIndices,
    .poseBanks         = {
        [AS12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gAs12RunAnimationBank1,
        [AS12_ANIMATION_PACKED_ROTATION_BANK]      = _gAs12RunAnimationBank4,
    },
};

AnimationBank gAs12PlayerAnimationBank = {
    .table = { .sets = {
                   [1] = &_gAs12IdleAnimation,
                   &_gAs12WalkAnimation,
                   &_gAs12BackwardWalkAnimation,
                   &_gAs12RunAnimation,
                   &_gAs12NegativeTurnAnimation,
                   &_gAs12PositiveTurnAnimation,
                   &_gAs12AimEntryAnimation,
                   &_gAs12AimExitAnimation,
                   &_gAs12AimHoldAnimation,
                   &_gAs12FireAnimation,
                   &_gAs12FireAnimation,
                   &_gAs12AimForwardAnimation,
                   &_gAs12AimBackwardTurnAnimation,
                   &_gAs12ReloadAnimation,
                   &_gAs12ReloadAnimation,
                   &_gAs12Part4HitAnimation,
                   &_gAs12OtherBodyHitAnimation,
                   &_gAs12DeathAnimation,
                   &_gAs12UnarmedWalkAnimation,
                   &_gAs12ReloadAnimation,
                   &_gAs12IdleAnimation,
                   &_gAs12IdleAnimation,
                   &_gAs12HealthyIdleAnimation,
                   &_gAs12LowHealthIdleAnimation,
                   &_gAs12CriticalHealthIdleAnimation,
                   &_gAs12PeReleaseFamily0Animation,
                   &_gAs12PeChargeFamily0Animation,
                   &_gAs12PeCastFamily0Animation,
                   &_gAs12PeReleaseFamily1Animation,
                   &_gAs12PeChargeFamily1Animation,
                   &_gAs12PeCastFamily1Animation,
                   &_gAs12ScriptedPresentationEntryAnimation,
                   &_gAs12ScriptedPresentationExitAnimation,
                   &_gAs12ScriptedPresentationEntryAnimation,
                   &_gAs12ScriptedPresentationExitAnimation,
                   &_gAs12StairAscentAnimation,
                   &_gAs12StairDescentAnimation,
                   &_gAs12StairAscentFinishEvenAnimation,
                   &_gAs12StairAscentFinishOddAnimation,
                   &_gAs12ItemUseAnimation,
                   &_gAs12IdleAnimation,
                   &_gAs12PeReleaseFamily2Animation,
                   &_gAs12PeChargeFamily2Animation,
                   &_gAs12PeCastFamily2Animation,
               } },
};
