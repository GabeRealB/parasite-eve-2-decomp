#include "common.h"

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

/// Scratch-stack block for the M249's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages the node a fired round's
/// impact sound is placed at.
///
/// Only the translation of `impactCoord`'s composed transform is written, and
/// only when the impact picker reports a hit, which is the one case the node
/// is then read in. A sound's pan and depth come from projecting the node's
/// origin, to which its rotation contributes nothing, so the rest of the node
/// is whatever the scratch stack last held. The position is a weapon contact
/// point offset by up to 7 units per axis, in the space the weapon node's
/// composed transform is expressed in.
///
/// The 0x18 bytes ahead of the node are reserved with it and never accessed.
typedef struct {
    byte     field_0[0x18]; // No recovered access; role unproven
    GfxCoord impactCoord;   // Node standing at the round's impact point: the source of the impact sound
} _M249AttackScratch;
STATIC_ASSERT_SIZEOF(_M249AttackScratch, 0x68);

void func_m249_8011D1DC(Task* arg0);

void func_m249_8011D1DC(Task* arg0)
{
    GameActor*          actor;
    GfxCoord*           coord;
    GfxCoord*           spot;
    _M249AttackScratch* scratch;
    s32                 anim;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_M249AttackScratch);
    actor   = arg0->work;
    coord   = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
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
            actor->stateTimer                  = 2;
            func_80106238(arg0, 0, actor->attackButton != 1);
            /* fallthrough */
        case 3:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                Gp_ConsumeSlotQty(0x90, 1);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20110004, 1);
                Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, 0x11,
                            NULL);
                playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 2);
            }
            break;
        case 4:
            spot                = &scratch->impactCoord;
            actor->movementSign = 0;
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
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(_M249AttackScratch);
}

static TmdBone _gM249Model0060CSkeleton[1] = {
#include "assets/m249_model_0060C_skeleton.inc"
};

static u32 _gM249Model0060CPartVerts[1] = {
#include "assets/m249_model_0060C_partVerts.inc"
};

static SVECTOR _gM249Model0060CVerts[58] = {
#include "assets/m249_model_0060C_verts.inc"
};

static SVECTOR _gM249Model0060CNormals[47] = {
#include "assets/m249_model_0060C_normals.inc"
};

static u32 _gM249Model0060CStream[426] = {
#include "assets/m249_model_0060C_stream.inc"
};

TmdSource D_m249_8011DE74 = {
    0,
    3052,
    0,
    1,
    _gM249Model0060CPartVerts,
    _gM249Model0060CVerts,
    _gM249Model0060CNormals,
    _gM249Model0060CSkeleton,
    _gM249Model0060CStream,
};

static AnimationPackedPose _gM249Animation00E68Bank1[2] = {
#include "assets/m249_animation_00E68_bank1.inc"
};

static AnimationPackedRotation _gM249Animation00E68Bank4[8] = {
#include "assets/m249_animation_00E68_bank4.inc"
};

static AnimationRecord _gM249Animation00E68Records[76] = {
#include "assets/m249_animation_00E68_records.inc"
};

static u16 _gM249Animation00E68Indices[20] = {
#include "assets/m249_animation_00E68_indices.inc"
};

static AnimationSet _gM249Animation00E68 = {
    _gM249Animation00E68Records,
    _gM249Animation00E68Indices,
    { NULL, _gM249Animation00E68Bank1, NULL, NULL, _gM249Animation00E68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0150CBank1[12] = {
#include "assets/m249_animation_0150C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0150CBank4[151] = {
#include "assets/m249_animation_0150C_bank4.inc"
};

static AnimationRecord _gM249Animation0150CRecords[218] = {
#include "assets/m249_animation_0150C_records.inc"
};

static u16 _gM249Animation0150CIndices[20] = {
#include "assets/m249_animation_0150C_indices.inc"
};

static AnimationSet _gM249Animation0150C = {
    _gM249Animation0150CRecords,
    _gM249Animation0150CIndices,
    { NULL, _gM249Animation0150CBank1, NULL, NULL, _gM249Animation0150CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation01D6CBank1[19] = {
#include "assets/m249_animation_01D6C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation01D6CBank4[169] = {
#include "assets/m249_animation_01D6C_bank4.inc"
};

static AnimationRecord _gM249Animation01D6CRecords[290] = {
#include "assets/m249_animation_01D6C_records.inc"
};

static u16 _gM249Animation01D6CIndices[20] = {
#include "assets/m249_animation_01D6C_indices.inc"
};

static AnimationSet _gM249Animation01D6C = {
    _gM249Animation01D6CRecords,
    _gM249Animation01D6CIndices,
    { NULL, _gM249Animation01D6CBank1, NULL, NULL, _gM249Animation01D6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation025D0Bank1[19] = {
#include "assets/m249_animation_025D0_bank1.inc"
};

static AnimationPackedRotation _gM249Animation025D0Bank4[170] = {
#include "assets/m249_animation_025D0_bank4.inc"
};

static AnimationRecord _gM249Animation025D0Records[290] = {
#include "assets/m249_animation_025D0_records.inc"
};

static u16 _gM249Animation025D0Indices[20] = {
#include "assets/m249_animation_025D0_indices.inc"
};

static AnimationSet _gM249Animation025D0 = {
    _gM249Animation025D0Records,
    _gM249Animation025D0Indices,
    { NULL, _gM249Animation025D0Bank1, NULL, NULL, _gM249Animation025D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation028E4Bank1[3] = {
#include "assets/m249_animation_028E4_bank1.inc"
};

static AnimationPackedRotation _gM249Animation028E4Bank4[69] = {
#include "assets/m249_animation_028E4_bank4.inc"
};

static AnimationRecord _gM249Animation028E4Records[99] = {
#include "assets/m249_animation_028E4_records.inc"
};

static u16 _gM249Animation028E4Indices[20] = {
#include "assets/m249_animation_028E4_indices.inc"
};

static AnimationSet _gM249Animation028E4 = {
    _gM249Animation028E4Records,
    _gM249Animation028E4Indices,
    { NULL, _gM249Animation028E4Bank1, NULL, NULL, _gM249Animation028E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation03040Bank1[14] = {
#include "assets/m249_animation_03040_bank1.inc"
};

static AnimationPackedRotation _gM249Animation03040Bank4[156] = {
#include "assets/m249_animation_03040_bank4.inc"
};

static AnimationRecord _gM249Animation03040Records[253] = {
#include "assets/m249_animation_03040_records.inc"
};

static u16 _gM249Animation03040Indices[20] = {
#include "assets/m249_animation_03040_indices.inc"
};

static AnimationSet _gM249Animation03040 = {
    _gM249Animation03040Records,
    _gM249Animation03040Indices,
    { NULL, _gM249Animation03040Bank1, NULL, NULL, _gM249Animation03040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation037C8Bank1[16] = {
#include "assets/m249_animation_037C8_bank1.inc"
};

static AnimationPackedRotation _gM249Animation037C8Bank4[167] = {
#include "assets/m249_animation_037C8_bank4.inc"
};

static AnimationRecord _gM249Animation037C8Records[247] = {
#include "assets/m249_animation_037C8_records.inc"
};

static u16 _gM249Animation037C8Indices[20] = {
#include "assets/m249_animation_037C8_indices.inc"
};

static AnimationSet _gM249Animation037C8 = {
    _gM249Animation037C8Records,
    _gM249Animation037C8Indices,
    { NULL, _gM249Animation037C8Bank1, NULL, NULL, _gM249Animation037C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation03A9CBank1[6] = {
#include "assets/m249_animation_03A9C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation03A9CBank4[52] = {
#include "assets/m249_animation_03A9C_bank4.inc"
};

static AnimationRecord _gM249Animation03A9CRecords[91] = {
#include "assets/m249_animation_03A9C_records.inc"
};

static u16 _gM249Animation03A9CIndices[20] = {
#include "assets/m249_animation_03A9C_indices.inc"
};

static AnimationSet _gM249Animation03A9C = {
    _gM249Animation03A9CRecords,
    _gM249Animation03A9CIndices,
    { NULL, _gM249Animation03A9CBank1, NULL, NULL, _gM249Animation03A9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation03E2CBank1[7] = {
#include "assets/m249_animation_03E2C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation03E2CBank4[73] = {
#include "assets/m249_animation_03E2C_bank4.inc"
};

static AnimationRecord _gM249Animation03E2CRecords[114] = {
#include "assets/m249_animation_03E2C_records.inc"
};

static u16 _gM249Animation03E2CIndices[20] = {
#include "assets/m249_animation_03E2C_indices.inc"
};

static AnimationSet _gM249Animation03E2C = {
    _gM249Animation03E2CRecords,
    _gM249Animation03E2CIndices,
    { NULL, _gM249Animation03E2CBank1, NULL, NULL, _gM249Animation03E2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation042B4Bank1[9] = {
#include "assets/m249_animation_042B4_bank1.inc"
};

static AnimationPackedRotation _gM249Animation042B4Bank4[104] = {
#include "assets/m249_animation_042B4_bank4.inc"
};

static AnimationRecord _gM249Animation042B4Records[139] = {
#include "assets/m249_animation_042B4_records.inc"
};

static u16 _gM249Animation042B4Indices[20] = {
#include "assets/m249_animation_042B4_indices.inc"
};

static AnimationSet _gM249Animation042B4 = {
    _gM249Animation042B4Records,
    _gM249Animation042B4Indices,
    { NULL, _gM249Animation042B4Bank1, NULL, NULL, _gM249Animation042B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation044B0Bank1[3] = {
#include "assets/m249_animation_044B0_bank1.inc"
};

static AnimationPackedRotation _gM249Animation044B0Bank4[22] = {
#include "assets/m249_animation_044B0_bank4.inc"
};

static AnimationRecord _gM249Animation044B0Records[76] = {
#include "assets/m249_animation_044B0_records.inc"
};

static u16 _gM249Animation044B0Indices[20] = {
#include "assets/m249_animation_044B0_indices.inc"
};

static AnimationSet _gM249Animation044B0 = {
    _gM249Animation044B0Records,
    _gM249Animation044B0Indices,
    { NULL, _gM249Animation044B0Bank1, NULL, NULL, _gM249Animation044B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation04788Bank1[6] = {
#include "assets/m249_animation_04788_bank1.inc"
};

static AnimationPackedRotation _gM249Animation04788Bank4[57] = {
#include "assets/m249_animation_04788_bank4.inc"
};

static AnimationRecord _gM249Animation04788Records[87] = {
#include "assets/m249_animation_04788_records.inc"
};

static u16 _gM249Animation04788Indices[20] = {
#include "assets/m249_animation_04788_indices.inc"
};

static AnimationSet _gM249Animation04788 = {
    _gM249Animation04788Records,
    _gM249Animation04788Indices,
    { NULL, _gM249Animation04788Bank1, NULL, NULL, _gM249Animation04788Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation04A2CBank1[4] = {
#include "assets/m249_animation_04A2C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation04A2CBank4[55] = {
#include "assets/m249_animation_04A2C_bank4.inc"
};

static AnimationRecord _gM249Animation04A2CRecords[82] = {
#include "assets/m249_animation_04A2C_records.inc"
};

static u16 _gM249Animation04A2CIndices[20] = {
#include "assets/m249_animation_04A2C_indices.inc"
};

static AnimationSet _gM249Animation04A2C = {
    _gM249Animation04A2CRecords,
    _gM249Animation04A2CIndices,
    { NULL, _gM249Animation04A2CBank1, NULL, NULL, _gM249Animation04A2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation04C2CBank1[3] = {
#include "assets/m249_animation_04C2C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation04C2CBank4[23] = {
#include "assets/m249_animation_04C2C_bank4.inc"
};

static AnimationRecord _gM249Animation04C2CRecords[76] = {
#include "assets/m249_animation_04C2C_records.inc"
};

static u16 _gM249Animation04C2CIndices[20] = {
#include "assets/m249_animation_04C2C_indices.inc"
};

static AnimationSet _gM249Animation04C2C = {
    _gM249Animation04C2CRecords,
    _gM249Animation04C2CIndices,
    { NULL, _gM249Animation04C2CBank1, NULL, NULL, _gM249Animation04C2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation04F80Bank1[8] = {
#include "assets/m249_animation_04F80_bank1.inc"
};

static AnimationPackedRotation _gM249Animation04F80Bank4[68] = {
#include "assets/m249_animation_04F80_bank4.inc"
};

static AnimationRecord _gM249Animation04F80Records[101] = {
#include "assets/m249_animation_04F80_records.inc"
};

static u16 _gM249Animation04F80Indices[20] = {
#include "assets/m249_animation_04F80_indices.inc"
};

static AnimationSet _gM249Animation04F80 = {
    _gM249Animation04F80Records,
    _gM249Animation04F80Indices,
    { NULL, _gM249Animation04F80Bank1, NULL, NULL, _gM249Animation04F80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation05234Bank1[5] = {
#include "assets/m249_animation_05234_bank1.inc"
};

static AnimationPackedRotation _gM249Animation05234Bank4[55] = {
#include "assets/m249_animation_05234_bank4.inc"
};

static AnimationRecord _gM249Animation05234Records[83] = {
#include "assets/m249_animation_05234_records.inc"
};

static u16 _gM249Animation05234Indices[20] = {
#include "assets/m249_animation_05234_indices.inc"
};

static AnimationSet _gM249Animation05234 = {
    _gM249Animation05234Records,
    _gM249Animation05234Indices,
    { NULL, _gM249Animation05234Bank1, NULL, NULL, _gM249Animation05234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation05554Bank1[6] = {
#include "assets/m249_animation_05554_bank1.inc"
};

static AnimationPackedRotation _gM249Animation05554Bank4[66] = {
#include "assets/m249_animation_05554_bank4.inc"
};

static AnimationRecord _gM249Animation05554Records[96] = {
#include "assets/m249_animation_05554_records.inc"
};

static u16 _gM249Animation05554Indices[20] = {
#include "assets/m249_animation_05554_indices.inc"
};

static AnimationSet _gM249Animation05554 = {
    _gM249Animation05554Records,
    _gM249Animation05554Indices,
    { NULL, _gM249Animation05554Bank1, NULL, NULL, _gM249Animation05554Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation05D18Bank1[18] = {
#include "assets/m249_animation_05D18_bank1.inc"
};

static AnimationPackedRotation _gM249Animation05D18Bank4[184] = {
#include "assets/m249_animation_05D18_bank4.inc"
};

static AnimationRecord _gM249Animation05D18Records[239] = {
#include "assets/m249_animation_05D18_records.inc"
};

static u16 _gM249Animation05D18Indices[20] = {
#include "assets/m249_animation_05D18_indices.inc"
};

static AnimationSet _gM249Animation05D18 = {
    _gM249Animation05D18Records,
    _gM249Animation05D18Indices,
    { NULL, _gM249Animation05D18Bank1, NULL, NULL, _gM249Animation05D18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation06FB0Bank1[29] = {
#include "assets/m249_animation_06FB0_bank1.inc"
};

static AnimationPackedRotation _gM249Animation06FB0Bank4[450] = {
#include "assets/m249_animation_06FB0_bank4.inc"
};

static AnimationRecord _gM249Animation06FB0Records[633] = {
#include "assets/m249_animation_06FB0_records.inc"
};

static u16 _gM249Animation06FB0Indices[20] = {
#include "assets/m249_animation_06FB0_indices.inc"
};

static AnimationSet _gM249Animation06FB0 = {
    _gM249Animation06FB0Records,
    _gM249Animation06FB0Indices,
    { NULL, _gM249Animation06FB0Bank1, NULL, NULL, _gM249Animation06FB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation07B28Bank1[12] = {
#include "assets/m249_animation_07B28_bank1.inc"
};

static AnimationPackedRotation _gM249Animation07B28Bank4[266] = {
#include "assets/m249_animation_07B28_bank4.inc"
};

static AnimationRecord _gM249Animation07B28Records[412] = {
#include "assets/m249_animation_07B28_records.inc"
};

static u16 _gM249Animation07B28Indices[20] = {
#include "assets/m249_animation_07B28_indices.inc"
};

static AnimationSet _gM249Animation07B28 = {
    _gM249Animation07B28Records,
    _gM249Animation07B28Indices,
    { NULL, _gM249Animation07B28Bank1, NULL, NULL, _gM249Animation07B28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation08244Bank1[9] = {
#include "assets/m249_animation_08244_bank1.inc"
};

static AnimationPackedRotation _gM249Animation08244Bank4[144] = {
#include "assets/m249_animation_08244_bank4.inc"
};

static AnimationRecord _gM249Animation08244Records[264] = {
#include "assets/m249_animation_08244_records.inc"
};

static u16 _gM249Animation08244Indices[20] = {
#include "assets/m249_animation_08244_indices.inc"
};

static AnimationSet _gM249Animation08244 = {
    _gM249Animation08244Records,
    _gM249Animation08244Indices,
    { NULL, _gM249Animation08244Bank1, NULL, NULL, _gM249Animation08244Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation086C4Bank1[6] = {
#include "assets/m249_animation_086C4_bank1.inc"
};

static AnimationPackedRotation _gM249Animation086C4Bank4[107] = {
#include "assets/m249_animation_086C4_bank4.inc"
};

static AnimationRecord _gM249Animation086C4Records[143] = {
#include "assets/m249_animation_086C4_records.inc"
};

static u16 _gM249Animation086C4Indices[20] = {
#include "assets/m249_animation_086C4_indices.inc"
};

static AnimationSet _gM249Animation086C4 = {
    _gM249Animation086C4Records,
    _gM249Animation086C4Indices,
    { NULL, _gM249Animation086C4Bank1, NULL, NULL, _gM249Animation086C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0889CBank1[3] = {
#include "assets/m249_animation_0889C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0889CBank4[32] = {
#include "assets/m249_animation_0889C_bank4.inc"
};

static AnimationRecord _gM249Animation0889CRecords[57] = {
#include "assets/m249_animation_0889C_records.inc"
};

static u16 _gM249Animation0889CIndices[20] = {
#include "assets/m249_animation_0889C_indices.inc"
};

static AnimationSet _gM249Animation0889C = {
    _gM249Animation0889CRecords,
    _gM249Animation0889CIndices,
    { NULL, _gM249Animation0889CBank1, NULL, NULL, _gM249Animation0889CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation08E00Bank1[11] = {
#include "assets/m249_animation_08E00_bank1.inc"
};

static AnimationPackedRotation _gM249Animation08E00Bank4[125] = {
#include "assets/m249_animation_08E00_bank4.inc"
};

static AnimationRecord _gM249Animation08E00Records[167] = {
#include "assets/m249_animation_08E00_records.inc"
};

static u16 _gM249Animation08E00Indices[20] = {
#include "assets/m249_animation_08E00_indices.inc"
};

static AnimationSet _gM249Animation08E00 = {
    _gM249Animation08E00Records,
    _gM249Animation08E00Indices,
    { NULL, _gM249Animation08E00Bank1, NULL, NULL, _gM249Animation08E00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation08FF4Bank1[3] = {
#include "assets/m249_animation_08FF4_bank1.inc"
};

static AnimationPackedRotation _gM249Animation08FF4Bank4[20] = {
#include "assets/m249_animation_08FF4_bank4.inc"
};

static AnimationRecord _gM249Animation08FF4Records[76] = {
#include "assets/m249_animation_08FF4_records.inc"
};

static u16 _gM249Animation08FF4Indices[20] = {
#include "assets/m249_animation_08FF4_indices.inc"
};

static AnimationSet _gM249Animation08FF4 = {
    _gM249Animation08FF4Records,
    _gM249Animation08FF4Indices,
    { NULL, _gM249Animation08FF4Bank1, NULL, NULL, _gM249Animation08FF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation09474Bank1[8] = {
#include "assets/m249_animation_09474_bank1.inc"
};

static AnimationPackedRotation _gM249Animation09474Bank4[105] = {
#include "assets/m249_animation_09474_bank4.inc"
};

static AnimationRecord _gM249Animation09474Records[139] = {
#include "assets/m249_animation_09474_records.inc"
};

static u16 _gM249Animation09474Indices[20] = {
#include "assets/m249_animation_09474_indices.inc"
};

static AnimationSet _gM249Animation09474 = {
    _gM249Animation09474Records,
    _gM249Animation09474Indices,
    { NULL, _gM249Animation09474Bank1, NULL, NULL, _gM249Animation09474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0964CBank1[2] = {
#include "assets/m249_animation_0964C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0964CBank4[16] = {
#include "assets/m249_animation_0964C_bank4.inc"
};

static AnimationRecord _gM249Animation0964CRecords[76] = {
#include "assets/m249_animation_0964C_records.inc"
};

static u16 _gM249Animation0964CIndices[20] = {
#include "assets/m249_animation_0964C_indices.inc"
};

static AnimationSet _gM249Animation0964C = {
    _gM249Animation0964CRecords,
    _gM249Animation0964CIndices,
    { NULL, _gM249Animation0964CBank1, NULL, NULL, _gM249Animation0964CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation09D80Bank1[12] = {
#include "assets/m249_animation_09D80_bank1.inc"
};

static AnimationPackedRotation _gM249Animation09D80Bank4[165] = {
#include "assets/m249_animation_09D80_bank4.inc"
};

static AnimationRecord _gM249Animation09D80Records[240] = {
#include "assets/m249_animation_09D80_records.inc"
};

static u16 _gM249Animation09D80Indices[20] = {
#include "assets/m249_animation_09D80_indices.inc"
};

static AnimationSet _gM249Animation09D80 = {
    _gM249Animation09D80Records,
    _gM249Animation09D80Indices,
    { NULL, _gM249Animation09D80Bank1, NULL, NULL, _gM249Animation09D80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0A828Bank1[19] = {
#include "assets/m249_animation_0A828_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0A828Bank4[269] = {
#include "assets/m249_animation_0A828_bank4.inc"
};

static AnimationRecord _gM249Animation0A828Records[336] = {
#include "assets/m249_animation_0A828_records.inc"
};

static u16 _gM249Animation0A828Indices[20] = {
#include "assets/m249_animation_0A828_indices.inc"
};

static AnimationSet _gM249Animation0A828 = {
    _gM249Animation0A828Records,
    _gM249Animation0A828Indices,
    { NULL, _gM249Animation0A828Bank1, NULL, NULL, _gM249Animation0A828Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0CC20Bank1[63] = {
#include "assets/m249_animation_0CC20_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0CC20Bank4[982] = {
#include "assets/m249_animation_0CC20_bank4.inc"
};

static AnimationRecord _gM249Animation0CC20Records[1111] = {
#include "assets/m249_animation_0CC20_records.inc"
};

static u16 _gM249Animation0CC20Indices[20] = {
#include "assets/m249_animation_0CC20_indices.inc"
};

static AnimationSet _gM249Animation0CC20 = {
    _gM249Animation0CC20Records,
    _gM249Animation0CC20Indices,
    { NULL, _gM249Animation0CC20Bank1, NULL, NULL, _gM249Animation0CC20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0D2BCBank1[11] = {
#include "assets/m249_animation_0D2BC_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0D2BCBank4[160] = {
#include "assets/m249_animation_0D2BC_bank4.inc"
};

static AnimationRecord _gM249Animation0D2BCRecords[210] = {
#include "assets/m249_animation_0D2BC_records.inc"
};

static u16 _gM249Animation0D2BCIndices[20] = {
#include "assets/m249_animation_0D2BC_indices.inc"
};

static AnimationSet _gM249Animation0D2BC = {
    _gM249Animation0D2BCRecords,
    _gM249Animation0D2BCIndices,
    { NULL, _gM249Animation0D2BCBank1, NULL, NULL, _gM249Animation0D2BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0DC30Bank1[17] = {
#include "assets/m249_animation_0DC30_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0DC30Bank4[244] = {
#include "assets/m249_animation_0DC30_bank4.inc"
};

static AnimationRecord _gM249Animation0DC30Records[290] = {
#include "assets/m249_animation_0DC30_records.inc"
};

static u16 _gM249Animation0DC30Indices[20] = {
#include "assets/m249_animation_0DC30_indices.inc"
};

static AnimationSet _gM249Animation0DC30 = {
    _gM249Animation0DC30Records,
    _gM249Animation0DC30Indices,
    { NULL, _gM249Animation0DC30Bank1, NULL, NULL, _gM249Animation0DC30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0E344Bank1[13] = {
#include "assets/m249_animation_0E344_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0E344Bank4[177] = {
#include "assets/m249_animation_0E344_bank4.inc"
};

static AnimationRecord _gM249Animation0E344Records[217] = {
#include "assets/m249_animation_0E344_records.inc"
};

static u16 _gM249Animation0E344Indices[20] = {
#include "assets/m249_animation_0E344_indices.inc"
};

static AnimationSet _gM249Animation0E344 = {
    _gM249Animation0E344Records,
    _gM249Animation0E344Indices,
    { NULL, _gM249Animation0E344Bank1, NULL, NULL, _gM249Animation0E344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0EBFCBank1[18] = {
#include "assets/m249_animation_0EBFC_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0EBFCBank4[201] = {
#include "assets/m249_animation_0EBFC_bank4.inc"
};

static AnimationRecord _gM249Animation0EBFCRecords[283] = {
#include "assets/m249_animation_0EBFC_records.inc"
};

static u16 _gM249Animation0EBFCIndices[20] = {
#include "assets/m249_animation_0EBFC_indices.inc"
};

static AnimationSet _gM249Animation0EBFC = {
    _gM249Animation0EBFCRecords,
    _gM249Animation0EBFCIndices,
    { NULL, _gM249Animation0EBFCBank1, NULL, NULL, _gM249Animation0EBFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0F39CBank1[16] = {
#include "assets/m249_animation_0F39C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0F39CBank4[157] = {
#include "assets/m249_animation_0F39C_bank4.inc"
};

static AnimationRecord _gM249Animation0F39CRecords[263] = {
#include "assets/m249_animation_0F39C_records.inc"
};

static u16 _gM249Animation0F39CIndices[20] = {
#include "assets/m249_animation_0F39C_indices.inc"
};

static AnimationSet _gM249Animation0F39C = {
    _gM249Animation0F39CRecords,
    _gM249Animation0F39CIndices,
    { NULL, _gM249Animation0F39CBank1, NULL, NULL, _gM249Animation0F39CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0FD70Bank1[25] = {
#include "assets/m249_animation_0FD70_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0FD70Bank4[223] = {
#include "assets/m249_animation_0FD70_bank4.inc"
};

static AnimationRecord _gM249Animation0FD70Records[311] = {
#include "assets/m249_animation_0FD70_records.inc"
};

static u16 _gM249Animation0FD70Indices[20] = {
#include "assets/m249_animation_0FD70_indices.inc"
};

static AnimationSet _gM249Animation0FD70 = {
    _gM249Animation0FD70Records,
    _gM249Animation0FD70Indices,
    { NULL, _gM249Animation0FD70Bank1, NULL, NULL, _gM249Animation0FD70Bank4, NULL, NULL, NULL },
};

AnimationBank D_m249_8012CF58 = { { {
    NULL,
    &_gM249Animation00E68,
    &_gM249Animation0EBFC,
    &_gM249Animation0F39C,
    &_gM249Animation0FD70,
    &_gM249Animation01D6C,
    &_gM249Animation025D0,
    &_gM249Animation0DC30,
    &_gM249Animation0E344,
    &_gM249Animation0964C,
    &_gM249Animation0D2BC,
    &_gM249Animation0D2BC,
    &_gM249Animation0A828,
    &_gM249Animation09D80,
    &_gM249Animation0CC20,
    &_gM249Animation0CC20,
    &_gM249Animation05234,
    &_gM249Animation05554,
    &_gM249Animation05D18,
    &_gM249Animation0150C,
    &_gM249Animation0CC20,
    &_gM249Animation00E68,
    &_gM249Animation00E68,
    &_gM249Animation06FB0,
    &_gM249Animation08244,
    &_gM249Animation07B28,
    &_gM249Animation042B4,
    &_gM249Animation044B0,
    &_gM249Animation04788,
    &_gM249Animation04A2C,
    &_gM249Animation04C2C,
    &_gM249Animation04F80,
    &_gM249Animation086C4,
    &_gM249Animation0889C,
    &_gM249Animation086C4,
    &_gM249Animation0889C,
    &_gM249Animation03040,
    &_gM249Animation037C8,
    &_gM249Animation03E2C,
    &_gM249Animation03A9C,
    &_gM249Animation028E4,
    &_gM249Animation00E68,
    &_gM249Animation08E00,
    &_gM249Animation08FF4,
    &_gM249Animation09474,
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
