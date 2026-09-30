#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "mist_parking_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"

/// Task descriptor tables the room spawns its tasks from.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// The "%" suffix appended to the play-data percentages.
static u8 Telephone_Data_80181A78[];

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

extern UiObjectDesc D_800611E4;

/// The 0xFFFF-terminated item id lists `func_mist_parking_8017D8F8` chooses
/// from.
static u16 Shop_Data_801815F8[];
static u16 Shop_Data_80181600[];
static u16 Shop_Data_80181608[];
static u16 Shop_Data_80181610[];
static u16 Shop_Data_80181620[];
static u16 Shop_Data_80181630[];
static u16 Shop_Data_80181640[];
static u16 Shop_Data_80181648[];
static u16 Shop_Data_80181658[];
static u16 Shop_Data_80181668[];
static u16 Shop_Data_80181678[];
static u16 Shop_Data_80181680[];
static u16 Shop_Data_80181694[];
static u16 Shop_Data_801816AC[];
static u16 Shop_Data_801816C0[];
static u16 Shop_Data_801816C8[];
static u16 Shop_Data_801816D8[];
static u16 Shop_Data_801816F0[];
static u16 Shop_Data_80181704[];
static u16 Shop_Data_8018170C[];
static u16 Shop_Data_80181720[];
static u16 Shop_Data_8018173C[];
static u16 Shop_Data_8018174C[];
static u16 Shop_Data_80181758[];
static u16 Shop_Data_80181770[];
static u16 Shop_Data_8018178C[];
static u16 Shop_Data_801817A0[];
static u16 Shop_Data_801817A8[];
static u16 Shop_Data_801817BC[];
static u16 Shop_Data_801817DC[];
static u16 Shop_Data_801817EC[];
static u16 Shop_Data_801817F8[];
static u16 Shop_Data_80181810[];
static u16 Shop_Data_80181814[];
static u16 Shop_Data_80181818[];
static u16 Shop_Data_80181820[];
static u16 Shop_Data_80181830[];
static u16 Shop_Data_80181838[];
static u16 Shop_Data_80181840[];
static u16 Shop_Data_80181848[];
static u16 Shop_Data_80181854[];
static u16 Shop_Data_8018185C[];
static u16 Shop_Data_80181868[];
static u16 Shop_Data_80181870[];
static u16 Shop_Data_8018187C[];
static u16 Shop_Data_80181888[];
static u16 Shop_Data_80181890[];
static u16 Shop_Data_80181898[];
static u16 Shop_Data_801818A4[];
static u16 Shop_Data_801818B0[];
static u16 Shop_Data_801818B8[];
static u16 Shop_Data_801818C4[];
static u16 Shop_Data_801818D0[];
static u16 Shop_Data_801818DC[];
static u16 Shop_Data_801818E0[];
static u16 Shop_Data_801818EC[];
static u16 Shop_Data_801818F8[];
static u16 Shop_Data_80181904[];
static u16 Shop_Data_8018190C[];
static u16 Shop_Data_80181918[];
static u16 Shop_Data_80181924[];
static u16 Shop_Data_80181930[];
static u16 Shop_Data_80181938[];
static u16 Shop_Data_80181944[];
static u16 Shop_Data_80181AD4[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\xF2\xEF"
#include "../../shared/telephone.h"

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xE2"
#include "../../shared/shop.h"

s32  func_mist_parking_801823F8(s32, s32, s32);
s32  func_mist_parking_801826B8(void);
s32  func_mist_parking_801826C0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_mist_parking_801826E8(Task*, s32, DirectionActionRequest* request);
void func_mist_parking_80182750(s32);
void func_mist_parking_801827A0(s32);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_mist_parking_8018668C = { 0, 192, Shop_SessionTask, { .model = NULL } };

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { 0, 32, roomCutsceneTask, { .model = NULL } },
    { 0, 32, roomCutsceneSoundTask, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

AnimationPackedPose D_mist_parking_801869DC[2] = {
#include "assets/mist_parking_animation_095D0_bank1.inc"
};

AnimationPackedRotation D_mist_parking_801869F4[17] = {
#include "assets/mist_parking_animation_095D0_bank4.inc"
};

AnimationRecord D_mist_parking_80186A38[76] = {
#include "assets/mist_parking_animation_095D0_records.inc"
};

u16 D_mist_parking_80186B68[20] = {
#include "assets/mist_parking_animation_095D0_indices.inc"
};

AnimationSet D_mist_parking_80186B90 = {
    D_mist_parking_80186A38,
    D_mist_parking_80186B68,
    { NULL, D_mist_parking_801869DC, NULL, NULL, D_mist_parking_801869F4, NULL, NULL, NULL },
};

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, DirectionActionRequest*);
        s32 (*call2)(Task*, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call3)(s32, s32, s32);
    } handler;
} MistParkingMessageEntry;
STATIC_ASSERT_SIZEOF(MistParkingMessageEntry, 8);

MistParkingMessageEntry D_mist_parking_80186BB8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, { .call2 = func_mist_parking_801826C0 } },
    { DIRECTION_MESSAGE_ROOM_ACTION, { .call1 = func_mist_parking_801826E8 } },
    { 5105, { .call0 = func_mist_parking_801826B8 } },
    { 5104, { .call3 = func_mist_parking_801823F8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

ActorTransform D_mist_parking_80186BE0 = { { 8448, 1, -2599, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_mist_parking_80186BF8 = { { 8448, 1, -3828, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_mist_parking_80186C10 = { { 3310, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

AnimationSet* D_mist_parking_80186C28[1] = {
    &D_mist_parking_80186B90,
};

GpCopyArg D_mist_parking_80186C2C = { { .sets = D_mist_parking_80186C28 }, 1 };

AnimationPlayRequest D_mist_parking_80186C34 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_80186C48 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpEvsCmd D_mist_parking_80186C5C[15] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C48 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_801827A0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_80186BE0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80182750 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80186DC4[13] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C48 }, { .value = 0 } },
    { 15, { .value = 0x51130001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_80186BF8 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80182750 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80186EFC[12] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_80186C10 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_80186C2C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C34 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_mist_parking_8018701C[1] = {
#include "assets/mist_parking_model_09CD4_skeleton.inc"
};

u32 D_mist_parking_80187040[1] = {
#include "assets/mist_parking_model_09CD4_partVerts.inc"
};

SVECTOR D_mist_parking_80187044[35] = {
#include "assets/mist_parking_model_09CD4_verts.inc"
};

u32 D_mist_parking_8018715C[78] = {
#include "assets/mist_parking_model_09CD4_stream.inc"
};

TmdSource D_mist_parking_80187294 = {
    0,
    576,
    0,
    1,
    D_mist_parking_80187040,
    D_mist_parking_80187044,
    &D_mist_parking_80187044[35],
    D_mist_parking_8018701C,
    D_mist_parking_8018715C,
};

AnimationPackedPose D_mist_parking_801872B8[6] = {
#include "assets/mist_parking_animation_09FD4_bank1.inc"
};

AnimationPackedRotation D_mist_parking_80187300[46] = {
#include "assets/mist_parking_animation_09FD4_bank4.inc"
};

AnimationRecord D_mist_parking_801873B8[109] = {
#include "assets/mist_parking_animation_09FD4_records.inc"
};

u16 D_mist_parking_8018756C[20] = {
#include "assets/mist_parking_animation_09FD4_indices.inc"
};

AnimationSet D_mist_parking_80187594 = {
    D_mist_parking_801873B8,
    D_mist_parking_8018756C,
    { NULL, D_mist_parking_801872B8, NULL, NULL, D_mist_parking_80187300, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_801875BC[13] = {
#include "assets/mist_parking_animation_0A774_bank1.inc"
};

AnimationPackedRotation D_mist_parking_80187658[179] = {
#include "assets/mist_parking_animation_0A774_bank4.inc"
};

AnimationRecord D_mist_parking_80187924[250] = {
#include "assets/mist_parking_animation_0A774_records.inc"
};

u16 D_mist_parking_80187D0C[20] = {
#include "assets/mist_parking_animation_0A774_indices.inc"
};

AnimationSet D_mist_parking_80187D34 = {
    D_mist_parking_80187924,
    D_mist_parking_80187D0C,
    { NULL, D_mist_parking_801875BC, NULL, NULL, D_mist_parking_80187658, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_80187D5C[6] = {
#include "assets/mist_parking_animation_0AC5C_bank1.inc"
};

AnimationPackedRotation D_mist_parking_80187DA4[110] = {
#include "assets/mist_parking_animation_0AC5C_bank4.inc"
};

AnimationRecord D_mist_parking_80187F5C[166] = {
#include "assets/mist_parking_animation_0AC5C_records.inc"
};

u16 D_mist_parking_801881F4[20] = {
#include "assets/mist_parking_animation_0AC5C_indices.inc"
};

AnimationSet D_mist_parking_8018821C = {
    D_mist_parking_80187F5C,
    D_mist_parking_801881F4,
    { NULL, D_mist_parking_80187D5C, NULL, NULL, D_mist_parking_80187DA4, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_80188244[9] = {
#include "assets/mist_parking_animation_0B138_bank1.inc"
};

AnimationPackedRotation D_mist_parking_801882B0[117] = {
#include "assets/mist_parking_animation_0B138_bank4.inc"
};

AnimationRecord D_mist_parking_80188484[147] = {
#include "assets/mist_parking_animation_0B138_records.inc"
};

u16 D_mist_parking_801886D0[20] = {
#include "assets/mist_parking_animation_0B138_indices.inc"
};

AnimationSet D_mist_parking_801886F8 = {
    D_mist_parking_80188484,
    D_mist_parking_801886D0,
    { NULL, D_mist_parking_80188244, NULL, NULL, D_mist_parking_801882B0, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_80188720[10] = {
#include "assets/mist_parking_animation_0B700_bank1.inc"
};

AnimationPackedRotation D_mist_parking_80188798[143] = {
#include "assets/mist_parking_animation_0B700_bank4.inc"
};

AnimationRecord D_mist_parking_801889D4[177] = {
#include "assets/mist_parking_animation_0B700_records.inc"
};

u16 D_mist_parking_80188C98[20] = {
#include "assets/mist_parking_animation_0B700_indices.inc"
};

AnimationSet D_mist_parking_80188CC0 = {
    D_mist_parking_801889D4,
    D_mist_parking_80188C98,
    { NULL, D_mist_parking_80188720, NULL, NULL, D_mist_parking_80188798, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_80188CE8[5] = {
#include "assets/mist_parking_animation_0BAB4_bank1.inc"
};

AnimationPackedRotation D_mist_parking_80188D24[65] = {
#include "assets/mist_parking_animation_0BAB4_bank4.inc"
};

AnimationRecord D_mist_parking_80188E28[137] = {
#include "assets/mist_parking_animation_0BAB4_records.inc"
};

u16 D_mist_parking_8018904C[20] = {
#include "assets/mist_parking_animation_0BAB4_indices.inc"
};

AnimationSet D_mist_parking_80189074 = {
    D_mist_parking_80188E28,
    D_mist_parking_8018904C,
    { NULL, D_mist_parking_80188CE8, NULL, NULL, D_mist_parking_80188D24, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018909C[12] = {
#include "assets/mist_parking_animation_0C1B0_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018912C[153] = {
#include "assets/mist_parking_animation_0C1B0_bank4.inc"
};

AnimationRecord D_mist_parking_80189390[238] = {
#include "assets/mist_parking_animation_0C1B0_records.inc"
};

u16 D_mist_parking_80189748[20] = {
#include "assets/mist_parking_animation_0C1B0_indices.inc"
};

AnimationSet D_mist_parking_80189770 = {
    D_mist_parking_80189390,
    D_mist_parking_80189748,
    { NULL, D_mist_parking_8018909C, NULL, NULL, D_mist_parking_8018912C, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_80189798[7] = {
#include "assets/mist_parking_animation_0C688_bank1.inc"
};

AnimationPackedRotation D_mist_parking_801897EC[94] = {
#include "assets/mist_parking_animation_0C688_bank4.inc"
};

AnimationRecord D_mist_parking_80189964[175] = {
#include "assets/mist_parking_animation_0C688_records.inc"
};

u16 D_mist_parking_80189C20[20] = {
#include "assets/mist_parking_animation_0C688_indices.inc"
};

AnimationSet D_mist_parking_80189C48 = {
    D_mist_parking_80189964,
    D_mist_parking_80189C20,
    { NULL, D_mist_parking_80189798, NULL, NULL, D_mist_parking_801897EC, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_80189C70[12] = {
#include "assets/mist_parking_animation_0CD80_bank1.inc"
};

AnimationPackedRotation D_mist_parking_80189D00[177] = {
#include "assets/mist_parking_animation_0CD80_bank4.inc"
};

AnimationRecord D_mist_parking_80189FC4[213] = {
#include "assets/mist_parking_animation_0CD80_records.inc"
};

u16 D_mist_parking_8018A318[20] = {
#include "assets/mist_parking_animation_0CD80_indices.inc"
};

AnimationSet D_mist_parking_8018A340 = {
    D_mist_parking_80189FC4,
    D_mist_parking_8018A318,
    { NULL, D_mist_parking_80189C70, NULL, NULL, D_mist_parking_80189D00, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018A368[3] = {
#include "assets/mist_parking_animation_0D060_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018A38C[63] = {
#include "assets/mist_parking_animation_0D060_bank4.inc"
};

AnimationRecord D_mist_parking_8018A488[92] = {
#include "assets/mist_parking_animation_0D060_records.inc"
};

u16 D_mist_parking_8018A5F8[20] = {
#include "assets/mist_parking_animation_0D060_indices.inc"
};

AnimationSet D_mist_parking_8018A620 = {
    D_mist_parking_8018A488,
    D_mist_parking_8018A5F8,
    { NULL, D_mist_parking_8018A368, NULL, NULL, D_mist_parking_8018A38C, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018A648[6] = {
#include "assets/mist_parking_animation_0D3E4_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018A690[69] = {
#include "assets/mist_parking_animation_0D3E4_bank4.inc"
};

AnimationRecord D_mist_parking_8018A7A4[118] = {
#include "assets/mist_parking_animation_0D3E4_records.inc"
};

u16 D_mist_parking_8018A97C[20] = {
#include "assets/mist_parking_animation_0D3E4_indices.inc"
};

AnimationSet D_mist_parking_8018A9A4 = {
    D_mist_parking_8018A7A4,
    D_mist_parking_8018A97C,
    { NULL, D_mist_parking_8018A648, NULL, NULL, D_mist_parking_8018A690, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018A9CC[4] = {
#include "assets/mist_parking_animation_0D6A4_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018A9FC[40] = {
#include "assets/mist_parking_animation_0D6A4_bank4.inc"
};

AnimationRecord D_mist_parking_8018AA9C[104] = {
#include "assets/mist_parking_animation_0D6A4_records.inc"
};

u16 D_mist_parking_8018AC3C[20] = {
#include "assets/mist_parking_animation_0D6A4_indices.inc"
};

AnimationSet D_mist_parking_8018AC64 = {
    D_mist_parking_8018AA9C,
    D_mist_parking_8018AC3C,
    { NULL, D_mist_parking_8018A9CC, NULL, NULL, D_mist_parking_8018A9FC, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018AC8C[7] = {
#include "assets/mist_parking_animation_0DA94_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018ACE0[62] = {
#include "assets/mist_parking_animation_0DA94_bank4.inc"
};

AnimationRecord D_mist_parking_8018ADD8[149] = {
#include "assets/mist_parking_animation_0DA94_records.inc"
};

u16 D_mist_parking_8018B02C[20] = {
#include "assets/mist_parking_animation_0DA94_indices.inc"
};

AnimationSet D_mist_parking_8018B054 = {
    D_mist_parking_8018ADD8,
    D_mist_parking_8018B02C,
    { NULL, D_mist_parking_8018AC8C, NULL, NULL, D_mist_parking_8018ACE0, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018B07C[10] = {
#include "assets/mist_parking_animation_0DE94_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018B0F4[85] = {
#include "assets/mist_parking_animation_0DE94_bank4.inc"
};

AnimationRecord D_mist_parking_8018B248[121] = {
#include "assets/mist_parking_animation_0DE94_records.inc"
};

u16 D_mist_parking_8018B42C[20] = {
#include "assets/mist_parking_animation_0DE94_indices.inc"
};

AnimationSet D_mist_parking_8018B454 = {
    D_mist_parking_8018B248,
    D_mist_parking_8018B42C,
    { NULL, D_mist_parking_8018B07C, NULL, NULL, D_mist_parking_8018B0F4, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018B47C[8] = {
#include "assets/mist_parking_animation_0E1D0_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018B4DC[65] = {
#include "assets/mist_parking_animation_0E1D0_bank4.inc"
};

AnimationRecord D_mist_parking_8018B5E0[98] = {
#include "assets/mist_parking_animation_0E1D0_records.inc"
};

u16 D_mist_parking_8018B768[20] = {
#include "assets/mist_parking_animation_0E1D0_indices.inc"
};

AnimationSet D_mist_parking_8018B790 = {
    D_mist_parking_8018B5E0,
    D_mist_parking_8018B768,
    { NULL, D_mist_parking_8018B47C, NULL, NULL, D_mist_parking_8018B4DC, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018B7B8[10] = {
#include "assets/mist_parking_animation_0E6E0_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018B830[109] = {
#include "assets/mist_parking_animation_0E6E0_bank4.inc"
};

AnimationRecord D_mist_parking_8018B9E4[165] = {
#include "assets/mist_parking_animation_0E6E0_records.inc"
};

u16 D_mist_parking_8018BC78[20] = {
#include "assets/mist_parking_animation_0E6E0_indices.inc"
};

AnimationSet D_mist_parking_8018BCA0 = {
    D_mist_parking_8018B9E4,
    D_mist_parking_8018BC78,
    { NULL, D_mist_parking_8018B7B8, NULL, NULL, D_mist_parking_8018B830, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018BCC8[7] = {
#include "assets/mist_parking_animation_0EA0C_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018BD1C[56] = {
#include "assets/mist_parking_animation_0EA0C_bank4.inc"
};

AnimationRecord D_mist_parking_8018BDFC[106] = {
#include "assets/mist_parking_animation_0EA0C_records.inc"
};

u16 D_mist_parking_8018BFA4[20] = {
#include "assets/mist_parking_animation_0EA0C_indices.inc"
};

AnimationSet D_mist_parking_8018BFCC = {
    D_mist_parking_8018BDFC,
    D_mist_parking_8018BFA4,
    { NULL, D_mist_parking_8018BCC8, NULL, NULL, D_mist_parking_8018BD1C, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018BFF4[8] = {
#include "assets/mist_parking_animation_0EDE0_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018C054[84] = {
#include "assets/mist_parking_animation_0EDE0_bank4.inc"
};

AnimationRecord D_mist_parking_8018C1A4[117] = {
#include "assets/mist_parking_animation_0EDE0_records.inc"
};

u16 D_mist_parking_8018C378[20] = {
#include "assets/mist_parking_animation_0EDE0_indices.inc"
};

AnimationSet D_mist_parking_8018C3A0 = {
    D_mist_parking_8018C1A4,
    D_mist_parking_8018C378,
    { NULL, D_mist_parking_8018BFF4, NULL, NULL, D_mist_parking_8018C054, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018C3C8[7] = {
#include "assets/mist_parking_animation_0F14C_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018C41C[74] = {
#include "assets/mist_parking_animation_0F14C_bank4.inc"
};

AnimationRecord D_mist_parking_8018C544[104] = {
#include "assets/mist_parking_animation_0F14C_records.inc"
};

u16 D_mist_parking_8018C6E4[20] = {
#include "assets/mist_parking_animation_0F14C_indices.inc"
};

AnimationSet D_mist_parking_8018C70C = {
    D_mist_parking_8018C544,
    D_mist_parking_8018C6E4,
    { NULL, D_mist_parking_8018C3C8, NULL, NULL, D_mist_parking_8018C41C, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018C734[5] = {
#include "assets/mist_parking_animation_0F574_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018C770[90] = {
#include "assets/mist_parking_animation_0F574_bank4.inc"
};

AnimationRecord D_mist_parking_8018C8D8[141] = {
#include "assets/mist_parking_animation_0F574_records.inc"
};

u16 D_mist_parking_8018CB0C[20] = {
#include "assets/mist_parking_animation_0F574_indices.inc"
};

AnimationSet D_mist_parking_8018CB34 = {
    D_mist_parking_8018C8D8,
    D_mist_parking_8018CB0C,
    { NULL, D_mist_parking_8018C734, NULL, NULL, D_mist_parking_8018C770, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018CB5C[3] = {
#include "assets/mist_parking_animation_0F7F0_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018CB80[43] = {
#include "assets/mist_parking_animation_0F7F0_bank4.inc"
};

AnimationRecord D_mist_parking_8018CC2C[87] = {
#include "assets/mist_parking_animation_0F7F0_records.inc"
};

u16 D_mist_parking_8018CD88[20] = {
#include "assets/mist_parking_animation_0F7F0_indices.inc"
};

AnimationSet D_mist_parking_8018CDB0 = {
    D_mist_parking_8018CC2C,
    D_mist_parking_8018CD88,
    { NULL, D_mist_parking_8018CB5C, NULL, NULL, D_mist_parking_8018CB80, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018CDD8[5] = {
#include "assets/mist_parking_animation_0FC60_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018CE14[99] = {
#include "assets/mist_parking_animation_0FC60_bank4.inc"
};

AnimationRecord D_mist_parking_8018CFA0[150] = {
#include "assets/mist_parking_animation_0FC60_records.inc"
};

u16 D_mist_parking_8018D1F8[20] = {
#include "assets/mist_parking_animation_0FC60_indices.inc"
};

AnimationSet D_mist_parking_8018D220 = {
    D_mist_parking_8018CFA0,
    D_mist_parking_8018D1F8,
    { NULL, D_mist_parking_8018CDD8, NULL, NULL, D_mist_parking_8018CE14, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018D248[4] = {
#include "assets/mist_parking_animation_0FE5C_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018D278[17] = {
#include "assets/mist_parking_animation_0FE5C_bank4.inc"
};

AnimationRecord D_mist_parking_8018D2BC[78] = {
#include "assets/mist_parking_animation_0FE5C_records.inc"
};

u16 D_mist_parking_8018D3F4[20] = {
#include "assets/mist_parking_animation_0FE5C_indices.inc"
};

AnimationSet D_mist_parking_8018D41C = {
    D_mist_parking_8018D2BC,
    D_mist_parking_8018D3F4,
    { NULL, D_mist_parking_8018D248, NULL, NULL, D_mist_parking_8018D278, NULL, NULL, NULL },
};

AnimationPackedPose D_mist_parking_8018D444[7] = {
#include "assets/mist_parking_animation_10174_bank1.inc"
};

AnimationPackedRotation D_mist_parking_8018D498[54] = {
#include "assets/mist_parking_animation_10174_bank4.inc"
};

AnimationRecord D_mist_parking_8018D570[103] = {
#include "assets/mist_parking_animation_10174_records.inc"
};

u16 D_mist_parking_8018D70C[20] = {
#include "assets/mist_parking_animation_10174_indices.inc"
};

AnimationSet D_mist_parking_8018D734 = {
    D_mist_parking_8018D570,
    D_mist_parking_8018D70C,
    { NULL, D_mist_parking_8018D444, NULL, NULL, D_mist_parking_8018D498, NULL, NULL, NULL },
};

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
static u8 Shop_Data_80181A20[];

static u8 Shop_Data_80181A0C[];

static u8 Shop_Data_80181A1C[];

static UiObjectDesc Shop_Data_80181B84;

static UiObjectDesc Shop_Data_80181BD8;

static RoomShopTier Shop_Data_80181950[13];

/// Messages of the shop's panels.
static u8 Shop_Data_801819F0[];

static u8 Shop_Data_80181A04[];

static u8 Shop_Data_80181A5C[];

static u8 Shop_Data_80181A64[];

static u8 Shop_Data_80181A70[];

static u8 Shop_Data_80181A78[];

static u8 Shop_Data_80181A80[];

static u8 Shop_Data_80181A94[];

static u8 Shop_Data_80181AA4[];

static u8 Shop_Data_80181AC4[];

static u8 Shop_Data_80181AD0[];

/// Row handlers, lists and panel descriptors of the shop's panels.
static UiListItemFunc Shop_Data_80181AD8[];

static UiList Shop_Data_80181AE0;

static UiList Shop_Data_80181B0C;

static UiObjectDesc Shop_Data_80181B4C;

static UiObjectDesc Shop_Data_80181B68;

static UiObjectDesc Shop_Data_80181BA0;

static UiObjectDesc Shop_Data_80181BF4;

static UiObjectDesc Shop_Data_80181C10;

/// Descriptor of the panel `func_mist_parking_8017FF9C` opens.
static UiObjectDesc Shop_Data_80181B30;

/// Labels, suffix and holder texts of the play-data summary rows.
static u8 Telephone_Data_80181A20[];

static u8 Telephone_Data_80181A50[];

static u8 Telephone_Data_80181A28[];

static u8 Telephone_Data_80181A2C[];

static u8 Telephone_Data_80181A34[];

static u8 Telephone_Data_80181A40[];

static u8 Telephone_Data_80181A58[];

static u8 Telephone_Data_80181A60[];

static u8 Telephone_Data_80181A68[];

static u8 Telephone_Data_80181A70[];

static u8 Telephone_Data_80181A7C[];

static u8 Telephone_Data_80181AA8[];

static u8 Telephone_Data_80181ACC[];

static u8 Telephone_Data_80181AFC[];

static u8 Telephone_Data_80181B30[];

static u8 Telephone_Data_80181B64[];

static u8 Telephone_Data_80181B9C[];

static u8 Telephone_Data_80181BD0[];

static u8 Telephone_Data_80181C08[];

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
static UiList Telephone_Data_80181C6C;

static UiList Telephone_Data_80181CF4;

static UiObjectDesc Telephone_Data_80181C90;

/// List of the menu panel `func_mist_parking_80181920` draws.
static UiList Telephone_Data_80181C44;

/// Texts of the four menu rows below, and the panels two of them open.
static u8 Telephone_Data_801819F8[];

static u8 Telephone_Data_80181A00[];

static u8 Telephone_Data_80181A0C[];

static u8 Telephone_Data_80181A18[];

static UiObjectDesc Telephone_Data_80181CAC;

static UiObjectDesc Telephone_Data_80181CC8;

extern GpEvsCmd D_mist_parking_80186EFC[];

static void func_mist_parking_801827C0(Task* arg0);

static void func_mist_parking_80182888(Task* task);

extern MistParkingMessageEntry D_mist_parking_80186BB8[5];

extern GpEvsCmd D_mist_parking_80186C5C[];

extern GpEvsCmd D_mist_parking_80186DC4[];

static inline s32 Shop_AddItemCount(s32 item, s32 count);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/telephone.inc.c"

void func_mist_parking_80181468(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// The area records applied when the scene hands the Dryfield story on.

#include "../../shared/room_cutscene_task.inc.c"

s32 func_mist_parking_801823F8(s32 arg0, s32 arg1, s32 arg2)
{
    GameSession* session;
    u8           temp;

    switch (arg2) {
        case 15:
            temp = gGameSession->location.loc.variant;
            if (temp == 2) {
                if (GameFlag_GetNibble(0xF1) == 1) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614(D_mist_parking_8018F0A4, 1);
                    GameFlag_SetNibble(0xF1, 2);
                } else if (GameFlag_GetNibble(0xF1) == temp) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614(D_mist_parking_8018F194, 1);
                    GameFlag_SetNibble(0xF1, 3);
                } else if (GameFlag_GetNibble(0xF1) == 3) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_mist_parking_8018D75C, 8, 0, 0);
                }
            } else if (GameFlag_GetNibble(0xED) == 1) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_mist_parking_80190824, 4, 0, 0);
            }
            break;
        case 8:
            D_mist_parking_8019533C.field_0  = 9;
            D_mist_parking_8019533C.field_1  = 1;
            D_mist_parking_8019533C.field_3  = 3;
            D_mist_parking_8019533C.field_2  = 0;
            D_mist_parking_8019533C.field_4  = 0x51130003;
            D_mist_parking_8019533C.field_8  = 0x51130004;
            D_mist_parking_8019533C.field_10 = 0x5113000B;
            D_mist_parking_8019533C.field_C  = 0x51130012;
            Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 4, &D_mist_parking_8019533C);
            session                                = gGameSession;
            Mc_SaveData[0].state.location.loc.warp = 2;
            session->location.loc.warp             = 2;
            break;
        case 18:
            Gp_MsgPlayerWeapon(0);
            if (gGameSession->location.loc.variant == 1) {
                Task_SpawnFromTable(D_mist_parking_80190824, 3, 0, 0);
            } else {
                Task_SpawnFromTable(D_mist_parking_8018D75C, 7, 0, 0);
            }
            break;
        case 1:
            func_800E8614(D_mist_parking_80186EFC, 1);
            break;
    }
    return 0;
}

/// State handlers of the task `func_mist_parking_80182898` runs: its set-up,
/// an empty per-frame state and the kill.
static const TaskFuncTable3 D_mist_parking_8017D7DC = {
    {
        func_mist_parking_801827C0,
        func_mist_parking_80182888,
        taskKill,
    },
};

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Handler that answers 0.
s32 func_mist_parking_801826B8(void)
{
    return 0;
}

/// Message handler that copies the location record it is given onto the
/// reply record and answers 1.
s32 func_mist_parking_801826C0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_mist_parking_801826E8(Task* task, s32 msgId, DirectionActionRequest* request)
{
    if (request->actionId == 1) {
        func_800E8614(D_mist_parking_80186C5C, 1);
    }
    if (request->actionId == 2) {
        func_800E8614(D_mist_parking_80186DC4, 1);
        GameFlag_SetNibble(0xED, 1);
    }
    return 1;
}

void func_mist_parking_80182750(s32 arg0)
{
    if (GameFlag_GetNibble(0x7A) != 0) {
        arg0 += 2;
    }
    Mc_SaveData[0].state.location.loc.room = arg0;
    gGameSession->location.loc.room        = arg0;
    gGameSession->roomObjsDirty            = 1;
}

void func_mist_parking_801827A0(s32 arg0)
{
    Gp_SpawnIfCapIdle(arg0, 0);
}

static void func_mist_parking_801827C0(Task* arg0)
{
    arg0->msgTable = D_mist_parking_80186BB8;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->location.loc.variant == 2) && (GameFlag_GetNibble(0xF1) == 0)) {
        if (Mc_SaveData[0].state.location.loc.warp == 3) {
            func_800E3FAC(0xA2, 0x3C);
            func_mist_parking_801837A4(0);
            func_800E8634(D_mist_parking_8018DF34, 0, D_mist_parking_8018EDBC);
        } else {
            func_mist_parking_8018471C(0);
            func_800E8614(D_mist_parking_8018EFE4, 1);
        }
    }
    arg0->state = arg0->state + 1;
}

/// The empty per-frame state of `D_mist_parking_8017D7DC`.
static void func_mist_parking_80182888(Task* task)
{
    char pad[0x10];
}

/// Runs the handler for the task's state from a stack copy of
/// `D_mist_parking_8017D7DC`.
void func_mist_parking_80182898(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_parking_8017D7DC;
    sp.funcs[task->state](task);
}

void func_mist_parking_801828F0(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].nextPose.indices.setIndex - ANIMATION_BANK_BASE_SET_COUNT;
        if ((idx > 0) && (idx < D_mist_parking_8018D82C.count)) {
            flag = D_mist_parking_8018DA28[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1.value != 0)) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
