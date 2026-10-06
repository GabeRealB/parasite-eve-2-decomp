#include "rooms/dryfield_night_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_night_trailer_coach_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

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

#include "mapui/map_dryfield_full.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
#include "../../shared/room_cutscene.h"
/// Selects the nighttime trailer coach view that uses the finer depth shift.
///
/// Keep this binding through the depth-shift implementation fragment.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/trailer_coach.h"

/// The "%" suffix the room's percentage formatters append.
static u8 Telephone_Data_80181A78[];

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `roomCutsceneTask` spawns entry 1 for the scene.
extern TaskDesc gRoomCutsceneTaskDescs[];

#define TELEPHONE_TITLE_BYTES "Telephone\0N\xF2"
#include "../../shared/telephone.h"

/// The 0xFFFF-terminated item id lists `func_dryfield_night_trailer_coach_8017D81C`
/// chooses from.
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

/// The list returned when no case matches.
static u16 Shop_Data_80181AD4[];

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xFD"
#include "../../shared/shop.h"

s32 func_dryfield_night_trailer_coach_801826A0(Task*, s32, s32, s32);
s32 func_dryfield_night_trailer_coach_801826A8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_trailer_coach_801826EC(Task*, s32, s32, s32);
s32 func_dryfield_night_trailer_coach_80182800(Task*, s32, s32, s32);
s32 func_dryfield_night_trailer_coach_80182808(Task*, s32, s32, s32);

void func_dryfield_night_trailer_coach_8018243C(Task*);

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187A20;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation07DD8;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation07FD0;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation08324;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation0851C;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation08820;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation08C48;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation09284;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation0959C;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation098C8;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation09E10;
static AnimationSet         _gDryfieldNightTrailerCoachAnimation0A364;

void func_dryfield_night_trailer_coach_8018283C(void);
void func_dryfield_night_trailer_coach_80182864(void);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_dryfield_night_trailer_coach_801846D0 = { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } };

static TmdBone _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0PartVerts,
    _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Verts,
    _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Normals,
    _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Skeleton,
    _gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0Stream,
};

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation07DD8Bank1[3] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation07DD8Bank4[81] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation07DD8Records[128] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation07DD8Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation07DD8 = {
    _gDryfieldNightTrailerCoachAnimation07DD8Records,
    _gDryfieldNightTrailerCoachAnimation07DD8Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation07DD8Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation07DD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation07FD0Bank1[3] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation07FD0Bank4[34] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation07FD0Records[63] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation07FD0Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation07FD0 = {
    _gDryfieldNightTrailerCoachAnimation07FD0Records,
    _gDryfieldNightTrailerCoachAnimation07FD0Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation07FD0Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation07FD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation08324Bank1[5] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation08324Bank4[58] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation08324Records[120] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation08324Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation08324 = {
    _gDryfieldNightTrailerCoachAnimation08324Records,
    _gDryfieldNightTrailerCoachAnimation08324Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation08324Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation08324Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation0851CBank1[3] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation0851CBank4[34] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation0851CRecords[63] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation0851CIndices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation0851C = {
    _gDryfieldNightTrailerCoachAnimation0851CRecords,
    _gDryfieldNightTrailerCoachAnimation0851CIndices,
    { NULL, _gDryfieldNightTrailerCoachAnimation0851CBank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation0851CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation08820Bank1[5] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation08820Bank4[55] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation08820Records[103] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation08820Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation08820 = {
    _gDryfieldNightTrailerCoachAnimation08820Records,
    _gDryfieldNightTrailerCoachAnimation08820Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation08820Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation08820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation08C48Bank1[5] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation08C48Bank4[90] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation08C48Records[141] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation08C48Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation08C48 = {
    _gDryfieldNightTrailerCoachAnimation08C48Records,
    _gDryfieldNightTrailerCoachAnimation08C48Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation08C48Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation08C48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation09284Bank1[14] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation09284Bank4[135] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation09284Records[202] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation09284Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation09284 = {
    _gDryfieldNightTrailerCoachAnimation09284Records,
    _gDryfieldNightTrailerCoachAnimation09284Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation09284Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation09284Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation0959CBank1[7] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation0959CBank4[54] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation0959CRecords[103] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation0959CIndices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation0959C = {
    _gDryfieldNightTrailerCoachAnimation0959CRecords,
    _gDryfieldNightTrailerCoachAnimation0959CIndices,
    { NULL, _gDryfieldNightTrailerCoachAnimation0959CBank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation0959CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation098C8Bank1[7] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation098C8Bank4[56] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation098C8Records[106] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation098C8Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation098C8 = {
    _gDryfieldNightTrailerCoachAnimation098C8Records,
    _gDryfieldNightTrailerCoachAnimation098C8Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation098C8Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation098C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation09E10Bank1[8] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation09E10Bank4[121] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation09E10Records[173] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation09E10Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation09E10 = {
    _gDryfieldNightTrailerCoachAnimation09E10Records,
    _gDryfieldNightTrailerCoachAnimation09E10Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation09E10Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation09E10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightTrailerCoachAnimation0A364Bank1[9] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightTrailerCoachAnimation0A364Bank4[106] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_bank4.inc"
};

static AnimationRecord _gDryfieldNightTrailerCoachAnimation0A364Records[188] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_records.inc"
};

static u16 _gDryfieldNightTrailerCoachAnimation0A364Indices[20] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_indices.inc"
};

static AnimationSet _gDryfieldNightTrailerCoachAnimation0A364 = {
    _gDryfieldNightTrailerCoachAnimation0A364Records,
    _gDryfieldNightTrailerCoachAnimation0A364Indices,
    { NULL, _gDryfieldNightTrailerCoachAnimation0A364Bank1, NULL, NULL, _gDryfieldNightTrailerCoachAnimation0A364Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_dryfield_night_trailer_coach_8018794C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_trailer_coach_801826A8 },
    { 5105, func_dryfield_night_trailer_coach_801826A0 },
    { 5103, func_dryfield_night_trailer_coach_80182800 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_trailer_coach_801826EC },
    { ROOM_MESSAGE_SOUND, func_dryfield_night_trailer_coach_80182808 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_trailer_coach_8018797C = { { { TASK_BODY_NONE, 32 } }, func_dryfield_night_trailer_coach_8018243C, { .value = 0 } };

ActorTransform D_dryfield_night_trailer_coach_80187988 = { { 4870, 0, -900, 0 }, { 0, -1536, 0, 0 } };

ActorTransform D_dryfield_night_trailer_coach_801879A0 = { { 4600, 0, -1300, 0 }, { 0, -1479, 0, 0 } };

ActorTransform D_dryfield_night_trailer_coach_801879B8 = { { 4400, 0, -2400, 0 }, { 0, 512, 0, 0 } };

AnimationPlayRequest D_dryfield_night_trailer_coach_801879D0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_801879E4 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_801879F8 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187A0C = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187A20 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187A34 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187A48[4] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_night_trailer_coach_80187A98 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187AAC = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187AC0 = { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187AD4 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187AE8[2] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B10 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B24 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B38 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B4C = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B60 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B74 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B88 = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187B9C[2] = {
    { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_night_trailer_coach_80187BC4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187BD8 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187BEC = { { .index = 1 }, 3, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187C00[3] = {
    { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_night_trailer_coach_80187C3C = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187C50 = { { .index = 1 }, 2, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187C64 = { { .index = 2 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187C78 = { { .index = 2 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187C8C = { { .index = 2 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187CA0 = { { .index = 2 }, 3, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_dryfield_night_trailer_coach_80187CB4[12] = {
    &_gDryfieldNightTrailerCoachAnimation09E10,
    &_gDryfieldNightTrailerCoachAnimation08C48,
    &_gDryfieldNightTrailerCoachAnimation07FD0,
    &_gDryfieldNightTrailerCoachAnimation08324,
    &_gDryfieldNightTrailerCoachAnimation0851C,
    &_gDryfieldNightTrailerCoachAnimation0A364,
    &_gDryfieldNightTrailerCoachAnimation08820,
    &_gDryfieldNightTrailerCoachAnimation07DD8,
    &_gDryfieldNightTrailerCoachAnimation09284,
    &_gDryfieldNightTrailerCoachAnimation0959C,
    &_gDryfieldNightTrailerCoachAnimation098C8,
    NULL,
};

AnimationBankCopyRequest D_dryfield_night_trailer_coach_80187CE4 = { { .sets = D_dryfield_night_trailer_coach_80187CB4 }, ARRAY_SIZE(D_dryfield_night_trailer_coach_80187CB4) - 1 };

AnimationPlayRequest D_dryfield_night_trailer_coach_80187CEC = { { .index = 6 }, 34, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_dryfield_night_trailer_coach_80187D00[25] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187CE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187988 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187C78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x531B0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187C8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x531B0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_80187A20 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187BC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187C50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_trailer_coach_80187F58[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187988 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_trailer_coach_801880A8[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_trailer_coach_8018283C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_trailer_coach_801881F8[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = func_800D4D2C }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_trailer_coach_80188348[19] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_trailer_coach_80182864 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_trailer_coach_80188510[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187BC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187BD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187C3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_trailer_coach_80182864 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
static u8 Shop_Data_80181A20[];

static u8 Shop_Data_80181A0C[];

static UiObjectDesc Shop_Data_80181BD8;

static u8 Shop_Data_80181A1C[];

static UiObjectDesc Shop_Data_80181B84;

static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT];

/// Messages and labels of the shop's panels.
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

/// Descriptor of the panel `func_dryfield_night_trailer_coach_8017FEC0` opens.
static UiObjectDesc Shop_Data_80181B30;

static u8 Telephone_Data_80181A20[];

static u8 Telephone_Data_80181A28[];

static u8 Telephone_Data_80181A2C[];

static u8 Telephone_Data_80181A34[];

static u8 Telephone_Data_80181A40[];

static u8 Telephone_Data_80181A50[];

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

static const char Telephone_Data_8017D638[];

extern UiObjectDesc D_800611E4;

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
static UiList Telephone_Data_80181C6C;

static UiList Telephone_Data_80181CF4;

static UiObjectDesc Telephone_Data_80181C90;

/// List of the menu panel `func_dryfield_night_trailer_coach_80181844` draws.
static UiList Telephone_Data_80181C44;

/// Texts of the four menu rows below, and the panels two of them open.
static u8 Telephone_Data_801819F8[];

static u8 Telephone_Data_80181A00[];

static u8 Telephone_Data_80181A0C[];

static u8 Telephone_Data_80181A18[];

static UiObjectDesc Telephone_Data_80181CAC;

static UiObjectDesc Telephone_Data_80181CC8;

extern TaskMessageEntry D_dryfield_night_trailer_coach_8018794C[6];

extern ActorTransform D_dryfield_night_trailer_coach_801879B8;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187CEC;

extern EvsCommand D_dryfield_night_trailer_coach_80187D00[];

extern EvsCommand D_dryfield_night_trailer_coach_80187F58[];

extern EvsCommand D_dryfield_night_trailer_coach_801880A8[];

extern EvsCommand D_dryfield_night_trailer_coach_801881F8[];

extern EvsCommand D_dryfield_night_trailer_coach_80188348[];

extern EvsCommand D_dryfield_night_trailer_coach_80188510[];

static void func_dryfield_night_trailer_coach_8018231C(Task* task);

/// Cutscene trigger for the trailer coach at night. Where the day version has
/// its own record and a save-view reset, this one only runs at the two ends of
/// the visit.
///
/// Request 0xE forces area 8 for the scene, fills the room's cutscene record
/// the same way the motel lobby fills its own -- save view 8, slot 1, and the
/// cap file picked by `gameFlagGetNibble(0x7A)` (file 1 below four, file 2 at
/// four or more) -- then hands it to `gRoomCutsceneTaskDescs`. Request 3
/// spawns entry 0 of the room's task table at `0x8018797C` and request 0x17
/// asks the cap system to run command 0x17. Always returns 0.
extern TaskDesc D_dryfield_night_trailer_coach_8018797C;

static inline s32 Shop_AddItemCount(s32 item, s32 count);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/telephone.inc.c"

void func_dryfield_night_trailer_coach_8018138C(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

#include "../../shared/room_cutscene_task.inc.c"

/// State handlers of the room's task `func_dryfield_night_trailer_coach_801828CC`
/// runs: its set-up, a per-frame state and the kill.
static const TaskFuncTable3 D_dryfield_night_trailer_coach_8017D7DC = {
    {
        func_dryfield_night_trailer_coach_8018231C,
        trailerCoachSetDepthShift,
        taskKill,
    },
};

static void func_dryfield_night_trailer_coach_8018231C(Task* task)
{
    s32* state;

    task->msgTable = D_dryfield_night_trailer_coach_8018794C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), 0x3E9, &D_dryfield_night_trailer_coach_801879B8, 0);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &D_dryfield_night_trailer_coach_80187CEC, 0);
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 2) {
        func_800E8634(D_dryfield_night_trailer_coach_80187D00, 0, D_dryfield_night_trailer_coach_80187F58);
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 3) {
        func_800E8634(D_dryfield_night_trailer_coach_80189080, 0, D_dryfield_night_trailer_coach_801892C0);
        Gp_SetCurBit2Flag(0x22, 1);
    }
    if (func_800E3FCC(0xA2) == 0x25) {
        func_800E3FAC(0xA2, 0x26);
    }
    gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_8X;
    /* Through a pointer rather than as `task->state`: a member access is
       struct memory, which the scheduler lets pass the store above it, and the
       original keeps the two in source order. */
    state   = &task->state;
    *state += 1;
}

void func_dryfield_night_trailer_coach_8018243C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) == 0) {
                func_800E8614(D_dryfield_night_trailer_coach_801880A8, 1);
                task->state++;
                break;
            }
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN) == 0 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 4) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN, 1);
                func_800E8614(D_dryfield_night_trailer_coach_80188510, 0);
            } else {
                func_800E8614(D_dryfield_night_trailer_coach_80188348, 0);
            }
            taskKill(task);
            break;
        case 2:
            if (capGetVariantKey() == 0xB) {
                func_800E8614(D_dryfield_night_trailer_coach_801881F8, 0);
            } else if (capGetVariantKey() == 0xC) {
                func_800E8614(D_dryfield_night_trailer_coach_80188708, 1);
            } else if (capGetVariantKey() == 0xD) {
                if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN) == 0) {
                    gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN, 1);
                    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
                    Gp_ApplyAreaRecs(D_dryfield_night_trailer_coach_8018C208);
                    func_800E8634(D_dryfield_night_trailer_coach_801889A8, 1,
                                  D_dryfield_night_trailer_coach_80188F00);
                    func_800E3FAC(0xA2, 0x13);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 2;
                } else {
                    func_800E8614(D_dryfield_night_trailer_coach_80188858, 1);
                }
            }
            task->state++;
            break;
        case 1:
        case 3:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case 4:
            taskKill(task);
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_dryfield_night_trailer_coach_801826A0(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_map_dryfield_full_80179954`. Always returns 1.
s32 func_dryfield_night_trailer_coach_801826A8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_dryfield_full_80179954(in, out);
    return 1;
}

s32 func_dryfield_night_trailer_coach_801826EC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0xE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
        D_dryfield_night_trailer_coach_8018C21C.view               = 8;
        D_dryfield_night_trailer_coach_8018C21C.capSlot            = 1;
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < 4) {
            D_dryfield_night_trailer_coach_8018C21C.capTPageX = 0x380;
            D_dryfield_night_trailer_coach_8018C21C.capFile   = 1;
        } else {
            D_dryfield_night_trailer_coach_8018C21C.capTPageX = 0x3C0;
            D_dryfield_night_trailer_coach_8018C21C.capFile   = 2;
        }
        D_dryfield_night_trailer_coach_8018C21C.skipScene       = 0;
        D_dryfield_night_trailer_coach_8018C21C.startSound      = 0x531B0003;
        D_dryfield_night_trailer_coach_8018C21C.endSound        = 0x531B0005;
        D_dryfield_night_trailer_coach_8018C21C.sceneSound      = 0x531B0004;
        D_dryfield_night_trailer_coach_8018C21C.afterSceneSound = 0x531B0006;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 3, &D_dryfield_night_trailer_coach_8018C21C);
    }
    if (arg2 == 3) {
        taskSpawnFromTable(&D_dryfield_night_trailer_coach_8018797C, 0, 0, 0);
    }
    if (arg2 == 0x17) {
        Gp_RunCapCmd1(0x17);
    }
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182800(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182808(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 0x0D), 0, 0);
    }
    return 0;
}

void func_dryfield_night_trailer_coach_8018283C(void)
{
    Gp_StartCapSlot(9, 0, 1);
}

void func_dryfield_night_trailer_coach_80182864(void)
{
    func_800D4D2C((gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN) == 0) ? 0x20 : 0x21);
}

#include "../../shared/trailer_coach_set_depth_shift.inc.c"

/// Runs the handler for the task's state from a stack copy of
/// `D_dryfield_night_trailer_coach_8017D7DC`.
void func_dryfield_night_trailer_coach_801828CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_trailer_coach_8017D7DC;
    sp.funcs[task->state](task);
}
