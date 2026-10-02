#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/session.h"
#include "main/task_types.h"

// This package contains no code. Its retained data shares the existing ID TU.

static const s32 packageId = PKG_ID;

typedef struct {
    u16   flags;
    u16   priority;
    void  (*callback)(Enemy*, Task*);
    void* argument;
} Actor142600RetainedTaskSeed;
STATIC_ASSERT_SIZEOF(Actor142600RetainedTaskSeed, 12);

// The copy descriptors request 32 words. Both banks include the following
// animation arguments in their backing allocation, as the original copies do.
typedef union {
    struct {
        AnimationSet*        sets[7];
        AnimationPlayRequest arguments[8];
    } data;
    s32 words[47];
} Actor142600AnimationBankA;
STATIC_ASSERT_SIZEOF(Actor142600AnimationBankA, 188);

typedef union {
    struct {
        AnimationSet*        sets[13];
        AnimationPlayRequest arguments[17];
    } data;
    s32 words[98];
} Actor142600AnimationBankB;
STATIC_ASSERT_SIZEOF(Actor142600AnimationBankB, 392);

extern Actor142600AnimationBankA D_actor_142600_80135E30;
extern Actor142600AnimationBankB D_actor_142600_80135EEC;

extern Actor142600AnimationBankA D_actor_142600_80135E30;
extern Actor142600AnimationBankB D_actor_142600_80135EEC;
extern AnimationBankCopyRequest  D_actor_142600_80136074;
extern AnimationBankCopyRequest  D_actor_142600_8013607C;

static AnimationPackedPose _gActor142600Animation00224Bank1[3] = {
#include "assets/actor_142600_animation_00224_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation00224Bank4[27] = {
#include "assets/actor_142600_animation_00224_bank4.inc"
};

static AnimationRecord _gActor142600Animation00224Records[90] = {
#include "assets/actor_142600_animation_00224_records.inc"
};

static u16 _gActor142600Animation00224Indices[20] = {
#include "assets/actor_142600_animation_00224_indices.inc"
};

static AnimationSet _gActor142600Animation00224 = {
    _gActor142600Animation00224Records,
    _gActor142600Animation00224Indices,
    { NULL, _gActor142600Animation00224Bank1, NULL, NULL, _gActor142600Animation00224Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation00464Bank1[2] = {
#include "assets/actor_142600_animation_00464_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation00464Bank4[41] = {
#include "assets/actor_142600_animation_00464_bank4.inc"
};

static AnimationRecord _gActor142600Animation00464Records[77] = {
#include "assets/actor_142600_animation_00464_records.inc"
};

static u16 _gActor142600Animation00464Indices[20] = {
#include "assets/actor_142600_animation_00464_indices.inc"
};

static AnimationSet _gActor142600Animation00464 = {
    _gActor142600Animation00464Records,
    _gActor142600Animation00464Indices,
    { NULL, _gActor142600Animation00464Bank1, NULL, NULL, _gActor142600Animation00464Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation006A4Bank1[2] = {
#include "assets/actor_142600_animation_006A4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation006A4Bank4[41] = {
#include "assets/actor_142600_animation_006A4_bank4.inc"
};

static AnimationRecord _gActor142600Animation006A4Records[77] = {
#include "assets/actor_142600_animation_006A4_records.inc"
};

static u16 _gActor142600Animation006A4Indices[20] = {
#include "assets/actor_142600_animation_006A4_indices.inc"
};

static AnimationSet _gActor142600Animation006A4 = {
    _gActor142600Animation006A4Records,
    _gActor142600Animation006A4Indices,
    { NULL, _gActor142600Animation006A4Bank1, NULL, NULL, _gActor142600Animation006A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation008F4Bank1[2] = {
#include "assets/actor_142600_animation_008F4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation008F4Bank4[30] = {
#include "assets/actor_142600_animation_008F4_bank4.inc"
};

static AnimationRecord _gActor142600Animation008F4Records[92] = {
#include "assets/actor_142600_animation_008F4_records.inc"
};

static u16 _gActor142600Animation008F4Indices[20] = {
#include "assets/actor_142600_animation_008F4_indices.inc"
};

static AnimationSet _gActor142600Animation008F4 = {
    _gActor142600Animation008F4Records,
    _gActor142600Animation008F4Indices,
    { NULL, _gActor142600Animation008F4Bank1, NULL, NULL, _gActor142600Animation008F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation00D38Bank1[2] = {
#include "assets/actor_142600_animation_00D38_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation00D38Bank4[102] = {
#include "assets/actor_142600_animation_00D38_bank4.inc"
};

static AnimationRecord _gActor142600Animation00D38Records[145] = {
#include "assets/actor_142600_animation_00D38_records.inc"
};

static u16 _gActor142600Animation00D38Indices[20] = {
#include "assets/actor_142600_animation_00D38_indices.inc"
};

static AnimationSet _gActor142600Animation00D38 = {
    _gActor142600Animation00D38Records,
    _gActor142600Animation00D38Indices,
    { NULL, _gActor142600Animation00D38Bank1, NULL, NULL, _gActor142600Animation00D38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation010E4Bank1[5] = {
#include "assets/actor_142600_animation_010E4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation010E4Bank4[71] = {
#include "assets/actor_142600_animation_010E4_bank4.inc"
};

static AnimationRecord _gActor142600Animation010E4Records[129] = {
#include "assets/actor_142600_animation_010E4_records.inc"
};

static u16 _gActor142600Animation010E4Indices[20] = {
#include "assets/actor_142600_animation_010E4_indices.inc"
};

static AnimationSet _gActor142600Animation010E4 = {
    _gActor142600Animation010E4Records,
    _gActor142600Animation010E4Indices,
    { NULL, _gActor142600Animation010E4Bank1, NULL, NULL, _gActor142600Animation010E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation013D4Bank1[2] = {
#include "assets/actor_142600_animation_013D4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation013D4Bank4[59] = {
#include "assets/actor_142600_animation_013D4_bank4.inc"
};

static AnimationRecord _gActor142600Animation013D4Records[103] = {
#include "assets/actor_142600_animation_013D4_records.inc"
};

static u16 _gActor142600Animation013D4Indices[20] = {
#include "assets/actor_142600_animation_013D4_indices.inc"
};

static AnimationSet _gActor142600Animation013D4 = {
    _gActor142600Animation013D4Records,
    _gActor142600Animation013D4Indices,
    { NULL, _gActor142600Animation013D4Bank1, NULL, NULL, _gActor142600Animation013D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation016C8Bank1[4] = {
#include "assets/actor_142600_animation_016C8_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation016C8Bank4[54] = {
#include "assets/actor_142600_animation_016C8_bank4.inc"
};

static AnimationRecord _gActor142600Animation016C8Records[103] = {
#include "assets/actor_142600_animation_016C8_records.inc"
};

static u16 _gActor142600Animation016C8Indices[20] = {
#include "assets/actor_142600_animation_016C8_indices.inc"
};

static AnimationSet _gActor142600Animation016C8 = {
    _gActor142600Animation016C8Records,
    _gActor142600Animation016C8Indices,
    { NULL, _gActor142600Animation016C8Bank1, NULL, NULL, _gActor142600Animation016C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation01884Bank1[2] = {
#include "assets/actor_142600_animation_01884_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation01884Bank4[20] = {
#include "assets/actor_142600_animation_01884_bank4.inc"
};

static AnimationRecord _gActor142600Animation01884Records[65] = {
#include "assets/actor_142600_animation_01884_records.inc"
};

static u16 _gActor142600Animation01884Indices[20] = {
#include "assets/actor_142600_animation_01884_indices.inc"
};

static AnimationSet _gActor142600Animation01884 = {
    _gActor142600Animation01884Records,
    _gActor142600Animation01884Indices,
    { NULL, _gActor142600Animation01884Bank1, NULL, NULL, _gActor142600Animation01884Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation01D98Bank1[8] = {
#include "assets/actor_142600_animation_01D98_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation01D98Bank4[116] = {
#include "assets/actor_142600_animation_01D98_bank4.inc"
};

static AnimationRecord _gActor142600Animation01D98Records[165] = {
#include "assets/actor_142600_animation_01D98_records.inc"
};

static u16 _gActor142600Animation01D98Indices[20] = {
#include "assets/actor_142600_animation_01D98_indices.inc"
};

static AnimationSet _gActor142600Animation01D98 = {
    _gActor142600Animation01D98Records,
    _gActor142600Animation01D98Indices,
    { NULL, _gActor142600Animation01D98Bank1, NULL, NULL, _gActor142600Animation01D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02000Bank1[2] = {
#include "assets/actor_142600_animation_02000_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02000Bank4[42] = {
#include "assets/actor_142600_animation_02000_bank4.inc"
};

static AnimationRecord _gActor142600Animation02000Records[86] = {
#include "assets/actor_142600_animation_02000_records.inc"
};

static u16 _gActor142600Animation02000Indices[20] = {
#include "assets/actor_142600_animation_02000_indices.inc"
};

static AnimationSet _gActor142600Animation02000 = {
    _gActor142600Animation02000Records,
    _gActor142600Animation02000Indices,
    { NULL, _gActor142600Animation02000Bank1, NULL, NULL, _gActor142600Animation02000Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02468Bank1[10] = {
#include "assets/actor_142600_animation_02468_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02468Bank4[96] = {
#include "assets/actor_142600_animation_02468_bank4.inc"
};

static AnimationRecord _gActor142600Animation02468Records[136] = {
#include "assets/actor_142600_animation_02468_records.inc"
};

static u16 _gActor142600Animation02468Indices[20] = {
#include "assets/actor_142600_animation_02468_indices.inc"
};

static AnimationSet _gActor142600Animation02468 = {
    _gActor142600Animation02468Records,
    _gActor142600Animation02468Indices,
    { NULL, _gActor142600Animation02468Bank1, NULL, NULL, _gActor142600Animation02468Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation0276CBank1[5] = {
#include "assets/actor_142600_animation_0276C_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation0276CBank4[55] = {
#include "assets/actor_142600_animation_0276C_bank4.inc"
};

static AnimationRecord _gActor142600Animation0276CRecords[103] = {
#include "assets/actor_142600_animation_0276C_records.inc"
};

static u16 _gActor142600Animation0276CIndices[20] = {
#include "assets/actor_142600_animation_0276C_indices.inc"
};

static AnimationSet _gActor142600Animation0276C = {
    _gActor142600Animation0276CRecords,
    _gActor142600Animation0276CIndices,
    { NULL, _gActor142600Animation0276CBank1, NULL, NULL, _gActor142600Animation0276CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02B40Bank1[8] = {
#include "assets/actor_142600_animation_02B40_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02B40Bank4[84] = {
#include "assets/actor_142600_animation_02B40_bank4.inc"
};

static AnimationRecord _gActor142600Animation02B40Records[117] = {
#include "assets/actor_142600_animation_02B40_records.inc"
};

static u16 _gActor142600Animation02B40Indices[20] = {
#include "assets/actor_142600_animation_02B40_indices.inc"
};

static AnimationSet _gActor142600Animation02B40 = {
    _gActor142600Animation02B40Records,
    _gActor142600Animation02B40Indices,
    { NULL, _gActor142600Animation02B40Bank1, NULL, NULL, _gActor142600Animation02B40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02F30Bank1[7] = {
#include "assets/actor_142600_animation_02F30_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02F30Bank4[62] = {
#include "assets/actor_142600_animation_02F30_bank4.inc"
};

static AnimationRecord _gActor142600Animation02F30Records[149] = {
#include "assets/actor_142600_animation_02F30_records.inc"
};

static u16 _gActor142600Animation02F30Indices[20] = {
#include "assets/actor_142600_animation_02F30_indices.inc"
};

static AnimationSet _gActor142600Animation02F30 = {
    _gActor142600Animation02F30Records,
    _gActor142600Animation02F30Indices,
    { NULL, _gActor142600Animation02F30Bank1, NULL, NULL, _gActor142600Animation02F30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation0329CBank1[7] = {
#include "assets/actor_142600_animation_0329C_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation0329CBank4[74] = {
#include "assets/actor_142600_animation_0329C_bank4.inc"
};

static AnimationRecord _gActor142600Animation0329CRecords[104] = {
#include "assets/actor_142600_animation_0329C_records.inc"
};

static u16 _gActor142600Animation0329CIndices[20] = {
#include "assets/actor_142600_animation_0329C_indices.inc"
};

static AnimationSet _gActor142600Animation0329C = {
    _gActor142600Animation0329CRecords,
    _gActor142600Animation0329CIndices,
    { NULL, _gActor142600Animation0329CBank1, NULL, NULL, _gActor142600Animation0329CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03784Bank1[6] = {
#include "assets/actor_142600_animation_03784_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03784Bank4[110] = {
#include "assets/actor_142600_animation_03784_bank4.inc"
};

static AnimationRecord _gActor142600Animation03784Records[166] = {
#include "assets/actor_142600_animation_03784_records.inc"
};

static u16 _gActor142600Animation03784Indices[20] = {
#include "assets/actor_142600_animation_03784_indices.inc"
};

static AnimationSet _gActor142600Animation03784 = {
    _gActor142600Animation03784Records,
    _gActor142600Animation03784Indices,
    { NULL, _gActor142600Animation03784Bank1, NULL, NULL, _gActor142600Animation03784Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03AECBank1[5] = {
#include "assets/actor_142600_animation_03AEC_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03AECBank4[74] = {
#include "assets/actor_142600_animation_03AEC_bank4.inc"
};

static AnimationRecord _gActor142600Animation03AECRecords[109] = {
#include "assets/actor_142600_animation_03AEC_records.inc"
};

static u16 _gActor142600Animation03AECIndices[20] = {
#include "assets/actor_142600_animation_03AEC_indices.inc"
};

static AnimationSet _gActor142600Animation03AEC = {
    _gActor142600Animation03AECRecords,
    _gActor142600Animation03AECIndices,
    { NULL, _gActor142600Animation03AECBank1, NULL, NULL, _gActor142600Animation03AECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03CC4Bank1[2] = {
#include "assets/actor_142600_animation_03CC4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03CC4Bank4[16] = {
#include "assets/actor_142600_animation_03CC4_bank4.inc"
};

static AnimationRecord _gActor142600Animation03CC4Records[76] = {
#include "assets/actor_142600_animation_03CC4_records.inc"
};

static u16 _gActor142600Animation03CC4Indices[20] = {
#include "assets/actor_142600_animation_03CC4_indices.inc"
};

static AnimationSet _gActor142600Animation03CC4 = {
    _gActor142600Animation03CC4Records,
    _gActor142600Animation03CC4Indices,
    { NULL, _gActor142600Animation03CC4Bank1, NULL, NULL, _gActor142600Animation03CC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03FDCBank1[5] = {
#include "assets/actor_142600_animation_03FDC_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03FDCBank4[66] = {
#include "assets/actor_142600_animation_03FDC_bank4.inc"
};

static AnimationRecord _gActor142600Animation03FDCRecords[97] = {
#include "assets/actor_142600_animation_03FDC_records.inc"
};

static u16 _gActor142600Animation03FDCIndices[20] = {
#include "assets/actor_142600_animation_03FDC_indices.inc"
};

static AnimationSet _gActor142600Animation03FDC = {
    _gActor142600Animation03FDCRecords,
    _gActor142600Animation03FDCIndices,
    { NULL, _gActor142600Animation03FDCBank1, NULL, NULL, _gActor142600Animation03FDCBank4, NULL, NULL, NULL },
};

Actor142600RetainedTaskSeed D_actor_142600_80135E24 = { 0, 192, Gp_DestroyEnemy, NULL };

Actor142600AnimationBankA D_actor_142600_80135E30 = { .data = { { &_gActor142600Animation00224, &_gActor142600Animation00464, &_gActor142600Animation006A4, &_gActor142600Animation008F4, &_gActor142600Animation00D38, &_gActor142600Animation010E4, &_gActor142600Animation013D4 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

Actor142600AnimationBankB D_actor_142600_80135EEC = { .data = { { &_gActor142600Animation016C8, &_gActor142600Animation01884, &_gActor142600Animation01D98, &_gActor142600Animation02000, &_gActor142600Animation02B40, &_gActor142600Animation02F30, &_gActor142600Animation0329C, &_gActor142600Animation0276C, &_gActor142600Animation03784, &_gActor142600Animation02468, &_gActor142600Animation03AEC, &_gActor142600Animation03CC4, &_gActor142600Animation03FDC }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationBankCopyRequest D_actor_142600_80136074 = { { .words = D_actor_142600_80135EEC.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

AnimationBankCopyRequest D_actor_142600_8013607C = { { .words = D_actor_142600_80135E30.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_142600_80136084 = { { -5860, 0, 1540, 0 }, { 0, 2275, 0, 0 } };

ActorTransform D_actor_142600_8013609C = { { -6440, 0, 110, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_142600_801360B4 = { { -6000, 0, 948, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_142600_801360CC = { { -6442, 0, 1540, 0 }, { 0, 2048, 0, 0 } };

EvsCommand D_actor_142600_801360E4[76] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_142600_80136074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_142600_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142600_80136084 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142600_8013609C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[11] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[14] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[15] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[12] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[13] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142600_801360B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142600_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_142600_80136804[15] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142600_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142600_801360B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};
