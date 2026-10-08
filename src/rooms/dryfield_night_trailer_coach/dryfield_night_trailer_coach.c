#include "rooms/dryfield_night_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_night_trailer_coach_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
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

static void _trailerCoachSetDepthShift(Task* unusedTask);

static void _roomCutsceneSoundTask(Task* task);

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

enum { DRYFIELD_NIGHT_TRAILER_COACH_LATE_CHAPTER = 4 };

static s32 _dryfieldNightTrailerCoachRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightTrailerCoachResolveRoomVariantMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightTrailerCoachCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightTrailerCoachIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _dryfieldNightTrailerCoachSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

static void _dryfieldNightTrailerCoachTalkTask(Task* task);

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

static void _dryfieldNightTrailerCoachStartTalkVariant1(void);
static void _dryfieldNightTrailerCoachOpenShop(void);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_dryfield_night_trailer_coach_801846D0 = { { { TASK_BODY_NONE, 192 } }, _shopSessionTask, { .value = 0 } };

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
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
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
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightTrailerCoachResolveRoomVariantMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightTrailerCoachRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightTrailerCoachIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightTrailerCoachCommandMessage },
    { ROOM_MESSAGE_SOUND, _dryfieldNightTrailerCoachSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_trailer_coach_8018797C = { { { TASK_BODY_NONE, 32 } }, _dryfieldNightTrailerCoachTalkTask, { .value = 0 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightTrailerCoachStartTalkVariant1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_trailer_coach_801881F8[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightTrailerCoachOpenShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightTrailerCoachOpenShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_trailer_coach_80187B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
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

static void _dryfieldNightTrailerCoachInitRoomTask(Task* task);

extern TaskDesc D_dryfield_night_trailer_coach_8018797C;

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/telephone.inc.c"

void dryfieldNightTrailerCoachTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

#include "../../shared/room_cutscene_task.inc.c"

/// State handlers of the room's task `dryfieldNightTrailerCoachRoomTask`
/// runs: its set-up, a per-frame state and the kill.
static const TaskFuncTable3 D_dryfield_night_trailer_coach_8017D7DC = {
    {
        _dryfieldNightTrailerCoachInitRoomTask,
        _trailerCoachSetDepthShift,
        taskKill,
    },
};

/// Registers the night trailer coach receiver and starts its arrival scene.
///
/// State 0 borrows the room's message table, places and animates a present
/// companion, and starts a skippable script for arrival warp 2 or 3. Warp 3
/// also sets object 34's state to 1; objective 37 advances to 38. The overlay
/// and its scene resources must remain loaded while the task and scripts run.
static void _dryfieldNightTrailerCoachInitRoomTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_WARP_2         = 2,
        DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_WARP_3         = 3,
        DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_OBJECT         = 0x22,
        DRYFIELD_NIGHT_TRAILER_COACH_OBJECTIVE_BEFORE_ENTRY = 0x25,
        DRYFIELD_NIGHT_TRAILER_COACH_OBJECTIVE_AFTER_ENTRY  = 0x26
    };
    s32* taskState;

    task->msgTable = D_dryfield_night_trailer_coach_8018794C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_night_trailer_coach_801879B8, 0);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &D_dryfield_night_trailer_coach_80187CEC, 0);
    }
    // Arrival scripts own their normal and skipped restoration paths.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_WARP_2) {
        evsStartScriptWithSkip(D_dryfield_night_trailer_coach_80187D00, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_night_trailer_coach_80187F58);
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_WARP_3) {
        evsStartScriptWithSkip(D_dryfield_night_trailer_coach_80189080, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_night_trailer_coach_801892C0);
        areaSetCurrentObjectState(DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_OBJECT, 1);
    }
    if (gameFlagGetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE) == DRYFIELD_NIGHT_TRAILER_COACH_OBJECTIVE_BEFORE_ENTRY) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_TRAILER_COACH_OBJECTIVE_AFTER_ENTRY);
    }
    gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_8X;
    // The indirect state store keeps the initial depth-scale write ordered.
    taskState   = &task->state;
    *taskState += 1;
}

/// Selects the night trailer coach's conversation, shop and story sequences.
///
/// A fresh bodyless task starts in state 0. If the balcony scene has been seen,
/// it queues the chapter-dependent greeting and releases itself immediately.
/// Otherwise it waits for the initial conversation, routes CAP variants 11..13,
/// waits for that follow-up, and releases itself. Variant 13 commits the story
/// update once. Event scripts restore player control; the overlay and scripts
/// must remain loaded through their completion.
static void _dryfieldNightTrailerCoachTalkTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_SELECT         = 0,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_WAIT_CHOICE    = 1,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_HANDLE_CHOICE  = 2,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_WAIT_FOLLOWUP  = 3,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_FINISH         = 4,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_SHOP   = 0xB,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_CAP_11 = 0xC,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_STORY  = 0xD,
        DRYFIELD_NIGHT_TRAILER_COACH_STORY_OBJECTIVE     = 0x13,
        DRYFIELD_NIGHT_TRAILER_COACH_STORY_SOUND_EVENT   = 2
    };

    switch (task->state) {
        case DRYFIELD_NIGHT_TRAILER_COACH_TALK_SELECT:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) == 0) {
                evsStartScript(D_dryfield_night_trailer_coach_801880A8, EVENT_SCRIPT_HUD_KEEP);
                task->state++;
                break;
            }
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN) == 0 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= DRYFIELD_NIGHT_TRAILER_COACH_LATE_CHAPTER) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN, 1);
                evsStartScript(D_dryfield_night_trailer_coach_80188510, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(D_dryfield_night_trailer_coach_80188348, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            taskKill(task);
            break;
        case DRYFIELD_NIGHT_TRAILER_COACH_TALK_HANDLE_CHOICE:
            // Route the completed conversation's choice into its follow-up scene.
            if (capGetVariantKey() == DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_SHOP) {
                evsStartScript(D_dryfield_night_trailer_coach_801881F8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else if (capGetVariantKey() == DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_CAP_11) {
                evsStartScript(D_dryfield_night_trailer_coach_80188708, EVENT_SCRIPT_HUD_KEEP);
            } else if (capGetVariantKey() == DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_STORY) {
                if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN) == 0) {
                    gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN, 1);
                    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
                    areaApplySavedUpdates(D_dryfield_night_trailer_coach_8018C208);
                    evsStartScriptWithSkip(D_dryfield_night_trailer_coach_801889A8, EVENT_SCRIPT_HUD_KEEP,
                                           D_dryfield_night_trailer_coach_80188F00);
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_TRAILER_COACH_STORY_OBJECTIVE);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = DRYFIELD_NIGHT_TRAILER_COACH_STORY_SOUND_EVENT;
                } else {
                    evsStartScript(D_dryfield_night_trailer_coach_80188858, EVENT_SCRIPT_HUD_KEEP);
                }
            }
            task->state++;
            break;
        case DRYFIELD_NIGHT_TRAILER_COACH_TALK_WAIT_CHOICE:
        case DRYFIELD_NIGHT_TRAILER_COACH_TALK_WAIT_FOLLOWUP:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case DRYFIELD_NIGHT_TRAILER_COACH_TALK_FINISH:
            taskKill(task);
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses every key-item use request without changing inventory.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; `itemId` is the collected item's ID.
/// All arguments are ignored. The refused reply makes the menu show that the
/// item cannot be used here.
static s32 _dryfieldNightTrailerCoachRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a Dryfield destination from a complete copy of the room request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` synchronously. `request` borrows eight
/// readable bytes and `reply` eight writable bytes; they may be the same record.
/// The map overlay and progress flags must be available. Neither pointer is
/// retained; the map resolver's result is ignored and this handler returns 1.
static s32 _dryfieldNightTrailerCoachResolveRoomVariantMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_NIGHT_TRAILER_COACH_ROOM_RESOLVED = 1 };

    *reply = *request;
    mapDryfieldFullResolveRoomVariant(request, reply);
    return DRYFIELD_NIGHT_TRAILER_COACH_ROOM_RESOLVED;
}

/// Configures and queues the chapter-dependent trailer coach cutscene.
///
/// The room-owned record stays borrowed until the runner finishes, so another
/// cutscene request must not overwrite it while a runner is live. Selects CAP
/// data resource 1 at VRAM X=896 before chapter 4, otherwise resource 2 at X=960;
/// Y remains the record's existing value. Requires those resources, view 8 and
/// sound entries 3..6 loaded. Saved arrival becomes 1 even if spawning fails.
static inline void _dryfieldNightTrailerCoachQueueCutscene(void)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_WARP              = 1,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_VIEW              = 8,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_SLOT          = 1,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_FILE_EARLY    = 1,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_FILE_LATE     = 2,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TPAGE_X_EARLY     = 0x380,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TPAGE_X_LATE      = 0x3C0,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_FOLLOWUP_COMMAND  = 3,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TASK              = 0,
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_START_SOUND       = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 3),
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_END_SOUND         = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 5),
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_SCENE_SOUND       = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 4),
        DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_AFTER_SCENE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 6)
    };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_WARP;
    D_dryfield_night_trailer_coach_8018C21C.view               = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_VIEW;
    D_dryfield_night_trailer_coach_8018C21C.capSlot            = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_SLOT;
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < DRYFIELD_NIGHT_TRAILER_COACH_LATE_CHAPTER) {
        D_dryfield_night_trailer_coach_8018C21C.capTPageX = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TPAGE_X_EARLY;
        D_dryfield_night_trailer_coach_8018C21C.capFile   = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_FILE_EARLY;
    } else {
        D_dryfield_night_trailer_coach_8018C21C.capTPageX = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TPAGE_X_LATE;
        D_dryfield_night_trailer_coach_8018C21C.capFile   = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_FILE_LATE;
    }
    D_dryfield_night_trailer_coach_8018C21C.skipScene       = false;
    D_dryfield_night_trailer_coach_8018C21C.startSound      = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_START_SOUND;
    D_dryfield_night_trailer_coach_8018C21C.endSound        = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_END_SOUND;
    D_dryfield_night_trailer_coach_8018C21C.sceneSound      = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_SCENE_SOUND;
    D_dryfield_night_trailer_coach_8018C21C.afterSceneSound = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_AFTER_SCENE_SOUND;
    taskSpawnFromTable(gRoomCutsceneTaskDescs, DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TASK, DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_FOLLOWUP_COMMAND, &D_dryfield_night_trailer_coach_8018C21C);
}

/// Handles the night trailer coach's CAP room commands.
///
/// Command 14 queues a cutscene in view 8, CAP slot 1, file 1 before chapter 4
/// or file 2 thereafter, and changes the saved arrival warp to 1. Command 3
/// queues the talk task; command 23 runs CAP command 23 with a display transition.
/// Other commands do nothing. Returns zero; the other callback arguments are
/// ignored. The room and its resources must remain loaded through the spawned
/// tasks. The cutscene record must not be overwritten while its runner is live.
static s32 _dryfieldNightTrailerCoachCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_CUTSCENE = 0xE,
        DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_TALK     = 3,
        DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_CAP_23   = 0x17
    };

    if (commandId == DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_CUTSCENE) {
        _dryfieldNightTrailerCoachQueueCutscene();
    }
    if (commandId == DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_TALK) {
        taskSpawnFromTable(&D_dryfield_night_trailer_coach_8018797C, 0, 0, 0);
    }
    if (commandId == DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_CAP_23) {
        capRunCommandWithTransition(DRYFIELD_NIGHT_TRAILER_COACH_COMMAND_CAP_23);
    }
    return 0;
}

/// Ignores room-action trigger requests and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`. The four-byte request is borrowed
/// during dispatch, but this handler neither reads nor retains it.
static s32 _dryfieldNightTrailerCoachIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Queues night trailer coach sound script 13 for CAP sound cue 99.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue keys do nothing. Returns zero and
/// ignores the other callback arguments. Requires the room sound bank to
/// remain available for the queued script.
static s32 _dryfieldNightTrailerCoachSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_SOUND_CUE    = 0x63,
        DRYFIELD_NIGHT_TRAILER_COACH_SOUND_SCRIPT = 0x0D
    };

    if (cueKey == DRYFIELD_NIGHT_TRAILER_COACH_SOUND_CUE) {
        sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, DRYFIELD_NIGHT_TRAILER_COACH_SOUND_SCRIPT), 0, 0);
    }
    return 0;
}

/// Starts variant 1 of the trailer coach's CAP conversation slot 9 in place.
///
/// Called after the initial talk script's cue and delay. The command table
/// must be relocated and contain slot 9; its resources stay borrowed through
/// playback. Busy playback starts nothing, and the start result is discarded.
static void _dryfieldNightTrailerCoachStartTalkVariant1(void)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_CAP_SLOT    = 9,
        DRYFIELD_NIGHT_TRAILER_COACH_TALK_CAP_VARIANT = 1
    };

    capStartSequenceSlot(DRYFIELD_NIGHT_TRAILER_COACH_TALK_CAP_SLOT, CAP_PLAYBACK_IN_PLACE, DRYFIELD_NIGHT_TRAILER_COACH_TALK_CAP_VARIANT);
}

/// Queues the trailer coach shop with normal or chapter-4 stock.
///
/// The chapter-4 greeting flag selects the late stock, independently of the
/// chapter counter. Called by the greeting scripts once their CAP cue is reached.
/// The shop-open result is discarded; the current room must remain loaded.
static void _dryfieldNightTrailerCoachOpenShop(void)
{
    shopOpenSession((gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN) == 0) ? SHOP_STOCK_DRYFIELD : SHOP_STOCK_DRYFIELD_LATE);
}

#include "../../shared/trailer_coach_set_depth_shift.inc.c"

void dryfieldNightTrailerCoachRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_dryfield_night_trailer_coach_8017D7DC;
    handlers.funcs[task->state](task);
}
