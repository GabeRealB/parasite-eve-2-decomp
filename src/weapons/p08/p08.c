#include "weapons/p08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
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

void p08AttackState(Task* playerTask)
{
    enum {
        P08_PHASE_PREPARE             = 0,
        P08_PHASE_WAIT_READY          = 1,
        P08_PHASE_FIRE                = 2,
        P08_PHASE_IMPACT              = 3,
        P08_PHASE_RECOVER             = 4,
        P08_PLAYER_ATTACK_STATE       = 4,
        P08_ANIMATION_READY           = 9,
        P08_ANIMATION_PRIMARY         = 0xA,
        P08_READY_BLEND_FRAMES        = 1,
        P08_MOVING_READY_BLEND_FRAMES = 5,
        P08_IMPACT_SOUND              = SOUND_COMMON(0x17),
        P08_CANCEL_FRAMES             = 0xB,
        P08_PRIMARY_SOUND_BASE        = SOUND_WEAPON(0, 4),
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case P08_PHASE_PREPARE:
            actor->state             = P08_PLAYER_ATTACK_STATE;
            actor->statePhase        = P08_PHASE_WAIT_READY;
            readyBlendFrames         = P08_READY_BLEND_FRAMES;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->attackCancelTicks = P08_CANCEL_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = P08_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, P08_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case P08_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case P08_PHASE_FIRE:
            actor->statePhase++;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM(WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords, P08_PRIMARY_SOUND_BASE | (WEAPON_ID << 16), 1);
            effectSpawn(P08_FLASH_EFFECT,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        P08_FLASH_WEAPON, NULL);
            playerActorResetChildSlots(playerTask, P08_ANIMATION_PRIMARY);
            break;
        case P08_PHASE_IMPACT:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, P08_IMPACT_SOUND, 1);
            }
            /* fallthrough */
        case P08_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = P08_FIELD_940;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/* Each package carries its own model. */
#if WEAPON_ID == 0x4
/// Identity transform for the P08 model's self-parented root.
static const TmdBone _gP08WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/p08_model_00440_skeleton.inc"
};

/// Vertex count for the P08 model's only part.
static const u32 _gP08WeaponModelPartVerts[] __attribute__((section(".data"))) = {
#include "assets/p08_model_00440_partVerts.inc"
};

/// P08 vertices in integer coordinates local to the weapon root.
static SVECTOR _gP08WeaponModelVerts[] = {
#include "assets/p08_model_00440_verts.inc"
};

/// Part-local P08 lighting normals, indexed independently of the vertices.
static SVECTOR _gP08WeaponModelNormals[] = {
#include "assets/p08_model_00440_normals.inc"
};

/// Writable P08 draw stream: four textured Gouraud triangles and 24 quads.
///
/// Model creation fills its handler slots. The sole skeletal group is followed
/// by an empty group and `TMD_STREAM_END`.
static u32 _gP08WeaponModelStream[] = {
#include "assets/p08_model_00440_stream.inc"
};

TmdSource gP08WeaponModel = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = 4 * sizeof(POLY_GT3) + 24 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gP08WeaponModelSkeleton),
    .partVertexCounts    = _gP08WeaponModelPartVerts,
    .verts               = _gP08WeaponModelVerts,
    .normals             = _gP08WeaponModelNormals,
    .skeleton            = _gP08WeaponModelSkeleton,
    .stream              = _gP08WeaponModelStream,
};
STATIC_ASSERT(ARRAY_SIZE(_gP08WeaponModelPartVerts) == ARRAY_SIZE(_gP08WeaponModelSkeleton), p08_weapon_model_part_counts);
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
/// Identity transform for the Mongoose model's self-parented root.
static TmdBone _gMongooseModelSkeleton[] = {
#include "assets/mongoose_model_00450_skeleton.inc"
};

/// Vertex count for the Mongoose model's only part.
static u32 _gMongooseModelPartVerts[] = {
#include "assets/mongoose_model_00450_partVerts.inc"
};

/// Mongoose vertices in integer coordinates local to the weapon root.
static SVECTOR _gMongooseModelVerts[] = {
#include "assets/mongoose_model_00450_verts.inc"
};

/// Part-local Mongoose lighting normals, indexed independently of the vertices.
static SVECTOR _gMongooseModelNormals[] = {
#include "assets/mongoose_model_00450_normals.inc"
};

/// Writable Mongoose draw stream: four textured Gouraud triangles and 24 quads.
///
/// Model creation fills its handler slots. The sole skeletal group is followed
/// by an empty group and `TMD_STREAM_END`.
static u32 _gMongooseModelStream[] = {
#include "assets/mongoose_model_00450_stream.inc"
};

TmdSource gMongooseModel = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = 4 * sizeof(POLY_GT3) + 24 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gMongooseModelSkeleton),
    .partVertexCounts    = _gMongooseModelPartVerts,
    .verts               = _gMongooseModelVerts,
    .normals             = _gMongooseModelNormals,
    .skeleton            = _gMongooseModelSkeleton,
    .stream              = _gMongooseModelStream,
};
STATIC_ASSERT(ARRAY_SIZE(_gMongooseModelPartVerts) == ARRAY_SIZE(_gMongooseModelSkeleton), mongoose_model_part_counts);
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
/// Idle encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gMongooseIdleAnimationBank1[] = {
#include "assets/mongoose_animation_00928_bank1.inc"
};

/// Idle encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gMongooseIdleAnimationBank4[] = {
#include "assets/mongoose_animation_00928_bank4.inc"
};

/// Idle keyframes and controls, holding each part in a two-frame loop.
static AnimationRecord _gMongooseIdleAnimationRecords[] = {
#include "assets/mongoose_animation_00928_records.inc"
};

/// Nineteen part-track starts in `_gMongooseIdleAnimationRecords`, plus a retained final halfword.
static u16 _gMongooseIdleAnimationIndices[] = {
#include "assets/mongoose_animation_00928_indices.inc"
};

/// Standing pose of the player using the Mongoose animation bank.
///
/// Native-bank set 1 supplies ordinary idle; sets 21, 22 and 41 reuse it.
/// Its nineteen part tracks and private arrays stay loaded during playback.
static AnimationSet _gMongooseIdleAnimation = {
    .records           = _gMongooseIdleAnimationRecords,
    .trackStartIndices = _gMongooseIdleAnimationIndices,
    .poseBanks         = { [1] = _gMongooseIdleAnimationBank1, [4] = _gMongooseIdleAnimationBank4 },
};

/// Unarmed-walk encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gMongooseUnarmedWalkAnimationBank1[] = {
#include "assets/mongoose_animation_00FCC_bank1.inc"
};

/// Unarmed-walk encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gMongooseUnarmedWalkAnimationBank4[] = {
#include "assets/mongoose_animation_00FCC_bank4.inc"
};

/// Unarmed-walk keyframes, cues and loop controls; durations count normal-rate frames.
static AnimationRecord _gMongooseUnarmedWalkAnimationRecords[] = {
#include "assets/mongoose_animation_00FCC_records.inc"
};

/// Nineteen part-track starts in `_gMongooseUnarmedWalkAnimationRecords`, plus a retained final halfword.
static u16 _gMongooseUnarmedWalkAnimationIndices[] = {
#include "assets/mongoose_animation_00FCC_indices.inc"
};

/// Player walk used when no held-weapon model occupies equipment slot 1.
///
/// Native-bank set 19 supplies nineteen player-part tracks. Moving tracks
/// repeat 29-frame loops after their initial keyframes; fixed tracks repeat
/// two-frame holds. This set's private arrays stay loaded during playback.
static AnimationSet _gMongooseUnarmedWalkAnimation = {
    .records           = _gMongooseUnarmedWalkAnimationRecords,
    .trackStartIndices = _gMongooseUnarmedWalkAnimationIndices,
    .poseBanks         = { [1] = _gMongooseUnarmedWalkAnimationBank1, [4] = _gMongooseUnarmedWalkAnimationBank4 },
};

/// Forward-walk encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gMongooseWalkAnimationBank1[] = {
#include "assets/mongoose_animation_01918_bank1.inc"
};

/// Forward-walk encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gMongooseWalkAnimationBank4[] = {
#include "assets/mongoose_animation_01918_bank4.inc"
};

/// Forward-walk keyframes, cues and loop controls; durations count normal-rate frames.
static AnimationRecord _gMongooseWalkAnimationRecords[] = {
#include "assets/mongoose_animation_01918_records.inc"
};

/// Nineteen part-track starts in `_gMongooseWalkAnimationRecords`, plus a retained final halfword.
static u16 _gMongooseWalkAnimationIndices[] = {
#include "assets/mongoose_animation_01918_indices.inc"
};

/// Player's forward walk while holding the Mongoose.
///
/// Native-bank set 2 supplies nineteen player-part tracks. Moving tracks
/// repeat 30-frame loops after their initial keyframes; fixed tracks repeat
/// two-frame holds. This set's private arrays stay loaded during playback.
static AnimationSet _gMongooseWalkAnimation = {
    .records           = _gMongooseWalkAnimationRecords,
    .trackStartIndices = _gMongooseWalkAnimationIndices,
    .poseBanks         = { [1] = _gMongooseWalkAnimationBank1, [4] = _gMongooseWalkAnimationBank4 },
};

/// Backward-walk encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gMongooseBackwardWalkAnimationBank1[] = {
#include "assets/mongoose_animation_021F0_bank1.inc"
};

/// Backward-walk encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gMongooseBackwardWalkAnimationBank4[] = {
#include "assets/mongoose_animation_021F0_bank4.inc"
};

/// Backward-walk keyframes, cues and loop controls; durations count normal-rate frames.
static AnimationRecord _gMongooseBackwardWalkAnimationRecords[] = {
#include "assets/mongoose_animation_021F0_records.inc"
};

/// Nineteen part-track starts in `_gMongooseBackwardWalkAnimationRecords`, plus a retained final halfword.
static u16 _gMongooseBackwardWalkAnimationIndices[] = {
#include "assets/mongoose_animation_021F0_indices.inc"
};

/// Player's backward walk while using the Mongoose animation bank.
///
/// Native-bank set 3 supplies nineteen player-part tracks. Moving tracks
/// repeat 33-frame loops after their initial keyframes; fixed tracks repeat
/// two-frame holds. This set's private arrays stay loaded during playback.
static AnimationSet _gMongooseBackwardWalkAnimation = {
    .records           = _gMongooseBackwardWalkAnimationRecords,
    .trackStartIndices = _gMongooseBackwardWalkAnimationIndices,
    .poseBanks         = { [1] = _gMongooseBackwardWalkAnimationBank1, [4] = _gMongooseBackwardWalkAnimationBank4 },
};

/// Run encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gMongooseRunAnimationBank1[] = {
#include "assets/mongoose_animation_02924_bank1.inc"
};

/// Run encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gMongooseRunAnimationBank4[] = {
#include "assets/mongoose_animation_02924_bank4.inc"
};

/// Run keyframes, cues and loop controls; durations count normal-rate frames.
static AnimationRecord _gMongooseRunAnimationRecords[] = {
#include "assets/mongoose_animation_02924_records.inc"
};

/// Nineteen part-track starts in `_gMongooseRunAnimationRecords`, plus a retained final halfword.
static u16 _gMongooseRunAnimationIndices[] = {
#include "assets/mongoose_animation_02924_indices.inc"
};

/// Player's forward run while using the Mongoose animation bank.
///
/// Native-bank set 4 supplies nineteen player-part tracks. Moving tracks
/// repeat 19-frame loops after their initial keyframes; fixed tracks repeat
/// two-frame holds. This set's private arrays stay loaded during playback.
static AnimationSet _gMongooseRunAnimation = {
    .records           = _gMongooseRunAnimationRecords,
    .trackStartIndices = _gMongooseRunAnimationIndices,
    .poseBanks         = { [1] = _gMongooseRunAnimationBank1, [4] = _gMongooseRunAnimationBank4 },
};

/// Negative-turn encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseNegativeTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03184_bank1.inc"
};

/// Negative-turn encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseNegativeTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03184_bank4.inc"
};

/// Negative-turn keyframes and loop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseNegativeTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03184_records.inc"
};

/// Nineteen part-track starts in `_gMongooseNegativeTurnAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseNegativeTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03184_indices.inc"
};

/// Player's negative-yaw turn while using the Mongoose animation bank.
///
/// Native-bank set 5 supplies nineteen player-part tracks. Moving tracks
/// repeat 32-frame loops after their initial keyframes; fixed tracks repeat
/// two-frame holds. This set's private arrays stay loaded during playback.
static AnimationSet _gMongooseNegativeTurnAnimation = {
    .records           = _gMongooseNegativeTurnAnimationRecords,
    .trackStartIndices = _gMongooseNegativeTurnAnimationIndices,
    .poseBanks         = { [1] = _gMongooseNegativeTurnAnimationBank1, [4] = _gMongooseNegativeTurnAnimationBank4 },
};

/// Positive-turn encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongoosePositiveTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_039E8_bank1.inc"
};

/// Positive-turn encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongoosePositiveTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_039E8_bank4.inc"
};

/// Positive-turn keyframes and loop controls; durations count normal-rate frames.
static const AnimationRecord _gMongoosePositiveTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_039E8_records.inc"
};

/// Nineteen part-track starts in `_gMongoosePositiveTurnAnimationRecords`, plus a retained final halfword.
static const u16 _gMongoosePositiveTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_039E8_indices.inc"
};

/// Player's positive-yaw turn while using the Mongoose animation bank.
///
/// Native-bank set 6 supplies nineteen player-part tracks. Moving tracks
/// repeat 32-frame loops after their initial keyframes; fixed tracks repeat
/// two-frame holds. This set's private arrays stay loaded during playback.
static AnimationSet _gMongoosePositiveTurnAnimation = {
    .records           = _gMongoosePositiveTurnAnimationRecords,
    .trackStartIndices = _gMongoosePositiveTurnAnimationIndices,
    .poseBanks         = { [1] = _gMongoosePositiveTurnAnimationBank1, [4] = _gMongoosePositiveTurnAnimationBank4 },
};

/// Item-use encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03CFC_bank1.inc"
};

/// Item-use encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03CFC_bank4.inc"
};

/// Item-use keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03CFC_records.inc"
};

/// Nineteen part-track starts in `_gMongooseItemUseAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_03CFC_indices.inc"
};

/// Player's item-use presentation while using the Mongoose animation bank.
///
/// Native-bank set 40 supplies nineteen player-part tracks ending in stop
/// controls. Playback presents the action; item effects are applied separately.
/// This set's private arrays stay loaded during playback.
static AnimationSet _gMongooseItemUseAnimation = {
    .records           = _gMongooseItemUseAnimationRecords,
    .trackStartIndices = _gMongooseItemUseAnimationIndices,
    .poseBanks         = { [1] = _gMongooseItemUseAnimationBank1, [4] = _gMongooseItemUseAnimationBank4 },
};

/// Stair-ascent encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04458_bank1.inc"
};

/// Stair-ascent encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04458_bank4.inc"
};

/// Stair-ascent keyframes, footstep cues and loop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04458_records.inc"
};

/// Nineteen part-track starts in `_gMongooseStairAscentAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04458_indices.inc"
};

/// Player's repeating stair-ascent stride while using the Mongoose animation bank.
///
/// Native-bank set 36 supplies nineteen player-part tracks. Moving tracks
/// repeat 32-frame loops; fixed tracks repeat two-frame holds. Track 1 carries
/// footstep cues that gate stair motion. The private arrays stay loaded during playback.
static AnimationSet _gMongooseStairAscentAnimation = {
    .records           = _gMongooseStairAscentAnimationRecords,
    .trackStartIndices = _gMongooseStairAscentAnimationIndices,
    .poseBanks         = { [1] = _gMongooseStairAscentAnimationBank1, [4] = _gMongooseStairAscentAnimationBank4 },
};

/// Stair-descent encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04BE0_bank1.inc"
};

/// Stair-descent encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04BE0_bank4.inc"
};

/// Stair-descent keyframes, footstep cues and loop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04BE0_records.inc"
};

/// Nineteen part-track starts in `_gMongooseStairDescentAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04BE0_indices.inc"
};

/// Player's repeating stair-descent stride while using the Mongoose animation bank.
///
/// Native-bank set 37 supplies nineteen player-part tracks. Moving tracks
/// repeat 30-frame loops; fixed tracks repeat two-frame holds. Track 1 carries
/// footstep cues that gate stair motion. The private arrays stay loaded during playback.
static AnimationSet _gMongooseStairDescentAnimation = {
    .records           = _gMongooseStairDescentAnimationRecords,
    .trackStartIndices = _gMongooseStairDescentAnimationIndices,
    .poseBanks         = { [1] = _gMongooseStairDescentAnimationBank1, [4] = _gMongooseStairDescentAnimationBank4 },
};

/// Odd-count ascent-finish encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04EB4_bank1.inc"
};

/// Odd-count ascent-finish encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04EB4_bank4.inc"
};

/// Odd-count ascent-finish keyframes, footstep cue and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04EB4_records.inc"
};

/// Nineteen part-track starts in `_gMongooseStairAscentFinishOddAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_04EB4_indices.inc"
};

/// Player's ascent finish after an odd-count stair flight with the Mongoose bank.
///
/// Native-bank set 39 supplies nineteen player-part tracks ending in stop
/// controls. Track 1 carries the final footstep cue. This set's private arrays
/// stay loaded during playback.
static AnimationSet _gMongooseStairAscentFinishOddAnimation = {
    .records           = _gMongooseStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gMongooseStairAscentFinishOddAnimationIndices,
    .poseBanks         = { [1] = _gMongooseStairAscentFinishOddAnimationBank1, [4] = _gMongooseStairAscentFinishOddAnimationBank4 },
};

/// Pose-bank slots selected by the record encodings in these Mongoose clips.
enum {
    MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    MONGOOSE_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Even-count stair-ascent finish encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05244_bank1.inc"
};

/// Even-count stair-ascent finish encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05244_bank4.inc"
};

/// Even-count stair-ascent finish keyframes, footstep cue and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05244_records.inc"
};

/// Nineteen part-track starts in `_gMongooseStairAscentFinishEvenAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05244_indices.inc"
};

/// Player's ascent finish after an even-count stair flight with the Mongoose bank.
///
/// Native-bank set 38 supplies nineteen player-part tracks ending in stop
/// controls. Track 1 carries the final footstep cue. This set and its private
/// arrays must remain loaded during playback.
static AnimationSet _gMongooseStairAscentFinishEvenAnimation = {
    .records           = _gMongooseStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gMongooseStairAscentFinishEvenAnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseStairAscentFinishEvenAnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseStairAscentFinishEvenAnimationBank4 },
};

/// Family-0 Parasite Energy release encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_056CC_bank1.inc"
};

/// Family-0 Parasite Energy release encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_056CC_bank4.inc"
};

/// Family-0 Parasite Energy release keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_056CC_records.inc"
};

/// Nineteen part-track starts in `_gMongooseParasiteEnergyReleaseFamily0AnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_056CC_indices.inc"
};

/// Player's Parasite Energy release for clip family 0 with the Mongoose bank.
///
/// Native-bank set 26 starts the release/charge/cast sequence selected by
/// `GameActor::actionArgument` 0. Its nineteen part tracks stop before the
/// charge pose begins. This set and its private arrays stay loaded during playback.
static AnimationSet _gMongooseParasiteEnergyReleaseFamily0Animation = {
    .records           = _gMongooseParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gMongooseParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseParasiteEnergyReleaseFamily0AnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseParasiteEnergyReleaseFamily0AnimationBank4 },
};

/// Family-0 Parasite Energy charge encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_058C8_bank1.inc"
};

/// Family-0 Parasite Energy charge encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_058C8_bank4.inc"
};

/// Family-0 Parasite Energy charge keyframes and loop controls with retained stop records; durations count normal-rate frames.
static const AnimationRecord _gMongooseParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_058C8_records.inc"
};

/// Nineteen part-track starts in `_gMongooseParasiteEnergyChargeFamily0AnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_058C8_indices.inc"
};

/// Player's repeating Parasite Energy charge pose for family 0 with the Mongoose bank.
///
/// Native-bank set 27 follows the family-0 release while the attachment
/// countdown remains nonzero. Its nineteen tracks repeat two-frame pose loops;
/// set 28 replaces it for the cast. This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseParasiteEnergyChargeFamily0Animation = {
    .records           = _gMongooseParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gMongooseParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseParasiteEnergyChargeFamily0AnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseParasiteEnergyChargeFamily0AnimationBank4 },
};

/// Family-0 Parasite Energy cast encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05BA0_bank1.inc"
};

/// Family-0 Parasite Energy cast encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05BA0_bank4.inc"
};

/// Family-0 Parasite Energy cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05BA0_records.inc"
};

/// Nineteen part-track starts in `_gMongooseParasiteEnergyCastFamily0AnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05BA0_indices.inc"
};

/// Player's Parasite Energy cast for clip family 0 with the Mongoose bank.
///
/// Native-bank set 28 replaces the family-0 charge when its countdown ends.
/// Its nineteen part tracks stop to let the action resume the previous control
/// state. This set and its private arrays must remain loaded during playback.
static AnimationSet _gMongooseParasiteEnergyCastFamily0Animation = {
    .records           = _gMongooseParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gMongooseParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseParasiteEnergyCastFamily0AnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseParasiteEnergyCastFamily0AnimationBank4 },
};

/// Family-1 Parasite Energy release encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05E44_bank1.inc"
};

/// Family-1 Parasite Energy release encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05E44_bank4.inc"
};

/// Family-1 Parasite Energy release keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05E44_records.inc"
};

/// Nineteen part-track starts in `_gMongooseParasiteEnergyReleaseFamily1AnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_05E44_indices.inc"
};

/// Player's Parasite Energy release for clip family 1 with the Mongoose bank.
///
/// Native-bank set 29 starts the release/charge/cast sequence selected by
/// `GameActor::actionArgument` 1. Its nineteen part tracks stop before the
/// charge pose begins. This set and its private arrays stay loaded during playback.
static AnimationSet _gMongooseParasiteEnergyReleaseFamily1Animation = {
    .records           = _gMongooseParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gMongooseParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseParasiteEnergyReleaseFamily1AnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseParasiteEnergyReleaseFamily1AnimationBank4 },
};

/// Family-1 Parasite Energy charge encoding-1 poses in integer model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06044_bank1.inc"
};

/// Family-1 Parasite Energy charge encoding-4 rotations, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06044_bank4.inc"
};

/// Family-1 Parasite Energy charge keyframes and loop controls with retained stop records; durations count normal-rate frames.
static const AnimationRecord _gMongooseParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06044_records.inc"
};

/// Nineteen part-track starts in `_gMongooseParasiteEnergyChargeFamily1AnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06044_indices.inc"
};

/// Player's repeating Parasite Energy charge pose for family 1 with the Mongoose bank.
///
/// Native-bank set 30 follows the family-1 release while the attachment
/// countdown remains nonzero. Its nineteen tracks repeat two-frame pose loops;
/// set 31 replaces it for the cast. This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseParasiteEnergyChargeFamily1Animation = {
    .records           = _gMongooseParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gMongooseParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseParasiteEnergyChargeFamily1AnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseParasiteEnergyChargeFamily1AnimationBank4 },
};

/// Family-1 cast translation/rotation poses for parts 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06398_bank1.inc"
};

/// Family-1 cast rotation-only poses for parts 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06398_bank4.inc"
};

/// Family-1 cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06398_records.inc"
};

/// Nineteen part-track starts in `_gMongooseParasiteEnergyCastFamily1AnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_06398_indices.inc"
};

/// Player's finite family-1 Parasite Energy cast animation in the Mongoose bank.
///
/// Native-bank set 31 replaces the family-1 charge when its countdown ends.
/// The action waits for clip completion before restoring normal control.
/// This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseParasiteEnergyCastFamily1Animation = {
    .records           = _gMongooseParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gMongooseParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseParasiteEnergyCastFamily1AnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseParasiteEnergyCastFamily1AnimationBank4 },
};

/// Part-4 hit translation/rotation poses for parts 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gMongoosePart4HitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0664C_bank1.inc"
};

/// Part-4 hit rotation-only poses for parts 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongoosePart4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0664C_bank4.inc"
};

/// Part-4 hit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongoosePart4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0664C_records.inc"
};

/// Nineteen part-track starts in `_gMongoosePart4HitReactionAnimationRecords`, plus a retained final halfword.
static const u16 _gMongoosePart4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0664C_indices.inc"
};

/// Player's finite reaction to a hit on the model-part-4 collision sphere with the Mongoose bank.
///
/// Native-bank set 16 is blended into child tracks for hit region 1; other
/// body hits select set 17. This set and its arrays stay loaded during playback.
static AnimationSet _gMongoosePart4HitReactionAnimation = {
    .records           = _gMongoosePart4HitReactionAnimationRecords,
    .trackStartIndices = _gMongoosePart4HitReactionAnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongoosePart4HitReactionAnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongoosePart4HitReactionAnimationBank4 },
};

/// Other-body hit translation/rotation poses for parts 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseOtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0696C_bank1.inc"
};

/// Other-body hit rotation-only poses for parts 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseOtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0696C_bank4.inc"
};

/// Other-body hit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseOtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0696C_records.inc"
};

/// Nineteen part-track starts in `_gMongooseOtherBodyHitReactionAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseOtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_0696C_indices.inc"
};

/// Player's finite reaction to body hits outside the model-part-4 sphere with the Mongoose bank.
///
/// Native-bank set 17 is blended into child tracks for other-body hit regions.
/// This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseOtherBodyHitReactionAnimation = {
    .records           = _gMongooseOtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gMongooseOtherBodyHitReactionAnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseOtherBodyHitReactionAnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseOtherBodyHitReactionAnimationBank4 },
};

/// Death translation/rotation poses for parts 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseDeathAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_07130_bank1.inc"
};

/// Death rotation-only poses for parts 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseDeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_07130_bank4.inc"
};

/// Death keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gMongooseDeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_07130_records.inc"
};

/// Nineteen part-track starts in `_gMongooseDeathAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseDeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_07130_indices.inc"
};

/// Player's finite death animation in the Mongoose bank.
///
/// Native-bank set 18 is requested at zero HP with movement and turning stopped.
/// Tracks hold their final keyframes. This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseDeathAnimation = {
    .records           = _gMongooseDeathAnimationRecords,
    .trackStartIndices = _gMongooseDeathAnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseDeathAnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseDeathAnimationBank4 },
};

/// Healthy-idle translation/rotation poses for parts 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseHealthyIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_083C8_bank1.inc"
};

/// Healthy-idle rotation-only poses for parts 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseHealthyIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_083C8_bank4.inc"
};

/// Healthy-idle keyframes and loop controls with retained stops; durations count normal-rate frames.
static const AnimationRecord _gMongooseHealthyIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_083C8_records.inc"
};

/// Nineteen part-track starts in `_gMongooseHealthyIdleAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseHealthyIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_083C8_indices.inc"
};

/// Player's repeating healthy idle animation in the Mongoose bank.
///
/// Native-bank set 23 follows 300 idle ticks when HP exceeds half maximum HP.
/// Its introductory motion leads into repeating track tails.
/// This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseHealthyIdleAnimation = {
    .records           = _gMongooseHealthyIdleAnimationRecords,
    .trackStartIndices = _gMongooseHealthyIdleAnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseHealthyIdleAnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseHealthyIdleAnimationBank4 },
};

/// Critical-idle translation/rotation poses for parts 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gMongooseCriticalIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_08F40_bank1.inc"
};

/// Critical-idle rotation-only poses for parts 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMongooseCriticalIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_08F40_bank4.inc"
};

/// Critical-idle keyframes and loop controls with retained stops; durations count normal-rate frames.
static const AnimationRecord _gMongooseCriticalIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_08F40_records.inc"
};

/// Nineteen part-track starts in `_gMongooseCriticalIdleAnimationRecords`, plus a retained final halfword.
static const u16 _gMongooseCriticalIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mongoose_animation_08F40_indices.inc"
};

/// Player's repeating critical-health idle and paralysis animation in the Mongoose bank.
///
/// Native-bank set 25 follows 300 idle ticks at or below the truncated quarter-HP
/// threshold, and paralysis entry also blends every child track into this set.
/// Its introductory motion leads into repeating track tails.
/// This set and its arrays stay loaded during playback.
static AnimationSet _gMongooseCriticalIdleAnimation = {
    .records           = _gMongooseCriticalIdleAnimationRecords,
    .trackStartIndices = _gMongooseCriticalIdleAnimationIndices,
    .poseBanks         = { [MONGOOSE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMongooseCriticalIdleAnimationBank1,
                           [MONGOOSE_ANIMATION_PACKED_ROTATION_BANK]      = _gMongooseCriticalIdleAnimationBank4 },
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
    &_gMongooseIdleAnimation,
    &_gMongooseWalkAnimation,
    &_gMongooseBackwardWalkAnimation,
    &_gMongooseRunAnimation,
    &_gMongooseNegativeTurnAnimation,
    &_gMongoosePositiveTurnAnimation,
    &_gMongooseAnimation0D20C,
    &_gMongooseAnimation0D7D8,
    &_gMongooseAnimation0AA68,
    &_gMongooseAnimation0CB90,
    &_gMongooseAnimation0CB90,
    &_gMongooseAnimation0BD44,
    &_gMongooseAnimation0B158,
    &_gMongooseAnimation0C59C,
    &_gMongooseAnimation0C59C,
    &_gMongoosePart4HitReactionAnimation,
    &_gMongooseOtherBodyHitReactionAnimation,
    &_gMongooseDeathAnimation,
    &_gMongooseUnarmedWalkAnimation,
    &_gMongooseAnimation0C59C,
    &_gMongooseIdleAnimation,
    &_gMongooseIdleAnimation,
    &_gMongooseHealthyIdleAnimation,
    &_gMongooseAnimation0965C,
    &_gMongooseCriticalIdleAnimation,
    &_gMongooseParasiteEnergyReleaseFamily0Animation,
    &_gMongooseParasiteEnergyChargeFamily0Animation,
    &_gMongooseParasiteEnergyCastFamily0Animation,
    &_gMongooseParasiteEnergyReleaseFamily1Animation,
    &_gMongooseParasiteEnergyChargeFamily1Animation,
    &_gMongooseParasiteEnergyCastFamily1Animation,
    &_gMongooseAnimation09ADC,
    &_gMongooseAnimation09CB4,
    &_gMongooseAnimation09ADC,
    &_gMongooseAnimation09CB4,
    &_gMongooseStairAscentAnimation,
    &_gMongooseStairDescentAnimation,
    &_gMongooseStairAscentFinishEvenAnimation,
    &_gMongooseStairAscentFinishOddAnimation,
    &_gMongooseItemUseAnimation,
    &_gMongooseIdleAnimation,
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
