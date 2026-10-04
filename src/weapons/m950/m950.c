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

void func_m950_8011D1DC(Task* arg0);

void func_m950_8011D1DC(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = actor->equipmentTasks[1]->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state                                          = 4;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            func_80106238(arg0, 0, 1);
            anim = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 5;
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
        fire:
            actor->statePhase                  = 3;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = 4;
            /* fallthrough */
        case 3:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                Gp_ConsumeSlotQty(0x82, 1);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20030004, 1);
                Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH, coord, 3, NULL);
                playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
            }
            break;
        case 4:
            actor->attackCancelTicks = 9;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            /* fallthrough */
        case 5:
            if ((s8)func_801060E0(arg0) != 0 && func_80106264(1) > 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static TmdBone _gM950Model00548Skeleton[1] = {
#include "assets/m950_model_00548_skeleton.inc"
};

static u32 _gM950Model00548PartVerts[1] = {
#include "assets/m950_model_00548_partVerts.inc"
};

static SVECTOR _gM950Model00548Verts[36] = {
#include "assets/m950_model_00548_verts.inc"
};

static SVECTOR _gM950Model00548Normals[36] = {
#include "assets/m950_model_00548_normals.inc"
};

static u32 _gM950Model00548Stream[229] = {
#include "assets/m950_model_00548_stream.inc"
};

TmdSource D_m950_8011DA9C = {
    0,
    1616,
    0,
    1,
    _gM950Model00548PartVerts,
    _gM950Model00548Verts,
    _gM950Model00548Normals,
    _gM950Model00548Skeleton,
    _gM950Model00548Stream,
};

static AnimationPackedPose _gM950Animation00A90Bank1[2] = {
#include "assets/m950_animation_00A90_bank1.inc"
};

static AnimationPackedRotation _gM950Animation00A90Bank4[8] = {
#include "assets/m950_animation_00A90_bank4.inc"
};

static AnimationRecord _gM950Animation00A90Records[76] = {
#include "assets/m950_animation_00A90_records.inc"
};

static u16 _gM950Animation00A90Indices[20] = {
#include "assets/m950_animation_00A90_indices.inc"
};

static AnimationSet _gM950Animation00A90 = {
    _gM950Animation00A90Records,
    _gM950Animation00A90Indices,
    { NULL, _gM950Animation00A90Bank1, NULL, NULL, _gM950Animation00A90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation01134Bank1[12] = {
#include "assets/m950_animation_01134_bank1.inc"
};

static AnimationPackedRotation _gM950Animation01134Bank4[151] = {
#include "assets/m950_animation_01134_bank4.inc"
};

static AnimationRecord _gM950Animation01134Records[218] = {
#include "assets/m950_animation_01134_records.inc"
};

static u16 _gM950Animation01134Indices[20] = {
#include "assets/m950_animation_01134_indices.inc"
};

static AnimationSet _gM950Animation01134 = {
    _gM950Animation01134Records,
    _gM950Animation01134Indices,
    { NULL, _gM950Animation01134Bank1, NULL, NULL, _gM950Animation01134Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation01A80Bank1[16] = {
#include "assets/m950_animation_01A80_bank1.inc"
};

static AnimationPackedRotation _gM950Animation01A80Bank4[221] = {
#include "assets/m950_animation_01A80_bank4.inc"
};

static AnimationRecord _gM950Animation01A80Records[306] = {
#include "assets/m950_animation_01A80_records.inc"
};

static u16 _gM950Animation01A80Indices[20] = {
#include "assets/m950_animation_01A80_indices.inc"
};

static AnimationSet _gM950Animation01A80 = {
    _gM950Animation01A80Records,
    _gM950Animation01A80Indices,
    { NULL, _gM950Animation01A80Bank1, NULL, NULL, _gM950Animation01A80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation02358Bank1[19] = {
#include "assets/m950_animation_02358_bank1.inc"
};

static AnimationPackedRotation _gM950Animation02358Bank4[193] = {
#include "assets/m950_animation_02358_bank4.inc"
};

static AnimationRecord _gM950Animation02358Records[296] = {
#include "assets/m950_animation_02358_records.inc"
};

static u16 _gM950Animation02358Indices[20] = {
#include "assets/m950_animation_02358_indices.inc"
};

static AnimationSet _gM950Animation02358 = {
    _gM950Animation02358Records,
    _gM950Animation02358Indices,
    { NULL, _gM950Animation02358Bank1, NULL, NULL, _gM950Animation02358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM950Animation02A8CBank1[13] = {
#include "assets/m950_animation_02A8C_bank1.inc"
};

static AnimationPackedRotation _gM950Animation02A8CBank4[167] = {
#include "assets/m950_animation_02A8C_bank4.inc"
};

static AnimationRecord _gM950Animation02A8CRecords[235] = {
#include "assets/m950_animation_02A8C_records.inc"
};

static u16 _gM950Animation02A8CIndices[20] = {
#include "assets/m950_animation_02A8C_indices.inc"
};

static AnimationSet _gM950Animation02A8C = {
    _gM950Animation02A8CRecords,
    _gM950Animation02A8CIndices,
    { NULL, _gM950Animation02A8CBank1, NULL, NULL, _gM950Animation02A8CBank4, NULL, NULL, NULL },
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
    &_gM950Animation00A90,
    &_gM950Animation01A80,
    &_gM950Animation02358,
    &_gM950Animation02A8C,
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
    &_gM950Animation01134,
    &_gM950Animation0D914,
    &_gM950Animation00A90,
    &_gM950Animation00A90,
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
    &_gM950Animation00A90,
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
