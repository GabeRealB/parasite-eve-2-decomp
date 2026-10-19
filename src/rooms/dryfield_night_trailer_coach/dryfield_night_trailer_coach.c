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

#include "rooms/akropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
#include "../../shared/room/room_cutscene.h"
static void _roomCutsceneTask(Task* task);

/// Selects the nighttime trailer coach view that uses the finer depth shift.
///
/// Keep this binding through the depth-shift implementation fragment.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/trailer_coach.h"

static void _trailerCoachSetDepthShift(Task* unusedTask);

static void _roomCutsceneSoundTask(Task* task);

static s32 _gShopPreviewRowId;

#include "../../shared/room/telephone.h"

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

/// Binds the shop's animated refill quantity to this room's writable s32 storage.
///
/// Defined in dryfield_night_trailer_coach_2.c and declared in the private header.
/// Uses eight fractional bits; keep the binding through shop.inc.c and the
/// storage alive through panel closure. Only one refill panel may use it.
#define SHOP_REFILL_DISPLAY_QUANTITY gShopRefillDisplayQuantityFixed

/// Binds the refill panel's borrowed catalog selection to this room's pointer storage.
///
/// Expands to the writable `const EquipmentWeaponSupply*` defined in
/// `dryfield_night_trailer_coach_2.c` and declared in the private header. Keep this
/// binding through `shop.inc.c`; `shop.h` specifies selection, lifetime and use by
/// one panel at a time.
#define SHOP_REFILL_SUPPLY gShopRefillSupply
#include "../../shared/room/shop.h"

enum { DRYFIELD_NIGHT_TRAILER_COACH_LATE_CHAPTER = 4 };

static s32 _dryfieldNightTrailerCoachRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightTrailerCoachResolveRoomVariantMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightTrailerCoachCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightTrailerCoachIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _dryfieldNightTrailerCoachSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

static void _dryfieldNightTrailerCoachTalkTask(Task* task);

static AnimationSet _gDryfieldNightTrailerCoachAnimation07DD8;
static AnimationSet _gDryfieldNightTrailerCoachAnimation07FD0;
static AnimationSet _gDryfieldNightTrailerCoachAnimation08324;
static AnimationSet _gDryfieldNightTrailerCoachAnimation0851C;
static AnimationSet _gDryfieldNightTrailerCoachAnimation08820;
static AnimationSet _gDryfieldNightTrailerCoachAnimation08C48;
static AnimationSet _gDryfieldNightTrailerCoachAnimation09284;
static AnimationSet _gDryfieldNightTrailerCoachAnimation0959C;
static AnimationSet _gDryfieldNightTrailerCoachAnimation098C8;
static AnimationSet _gDryfieldNightTrailerCoachAnimation09E10;
static AnimationSet _gDryfieldNightTrailerCoachAnimation0A364;

static void _dryfieldNightTrailerCoachStartTalkVariant1(void);
static void _dryfieldNightTrailerCoachOpenShop(void);

#include "../../shared/room/shop_data.inc.c"

TaskDesc gDryfieldNightTrailerCoachShopSessionTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = 192 } },
    .callback = _shopSessionTask,
    .data     = { .value = 0 },
};

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

#include "../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneSoundTask, { .value = 0 } },
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

/// Sentinel-terminated receiver table for the night trailer coach room task.
///
/// Five handlers resolve room variants, reject key items, ignore room actions,
/// start room commands and route sound cues. The task borrows this table and
/// its callback code while the room image remains loaded.
static const TaskMessageEntry _gDryfieldNightTrailerCoachRoomMessages[] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightTrailerCoachResolveRoomVariantMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightTrailerCoachRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightTrailerCoachIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightTrailerCoachCommandMessage },
    { ROOM_MESSAGE_SOUND, _dryfieldNightTrailerCoachSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { DRYFIELD_NIGHT_TRAILER_COACH_TALK_TASK_PRIORITY = 32 };

/// Bodyless priority-32 recipe for the room-command conversation task.
///
/// Spawning copies the recipe; the room's callback code must remain loaded
/// until the task completes. Spawn payload words are unused.
static TaskDesc _gDryfieldNightTrailerCoachTalkTaskDescriptor = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = DRYFIELD_NIGHT_TRAILER_COACH_TALK_TASK_PRIORITY } },
    .callback = _dryfieldNightTrailerCoachTalkTask,
    .data     = { .value = 0 },
};

ActorTransform gDryfieldNightTrailerCoachPlayerDialogueTransform = {
    .pos = { 4870, 0, -900, 0 },
    .rot = { 0, -1536, 0, 0 },
};

/// Unreferenced transform-shaped record; its receiver and coordinate frame are unproven.
static ActorTransform D_dryfield_night_trailer_coach_801879A0 = { { 4600, 0, -1300, 0 }, { 0, -1479, 0, 0 } };

/// Arrival placement of a present companion in room-coordinate units.
///
/// Yaw 512 is one eighth-turn; pitch and roll are zero. Placement copies both
/// vectors synchronously and does not retain the transform.
static const ActorTransform _gDryfieldNightTrailerCoachCompanionArrivalTransform SECTION(".data") = {
    .pos = { 4400, 0, -2400, 0 },
    .rot = { 0, 512, 0, 0 },
};

enum { DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_IDLE_CLIP = 1 };

const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerIdleAnimation SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_IDLE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unused bank-1 clip-47 request, separate from the live story-closing lead-in.
///
/// Requests an eight-frame blend and disabled world collision. Its bytes equal
/// the live request, but no script or message selects this retained copy.
static AnimationPlayRequest _gDryfieldNightTrailerCoachUnusedBank1Animation47Request = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED         = 1,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_DIALOGUE_BLEND_FRAMES      = 8,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_CLOSING_LEAD_IN_CLIP = 47,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_CLOSING_CLIP         = 52,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_ARRIVAL_DIALOGUE_CLIP      = 48,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_DIALOGUE_CLIP        = 53,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_CHAPTER4_DIALOGUE_CLIP     = 56,
    DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_REPLY_CLIP           = 57,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_STORY_LEAD_IN_CLIP           = 1,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_OPENING_HOLD_CLIP       = 7,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_REPLY_CLIP              = 10,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_STORY_REPLY_CLIP             = 3,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_OPENING_CLIP         = 1,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_FOLLOWUP_CLIP        = 2,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_STORY_CLOSING_CLIP           = 3,
    DRYFIELD_NIGHT_TRAILER_COACH_FLINT_ARRIVAL_CLIP                = 34,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_BANK                 = 2,
    DRYFIELD_NIGHT_TRAILER_COACH_FLINT_BANK                        = 6
};

const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryClosingLeadInAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_CLOSING_LEAD_IN_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryClosingAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_CLOSING_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Equipped-player clip 48 for the warp-2 arrival dialogue.
///
/// Playback copies the request, replaces the bank seed from current equipment,
/// blends over eight frames and disables world collision. The request is
/// borrowed until dispatch; room-owned clip data stays live during playback.
static const AnimationPlayRequest _gDryfieldNightTrailerCoachPlayerArrivalDialogueAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_ARRIVAL_DIALOGUE_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryDialogueAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_DIALOGUE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Four unused bank-1 reset requests for clips 49, 50, 51 and 55.
///
/// Every record requests disabled world collision. No dialogue script selects
/// these retained records; the array has four entries and no terminator.
static AnimationPlayRequest _gDryfieldNightTrailerCoachUnusedBank1AnimationRequests[4] = {
    { .source = { .index = 1 }, .animationId = 49, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 50, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 51, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 55, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerChapter4DialogueAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_CHAPTER4_DIALOGUE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryReplyAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_EQUIPPED_BANK_SEED },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_PLAYER_STORY_REPLY_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unused bank-0 clip-0 reset request retained before the Gary animation requests.
///
/// Requests disabled world collision; no script or message selects this record.
static AnimationPlayRequest _gDryfieldNightTrailerCoachUnusedBank0Animation0Request = {
    .source               = { .index = 0 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK                 = 0,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK             = 1,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_CLOSING_CLIP         = 4,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_REST_CLIP            = 5,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_HOLD_CLIP            = 8,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_CLOSING_LEAD_IN_CLIP = 9,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_LEAD_IN_CLIP     = 1,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_HOLD_CLIP        = 2,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_FOLLOWUP_CLIP    = 7,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryStoryLeadInAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_STORY_LEAD_IN_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Two unused bank-0 reset requests for clips 2 and 3.
///
/// Both request disabled world collision. Neither retained record is selected
/// by the dialogue scripts; the array has two entries and no terminator.
static AnimationPlayRequest _gDryfieldNightTrailerCoachUnusedBank0AnimationRequests[2] = {
    { .source = { .index = 0 }, .animationId = 2, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 3, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkClosingAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_CLOSING_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkRestAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_REST_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BANK         = 0,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_OPENING_CLIP = 6,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BLEND_FRAMES = 8
};

/// Gary's opening body animation for the nighttime trailer-coach conversations.
///
/// Requests an eight-frame blend into his base-bank opening clip. The receiver
/// uses its own eight-frame blend and ignores the requested collision setting.
/// Event scripts borrow this request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
static AnimationPlayRequest _gDryfieldNightTrailerCoachGaryTalkOpeningAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_OPENING_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Gary's reset base-bank clip 7 between talk opening and the held talk pose.
///
/// Follows the opening after 21 frames; the script waits another nine frames
/// before talk hold. Placement-0 dispatch consumes the request synchronously;
/// blend duration and world collision are ignored. The room stays loaded until dispatch.
static AnimationPlayRequest _gDryfieldNightTrailerCoachGaryTalkOpeningHoldAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_OPENING_HOLD_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkHoldAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_HOLD_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_CLOSING_LEAD_IN_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkReplyAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_REPLY_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_0 = 0,
};

/// Retained, unused Gary requests for base-bank clip 8 and dialogue-bank clip 0.
///
/// The first stores interpolation with zero requested frames; the second resets.
/// Both disable world collision. Gary maps their selectors to local sets 8 and 10
/// and uses eight frames for interpolation; no script selects these records.
static const AnimationPlayRequest _gDryfieldNightTrailerCoachGaryUnusedBase8AndDialogue0Animations[2] SECTION(".data") = {
    {
        .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BASE_BANK },
        .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_TALK_HOLD_CLIP,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
        .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_0,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_LEAD_IN_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryDialogueHoldAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_HOLD_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryStoryReplyAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_STORY_REPLY_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_4 = 4,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_5 = 5,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_6 = 6,
};

/// Retained, unused Gary dialogue-bank requests for clips 4, 5 and 6.
///
/// Bank 1 maps to local sets 14..16. Clip 4 stores interpolation with zero
/// requested frames; clips 5 and 6 reset. Collision is disabled in all three
/// records; no script selects them and their intended scene use is unproven.
static const AnimationPlayRequest _gDryfieldNightTrailerCoachGaryUnusedDialogueAnimations4To6[3] SECTION(".data") = {
    {
        .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
        .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_4,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
        .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_5,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
        .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_DIALOGUE_CLIP_6,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryDialogueFollowupAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_FOLLOWUP_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gary's blended bank-1 clip 2 held at the warp-2 arrival dialogue cue.
///
/// Follows the dialogue lead-in after sixteen frames. Maps to local set 12
/// and blends over eight frames despite the zero requested duration. World
/// collision is ignored; the script borrows this record until placement-0 dispatch.
static AnimationPlayRequest _gDryfieldNightTrailerCoachGaryArrivalDialogueHoldAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_DIALOGUE_HOLD_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_ARRIVAL_CLIP_0 = 0,
};

/// Retained, unused Gary arrival-bank reset request for clip 0.
///
/// Bank 2 maps this ID to loaded local set 17. The stored blend duration is
/// zero and collision is disabled; no script selects this request.
static const AnimationPlayRequest _gDryfieldNightTrailerCoachGaryUnusedArrivalAnimation0 SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_UNUSED_ARRIVAL_CLIP_0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gary's reset bank-2 clip 1 at the opening of warp-2 arrival dialogue.
///
/// Maps to local set 18. Placement-0 dispatch resets playback immediately;
/// blend duration and collision are ignored. The room stays loaded until dispatch.
static AnimationPlayRequest _gDryfieldNightTrailerCoachGaryArrivalOpeningAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_OPENING_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gary's reset bank-2 clip 2 after the first warp-2 arrival CAP cue.
///
/// Maps to local set 19. Placement-0 dispatch resets playback immediately;
/// blend duration and collision are ignored. The room stays loaded until dispatch.
static AnimationPlayRequest _gDryfieldNightTrailerCoachGaryArrivalFollowupAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_FOLLOWUP_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest gDryfieldNightTrailerCoachGaryStoryClosingAnimation = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_GARY_ARRIVAL_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_GARY_STORY_CLOSING_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Room-owned player clips installed at equipped-bank indices 47..57 for dialogue.
///
/// The copy request transfers eleven set pointers in table order. The trailing
/// NULL remains stored here and is excluded from the transfer. Playback borrows
/// these descriptors and their tracks, so the room image must remain loaded.
static AnimationSet* const _gDryfieldNightTrailerCoachPlayerDialogueAnimationSets[12] SECTION(".data") = {
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

const AnimationBankCopyRequest gDryfieldNightTrailerCoachPlayerDialogueBankCopyRequest SECTION(".data") = {
    .sets  = _gDryfieldNightTrailerCoachPlayerDialogueAnimationSets,
    .count = ARRAY_SIZE(_gDryfieldNightTrailerCoachPlayerDialogueAnimationSets) - 1,
};

/// Flint-bank clip 34 requested of a present companion on room entry.
///
/// Selects bank 6 directly, resets playback and disables world collision.
/// Dispatch reads the request synchronously; Flint's loaded animation bank
/// and tracks must remain live throughout the companion's playback.
static const AnimationPlayRequest _gDryfieldNightTrailerCoachFlintArrivalAnimation SECTION(".data") = {
    .source               = { .index = DRYFIELD_NIGHT_TRAILER_COACH_FLINT_BANK },
    .animationId          = DRYFIELD_NIGHT_TRAILER_COACH_FLINT_ARRIVAL_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_WARP2_ARRIVAL      = 8,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_TALK_CHOICE        = 9,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_SHOP_GREETING      = 10,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_CHAPTER4_GREETING  = 24,
    DRYFIELD_NIGHT_TRAILER_COACH_SHOP_CHOICE_STOCK      = 0x20,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX   = 0,
    DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_OPENING_SOUND  = 8,
    DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_FOLLOWUP_SOUND = 9,
    DRYFIELD_NIGHT_TRAILER_COACH_RESTORED_VIEW          = 3
};

/// Skippable warp-2 arrival dialogue with Gary.
///
/// Starts CAP command 8, installs the player's room clips and coordinates
/// player/Gary animation with CAP cues and sound requests. Restores weapons,
/// lighting, view 3 and normal player control before END. The interpreter
/// borrows this complete command stream and its room resources until it ends.
static EvsCommand _gDryfieldNightTrailerCoachWarp2ArrivalScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_WARP2_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gDryfieldNightTrailerCoachPlayerDialogueBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &gDryfieldNightTrailerCoachPlayerDialogueTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryArrivalOpeningAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_OPENING_SOUND) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryArrivalFollowupAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_FOLLOWUP_SOUND) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightTrailerCoachPlayerArrivalDialogueAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryArrivalDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_TRAILER_COACH_RESTORED_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restores the warp-2 arrival scene when the player skips dialogue.
///
/// Covers restoration with a fade, places the player, resets player/Gary poses,
/// restores weapons and lighting, selects view 3, and ends scripted control.
/// This END-terminated stream and its resources remain borrowed until completion.
static EvsCommand _gDryfieldNightTrailerCoachWarp2ArrivalSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &gDryfieldNightTrailerCoachPlayerDialogueTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_TRAILER_COACH_RESTORED_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Opens the initial Gary conversation and leaves its CAP choice for the talk task.
///
/// CAP command 9 hides the HUD and enters Gary's talk pose, then the callback
/// starts variant 1. CAP cues bracket the choice. Player control and the HUD
/// remain held for the selected follow-up. The room stays loaded through END.
static EvsCommand _gDryfieldNightTrailerCoachTalkChoiceScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_TALK_CHOICE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryTalkOpeningAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryTalkOpeningHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightTrailerCoachStartTalkVariant1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the shop follow-up selected by conversation variant 11.
///
/// CAP command 10 opens stock profile 32 at its cue, then closes Gary's talk
/// pose and restores player control and the HUD. The interpreter borrows
/// this END-terminated stream and its callbacks through completion.
static EvsCommand _gDryfieldNightTrailerCoachShopChoiceScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_SHOP_GREETING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_SHOP_CHOICE_STOCK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs Gary's repeat greeting and opens the night trailer coach shop.
///
/// CAP command 10 drives the opening and closing talk poses around the shop
/// callback. Ends by restoring player control and the HUD. The complete
/// END-terminated stream and callback code must remain loaded until completion.
static EvsCommand _gDryfieldNightTrailerCoachShopGreetingScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_SHOP_GREETING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryTalkOpeningAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryTalkOpeningHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightTrailerCoachOpenShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs Gary's first chapter-4 greeting and opens the night trailer coach shop.
///
/// CAP command 24 uses the story-dialogue poses before opening the shop,
/// then closes the conversation and restores player control and the HUD.
/// The interpreter borrows this END-terminated stream until completion.
static EvsCommand _gDryfieldNightTrailerCoachChapter4ShopGreetingScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_CHAPTER4_GREETING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldNightTrailerCoachGaryTalkOpeningAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueFollowupAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightTrailerCoachOpenShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_GARY_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static void _dryfieldNightTrailerCoachInitRoomTask(Task* task);

/// Retained byte following this room's weapon-supply refill title terminator.
///
/// One-byte string-literal binding for `shop.inc.c`; label drawing does not read it.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\xFD"
#include "../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x4E\xF2"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void dryfieldNightTrailerCoachTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

#include "../../shared/room/room_cutscene_task.inc.c"

/// Room-task dispatch for states 0 initialize, 1 update depth scale and 2 kill.
///
/// State indices must stay in 0..2. This table stays at its original rodata
/// position among the callback definitions.
static const TaskFuncTable3 _gDryfieldNightTrailerCoachRoomTaskStates = {
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

    task->msgTable = _gDryfieldNightTrailerCoachRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldNightTrailerCoachCompanionArrivalTransform, 0);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &_gDryfieldNightTrailerCoachFlintArrivalAnimation, 0);
    }
    // Arrival scripts own their normal and skipped restoration paths.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_WARP_2) {
        evsStartScriptWithSkip(_gDryfieldNightTrailerCoachWarp2ArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldNightTrailerCoachWarp2ArrivalSkipScript);
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_WARP_3) {
        evsStartScriptWithSkip(gDryfieldNightTrailerCoachWarp3ArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gDryfieldNightTrailerCoachWarp3ArrivalSkipScript);
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
                evsStartScript(_gDryfieldNightTrailerCoachTalkChoiceScript, EVENT_SCRIPT_HUD_KEEP);
                task->state++;
                break;
            }
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN) == 0 && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= DRYFIELD_NIGHT_TRAILER_COACH_LATE_CHAPTER) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN, 1);
                evsStartScript(_gDryfieldNightTrailerCoachChapter4ShopGreetingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(_gDryfieldNightTrailerCoachShopGreetingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            taskKill(task);
            break;
        case DRYFIELD_NIGHT_TRAILER_COACH_TALK_HANDLE_CHOICE:
            // Route the completed conversation's choice into its follow-up scene.
            if (capGetVariantKey() == DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_SHOP) {
                evsStartScript(_gDryfieldNightTrailerCoachShopChoiceScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else if (capGetVariantKey() == DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_CAP_11) {
                evsStartScript(gDryfieldNightTrailerCoachCap11ReplyScript, EVENT_SCRIPT_HUD_KEEP);
            } else if (capGetVariantKey() == DRYFIELD_NIGHT_TRAILER_COACH_TALK_VARIANT_STORY) {
                if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN) == 0) {
                    gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN, 1);
                    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
                    areaApplySavedUpdates(gDryfieldNightTrailerCoachStoryAreaUpdates);
                    evsStartScriptWithSkip(gDryfieldNightTrailerCoachStoryScript, EVENT_SCRIPT_HUD_KEEP,
                                           gDryfieldNightTrailerCoachStorySkipScript);
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_TRAILER_COACH_STORY_OBJECTIVE);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = DRYFIELD_NIGHT_TRAILER_COACH_STORY_SOUND_EVENT;
                } else {
                    evsStartScript(gDryfieldNightTrailerCoachStoryRepeatScript, EVENT_SCRIPT_HUD_KEEP);
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

#include "../../shared/room/room_cutscene_sound_task.inc.c"

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

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_WARP;
    gDryfieldNightTrailerCoachCutscene.view                = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_VIEW;
    gDryfieldNightTrailerCoachCutscene.capSlot             = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_SLOT;
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < DRYFIELD_NIGHT_TRAILER_COACH_LATE_CHAPTER) {
        gDryfieldNightTrailerCoachCutscene.capTPageX = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TPAGE_X_EARLY;
        gDryfieldNightTrailerCoachCutscene.capFile   = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_FILE_EARLY;
    } else {
        gDryfieldNightTrailerCoachCutscene.capTPageX = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TPAGE_X_LATE;
        gDryfieldNightTrailerCoachCutscene.capFile   = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_CAP_FILE_LATE;
    }
    gDryfieldNightTrailerCoachCutscene.skipScene       = false;
    gDryfieldNightTrailerCoachCutscene.startSound      = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_START_SOUND;
    gDryfieldNightTrailerCoachCutscene.endSound        = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_END_SOUND;
    gDryfieldNightTrailerCoachCutscene.sceneSound      = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_SCENE_SOUND;
    gDryfieldNightTrailerCoachCutscene.afterSceneSound = DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_AFTER_SCENE_SOUND;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_TASK, DRYFIELD_NIGHT_TRAILER_COACH_CUTSCENE_FOLLOWUP_COMMAND, &gDryfieldNightTrailerCoachCutscene);
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
        taskSpawnFromTable(&_gDryfieldNightTrailerCoachTalkTaskDescriptor, 0, 0, 0);
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

    handlers = _gDryfieldNightTrailerCoachRoomTaskStates;
    handlers.funcs[task->state](task);
}
