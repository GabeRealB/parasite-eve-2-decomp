#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"

#include "rooms/shelter_r47.h"

static void _actor143400SelectSceneCaptions(s32 enable);
void        func_actor_143400_80131E6C(void);
static void _actor143400SetSceneEvent(s8 sceneEvent);

static AnimationPackedPose _gActor143400Animation00358Bank1[6] = {
#include "assets/actor_143400_animation_00358_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation00358Bank4[46] = {
#include "assets/actor_143400_animation_00358_bank4.inc"
};

static AnimationRecord _gActor143400Animation00358Records[109] = {
#include "assets/actor_143400_animation_00358_records.inc"
};

static u16 _gActor143400Animation00358Indices[20] = {
#include "assets/actor_143400_animation_00358_indices.inc"
};

static AnimationSet _gActor143400Animation00358 = {
    _gActor143400Animation00358Records,
    _gActor143400Animation00358Indices,
    { NULL, _gActor143400Animation00358Bank1, NULL, NULL, _gActor143400Animation00358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation005A4Bank1[3] = {
#include "assets/actor_143400_animation_005A4_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation005A4Bank4[29] = {
#include "assets/actor_143400_animation_005A4_bank4.inc"
};

static AnimationRecord _gActor143400Animation005A4Records[89] = {
#include "assets/actor_143400_animation_005A4_records.inc"
};

static u16 _gActor143400Animation005A4Indices[20] = {
#include "assets/actor_143400_animation_005A4_indices.inc"
};

static AnimationSet _gActor143400Animation005A4 = {
    _gActor143400Animation005A4Records,
    _gActor143400Animation005A4Indices,
    { NULL, _gActor143400Animation005A4Bank1, NULL, NULL, _gActor143400Animation005A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation00918Bank1[4] = {
#include "assets/actor_143400_animation_00918_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation00918Bank4[69] = {
#include "assets/actor_143400_animation_00918_bank4.inc"
};

static AnimationRecord _gActor143400Animation00918Records[120] = {
#include "assets/actor_143400_animation_00918_records.inc"
};

static u16 _gActor143400Animation00918Indices[20] = {
#include "assets/actor_143400_animation_00918_indices.inc"
};

static AnimationSet _gActor143400Animation00918 = {
    _gActor143400Animation00918Records,
    _gActor143400Animation00918Indices,
    { NULL, _gActor143400Animation00918Bank1, NULL, NULL, _gActor143400Animation00918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation00AC4Bank1[2] = {
#include "assets/actor_143400_animation_00AC4_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation00AC4Bank4[24] = {
#include "assets/actor_143400_animation_00AC4_bank4.inc"
};

static AnimationRecord _gActor143400Animation00AC4Records[57] = {
#include "assets/actor_143400_animation_00AC4_records.inc"
};

static u16 _gActor143400Animation00AC4Indices[20] = {
#include "assets/actor_143400_animation_00AC4_indices.inc"
};

static AnimationSet _gActor143400Animation00AC4 = {
    _gActor143400Animation00AC4Records,
    _gActor143400Animation00AC4Indices,
    { NULL, _gActor143400Animation00AC4Bank1, NULL, NULL, _gActor143400Animation00AC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation00CDCBank1[2] = {
#include "assets/actor_143400_animation_00CDC_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation00CDCBank4[24] = {
#include "assets/actor_143400_animation_00CDC_bank4.inc"
};

static AnimationRecord _gActor143400Animation00CDCRecords[84] = {
#include "assets/actor_143400_animation_00CDC_records.inc"
};

static u16 _gActor143400Animation00CDCIndices[20] = {
#include "assets/actor_143400_animation_00CDC_indices.inc"
};

static AnimationSet _gActor143400Animation00CDC = {
    _gActor143400Animation00CDCRecords,
    _gActor143400Animation00CDCIndices,
    { NULL, _gActor143400Animation00CDCBank1, NULL, NULL, _gActor143400Animation00CDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation00EA8Bank1[3] = {
#include "assets/actor_143400_animation_00EA8_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation00EA8Bank4[29] = {
#include "assets/actor_143400_animation_00EA8_bank4.inc"
};

static AnimationRecord _gActor143400Animation00EA8Records[57] = {
#include "assets/actor_143400_animation_00EA8_records.inc"
};

static u16 _gActor143400Animation00EA8Indices[20] = {
#include "assets/actor_143400_animation_00EA8_indices.inc"
};

static AnimationSet _gActor143400Animation00EA8 = {
    _gActor143400Animation00EA8Records,
    _gActor143400Animation00EA8Indices,
    { NULL, _gActor143400Animation00EA8Bank1, NULL, NULL, _gActor143400Animation00EA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation011C0Bank1[2] = {
#include "assets/actor_143400_animation_011C0_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation011C0Bank4[62] = {
#include "assets/actor_143400_animation_011C0_bank4.inc"
};

static AnimationRecord _gActor143400Animation011C0Records[110] = {
#include "assets/actor_143400_animation_011C0_records.inc"
};

static u16 _gActor143400Animation011C0Indices[20] = {
#include "assets/actor_143400_animation_011C0_indices.inc"
};

static AnimationSet _gActor143400Animation011C0 = {
    _gActor143400Animation011C0Records,
    _gActor143400Animation011C0Indices,
    { NULL, _gActor143400Animation011C0Bank1, NULL, NULL, _gActor143400Animation011C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation013C0Bank1[3] = {
#include "assets/actor_143400_animation_013C0_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation013C0Bank4[30] = {
#include "assets/actor_143400_animation_013C0_bank4.inc"
};

static AnimationRecord _gActor143400Animation013C0Records[69] = {
#include "assets/actor_143400_animation_013C0_records.inc"
};

static u16 _gActor143400Animation013C0Indices[20] = {
#include "assets/actor_143400_animation_013C0_indices.inc"
};

static AnimationSet _gActor143400Animation013C0 = {
    _gActor143400Animation013C0Records,
    _gActor143400Animation013C0Indices,
    { NULL, _gActor143400Animation013C0Bank1, NULL, NULL, _gActor143400Animation013C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation015A4Bank1[2] = {
#include "assets/actor_143400_animation_015A4_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation015A4Bank4[24] = {
#include "assets/actor_143400_animation_015A4_bank4.inc"
};

static AnimationRecord _gActor143400Animation015A4Records[71] = {
#include "assets/actor_143400_animation_015A4_records.inc"
};

static u16 _gActor143400Animation015A4Indices[20] = {
#include "assets/actor_143400_animation_015A4_indices.inc"
};

static AnimationSet _gActor143400Animation015A4 = {
    _gActor143400Animation015A4Records,
    _gActor143400Animation015A4Indices,
    { NULL, _gActor143400Animation015A4Bank1, NULL, NULL, _gActor143400Animation015A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation017F0Bank1[3] = {
#include "assets/actor_143400_animation_017F0_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation017F0Bank4[29] = {
#include "assets/actor_143400_animation_017F0_bank4.inc"
};

static AnimationRecord _gActor143400Animation017F0Records[89] = {
#include "assets/actor_143400_animation_017F0_records.inc"
};

static u16 _gActor143400Animation017F0Indices[20] = {
#include "assets/actor_143400_animation_017F0_indices.inc"
};

static AnimationSet _gActor143400Animation017F0 = {
    _gActor143400Animation017F0Records,
    _gActor143400Animation017F0Indices,
    { NULL, _gActor143400Animation017F0Bank1, NULL, NULL, _gActor143400Animation017F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation01A6CBank1[2] = {
#include "assets/actor_143400_animation_01A6C_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation01A6CBank4[32] = {
#include "assets/actor_143400_animation_01A6C_bank4.inc"
};

static AnimationRecord _gActor143400Animation01A6CRecords[101] = {
#include "assets/actor_143400_animation_01A6C_records.inc"
};

static u16 _gActor143400Animation01A6CIndices[20] = {
#include "assets/actor_143400_animation_01A6C_indices.inc"
};

static AnimationSet _gActor143400Animation01A6C = {
    _gActor143400Animation01A6CRecords,
    _gActor143400Animation01A6CIndices,
    { NULL, _gActor143400Animation01A6CBank1, NULL, NULL, _gActor143400Animation01A6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation01D18Bank1[3] = {
#include "assets/actor_143400_animation_01D18_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation01D18Bank4[28] = {
#include "assets/actor_143400_animation_01D18_bank4.inc"
};

static AnimationRecord _gActor143400Animation01D18Records[114] = {
#include "assets/actor_143400_animation_01D18_records.inc"
};

static u16 _gActor143400Animation01D18Indices[20] = {
#include "assets/actor_143400_animation_01D18_indices.inc"
};

static AnimationSet _gActor143400Animation01D18 = {
    _gActor143400Animation01D18Records,
    _gActor143400Animation01D18Indices,
    { NULL, _gActor143400Animation01D18Bank1, NULL, NULL, _gActor143400Animation01D18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation01EF0Bank1[2] = {
#include "assets/actor_143400_animation_01EF0_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation01EF0Bank4[22] = {
#include "assets/actor_143400_animation_01EF0_bank4.inc"
};

static AnimationRecord _gActor143400Animation01EF0Records[70] = {
#include "assets/actor_143400_animation_01EF0_records.inc"
};

static u16 _gActor143400Animation01EF0Indices[20] = {
#include "assets/actor_143400_animation_01EF0_indices.inc"
};

static AnimationSet _gActor143400Animation01EF0 = {
    _gActor143400Animation01EF0Records,
    _gActor143400Animation01EF0Indices,
    { NULL, _gActor143400Animation01EF0Bank1, NULL, NULL, _gActor143400Animation01EF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation021F4Bank1[2] = {
#include "assets/actor_143400_animation_021F4_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation021F4Bank4[56] = {
#include "assets/actor_143400_animation_021F4_bank4.inc"
};

static AnimationRecord _gActor143400Animation021F4Records[111] = {
#include "assets/actor_143400_animation_021F4_records.inc"
};

static u16 _gActor143400Animation021F4Indices[20] = {
#include "assets/actor_143400_animation_021F4_indices.inc"
};

static AnimationSet _gActor143400Animation021F4 = {
    _gActor143400Animation021F4Records,
    _gActor143400Animation021F4Indices,
    { NULL, _gActor143400Animation021F4Bank1, NULL, NULL, _gActor143400Animation021F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation025CCBank1[2] = {
#include "assets/actor_143400_animation_025CC_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation025CCBank4[71] = {
#include "assets/actor_143400_animation_025CC_bank4.inc"
};

static AnimationRecord _gActor143400Animation025CCRecords[149] = {
#include "assets/actor_143400_animation_025CC_records.inc"
};

static u16 _gActor143400Animation025CCIndices[20] = {
#include "assets/actor_143400_animation_025CC_indices.inc"
};

static AnimationSet _gActor143400Animation025CC = {
    _gActor143400Animation025CCRecords,
    _gActor143400Animation025CCIndices,
    { NULL, _gActor143400Animation025CCBank1, NULL, NULL, _gActor143400Animation025CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation02870Bank1[2] = {
#include "assets/actor_143400_animation_02870_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation02870Bank4[47] = {
#include "assets/actor_143400_animation_02870_bank4.inc"
};

static AnimationRecord _gActor143400Animation02870Records[96] = {
#include "assets/actor_143400_animation_02870_records.inc"
};

static u16 _gActor143400Animation02870Indices[20] = {
#include "assets/actor_143400_animation_02870_indices.inc"
};

static AnimationSet _gActor143400Animation02870 = {
    _gActor143400Animation02870Records,
    _gActor143400Animation02870Indices,
    { NULL, _gActor143400Animation02870Bank1, NULL, NULL, _gActor143400Animation02870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation02BD8Bank1[2] = {
#include "assets/actor_143400_animation_02BD8_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation02BD8Bank4[58] = {
#include "assets/actor_143400_animation_02BD8_bank4.inc"
};

static AnimationRecord _gActor143400Animation02BD8Records[134] = {
#include "assets/actor_143400_animation_02BD8_records.inc"
};

static u16 _gActor143400Animation02BD8Indices[20] = {
#include "assets/actor_143400_animation_02BD8_indices.inc"
};

static AnimationSet _gActor143400Animation02BD8 = {
    _gActor143400Animation02BD8Records,
    _gActor143400Animation02BD8Indices,
    { NULL, _gActor143400Animation02BD8Bank1, NULL, NULL, _gActor143400Animation02BD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143400Animation03024Bank1[2] = {
#include "assets/actor_143400_animation_03024_bank1.inc"
};

static AnimationPackedRotation _gActor143400Animation03024Bank4[98] = {
#include "assets/actor_143400_animation_03024_bank4.inc"
};

static AnimationRecord _gActor143400Animation03024Records[151] = {
#include "assets/actor_143400_animation_03024_records.inc"
};

static u16 _gActor143400Animation03024Indices[20] = {
#include "assets/actor_143400_animation_03024_indices.inc"
};

static AnimationSet _gActor143400Animation03024 = {
    _gActor143400Animation03024Records,
    _gActor143400Animation03024Indices,
    { NULL, _gActor143400Animation03024Bank1, NULL, NULL, _gActor143400Animation03024Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_143400_80134E6C[11] = {
    NULL,
    &_gActor143400Animation00358,
    &_gActor143400Animation005A4,
    &_gActor143400Animation00918,
    &_gActor143400Animation00AC4,
    &_gActor143400Animation00CDC,
    &_gActor143400Animation00EA8,
    &_gActor143400Animation011C0,
    &_gActor143400Animation013C0,
    &_gActor143400Animation015A4,
    &_gActor143400Animation017F0,
};

AnimationBankCopyRequest D_actor_143400_80134E98 = { { .sets = D_actor_143400_80134E6C }, ARRAY_SIZE(D_actor_143400_80134E6C) };

AnimationPlayRequest D_actor_143400_80134EA0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134EB4 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134EC8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134EDC = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134EF0 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134F04 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134F18 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134F2C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134F40 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134F54 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134F68 = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_143400_80134F7C = { { 7000, 0, 4040, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_143400_80134F94 = { { 6800, 0, 4160, 0 }, { 0, 0, 0, 0 } };

AnimationSet* D_actor_143400_80134FAC[9] = {
    NULL,
    &_gActor143400Animation01A6C,
    &_gActor143400Animation01D18,
    &_gActor143400Animation01EF0,
    &_gActor143400Animation021F4,
    &_gActor143400Animation025CC,
    &_gActor143400Animation02870,
    &_gActor143400Animation02BD8,
    &_gActor143400Animation03024,
};

AnimationBankCopyRequest D_actor_143400_80134FD0 = { { .sets = D_actor_143400_80134FAC }, ARRAY_SIZE(D_actor_143400_80134FAC) };

AnimationPlayRequest D_actor_143400_80134FD8 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80134FEC = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80135000 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80135014 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80135028 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_8013503C = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80135050 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80135064 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_143400_80135078 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_143400_8013508C = { { 9440, 0, 3160, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_143400_801350A4 = { { 7650, 0, 4110, 0 }, { 0, 0, 0, 0 } };

EvsCommand D_actor_143400_801350BC[97] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_143400_80134E98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_143400_80134FD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor143400SelectSceneCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143400_80134F7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134EC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134FEC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542F0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134EDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542F0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor143400SetSceneEvent }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134EF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143400_80134F94 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143400_8013508C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_143400_801350A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542F0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_143400_801350A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143400_801350A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135000 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135014 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134EC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135028 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135000 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F2C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134EC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_8013503C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135050 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135000 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135014 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135078 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 55 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80135000 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134F68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134EB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143400_80134FEC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor143400SelectSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143400_80131E6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0007 }, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143400_801359D4[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143400_80134F94 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_143400_801350A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor143400SetSceneEvent }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor143400SelectSceneCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143400_80131E6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0007 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Selects this scene's CAP resources, or restores the bundle's default resources.
///
/// Nonzero `enable` selects data resource ordinal 2 and a VRAM text origin of
/// (320, 256) pixels; zero resets CAP selection after playback has stopped.
/// The selected resource must be loaded, writable and live through playback.
static void _actor143400SelectSceneCaptions(s32 enable)
{
    enum { SCENE_CAP_RESOURCE  = 2,
           SCENE_CAP_TEXTURE_X = 320,
           SCENE_CAP_TEXTURE_Y = 256 };

    if (enable != 0) {
        Gp_CapFile = NULL;
        capSelectLoadedFile(SCENE_CAP_RESOURCE);
        capSetTexturePage(SCENE_CAP_TEXTURE_X, SCENE_CAP_TEXTURE_Y);
        return;
    }
    capReset();
}

/// Applies the 0xFF-terminated area record list at `D_shelter_r47_8018A638` through
/// `areaApplySavedUpdates`. Reached only through the function pointers in the
/// actor's data.
void func_actor_143400_80131E6C(void)
{
    areaApplySavedUpdates(D_shelter_r47_8018A638);
}

/// Sets the live save's scene-event selector for stage music.
///
/// Both event-script paths supply event 15. Stores the signed byte unchanged;
/// this does not start playback or select an area music track itself.
static void _actor143400SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}
