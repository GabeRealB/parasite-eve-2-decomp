#include "weapons/pa3.h"
#include "weapons/sp12.h"

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
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

/// One-based weapon index 13 (item 0x8C), selecting the PA3 build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Selects the held
/// model, player animation bank, sound bank and shotgun muzzle-flash profile.
#define PA3_WEAPON_INDEX_STANDARD 0xD

/// One-based weapon index 14 (item 0x8D), selecting the SP12 build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Selects the SP12
/// model, player animation bank, sound bank and shotgun muzzle-flash profile.
#define PA3_WEAPON_INDEX_SP12 0xE

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 13 PA3 or 14 SP12. It selects
/// assets, the firing/pump sound bank and the shotgun flash profile;
/// `WEAPON_ITEM_ID` converts it to the weapon item whose load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#elif WEAPON_INDEX != PA3_WEAPON_INDEX_STANDARD && WEAPON_INDEX != PA3_WEAPON_INDEX_SP12
#error "WEAPON_INDEX must select a PA3 or SP12 build"
#endif

/// Recovery dispatch ticks before held action buttons may finish a shot early.
///
/// Required nonnegative integer preprocessor expression fitting s8: 32 PA3,
/// 36 SP12. Seeds `attackCancelTicks` during preparation, but counts down only
/// from impact through pump delay and recovery. Animation completion can end
/// the attack before this countdown expires.
#ifndef PA3_ATTACK_CANCEL_TICKS
#error "PA3_ATTACK_CANCEL_TICKS is a per-package build parameter"
#elif PA3_ATTACK_CANCEL_TICKS < 0 || PA3_ATTACK_CANCEL_TICKS > 127
#error "PA3_ATTACK_CANCEL_TICKS must fit a nonnegative s8"
#endif

void pa3AttackState(Task* playerTask)
{
    enum {
        PA3_PHASE_PREPARE              = 0,
        PA3_PHASE_WAIT_READY           = 1,
        PA3_PHASE_FIRE                 = 2,
        PA3_PHASE_IMPACT               = 3,
        PA3_PHASE_PUMP_DELAY           = 4,
        PA3_PHASE_RECOVER              = 5,
        PA3_PLAYER_ATTACK_STATE        = 4,
        PA3_ANIMATION_READY            = 9,
        PA3_ANIMATION_PRIMARY          = 0xA,
        PA3_READY_BLEND_FRAMES         = 1,
        PA3_MOVING_READY_BLEND_FRAMES  = 8,
        PA3_IMPACT_SOUND               = SOUND_COMMON(0x17),
        PA3_PUMP_DELAY_FRAMES          = 0x1F,
        PA3_AMMUNITION_BUCKSHOT        = WEAPON_AMMUNITION_INDEX(0xAC),
        PA3_AMMUNITION_FIREFLY         = WEAPON_AMMUNITION_INDEX(0xAD),
        PA3_FIREFLY_CONTACT_SOUND_BASE = SOUND_WEAPON(0, 4),
        PA3_FIRE_SOUND_BASE            = SOUND_WEAPON(0, 5),
        PA3_PUMP_SOUND_BASE            = SOUND_WEAPON(0, 2),
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        gridImpact;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case PA3_PHASE_PREPARE:
            actor->state             = PA3_PLAYER_ATTACK_STATE;
            actor->statePhase        = PA3_PHASE_WAIT_READY;
            actor->attackCancelTicks = PA3_ATTACK_CANCEL_TICKS;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->stateTimer        = PA3_PUMP_DELAY_FRAMES;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (gPlayerStatus.weaponSlotItem == PA3_AMMUNITION_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~WORLD_COLLISION_BODY_SINGLE_CONTACT;
            }
            readyBlendFrames = PA3_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PA3_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PA3_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            /* fallthrough */
        case PA3_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PA3_PHASE_FIRE:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                ((gPlayerStatus.weaponSlotItem - PA3_AMMUNITION_BUCKSHOT) << 0x18) | PA3_FIRE_SOUND_BASE | (WEAPON_INDEX << 16), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | WEAPON_INDEX, NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, PA3_ANIMATION_PRIMARY, 1, 3);
            break;
        case PA3_PHASE_IMPACT:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != PA3_AMMUNITION_BUCKSHOT) {
                gridImpact = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord);
                if (gPlayerStatus.weaponSlotItem == PA3_AMMUNITION_FIREFLY) {
                    if (gridImpact != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        impactCoord->workm.t[0] = actor->weaponContacts[0].point.vx;
                        impactCoord->workm.t[1] = actor->weaponContacts[0].point.vy;
                        impactCoord->workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(impactCoord,
                                            ((gPlayerStatus.weaponSlotItem - PA3_AMMUNITION_BUCKSHOT) << 0x18) | PA3_FIREFLY_CONTACT_SOUND_BASE | (WEAPON_INDEX << 16), 1);
                    }
                } else if (gridImpact != 0) {
                    worldCoordPlaySound(impactCoord, PA3_IMPACT_SOUND, 1);
                }
            }
            /* fallthrough */
        case PA3_PHASE_PUMP_DELAY:
            // Pump timing and recovery checks run together through the fallthrough.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                worldCoordPlaySound(playerTask->extra.tmd->coords,
                                    ((gPlayerStatus.weaponSlotItem - PA3_AMMUNITION_BUCKSHOT) << 0x18) | PA3_PUMP_SOUND_BASE | (WEAPON_INDEX << 16), 0);
            }
            /* fallthrough */
        case PA3_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 1;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/* Each package carries its own model. */
#if WEAPON_INDEX == PA3_WEAPON_INDEX_STANDARD
/// Identity local transform and self-parented root of the rigid held shotgun.
///
/// Read-only asset arrays retain their serialized positions in .data.
static const TmdBone _gPa3WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/pa3_model_0056C_skeleton.inc"
};

/// Vertex count of the sole part, covering all 26 local vertices.
static const u32 _gPa3WeaponModelPartVerts[ARRAY_SIZE(_gPa3WeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/pa3_model_0056C_partVerts.inc"
};

/// Shotgun vertices in integer coordinates local to the root part.
static SVECTOR _gPa3WeaponModelVerts[] = {
#include "assets/pa3_model_0056C_verts.inc"
};

/// Part-local lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gPa3WeaponModelNormals[] = {
#include "assets/pa3_model_0056C_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and 23 quads.
///
/// Creation resolves handler slots in place. The part group and an empty final
/// pre-transformed group each end with `TMD_STREAM_GROUP_END`, followed by
/// `TMD_STREAM_END`; all primitives use the directly transformed buffer region.
static u32 _gPa3WeaponModelStream[] = {
#include "assets/pa3_model_0056C_stream.inc"
};

TmdSource gPa3WeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (23 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gPa3WeaponModelSkeleton),
    .partVertexCounts    = _gPa3WeaponModelPartVerts,
    .verts               = _gPa3WeaponModelVerts,
    .normals             = _gPa3WeaponModelNormals,
    .skeleton            = _gPa3WeaponModelSkeleton,
    .stream              = _gPa3WeaponModelStream,
};
#elif WEAPON_INDEX == PA3_WEAPON_INDEX_SP12
/// Initial identity transform and self-parented root of the rigid shotgun model.
///
/// Read-only asset arrays retain their serialized positions in .data.
static const TmdBone _gSp12ModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/sp12_model_005CC_skeleton.inc"
};

/// Vertex count of the model's sole part, covering all 34 local vertices.
static const u32 _gSp12ModelPartVerts[ARRAY_SIZE(_gSp12ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/sp12_model_005CC_partVerts.inc"
};

/// Shotgun vertices in integer coordinates local to its single root part.
static SVECTOR _gSp12ModelVerts[] = {
#include "assets/sp12_model_005CC_verts.inc"
};

/// Part-local lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gSp12ModelNormals[] = {
#include "assets/sp12_model_005CC_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and 31 quads.
///
/// First creation resolves handler slots in place. The single part group ends
/// with `TMD_STREAM_GROUP_END`, followed by `TMD_STREAM_END`; no pre-transformed
/// region is used.
static u32 _gSp12ModelStream[] = {
#include "assets/sp12_model_005CC_stream.inc"
};

TmdSource gSp12Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (31 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gSp12ModelSkeleton),
    .partVertexCounts    = _gSp12ModelPartVerts,
    .verts               = _gSp12ModelVerts,
    .normals             = _gSp12ModelNormals,
    .skeleton            = _gSp12ModelSkeleton,
    .stream              = _gSp12ModelStream,
};
#endif

/* Each package carries its own animation bank. */
#if WEAPON_INDEX == PA3_WEAPON_INDEX_STANDARD
static AnimationPackedPose _gPa3Animation009F8Bank1[2] = {
#include "assets/pa3_animation_009F8_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation009F8Bank4[8] = {
#include "assets/pa3_animation_009F8_bank4.inc"
};

static AnimationRecord _gPa3Animation009F8Records[76] = {
#include "assets/pa3_animation_009F8_records.inc"
};

static u16 _gPa3Animation009F8Indices[20] = {
#include "assets/pa3_animation_009F8_indices.inc"
};

static AnimationSet _gPa3Animation009F8 = {
    _gPa3Animation009F8Records,
    _gPa3Animation009F8Indices,
    { NULL, _gPa3Animation009F8Bank1, NULL, NULL, _gPa3Animation009F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0109CBank1[12] = {
#include "assets/pa3_animation_0109C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0109CBank4[151] = {
#include "assets/pa3_animation_0109C_bank4.inc"
};

static AnimationRecord _gPa3Animation0109CRecords[218] = {
#include "assets/pa3_animation_0109C_records.inc"
};

static u16 _gPa3Animation0109CIndices[20] = {
#include "assets/pa3_animation_0109C_indices.inc"
};

static AnimationSet _gPa3Animation0109C = {
    _gPa3Animation0109CRecords,
    _gPa3Animation0109CIndices,
    { NULL, _gPa3Animation0109CBank1, NULL, NULL, _gPa3Animation0109CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation018FCBank1[19] = {
#include "assets/pa3_animation_018FC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation018FCBank4[169] = {
#include "assets/pa3_animation_018FC_bank4.inc"
};

static AnimationRecord _gPa3Animation018FCRecords[290] = {
#include "assets/pa3_animation_018FC_records.inc"
};

static u16 _gPa3Animation018FCIndices[20] = {
#include "assets/pa3_animation_018FC_indices.inc"
};

static AnimationSet _gPa3Animation018FC = {
    _gPa3Animation018FCRecords,
    _gPa3Animation018FCIndices,
    { NULL, _gPa3Animation018FCBank1, NULL, NULL, _gPa3Animation018FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation02160Bank1[19] = {
#include "assets/pa3_animation_02160_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation02160Bank4[170] = {
#include "assets/pa3_animation_02160_bank4.inc"
};

static AnimationRecord _gPa3Animation02160Records[290] = {
#include "assets/pa3_animation_02160_records.inc"
};

static u16 _gPa3Animation02160Indices[20] = {
#include "assets/pa3_animation_02160_indices.inc"
};

static AnimationSet _gPa3Animation02160 = {
    _gPa3Animation02160Records,
    _gPa3Animation02160Indices,
    { NULL, _gPa3Animation02160Bank1, NULL, NULL, _gPa3Animation02160Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation02474Bank1[3] = {
#include "assets/pa3_animation_02474_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation02474Bank4[69] = {
#include "assets/pa3_animation_02474_bank4.inc"
};

static AnimationRecord _gPa3Animation02474Records[99] = {
#include "assets/pa3_animation_02474_records.inc"
};

static u16 _gPa3Animation02474Indices[20] = {
#include "assets/pa3_animation_02474_indices.inc"
};

static AnimationSet _gPa3Animation02474 = {
    _gPa3Animation02474Records,
    _gPa3Animation02474Indices,
    { NULL, _gPa3Animation02474Bank1, NULL, NULL, _gPa3Animation02474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation02BD0Bank1[14] = {
#include "assets/pa3_animation_02BD0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation02BD0Bank4[156] = {
#include "assets/pa3_animation_02BD0_bank4.inc"
};

static AnimationRecord _gPa3Animation02BD0Records[253] = {
#include "assets/pa3_animation_02BD0_records.inc"
};

static u16 _gPa3Animation02BD0Indices[20] = {
#include "assets/pa3_animation_02BD0_indices.inc"
};

static AnimationSet _gPa3Animation02BD0 = {
    _gPa3Animation02BD0Records,
    _gPa3Animation02BD0Indices,
    { NULL, _gPa3Animation02BD0Bank1, NULL, NULL, _gPa3Animation02BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation03358Bank1[16] = {
#include "assets/pa3_animation_03358_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation03358Bank4[167] = {
#include "assets/pa3_animation_03358_bank4.inc"
};

static AnimationRecord _gPa3Animation03358Records[247] = {
#include "assets/pa3_animation_03358_records.inc"
};

static u16 _gPa3Animation03358Indices[20] = {
#include "assets/pa3_animation_03358_indices.inc"
};

static AnimationSet _gPa3Animation03358 = {
    _gPa3Animation03358Records,
    _gPa3Animation03358Indices,
    { NULL, _gPa3Animation03358Bank1, NULL, NULL, _gPa3Animation03358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0362CBank1[6] = {
#include "assets/pa3_animation_0362C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0362CBank4[52] = {
#include "assets/pa3_animation_0362C_bank4.inc"
};

static AnimationRecord _gPa3Animation0362CRecords[91] = {
#include "assets/pa3_animation_0362C_records.inc"
};

static u16 _gPa3Animation0362CIndices[20] = {
#include "assets/pa3_animation_0362C_indices.inc"
};

static AnimationSet _gPa3Animation0362C = {
    _gPa3Animation0362CRecords,
    _gPa3Animation0362CIndices,
    { NULL, _gPa3Animation0362CBank1, NULL, NULL, _gPa3Animation0362CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation039BCBank1[7] = {
#include "assets/pa3_animation_039BC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation039BCBank4[73] = {
#include "assets/pa3_animation_039BC_bank4.inc"
};

static AnimationRecord _gPa3Animation039BCRecords[114] = {
#include "assets/pa3_animation_039BC_records.inc"
};

static u16 _gPa3Animation039BCIndices[20] = {
#include "assets/pa3_animation_039BC_indices.inc"
};

static AnimationSet _gPa3Animation039BC = {
    _gPa3Animation039BCRecords,
    _gPa3Animation039BCIndices,
    { NULL, _gPa3Animation039BCBank1, NULL, NULL, _gPa3Animation039BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation03E44Bank1[9] = {
#include "assets/pa3_animation_03E44_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation03E44Bank4[104] = {
#include "assets/pa3_animation_03E44_bank4.inc"
};

static AnimationRecord _gPa3Animation03E44Records[139] = {
#include "assets/pa3_animation_03E44_records.inc"
};

static u16 _gPa3Animation03E44Indices[20] = {
#include "assets/pa3_animation_03E44_indices.inc"
};

static AnimationSet _gPa3Animation03E44 = {
    _gPa3Animation03E44Records,
    _gPa3Animation03E44Indices,
    { NULL, _gPa3Animation03E44Bank1, NULL, NULL, _gPa3Animation03E44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04040Bank1[3] = {
#include "assets/pa3_animation_04040_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04040Bank4[22] = {
#include "assets/pa3_animation_04040_bank4.inc"
};

static AnimationRecord _gPa3Animation04040Records[76] = {
#include "assets/pa3_animation_04040_records.inc"
};

static u16 _gPa3Animation04040Indices[20] = {
#include "assets/pa3_animation_04040_indices.inc"
};

static AnimationSet _gPa3Animation04040 = {
    _gPa3Animation04040Records,
    _gPa3Animation04040Indices,
    { NULL, _gPa3Animation04040Bank1, NULL, NULL, _gPa3Animation04040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04318Bank1[6] = {
#include "assets/pa3_animation_04318_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04318Bank4[57] = {
#include "assets/pa3_animation_04318_bank4.inc"
};

static AnimationRecord _gPa3Animation04318Records[87] = {
#include "assets/pa3_animation_04318_records.inc"
};

static u16 _gPa3Animation04318Indices[20] = {
#include "assets/pa3_animation_04318_indices.inc"
};

static AnimationSet _gPa3Animation04318 = {
    _gPa3Animation04318Records,
    _gPa3Animation04318Indices,
    { NULL, _gPa3Animation04318Bank1, NULL, NULL, _gPa3Animation04318Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation045BCBank1[4] = {
#include "assets/pa3_animation_045BC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation045BCBank4[55] = {
#include "assets/pa3_animation_045BC_bank4.inc"
};

static AnimationRecord _gPa3Animation045BCRecords[82] = {
#include "assets/pa3_animation_045BC_records.inc"
};

static u16 _gPa3Animation045BCIndices[20] = {
#include "assets/pa3_animation_045BC_indices.inc"
};

static AnimationSet _gPa3Animation045BC = {
    _gPa3Animation045BCRecords,
    _gPa3Animation045BCIndices,
    { NULL, _gPa3Animation045BCBank1, NULL, NULL, _gPa3Animation045BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation047BCBank1[3] = {
#include "assets/pa3_animation_047BC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation047BCBank4[23] = {
#include "assets/pa3_animation_047BC_bank4.inc"
};

static AnimationRecord _gPa3Animation047BCRecords[76] = {
#include "assets/pa3_animation_047BC_records.inc"
};

static u16 _gPa3Animation047BCIndices[20] = {
#include "assets/pa3_animation_047BC_indices.inc"
};

static AnimationSet _gPa3Animation047BC = {
    _gPa3Animation047BCRecords,
    _gPa3Animation047BCIndices,
    { NULL, _gPa3Animation047BCBank1, NULL, NULL, _gPa3Animation047BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04B10Bank1[8] = {
#include "assets/pa3_animation_04B10_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04B10Bank4[68] = {
#include "assets/pa3_animation_04B10_bank4.inc"
};

static AnimationRecord _gPa3Animation04B10Records[101] = {
#include "assets/pa3_animation_04B10_records.inc"
};

static u16 _gPa3Animation04B10Indices[20] = {
#include "assets/pa3_animation_04B10_indices.inc"
};

static AnimationSet _gPa3Animation04B10 = {
    _gPa3Animation04B10Records,
    _gPa3Animation04B10Indices,
    { NULL, _gPa3Animation04B10Bank1, NULL, NULL, _gPa3Animation04B10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04DC4Bank1[5] = {
#include "assets/pa3_animation_04DC4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04DC4Bank4[55] = {
#include "assets/pa3_animation_04DC4_bank4.inc"
};

static AnimationRecord _gPa3Animation04DC4Records[83] = {
#include "assets/pa3_animation_04DC4_records.inc"
};

static u16 _gPa3Animation04DC4Indices[20] = {
#include "assets/pa3_animation_04DC4_indices.inc"
};

static AnimationSet _gPa3Animation04DC4 = {
    _gPa3Animation04DC4Records,
    _gPa3Animation04DC4Indices,
    { NULL, _gPa3Animation04DC4Bank1, NULL, NULL, _gPa3Animation04DC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation050E4Bank1[6] = {
#include "assets/pa3_animation_050E4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation050E4Bank4[66] = {
#include "assets/pa3_animation_050E4_bank4.inc"
};

static AnimationRecord _gPa3Animation050E4Records[96] = {
#include "assets/pa3_animation_050E4_records.inc"
};

static u16 _gPa3Animation050E4Indices[20] = {
#include "assets/pa3_animation_050E4_indices.inc"
};

static AnimationSet _gPa3Animation050E4 = {
    _gPa3Animation050E4Records,
    _gPa3Animation050E4Indices,
    { NULL, _gPa3Animation050E4Bank1, NULL, NULL, _gPa3Animation050E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation058A8Bank1[18] = {
#include "assets/pa3_animation_058A8_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation058A8Bank4[184] = {
#include "assets/pa3_animation_058A8_bank4.inc"
};

static AnimationRecord _gPa3Animation058A8Records[239] = {
#include "assets/pa3_animation_058A8_records.inc"
};

static u16 _gPa3Animation058A8Indices[20] = {
#include "assets/pa3_animation_058A8_indices.inc"
};

static AnimationSet _gPa3Animation058A8 = {
    _gPa3Animation058A8Records,
    _gPa3Animation058A8Indices,
    { NULL, _gPa3Animation058A8Bank1, NULL, NULL, _gPa3Animation058A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation06B40Bank1[29] = {
#include "assets/pa3_animation_06B40_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation06B40Bank4[450] = {
#include "assets/pa3_animation_06B40_bank4.inc"
};

static AnimationRecord _gPa3Animation06B40Records[633] = {
#include "assets/pa3_animation_06B40_records.inc"
};

static u16 _gPa3Animation06B40Indices[20] = {
#include "assets/pa3_animation_06B40_indices.inc"
};

static AnimationSet _gPa3Animation06B40 = {
    _gPa3Animation06B40Records,
    _gPa3Animation06B40Indices,
    { NULL, _gPa3Animation06B40Bank1, NULL, NULL, _gPa3Animation06B40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation076B8Bank1[12] = {
#include "assets/pa3_animation_076B8_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation076B8Bank4[266] = {
#include "assets/pa3_animation_076B8_bank4.inc"
};

static AnimationRecord _gPa3Animation076B8Records[412] = {
#include "assets/pa3_animation_076B8_records.inc"
};

static u16 _gPa3Animation076B8Indices[20] = {
#include "assets/pa3_animation_076B8_indices.inc"
};

static AnimationSet _gPa3Animation076B8 = {
    _gPa3Animation076B8Records,
    _gPa3Animation076B8Indices,
    { NULL, _gPa3Animation076B8Bank1, NULL, NULL, _gPa3Animation076B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation07DD4Bank1[9] = {
#include "assets/pa3_animation_07DD4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation07DD4Bank4[144] = {
#include "assets/pa3_animation_07DD4_bank4.inc"
};

static AnimationRecord _gPa3Animation07DD4Records[264] = {
#include "assets/pa3_animation_07DD4_records.inc"
};

static u16 _gPa3Animation07DD4Indices[20] = {
#include "assets/pa3_animation_07DD4_indices.inc"
};

static AnimationSet _gPa3Animation07DD4 = {
    _gPa3Animation07DD4Records,
    _gPa3Animation07DD4Indices,
    { NULL, _gPa3Animation07DD4Bank1, NULL, NULL, _gPa3Animation07DD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation08254Bank1[6] = {
#include "assets/pa3_animation_08254_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation08254Bank4[107] = {
#include "assets/pa3_animation_08254_bank4.inc"
};

static AnimationRecord _gPa3Animation08254Records[143] = {
#include "assets/pa3_animation_08254_records.inc"
};

static u16 _gPa3Animation08254Indices[20] = {
#include "assets/pa3_animation_08254_indices.inc"
};

static AnimationSet _gPa3Animation08254 = {
    _gPa3Animation08254Records,
    _gPa3Animation08254Indices,
    { NULL, _gPa3Animation08254Bank1, NULL, NULL, _gPa3Animation08254Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0842CBank1[3] = {
#include "assets/pa3_animation_0842C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0842CBank4[32] = {
#include "assets/pa3_animation_0842C_bank4.inc"
};

static AnimationRecord _gPa3Animation0842CRecords[57] = {
#include "assets/pa3_animation_0842C_records.inc"
};

static u16 _gPa3Animation0842CIndices[20] = {
#include "assets/pa3_animation_0842C_indices.inc"
};

static AnimationSet _gPa3Animation0842C = {
    _gPa3Animation0842CRecords,
    _gPa3Animation0842CIndices,
    { NULL, _gPa3Animation0842CBank1, NULL, NULL, _gPa3Animation0842CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation08990Bank1[11] = {
#include "assets/pa3_animation_08990_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation08990Bank4[125] = {
#include "assets/pa3_animation_08990_bank4.inc"
};

static AnimationRecord _gPa3Animation08990Records[167] = {
#include "assets/pa3_animation_08990_records.inc"
};

static u16 _gPa3Animation08990Indices[20] = {
#include "assets/pa3_animation_08990_indices.inc"
};

static AnimationSet _gPa3Animation08990 = {
    _gPa3Animation08990Records,
    _gPa3Animation08990Indices,
    { NULL, _gPa3Animation08990Bank1, NULL, NULL, _gPa3Animation08990Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation08B84Bank1[3] = {
#include "assets/pa3_animation_08B84_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation08B84Bank4[20] = {
#include "assets/pa3_animation_08B84_bank4.inc"
};

static AnimationRecord _gPa3Animation08B84Records[76] = {
#include "assets/pa3_animation_08B84_records.inc"
};

static u16 _gPa3Animation08B84Indices[20] = {
#include "assets/pa3_animation_08B84_indices.inc"
};

static AnimationSet _gPa3Animation08B84 = {
    _gPa3Animation08B84Records,
    _gPa3Animation08B84Indices,
    { NULL, _gPa3Animation08B84Bank1, NULL, NULL, _gPa3Animation08B84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation09004Bank1[8] = {
#include "assets/pa3_animation_09004_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation09004Bank4[105] = {
#include "assets/pa3_animation_09004_bank4.inc"
};

static AnimationRecord _gPa3Animation09004Records[139] = {
#include "assets/pa3_animation_09004_records.inc"
};

static u16 _gPa3Animation09004Indices[20] = {
#include "assets/pa3_animation_09004_indices.inc"
};

static AnimationSet _gPa3Animation09004 = {
    _gPa3Animation09004Records,
    _gPa3Animation09004Indices,
    { NULL, _gPa3Animation09004Bank1, NULL, NULL, _gPa3Animation09004Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation091E0Bank1[2] = {
#include "assets/pa3_animation_091E0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation091E0Bank4[17] = {
#include "assets/pa3_animation_091E0_bank4.inc"
};

static AnimationRecord _gPa3Animation091E0Records[76] = {
#include "assets/pa3_animation_091E0_records.inc"
};

static u16 _gPa3Animation091E0Indices[20] = {
#include "assets/pa3_animation_091E0_indices.inc"
};

static AnimationSet _gPa3Animation091E0 = {
    _gPa3Animation091E0Records,
    _gPa3Animation091E0Indices,
    { NULL, _gPa3Animation091E0Bank1, NULL, NULL, _gPa3Animation091E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation09984Bank1[13] = {
#include "assets/pa3_animation_09984_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation09984Bank4[177] = {
#include "assets/pa3_animation_09984_bank4.inc"
};

static AnimationRecord _gPa3Animation09984Records[253] = {
#include "assets/pa3_animation_09984_records.inc"
};

static u16 _gPa3Animation09984Indices[20] = {
#include "assets/pa3_animation_09984_indices.inc"
};

static AnimationSet _gPa3Animation09984 = {
    _gPa3Animation09984Records,
    _gPa3Animation09984Indices,
    { NULL, _gPa3Animation09984Bank1, NULL, NULL, _gPa3Animation09984Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0A42CBank1[19] = {
#include "assets/pa3_animation_0A42C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0A42CBank4[269] = {
#include "assets/pa3_animation_0A42C_bank4.inc"
};

static AnimationRecord _gPa3Animation0A42CRecords[336] = {
#include "assets/pa3_animation_0A42C_records.inc"
};

static u16 _gPa3Animation0A42CIndices[20] = {
#include "assets/pa3_animation_0A42C_indices.inc"
};

static AnimationSet _gPa3Animation0A42C = {
    _gPa3Animation0A42CRecords,
    _gPa3Animation0A42CIndices,
    { NULL, _gPa3Animation0A42CBank1, NULL, NULL, _gPa3Animation0A42CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0B590Bank1[30] = {
#include "assets/pa3_animation_0B590_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0B590Bank4[461] = {
#include "assets/pa3_animation_0B590_bank4.inc"
};

static AnimationRecord _gPa3Animation0B590Records[542] = {
#include "assets/pa3_animation_0B590_records.inc"
};

static u16 _gPa3Animation0B590Indices[20] = {
#include "assets/pa3_animation_0B590_indices.inc"
};

static AnimationSet _gPa3Animation0B590 = {
    _gPa3Animation0B590Records,
    _gPa3Animation0B590Indices,
    { NULL, _gPa3Animation0B590Bank1, NULL, NULL, _gPa3Animation0B590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0C348Bank1[23] = {
#include "assets/pa3_animation_0C348_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0C348Bank4[354] = {
#include "assets/pa3_animation_0C348_bank4.inc"
};

static AnimationRecord _gPa3Animation0C348Records[435] = {
#include "assets/pa3_animation_0C348_records.inc"
};

static u16 _gPa3Animation0C348Indices[20] = {
#include "assets/pa3_animation_0C348_indices.inc"
};

static AnimationSet _gPa3Animation0C348 = {
    _gPa3Animation0C348Records,
    _gPa3Animation0C348Indices,
    { NULL, _gPa3Animation0C348Bank1, NULL, NULL, _gPa3Animation0C348Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0CAA0Bank1[13] = {
#include "assets/pa3_animation_0CAA0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0CAA0Bank4[184] = {
#include "assets/pa3_animation_0CAA0_bank4.inc"
};

static AnimationRecord _gPa3Animation0CAA0Records[227] = {
#include "assets/pa3_animation_0CAA0_records.inc"
};

static u16 _gPa3Animation0CAA0Indices[20] = {
#include "assets/pa3_animation_0CAA0_indices.inc"
};

static AnimationSet _gPa3Animation0CAA0 = {
    _gPa3Animation0CAA0Records,
    _gPa3Animation0CAA0Indices,
    { NULL, _gPa3Animation0CAA0Bank1, NULL, NULL, _gPa3Animation0CAA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0D578Bank1[19] = {
#include "assets/pa3_animation_0D578_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0D578Bank4[288] = {
#include "assets/pa3_animation_0D578_bank4.inc"
};

static AnimationRecord _gPa3Animation0D578Records[329] = {
#include "assets/pa3_animation_0D578_records.inc"
};

static u16 _gPa3Animation0D578Indices[20] = {
#include "assets/pa3_animation_0D578_indices.inc"
};

static AnimationSet _gPa3Animation0D578 = {
    _gPa3Animation0D578Records,
    _gPa3Animation0D578Indices,
    { NULL, _gPa3Animation0D578Bank1, NULL, NULL, _gPa3Animation0D578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0DE30Bank1[18] = {
#include "assets/pa3_animation_0DE30_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0DE30Bank4[201] = {
#include "assets/pa3_animation_0DE30_bank4.inc"
};

static AnimationRecord _gPa3Animation0DE30Records[283] = {
#include "assets/pa3_animation_0DE30_records.inc"
};

static u16 _gPa3Animation0DE30Indices[20] = {
#include "assets/pa3_animation_0DE30_indices.inc"
};

static AnimationSet _gPa3Animation0DE30 = {
    _gPa3Animation0DE30Records,
    _gPa3Animation0DE30Indices,
    { NULL, _gPa3Animation0DE30Bank1, NULL, NULL, _gPa3Animation0DE30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0E5D0Bank1[16] = {
#include "assets/pa3_animation_0E5D0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0E5D0Bank4[157] = {
#include "assets/pa3_animation_0E5D0_bank4.inc"
};

static AnimationRecord _gPa3Animation0E5D0Records[263] = {
#include "assets/pa3_animation_0E5D0_records.inc"
};

static u16 _gPa3Animation0E5D0Indices[20] = {
#include "assets/pa3_animation_0E5D0_indices.inc"
};

static AnimationSet _gPa3Animation0E5D0 = {
    _gPa3Animation0E5D0Records,
    _gPa3Animation0E5D0Indices,
    { NULL, _gPa3Animation0E5D0Bank1, NULL, NULL, _gPa3Animation0E5D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0EFA4Bank1[25] = {
#include "assets/pa3_animation_0EFA4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0EFA4Bank4[223] = {
#include "assets/pa3_animation_0EFA4_bank4.inc"
};

static AnimationRecord _gPa3Animation0EFA4Records[311] = {
#include "assets/pa3_animation_0EFA4_records.inc"
};

static u16 _gPa3Animation0EFA4Indices[20] = {
#include "assets/pa3_animation_0EFA4_indices.inc"
};

static AnimationSet _gPa3Animation0EFA4 = {
    _gPa3Animation0EFA4Records,
    _gPa3Animation0EFA4Indices,
    { NULL, _gPa3Animation0EFA4Bank1, NULL, NULL, _gPa3Animation0EFA4Bank4, NULL, NULL, NULL },
};

AnimationSet* gPa3PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY] = {
    [1] = &_gPa3Animation009F8,
    &_gPa3Animation0DE30,
    &_gPa3Animation0E5D0,
    &_gPa3Animation0EFA4,
    &_gPa3Animation018FC,
    &_gPa3Animation02160,
    &_gPa3Animation0CAA0,
    &_gPa3Animation0D578,
    &_gPa3Animation091E0,
    &_gPa3Animation0C348,
    &_gPa3Animation0C348,
    &_gPa3Animation0A42C,
    &_gPa3Animation09984,
    &_gPa3Animation0B590,
    &_gPa3Animation0B590,
    &_gPa3Animation04DC4,
    &_gPa3Animation050E4,
    &_gPa3Animation058A8,
    &_gPa3Animation0109C,
    &_gPa3Animation0B590,
    &_gPa3Animation009F8,
    &_gPa3Animation009F8,
    &_gPa3Animation06B40,
    &_gPa3Animation07DD4,
    &_gPa3Animation076B8,
    &_gPa3Animation03E44,
    &_gPa3Animation04040,
    &_gPa3Animation04318,
    &_gPa3Animation045BC,
    &_gPa3Animation047BC,
    &_gPa3Animation04B10,
    &_gPa3Animation08254,
    &_gPa3Animation0842C,
    &_gPa3Animation08254,
    &_gPa3Animation0842C,
    &_gPa3Animation02BD0,
    &_gPa3Animation03358,
    &_gPa3Animation039BC,
    &_gPa3Animation0362C,
    &_gPa3Animation02474,
    &_gPa3Animation009F8,
    &_gPa3Animation08990,
    &_gPa3Animation08B84,
    &_gPa3Animation09004,
};
#elif WEAPON_INDEX == PA3_WEAPON_INDEX_SP12
/// Encoding-indexed banks used by the player clips below; all other banks are absent.
enum {
    SP12_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    SP12_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Translation/rotation keyframes for `_gSp12IdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12IdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_00B38_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12IdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12IdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_00B38_bank4.inc"
};

/// Keyframe durations and control records for `_gSp12IdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12IdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_00B38_records.inc"
};

/// Record-array starts of `_gSp12IdleAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gSp12IdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_00B38_indices.inc"
};

/// Standing player pose in the SP12 bank, shared by slots 1, 21, 22 and 41.
///
/// Locomotion selects slot 1 when movement and turning stop.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12IdleAnimation = {
    .records           = _gSp12IdleAnimationRecords,
    .trackStartIndices = _gSp12IdleAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12IdleAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12IdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12UnarmedWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12UnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_011DC_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12UnarmedWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12UnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_011DC_bank4.inc"
};

/// Keyframe durations and control records for `_gSp12UnarmedWalkAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12UnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_011DC_records.inc"
};

/// Record-array starts of `_gSp12UnarmedWalkAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gSp12UnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_011DC_indices.inc"
};

/// Player forward-walk clip in SP12 bank slot 19 while the held model is absent.
///
/// The weapon remains selected; an empty equipment-model slot selects this clip.
/// Borrowed tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12UnarmedWalkAnimation = {
    .records           = _gSp12UnarmedWalkAnimationRecords,
    .trackStartIndices = _gSp12UnarmedWalkAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12UnarmedWalkAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12UnarmedWalkAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12NegativeTurnAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12NegativeTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_01A3C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12NegativeTurnAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12NegativeTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_01A3C_bank4.inc"
};

/// Keyframe durations and control records for `_gSp12NegativeTurnAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12NegativeTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_01A3C_records.inc"
};

/// Record-array starts of `_gSp12NegativeTurnAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gSp12NegativeTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_01A3C_indices.inc"
};

/// Stationary player turn for a negative turn sign, in SP12 bank slot 5.
///
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12NegativeTurnAnimation = {
    .records           = _gSp12NegativeTurnAnimationRecords,
    .trackStartIndices = _gSp12NegativeTurnAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12NegativeTurnAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12NegativeTurnAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PositiveTurnAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12PositiveTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_022A0_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PositiveTurnAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PositiveTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_022A0_bank4.inc"
};

/// Keyframe durations and control records for `_gSp12PositiveTurnAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12PositiveTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_022A0_records.inc"
};

/// Record-array starts of `_gSp12PositiveTurnAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gSp12PositiveTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_022A0_indices.inc"
};

/// Stationary player turn for a positive turn sign, in SP12 bank slot 6.
///
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12PositiveTurnAnimation = {
    .records           = _gSp12PositiveTurnAnimationRecords,
    .trackStartIndices = _gSp12PositiveTurnAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PositiveTurnAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PositiveTurnAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12ItemUseAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12ItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_025B4_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12ItemUseAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12ItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_025B4_bank4.inc"
};

/// Keyframe durations and control records for `_gSp12ItemUseAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12ItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_025B4_records.inc"
};

/// Record-array starts of `_gSp12ItemUseAnimation`'s nineteen model-part tracks.
///
/// The final halfword preserves the serialized descriptor's word alignment;
/// playback indexes only the first nineteen entries (tracks 0..18).
static const u16 _gSp12ItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_025B4_indices.inc"
};

/// Scripted player item-use clip in SP12 bank slot 40.
///
/// Playback blends in over six normal-rate frames before returning to aim locomotion.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12ItemUseAnimation = {
    .records           = _gSp12ItemUseAnimationRecords,
    .trackStartIndices = _gSp12ItemUseAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12ItemUseAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12ItemUseAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12StairAscentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12StairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_02D10_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12StairAscentAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12StairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_02D10_bank4.inc"
};

/// Keyframe durations, footstep cues and loop controls for `_gSp12StairAscentAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12StairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_02D10_records.inc"
};

/// Record-array starts of `_gSp12StairAscentAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12StairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_02D10_indices.inc"
};

/// Looping scripted stair-ascent clip in SP12 bank slot 36.
///
/// Footstep cues gate each stair stride; an even/odd finish clip completes the flight.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12StairAscentAnimation = {
    .records           = _gSp12StairAscentAnimationRecords,
    .trackStartIndices = _gSp12StairAscentAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12StairAscentAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12StairAscentAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12StairDescentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12StairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03498_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12StairDescentAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12StairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03498_bank4.inc"
};

/// Keyframe durations, footstep cues and loop controls for `_gSp12StairDescentAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12StairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03498_records.inc"
};

/// Record-array starts of `_gSp12StairDescentAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12StairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03498_indices.inc"
};

/// Looping scripted stair-descent clip in SP12 bank slot 37.
///
/// Footstep cues gate the descending strides before playback returns to idle.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12StairDescentAnimation = {
    .records           = _gSp12StairDescentAnimationRecords,
    .trackStartIndices = _gSp12StairDescentAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12StairDescentAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12StairDescentAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12StairAscentFinishOddAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12StairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0376C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12StairAscentFinishOddAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12StairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0376C_bank4.inc"
};

/// Keyframe durations, a footstep cue and stop controls for `_gSp12StairAscentFinishOddAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12StairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0376C_records.inc"
};

/// Record-array starts of `_gSp12StairAscentFinishOddAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12StairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0376C_indices.inc"
};

/// Stair-ascent finish for an odd original stair count, in SP12 bank slot 39.
///
/// Blends in over three normal-rate frames and holds its final pose before returning to idle.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12StairAscentFinishOddAnimation = {
    .records           = _gSp12StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gSp12StairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12StairAscentFinishOddAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12StairAscentFinishOddAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12StairAscentFinishEvenAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12StairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03AFC_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12StairAscentFinishEvenAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12StairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03AFC_bank4.inc"
};

/// Keyframe durations, a footstep cue and stop controls for `_gSp12StairAscentFinishEvenAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12StairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03AFC_records.inc"
};

/// Record-array starts of `_gSp12StairAscentFinishEvenAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12StairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03AFC_indices.inc"
};

/// Stair-ascent finish for an even original stair count, in SP12 bank slot 38.
///
/// Blends in over three normal-rate frames and holds its final pose before returning to idle.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12StairAscentFinishEvenAnimation = {
    .records           = _gSp12StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gSp12StairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12StairAscentFinishEvenAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12StairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PeReleaseFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12PeReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03F84_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeReleaseFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03F84_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12PeReleaseFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03F84_records.inc"
};

/// Record-array starts of `_gSp12PeReleaseFamily0Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_03F84_indices.inc"
};

/// Parasite Energy action release for clip family 0, in SP12 bank slot 26.
///
/// Selected when `GameActor.actionArgument` is zero; blends in over six normal-rate frames
/// before the charge phase. Borrowed nineteen-part tracks and pose data must remain loaded.
static AnimationSet _gSp12PeReleaseFamily0Animation = {
    .records           = _gSp12PeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gSp12PeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeReleaseFamily0AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeReleaseFamily0AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PeChargeFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12PeChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04180_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeChargeFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04180_bank4.inc"
};

/// Keyframe durations and loop controls for `_gSp12PeChargeFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04180_records.inc"
};

/// Record-array starts of `_gSp12PeChargeFamily0Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04180_indices.inc"
};

/// Looping Parasite Energy charge for clip family 0, in SP12 bank slot 27.
///
/// Selected when `GameActor.actionArgument` is zero; restarts without blending and loops
/// until the attachment countdown reaches zero. Borrowed tracks and pose data must remain loaded.
static AnimationSet _gSp12PeChargeFamily0Animation = {
    .records           = _gSp12PeChargeFamily0AnimationRecords,
    .trackStartIndices = _gSp12PeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeChargeFamily0AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeChargeFamily0AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PeCastFamily0Animation`, in model units and 1/4096 turns.
///
/// Read-only clip arrays retain their serialized placement in the package's data section.
static const AnimationPackedPose _gSp12PeCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04458_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeCastFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04458_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12PeCastFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04458_records.inc"
};

/// Record-array starts of `_gSp12PeCastFamily0Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04458_indices.inc"
};

/// Parasite Energy cast for clip family 0, in SP12 bank slot 28.
///
/// Selected when `GameActor.actionArgument` is zero and the charge countdown ends.
/// Restarts without blending and holds its final pose before normal control resumes.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12PeCastFamily0Animation = {
    .records           = _gSp12PeCastFamily0AnimationRecords,
    .trackStartIndices = _gSp12PeCastFamily0AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeCastFamily0AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeCastFamily0AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PeReleaseFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12PeReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_046FC_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeReleaseFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_046FC_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12PeReleaseFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_046FC_records.inc"
};

/// Record-array starts of `_gSp12PeReleaseFamily1Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_046FC_indices.inc"
};

/// Parasite Energy action release for clip family 1, in SP12 bank slot 29.
///
/// Selected when `GameActor.actionArgument` is one; blends in over six normal-rate frames
/// before the charge phase. Borrowed nineteen-part tracks and pose data must remain loaded.
static AnimationSet _gSp12PeReleaseFamily1Animation = {
    .records           = _gSp12PeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gSp12PeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeReleaseFamily1AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeReleaseFamily1AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PeChargeFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12PeChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_048FC_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeChargeFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_048FC_bank4.inc"
};

/// Keyframe durations and loop/stop controls for `_gSp12PeChargeFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_048FC_records.inc"
};

/// Record-array starts of `_gSp12PeChargeFamily1Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_048FC_indices.inc"
};

/// Looping Parasite Energy charge for clip family 1, in SP12 bank slot 30.
///
/// Selected when `GameActor.actionArgument` is one; restarts without blending and loops
/// until the attachment countdown reaches zero. Borrowed tracks and pose data must remain loaded.
static AnimationSet _gSp12PeChargeFamily1Animation = {
    .records           = _gSp12PeChargeFamily1AnimationRecords,
    .trackStartIndices = _gSp12PeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeChargeFamily1AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeChargeFamily1AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12PeCastFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12PeCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04C50_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeCastFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04C50_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12PeCastFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04C50_records.inc"
};

/// Record-array starts of `_gSp12PeCastFamily1Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04C50_indices.inc"
};

/// Parasite Energy cast for clip family 1, in SP12 bank slot 31.
///
/// Selected when `GameActor.actionArgument` is one and the charge countdown ends.
/// Restarts without blending and holds its final pose before normal control resumes.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12PeCastFamily1Animation = {
    .records           = _gSp12PeCastFamily1AnimationRecords,
    .trackStartIndices = _gSp12PeCastFamily1AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeCastFamily1AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeCastFamily1AnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12Part4HitReactionAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12Part4HitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04F04_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12Part4HitReactionAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12Part4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04F04_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12Part4HitReactionAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12Part4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04F04_records.inc"
};

/// Record-array starts of `_gSp12Part4HitReactionAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12Part4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_04F04_indices.inc"
};

/// Player hit reaction for contact on model part 4, in SP12 bank slot 16.
///
/// Selected for `GameActor.hitRegion` one, the collision body attached to part 4.
/// Blends child tracks 1..18 over three normal-rate frames and holds the final pose.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12Part4HitReactionAnimation = {
    .records           = _gSp12Part4HitReactionAnimationRecords,
    .trackStartIndices = _gSp12Part4HitReactionAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12Part4HitReactionAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12Part4HitReactionAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12OtherBodyHitReactionAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12OtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_05224_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12OtherBodyHitReactionAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12OtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_05224_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12OtherBodyHitReactionAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12OtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_05224_records.inc"
};

/// Record-array starts of `_gSp12OtherBodyHitReactionAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12OtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_05224_indices.inc"
};

/// Player hit reaction for other body contacts, in SP12 bank slot 17.
///
/// Selected for a pending hit outside the model-part-4 region.
/// Blends child tracks 1..18 over three normal-rate frames and holds the final pose.
/// Borrowed nineteen-part tracks and pose data must remain loaded throughout playback.
static AnimationSet _gSp12OtherBodyHitReactionAnimation = {
    .records           = _gSp12OtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gSp12OtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12OtherBodyHitReactionAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12OtherBodyHitReactionAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12DeathAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12DeathAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_059E8_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12DeathAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12DeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_059E8_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12DeathAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12DeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_059E8_records.inc"
};

/// Record-array starts of `_gSp12DeathAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12DeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_059E8_indices.inc"
};

/// Player death pose in SP12 bank slot 18.
///
/// Zero or negative HP selects this clip on child tracks 1..18 with a four-frame
/// blend. Its stop controls retain the final pose; a zero-duration entry blend
/// resets those tracks directly. The nineteen tracks and their pose data borrow
/// the loaded SP12 package for as long as playback refers to them.
static AnimationSet _gSp12DeathAnimation = {
    .records           = _gSp12DeathAnimationRecords,
    .trackStartIndices = _gSp12DeathAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12DeathAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12DeathAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12HighHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12HighHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_06C80_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12HighHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12HighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_06C80_bank4.inc"
};

/// Keyframe durations and jump/stop controls for `_gSp12HighHealthIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12HighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_06C80_records.inc"
};

/// Record-array starts of `_gSp12HighHealthIdleAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12HighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_06C80_indices.inc"
};

/// Extended player idle above half maximum HP, in SP12 bank slot 23.
///
/// After 300 direction-free locomotion ticks, child tracks 1..18 blend in over
/// five normal-rate frames. Track jumps repeat the idle until another clip is
/// selected. The nineteen tracks and pose data must remain loaded during playback.
static AnimationSet _gSp12HighHealthIdleAnimation = {
    .records           = _gSp12HighHealthIdleAnimationRecords,
    .trackStartIndices = _gSp12HighHealthIdleAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12HighHealthIdleAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12HighHealthIdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12LowHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12LowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_077F8_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12LowHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12LowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_077F8_bank4.inc"
};

/// Keyframe durations and jump/stop controls for `_gSp12LowHealthIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12LowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_077F8_records.inc"
};

/// Record-array starts of `_gSp12LowHealthIdleAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12LowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_077F8_indices.inc"
};

/// Extended idle at or below a quarter of maximum HP and paralysis clip, in SP12 bank slot 25.
///
/// Direction-free locomotion selects it after 300 ticks with a five-frame blend;
/// paralysis selects it with a six-frame blend. Both update all child tracks
/// 1..18. Track jumps repeat the pose sequence. The nineteen tracks and pose
/// data must remain loaded during playback.
static AnimationSet _gSp12LowHealthIdleAnimation = {
    .records           = _gSp12LowHealthIdleAnimationRecords,
    .trackStartIndices = _gSp12LowHealthIdleAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12LowHealthIdleAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12LowHealthIdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12MediumHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12MediumHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_07F14_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12MediumHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12MediumHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_07F14_bank4.inc"
};

/// Keyframe durations and jump/stop controls for `_gSp12MediumHealthIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12MediumHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_07F14_records.inc"
};

/// Record-array starts of `_gSp12MediumHealthIdleAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12MediumHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_07F14_indices.inc"
};

/// Extended player idle above a quarter and at most half maximum HP, in SP12 bank slot 24.
///
/// After 300 direction-free locomotion ticks, child tracks 1..18 blend in over
/// five normal-rate frames. Track jumps repeat the idle until another clip is
/// selected. HP thresholds use integer-truncated halves and quarters of maximum
/// HP. The nineteen tracks and pose data must remain loaded during playback.
static AnimationSet _gSp12MediumHealthIdleAnimation = {
    .records           = _gSp12MediumHealthIdleAnimationRecords,
    .trackStartIndices = _gSp12MediumHealthIdleAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12MediumHealthIdleAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12MediumHealthIdleAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12ScriptedPresentationEnterAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12ScriptedPresentationEnterAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08394_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12ScriptedPresentationEnterAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12ScriptedPresentationEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08394_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12ScriptedPresentationEnterAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12ScriptedPresentationEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08394_records.inc"
};

/// Record-array starts of `_gSp12ScriptedPresentationEnterAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12ScriptedPresentationEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08394_indices.inc"
};

/// Player entry to a scripted presentation, in SP12 bank slots 32 and 34.
///
/// A zero presentation selector plays slot 32 before a CAP or room command,
/// starting child tracks 1..18 without a transition interval. Stop controls
/// retain the final pose while the command runs. Slot 34 aliases the same set.
/// The nineteen tracks and pose data must remain loaded during playback.
static AnimationSet _gSp12ScriptedPresentationEnterAnimation = {
    .records           = _gSp12ScriptedPresentationEnterAnimationRecords,
    .trackStartIndices = _gSp12ScriptedPresentationEnterAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12ScriptedPresentationEnterAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12ScriptedPresentationEnterAnimationBank4,
    },
};

/// Translation/rotation keyframes for `_gSp12ScriptedPresentationLeaveAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gSp12ScriptedPresentationLeaveAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0856C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12ScriptedPresentationLeaveAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12ScriptedPresentationLeaveAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0856C_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12ScriptedPresentationLeaveAnimation`'s nineteen part tracks.
static const AnimationRecord _gSp12ScriptedPresentationLeaveAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0856C_records.inc"
};

/// Record-array starts of `_gSp12ScriptedPresentationLeaveAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12ScriptedPresentationLeaveAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0856C_indices.inc"
};

/// Player exit from a scripted presentation, in SP12 bank slots 33 and 35.
///
/// A nonzero presentation selector plays slot 33 after a CAP or room command,
/// starting child tracks 1..18 without a transition interval. Stop controls
/// retain the final pose until later playback resumes. Slot 35 aliases the same
/// set. The nineteen tracks and pose data must remain loaded during playback.
static AnimationSet _gSp12ScriptedPresentationLeaveAnimation = {
    .records           = _gSp12ScriptedPresentationLeaveAnimationRecords,
    .trackStartIndices = _gSp12ScriptedPresentationLeaveAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12ScriptedPresentationLeaveAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12ScriptedPresentationLeaveAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12PeReleaseFamily2Animation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12PeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_08AD0_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeReleaseFamily2Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08AD0_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12PeReleaseFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08AD0_records.inc"
};

/// Absolute record starts for `_gSp12PeReleaseFamily2Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08AD0_indices.inc"
};

/// Parasite Energy release for clip family 2, in SP12 bank slot 42.
///
/// Child tracks 1..18 blend in over six normal-rate frames and stop before
/// charge playback begins. The nineteen tracks and pose banks must remain
/// loaded while any playback slot or cached record pointer refers to them.
static AnimationSet _gSp12PeReleaseFamily2Animation = {
    .records           = _gSp12PeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gSp12PeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeReleaseFamily2AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeReleaseFamily2AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12PeChargeFamily2Animation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12PeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_08CC4_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeChargeFamily2Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08CC4_bank4.inc"
};

/// Keyframe durations, return jumps and terminal controls for `_gSp12PeChargeFamily2Animation`'s nineteen tracks.
static const AnimationRecord _gSp12PeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08CC4_records.inc"
};

/// Absolute record starts for `_gSp12PeChargeFamily2Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_08CC4_indices.inc"
};

/// Looping Parasite Energy charge for clip family 2, in SP12 bank slot 43.
///
/// Child tracks 1..18 restart after release and loop until the attachment's
/// countdown ends, when cast playback replaces them. The nineteen tracks
/// and pose banks must remain loaded while referenced by playback or a cached record.
static AnimationSet _gSp12PeChargeFamily2Animation = {
    .records           = _gSp12PeChargeFamily2AnimationRecords,
    .trackStartIndices = _gSp12PeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeChargeFamily2AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeChargeFamily2AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12PeCastFamily2Animation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12PeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_09144_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12PeCastFamily2Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12PeCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_09144_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12PeCastFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gSp12PeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_09144_records.inc"
};

/// Absolute record starts for `_gSp12PeCastFamily2Animation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12PeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_09144_indices.inc"
};

/// Parasite Energy cast for clip family 2, in SP12 bank slot 44.
///
/// Child tracks 1..18 restart when the attachment countdown ends and stop
/// before normal movement or aiming resumes. The nineteen tracks and pose banks
/// must remain loaded while referenced by playback or a cached record.
static AnimationSet _gSp12PeCastFamily2Animation = {
    .records           = _gSp12PeCastFamily2AnimationRecords,
    .trackStartIndices = _gSp12PeCastFamily2AnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12PeCastFamily2AnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12PeCastFamily2AnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12AimIdleAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12AimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0931C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12AimIdleAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12AimIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0931C_bank4.inc"
};

/// Keyframe durations, return jumps and terminal controls for `_gSp12AimIdleAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12AimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0931C_records.inc"
};

/// Absolute record starts for `_gSp12AimIdleAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12AimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0931C_indices.inc"
};

/// Stationary aiming pose and attack-ready clip in SP12 bank slot 9.
///
/// Aiming uses child tracks 1..18 when movement and turning stop; attack
/// preparation blends into the same pose. Recovery from a Desert Chaser's
/// knock-back borrows this set through the equipped weapon bank. The nineteen
/// tracks and pose banks must remain loaded while any borrower refers to them.
static AnimationSet _gSp12AimIdleAnimation = {
    .records           = _gSp12AimIdleAnimationRecords,
    .trackStartIndices = _gSp12AimIdleAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12AimIdleAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12AimIdleAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12AimBackwardOrTurnAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12AimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_09B5C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12AimBackwardOrTurnAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12AimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_09B5C_bank4.inc"
};

/// Keyframes, loop jumps and terminal controls for `_gSp12AimBackwardOrTurnAnimation`'s nineteen tracks.
///
/// Track 1 carries the paired footstep cues consumed during locomotion.
static const AnimationRecord _gSp12AimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_09B5C_records.inc"
};

/// Absolute record starts for `_gSp12AimBackwardOrTurnAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12AimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_09B5C_indices.inc"
};

/// Looping backward-walk or stationary-turn aiming clip in SP12 bank slot 13.
///
/// Child tracks 1..18 play while aiming with backward movement or a turn
/// without movement. Track 1 supplies alternating footstep cues. The nineteen
/// tracks and pose banks must remain loaded while referenced by playback or a cached record.
static AnimationSet _gSp12AimBackwardOrTurnAnimation = {
    .records           = _gSp12AimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gSp12AimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12AimBackwardOrTurnAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12AimBackwardOrTurnAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12AimForwardWalkAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12AimForwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0A604_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12AimForwardWalkAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12AimForwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0A604_bank4.inc"
};

/// Keyframes, loop jumps and terminal controls for `_gSp12AimForwardWalkAnimation`'s nineteen tracks.
///
/// Track 1 carries the paired footstep cues consumed during locomotion.
static const AnimationRecord _gSp12AimForwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0A604_records.inc"
};

/// Absolute record starts for `_gSp12AimForwardWalkAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12AimForwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0A604_indices.inc"
};

/// Looping forward-walk aiming clip in SP12 bank slot 12.
///
/// Child tracks 1..18 play while aiming with forward movement; track 1 supplies
/// alternating footstep cues. Root displacement is driven separately. The
/// nineteen tracks and pose banks must remain loaded while referenced by
/// playback or a cached record.
static AnimationSet _gSp12AimForwardWalkAnimation = {
    .records           = _gSp12AimForwardWalkAnimationRecords,
    .trackStartIndices = _gSp12AimForwardWalkAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12AimForwardWalkAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12AimForwardWalkAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12ReloadAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12ReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0BA44_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12ReloadAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12ReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0BA44_bank4.inc"
};

/// Keyframe durations, paired reload cues and stop controls for `_gSp12ReloadAnimation`'s nineteen tracks.
///
/// Track 1's first full-mask cue applies the load; its second requests the pump sound.
static const AnimationRecord _gSp12ReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0BA44_records.inc"
};

/// Absolute record starts for `_gSp12ReloadAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12ReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0BA44_indices.inc"
};

/// Reload clip shared by primary, secondary and battle-end reloads in SP12 bank slots 14, 15 and 20.
///
/// Child tracks 1..18 play through the load and pump cues, then hold their final
/// poses. The nineteen tracks and pose banks must remain loaded while referenced
/// by playback or a cached record.
static AnimationSet _gSp12ReloadAnimation = {
    .records           = _gSp12ReloadAnimationRecords,
    .trackStartIndices = _gSp12ReloadAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12ReloadAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12ReloadAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12ShotAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12ShotAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0C4B8_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12ShotAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12ShotAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0C4B8_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12ShotAnimation`'s nineteen tracks.
///
/// Track 1 retains a full-mask cue unused by normal shotgun firing.
static const AnimationRecord _gSp12ShotAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0C4B8_records.inc"
};

/// Absolute record starts for `_gSp12ShotAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12ShotAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0C4B8_indices.inc"
};

/// Shot and recovery clip in SP12 bank slots 10 and 11.
///
/// Firing blends child tracks 1..18 into slot 10 after the ready pose. The clip
/// holds its final poses while the attack handler controls impact and pump
/// timing. The nineteen tracks and pose banks must remain loaded while
/// referenced by playback or a cached record.
static AnimationSet _gSp12ShotAnimation = {
    .records           = _gSp12ShotAnimationRecords,
    .trackStartIndices = _gSp12ShotAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12ShotAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12ShotAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12AimEnterAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12AimEnterAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0CB3C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12AimEnterAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12AimEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0CB3C_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12AimEnterAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12AimEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0CB3C_records.inc"
};

/// Absolute record starts for `_gSp12AimEnterAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12AimEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0CB3C_indices.inc"
};

/// Transition into shotgun aiming in SP12 bank slot 7.
///
/// Aim entry resets or blends child tracks 1..18 and holds the final poses
/// before aim locomotion. The nineteen tracks and pose banks must remain
/// loaded while referenced by playback or a cached record.
static AnimationSet _gSp12AimEnterAnimation = {
    .records           = _gSp12AimEnterAnimationRecords,
    .trackStartIndices = _gSp12AimEnterAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12AimEnterAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12AimEnterAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12AimExitAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12AimExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0D0B4_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12AimExitAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12AimExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0D0B4_bank4.inc"
};

/// Keyframe durations and stop controls for `_gSp12AimExitAnimation`'s nineteen tracks.
static const AnimationRecord _gSp12AimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0D0B4_records.inc"
};

/// Absolute record starts for `_gSp12AimExitAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12AimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0D0B4_indices.inc"
};

/// Transition out of shotgun aiming in SP12 bank slot 8.
///
/// Aim exit blends child tracks 1..18 over six normal-rate frames and holds
/// their final poses before ordinary locomotion resumes. The nineteen tracks
/// and pose banks must remain loaded while referenced by playback or a cached record.
static AnimationSet _gSp12AimExitAnimation = {
    .records           = _gSp12AimExitAnimationRecords,
    .trackStartIndices = _gSp12AimExitAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12AimExitAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12AimExitAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12ForwardWalkAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12ForwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0D96C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12ForwardWalkAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12ForwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0D96C_bank4.inc"
};

/// Keyframes, loop jumps and terminal controls for `_gSp12ForwardWalkAnimation`'s nineteen tracks.
///
/// Track 1 carries the paired footstep cues consumed during locomotion.
static const AnimationRecord _gSp12ForwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0D96C_records.inc"
};

/// Absolute record starts for `_gSp12ForwardWalkAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12ForwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0D96C_indices.inc"
};

/// Looping forward walk with the equipped shotgun in SP12 bank slot 2.
///
/// Ordinary forward locomotion plays child tracks 1..18 when the weapon model
/// is present; track 1 supplies alternating footstep cues. Root displacement
/// is driven separately. The nineteen tracks and pose banks must remain loaded
/// while referenced by playback or a cached record.
static AnimationSet _gSp12ForwardWalkAnimation = {
    .records           = _gSp12ForwardWalkAnimationRecords,
    .trackStartIndices = _gSp12ForwardWalkAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12ForwardWalkAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12ForwardWalkAnimationBank4,
    },
};

/// Translation/Euler keyframes for `_gSp12BackwardWalkAnimation`'s tracks 0 and 1.
///
/// Translations use model units; angles use 4096 units per turn. Pose reads require word alignment.
static const AnimationPackedPose _gSp12BackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/sp12_animation_0E10C_bank1.inc"
};

/// Rotation-only keyframes for `_gSp12BackwardWalkAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gSp12BackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0E10C_bank4.inc"
};

/// Keyframes, loop jumps and terminal controls for `_gSp12BackwardWalkAnimation`'s nineteen tracks.
///
/// Track 1 carries the paired footstep cues consumed during locomotion.
static const AnimationRecord _gSp12BackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0E10C_records.inc"
};

/// Absolute record starts for `_gSp12BackwardWalkAnimation`'s nineteen model-part tracks.
///
/// Entries 0..18 are track starts; the serialized trailing zero halfword is retained.
static const u16 _gSp12BackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0E10C_indices.inc"
};

/// Looping backward walk with the shotgun in SP12 bank slot 3.
///
/// Ordinary backward locomotion plays child tracks 1..18; track 1 supplies
/// alternating footstep cues. Root displacement is driven separately. The
/// nineteen tracks and pose banks must remain loaded while referenced by
/// playback or a cached record.
static AnimationSet _gSp12BackwardWalkAnimation = {
    .records           = _gSp12BackwardWalkAnimationRecords,
    .trackStartIndices = _gSp12BackwardWalkAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12BackwardWalkAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12BackwardWalkAnimationBank4,
    },
};

/// Run translations in model units and rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gSp12RunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0EAE0_bank1.inc"
};

/// Run rotations for tracks 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gSp12RunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0EAE0_bank4.inc"
};

/// Run keyframes, loop jumps and retained stops; durations count normal-rate frames.
///
/// Pose offsets count four-byte words; jump targets are absolute record indices.
/// Track 1 carries alternating footstep cues `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2`.
static const AnimationRecord _gSp12RunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0EAE0_records.inc"
};

/// Absolute run record starts for tracks 0..18, followed by a retained serialized halfword.
static const u16 _gSp12RunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/sp12_animation_0EAE0_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gSp12RunAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, sp12_run_track_starts);

/// Looping player run with the SP12, at native clip id 4.
///
/// Normal locomotion and scripted run-to playback use child tracks 1..18;
/// track 1 supplies paired footstep cues. Root displacement is driven separately.
/// Tracks 0..1 carry translation/rotation and 2..18 rotation only. Keep the
/// SP12 package loaded while any slot or cached record refers to this clip.
static AnimationSet _gSp12RunAnimation = {
    .records           = _gSp12RunAnimationRecords,
    .trackStartIndices = _gSp12RunAnimationIndices,
    .poseBanks         = {
        [SP12_ANIMATION_TRANSLATION_ROTATION_BANK] = _gSp12RunAnimationBank1,
        [SP12_ANIMATION_PACKED_ROTATION_BANK]      = _gSp12RunAnimationBank4,
    },
};

AnimationSet* gSp12PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY] = {
    NULL,
    &_gSp12IdleAnimation,
    &_gSp12ForwardWalkAnimation,
    &_gSp12BackwardWalkAnimation,
    &_gSp12RunAnimation,
    &_gSp12NegativeTurnAnimation,
    &_gSp12PositiveTurnAnimation,
    &_gSp12AimEnterAnimation,
    &_gSp12AimExitAnimation,
    &_gSp12AimIdleAnimation,
    &_gSp12ShotAnimation,
    &_gSp12ShotAnimation,
    &_gSp12AimForwardWalkAnimation,
    &_gSp12AimBackwardOrTurnAnimation,
    &_gSp12ReloadAnimation,
    &_gSp12ReloadAnimation,
    &_gSp12Part4HitReactionAnimation,
    &_gSp12OtherBodyHitReactionAnimation,
    &_gSp12DeathAnimation,
    &_gSp12UnarmedWalkAnimation,
    &_gSp12ReloadAnimation,
    &_gSp12IdleAnimation,
    &_gSp12IdleAnimation,
    &_gSp12HighHealthIdleAnimation,
    &_gSp12MediumHealthIdleAnimation,
    &_gSp12LowHealthIdleAnimation,
    &_gSp12PeReleaseFamily0Animation,
    &_gSp12PeChargeFamily0Animation,
    &_gSp12PeCastFamily0Animation,
    &_gSp12PeReleaseFamily1Animation,
    &_gSp12PeChargeFamily1Animation,
    &_gSp12PeCastFamily1Animation,
    &_gSp12ScriptedPresentationEnterAnimation,
    &_gSp12ScriptedPresentationLeaveAnimation,
    &_gSp12ScriptedPresentationEnterAnimation,
    &_gSp12ScriptedPresentationLeaveAnimation,
    &_gSp12StairAscentAnimation,
    &_gSp12StairDescentAnimation,
    &_gSp12StairAscentFinishEvenAnimation,
    &_gSp12StairAscentFinishOddAnimation,
    &_gSp12ItemUseAnimation,
    &_gSp12IdleAnimation,
    &_gSp12PeReleaseFamily2Animation,
    &_gSp12PeChargeFamily2Animation,
    &_gSp12PeCastFamily2Animation,
    // Unused base slots and all raw extension words start cleared.
};
#endif

PACKAGE_ALIASES
