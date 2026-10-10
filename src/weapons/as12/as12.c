#include "weapons/as12.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/animation.h"
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
            equipmentConsumeWeaponLoad(WEAPON_ITEM(AS12_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
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
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
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

static AnimationPackedPose _gAs12Animation01C60Bank1[19] = {
#include "assets/as12_animation_01C60_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation01C60Bank4[193] = {
#include "assets/as12_animation_01C60_bank4.inc"
};

static AnimationRecord _gAs12Animation01C60Records[296] = {
#include "assets/as12_animation_01C60_records.inc"
};

static u16 _gAs12Animation01C60Indices[20] = {
#include "assets/as12_animation_01C60_indices.inc"
};

static AnimationSet _gAs12Animation01C60 = {
    _gAs12Animation01C60Records,
    _gAs12Animation01C60Indices,
    { NULL, _gAs12Animation01C60Bank1, NULL, NULL, _gAs12Animation01C60Bank4, NULL, NULL, NULL },
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

static AnimationPackedPose _gAs12Animation04EDCBank1[6] = {
#include "assets/as12_animation_04EDC_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation04EDCBank4[57] = {
#include "assets/as12_animation_04EDC_bank4.inc"
};

static AnimationRecord _gAs12Animation04EDCRecords[87] = {
#include "assets/as12_animation_04EDC_records.inc"
};

static u16 _gAs12Animation04EDCIndices[20] = {
#include "assets/as12_animation_04EDC_indices.inc"
};

static AnimationSet _gAs12Animation04EDC = {
    _gAs12Animation04EDCRecords,
    _gAs12Animation04EDCIndices,
    { NULL, _gAs12Animation04EDCBank1, NULL, NULL, _gAs12Animation04EDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation05180Bank1[4] = {
#include "assets/as12_animation_05180_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation05180Bank4[55] = {
#include "assets/as12_animation_05180_bank4.inc"
};

static AnimationRecord _gAs12Animation05180Records[82] = {
#include "assets/as12_animation_05180_records.inc"
};

static u16 _gAs12Animation05180Indices[20] = {
#include "assets/as12_animation_05180_indices.inc"
};

static AnimationSet _gAs12Animation05180 = {
    _gAs12Animation05180Records,
    _gAs12Animation05180Indices,
    { NULL, _gAs12Animation05180Bank1, NULL, NULL, _gAs12Animation05180Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation05380Bank1[3] = {
#include "assets/as12_animation_05380_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation05380Bank4[23] = {
#include "assets/as12_animation_05380_bank4.inc"
};

static AnimationRecord _gAs12Animation05380Records[76] = {
#include "assets/as12_animation_05380_records.inc"
};

static u16 _gAs12Animation05380Indices[20] = {
#include "assets/as12_animation_05380_indices.inc"
};

static AnimationSet _gAs12Animation05380 = {
    _gAs12Animation05380Records,
    _gAs12Animation05380Indices,
    { NULL, _gAs12Animation05380Bank1, NULL, NULL, _gAs12Animation05380Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation056D4Bank1[8] = {
#include "assets/as12_animation_056D4_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation056D4Bank4[68] = {
#include "assets/as12_animation_056D4_bank4.inc"
};

static AnimationRecord _gAs12Animation056D4Records[101] = {
#include "assets/as12_animation_056D4_records.inc"
};

static u16 _gAs12Animation056D4Indices[20] = {
#include "assets/as12_animation_056D4_indices.inc"
};

static AnimationSet _gAs12Animation056D4 = {
    _gAs12Animation056D4Records,
    _gAs12Animation056D4Indices,
    { NULL, _gAs12Animation056D4Bank1, NULL, NULL, _gAs12Animation056D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation05988Bank1[5] = {
#include "assets/as12_animation_05988_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation05988Bank4[55] = {
#include "assets/as12_animation_05988_bank4.inc"
};

static AnimationRecord _gAs12Animation05988Records[83] = {
#include "assets/as12_animation_05988_records.inc"
};

static u16 _gAs12Animation05988Indices[20] = {
#include "assets/as12_animation_05988_indices.inc"
};

static AnimationSet _gAs12Animation05988 = {
    _gAs12Animation05988Records,
    _gAs12Animation05988Indices,
    { NULL, _gAs12Animation05988Bank1, NULL, NULL, _gAs12Animation05988Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation05CA8Bank1[6] = {
#include "assets/as12_animation_05CA8_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation05CA8Bank4[66] = {
#include "assets/as12_animation_05CA8_bank4.inc"
};

static AnimationRecord _gAs12Animation05CA8Records[96] = {
#include "assets/as12_animation_05CA8_records.inc"
};

static u16 _gAs12Animation05CA8Indices[20] = {
#include "assets/as12_animation_05CA8_indices.inc"
};

static AnimationSet _gAs12Animation05CA8 = {
    _gAs12Animation05CA8Records,
    _gAs12Animation05CA8Indices,
    { NULL, _gAs12Animation05CA8Bank1, NULL, NULL, _gAs12Animation05CA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0646CBank1[18] = {
#include "assets/as12_animation_0646C_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0646CBank4[184] = {
#include "assets/as12_animation_0646C_bank4.inc"
};

static AnimationRecord _gAs12Animation0646CRecords[239] = {
#include "assets/as12_animation_0646C_records.inc"
};

static u16 _gAs12Animation0646CIndices[20] = {
#include "assets/as12_animation_0646C_indices.inc"
};

static AnimationSet _gAs12Animation0646C = {
    _gAs12Animation0646CRecords,
    _gAs12Animation0646CIndices,
    { NULL, _gAs12Animation0646CBank1, NULL, NULL, _gAs12Animation0646CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation07704Bank1[29] = {
#include "assets/as12_animation_07704_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation07704Bank4[450] = {
#include "assets/as12_animation_07704_bank4.inc"
};

static AnimationRecord _gAs12Animation07704Records[633] = {
#include "assets/as12_animation_07704_records.inc"
};

static u16 _gAs12Animation07704Indices[20] = {
#include "assets/as12_animation_07704_indices.inc"
};

static AnimationSet _gAs12Animation07704 = {
    _gAs12Animation07704Records,
    _gAs12Animation07704Indices,
    { NULL, _gAs12Animation07704Bank1, NULL, NULL, _gAs12Animation07704Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0827CBank1[12] = {
#include "assets/as12_animation_0827C_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0827CBank4[266] = {
#include "assets/as12_animation_0827C_bank4.inc"
};

static AnimationRecord _gAs12Animation0827CRecords[412] = {
#include "assets/as12_animation_0827C_records.inc"
};

static u16 _gAs12Animation0827CIndices[20] = {
#include "assets/as12_animation_0827C_indices.inc"
};

static AnimationSet _gAs12Animation0827C = {
    _gAs12Animation0827CRecords,
    _gAs12Animation0827CIndices,
    { NULL, _gAs12Animation0827CBank1, NULL, NULL, _gAs12Animation0827CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation08998Bank1[9] = {
#include "assets/as12_animation_08998_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation08998Bank4[144] = {
#include "assets/as12_animation_08998_bank4.inc"
};

static AnimationRecord _gAs12Animation08998Records[264] = {
#include "assets/as12_animation_08998_records.inc"
};

static u16 _gAs12Animation08998Indices[20] = {
#include "assets/as12_animation_08998_indices.inc"
};

static AnimationSet _gAs12Animation08998 = {
    _gAs12Animation08998Records,
    _gAs12Animation08998Indices,
    { NULL, _gAs12Animation08998Bank1, NULL, NULL, _gAs12Animation08998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation08E18Bank1[6] = {
#include "assets/as12_animation_08E18_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation08E18Bank4[107] = {
#include "assets/as12_animation_08E18_bank4.inc"
};

static AnimationRecord _gAs12Animation08E18Records[143] = {
#include "assets/as12_animation_08E18_records.inc"
};

static u16 _gAs12Animation08E18Indices[20] = {
#include "assets/as12_animation_08E18_indices.inc"
};

static AnimationSet _gAs12Animation08E18 = {
    _gAs12Animation08E18Records,
    _gAs12Animation08E18Indices,
    { NULL, _gAs12Animation08E18Bank1, NULL, NULL, _gAs12Animation08E18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation08FF0Bank1[3] = {
#include "assets/as12_animation_08FF0_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation08FF0Bank4[32] = {
#include "assets/as12_animation_08FF0_bank4.inc"
};

static AnimationRecord _gAs12Animation08FF0Records[57] = {
#include "assets/as12_animation_08FF0_records.inc"
};

static u16 _gAs12Animation08FF0Indices[20] = {
#include "assets/as12_animation_08FF0_indices.inc"
};

static AnimationSet _gAs12Animation08FF0 = {
    _gAs12Animation08FF0Records,
    _gAs12Animation08FF0Indices,
    { NULL, _gAs12Animation08FF0Bank1, NULL, NULL, _gAs12Animation08FF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation09554Bank1[11] = {
#include "assets/as12_animation_09554_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation09554Bank4[125] = {
#include "assets/as12_animation_09554_bank4.inc"
};

static AnimationRecord _gAs12Animation09554Records[167] = {
#include "assets/as12_animation_09554_records.inc"
};

static u16 _gAs12Animation09554Indices[20] = {
#include "assets/as12_animation_09554_indices.inc"
};

static AnimationSet _gAs12Animation09554 = {
    _gAs12Animation09554Records,
    _gAs12Animation09554Indices,
    { NULL, _gAs12Animation09554Bank1, NULL, NULL, _gAs12Animation09554Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation09748Bank1[3] = {
#include "assets/as12_animation_09748_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation09748Bank4[20] = {
#include "assets/as12_animation_09748_bank4.inc"
};

static AnimationRecord _gAs12Animation09748Records[76] = {
#include "assets/as12_animation_09748_records.inc"
};

static u16 _gAs12Animation09748Indices[20] = {
#include "assets/as12_animation_09748_indices.inc"
};

static AnimationSet _gAs12Animation09748 = {
    _gAs12Animation09748Records,
    _gAs12Animation09748Indices,
    { NULL, _gAs12Animation09748Bank1, NULL, NULL, _gAs12Animation09748Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation09BC8Bank1[8] = {
#include "assets/as12_animation_09BC8_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation09BC8Bank4[105] = {
#include "assets/as12_animation_09BC8_bank4.inc"
};

static AnimationRecord _gAs12Animation09BC8Records[139] = {
#include "assets/as12_animation_09BC8_records.inc"
};

static u16 _gAs12Animation09BC8Indices[20] = {
#include "assets/as12_animation_09BC8_indices.inc"
};

static AnimationSet _gAs12Animation09BC8 = {
    _gAs12Animation09BC8Records,
    _gAs12Animation09BC8Indices,
    { NULL, _gAs12Animation09BC8Bank1, NULL, NULL, _gAs12Animation09BC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation09DA0Bank1[2] = {
#include "assets/as12_animation_09DA0_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation09DA0Bank4[16] = {
#include "assets/as12_animation_09DA0_bank4.inc"
};

static AnimationRecord _gAs12Animation09DA0Records[76] = {
#include "assets/as12_animation_09DA0_records.inc"
};

static u16 _gAs12Animation09DA0Indices[20] = {
#include "assets/as12_animation_09DA0_indices.inc"
};

static AnimationSet _gAs12Animation09DA0 = {
    _gAs12Animation09DA0Records,
    _gAs12Animation09DA0Indices,
    { NULL, _gAs12Animation09DA0Bank1, NULL, NULL, _gAs12Animation09DA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0A5E0Bank1[14] = {
#include "assets/as12_animation_0A5E0_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0A5E0Bank4[194] = {
#include "assets/as12_animation_0A5E0_bank4.inc"
};

static AnimationRecord _gAs12Animation0A5E0Records[272] = {
#include "assets/as12_animation_0A5E0_records.inc"
};

static u16 _gAs12Animation0A5E0Indices[20] = {
#include "assets/as12_animation_0A5E0_indices.inc"
};

static AnimationSet _gAs12Animation0A5E0 = {
    _gAs12Animation0A5E0Records,
    _gAs12Animation0A5E0Indices,
    { NULL, _gAs12Animation0A5E0Bank1, NULL, NULL, _gAs12Animation0A5E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0B088Bank1[19] = {
#include "assets/as12_animation_0B088_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0B088Bank4[269] = {
#include "assets/as12_animation_0B088_bank4.inc"
};

static AnimationRecord _gAs12Animation0B088Records[336] = {
#include "assets/as12_animation_0B088_records.inc"
};

static u16 _gAs12Animation0B088Indices[20] = {
#include "assets/as12_animation_0B088_indices.inc"
};

static AnimationSet _gAs12Animation0B088 = {
    _gAs12Animation0B088Records,
    _gAs12Animation0B088Indices,
    { NULL, _gAs12Animation0B088Bank1, NULL, NULL, _gAs12Animation0B088Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0C468Bank1[33] = {
#include "assets/as12_animation_0C468_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0C468Bank4[538] = {
#include "assets/as12_animation_0C468_bank4.inc"
};

static AnimationRecord _gAs12Animation0C468Records[615] = {
#include "assets/as12_animation_0C468_records.inc"
};

static u16 _gAs12Animation0C468Indices[20] = {
#include "assets/as12_animation_0C468_indices.inc"
};

static AnimationSet _gAs12Animation0C468 = {
    _gAs12Animation0C468Records,
    _gAs12Animation0C468Indices,
    { NULL, _gAs12Animation0C468Bank1, NULL, NULL, _gAs12Animation0C468Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0CAF0Bank1[11] = {
#include "assets/as12_animation_0CAF0_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0CAF0Bank4[157] = {
#include "assets/as12_animation_0CAF0_bank4.inc"
};

static AnimationRecord _gAs12Animation0CAF0Records[208] = {
#include "assets/as12_animation_0CAF0_records.inc"
};

static u16 _gAs12Animation0CAF0Indices[20] = {
#include "assets/as12_animation_0CAF0_indices.inc"
};

static AnimationSet _gAs12Animation0CAF0 = {
    _gAs12Animation0CAF0Records,
    _gAs12Animation0CAF0Indices,
    { NULL, _gAs12Animation0CAF0Bank1, NULL, NULL, _gAs12Animation0CAF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0D174Bank1[12] = {
#include "assets/as12_animation_0D174_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0D174Bank4[160] = {
#include "assets/as12_animation_0D174_bank4.inc"
};

static AnimationRecord _gAs12Animation0D174Records[201] = {
#include "assets/as12_animation_0D174_records.inc"
};

static u16 _gAs12Animation0D174Indices[20] = {
#include "assets/as12_animation_0D174_indices.inc"
};

static AnimationSet _gAs12Animation0D174 = {
    _gAs12Animation0D174Records,
    _gAs12Animation0D174Indices,
    { NULL, _gAs12Animation0D174Bank1, NULL, NULL, _gAs12Animation0D174Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0D6ECBank1[10] = {
#include "assets/as12_animation_0D6EC_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0D6ECBank4[131] = {
#include "assets/as12_animation_0D6EC_bank4.inc"
};

static AnimationRecord _gAs12Animation0D6ECRecords[169] = {
#include "assets/as12_animation_0D6EC_records.inc"
};

static u16 _gAs12Animation0D6ECIndices[20] = {
#include "assets/as12_animation_0D6EC_indices.inc"
};

static AnimationSet _gAs12Animation0D6EC = {
    _gAs12Animation0D6ECRecords,
    _gAs12Animation0D6ECIndices,
    { NULL, _gAs12Animation0D6ECBank1, NULL, NULL, _gAs12Animation0D6ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0DFA4Bank1[18] = {
#include "assets/as12_animation_0DFA4_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0DFA4Bank4[201] = {
#include "assets/as12_animation_0DFA4_bank4.inc"
};

static AnimationRecord _gAs12Animation0DFA4Records[283] = {
#include "assets/as12_animation_0DFA4_records.inc"
};

static u16 _gAs12Animation0DFA4Indices[20] = {
#include "assets/as12_animation_0DFA4_indices.inc"
};

static AnimationSet _gAs12Animation0DFA4 = {
    _gAs12Animation0DFA4Records,
    _gAs12Animation0DFA4Indices,
    { NULL, _gAs12Animation0DFA4Bank1, NULL, NULL, _gAs12Animation0DFA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0E744Bank1[16] = {
#include "assets/as12_animation_0E744_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0E744Bank4[157] = {
#include "assets/as12_animation_0E744_bank4.inc"
};

static AnimationRecord _gAs12Animation0E744Records[263] = {
#include "assets/as12_animation_0E744_records.inc"
};

static u16 _gAs12Animation0E744Indices[20] = {
#include "assets/as12_animation_0E744_indices.inc"
};

static AnimationSet _gAs12Animation0E744 = {
    _gAs12Animation0E744Records,
    _gAs12Animation0E744Indices,
    { NULL, _gAs12Animation0E744Bank1, NULL, NULL, _gAs12Animation0E744Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation0F118Bank1[25] = {
#include "assets/as12_animation_0F118_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation0F118Bank4[223] = {
#include "assets/as12_animation_0F118_bank4.inc"
};

static AnimationRecord _gAs12Animation0F118Records[311] = {
#include "assets/as12_animation_0F118_records.inc"
};

static u16 _gAs12Animation0F118Indices[20] = {
#include "assets/as12_animation_0F118_indices.inc"
};

static AnimationSet _gAs12Animation0F118 = {
    _gAs12Animation0F118Records,
    _gAs12Animation0F118Indices,
    { NULL, _gAs12Animation0F118Bank1, NULL, NULL, _gAs12Animation0F118Bank4, NULL, NULL, NULL },
};

AnimationBank D_as12_8012C300 = { { {
    NULL,
    &_gAs12IdleAnimation,
    &_gAs12Animation0DFA4,
    &_gAs12Animation0E744,
    &_gAs12Animation0F118,
    &_gAs12NegativeTurnAnimation,
    &_gAs12PositiveTurnAnimation,
    &_gAs12Animation0D174,
    &_gAs12Animation0D6EC,
    &_gAs12Animation09DA0,
    &_gAs12Animation0CAF0,
    &_gAs12Animation0CAF0,
    &_gAs12Animation0B088,
    &_gAs12Animation0A5E0,
    &_gAs12Animation0C468,
    &_gAs12Animation0C468,
    &_gAs12Animation05988,
    &_gAs12Animation05CA8,
    &_gAs12Animation0646C,
    &_gAs12UnarmedWalkAnimation,
    &_gAs12Animation0C468,
    &_gAs12IdleAnimation,
    &_gAs12IdleAnimation,
    &_gAs12Animation07704,
    &_gAs12Animation08998,
    &_gAs12Animation0827C,
    &_gAs12PeReleaseFamily0Animation,
    &_gAs12PeChargeFamily0Animation,
    &_gAs12Animation04EDC,
    &_gAs12Animation05180,
    &_gAs12Animation05380,
    &_gAs12Animation056D4,
    &_gAs12Animation08E18,
    &_gAs12Animation08FF0,
    &_gAs12Animation08E18,
    &_gAs12Animation08FF0,
    &_gAs12StairAscentAnimation,
    &_gAs12StairDescentAnimation,
    &_gAs12StairAscentFinishEvenAnimation,
    &_gAs12StairAscentFinishOddAnimation,
    &_gAs12ItemUseAnimation,
    &_gAs12IdleAnimation,
    &_gAs12Animation09554,
    &_gAs12Animation09748,
    &_gAs12Animation09BC8,
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
