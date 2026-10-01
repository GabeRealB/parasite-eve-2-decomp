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

void func_actor_143400_80131E24(s32);
void func_actor_143400_80131E6C(void);
void func_actor_143400_80131E90(s8);

AnimationPackedPose D_actor_143400_80131E9C[6] = {
#include "assets/actor_143400_animation_00358_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80131EE4[46] = {
#include "assets/actor_143400_animation_00358_bank4.inc"
};

AnimationRecord D_actor_143400_80131F9C[109] = {
#include "assets/actor_143400_animation_00358_records.inc"
};

u16 D_actor_143400_80132150[20] = {
#include "assets/actor_143400_animation_00358_indices.inc"
};

AnimationSet D_actor_143400_80132178 = {
    D_actor_143400_80131F9C,
    D_actor_143400_80132150,
    { NULL, D_actor_143400_80131E9C, NULL, NULL, D_actor_143400_80131EE4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_801321A0[3] = {
#include "assets/actor_143400_animation_005A4_bank1.inc"
};

AnimationPackedRotation D_actor_143400_801321C4[29] = {
#include "assets/actor_143400_animation_005A4_bank4.inc"
};

AnimationRecord D_actor_143400_80132238[89] = {
#include "assets/actor_143400_animation_005A4_records.inc"
};

u16 D_actor_143400_8013239C[20] = {
#include "assets/actor_143400_animation_005A4_indices.inc"
};

AnimationSet D_actor_143400_801323C4 = {
    D_actor_143400_80132238,
    D_actor_143400_8013239C,
    { NULL, D_actor_143400_801321A0, NULL, NULL, D_actor_143400_801321C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_801323EC[4] = {
#include "assets/actor_143400_animation_00918_bank1.inc"
};

AnimationPackedRotation D_actor_143400_8013241C[69] = {
#include "assets/actor_143400_animation_00918_bank4.inc"
};

AnimationRecord D_actor_143400_80132530[120] = {
#include "assets/actor_143400_animation_00918_records.inc"
};

u16 D_actor_143400_80132710[20] = {
#include "assets/actor_143400_animation_00918_indices.inc"
};

AnimationSet D_actor_143400_80132738 = {
    D_actor_143400_80132530,
    D_actor_143400_80132710,
    { NULL, D_actor_143400_801323EC, NULL, NULL, D_actor_143400_8013241C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80132760[2] = {
#include "assets/actor_143400_animation_00AC4_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80132778[24] = {
#include "assets/actor_143400_animation_00AC4_bank4.inc"
};

AnimationRecord D_actor_143400_801327D8[57] = {
#include "assets/actor_143400_animation_00AC4_records.inc"
};

u16 D_actor_143400_801328BC[20] = {
#include "assets/actor_143400_animation_00AC4_indices.inc"
};

AnimationSet D_actor_143400_801328E4 = {
    D_actor_143400_801327D8,
    D_actor_143400_801328BC,
    { NULL, D_actor_143400_80132760, NULL, NULL, D_actor_143400_80132778, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_8013290C[2] = {
#include "assets/actor_143400_animation_00CDC_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80132924[24] = {
#include "assets/actor_143400_animation_00CDC_bank4.inc"
};

AnimationRecord D_actor_143400_80132984[84] = {
#include "assets/actor_143400_animation_00CDC_records.inc"
};

u16 D_actor_143400_80132AD4[20] = {
#include "assets/actor_143400_animation_00CDC_indices.inc"
};

AnimationSet D_actor_143400_80132AFC = {
    D_actor_143400_80132984,
    D_actor_143400_80132AD4,
    { NULL, D_actor_143400_8013290C, NULL, NULL, D_actor_143400_80132924, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80132B24[3] = {
#include "assets/actor_143400_animation_00EA8_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80132B48[29] = {
#include "assets/actor_143400_animation_00EA8_bank4.inc"
};

AnimationRecord D_actor_143400_80132BBC[57] = {
#include "assets/actor_143400_animation_00EA8_records.inc"
};

u16 D_actor_143400_80132CA0[20] = {
#include "assets/actor_143400_animation_00EA8_indices.inc"
};

AnimationSet D_actor_143400_80132CC8 = {
    D_actor_143400_80132BBC,
    D_actor_143400_80132CA0,
    { NULL, D_actor_143400_80132B24, NULL, NULL, D_actor_143400_80132B48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80132CF0[2] = {
#include "assets/actor_143400_animation_011C0_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80132D08[62] = {
#include "assets/actor_143400_animation_011C0_bank4.inc"
};

AnimationRecord D_actor_143400_80132E00[110] = {
#include "assets/actor_143400_animation_011C0_records.inc"
};

u16 D_actor_143400_80132FB8[20] = {
#include "assets/actor_143400_animation_011C0_indices.inc"
};

AnimationSet D_actor_143400_80132FE0 = {
    D_actor_143400_80132E00,
    D_actor_143400_80132FB8,
    { NULL, D_actor_143400_80132CF0, NULL, NULL, D_actor_143400_80132D08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80133008[3] = {
#include "assets/actor_143400_animation_013C0_bank1.inc"
};

AnimationPackedRotation D_actor_143400_8013302C[30] = {
#include "assets/actor_143400_animation_013C0_bank4.inc"
};

AnimationRecord D_actor_143400_801330A4[69] = {
#include "assets/actor_143400_animation_013C0_records.inc"
};

u16 D_actor_143400_801331B8[20] = {
#include "assets/actor_143400_animation_013C0_indices.inc"
};

AnimationSet D_actor_143400_801331E0 = {
    D_actor_143400_801330A4,
    D_actor_143400_801331B8,
    { NULL, D_actor_143400_80133008, NULL, NULL, D_actor_143400_8013302C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80133208[2] = {
#include "assets/actor_143400_animation_015A4_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80133220[24] = {
#include "assets/actor_143400_animation_015A4_bank4.inc"
};

AnimationRecord D_actor_143400_80133280[71] = {
#include "assets/actor_143400_animation_015A4_records.inc"
};

u16 D_actor_143400_8013339C[20] = {
#include "assets/actor_143400_animation_015A4_indices.inc"
};

AnimationSet D_actor_143400_801333C4 = {
    D_actor_143400_80133280,
    D_actor_143400_8013339C,
    { NULL, D_actor_143400_80133208, NULL, NULL, D_actor_143400_80133220, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_801333EC[3] = {
#include "assets/actor_143400_animation_017F0_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80133410[29] = {
#include "assets/actor_143400_animation_017F0_bank4.inc"
};

AnimationRecord D_actor_143400_80133484[89] = {
#include "assets/actor_143400_animation_017F0_records.inc"
};

u16 D_actor_143400_801335E8[20] = {
#include "assets/actor_143400_animation_017F0_indices.inc"
};

AnimationSet D_actor_143400_80133610 = {
    D_actor_143400_80133484,
    D_actor_143400_801335E8,
    { NULL, D_actor_143400_801333EC, NULL, NULL, D_actor_143400_80133410, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80133638[2] = {
#include "assets/actor_143400_animation_01A6C_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80133650[32] = {
#include "assets/actor_143400_animation_01A6C_bank4.inc"
};

AnimationRecord D_actor_143400_801336D0[101] = {
#include "assets/actor_143400_animation_01A6C_records.inc"
};

u16 D_actor_143400_80133864[20] = {
#include "assets/actor_143400_animation_01A6C_indices.inc"
};

AnimationSet D_actor_143400_8013388C = {
    D_actor_143400_801336D0,
    D_actor_143400_80133864,
    { NULL, D_actor_143400_80133638, NULL, NULL, D_actor_143400_80133650, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_801338B4[3] = {
#include "assets/actor_143400_animation_01D18_bank1.inc"
};

AnimationPackedRotation D_actor_143400_801338D8[28] = {
#include "assets/actor_143400_animation_01D18_bank4.inc"
};

AnimationRecord D_actor_143400_80133948[114] = {
#include "assets/actor_143400_animation_01D18_records.inc"
};

u16 D_actor_143400_80133B10[20] = {
#include "assets/actor_143400_animation_01D18_indices.inc"
};

AnimationSet D_actor_143400_80133B38 = {
    D_actor_143400_80133948,
    D_actor_143400_80133B10,
    { NULL, D_actor_143400_801338B4, NULL, NULL, D_actor_143400_801338D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80133B60[2] = {
#include "assets/actor_143400_animation_01EF0_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80133B78[22] = {
#include "assets/actor_143400_animation_01EF0_bank4.inc"
};

AnimationRecord D_actor_143400_80133BD0[70] = {
#include "assets/actor_143400_animation_01EF0_records.inc"
};

u16 D_actor_143400_80133CE8[20] = {
#include "assets/actor_143400_animation_01EF0_indices.inc"
};

AnimationSet D_actor_143400_80133D10 = {
    D_actor_143400_80133BD0,
    D_actor_143400_80133CE8,
    { NULL, D_actor_143400_80133B60, NULL, NULL, D_actor_143400_80133B78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80133D38[2] = {
#include "assets/actor_143400_animation_021F4_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80133D50[56] = {
#include "assets/actor_143400_animation_021F4_bank4.inc"
};

AnimationRecord D_actor_143400_80133E30[111] = {
#include "assets/actor_143400_animation_021F4_records.inc"
};

u16 D_actor_143400_80133FEC[20] = {
#include "assets/actor_143400_animation_021F4_indices.inc"
};

AnimationSet D_actor_143400_80134014 = {
    D_actor_143400_80133E30,
    D_actor_143400_80133FEC,
    { NULL, D_actor_143400_80133D38, NULL, NULL, D_actor_143400_80133D50, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_8013403C[2] = {
#include "assets/actor_143400_animation_025CC_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80134054[71] = {
#include "assets/actor_143400_animation_025CC_bank4.inc"
};

AnimationRecord D_actor_143400_80134170[149] = {
#include "assets/actor_143400_animation_025CC_records.inc"
};

u16 D_actor_143400_801343C4[20] = {
#include "assets/actor_143400_animation_025CC_indices.inc"
};

AnimationSet D_actor_143400_801343EC = {
    D_actor_143400_80134170,
    D_actor_143400_801343C4,
    { NULL, D_actor_143400_8013403C, NULL, NULL, D_actor_143400_80134054, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80134414[2] = {
#include "assets/actor_143400_animation_02870_bank1.inc"
};

AnimationPackedRotation D_actor_143400_8013442C[47] = {
#include "assets/actor_143400_animation_02870_bank4.inc"
};

AnimationRecord D_actor_143400_801344E8[96] = {
#include "assets/actor_143400_animation_02870_records.inc"
};

u16 D_actor_143400_80134668[20] = {
#include "assets/actor_143400_animation_02870_indices.inc"
};

AnimationSet D_actor_143400_80134690 = {
    D_actor_143400_801344E8,
    D_actor_143400_80134668,
    { NULL, D_actor_143400_80134414, NULL, NULL, D_actor_143400_8013442C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_801346B8[2] = {
#include "assets/actor_143400_animation_02BD8_bank1.inc"
};

AnimationPackedRotation D_actor_143400_801346D0[58] = {
#include "assets/actor_143400_animation_02BD8_bank4.inc"
};

AnimationRecord D_actor_143400_801347B8[134] = {
#include "assets/actor_143400_animation_02BD8_records.inc"
};

u16 D_actor_143400_801349D0[20] = {
#include "assets/actor_143400_animation_02BD8_indices.inc"
};

AnimationSet D_actor_143400_801349F8 = {
    D_actor_143400_801347B8,
    D_actor_143400_801349D0,
    { NULL, D_actor_143400_801346B8, NULL, NULL, D_actor_143400_801346D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143400_80134A20[2] = {
#include "assets/actor_143400_animation_03024_bank1.inc"
};

AnimationPackedRotation D_actor_143400_80134A38[98] = {
#include "assets/actor_143400_animation_03024_bank4.inc"
};

AnimationRecord D_actor_143400_80134BC0[151] = {
#include "assets/actor_143400_animation_03024_records.inc"
};

u16 D_actor_143400_80134E1C[20] = {
#include "assets/actor_143400_animation_03024_indices.inc"
};

AnimationSet D_actor_143400_80134E44 = {
    D_actor_143400_80134BC0,
    D_actor_143400_80134E1C,
    { NULL, D_actor_143400_80134A20, NULL, NULL, D_actor_143400_80134A38, NULL, NULL, NULL },
};

AnimationSet* D_actor_143400_80134E6C[11] = {
    NULL,
    &D_actor_143400_80132178,
    &D_actor_143400_801323C4,
    &D_actor_143400_80132738,
    &D_actor_143400_801328E4,
    &D_actor_143400_80132AFC,
    &D_actor_143400_80132CC8,
    &D_actor_143400_80132FE0,
    &D_actor_143400_801331E0,
    &D_actor_143400_801333C4,
    &D_actor_143400_80133610,
};

GpCopyArg D_actor_143400_80134E98 = { { .sets = D_actor_143400_80134E6C }, 11 };

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
    &D_actor_143400_8013388C,
    &D_actor_143400_80133B38,
    &D_actor_143400_80133D10,
    &D_actor_143400_80134014,
    &D_actor_143400_801343EC,
    &D_actor_143400_80134690,
    &D_actor_143400_801349F8,
    &D_actor_143400_80134E44,
};

GpCopyArg D_actor_143400_80134FD0 = { { .sets = D_actor_143400_80134FAC }, 9 };

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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_143400_80134E98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_143400_80134FD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143400_80131E24 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_143400_80131E90 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143400_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_143400_80131E90 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_143400_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143400_80131E6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0007 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
}; /// With a non-zero `arg0`, clears `Gp_CapFile`, loads capture file 2 and
/// passes (0x140, 0x100) to `func_800E6D4C`; with zero, resets the capture
/// state instead. Reached only through the function pointers in the actor's
/// data.
void func_actor_143400_80131E24(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x140, 0x100);
        return;
    }
    Gp_ResetCap();
}

/// Applies the 0xFF-terminated area record list at `D_shelter_r47_8018A638` through
/// `Gp_ApplyAreaRecs`. Reached only through the function pointers in the
/// actor's data.
void func_actor_143400_80131E6C(void)
{
    Gp_ApplyAreaRecs(D_shelter_r47_8018A638);
}

/// Stores `arg0` in the gameplay byte `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent`. Reached only through the
/// function pointers in the actor's data.
void func_actor_143400_80131E90(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}
