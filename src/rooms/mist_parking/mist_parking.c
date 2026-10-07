#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "mist_parking_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

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
#include "rooms/shop_tier.h"
#include "../../shared/room_cutscene.h"

static void _roomCutsceneSoundTask(Task* task);

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

s32  func_mist_parking_801823F8(Task*, s32, s32, s32);
s32  func_mist_parking_801826B8(Task*, s32, s32, s32);
s32  func_mist_parking_801826C0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_mist_parking_801826E8(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3);
void func_mist_parking_80182750(s32);
void func_mist_parking_801827A0(s32);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_mist_parking_8018668C = { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } };

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static AnimationPackedPose _gMistParkingAnimation095D0Bank1[2] = {
#include "assets/mist_parking_animation_095D0_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation095D0Bank4[17] = {
#include "assets/mist_parking_animation_095D0_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation095D0Records[76] = {
#include "assets/mist_parking_animation_095D0_records.inc"
};

static u16 _gMistParkingAnimation095D0Indices[20] = {
#include "assets/mist_parking_animation_095D0_indices.inc"
};

static AnimationSet _gMistParkingAnimation095D0 = {
    _gMistParkingAnimation095D0Records,
    _gMistParkingAnimation095D0Indices,
    { NULL, _gMistParkingAnimation095D0Bank1, NULL, NULL, _gMistParkingAnimation095D0Bank4, NULL, NULL, NULL },
};

// Message-table callbacks use the argument views required by this TU.

TaskMessageEntry D_mist_parking_80186BB8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mist_parking_801826C0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mist_parking_801826E8 },
    { 5105, func_mist_parking_801826B8 },
    { ROOM_MESSAGE_COMMAND, func_mist_parking_801823F8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorTransform D_mist_parking_80186BE0 = { { 8448, 1, -2599, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_mist_parking_80186BF8 = { { 8448, 1, -3828, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_mist_parking_80186C10 = { { 3310, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

AnimationSet* D_mist_parking_80186C28[1] = {
    &_gMistParkingAnimation095D0,
};

AnimationBankCopyRequest D_mist_parking_80186C2C = { { .sets = D_mist_parking_80186C28 }, ARRAY_SIZE(D_mist_parking_80186C28) };

AnimationPlayRequest D_mist_parking_80186C34 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_80186C48 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_mist_parking_80186C5C[15] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_801827A0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_80186BE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80182750 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80186DC4[13] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_80186BF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80182750 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80186EFC[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_80186C10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80186C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C34 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gMistParkingModel09B9CSkeleton[1] = {
#include "assets/mist_parking_model_09B9C_skeleton.inc"
};

static u32 _gMistParkingModel09B9CPartVerts[1] = {
#include "assets/mist_parking_model_09B9C_partVerts.inc"
};

static SVECTOR _gMistParkingModel09B9CVerts[35] = {
#include "assets/mist_parking_model_09B9C_verts.inc"
};

static u32 _gMistParkingModel09B9CStream[78] = {
#include "assets/mist_parking_model_09B9C_stream.inc"
};

TmdSource gMistParkingModel09B9C = {
    0,
    576,
    0,
    1,
    _gMistParkingModel09B9CPartVerts,
    _gMistParkingModel09B9CVerts,
    &_gMistParkingModel09B9CVerts[35],
    _gMistParkingModel09B9CSkeleton,
    _gMistParkingModel09B9CStream,
};

static AnimationPackedPose _gMistParkingAnimation09FD4Bank1[6] = {
#include "assets/mist_parking_animation_09FD4_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation09FD4Bank4[46] = {
#include "assets/mist_parking_animation_09FD4_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation09FD4Records[109] = {
#include "assets/mist_parking_animation_09FD4_records.inc"
};

static u16 _gMistParkingAnimation09FD4Indices[20] = {
#include "assets/mist_parking_animation_09FD4_indices.inc"
};

AnimationSet gMistParkingAnimation09FD4 = {
    _gMistParkingAnimation09FD4Records,
    _gMistParkingAnimation09FD4Indices,
    { NULL, _gMistParkingAnimation09FD4Bank1, NULL, NULL, _gMistParkingAnimation09FD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0A774Bank1[13] = {
#include "assets/mist_parking_animation_0A774_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0A774Bank4[179] = {
#include "assets/mist_parking_animation_0A774_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0A774Records[250] = {
#include "assets/mist_parking_animation_0A774_records.inc"
};

static u16 _gMistParkingAnimation0A774Indices[20] = {
#include "assets/mist_parking_animation_0A774_indices.inc"
};

AnimationSet gMistParkingAnimation0A774 = {
    _gMistParkingAnimation0A774Records,
    _gMistParkingAnimation0A774Indices,
    { NULL, _gMistParkingAnimation0A774Bank1, NULL, NULL, _gMistParkingAnimation0A774Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0AC5CBank1[6] = {
#include "assets/mist_parking_animation_0AC5C_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0AC5CBank4[110] = {
#include "assets/mist_parking_animation_0AC5C_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0AC5CRecords[166] = {
#include "assets/mist_parking_animation_0AC5C_records.inc"
};

static u16 _gMistParkingAnimation0AC5CIndices[20] = {
#include "assets/mist_parking_animation_0AC5C_indices.inc"
};

AnimationSet gMistParkingAnimation0AC5C = {
    _gMistParkingAnimation0AC5CRecords,
    _gMistParkingAnimation0AC5CIndices,
    { NULL, _gMistParkingAnimation0AC5CBank1, NULL, NULL, _gMistParkingAnimation0AC5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0B138Bank1[9] = {
#include "assets/mist_parking_animation_0B138_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0B138Bank4[117] = {
#include "assets/mist_parking_animation_0B138_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0B138Records[147] = {
#include "assets/mist_parking_animation_0B138_records.inc"
};

static u16 _gMistParkingAnimation0B138Indices[20] = {
#include "assets/mist_parking_animation_0B138_indices.inc"
};

AnimationSet gMistParkingAnimation0B138 = {
    _gMistParkingAnimation0B138Records,
    _gMistParkingAnimation0B138Indices,
    { NULL, _gMistParkingAnimation0B138Bank1, NULL, NULL, _gMistParkingAnimation0B138Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0B700Bank1[10] = {
#include "assets/mist_parking_animation_0B700_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0B700Bank4[143] = {
#include "assets/mist_parking_animation_0B700_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0B700Records[177] = {
#include "assets/mist_parking_animation_0B700_records.inc"
};

static u16 _gMistParkingAnimation0B700Indices[20] = {
#include "assets/mist_parking_animation_0B700_indices.inc"
};

AnimationSet gMistParkingAnimation0B700 = {
    _gMistParkingAnimation0B700Records,
    _gMistParkingAnimation0B700Indices,
    { NULL, _gMistParkingAnimation0B700Bank1, NULL, NULL, _gMistParkingAnimation0B700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0BAB4Bank1[5] = {
#include "assets/mist_parking_animation_0BAB4_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0BAB4Bank4[65] = {
#include "assets/mist_parking_animation_0BAB4_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0BAB4Records[137] = {
#include "assets/mist_parking_animation_0BAB4_records.inc"
};

static u16 _gMistParkingAnimation0BAB4Indices[20] = {
#include "assets/mist_parking_animation_0BAB4_indices.inc"
};

AnimationSet gMistParkingAnimation0BAB4 = {
    _gMistParkingAnimation0BAB4Records,
    _gMistParkingAnimation0BAB4Indices,
    { NULL, _gMistParkingAnimation0BAB4Bank1, NULL, NULL, _gMistParkingAnimation0BAB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0C1B0Bank1[12] = {
#include "assets/mist_parking_animation_0C1B0_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0C1B0Bank4[153] = {
#include "assets/mist_parking_animation_0C1B0_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0C1B0Records[238] = {
#include "assets/mist_parking_animation_0C1B0_records.inc"
};

static u16 _gMistParkingAnimation0C1B0Indices[20] = {
#include "assets/mist_parking_animation_0C1B0_indices.inc"
};

AnimationSet gMistParkingAnimation0C1B0 = {
    _gMistParkingAnimation0C1B0Records,
    _gMistParkingAnimation0C1B0Indices,
    { NULL, _gMistParkingAnimation0C1B0Bank1, NULL, NULL, _gMistParkingAnimation0C1B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0C688Bank1[7] = {
#include "assets/mist_parking_animation_0C688_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0C688Bank4[94] = {
#include "assets/mist_parking_animation_0C688_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0C688Records[175] = {
#include "assets/mist_parking_animation_0C688_records.inc"
};

static u16 _gMistParkingAnimation0C688Indices[20] = {
#include "assets/mist_parking_animation_0C688_indices.inc"
};

AnimationSet gMistParkingAnimation0C688 = {
    _gMistParkingAnimation0C688Records,
    _gMistParkingAnimation0C688Indices,
    { NULL, _gMistParkingAnimation0C688Bank1, NULL, NULL, _gMistParkingAnimation0C688Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0CD80Bank1[12] = {
#include "assets/mist_parking_animation_0CD80_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0CD80Bank4[177] = {
#include "assets/mist_parking_animation_0CD80_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0CD80Records[213] = {
#include "assets/mist_parking_animation_0CD80_records.inc"
};

static u16 _gMistParkingAnimation0CD80Indices[20] = {
#include "assets/mist_parking_animation_0CD80_indices.inc"
};

AnimationSet gMistParkingAnimation0CD80 = {
    _gMistParkingAnimation0CD80Records,
    _gMistParkingAnimation0CD80Indices,
    { NULL, _gMistParkingAnimation0CD80Bank1, NULL, NULL, _gMistParkingAnimation0CD80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0D060Bank1[3] = {
#include "assets/mist_parking_animation_0D060_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0D060Bank4[63] = {
#include "assets/mist_parking_animation_0D060_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0D060Records[92] = {
#include "assets/mist_parking_animation_0D060_records.inc"
};

static u16 _gMistParkingAnimation0D060Indices[20] = {
#include "assets/mist_parking_animation_0D060_indices.inc"
};

AnimationSet gMistParkingAnimation0D060 = {
    _gMistParkingAnimation0D060Records,
    _gMistParkingAnimation0D060Indices,
    { NULL, _gMistParkingAnimation0D060Bank1, NULL, NULL, _gMistParkingAnimation0D060Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0D3E4Bank1[6] = {
#include "assets/mist_parking_animation_0D3E4_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0D3E4Bank4[69] = {
#include "assets/mist_parking_animation_0D3E4_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0D3E4Records[118] = {
#include "assets/mist_parking_animation_0D3E4_records.inc"
};

static u16 _gMistParkingAnimation0D3E4Indices[20] = {
#include "assets/mist_parking_animation_0D3E4_indices.inc"
};

AnimationSet gMistParkingAnimation0D3E4 = {
    _gMistParkingAnimation0D3E4Records,
    _gMistParkingAnimation0D3E4Indices,
    { NULL, _gMistParkingAnimation0D3E4Bank1, NULL, NULL, _gMistParkingAnimation0D3E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0D6A4Bank1[4] = {
#include "assets/mist_parking_animation_0D6A4_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0D6A4Bank4[40] = {
#include "assets/mist_parking_animation_0D6A4_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0D6A4Records[104] = {
#include "assets/mist_parking_animation_0D6A4_records.inc"
};

static u16 _gMistParkingAnimation0D6A4Indices[20] = {
#include "assets/mist_parking_animation_0D6A4_indices.inc"
};

AnimationSet gMistParkingAnimation0D6A4 = {
    _gMistParkingAnimation0D6A4Records,
    _gMistParkingAnimation0D6A4Indices,
    { NULL, _gMistParkingAnimation0D6A4Bank1, NULL, NULL, _gMistParkingAnimation0D6A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0DA94Bank1[7] = {
#include "assets/mist_parking_animation_0DA94_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0DA94Bank4[62] = {
#include "assets/mist_parking_animation_0DA94_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0DA94Records[149] = {
#include "assets/mist_parking_animation_0DA94_records.inc"
};

static u16 _gMistParkingAnimation0DA94Indices[20] = {
#include "assets/mist_parking_animation_0DA94_indices.inc"
};

AnimationSet gMistParkingAnimation0DA94 = {
    _gMistParkingAnimation0DA94Records,
    _gMistParkingAnimation0DA94Indices,
    { NULL, _gMistParkingAnimation0DA94Bank1, NULL, NULL, _gMistParkingAnimation0DA94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0DE94Bank1[10] = {
#include "assets/mist_parking_animation_0DE94_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0DE94Bank4[85] = {
#include "assets/mist_parking_animation_0DE94_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0DE94Records[121] = {
#include "assets/mist_parking_animation_0DE94_records.inc"
};

static u16 _gMistParkingAnimation0DE94Indices[20] = {
#include "assets/mist_parking_animation_0DE94_indices.inc"
};

AnimationSet gMistParkingAnimation0DE94 = {
    _gMistParkingAnimation0DE94Records,
    _gMistParkingAnimation0DE94Indices,
    { NULL, _gMistParkingAnimation0DE94Bank1, NULL, NULL, _gMistParkingAnimation0DE94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0E1D0Bank1[8] = {
#include "assets/mist_parking_animation_0E1D0_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0E1D0Bank4[65] = {
#include "assets/mist_parking_animation_0E1D0_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0E1D0Records[98] = {
#include "assets/mist_parking_animation_0E1D0_records.inc"
};

static u16 _gMistParkingAnimation0E1D0Indices[20] = {
#include "assets/mist_parking_animation_0E1D0_indices.inc"
};

AnimationSet gMistParkingAnimation0E1D0 = {
    _gMistParkingAnimation0E1D0Records,
    _gMistParkingAnimation0E1D0Indices,
    { NULL, _gMistParkingAnimation0E1D0Bank1, NULL, NULL, _gMistParkingAnimation0E1D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0E6E0Bank1[10] = {
#include "assets/mist_parking_animation_0E6E0_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0E6E0Bank4[109] = {
#include "assets/mist_parking_animation_0E6E0_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0E6E0Records[165] = {
#include "assets/mist_parking_animation_0E6E0_records.inc"
};

static u16 _gMistParkingAnimation0E6E0Indices[20] = {
#include "assets/mist_parking_animation_0E6E0_indices.inc"
};

AnimationSet gMistParkingAnimation0E6E0 = {
    _gMistParkingAnimation0E6E0Records,
    _gMistParkingAnimation0E6E0Indices,
    { NULL, _gMistParkingAnimation0E6E0Bank1, NULL, NULL, _gMistParkingAnimation0E6E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0EA0CBank1[7] = {
#include "assets/mist_parking_animation_0EA0C_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0EA0CBank4[56] = {
#include "assets/mist_parking_animation_0EA0C_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0EA0CRecords[106] = {
#include "assets/mist_parking_animation_0EA0C_records.inc"
};

static u16 _gMistParkingAnimation0EA0CIndices[20] = {
#include "assets/mist_parking_animation_0EA0C_indices.inc"
};

AnimationSet gMistParkingAnimation0EA0C = {
    _gMistParkingAnimation0EA0CRecords,
    _gMistParkingAnimation0EA0CIndices,
    { NULL, _gMistParkingAnimation0EA0CBank1, NULL, NULL, _gMistParkingAnimation0EA0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0EDE0Bank1[8] = {
#include "assets/mist_parking_animation_0EDE0_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0EDE0Bank4[84] = {
#include "assets/mist_parking_animation_0EDE0_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0EDE0Records[117] = {
#include "assets/mist_parking_animation_0EDE0_records.inc"
};

static u16 _gMistParkingAnimation0EDE0Indices[20] = {
#include "assets/mist_parking_animation_0EDE0_indices.inc"
};

AnimationSet gMistParkingAnimation0EDE0 = {
    _gMistParkingAnimation0EDE0Records,
    _gMistParkingAnimation0EDE0Indices,
    { NULL, _gMistParkingAnimation0EDE0Bank1, NULL, NULL, _gMistParkingAnimation0EDE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0F14CBank1[7] = {
#include "assets/mist_parking_animation_0F14C_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0F14CBank4[74] = {
#include "assets/mist_parking_animation_0F14C_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0F14CRecords[104] = {
#include "assets/mist_parking_animation_0F14C_records.inc"
};

static u16 _gMistParkingAnimation0F14CIndices[20] = {
#include "assets/mist_parking_animation_0F14C_indices.inc"
};

AnimationSet gMistParkingAnimation0F14C = {
    _gMistParkingAnimation0F14CRecords,
    _gMistParkingAnimation0F14CIndices,
    { NULL, _gMistParkingAnimation0F14CBank1, NULL, NULL, _gMistParkingAnimation0F14CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0F574Bank1[5] = {
#include "assets/mist_parking_animation_0F574_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0F574Bank4[90] = {
#include "assets/mist_parking_animation_0F574_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0F574Records[141] = {
#include "assets/mist_parking_animation_0F574_records.inc"
};

static u16 _gMistParkingAnimation0F574Indices[20] = {
#include "assets/mist_parking_animation_0F574_indices.inc"
};

AnimationSet gMistParkingAnimation0F574 = {
    _gMistParkingAnimation0F574Records,
    _gMistParkingAnimation0F574Indices,
    { NULL, _gMistParkingAnimation0F574Bank1, NULL, NULL, _gMistParkingAnimation0F574Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0F7F0Bank1[3] = {
#include "assets/mist_parking_animation_0F7F0_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0F7F0Bank4[43] = {
#include "assets/mist_parking_animation_0F7F0_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0F7F0Records[87] = {
#include "assets/mist_parking_animation_0F7F0_records.inc"
};

static u16 _gMistParkingAnimation0F7F0Indices[20] = {
#include "assets/mist_parking_animation_0F7F0_indices.inc"
};

AnimationSet gMistParkingAnimation0F7F0 = {
    _gMistParkingAnimation0F7F0Records,
    _gMistParkingAnimation0F7F0Indices,
    { NULL, _gMistParkingAnimation0F7F0Bank1, NULL, NULL, _gMistParkingAnimation0F7F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0FC60Bank1[5] = {
#include "assets/mist_parking_animation_0FC60_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0FC60Bank4[99] = {
#include "assets/mist_parking_animation_0FC60_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0FC60Records[150] = {
#include "assets/mist_parking_animation_0FC60_records.inc"
};

static u16 _gMistParkingAnimation0FC60Indices[20] = {
#include "assets/mist_parking_animation_0FC60_indices.inc"
};

AnimationSet gMistParkingAnimation0FC60 = {
    _gMistParkingAnimation0FC60Records,
    _gMistParkingAnimation0FC60Indices,
    { NULL, _gMistParkingAnimation0FC60Bank1, NULL, NULL, _gMistParkingAnimation0FC60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation0FE5CBank1[4] = {
#include "assets/mist_parking_animation_0FE5C_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation0FE5CBank4[17] = {
#include "assets/mist_parking_animation_0FE5C_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation0FE5CRecords[78] = {
#include "assets/mist_parking_animation_0FE5C_records.inc"
};

static u16 _gMistParkingAnimation0FE5CIndices[20] = {
#include "assets/mist_parking_animation_0FE5C_indices.inc"
};

AnimationSet gMistParkingAnimation0FE5C = {
    _gMistParkingAnimation0FE5CRecords,
    _gMistParkingAnimation0FE5CIndices,
    { NULL, _gMistParkingAnimation0FE5CBank1, NULL, NULL, _gMistParkingAnimation0FE5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation10174Bank1[7] = {
#include "assets/mist_parking_animation_10174_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation10174Bank4[54] = {
#include "assets/mist_parking_animation_10174_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation10174Records[103] = {
#include "assets/mist_parking_animation_10174_records.inc"
};

static u16 _gMistParkingAnimation10174Indices[20] = {
#include "assets/mist_parking_animation_10174_indices.inc"
};

AnimationSet gMistParkingAnimation10174 = {
    _gMistParkingAnimation10174Records,
    _gMistParkingAnimation10174Indices,
    { NULL, _gMistParkingAnimation10174Bank1, NULL, NULL, _gMistParkingAnimation10174Bank4, NULL, NULL, NULL },
};

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
static u8 Shop_Data_80181A20[];

static u8 Shop_Data_80181A0C[];

static u8 Shop_Data_80181A1C[];

static UiObjectDesc Shop_Data_80181B84;

static UiObjectDesc Shop_Data_80181BD8;

static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT];

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
static UiListRowCallback Shop_Data_80181AD8[];

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

extern EvsCommand D_mist_parking_80186EFC[];

static void func_mist_parking_801827C0(Task* arg0);

static void func_mist_parking_80182888(Task* task);

extern TaskMessageEntry D_mist_parking_80186BB8[5];

extern EvsCommand D_mist_parking_80186C5C[];

extern EvsCommand D_mist_parking_80186DC4[];

static inline s32 Shop_AddItemCount(s32 item, s32 count);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/telephone.inc.c"

void func_mist_parking_80181468(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// The area records applied when the scene hands the Dryfield story on.

#include "../../shared/room_cutscene_task.inc.c"

s32 func_mist_parking_801823F8(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameSession* session;
    u8           temp;

    switch (arg2) {
        case 15:
            temp = gGameSession->location.loc.variant;
            if (temp == 2) {
                if (gameFlagGetNibble(GAME_FLAG_0F1) == 1) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    evsStartScript(D_mist_parking_8018F0A4, EVENT_SCRIPT_HUD_KEEP);
                    gameFlagSetNibble(GAME_FLAG_0F1, 2);
                } else if (gameFlagGetNibble(GAME_FLAG_0F1) == temp) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    evsStartScript(D_mist_parking_8018F194, EVENT_SCRIPT_HUD_KEEP);
                    gameFlagSetNibble(GAME_FLAG_0F1, 3);
                } else if (gameFlagGetNibble(GAME_FLAG_0F1) == 3) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(D_mist_parking_8018D75C, 8, 0, 0);
                }
            } else if (gameFlagGetNibble(GAME_FLAG_0ED) == 1) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(D_mist_parking_80190824, 4, 0, 0);
            }
            break;
        case 8:
            D_mist_parking_8019533C.view            = 9;
            D_mist_parking_8019533C.capSlot         = 1;
            D_mist_parking_8019533C.capFile         = 3;
            D_mist_parking_8019533C.skipScene       = 0;
            D_mist_parking_8019533C.startSound      = 0x51130003;
            D_mist_parking_8019533C.endSound        = 0x51130004;
            D_mist_parking_8019533C.sceneSound      = 0x5113000B;
            D_mist_parking_8019533C.afterSceneSound = 0x51130012;
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 4, &D_mist_parking_8019533C);
            session                                                    = gGameSession;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 2;
            session->location.loc.warp                                 = 2;
            break;
        case 18:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            if (gGameSession->location.loc.variant == 1) {
                taskSpawnFromTable(D_mist_parking_80190824, 3, 0, 0);
            } else {
                taskSpawnFromTable(D_mist_parking_8018D75C, 7, 0, 0);
            }
            break;
        case 1:
            evsStartScript(D_mist_parking_80186EFC, EVENT_SCRIPT_HUD_KEEP);
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
s32 func_mist_parking_801826B8(Task* task, s32 msgId, s32 arg2, s32 arg3)
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

s32 func_mist_parking_801826E8(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3)
{
    if (request->actionId == 1) {
        evsStartScript(D_mist_parking_80186C5C, EVENT_SCRIPT_HUD_KEEP);
    }
    if (request->actionId == 2) {
        evsStartScript(D_mist_parking_80186DC4, EVENT_SCRIPT_HUD_KEEP);
        gameFlagSetNibble(GAME_FLAG_0ED, 1);
    }
    return 1;
}

void func_mist_parking_80182750(s32 arg0)
{
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) != 0) {
        arg0 += 2;
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = arg0;
    gGameSession->location.loc.room                            = arg0;
    gGameSession->roomObjsDirty                                = 1;
}

void func_mist_parking_801827A0(s32 arg0)
{
    capSpawnEventIfIdle(arg0, CAP_EVENT_NO_FLAGS);
}

static void func_mist_parking_801827C0(Task* arg0)
{
    arg0->msgTable = D_mist_parking_80186BB8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == 2) && (gameFlagGetNibble(GAME_FLAG_0F1) == 0)) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 3) {
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x3C);
            mistParkingResetCutsceneTaskHandles(0);
            evsStartScriptWithSkip(D_mist_parking_8018DF34, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mist_parking_8018EDBC);
        } else {
            func_mist_parking_8018471C(0);
            evsStartScript(D_mist_parking_8018EFE4, EVENT_SCRIPT_HUD_KEEP);
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
    Enemy*     enemy;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    if (D_801156F9 == 0) {
        idx = actor->animationSlots[1].nextPose.indices.setIndex - ANIMATION_BANK_BASE_SET_COUNT;
        if ((idx > 0) && (idx < D_mist_parking_8018D82C.wordCount)) {
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
            enemy = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), enemy->task, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
