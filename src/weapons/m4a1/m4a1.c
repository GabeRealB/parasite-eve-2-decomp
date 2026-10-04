#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
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

void func_m4a1_8011D1C4(Task* arg0);

void func_m4a1_8011D1C4(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;
    s32        delay;
    /* Narrower than the field it feeds on purpose: an `s32 shots = 1` would join
       the switch's SImode `1` in the same cse class and steal its register for
       the `field_97F == 1` compare below. */
    s16 shots;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = 1;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = 9;
            shots                    = 1;
            if (actor->attackButton == 1) {
                shots = 3;
            }
            actor->actionValue = shots;
            func_80106238(arg0, 0, actor->attackButton == 1);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            anim                                                  = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 6;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = 3;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                }
                actor->stateTimer = delay - 1;
                if (delay - 1 == 2) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                        Gp_PlayObjSfx(spot, 0x17, 1);
                    }
                }
            } else {
                /* The lock-on block is spelled out in both arms, not shared: with
                   one copy after the `if`, cross-jumping merges the `field_12A`
                   load into the tail and drops two instructions. */
                actor->statePhase                                     = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                    Gp_PlayObjSfx(spot, 0x17, 1);
                }
            }
            break;
        case 3:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
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
