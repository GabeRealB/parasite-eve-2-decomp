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

/// The P08, the P08 with the snail magazine and the Mongoose are this source
/// built once each, and each declares these values in the manifest.
///
/// `WEAPON_ID` is the weapon's index (4, 1 and 9), which keys the firing sound
/// and the item. `P08_FLASH_EFFECT` is the muzzle-flash effect spawned per shot
/// and `P08_FLASH_WEAPON` the weapon index it is handed - 1 for both P08s, the
/// Mongoose its own. `P08_FIELD_940` is what `field_940` is set to when the
/// firing pose ends.
#if !defined(WEAPON_ID) || !defined(P08_FLASH_EFFECT) || !defined(P08_FLASH_WEAPON) || !defined(P08_FIELD_940)
#error "WEAPON_ID, P08_FLASH_EFFECT, P08_FLASH_WEAPON and P08_FIELD_940 are per-package build parameters"
#endif

void func_p08_8011D1D8(Task* arg0);

void func_p08_8011D1D8(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->statePhase        = 1;
            anim                     = 1;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->attackCancelTicks = 0xB;
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
            actor->statePhase++;
            func_80106238(arg0, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
            Gp_SpawnEff(P08_FLASH_EFFECT,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        P08_FLASH_WEAPON, NULL);
            Gp_AnimResetChildSlots(arg0, 0xA);
            break;
        case 3:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            /* fallthrough */
        case 4:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = P08_FIELD_940;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

/* Each package carries its own model. */
#if WEAPON_ID == 0x4
static TmdBone _gP08Model00440Skeleton[1] = {
#include "assets/p08_model_00440_skeleton.inc"
};

static u32 _gP08Model00440PartVerts[1] = {
#include "assets/p08_model_00440_partVerts.inc"
};

static SVECTOR _gP08Model00440Verts[28] = {
#include "assets/p08_model_00440_verts.inc"
};

static SVECTOR _gP08Model00440Normals[26] = {
#include "assets/p08_model_00440_normals.inc"
};

static u32 _gP08Model00440Stream[201] = {
#include "assets/p08_model_00440_stream.inc"
};

TmdSource D_p08_8011D924 = {
    0,
    1408,
    0,
    1,
    _gP08Model00440PartVerts,
    _gP08Model00440Verts,
    _gP08Model00440Normals,
    _gP08Model00440Skeleton,
    _gP08Model00440Stream,
};
#elif WEAPON_ID == 0x1
static TmdBone _gP08SnailP08Model00440Skeleton[1] = {
#include "assets/p08_model_00440_skeleton.inc"
};

static u32 _gP08SnailP08Model00440PartVerts[1] = {
#include "assets/p08_model_00440_partVerts.inc"
};

static SVECTOR _gP08SnailP08Model00440Verts[28] = {
#include "assets/p08_model_00440_verts.inc"
};

static SVECTOR _gP08SnailP08Model00440Normals[26] = {
#include "assets/p08_model_00440_normals.inc"
};

static u32 _gP08SnailP08Model00440Stream[201] = {
#include "assets/p08_model_00440_stream.inc"
};

TmdSource D_p08_snail_8011D924 = {
    0,
    1408,
    0,
    1,
    _gP08SnailP08Model00440PartVerts,
    _gP08SnailP08Model00440Verts,
    _gP08SnailP08Model00440Normals,
    _gP08SnailP08Model00440Skeleton,
    _gP08SnailP08Model00440Stream,
};
#elif WEAPON_ID == 0x9
static TmdBone _gMongooseModel00450Skeleton[1] = {
#include "assets/mongoose_model_00450_skeleton.inc"
};

static u32 _gMongooseModel00450PartVerts[1] = {
#include "assets/mongoose_model_00450_partVerts.inc"
};

static SVECTOR _gMongooseModel00450Verts[28] = {
#include "assets/mongoose_model_00450_verts.inc"
};

static SVECTOR _gMongooseModel00450Normals[28] = {
#include "assets/mongoose_model_00450_normals.inc"
};

static u32 _gMongooseModel00450Stream[201] = {
#include "assets/mongoose_model_00450_stream.inc"
};

TmdSource D_mongoose_8011D934 = {
    0,
    1408,
    0,
    1,
    _gMongooseModel00450PartVerts,
    _gMongooseModel00450Verts,
    _gMongooseModel00450Normals,
    _gMongooseModel00450Skeleton,
    _gMongooseModel00450Stream,
};
#endif

/* Each package carries its own animation bank. */
#if WEAPON_ID == 0x4 || WEAPON_ID == 0x1
static AnimationPackedPose _gP08Animation00918Bank1[2] = {
#include "assets/p08_animation_00918_bank1.inc"
};

static AnimationPackedRotation _gP08Animation00918Bank4[8] = {
#include "assets/p08_animation_00918_bank4.inc"
};

static AnimationRecord _gP08Animation00918Records[76] = {
#include "assets/p08_animation_00918_records.inc"
};

static u16 _gP08Animation00918Indices[20] = {
#include "assets/p08_animation_00918_indices.inc"
};

static AnimationSet _gP08Animation00918 = {
    _gP08Animation00918Records,
    _gP08Animation00918Indices,
    { NULL, _gP08Animation00918Bank1, NULL, NULL, _gP08Animation00918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation00FBCBank1[12] = {
#include "assets/p08_animation_00FBC_bank1.inc"
};

static AnimationPackedRotation _gP08Animation00FBCBank4[151] = {
#include "assets/p08_animation_00FBC_bank4.inc"
};

static AnimationRecord _gP08Animation00FBCRecords[218] = {
#include "assets/p08_animation_00FBC_records.inc"
};

static u16 _gP08Animation00FBCIndices[20] = {
#include "assets/p08_animation_00FBC_indices.inc"
};

static AnimationSet _gP08Animation00FBC = {
    _gP08Animation00FBCRecords,
    _gP08Animation00FBCIndices,
    { NULL, _gP08Animation00FBCBank1, NULL, NULL, _gP08Animation00FBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation01908Bank1[16] = {
#include "assets/p08_animation_01908_bank1.inc"
};

static AnimationPackedRotation _gP08Animation01908Bank4[221] = {
#include "assets/p08_animation_01908_bank4.inc"
};

static AnimationRecord _gP08Animation01908Records[306] = {
#include "assets/p08_animation_01908_records.inc"
};

static u16 _gP08Animation01908Indices[20] = {
#include "assets/p08_animation_01908_indices.inc"
};

static AnimationSet _gP08Animation01908 = {
    _gP08Animation01908Records,
    _gP08Animation01908Indices,
    { NULL, _gP08Animation01908Bank1, NULL, NULL, _gP08Animation01908Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation021E0Bank1[19] = {
#include "assets/p08_animation_021E0_bank1.inc"
};

static AnimationPackedRotation _gP08Animation021E0Bank4[193] = {
#include "assets/p08_animation_021E0_bank4.inc"
};

static AnimationRecord _gP08Animation021E0Records[296] = {
#include "assets/p08_animation_021E0_records.inc"
};

static u16 _gP08Animation021E0Indices[20] = {
#include "assets/p08_animation_021E0_indices.inc"
};

static AnimationSet _gP08Animation021E0 = {
    _gP08Animation021E0Records,
    _gP08Animation021E0Indices,
    { NULL, _gP08Animation021E0Bank1, NULL, NULL, _gP08Animation021E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation02914Bank1[13] = {
#include "assets/p08_animation_02914_bank1.inc"
};

static AnimationPackedRotation _gP08Animation02914Bank4[167] = {
#include "assets/p08_animation_02914_bank4.inc"
};

static AnimationRecord _gP08Animation02914Records[235] = {
#include "assets/p08_animation_02914_records.inc"
};

static u16 _gP08Animation02914Indices[20] = {
#include "assets/p08_animation_02914_indices.inc"
};

static AnimationSet _gP08Animation02914 = {
    _gP08Animation02914Records,
    _gP08Animation02914Indices,
    { NULL, _gP08Animation02914Bank1, NULL, NULL, _gP08Animation02914Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation03174Bank1[19] = {
#include "assets/p08_animation_03174_bank1.inc"
};

static AnimationPackedRotation _gP08Animation03174Bank4[169] = {
#include "assets/p08_animation_03174_bank4.inc"
};

static AnimationRecord _gP08Animation03174Records[290] = {
#include "assets/p08_animation_03174_records.inc"
};

static u16 _gP08Animation03174Indices[20] = {
#include "assets/p08_animation_03174_indices.inc"
};

static AnimationSet _gP08Animation03174 = {
    _gP08Animation03174Records,
    _gP08Animation03174Indices,
    { NULL, _gP08Animation03174Bank1, NULL, NULL, _gP08Animation03174Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation039D8Bank1[19] = {
#include "assets/p08_animation_039D8_bank1.inc"
};

static AnimationPackedRotation _gP08Animation039D8Bank4[170] = {
#include "assets/p08_animation_039D8_bank4.inc"
};

static AnimationRecord _gP08Animation039D8Records[290] = {
#include "assets/p08_animation_039D8_records.inc"
};

static u16 _gP08Animation039D8Indices[20] = {
#include "assets/p08_animation_039D8_indices.inc"
};

static AnimationSet _gP08Animation039D8 = {
    _gP08Animation039D8Records,
    _gP08Animation039D8Indices,
    { NULL, _gP08Animation039D8Bank1, NULL, NULL, _gP08Animation039D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation03CECBank1[3] = {
#include "assets/p08_animation_03CEC_bank1.inc"
};

static AnimationPackedRotation _gP08Animation03CECBank4[69] = {
#include "assets/p08_animation_03CEC_bank4.inc"
};

static AnimationRecord _gP08Animation03CECRecords[99] = {
#include "assets/p08_animation_03CEC_records.inc"
};

static u16 _gP08Animation03CECIndices[20] = {
#include "assets/p08_animation_03CEC_indices.inc"
};

static AnimationSet _gP08Animation03CEC = {
    _gP08Animation03CECRecords,
    _gP08Animation03CECIndices,
    { NULL, _gP08Animation03CECBank1, NULL, NULL, _gP08Animation03CECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation04448Bank1[14] = {
#include "assets/p08_animation_04448_bank1.inc"
};

static AnimationPackedRotation _gP08Animation04448Bank4[156] = {
#include "assets/p08_animation_04448_bank4.inc"
};

static AnimationRecord _gP08Animation04448Records[253] = {
#include "assets/p08_animation_04448_records.inc"
};

static u16 _gP08Animation04448Indices[20] = {
#include "assets/p08_animation_04448_indices.inc"
};

static AnimationSet _gP08Animation04448 = {
    _gP08Animation04448Records,
    _gP08Animation04448Indices,
    { NULL, _gP08Animation04448Bank1, NULL, NULL, _gP08Animation04448Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation04BD0Bank1[16] = {
#include "assets/p08_animation_04BD0_bank1.inc"
};

static AnimationPackedRotation _gP08Animation04BD0Bank4[167] = {
#include "assets/p08_animation_04BD0_bank4.inc"
};

static AnimationRecord _gP08Animation04BD0Records[247] = {
#include "assets/p08_animation_04BD0_records.inc"
};

static u16 _gP08Animation04BD0Indices[20] = {
#include "assets/p08_animation_04BD0_indices.inc"
};

static AnimationSet _gP08Animation04BD0 = {
    _gP08Animation04BD0Records,
    _gP08Animation04BD0Indices,
    { NULL, _gP08Animation04BD0Bank1, NULL, NULL, _gP08Animation04BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation04EA4Bank1[6] = {
#include "assets/p08_animation_04EA4_bank1.inc"
};

static AnimationPackedRotation _gP08Animation04EA4Bank4[52] = {
#include "assets/p08_animation_04EA4_bank4.inc"
};

static AnimationRecord _gP08Animation04EA4Records[91] = {
#include "assets/p08_animation_04EA4_records.inc"
};

static u16 _gP08Animation04EA4Indices[20] = {
#include "assets/p08_animation_04EA4_indices.inc"
};

static AnimationSet _gP08Animation04EA4 = {
    _gP08Animation04EA4Records,
    _gP08Animation04EA4Indices,
    { NULL, _gP08Animation04EA4Bank1, NULL, NULL, _gP08Animation04EA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation05234Bank1[7] = {
#include "assets/p08_animation_05234_bank1.inc"
};

static AnimationPackedRotation _gP08Animation05234Bank4[73] = {
#include "assets/p08_animation_05234_bank4.inc"
};

static AnimationRecord _gP08Animation05234Records[114] = {
#include "assets/p08_animation_05234_records.inc"
};

static u16 _gP08Animation05234Indices[20] = {
#include "assets/p08_animation_05234_indices.inc"
};

static AnimationSet _gP08Animation05234 = {
    _gP08Animation05234Records,
    _gP08Animation05234Indices,
    { NULL, _gP08Animation05234Bank1, NULL, NULL, _gP08Animation05234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation056BCBank1[9] = {
#include "assets/p08_animation_056BC_bank1.inc"
};

static AnimationPackedRotation _gP08Animation056BCBank4[104] = {
#include "assets/p08_animation_056BC_bank4.inc"
};

static AnimationRecord _gP08Animation056BCRecords[139] = {
#include "assets/p08_animation_056BC_records.inc"
};

static u16 _gP08Animation056BCIndices[20] = {
#include "assets/p08_animation_056BC_indices.inc"
};

static AnimationSet _gP08Animation056BC = {
    _gP08Animation056BCRecords,
    _gP08Animation056BCIndices,
    { NULL, _gP08Animation056BCBank1, NULL, NULL, _gP08Animation056BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation058B8Bank1[3] = {
#include "assets/p08_animation_058B8_bank1.inc"
};

static AnimationPackedRotation _gP08Animation058B8Bank4[22] = {
#include "assets/p08_animation_058B8_bank4.inc"
};

static AnimationRecord _gP08Animation058B8Records[76] = {
#include "assets/p08_animation_058B8_records.inc"
};

static u16 _gP08Animation058B8Indices[20] = {
#include "assets/p08_animation_058B8_indices.inc"
};

static AnimationSet _gP08Animation058B8 = {
    _gP08Animation058B8Records,
    _gP08Animation058B8Indices,
    { NULL, _gP08Animation058B8Bank1, NULL, NULL, _gP08Animation058B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation05B90Bank1[6] = {
#include "assets/p08_animation_05B90_bank1.inc"
};

static AnimationPackedRotation _gP08Animation05B90Bank4[57] = {
#include "assets/p08_animation_05B90_bank4.inc"
};

static AnimationRecord _gP08Animation05B90Records[87] = {
#include "assets/p08_animation_05B90_records.inc"
};

static u16 _gP08Animation05B90Indices[20] = {
#include "assets/p08_animation_05B90_indices.inc"
};

static AnimationSet _gP08Animation05B90 = {
    _gP08Animation05B90Records,
    _gP08Animation05B90Indices,
    { NULL, _gP08Animation05B90Bank1, NULL, NULL, _gP08Animation05B90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation05E34Bank1[4] = {
#include "assets/p08_animation_05E34_bank1.inc"
};

static AnimationPackedRotation _gP08Animation05E34Bank4[55] = {
#include "assets/p08_animation_05E34_bank4.inc"
};

static AnimationRecord _gP08Animation05E34Records[82] = {
#include "assets/p08_animation_05E34_records.inc"
};

static u16 _gP08Animation05E34Indices[20] = {
#include "assets/p08_animation_05E34_indices.inc"
};

static AnimationSet _gP08Animation05E34 = {
    _gP08Animation05E34Records,
    _gP08Animation05E34Indices,
    { NULL, _gP08Animation05E34Bank1, NULL, NULL, _gP08Animation05E34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation06034Bank1[3] = {
#include "assets/p08_animation_06034_bank1.inc"
};

static AnimationPackedRotation _gP08Animation06034Bank4[23] = {
#include "assets/p08_animation_06034_bank4.inc"
};

static AnimationRecord _gP08Animation06034Records[76] = {
#include "assets/p08_animation_06034_records.inc"
};

static u16 _gP08Animation06034Indices[20] = {
#include "assets/p08_animation_06034_indices.inc"
};

static AnimationSet _gP08Animation06034 = {
    _gP08Animation06034Records,
    _gP08Animation06034Indices,
    { NULL, _gP08Animation06034Bank1, NULL, NULL, _gP08Animation06034Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation06388Bank1[8] = {
#include "assets/p08_animation_06388_bank1.inc"
};

static AnimationPackedRotation _gP08Animation06388Bank4[68] = {
#include "assets/p08_animation_06388_bank4.inc"
};

static AnimationRecord _gP08Animation06388Records[101] = {
#include "assets/p08_animation_06388_records.inc"
};

static u16 _gP08Animation06388Indices[20] = {
#include "assets/p08_animation_06388_indices.inc"
};

static AnimationSet _gP08Animation06388 = {
    _gP08Animation06388Records,
    _gP08Animation06388Indices,
    { NULL, _gP08Animation06388Bank1, NULL, NULL, _gP08Animation06388Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0663CBank1[5] = {
#include "assets/p08_animation_0663C_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0663CBank4[55] = {
#include "assets/p08_animation_0663C_bank4.inc"
};

static AnimationRecord _gP08Animation0663CRecords[83] = {
#include "assets/p08_animation_0663C_records.inc"
};

static u16 _gP08Animation0663CIndices[20] = {
#include "assets/p08_animation_0663C_indices.inc"
};

static AnimationSet _gP08Animation0663C = {
    _gP08Animation0663CRecords,
    _gP08Animation0663CIndices,
    { NULL, _gP08Animation0663CBank1, NULL, NULL, _gP08Animation0663CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0695CBank1[6] = {
#include "assets/p08_animation_0695C_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0695CBank4[66] = {
#include "assets/p08_animation_0695C_bank4.inc"
};

static AnimationRecord _gP08Animation0695CRecords[96] = {
#include "assets/p08_animation_0695C_records.inc"
};

static u16 _gP08Animation0695CIndices[20] = {
#include "assets/p08_animation_0695C_indices.inc"
};

static AnimationSet _gP08Animation0695C = {
    _gP08Animation0695CRecords,
    _gP08Animation0695CIndices,
    { NULL, _gP08Animation0695CBank1, NULL, NULL, _gP08Animation0695CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation07120Bank1[18] = {
#include "assets/p08_animation_07120_bank1.inc"
};

static AnimationPackedRotation _gP08Animation07120Bank4[184] = {
#include "assets/p08_animation_07120_bank4.inc"
};

static AnimationRecord _gP08Animation07120Records[239] = {
#include "assets/p08_animation_07120_records.inc"
};

static u16 _gP08Animation07120Indices[20] = {
#include "assets/p08_animation_07120_indices.inc"
};

static AnimationSet _gP08Animation07120 = {
    _gP08Animation07120Records,
    _gP08Animation07120Indices,
    { NULL, _gP08Animation07120Bank1, NULL, NULL, _gP08Animation07120Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation083B8Bank1[29] = {
#include "assets/p08_animation_083B8_bank1.inc"
};

static AnimationPackedRotation _gP08Animation083B8Bank4[450] = {
#include "assets/p08_animation_083B8_bank4.inc"
};

static AnimationRecord _gP08Animation083B8Records[633] = {
#include "assets/p08_animation_083B8_records.inc"
};

static u16 _gP08Animation083B8Indices[20] = {
#include "assets/p08_animation_083B8_indices.inc"
};

static AnimationSet _gP08Animation083B8 = {
    _gP08Animation083B8Records,
    _gP08Animation083B8Indices,
    { NULL, _gP08Animation083B8Bank1, NULL, NULL, _gP08Animation083B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation08F30Bank1[12] = {
#include "assets/p08_animation_08F30_bank1.inc"
};

static AnimationPackedRotation _gP08Animation08F30Bank4[266] = {
#include "assets/p08_animation_08F30_bank4.inc"
};

static AnimationRecord _gP08Animation08F30Records[412] = {
#include "assets/p08_animation_08F30_records.inc"
};

static u16 _gP08Animation08F30Indices[20] = {
#include "assets/p08_animation_08F30_indices.inc"
};

static AnimationSet _gP08Animation08F30 = {
    _gP08Animation08F30Records,
    _gP08Animation08F30Indices,
    { NULL, _gP08Animation08F30Bank1, NULL, NULL, _gP08Animation08F30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0964CBank1[9] = {
#include "assets/p08_animation_0964C_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0964CBank4[144] = {
#include "assets/p08_animation_0964C_bank4.inc"
};

static AnimationRecord _gP08Animation0964CRecords[264] = {
#include "assets/p08_animation_0964C_records.inc"
};

static u16 _gP08Animation0964CIndices[20] = {
#include "assets/p08_animation_0964C_indices.inc"
};

static AnimationSet _gP08Animation0964C = {
    _gP08Animation0964CRecords,
    _gP08Animation0964CIndices,
    { NULL, _gP08Animation0964CBank1, NULL, NULL, _gP08Animation0964CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation09ACCBank1[6] = {
#include "assets/p08_animation_09ACC_bank1.inc"
};

static AnimationPackedRotation _gP08Animation09ACCBank4[107] = {
#include "assets/p08_animation_09ACC_bank4.inc"
};

static AnimationRecord _gP08Animation09ACCRecords[143] = {
#include "assets/p08_animation_09ACC_records.inc"
};

static u16 _gP08Animation09ACCIndices[20] = {
#include "assets/p08_animation_09ACC_indices.inc"
};

static AnimationSet _gP08Animation09ACC = {
    _gP08Animation09ACCRecords,
    _gP08Animation09ACCIndices,
    { NULL, _gP08Animation09ACCBank1, NULL, NULL, _gP08Animation09ACCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation09CA4Bank1[3] = {
#include "assets/p08_animation_09CA4_bank1.inc"
};

static AnimationPackedRotation _gP08Animation09CA4Bank4[32] = {
#include "assets/p08_animation_09CA4_bank4.inc"
};

static AnimationRecord _gP08Animation09CA4Records[57] = {
#include "assets/p08_animation_09CA4_records.inc"
};

static u16 _gP08Animation09CA4Indices[20] = {
#include "assets/p08_animation_09CA4_indices.inc"
};

static AnimationSet _gP08Animation09CA4 = {
    _gP08Animation09CA4Records,
    _gP08Animation09CA4Indices,
    { NULL, _gP08Animation09CA4Bank1, NULL, NULL, _gP08Animation09CA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0A208Bank1[11] = {
#include "assets/p08_animation_0A208_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0A208Bank4[125] = {
#include "assets/p08_animation_0A208_bank4.inc"
};

static AnimationRecord _gP08Animation0A208Records[167] = {
#include "assets/p08_animation_0A208_records.inc"
};

static u16 _gP08Animation0A208Indices[20] = {
#include "assets/p08_animation_0A208_indices.inc"
};

static AnimationSet _gP08Animation0A208 = {
    _gP08Animation0A208Records,
    _gP08Animation0A208Indices,
    { NULL, _gP08Animation0A208Bank1, NULL, NULL, _gP08Animation0A208Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0A3FCBank1[3] = {
#include "assets/p08_animation_0A3FC_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0A3FCBank4[20] = {
#include "assets/p08_animation_0A3FC_bank4.inc"
};

static AnimationRecord _gP08Animation0A3FCRecords[76] = {
#include "assets/p08_animation_0A3FC_records.inc"
};

static u16 _gP08Animation0A3FCIndices[20] = {
#include "assets/p08_animation_0A3FC_indices.inc"
};

static AnimationSet _gP08Animation0A3FC = {
    _gP08Animation0A3FCRecords,
    _gP08Animation0A3FCIndices,
    { NULL, _gP08Animation0A3FCBank1, NULL, NULL, _gP08Animation0A3FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0A87CBank1[8] = {
#include "assets/p08_animation_0A87C_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0A87CBank4[105] = {
#include "assets/p08_animation_0A87C_bank4.inc"
};

static AnimationRecord _gP08Animation0A87CRecords[139] = {
#include "assets/p08_animation_0A87C_records.inc"
};

static u16 _gP08Animation0A87CIndices[20] = {
#include "assets/p08_animation_0A87C_indices.inc"
};

static AnimationSet _gP08Animation0A87C = {
    _gP08Animation0A87CRecords,
    _gP08Animation0A87CIndices,
    { NULL, _gP08Animation0A87CBank1, NULL, NULL, _gP08Animation0A87CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0AA58Bank1[2] = {
#include "assets/p08_animation_0AA58_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0AA58Bank4[17] = {
#include "assets/p08_animation_0AA58_bank4.inc"
};

static AnimationRecord _gP08Animation0AA58Records[76] = {
#include "assets/p08_animation_0AA58_records.inc"
};

static u16 _gP08Animation0AA58Indices[20] = {
#include "assets/p08_animation_0AA58_indices.inc"
};

static AnimationSet _gP08Animation0AA58 = {
    _gP08Animation0AA58Records,
    _gP08Animation0AA58Indices,
    { NULL, _gP08Animation0AA58Bank1, NULL, NULL, _gP08Animation0AA58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0B148Bank1[12] = {
#include "assets/p08_animation_0B148_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0B148Bank4[164] = {
#include "assets/p08_animation_0B148_bank4.inc"
};

static AnimationRecord _gP08Animation0B148Records[224] = {
#include "assets/p08_animation_0B148_records.inc"
};

static u16 _gP08Animation0B148Indices[20] = {
#include "assets/p08_animation_0B148_indices.inc"
};

static AnimationSet _gP08Animation0B148 = {
    _gP08Animation0B148Records,
    _gP08Animation0B148Indices,
    { NULL, _gP08Animation0B148Bank1, NULL, NULL, _gP08Animation0B148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0BD34Bank1[19] = {
#include "assets/p08_animation_0BD34_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0BD34Bank4[304] = {
#include "assets/p08_animation_0BD34_bank4.inc"
};

static AnimationRecord _gP08Animation0BD34Records[382] = {
#include "assets/p08_animation_0BD34_records.inc"
};

static u16 _gP08Animation0BD34Indices[20] = {
#include "assets/p08_animation_0BD34_indices.inc"
};

static AnimationSet _gP08Animation0BD34 = {
    _gP08Animation0BD34Records,
    _gP08Animation0BD34Indices,
    { NULL, _gP08Animation0BD34Bank1, NULL, NULL, _gP08Animation0BD34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0C65CBank1[15] = {
#include "assets/p08_animation_0C65C_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0C65CBank4[231] = {
#include "assets/p08_animation_0C65C_bank4.inc"
};

static AnimationRecord _gP08Animation0C65CRecords[290] = {
#include "assets/p08_animation_0C65C_records.inc"
};

static u16 _gP08Animation0C65CIndices[20] = {
#include "assets/p08_animation_0C65C_indices.inc"
};

static AnimationSet _gP08Animation0C65C = {
    _gP08Animation0C65CRecords,
    _gP08Animation0C65CIndices,
    { NULL, _gP08Animation0C65CBank1, NULL, NULL, _gP08Animation0C65CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0CA2CBank1[6] = {
#include "assets/p08_animation_0CA2C_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0CA2CBank4[81] = {
#include "assets/p08_animation_0CA2C_bank4.inc"
};

static AnimationRecord _gP08Animation0CA2CRecords[125] = {
#include "assets/p08_animation_0CA2C_records.inc"
};

static u16 _gP08Animation0CA2CIndices[20] = {
#include "assets/p08_animation_0CA2C_indices.inc"
};

static AnimationSet _gP08Animation0CA2C = {
    _gP08Animation0CA2CRecords,
    _gP08Animation0CA2CIndices,
    { NULL, _gP08Animation0CA2CBank1, NULL, NULL, _gP08Animation0CA2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0D0A8Bank1[11] = {
#include "assets/p08_animation_0D0A8_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0D0A8Bank4[161] = {
#include "assets/p08_animation_0D0A8_bank4.inc"
};

static AnimationRecord _gP08Animation0D0A8Records[201] = {
#include "assets/p08_animation_0D0A8_records.inc"
};

static u16 _gP08Animation0D0A8Indices[20] = {
#include "assets/p08_animation_0D0A8_indices.inc"
};

static AnimationSet _gP08Animation0D0A8 = {
    _gP08Animation0D0A8Records,
    _gP08Animation0D0A8Indices,
    { NULL, _gP08Animation0D0A8Bank1, NULL, NULL, _gP08Animation0D0A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP08Animation0D674Bank1[10] = {
#include "assets/p08_animation_0D674_bank1.inc"
};

static AnimationPackedRotation _gP08Animation0D674Bank4[145] = {
#include "assets/p08_animation_0D674_bank4.inc"
};

static AnimationRecord _gP08Animation0D674Records[176] = {
#include "assets/p08_animation_0D674_records.inc"
};

static u16 _gP08Animation0D674Indices[20] = {
#include "assets/p08_animation_0D674_indices.inc"
};

static AnimationSet _gP08Animation0D674 = {
    _gP08Animation0D674Records,
    _gP08Animation0D674Indices,
    { NULL, _gP08Animation0D674Bank1, NULL, NULL, _gP08Animation0D674Bank4, NULL, NULL, NULL },
};

AnimationBank D_p08_8012A85C = { { {
    NULL,
    &_gP08Animation00918,
    &_gP08Animation01908,
    &_gP08Animation021E0,
    &_gP08Animation02914,
    &_gP08Animation03174,
    &_gP08Animation039D8,
    &_gP08Animation0D0A8,
    &_gP08Animation0D674,
    &_gP08Animation0AA58,
    &_gP08Animation0CA2C,
    &_gP08Animation0CA2C,
    &_gP08Animation0BD34,
    &_gP08Animation0B148,
    &_gP08Animation0C65C,
    &_gP08Animation0C65C,
    &_gP08Animation0663C,
    &_gP08Animation0695C,
    &_gP08Animation07120,
    &_gP08Animation00FBC,
    &_gP08Animation0C65C,
    &_gP08Animation00918,
    &_gP08Animation00918,
    &_gP08Animation083B8,
    &_gP08Animation0964C,
    &_gP08Animation08F30,
    &_gP08Animation056BC,
    &_gP08Animation058B8,
    &_gP08Animation05B90,
    &_gP08Animation05E34,
    &_gP08Animation06034,
    &_gP08Animation06388,
    &_gP08Animation09ACC,
    &_gP08Animation09CA4,
    &_gP08Animation09ACC,
    &_gP08Animation09CA4,
    &_gP08Animation04448,
    &_gP08Animation04BD0,
    &_gP08Animation05234,
    &_gP08Animation04EA4,
    &_gP08Animation03CEC,
    &_gP08Animation00918,
    &_gP08Animation0A208,
    &_gP08Animation0A3FC,
    &_gP08Animation0A87C,
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
#elif WEAPON_ID == 0x9
static AnimationPackedPose _gMongooseAnimation00928Bank1[2] = {
#include "assets/mongoose_animation_00928_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation00928Bank4[8] = {
#include "assets/mongoose_animation_00928_bank4.inc"
};

static AnimationRecord _gMongooseAnimation00928Records[76] = {
#include "assets/mongoose_animation_00928_records.inc"
};

static u16 _gMongooseAnimation00928Indices[20] = {
#include "assets/mongoose_animation_00928_indices.inc"
};

static AnimationSet _gMongooseAnimation00928 = {
    _gMongooseAnimation00928Records,
    _gMongooseAnimation00928Indices,
    { NULL, _gMongooseAnimation00928Bank1, NULL, NULL, _gMongooseAnimation00928Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation00FCCBank1[12] = {
#include "assets/mongoose_animation_00FCC_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation00FCCBank4[151] = {
#include "assets/mongoose_animation_00FCC_bank4.inc"
};

static AnimationRecord _gMongooseAnimation00FCCRecords[218] = {
#include "assets/mongoose_animation_00FCC_records.inc"
};

static u16 _gMongooseAnimation00FCCIndices[20] = {
#include "assets/mongoose_animation_00FCC_indices.inc"
};

static AnimationSet _gMongooseAnimation00FCC = {
    _gMongooseAnimation00FCCRecords,
    _gMongooseAnimation00FCCIndices,
    { NULL, _gMongooseAnimation00FCCBank1, NULL, NULL, _gMongooseAnimation00FCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation01918Bank1[16] = {
#include "assets/mongoose_animation_01918_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation01918Bank4[221] = {
#include "assets/mongoose_animation_01918_bank4.inc"
};

static AnimationRecord _gMongooseAnimation01918Records[306] = {
#include "assets/mongoose_animation_01918_records.inc"
};

static u16 _gMongooseAnimation01918Indices[20] = {
#include "assets/mongoose_animation_01918_indices.inc"
};

static AnimationSet _gMongooseAnimation01918 = {
    _gMongooseAnimation01918Records,
    _gMongooseAnimation01918Indices,
    { NULL, _gMongooseAnimation01918Bank1, NULL, NULL, _gMongooseAnimation01918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation021F0Bank1[19] = {
#include "assets/mongoose_animation_021F0_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation021F0Bank4[193] = {
#include "assets/mongoose_animation_021F0_bank4.inc"
};

static AnimationRecord _gMongooseAnimation021F0Records[296] = {
#include "assets/mongoose_animation_021F0_records.inc"
};

static u16 _gMongooseAnimation021F0Indices[20] = {
#include "assets/mongoose_animation_021F0_indices.inc"
};

static AnimationSet _gMongooseAnimation021F0 = {
    _gMongooseAnimation021F0Records,
    _gMongooseAnimation021F0Indices,
    { NULL, _gMongooseAnimation021F0Bank1, NULL, NULL, _gMongooseAnimation021F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation02924Bank1[13] = {
#include "assets/mongoose_animation_02924_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation02924Bank4[167] = {
#include "assets/mongoose_animation_02924_bank4.inc"
};

static AnimationRecord _gMongooseAnimation02924Records[235] = {
#include "assets/mongoose_animation_02924_records.inc"
};

static u16 _gMongooseAnimation02924Indices[20] = {
#include "assets/mongoose_animation_02924_indices.inc"
};

static AnimationSet _gMongooseAnimation02924 = {
    _gMongooseAnimation02924Records,
    _gMongooseAnimation02924Indices,
    { NULL, _gMongooseAnimation02924Bank1, NULL, NULL, _gMongooseAnimation02924Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation03184Bank1[19] = {
#include "assets/mongoose_animation_03184_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation03184Bank4[169] = {
#include "assets/mongoose_animation_03184_bank4.inc"
};

static AnimationRecord _gMongooseAnimation03184Records[290] = {
#include "assets/mongoose_animation_03184_records.inc"
};

static u16 _gMongooseAnimation03184Indices[20] = {
#include "assets/mongoose_animation_03184_indices.inc"
};

static AnimationSet _gMongooseAnimation03184 = {
    _gMongooseAnimation03184Records,
    _gMongooseAnimation03184Indices,
    { NULL, _gMongooseAnimation03184Bank1, NULL, NULL, _gMongooseAnimation03184Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation039E8Bank1[19] = {
#include "assets/mongoose_animation_039E8_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation039E8Bank4[170] = {
#include "assets/mongoose_animation_039E8_bank4.inc"
};

static AnimationRecord _gMongooseAnimation039E8Records[290] = {
#include "assets/mongoose_animation_039E8_records.inc"
};

static u16 _gMongooseAnimation039E8Indices[20] = {
#include "assets/mongoose_animation_039E8_indices.inc"
};

static AnimationSet _gMongooseAnimation039E8 = {
    _gMongooseAnimation039E8Records,
    _gMongooseAnimation039E8Indices,
    { NULL, _gMongooseAnimation039E8Bank1, NULL, NULL, _gMongooseAnimation039E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation03CFCBank1[3] = {
#include "assets/mongoose_animation_03CFC_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation03CFCBank4[69] = {
#include "assets/mongoose_animation_03CFC_bank4.inc"
};

static AnimationRecord _gMongooseAnimation03CFCRecords[99] = {
#include "assets/mongoose_animation_03CFC_records.inc"
};

static u16 _gMongooseAnimation03CFCIndices[20] = {
#include "assets/mongoose_animation_03CFC_indices.inc"
};

static AnimationSet _gMongooseAnimation03CFC = {
    _gMongooseAnimation03CFCRecords,
    _gMongooseAnimation03CFCIndices,
    { NULL, _gMongooseAnimation03CFCBank1, NULL, NULL, _gMongooseAnimation03CFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation04458Bank1[14] = {
#include "assets/mongoose_animation_04458_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation04458Bank4[156] = {
#include "assets/mongoose_animation_04458_bank4.inc"
};

static AnimationRecord _gMongooseAnimation04458Records[253] = {
#include "assets/mongoose_animation_04458_records.inc"
};

static u16 _gMongooseAnimation04458Indices[20] = {
#include "assets/mongoose_animation_04458_indices.inc"
};

static AnimationSet _gMongooseAnimation04458 = {
    _gMongooseAnimation04458Records,
    _gMongooseAnimation04458Indices,
    { NULL, _gMongooseAnimation04458Bank1, NULL, NULL, _gMongooseAnimation04458Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation04BE0Bank1[16] = {
#include "assets/mongoose_animation_04BE0_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation04BE0Bank4[167] = {
#include "assets/mongoose_animation_04BE0_bank4.inc"
};

static AnimationRecord _gMongooseAnimation04BE0Records[247] = {
#include "assets/mongoose_animation_04BE0_records.inc"
};

static u16 _gMongooseAnimation04BE0Indices[20] = {
#include "assets/mongoose_animation_04BE0_indices.inc"
};

static AnimationSet _gMongooseAnimation04BE0 = {
    _gMongooseAnimation04BE0Records,
    _gMongooseAnimation04BE0Indices,
    { NULL, _gMongooseAnimation04BE0Bank1, NULL, NULL, _gMongooseAnimation04BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation04EB4Bank1[6] = {
#include "assets/mongoose_animation_04EB4_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation04EB4Bank4[52] = {
#include "assets/mongoose_animation_04EB4_bank4.inc"
};

static AnimationRecord _gMongooseAnimation04EB4Records[91] = {
#include "assets/mongoose_animation_04EB4_records.inc"
};

static u16 _gMongooseAnimation04EB4Indices[20] = {
#include "assets/mongoose_animation_04EB4_indices.inc"
};

static AnimationSet _gMongooseAnimation04EB4 = {
    _gMongooseAnimation04EB4Records,
    _gMongooseAnimation04EB4Indices,
    { NULL, _gMongooseAnimation04EB4Bank1, NULL, NULL, _gMongooseAnimation04EB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation05244Bank1[7] = {
#include "assets/mongoose_animation_05244_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation05244Bank4[73] = {
#include "assets/mongoose_animation_05244_bank4.inc"
};

static AnimationRecord _gMongooseAnimation05244Records[114] = {
#include "assets/mongoose_animation_05244_records.inc"
};

static u16 _gMongooseAnimation05244Indices[20] = {
#include "assets/mongoose_animation_05244_indices.inc"
};

static AnimationSet _gMongooseAnimation05244 = {
    _gMongooseAnimation05244Records,
    _gMongooseAnimation05244Indices,
    { NULL, _gMongooseAnimation05244Bank1, NULL, NULL, _gMongooseAnimation05244Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation056CCBank1[9] = {
#include "assets/mongoose_animation_056CC_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation056CCBank4[104] = {
#include "assets/mongoose_animation_056CC_bank4.inc"
};

static AnimationRecord _gMongooseAnimation056CCRecords[139] = {
#include "assets/mongoose_animation_056CC_records.inc"
};

static u16 _gMongooseAnimation056CCIndices[20] = {
#include "assets/mongoose_animation_056CC_indices.inc"
};

static AnimationSet _gMongooseAnimation056CC = {
    _gMongooseAnimation056CCRecords,
    _gMongooseAnimation056CCIndices,
    { NULL, _gMongooseAnimation056CCBank1, NULL, NULL, _gMongooseAnimation056CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation058C8Bank1[3] = {
#include "assets/mongoose_animation_058C8_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation058C8Bank4[22] = {
#include "assets/mongoose_animation_058C8_bank4.inc"
};

static AnimationRecord _gMongooseAnimation058C8Records[76] = {
#include "assets/mongoose_animation_058C8_records.inc"
};

static u16 _gMongooseAnimation058C8Indices[20] = {
#include "assets/mongoose_animation_058C8_indices.inc"
};

static AnimationSet _gMongooseAnimation058C8 = {
    _gMongooseAnimation058C8Records,
    _gMongooseAnimation058C8Indices,
    { NULL, _gMongooseAnimation058C8Bank1, NULL, NULL, _gMongooseAnimation058C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation05BA0Bank1[6] = {
#include "assets/mongoose_animation_05BA0_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation05BA0Bank4[57] = {
#include "assets/mongoose_animation_05BA0_bank4.inc"
};

static AnimationRecord _gMongooseAnimation05BA0Records[87] = {
#include "assets/mongoose_animation_05BA0_records.inc"
};

static u16 _gMongooseAnimation05BA0Indices[20] = {
#include "assets/mongoose_animation_05BA0_indices.inc"
};

static AnimationSet _gMongooseAnimation05BA0 = {
    _gMongooseAnimation05BA0Records,
    _gMongooseAnimation05BA0Indices,
    { NULL, _gMongooseAnimation05BA0Bank1, NULL, NULL, _gMongooseAnimation05BA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation05E44Bank1[4] = {
#include "assets/mongoose_animation_05E44_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation05E44Bank4[55] = {
#include "assets/mongoose_animation_05E44_bank4.inc"
};

static AnimationRecord _gMongooseAnimation05E44Records[82] = {
#include "assets/mongoose_animation_05E44_records.inc"
};

static u16 _gMongooseAnimation05E44Indices[20] = {
#include "assets/mongoose_animation_05E44_indices.inc"
};

static AnimationSet _gMongooseAnimation05E44 = {
    _gMongooseAnimation05E44Records,
    _gMongooseAnimation05E44Indices,
    { NULL, _gMongooseAnimation05E44Bank1, NULL, NULL, _gMongooseAnimation05E44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation06044Bank1[3] = {
#include "assets/mongoose_animation_06044_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation06044Bank4[23] = {
#include "assets/mongoose_animation_06044_bank4.inc"
};

static AnimationRecord _gMongooseAnimation06044Records[76] = {
#include "assets/mongoose_animation_06044_records.inc"
};

static u16 _gMongooseAnimation06044Indices[20] = {
#include "assets/mongoose_animation_06044_indices.inc"
};

static AnimationSet _gMongooseAnimation06044 = {
    _gMongooseAnimation06044Records,
    _gMongooseAnimation06044Indices,
    { NULL, _gMongooseAnimation06044Bank1, NULL, NULL, _gMongooseAnimation06044Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation06398Bank1[8] = {
#include "assets/mongoose_animation_06398_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation06398Bank4[68] = {
#include "assets/mongoose_animation_06398_bank4.inc"
};

static AnimationRecord _gMongooseAnimation06398Records[101] = {
#include "assets/mongoose_animation_06398_records.inc"
};

static u16 _gMongooseAnimation06398Indices[20] = {
#include "assets/mongoose_animation_06398_indices.inc"
};

static AnimationSet _gMongooseAnimation06398 = {
    _gMongooseAnimation06398Records,
    _gMongooseAnimation06398Indices,
    { NULL, _gMongooseAnimation06398Bank1, NULL, NULL, _gMongooseAnimation06398Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0664CBank1[5] = {
#include "assets/mongoose_animation_0664C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0664CBank4[55] = {
#include "assets/mongoose_animation_0664C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0664CRecords[83] = {
#include "assets/mongoose_animation_0664C_records.inc"
};

static u16 _gMongooseAnimation0664CIndices[20] = {
#include "assets/mongoose_animation_0664C_indices.inc"
};

static AnimationSet _gMongooseAnimation0664C = {
    _gMongooseAnimation0664CRecords,
    _gMongooseAnimation0664CIndices,
    { NULL, _gMongooseAnimation0664CBank1, NULL, NULL, _gMongooseAnimation0664CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0696CBank1[6] = {
#include "assets/mongoose_animation_0696C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0696CBank4[66] = {
#include "assets/mongoose_animation_0696C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0696CRecords[96] = {
#include "assets/mongoose_animation_0696C_records.inc"
};

static u16 _gMongooseAnimation0696CIndices[20] = {
#include "assets/mongoose_animation_0696C_indices.inc"
};

static AnimationSet _gMongooseAnimation0696C = {
    _gMongooseAnimation0696CRecords,
    _gMongooseAnimation0696CIndices,
    { NULL, _gMongooseAnimation0696CBank1, NULL, NULL, _gMongooseAnimation0696CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation07130Bank1[18] = {
#include "assets/mongoose_animation_07130_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation07130Bank4[184] = {
#include "assets/mongoose_animation_07130_bank4.inc"
};

static AnimationRecord _gMongooseAnimation07130Records[239] = {
#include "assets/mongoose_animation_07130_records.inc"
};

static u16 _gMongooseAnimation07130Indices[20] = {
#include "assets/mongoose_animation_07130_indices.inc"
};

static AnimationSet _gMongooseAnimation07130 = {
    _gMongooseAnimation07130Records,
    _gMongooseAnimation07130Indices,
    { NULL, _gMongooseAnimation07130Bank1, NULL, NULL, _gMongooseAnimation07130Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation083C8Bank1[29] = {
#include "assets/mongoose_animation_083C8_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation083C8Bank4[450] = {
#include "assets/mongoose_animation_083C8_bank4.inc"
};

static AnimationRecord _gMongooseAnimation083C8Records[633] = {
#include "assets/mongoose_animation_083C8_records.inc"
};

static u16 _gMongooseAnimation083C8Indices[20] = {
#include "assets/mongoose_animation_083C8_indices.inc"
};

static AnimationSet _gMongooseAnimation083C8 = {
    _gMongooseAnimation083C8Records,
    _gMongooseAnimation083C8Indices,
    { NULL, _gMongooseAnimation083C8Bank1, NULL, NULL, _gMongooseAnimation083C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation08F40Bank1[12] = {
#include "assets/mongoose_animation_08F40_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation08F40Bank4[266] = {
#include "assets/mongoose_animation_08F40_bank4.inc"
};

static AnimationRecord _gMongooseAnimation08F40Records[412] = {
#include "assets/mongoose_animation_08F40_records.inc"
};

static u16 _gMongooseAnimation08F40Indices[20] = {
#include "assets/mongoose_animation_08F40_indices.inc"
};

static AnimationSet _gMongooseAnimation08F40 = {
    _gMongooseAnimation08F40Records,
    _gMongooseAnimation08F40Indices,
    { NULL, _gMongooseAnimation08F40Bank1, NULL, NULL, _gMongooseAnimation08F40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0965CBank1[9] = {
#include "assets/mongoose_animation_0965C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0965CBank4[144] = {
#include "assets/mongoose_animation_0965C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0965CRecords[264] = {
#include "assets/mongoose_animation_0965C_records.inc"
};

static u16 _gMongooseAnimation0965CIndices[20] = {
#include "assets/mongoose_animation_0965C_indices.inc"
};

static AnimationSet _gMongooseAnimation0965C = {
    _gMongooseAnimation0965CRecords,
    _gMongooseAnimation0965CIndices,
    { NULL, _gMongooseAnimation0965CBank1, NULL, NULL, _gMongooseAnimation0965CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation09ADCBank1[6] = {
#include "assets/mongoose_animation_09ADC_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation09ADCBank4[107] = {
#include "assets/mongoose_animation_09ADC_bank4.inc"
};

static AnimationRecord _gMongooseAnimation09ADCRecords[143] = {
#include "assets/mongoose_animation_09ADC_records.inc"
};

static u16 _gMongooseAnimation09ADCIndices[20] = {
#include "assets/mongoose_animation_09ADC_indices.inc"
};

static AnimationSet _gMongooseAnimation09ADC = {
    _gMongooseAnimation09ADCRecords,
    _gMongooseAnimation09ADCIndices,
    { NULL, _gMongooseAnimation09ADCBank1, NULL, NULL, _gMongooseAnimation09ADCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation09CB4Bank1[3] = {
#include "assets/mongoose_animation_09CB4_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation09CB4Bank4[32] = {
#include "assets/mongoose_animation_09CB4_bank4.inc"
};

static AnimationRecord _gMongooseAnimation09CB4Records[57] = {
#include "assets/mongoose_animation_09CB4_records.inc"
};

static u16 _gMongooseAnimation09CB4Indices[20] = {
#include "assets/mongoose_animation_09CB4_indices.inc"
};

static AnimationSet _gMongooseAnimation09CB4 = {
    _gMongooseAnimation09CB4Records,
    _gMongooseAnimation09CB4Indices,
    { NULL, _gMongooseAnimation09CB4Bank1, NULL, NULL, _gMongooseAnimation09CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0A218Bank1[11] = {
#include "assets/mongoose_animation_0A218_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0A218Bank4[125] = {
#include "assets/mongoose_animation_0A218_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0A218Records[167] = {
#include "assets/mongoose_animation_0A218_records.inc"
};

static u16 _gMongooseAnimation0A218Indices[20] = {
#include "assets/mongoose_animation_0A218_indices.inc"
};

static AnimationSet _gMongooseAnimation0A218 = {
    _gMongooseAnimation0A218Records,
    _gMongooseAnimation0A218Indices,
    { NULL, _gMongooseAnimation0A218Bank1, NULL, NULL, _gMongooseAnimation0A218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0A40CBank1[3] = {
#include "assets/mongoose_animation_0A40C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0A40CBank4[20] = {
#include "assets/mongoose_animation_0A40C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0A40CRecords[76] = {
#include "assets/mongoose_animation_0A40C_records.inc"
};

static u16 _gMongooseAnimation0A40CIndices[20] = {
#include "assets/mongoose_animation_0A40C_indices.inc"
};

static AnimationSet _gMongooseAnimation0A40C = {
    _gMongooseAnimation0A40CRecords,
    _gMongooseAnimation0A40CIndices,
    { NULL, _gMongooseAnimation0A40CBank1, NULL, NULL, _gMongooseAnimation0A40CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0A88CBank1[8] = {
#include "assets/mongoose_animation_0A88C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0A88CBank4[105] = {
#include "assets/mongoose_animation_0A88C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0A88CRecords[139] = {
#include "assets/mongoose_animation_0A88C_records.inc"
};

static u16 _gMongooseAnimation0A88CIndices[20] = {
#include "assets/mongoose_animation_0A88C_indices.inc"
};

static AnimationSet _gMongooseAnimation0A88C = {
    _gMongooseAnimation0A88CRecords,
    _gMongooseAnimation0A88CIndices,
    { NULL, _gMongooseAnimation0A88CBank1, NULL, NULL, _gMongooseAnimation0A88CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0AA68Bank1[2] = {
#include "assets/mongoose_animation_0AA68_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0AA68Bank4[17] = {
#include "assets/mongoose_animation_0AA68_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0AA68Records[76] = {
#include "assets/mongoose_animation_0AA68_records.inc"
};

static u16 _gMongooseAnimation0AA68Indices[20] = {
#include "assets/mongoose_animation_0AA68_indices.inc"
};

static AnimationSet _gMongooseAnimation0AA68 = {
    _gMongooseAnimation0AA68Records,
    _gMongooseAnimation0AA68Indices,
    { NULL, _gMongooseAnimation0AA68Bank1, NULL, NULL, _gMongooseAnimation0AA68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0B158Bank1[12] = {
#include "assets/mongoose_animation_0B158_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0B158Bank4[164] = {
#include "assets/mongoose_animation_0B158_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0B158Records[224] = {
#include "assets/mongoose_animation_0B158_records.inc"
};

static u16 _gMongooseAnimation0B158Indices[20] = {
#include "assets/mongoose_animation_0B158_indices.inc"
};

static AnimationSet _gMongooseAnimation0B158 = {
    _gMongooseAnimation0B158Records,
    _gMongooseAnimation0B158Indices,
    { NULL, _gMongooseAnimation0B158Bank1, NULL, NULL, _gMongooseAnimation0B158Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0BD44Bank1[19] = {
#include "assets/mongoose_animation_0BD44_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0BD44Bank4[304] = {
#include "assets/mongoose_animation_0BD44_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0BD44Records[382] = {
#include "assets/mongoose_animation_0BD44_records.inc"
};

static u16 _gMongooseAnimation0BD44Indices[20] = {
#include "assets/mongoose_animation_0BD44_indices.inc"
};

static AnimationSet _gMongooseAnimation0BD44 = {
    _gMongooseAnimation0BD44Records,
    _gMongooseAnimation0BD44Indices,
    { NULL, _gMongooseAnimation0BD44Bank1, NULL, NULL, _gMongooseAnimation0BD44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0C59CBank1[15] = {
#include "assets/mongoose_animation_0C59C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0C59CBank4[205] = {
#include "assets/mongoose_animation_0C59C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0C59CRecords[264] = {
#include "assets/mongoose_animation_0C59C_records.inc"
};

static u16 _gMongooseAnimation0C59CIndices[20] = {
#include "assets/mongoose_animation_0C59C_indices.inc"
};

static AnimationSet _gMongooseAnimation0C59C = {
    _gMongooseAnimation0C59CRecords,
    _gMongooseAnimation0C59CIndices,
    { NULL, _gMongooseAnimation0C59CBank1, NULL, NULL, _gMongooseAnimation0C59CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0CB90Bank1[10] = {
#include "assets/mongoose_animation_0CB90_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0CB90Bank4[138] = {
#include "assets/mongoose_animation_0CB90_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0CB90Records[193] = {
#include "assets/mongoose_animation_0CB90_records.inc"
};

static u16 _gMongooseAnimation0CB90Indices[20] = {
#include "assets/mongoose_animation_0CB90_indices.inc"
};

static AnimationSet _gMongooseAnimation0CB90 = {
    _gMongooseAnimation0CB90Records,
    _gMongooseAnimation0CB90Indices,
    { NULL, _gMongooseAnimation0CB90Bank1, NULL, NULL, _gMongooseAnimation0CB90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0D20CBank1[11] = {
#include "assets/mongoose_animation_0D20C_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0D20CBank4[161] = {
#include "assets/mongoose_animation_0D20C_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0D20CRecords[201] = {
#include "assets/mongoose_animation_0D20C_records.inc"
};

static u16 _gMongooseAnimation0D20CIndices[20] = {
#include "assets/mongoose_animation_0D20C_indices.inc"
};

static AnimationSet _gMongooseAnimation0D20C = {
    _gMongooseAnimation0D20CRecords,
    _gMongooseAnimation0D20CIndices,
    { NULL, _gMongooseAnimation0D20CBank1, NULL, NULL, _gMongooseAnimation0D20CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMongooseAnimation0D7D8Bank1[10] = {
#include "assets/mongoose_animation_0D7D8_bank1.inc"
};

static AnimationPackedRotation _gMongooseAnimation0D7D8Bank4[145] = {
#include "assets/mongoose_animation_0D7D8_bank4.inc"
};

static AnimationRecord _gMongooseAnimation0D7D8Records[176] = {
#include "assets/mongoose_animation_0D7D8_records.inc"
};

static u16 _gMongooseAnimation0D7D8Indices[20] = {
#include "assets/mongoose_animation_0D7D8_indices.inc"
};

static AnimationSet _gMongooseAnimation0D7D8 = {
    _gMongooseAnimation0D7D8Records,
    _gMongooseAnimation0D7D8Indices,
    { NULL, _gMongooseAnimation0D7D8Bank1, NULL, NULL, _gMongooseAnimation0D7D8Bank4, NULL, NULL, NULL },
};

AnimationBank D_mongoose_8012A9C0 = { { {
    NULL,
    &_gMongooseAnimation00928,
    &_gMongooseAnimation01918,
    &_gMongooseAnimation021F0,
    &_gMongooseAnimation02924,
    &_gMongooseAnimation03184,
    &_gMongooseAnimation039E8,
    &_gMongooseAnimation0D20C,
    &_gMongooseAnimation0D7D8,
    &_gMongooseAnimation0AA68,
    &_gMongooseAnimation0CB90,
    &_gMongooseAnimation0CB90,
    &_gMongooseAnimation0BD44,
    &_gMongooseAnimation0B158,
    &_gMongooseAnimation0C59C,
    &_gMongooseAnimation0C59C,
    &_gMongooseAnimation0664C,
    &_gMongooseAnimation0696C,
    &_gMongooseAnimation07130,
    &_gMongooseAnimation00FCC,
    &_gMongooseAnimation0C59C,
    &_gMongooseAnimation00928,
    &_gMongooseAnimation00928,
    &_gMongooseAnimation083C8,
    &_gMongooseAnimation0965C,
    &_gMongooseAnimation08F40,
    &_gMongooseAnimation056CC,
    &_gMongooseAnimation058C8,
    &_gMongooseAnimation05BA0,
    &_gMongooseAnimation05E44,
    &_gMongooseAnimation06044,
    &_gMongooseAnimation06398,
    &_gMongooseAnimation09ADC,
    &_gMongooseAnimation09CB4,
    &_gMongooseAnimation09ADC,
    &_gMongooseAnimation09CB4,
    &_gMongooseAnimation04458,
    &_gMongooseAnimation04BE0,
    &_gMongooseAnimation05244,
    &_gMongooseAnimation04EB4,
    &_gMongooseAnimation03CFC,
    &_gMongooseAnimation00928,
    &_gMongooseAnimation0A218,
    &_gMongooseAnimation0A40C,
    &_gMongooseAnimation0A88C,
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
#endif

PACKAGE_ALIASES
