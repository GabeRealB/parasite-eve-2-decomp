#include "weapons/as12.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

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
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

enum { AS12_AMMUNITION_BUCKSHOT  = 13,
       AS12_FIREFLY_IMPACT_SOUND = SOUND_WEAPON(15, 4) };

/// Places the Firefly impact sound at the first weapon contact point.
///
/// Requires a live first contact in the root's composition frame, writable
/// uninitialized scratch node and the current loaded ammunition selector.
/// Writes only the cached translation, then plays the sound without retaining
/// the node. Does not consume the contact or initialize unrelated node fields.
static inline void _as12PlayFireflyImpact(GameActor* actor, GfxCoord* impactCoord)
{
    impactCoord->workm.t[0] = actor->weaponContacts[0].point.vx;
    impactCoord->workm.t[1] = actor->weaponContacts[0].point.vy;
    impactCoord->workm.t[2] = actor->weaponContacts[0].point.vz;
    worldCoordPlaySound(impactCoord,
                        ((gPlayerStatus.weaponSlotItem - AS12_AMMUNITION_BUCKSHOT) << 0x18) | AS12_FIREFLY_IMPACT_SOUND, 1);
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
                        _as12PlayFireflyImpact(actor, impactCoord);
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

static TmdBone _gAs12Model00660Skeleton[1] = {
#include "assets/as12_model_00660_skeleton.inc"
};

static u32 _gAs12Model00660PartVerts[1] = {
#include "assets/as12_model_00660_partVerts.inc"
};

static SVECTOR _gAs12Model00660Verts[46] = {
#include "assets/as12_model_00660_verts.inc"
};

static SVECTOR _gAs12Model00660Normals[38] = {
#include "assets/as12_model_00660_normals.inc"
};

static u32 _gAs12Model00660Stream[308] = {
#include "assets/as12_model_00660_stream.inc"
};

TmdSource D_as12_8011DCF0 = {
    0,
    2212,
    0,
    1,
    _gAs12Model00660PartVerts,
    _gAs12Model00660Verts,
    _gAs12Model00660Normals,
    _gAs12Model00660Skeleton,
    _gAs12Model00660Stream,
};

static AnimationPackedPose _gAs12Animation00CE4Bank1[2] = {
#include "assets/as12_animation_00CE4_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation00CE4Bank4[8] = {
#include "assets/as12_animation_00CE4_bank4.inc"
};

static AnimationRecord _gAs12Animation00CE4Records[76] = {
#include "assets/as12_animation_00CE4_records.inc"
};

static u16 _gAs12Animation00CE4Indices[20] = {
#include "assets/as12_animation_00CE4_indices.inc"
};

static AnimationSet _gAs12Animation00CE4 = {
    _gAs12Animation00CE4Records,
    _gAs12Animation00CE4Indices,
    { NULL, _gAs12Animation00CE4Bank1, NULL, NULL, _gAs12Animation00CE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation01388Bank1[12] = {
#include "assets/as12_animation_01388_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation01388Bank4[151] = {
#include "assets/as12_animation_01388_bank4.inc"
};

static AnimationRecord _gAs12Animation01388Records[218] = {
#include "assets/as12_animation_01388_records.inc"
};

static u16 _gAs12Animation01388Indices[20] = {
#include "assets/as12_animation_01388_indices.inc"
};

static AnimationSet _gAs12Animation01388 = {
    _gAs12Animation01388Records,
    _gAs12Animation01388Indices,
    { NULL, _gAs12Animation01388Bank1, NULL, NULL, _gAs12Animation01388Bank4, NULL, NULL, NULL },
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

static AnimationPackedPose _gAs12Animation024C0Bank1[19] = {
#include "assets/as12_animation_024C0_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation024C0Bank4[169] = {
#include "assets/as12_animation_024C0_bank4.inc"
};

static AnimationRecord _gAs12Animation024C0Records[290] = {
#include "assets/as12_animation_024C0_records.inc"
};

static u16 _gAs12Animation024C0Indices[20] = {
#include "assets/as12_animation_024C0_indices.inc"
};

static AnimationSet _gAs12Animation024C0 = {
    _gAs12Animation024C0Records,
    _gAs12Animation024C0Indices,
    { NULL, _gAs12Animation024C0Bank1, NULL, NULL, _gAs12Animation024C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation02D24Bank1[19] = {
#include "assets/as12_animation_02D24_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation02D24Bank4[170] = {
#include "assets/as12_animation_02D24_bank4.inc"
};

static AnimationRecord _gAs12Animation02D24Records[290] = {
#include "assets/as12_animation_02D24_records.inc"
};

static u16 _gAs12Animation02D24Indices[20] = {
#include "assets/as12_animation_02D24_indices.inc"
};

static AnimationSet _gAs12Animation02D24 = {
    _gAs12Animation02D24Records,
    _gAs12Animation02D24Indices,
    { NULL, _gAs12Animation02D24Bank1, NULL, NULL, _gAs12Animation02D24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation03038Bank1[3] = {
#include "assets/as12_animation_03038_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation03038Bank4[69] = {
#include "assets/as12_animation_03038_bank4.inc"
};

static AnimationRecord _gAs12Animation03038Records[99] = {
#include "assets/as12_animation_03038_records.inc"
};

static u16 _gAs12Animation03038Indices[20] = {
#include "assets/as12_animation_03038_indices.inc"
};

static AnimationSet _gAs12Animation03038 = {
    _gAs12Animation03038Records,
    _gAs12Animation03038Indices,
    { NULL, _gAs12Animation03038Bank1, NULL, NULL, _gAs12Animation03038Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation03794Bank1[14] = {
#include "assets/as12_animation_03794_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation03794Bank4[156] = {
#include "assets/as12_animation_03794_bank4.inc"
};

static AnimationRecord _gAs12Animation03794Records[253] = {
#include "assets/as12_animation_03794_records.inc"
};

static u16 _gAs12Animation03794Indices[20] = {
#include "assets/as12_animation_03794_indices.inc"
};

static AnimationSet _gAs12Animation03794 = {
    _gAs12Animation03794Records,
    _gAs12Animation03794Indices,
    { NULL, _gAs12Animation03794Bank1, NULL, NULL, _gAs12Animation03794Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation03F1CBank1[16] = {
#include "assets/as12_animation_03F1C_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation03F1CBank4[167] = {
#include "assets/as12_animation_03F1C_bank4.inc"
};

static AnimationRecord _gAs12Animation03F1CRecords[247] = {
#include "assets/as12_animation_03F1C_records.inc"
};

static u16 _gAs12Animation03F1CIndices[20] = {
#include "assets/as12_animation_03F1C_indices.inc"
};

static AnimationSet _gAs12Animation03F1C = {
    _gAs12Animation03F1CRecords,
    _gAs12Animation03F1CIndices,
    { NULL, _gAs12Animation03F1CBank1, NULL, NULL, _gAs12Animation03F1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation041F0Bank1[6] = {
#include "assets/as12_animation_041F0_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation041F0Bank4[52] = {
#include "assets/as12_animation_041F0_bank4.inc"
};

static AnimationRecord _gAs12Animation041F0Records[91] = {
#include "assets/as12_animation_041F0_records.inc"
};

static u16 _gAs12Animation041F0Indices[20] = {
#include "assets/as12_animation_041F0_indices.inc"
};

static AnimationSet _gAs12Animation041F0 = {
    _gAs12Animation041F0Records,
    _gAs12Animation041F0Indices,
    { NULL, _gAs12Animation041F0Bank1, NULL, NULL, _gAs12Animation041F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation04580Bank1[7] = {
#include "assets/as12_animation_04580_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation04580Bank4[73] = {
#include "assets/as12_animation_04580_bank4.inc"
};

static AnimationRecord _gAs12Animation04580Records[114] = {
#include "assets/as12_animation_04580_records.inc"
};

static u16 _gAs12Animation04580Indices[20] = {
#include "assets/as12_animation_04580_indices.inc"
};

static AnimationSet _gAs12Animation04580 = {
    _gAs12Animation04580Records,
    _gAs12Animation04580Indices,
    { NULL, _gAs12Animation04580Bank1, NULL, NULL, _gAs12Animation04580Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation04A08Bank1[9] = {
#include "assets/as12_animation_04A08_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation04A08Bank4[104] = {
#include "assets/as12_animation_04A08_bank4.inc"
};

static AnimationRecord _gAs12Animation04A08Records[139] = {
#include "assets/as12_animation_04A08_records.inc"
};

static u16 _gAs12Animation04A08Indices[20] = {
#include "assets/as12_animation_04A08_indices.inc"
};

static AnimationSet _gAs12Animation04A08 = {
    _gAs12Animation04A08Records,
    _gAs12Animation04A08Indices,
    { NULL, _gAs12Animation04A08Bank1, NULL, NULL, _gAs12Animation04A08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAs12Animation04C04Bank1[3] = {
#include "assets/as12_animation_04C04_bank1.inc"
};

static AnimationPackedRotation _gAs12Animation04C04Bank4[22] = {
#include "assets/as12_animation_04C04_bank4.inc"
};

static AnimationRecord _gAs12Animation04C04Records[76] = {
#include "assets/as12_animation_04C04_records.inc"
};

static u16 _gAs12Animation04C04Indices[20] = {
#include "assets/as12_animation_04C04_indices.inc"
};

static AnimationSet _gAs12Animation04C04 = {
    _gAs12Animation04C04Records,
    _gAs12Animation04C04Indices,
    { NULL, _gAs12Animation04C04Bank1, NULL, NULL, _gAs12Animation04C04Bank4, NULL, NULL, NULL },
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
    &_gAs12Animation00CE4,
    &_gAs12Animation0DFA4,
    &_gAs12Animation0E744,
    &_gAs12Animation0F118,
    &_gAs12Animation024C0,
    &_gAs12Animation02D24,
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
    &_gAs12Animation01388,
    &_gAs12Animation0C468,
    &_gAs12Animation00CE4,
    &_gAs12Animation00CE4,
    &_gAs12Animation07704,
    &_gAs12Animation08998,
    &_gAs12Animation0827C,
    &_gAs12Animation04A08,
    &_gAs12Animation04C04,
    &_gAs12Animation04EDC,
    &_gAs12Animation05180,
    &_gAs12Animation05380,
    &_gAs12Animation056D4,
    &_gAs12Animation08E18,
    &_gAs12Animation08FF0,
    &_gAs12Animation08E18,
    &_gAs12Animation08FF0,
    &_gAs12Animation03794,
    &_gAs12Animation03F1C,
    &_gAs12Animation04580,
    &_gAs12Animation041F0,
    &_gAs12Animation03038,
    &_gAs12Animation00CE4,
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
