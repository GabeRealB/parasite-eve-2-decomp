#include "weapons/m4a1.h"

#include "types.h"

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

static TmdBone _gM4a1Model006ACSkeleton[1] = {
#include "assets/m4a1_model_006AC_skeleton.inc"
};

static u32 _gM4a1Model006ACPartVerts[1] = {
#include "assets/m4a1_model_006AC_partVerts.inc"
};

static SVECTOR _gM4a1Model006ACVerts[58] = {
#include "assets/m4a1_model_006AC_verts.inc"
};

static SVECTOR _gM4a1Model006ACNormals[58] = {
#include "assets/m4a1_model_006AC_normals.inc"
};

static u32 _gM4a1Model006ACStream[406] = {
#include "assets/m4a1_model_006AC_stream.inc"
};

TmdSource D_m4a1_8011DEC4 = {
    0,
    2940,
    0,
    1,
    _gM4a1Model006ACPartVerts,
    _gM4a1Model006ACVerts,
    _gM4a1Model006ACNormals,
    _gM4a1Model006ACSkeleton,
    _gM4a1Model006ACStream,
};

static AnimationPackedPose _gM4a1Animation00EB8Bank1[2] = {
#include "assets/m4a1_animation_00EB8_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation00EB8Bank4[8] = {
#include "assets/m4a1_animation_00EB8_bank4.inc"
};

static AnimationRecord _gM4a1Animation00EB8Records[76] = {
#include "assets/m4a1_animation_00EB8_records.inc"
};

static u16 _gM4a1Animation00EB8Indices[20] = {
#include "assets/m4a1_animation_00EB8_indices.inc"
};

static AnimationSet _gM4a1Animation00EB8 = {
    _gM4a1Animation00EB8Records,
    _gM4a1Animation00EB8Indices,
    { NULL, _gM4a1Animation00EB8Bank1, NULL, NULL, _gM4a1Animation00EB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0155CBank1[12] = {
#include "assets/m4a1_animation_0155C_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0155CBank4[151] = {
#include "assets/m4a1_animation_0155C_bank4.inc"
};

static AnimationRecord _gM4a1Animation0155CRecords[218] = {
#include "assets/m4a1_animation_0155C_records.inc"
};

static u16 _gM4a1Animation0155CIndices[20] = {
#include "assets/m4a1_animation_0155C_indices.inc"
};

static AnimationSet _gM4a1Animation0155C = {
    _gM4a1Animation0155CRecords,
    _gM4a1Animation0155CIndices,
    { NULL, _gM4a1Animation0155CBank1, NULL, NULL, _gM4a1Animation0155CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation01DBCBank1[19] = {
#include "assets/m4a1_animation_01DBC_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation01DBCBank4[169] = {
#include "assets/m4a1_animation_01DBC_bank4.inc"
};

static AnimationRecord _gM4a1Animation01DBCRecords[290] = {
#include "assets/m4a1_animation_01DBC_records.inc"
};

static u16 _gM4a1Animation01DBCIndices[20] = {
#include "assets/m4a1_animation_01DBC_indices.inc"
};

static AnimationSet _gM4a1Animation01DBC = {
    _gM4a1Animation01DBCRecords,
    _gM4a1Animation01DBCIndices,
    { NULL, _gM4a1Animation01DBCBank1, NULL, NULL, _gM4a1Animation01DBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation02620Bank1[19] = {
#include "assets/m4a1_animation_02620_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation02620Bank4[170] = {
#include "assets/m4a1_animation_02620_bank4.inc"
};

static AnimationRecord _gM4a1Animation02620Records[290] = {
#include "assets/m4a1_animation_02620_records.inc"
};

static u16 _gM4a1Animation02620Indices[20] = {
#include "assets/m4a1_animation_02620_indices.inc"
};

static AnimationSet _gM4a1Animation02620 = {
    _gM4a1Animation02620Records,
    _gM4a1Animation02620Indices,
    { NULL, _gM4a1Animation02620Bank1, NULL, NULL, _gM4a1Animation02620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation02934Bank1[3] = {
#include "assets/m4a1_animation_02934_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation02934Bank4[69] = {
#include "assets/m4a1_animation_02934_bank4.inc"
};

static AnimationRecord _gM4a1Animation02934Records[99] = {
#include "assets/m4a1_animation_02934_records.inc"
};

static u16 _gM4a1Animation02934Indices[20] = {
#include "assets/m4a1_animation_02934_indices.inc"
};

static AnimationSet _gM4a1Animation02934 = {
    _gM4a1Animation02934Records,
    _gM4a1Animation02934Indices,
    { NULL, _gM4a1Animation02934Bank1, NULL, NULL, _gM4a1Animation02934Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation03090Bank1[14] = {
#include "assets/m4a1_animation_03090_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation03090Bank4[156] = {
#include "assets/m4a1_animation_03090_bank4.inc"
};

static AnimationRecord _gM4a1Animation03090Records[253] = {
#include "assets/m4a1_animation_03090_records.inc"
};

static u16 _gM4a1Animation03090Indices[20] = {
#include "assets/m4a1_animation_03090_indices.inc"
};

static AnimationSet _gM4a1Animation03090 = {
    _gM4a1Animation03090Records,
    _gM4a1Animation03090Indices,
    { NULL, _gM4a1Animation03090Bank1, NULL, NULL, _gM4a1Animation03090Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation03818Bank1[16] = {
#include "assets/m4a1_animation_03818_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation03818Bank4[167] = {
#include "assets/m4a1_animation_03818_bank4.inc"
};

static AnimationRecord _gM4a1Animation03818Records[247] = {
#include "assets/m4a1_animation_03818_records.inc"
};

static u16 _gM4a1Animation03818Indices[20] = {
#include "assets/m4a1_animation_03818_indices.inc"
};

static AnimationSet _gM4a1Animation03818 = {
    _gM4a1Animation03818Records,
    _gM4a1Animation03818Indices,
    { NULL, _gM4a1Animation03818Bank1, NULL, NULL, _gM4a1Animation03818Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation03AECBank1[6] = {
#include "assets/m4a1_animation_03AEC_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation03AECBank4[52] = {
#include "assets/m4a1_animation_03AEC_bank4.inc"
};

static AnimationRecord _gM4a1Animation03AECRecords[91] = {
#include "assets/m4a1_animation_03AEC_records.inc"
};

static u16 _gM4a1Animation03AECIndices[20] = {
#include "assets/m4a1_animation_03AEC_indices.inc"
};

static AnimationSet _gM4a1Animation03AEC = {
    _gM4a1Animation03AECRecords,
    _gM4a1Animation03AECIndices,
    { NULL, _gM4a1Animation03AECBank1, NULL, NULL, _gM4a1Animation03AECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation03E7CBank1[7] = {
#include "assets/m4a1_animation_03E7C_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation03E7CBank4[73] = {
#include "assets/m4a1_animation_03E7C_bank4.inc"
};

static AnimationRecord _gM4a1Animation03E7CRecords[114] = {
#include "assets/m4a1_animation_03E7C_records.inc"
};

static u16 _gM4a1Animation03E7CIndices[20] = {
#include "assets/m4a1_animation_03E7C_indices.inc"
};

static AnimationSet _gM4a1Animation03E7C = {
    _gM4a1Animation03E7CRecords,
    _gM4a1Animation03E7CIndices,
    { NULL, _gM4a1Animation03E7CBank1, NULL, NULL, _gM4a1Animation03E7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation04304Bank1[9] = {
#include "assets/m4a1_animation_04304_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation04304Bank4[104] = {
#include "assets/m4a1_animation_04304_bank4.inc"
};

static AnimationRecord _gM4a1Animation04304Records[139] = {
#include "assets/m4a1_animation_04304_records.inc"
};

static u16 _gM4a1Animation04304Indices[20] = {
#include "assets/m4a1_animation_04304_indices.inc"
};

static AnimationSet _gM4a1Animation04304 = {
    _gM4a1Animation04304Records,
    _gM4a1Animation04304Indices,
    { NULL, _gM4a1Animation04304Bank1, NULL, NULL, _gM4a1Animation04304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation04500Bank1[3] = {
#include "assets/m4a1_animation_04500_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation04500Bank4[22] = {
#include "assets/m4a1_animation_04500_bank4.inc"
};

static AnimationRecord _gM4a1Animation04500Records[76] = {
#include "assets/m4a1_animation_04500_records.inc"
};

static u16 _gM4a1Animation04500Indices[20] = {
#include "assets/m4a1_animation_04500_indices.inc"
};

static AnimationSet _gM4a1Animation04500 = {
    _gM4a1Animation04500Records,
    _gM4a1Animation04500Indices,
    { NULL, _gM4a1Animation04500Bank1, NULL, NULL, _gM4a1Animation04500Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation047D8Bank1[6] = {
#include "assets/m4a1_animation_047D8_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation047D8Bank4[57] = {
#include "assets/m4a1_animation_047D8_bank4.inc"
};

static AnimationRecord _gM4a1Animation047D8Records[87] = {
#include "assets/m4a1_animation_047D8_records.inc"
};

static u16 _gM4a1Animation047D8Indices[20] = {
#include "assets/m4a1_animation_047D8_indices.inc"
};

static AnimationSet _gM4a1Animation047D8 = {
    _gM4a1Animation047D8Records,
    _gM4a1Animation047D8Indices,
    { NULL, _gM4a1Animation047D8Bank1, NULL, NULL, _gM4a1Animation047D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation04A7CBank1[4] = {
#include "assets/m4a1_animation_04A7C_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation04A7CBank4[55] = {
#include "assets/m4a1_animation_04A7C_bank4.inc"
};

static AnimationRecord _gM4a1Animation04A7CRecords[82] = {
#include "assets/m4a1_animation_04A7C_records.inc"
};

static u16 _gM4a1Animation04A7CIndices[20] = {
#include "assets/m4a1_animation_04A7C_indices.inc"
};

static AnimationSet _gM4a1Animation04A7C = {
    _gM4a1Animation04A7CRecords,
    _gM4a1Animation04A7CIndices,
    { NULL, _gM4a1Animation04A7CBank1, NULL, NULL, _gM4a1Animation04A7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation04C7CBank1[3] = {
#include "assets/m4a1_animation_04C7C_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation04C7CBank4[23] = {
#include "assets/m4a1_animation_04C7C_bank4.inc"
};

static AnimationRecord _gM4a1Animation04C7CRecords[76] = {
#include "assets/m4a1_animation_04C7C_records.inc"
};

static u16 _gM4a1Animation04C7CIndices[20] = {
#include "assets/m4a1_animation_04C7C_indices.inc"
};

static AnimationSet _gM4a1Animation04C7C = {
    _gM4a1Animation04C7CRecords,
    _gM4a1Animation04C7CIndices,
    { NULL, _gM4a1Animation04C7CBank1, NULL, NULL, _gM4a1Animation04C7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation04FD0Bank1[8] = {
#include "assets/m4a1_animation_04FD0_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation04FD0Bank4[68] = {
#include "assets/m4a1_animation_04FD0_bank4.inc"
};

static AnimationRecord _gM4a1Animation04FD0Records[101] = {
#include "assets/m4a1_animation_04FD0_records.inc"
};

static u16 _gM4a1Animation04FD0Indices[20] = {
#include "assets/m4a1_animation_04FD0_indices.inc"
};

static AnimationSet _gM4a1Animation04FD0 = {
    _gM4a1Animation04FD0Records,
    _gM4a1Animation04FD0Indices,
    { NULL, _gM4a1Animation04FD0Bank1, NULL, NULL, _gM4a1Animation04FD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation05284Bank1[5] = {
#include "assets/m4a1_animation_05284_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation05284Bank4[55] = {
#include "assets/m4a1_animation_05284_bank4.inc"
};

static AnimationRecord _gM4a1Animation05284Records[83] = {
#include "assets/m4a1_animation_05284_records.inc"
};

static u16 _gM4a1Animation05284Indices[20] = {
#include "assets/m4a1_animation_05284_indices.inc"
};

static AnimationSet _gM4a1Animation05284 = {
    _gM4a1Animation05284Records,
    _gM4a1Animation05284Indices,
    { NULL, _gM4a1Animation05284Bank1, NULL, NULL, _gM4a1Animation05284Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation055A4Bank1[6] = {
#include "assets/m4a1_animation_055A4_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation055A4Bank4[66] = {
#include "assets/m4a1_animation_055A4_bank4.inc"
};

static AnimationRecord _gM4a1Animation055A4Records[96] = {
#include "assets/m4a1_animation_055A4_records.inc"
};

static u16 _gM4a1Animation055A4Indices[20] = {
#include "assets/m4a1_animation_055A4_indices.inc"
};

static AnimationSet _gM4a1Animation055A4 = {
    _gM4a1Animation055A4Records,
    _gM4a1Animation055A4Indices,
    { NULL, _gM4a1Animation055A4Bank1, NULL, NULL, _gM4a1Animation055A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation05D68Bank1[18] = {
#include "assets/m4a1_animation_05D68_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation05D68Bank4[184] = {
#include "assets/m4a1_animation_05D68_bank4.inc"
};

static AnimationRecord _gM4a1Animation05D68Records[239] = {
#include "assets/m4a1_animation_05D68_records.inc"
};

static u16 _gM4a1Animation05D68Indices[20] = {
#include "assets/m4a1_animation_05D68_indices.inc"
};

static AnimationSet _gM4a1Animation05D68 = {
    _gM4a1Animation05D68Records,
    _gM4a1Animation05D68Indices,
    { NULL, _gM4a1Animation05D68Bank1, NULL, NULL, _gM4a1Animation05D68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation07000Bank1[29] = {
#include "assets/m4a1_animation_07000_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation07000Bank4[450] = {
#include "assets/m4a1_animation_07000_bank4.inc"
};

static AnimationRecord _gM4a1Animation07000Records[633] = {
#include "assets/m4a1_animation_07000_records.inc"
};

static u16 _gM4a1Animation07000Indices[20] = {
#include "assets/m4a1_animation_07000_indices.inc"
};

static AnimationSet _gM4a1Animation07000 = {
    _gM4a1Animation07000Records,
    _gM4a1Animation07000Indices,
    { NULL, _gM4a1Animation07000Bank1, NULL, NULL, _gM4a1Animation07000Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation07B78Bank1[12] = {
#include "assets/m4a1_animation_07B78_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation07B78Bank4[266] = {
#include "assets/m4a1_animation_07B78_bank4.inc"
};

static AnimationRecord _gM4a1Animation07B78Records[412] = {
#include "assets/m4a1_animation_07B78_records.inc"
};

static u16 _gM4a1Animation07B78Indices[20] = {
#include "assets/m4a1_animation_07B78_indices.inc"
};

static AnimationSet _gM4a1Animation07B78 = {
    _gM4a1Animation07B78Records,
    _gM4a1Animation07B78Indices,
    { NULL, _gM4a1Animation07B78Bank1, NULL, NULL, _gM4a1Animation07B78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation08294Bank1[9] = {
#include "assets/m4a1_animation_08294_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation08294Bank4[144] = {
#include "assets/m4a1_animation_08294_bank4.inc"
};

static AnimationRecord _gM4a1Animation08294Records[264] = {
#include "assets/m4a1_animation_08294_records.inc"
};

static u16 _gM4a1Animation08294Indices[20] = {
#include "assets/m4a1_animation_08294_indices.inc"
};

static AnimationSet _gM4a1Animation08294 = {
    _gM4a1Animation08294Records,
    _gM4a1Animation08294Indices,
    { NULL, _gM4a1Animation08294Bank1, NULL, NULL, _gM4a1Animation08294Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation08714Bank1[6] = {
#include "assets/m4a1_animation_08714_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation08714Bank4[107] = {
#include "assets/m4a1_animation_08714_bank4.inc"
};

static AnimationRecord _gM4a1Animation08714Records[143] = {
#include "assets/m4a1_animation_08714_records.inc"
};

static u16 _gM4a1Animation08714Indices[20] = {
#include "assets/m4a1_animation_08714_indices.inc"
};

static AnimationSet _gM4a1Animation08714 = {
    _gM4a1Animation08714Records,
    _gM4a1Animation08714Indices,
    { NULL, _gM4a1Animation08714Bank1, NULL, NULL, _gM4a1Animation08714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation088ECBank1[3] = {
#include "assets/m4a1_animation_088EC_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation088ECBank4[32] = {
#include "assets/m4a1_animation_088EC_bank4.inc"
};

static AnimationRecord _gM4a1Animation088ECRecords[57] = {
#include "assets/m4a1_animation_088EC_records.inc"
};

static u16 _gM4a1Animation088ECIndices[20] = {
#include "assets/m4a1_animation_088EC_indices.inc"
};

static AnimationSet _gM4a1Animation088EC = {
    _gM4a1Animation088ECRecords,
    _gM4a1Animation088ECIndices,
    { NULL, _gM4a1Animation088ECBank1, NULL, NULL, _gM4a1Animation088ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation08E50Bank1[11] = {
#include "assets/m4a1_animation_08E50_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation08E50Bank4[125] = {
#include "assets/m4a1_animation_08E50_bank4.inc"
};

static AnimationRecord _gM4a1Animation08E50Records[167] = {
#include "assets/m4a1_animation_08E50_records.inc"
};

static u16 _gM4a1Animation08E50Indices[20] = {
#include "assets/m4a1_animation_08E50_indices.inc"
};

static AnimationSet _gM4a1Animation08E50 = {
    _gM4a1Animation08E50Records,
    _gM4a1Animation08E50Indices,
    { NULL, _gM4a1Animation08E50Bank1, NULL, NULL, _gM4a1Animation08E50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation09044Bank1[3] = {
#include "assets/m4a1_animation_09044_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation09044Bank4[20] = {
#include "assets/m4a1_animation_09044_bank4.inc"
};

static AnimationRecord _gM4a1Animation09044Records[76] = {
#include "assets/m4a1_animation_09044_records.inc"
};

static u16 _gM4a1Animation09044Indices[20] = {
#include "assets/m4a1_animation_09044_indices.inc"
};

static AnimationSet _gM4a1Animation09044 = {
    _gM4a1Animation09044Records,
    _gM4a1Animation09044Indices,
    { NULL, _gM4a1Animation09044Bank1, NULL, NULL, _gM4a1Animation09044Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation094C4Bank1[8] = {
#include "assets/m4a1_animation_094C4_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation094C4Bank4[105] = {
#include "assets/m4a1_animation_094C4_bank4.inc"
};

static AnimationRecord _gM4a1Animation094C4Records[139] = {
#include "assets/m4a1_animation_094C4_records.inc"
};

static u16 _gM4a1Animation094C4Indices[20] = {
#include "assets/m4a1_animation_094C4_indices.inc"
};

static AnimationSet _gM4a1Animation094C4 = {
    _gM4a1Animation094C4Records,
    _gM4a1Animation094C4Indices,
    { NULL, _gM4a1Animation094C4Bank1, NULL, NULL, _gM4a1Animation094C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation096A0Bank1[2] = {
#include "assets/m4a1_animation_096A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation096A0Bank4[17] = {
#include "assets/m4a1_animation_096A0_bank4.inc"
};

static AnimationRecord _gM4a1Animation096A0Records[76] = {
#include "assets/m4a1_animation_096A0_records.inc"
};

static u16 _gM4a1Animation096A0Indices[20] = {
#include "assets/m4a1_animation_096A0_indices.inc"
};

static AnimationSet _gM4a1Animation096A0 = {
    _gM4a1Animation096A0Records,
    _gM4a1Animation096A0Indices,
    { NULL, _gM4a1Animation096A0Bank1, NULL, NULL, _gM4a1Animation096A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation09E88Bank1[14] = {
#include "assets/m4a1_animation_09E88_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation09E88Bank4[189] = {
#include "assets/m4a1_animation_09E88_bank4.inc"
};

static AnimationRecord _gM4a1Animation09E88Records[255] = {
#include "assets/m4a1_animation_09E88_records.inc"
};

static u16 _gM4a1Animation09E88Indices[20] = {
#include "assets/m4a1_animation_09E88_indices.inc"
};

static AnimationSet _gM4a1Animation09E88 = {
    _gM4a1Animation09E88Records,
    _gM4a1Animation09E88Indices,
    { NULL, _gM4a1Animation09E88Bank1, NULL, NULL, _gM4a1Animation09E88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0A930Bank1[19] = {
#include "assets/m4a1_animation_0A930_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0A930Bank4[269] = {
#include "assets/m4a1_animation_0A930_bank4.inc"
};

static AnimationRecord _gM4a1Animation0A930Records[336] = {
#include "assets/m4a1_animation_0A930_records.inc"
};

static u16 _gM4a1Animation0A930Indices[20] = {
#include "assets/m4a1_animation_0A930_indices.inc"
};

static AnimationSet _gM4a1Animation0A930 = {
    _gM4a1Animation0A930Records,
    _gM4a1Animation0A930Indices,
    { NULL, _gM4a1Animation0A930Bank1, NULL, NULL, _gM4a1Animation0A930Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0B894Bank1[26] = {
#include "assets/m4a1_animation_0B894_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0B894Bank4[416] = {
#include "assets/m4a1_animation_0B894_bank4.inc"
};

static AnimationRecord _gM4a1Animation0B894Records[471] = {
#include "assets/m4a1_animation_0B894_records.inc"
};

static u16 _gM4a1Animation0B894Indices[20] = {
#include "assets/m4a1_animation_0B894_indices.inc"
};

static AnimationSet _gM4a1Animation0B894 = {
    _gM4a1Animation0B894Records,
    _gM4a1Animation0B894Indices,
    { NULL, _gM4a1Animation0B894Bank1, NULL, NULL, _gM4a1Animation0B894Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0BC60Bank1[6] = {
#include "assets/m4a1_animation_0BC60_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0BC60Bank4[80] = {
#include "assets/m4a1_animation_0BC60_bank4.inc"
};

static AnimationRecord _gM4a1Animation0BC60Records[125] = {
#include "assets/m4a1_animation_0BC60_records.inc"
};

static u16 _gM4a1Animation0BC60Indices[20] = {
#include "assets/m4a1_animation_0BC60_indices.inc"
};

static AnimationSet _gM4a1Animation0BC60 = {
    _gM4a1Animation0BC60Records,
    _gM4a1Animation0BC60Indices,
    { NULL, _gM4a1Animation0BC60Bank1, NULL, NULL, _gM4a1Animation0BC60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0C3A0Bank1[13] = {
#include "assets/m4a1_animation_0C3A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0C3A0Bank4[182] = {
#include "assets/m4a1_animation_0C3A0_bank4.inc"
};

static AnimationRecord _gM4a1Animation0C3A0Records[223] = {
#include "assets/m4a1_animation_0C3A0_records.inc"
};

static u16 _gM4a1Animation0C3A0Indices[20] = {
#include "assets/m4a1_animation_0C3A0_indices.inc"
};

static AnimationSet _gM4a1Animation0C3A0 = {
    _gM4a1Animation0C3A0Records,
    _gM4a1Animation0C3A0Indices,
    { NULL, _gM4a1Animation0C3A0Bank1, NULL, NULL, _gM4a1Animation0C3A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0CDD0Bank1[18] = {
#include "assets/m4a1_animation_0CDD0_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0CDD0Bank4[266] = {
#include "assets/m4a1_animation_0CDD0_bank4.inc"
};

static AnimationRecord _gM4a1Animation0CDD0Records[312] = {
#include "assets/m4a1_animation_0CDD0_records.inc"
};

static u16 _gM4a1Animation0CDD0Indices[20] = {
#include "assets/m4a1_animation_0CDD0_indices.inc"
};

static AnimationSet _gM4a1Animation0CDD0 = {
    _gM4a1Animation0CDD0Records,
    _gM4a1Animation0CDD0Indices,
    { NULL, _gM4a1Animation0CDD0Bank1, NULL, NULL, _gM4a1Animation0CDD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0D688Bank1[18] = {
#include "assets/m4a1_animation_0D688_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0D688Bank4[201] = {
#include "assets/m4a1_animation_0D688_bank4.inc"
};

static AnimationRecord _gM4a1Animation0D688Records[283] = {
#include "assets/m4a1_animation_0D688_records.inc"
};

static u16 _gM4a1Animation0D688Indices[20] = {
#include "assets/m4a1_animation_0D688_indices.inc"
};

static AnimationSet _gM4a1Animation0D688 = {
    _gM4a1Animation0D688Records,
    _gM4a1Animation0D688Indices,
    { NULL, _gM4a1Animation0D688Bank1, NULL, NULL, _gM4a1Animation0D688Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0DE28Bank1[16] = {
#include "assets/m4a1_animation_0DE28_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0DE28Bank4[157] = {
#include "assets/m4a1_animation_0DE28_bank4.inc"
};

static AnimationRecord _gM4a1Animation0DE28Records[263] = {
#include "assets/m4a1_animation_0DE28_records.inc"
};

static u16 _gM4a1Animation0DE28Indices[20] = {
#include "assets/m4a1_animation_0DE28_indices.inc"
};

static AnimationSet _gM4a1Animation0DE28 = {
    _gM4a1Animation0DE28Records,
    _gM4a1Animation0DE28Indices,
    { NULL, _gM4a1Animation0DE28Bank1, NULL, NULL, _gM4a1Animation0DE28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1Animation0E7FCBank1[25] = {
#include "assets/m4a1_animation_0E7FC_bank1.inc"
};

static AnimationPackedRotation _gM4a1Animation0E7FCBank4[223] = {
#include "assets/m4a1_animation_0E7FC_bank4.inc"
};

static AnimationRecord _gM4a1Animation0E7FCRecords[311] = {
#include "assets/m4a1_animation_0E7FC_records.inc"
};

static u16 _gM4a1Animation0E7FCIndices[20] = {
#include "assets/m4a1_animation_0E7FC_indices.inc"
};

static AnimationSet _gM4a1Animation0E7FC = {
    _gM4a1Animation0E7FCRecords,
    _gM4a1Animation0E7FCIndices,
    { NULL, _gM4a1Animation0E7FCBank1, NULL, NULL, _gM4a1Animation0E7FCBank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_8012B9E4 = { { {
    NULL,
    &_gM4a1Animation00EB8,
    &_gM4a1Animation0D688,
    &_gM4a1Animation0DE28,
    &_gM4a1Animation0E7FC,
    &_gM4a1Animation01DBC,
    &_gM4a1Animation02620,
    &_gM4a1Animation0C3A0,
    &_gM4a1Animation0CDD0,
    &_gM4a1Animation096A0,
    &_gM4a1Animation0BC60,
    &_gM4a1Animation0BC60,
    &_gM4a1Animation0A930,
    &_gM4a1Animation09E88,
    &_gM4a1Animation0B894,
    &_gM4a1Animation0B894,
    &_gM4a1Animation05284,
    &_gM4a1Animation055A4,
    &_gM4a1Animation05D68,
    &_gM4a1Animation0155C,
    &_gM4a1Animation0B894,
    &_gM4a1Animation00EB8,
    &_gM4a1Animation00EB8,
    &_gM4a1Animation07000,
    &_gM4a1Animation08294,
    &_gM4a1Animation07B78,
    &_gM4a1Animation04304,
    &_gM4a1Animation04500,
    &_gM4a1Animation047D8,
    &_gM4a1Animation04A7C,
    &_gM4a1Animation04C7C,
    &_gM4a1Animation04FD0,
    &_gM4a1Animation08714,
    &_gM4a1Animation088EC,
    &_gM4a1Animation08714,
    &_gM4a1Animation088EC,
    &_gM4a1Animation03090,
    &_gM4a1Animation03818,
    &_gM4a1Animation03E7C,
    &_gM4a1Animation03AEC,
    &_gM4a1Animation02934,
    &_gM4a1Animation00EB8,
    &_gM4a1Animation08E50,
    &_gM4a1Animation09044,
    &_gM4a1Animation094C4,
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

PACKAGE_ALIASES
