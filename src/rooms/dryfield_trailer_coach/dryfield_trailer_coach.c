#include "rooms/dryfield_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_420700.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
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

#include "mapui/map_dryfield.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
// Symbol is `RoomCutsceneSoundTaskStorage`; the runner reads `task`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask.task
#include "../../shared/room_cutscene.h"
#include "../../shared/glow_draw.h"
static void _roomCutsceneTask(Task* task);

/// Selects the daytime trailer coach view that uses the finer depth shift.
///
/// Keep this binding through the depth-shift implementation fragment.
#define DRYFIELD_TIME DRYFIELD_DAY
#include "../../shared/trailer_coach.h"

static void _trailerCoachSetDepthShift(Task* unusedTask);

static void _roomCutsceneSoundTask(Task* task);

static s32 _gShopPreviewRowId;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `_roomCutsceneTask` spawns entry 1 for the scene.
extern TaskDesc gRoomCutsceneTaskDescs[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\xD0\xFF"
#include "../../shared/telephone.h"

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];
static u16       Shop_Data_80181620[];
static u16       Shop_Data_80181630[];
static u16       Shop_Data_80181640[];
static u16       Shop_Data_80181648[];
static u16       Shop_Data_80181658[];
static u16       Shop_Data_80181668[];
static u16       Shop_Data_80181678[];
static u16       Shop_Data_80181680[];
static u16       Shop_Data_80181694[];
static u16       Shop_Data_801816AC[];
static u16       Shop_Data_80181888[];
static u16       Shop_Data_80181890[];
static u16       Shop_Data_80181898[];
static u16       Shop_Data_801818A4[];
static u16       Shop_Data_801818B0[];
static u16       Shop_Data_801818B8[];
static u16       Shop_Data_801818C4[];
static u16       Shop_Data_801818D0[];
static u16       Shop_Data_801818DC[];
static u16       Shop_Data_801818E0[];
static u16       Shop_Data_801818EC[];
static u16       Shop_Data_801818F8[];
static u16       Shop_Data_80181904[];
static u16       Shop_Data_8018190C[];
static u16       Shop_Data_80181918[];
static u16       Shop_Data_80181924[];
static u16       Shop_Data_80181930[];
static u16       Shop_Data_80181938[];
static u16       Shop_Data_80181944[];

/// The list returned when no case matches.
static u16 Shop_Data_80181AD4[];

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xEF"
#include "../../shared/shop.h"

static s32  _dryfieldTrailerCoachRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldTrailerCoachResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _dryfieldTrailerCoachWaitForTopicChoice(Task* task);
static s32  _dryfieldTrailerCoachHandleCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedSecondArg);
static void _dryfieldTrailerCoachConversationTask(Task* task);
static void _dryfieldTrailerCoachTopicChoiceTask(Task* task);

/// Story milestones selecting this room's conversation and caption variants.
enum { DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE  = 2,
       DRYFIELD_TRAILER_COACH_SECOND_CONVERSATION_COMPLETE = 3 };

/// Inventory request to use a key item in this room.
enum { DRYFIELD_TRAILER_COACH_MESSAGE_USE_KEY_ITEM = 0x13F1 };

extern AnimationPlayRequest             D_dryfield_trailer_coach_8018504C;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185060;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185088;
extern AnimationPlayRequest             D_dryfield_trailer_coach_8018509C;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801850B0;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801850C4;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801850EC;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185100;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185114;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185128;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185150;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185178;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801851B0;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185214;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185228;
extern AnimationPlayRequest             D_dryfield_trailer_coach_8018523C;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185264;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801852B4;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801852C8;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801852DC;
extern AnimationPlayRequest             D_dryfield_trailer_coach_801852F0;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185304;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185318;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185340;
extern AnimationPlayRequest             D_dryfield_trailer_coach_80185354;
static AnimationSet                     _gDryfieldTrailerCoachAnimation07668;
static AnimationSet                     _gDryfieldTrailerCoachAnimation07994;
extern ActorCommand                     D_dryfield_trailer_coach_8018518C;
extern ActorCommand                     D_dryfield_trailer_coach_80185190;
extern ActorCommand                     D_dryfield_trailer_coach_80185194;
extern ActorCommand                     D_dryfield_trailer_coach_80185198;
static WorldCollisionGrid               _gDryfieldTrailerCoachRoomCollisionGrid;
static WorldCollisionTrigger            _gDryfieldTrailerCoachViewBoundaryTriggers[4];
static WorldCollisionTrigger            _gDryfieldTrailerCoachActionTriggers[12];
static const WorldCoordRoomAmbientEntry _gDryfieldTrailerCoachAmbientByView[DRYFIELD_TRAILER_COACH_VIEW_COUNT + 1];
static WorldCoordRoomLights             _gDryfieldTrailerCoachLights;
extern ActorTransform                   D_dryfield_trailer_coach_80184FF0;
static void                             _dryfieldTrailerCoachStartConversationCaption(void);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc gDryfieldTrailerCoachShopSessionTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = 192 } },
    .callback = _shopSessionTask,
    .data     = { .value = 0 },
};

static TmdBone _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gDryfieldTrailerCoachAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0PartVerts,
    _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Verts,
    _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Normals,
    _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Skeleton,
    _gDryfieldTrailerCoachAcropolisSanctuaryModel090F0Stream,
};

#include "../../shared/telephone_data.inc.c"

static AnimationPackedPose _gDryfieldTrailerCoachAnimation07668Bank1[3] = {
#include "assets/dryfield_trailer_coach_animation_07668_bank1.inc"
};

static AnimationPackedRotation _gDryfieldTrailerCoachAnimation07668Bank4[81] = {
#include "assets/dryfield_trailer_coach_animation_07668_bank4.inc"
};

static AnimationRecord _gDryfieldTrailerCoachAnimation07668Records[128] = {
#include "assets/dryfield_trailer_coach_animation_07668_records.inc"
};

static u16 _gDryfieldTrailerCoachAnimation07668Indices[20] = {
#include "assets/dryfield_trailer_coach_animation_07668_indices.inc"
};

static AnimationSet _gDryfieldTrailerCoachAnimation07668 = {
    _gDryfieldTrailerCoachAnimation07668Records,
    _gDryfieldTrailerCoachAnimation07668Indices,
    { NULL, _gDryfieldTrailerCoachAnimation07668Bank1, NULL, NULL, _gDryfieldTrailerCoachAnimation07668Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldTrailerCoachAnimation07994Bank1[7] = {
#include "assets/dryfield_trailer_coach_animation_07994_bank1.inc"
};

static AnimationPackedRotation _gDryfieldTrailerCoachAnimation07994Bank4[56] = {
#include "assets/dryfield_trailer_coach_animation_07994_bank4.inc"
};

static AnimationRecord _gDryfieldTrailerCoachAnimation07994Records[106] = {
#include "assets/dryfield_trailer_coach_animation_07994_records.inc"
};

static u16 _gDryfieldTrailerCoachAnimation07994Indices[20] = {
#include "assets/dryfield_trailer_coach_animation_07994_indices.inc"
};

static AnimationSet _gDryfieldTrailerCoachAnimation07994 = {
    _gDryfieldTrailerCoachAnimation07994Records,
    _gDryfieldTrailerCoachAnimation07994Indices,
    { NULL, _gDryfieldTrailerCoachAnimation07994Bank1, NULL, NULL, _gDryfieldTrailerCoachAnimation07994Bank4, NULL, NULL, NULL },
};

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_trailer_coach_80184FA0[4] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldTrailerCoachResolveRoomEvent },
    { DRYFIELD_TRAILER_COACH_MESSAGE_USE_KEY_ITEM, _dryfieldTrailerCoachRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _dryfieldTrailerCoachHandleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_trailer_coach_80184FC0[2] = {
    { { { TASK_BODY_NONE, 32 } }, _dryfieldTrailerCoachTopicChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _dryfieldTrailerCoachConversationTask, { .value = 0 } },
};

/// Places the player for trailer-coach conversations in room coordinates.
///
/// XYZ are game-coordinate units; yaw is -2560 with 4096 units per turn.
/// Scripts borrow this unchanged transform through synchronous placement.
static const ActorTransform _gDryfieldTrailerCoachPlayerConversationTransform SECTION(".data") = { { 4870, 0, -900, 0 }, { 0, -2560, 0, 0 } };

ActorTransform D_dryfield_trailer_coach_80184FF0 = { { 5270, 0, -500, 0 }, { 0, -2560, 0, 0 } };

/// Restores the player placement after a conversation or skipped scene.
///
/// Shares the conversation position and uses yaw +2560, in 4096 units per turn.
/// The transform is borrowed unchanged through synchronous placement.
static const ActorTransform _gDryfieldTrailerCoachPlayerAfterConversationTransform SECTION(".data") = { { 4870, 0, -900, 0 }, { 0, 2560, 0, 0 } };

// Retained parameter record; layout follows the adjacent script arguments.
ActorTransform D_dryfield_trailer_coach_80185020 = { { 4400, 128, -2400, 0 }, { 0, 512, 0, 0 } };

enum { DRYFIELD_TRAILER_COACH_PLAYER_RESET_CLIP = 1 };

/// Resets player clip 1 in the equipped weapon bank and restores grid participation.
///
/// Event scripts borrow this template and resolve the bank in a local copy;
/// the stored source index is overwritten, and reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip1ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_RESET_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

AnimationPlayRequest D_dryfield_trailer_coach_8018504C = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185060 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_80185074 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185088 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_8018509C = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_801850B0 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_801850C4 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_801850D8 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_801850EC = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185100 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185114 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185128 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Bank selectors and clip keys used by the room's playback templates.
enum {
    DRYFIELD_TRAILER_COACH_PLAYER_CONVERSATION_CLIP = 51,
    DRYFIELD_TRAILER_COACH_GARY_BASE_BANK           = 0,
    DRYFIELD_TRAILER_COACH_GARY_BANK_1              = 1,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_4         = 4,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_5         = 5,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_9         = 9,
    DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_1       = 1,
    DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_2       = 2,
};

/// Resets player extension clip 51 and restores world-grid participation.
///
/// The event interpreter resolves the equipped weapon bank in a local copy.
/// Reset ignores blendFrames; the template remains borrowed and unchanged.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip51ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CONVERSATION_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

AnimationPlayRequest D_dryfield_trailer_coach_80185150 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_80185164 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185178 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_dryfield_trailer_coach_8018518C = { { .loc = { 2, 27 } }, 0 };

ActorCommand D_dryfield_trailer_coach_80185190 = { { .loc = { 2, 27 } }, 1 };

ActorCommand D_dryfield_trailer_coach_80185194 = { { .loc = { 2, 27 } }, 2 };

ActorCommand D_dryfield_trailer_coach_80185198 = { { .loc = { 2, 27 } }, 3 };

AnimationPlayRequest D_dryfield_trailer_coach_8018519C = { 0 };

AnimationPlayRequest D_dryfield_trailer_coach_801851B0 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_801851C4 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_801851D8 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets Gary Douglas base-bank clip 4 during room conversations.
///
/// The actor borrows this request synchronously; reset ignores blendFrames.
/// Its receiver does not consume the world-collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip4ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Gary Douglas base-bank clip 5 during conversations and scene cleanup.
///
/// The actor borrows this request synchronously; reset ignores blendFrames.
/// Its receiver does not consume the world-collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip5ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_dryfield_trailer_coach_80185214 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185228 = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_8018523C = { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends Gary Douglas to base-bank clip 9 during room conversations.
///
/// The actor uses its fixed blend duration rather than blendFrames.
/// Its receiver does not consume the world-collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip9BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_dryfield_trailer_coach_80185264 = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_80185278 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_trailer_coach_8018528C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets Gary Douglas bank-1 clip 1, selecting actor set-table entry 11.
///
/// The actor borrows this request synchronously; reset ignores blendFrames.
/// Its receiver does not consume the world-collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip1ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_dryfield_trailer_coach_801852B4 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_801852C8 = { { .index = 1 }, 3, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_801852DC = { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_801852F0 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185304 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185318 = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends Gary Douglas to bank-1 clip 2, selecting actor set-table entry 12.
///
/// The actor uses its fixed blend duration rather than blendFrames.
/// Its receiver does not consume the world-collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip2BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_2,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_dryfield_trailer_coach_80185340 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_trailer_coach_80185354 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Seventeen player extension slots for clip ids 47..63, borrowed from this room and Gary's actor package.
///
/// Slots for ids 55..57 are NULL. The copy request transfers 32 words: it reads
/// 15 words past this table through the following copy request, three labels,
/// their alignment bytes and the first topic-choice pointer. This is retained
/// original behavior; those extra words are not playable animation sets.
/// Both overlays must remain loaded while the installed clip pointers are used.
static AnimationSet* const _gDryfieldTrailerCoachPlayerAnimationExtensionSets[17] SECTION(".data") = { &gActor420700Animation01C5C, &gActor420700Animation01EBC, &gActor420700Animation02328, &gActor420700Animation02F18, &gActor420700Animation0316C, &gActor420700Animation03404, &gActor420700Animation035D4, &gActor420700Animation03DD8, NULL, NULL, NULL, &gActor420700Animation00DE0, &gActor420700Animation013A8, &gActor420700Animation0164C, &gActor420700Animation01878, &_gDryfieldTrailerCoachAnimation07994, &_gDryfieldTrailerCoachAnimation07668 };

/// Copies 32 source words into the equipped player's extension slots starting at 47.
///
/// The count is the destination capacity, not the seventeen playable source slots.
/// The request is borrowed synchronously; the copied animation pointers remain
/// borrowed for playback. See `_gDryfieldTrailerCoachPlayerAnimationExtensionSets`
/// for the retained read across adjacent objects.
static const AnimationBankCopyRequest _gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest SECTION(".data") = {
    .source    = { .sets = _gDryfieldTrailerCoachPlayerAnimationExtensionSets },
    .wordCount = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Null-terminated Shift-JIS topic label meaning "About firearms".
static u8 _gDryfieldTrailerCoachFirearmsTopicLabel[] = "\x8F\x65\x8A\xED\x82\xC9\x82\xC2\x82\xA2\x82\xC4";

/// Null-terminated Shift-JIS topic label meaning "About the shelter".
static u8 _gDryfieldTrailerCoachShelterTopicLabel[] = "\x83\x56\x83\x46\x83\x8B\x83\x5E\x81\x5B\x82\xC9\x82\xC2\x82\xA2\x82\xC4";

/// Null-terminated Shift-JIS topic label meaning "Anything else?".
static u8 _gDryfieldTrailerCoachAnythingElseTopicLabel[] = "\x91\xBC\x82\xC9\x89\xBD\x82\xA9\x81\x48";

/// Borrowed labels for the firearms/shelter and firearms/anything-else choices.
///
/// The two contiguous pairs occupy entries 0..1 and 2..3. The option dialog
/// borrows the selected labels until answered; keep this overlay loaded.
static u8* const _gDryfieldTrailerCoachTopicChoiceLabels[4] SECTION(".data") = { _gDryfieldTrailerCoachFirearmsTopicLabel, _gDryfieldTrailerCoachShelterTopicLabel, _gDryfieldTrailerCoachFirearmsTopicLabel, _gDryfieldTrailerCoachAnythingElseTopicLabel };

EvsCommand D_dryfield_trailer_coach_801853F4[58] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018504C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018504C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185088 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018509C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185214 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_8018523C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185264 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_801850B0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_trailer_coach_80185190 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_801850C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_trailer_coach_80185194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_trailer_coach_8018518C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80185964[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80185AFC[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185214 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_8018523C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldTrailerCoachStartConversationCaption }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80185C4C[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80185D54[98] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185060 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 75 }, { .value = 75 }, { .value = 75 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185114 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_801850EC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x521B0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185150 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801852C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185114 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185100 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801852DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185114 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801852B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x521B000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801852F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185128 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_trailer_coach_80184FF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185178 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 89 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80186684[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_8018681C[25] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018504C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801851B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_trailer_coach_80185198 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x521B000E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185304 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_trailer_coach_8018518C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80186A74[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80186BDC[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80186D2C[35] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185354 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801851B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_80185318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185340 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_trailer_coach_801851B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_trailer_coach_80187074[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Local-space centre shared by the trailer coach's projected glows.
///
/// XYZ use signed game-coordinate units in the effect body's frame. Both glow
/// drawers borrow this point without changing it; the fourth halfword is zero.
static const SVECTOR _gDryfieldTrailerCoachGlowLocalPoint SECTION(".data") = { 163, -1532, -766, 0 };

WorldCollisionRoomResources gDryfieldTrailerCoachRoomCollisionResources[1] = {
    { &_gDryfieldTrailerCoachRoomCollisionGrid, _gDryfieldTrailerCoachViewBoundaryTriggers, _gDryfieldTrailerCoachActionTriggers, NULL },
};

WorldCoordRoomLighting gDryfieldTrailerCoachRoomLighting[1] = {
    { &_gDryfieldTrailerCoachLights, _gDryfieldTrailerCoachAmbientByView },
};

u8* gDryfieldTrailerCoachViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldTrailerCoachViewCounts[1] = { DRYFIELD_TRAILER_COACH_VIEW_COUNT };

/// Map marker written on later arrivals through either trailer-coach endpoint.
enum { DRYFIELD_TRAILER_COACH_MAP_VISIT_FLAG = 471 };

DirectionWarpEntry gDryfieldTrailerCoachWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1579, 0, -800 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1579, 0, -800 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_TRAILER_COACH_MAP_VISIT_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1579, 0, -800 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1579, 0, -800 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 1), DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DRYFIELD_TRAILER_COACH_MAP_VISIT_FLAG },
};

static SVECTOR _gDryfieldTrailerCoachRoomCollisionGridNormals[10] = {
#include "assets/dryfield_trailer_coach_collision_0A0F4_normals.inc"
};

static SVECTOR _gDryfieldTrailerCoachRoomCollisionGridVerts[65] = {
#include "assets/dryfield_trailer_coach_collision_0A0F4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldTrailerCoachRoomCollisionGridFaces[32] = {
#include "assets/dryfield_trailer_coach_collision_0A0F4_faces.inc"
};

static s16 _gDryfieldTrailerCoachRoomCollisionGridCells[58] = {
#include "assets/dryfield_trailer_coach_collision_0A0F4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldTrailerCoachRoomCollisionGridCells[i])
static s16* _gDryfieldTrailerCoachRoomCollisionGridTable[3] = {
#include "assets/dryfield_trailer_coach_collision_0A0F4_table.inc"
};
#undef GRID_CELL

/// Room-space collision mesh with a three-by-one XZ cell directory.
///
/// The 32 faces index 65 vertices and ten normals; cell lists contain 58 signed
/// face ids including terminators. Cell side length is 4000 game-coordinate
/// units, with additive X/Z biases 200/3450. Activation binds viewCoord; all
/// borrowed mesh pools and cell lists remain live while this room is active.
static WorldCollisionGrid _gDryfieldTrailerCoachRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldTrailerCoachRoomCollisionGridNormals,
    .vertices    = _gDryfieldTrailerCoachRoomCollisionGridVerts,
    .faces       = _gDryfieldTrailerCoachRoomCollisionGridFaces,
    .cellFaceIds = _gDryfieldTrailerCoachRoomCollisionGridTable,
    .xBias       = 200,
    .zBias       = 3450,
    .cellCountX  = ARRAY_SIZE(_gDryfieldTrailerCoachRoomCollisionGridTable),
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldTrailerCoachRoomCollisionGridFaces),
};

AreaResource D_dryfield_trailer_coach_801876D8[2] = {
    { 106, 207, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_420700_8013EF68 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_trailer_coach_801876F0[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017BA14, D_dryfield_trailer_coach_801876D8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

ViewCamera gDryfieldTrailerCoachViewCameras[DRYFIELD_TRAILER_COACH_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -5000, 0x2710, 1625 } }, 230 },
    { { { { 373, 0, 4078 }, { 877, 4000, -80 }, { -3983, 881, 364 } }, { -5900, 1690, 2010 } }, 230 },
    { { { { 879, 0, -4000 }, { -892, 3992, -196 }, { 3899, 913, 857 } }, { -1154, 1800, 2260 } }, 230 },
    { { { { 595, 0, -4052 }, { -1245, 3897, -183 }, { 3856, 1258, 567 } }, { -6104, 1950, 2060 } }, 230 },
    { { { { -18, 0, -4095 }, { 0, ONE, 0 }, { 4095, 0, -18 } }, { -8071, 1241, 2144 } }, 230 },
    { { { { 2991, 0, -2797 }, { 212, 4084, 226 }, { 2789, -310, 2982 } }, { -5571, 801, 2134 } }, 230 },
    { { { { -3734, 0, -1681 }, { -909, 3445, 2019 }, { 1414, 2214, -3141 } }, { -6604, 1680, 1710 } }, 230 },
    { { { { -450, 0, 4071 }, { 108, 4094, 12 }, { -4069, 109, -450 } }, { -6545, 1155, 1159 } }, 230 },
    { { { { -3740, 0, 1669 }, { 740, 3671, 1658 }, { -1496, 1816, -3352 } }, { -4627, 1470, 492 } }, 221 },
    { { { { 1613, 0, 3764 }, { 1597, 3709, -684 }, { -3409, 1737, 1460 } }, { -525, 1650, 866 } }, 230 },
    { { { { 3255, 0, -2485 }, { 160, 4087, 210 }, { 2480, -265, 3248 } }, { -3895, 1115, 2089 } }, 289 },
};

/// Image-background selector and empty sprite list for mapped view 1.
static SpriteBatch _gDryfieldTrailerCoachView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and unscaled sorting depths for mapped view 2.
///
/// The 101 mutable sources use pixel coordinates relative to the view centre;
/// the corresponding batch list partitions the complete source array.
static SpriteSource _gDryfieldTrailerCoachView2SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -72, 24, 750, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 40 } }, -72, 64, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -64, 104, 750, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 48, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 48, 585, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 591, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 547, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 48, 591, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 48, 591, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 591, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 48, 591, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 48, 582, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 48, 591, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 48, 551, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 56, 468, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 56, 492, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 56, 549, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 56, 522, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 549, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 56, 549, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 549, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 56, 549, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 56, 549, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 56, 562, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 56, 554, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 443, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 64, 461, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 64, 484, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 64, 512, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 64, 512, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 512, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 512, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 64, 512, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 64, 512, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 64, 512, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 64, 533, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 480, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 72, 453, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 72, 476, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 72, 480, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 72, 480, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 479, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 479, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 72, 479, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 72, 479, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 72, 490, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 80, 401, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 80, 401, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 80, 409, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 80, 451, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 80, 451, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 80, 451, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 80, 452, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 80, 452, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 80, 452, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 80, 467, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 88, 373, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 88, 373, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 88, 379, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 88, 386, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 88, 407, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 88, 407, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 88, 426, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 88, 426, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 88, 434, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 96, 353, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 96, 359, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 96, 366, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 96, 382, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 96, 385, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 96, 403, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 96, 404, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 96, 404, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 416, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 104, 347, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 104, 364, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 104, 365, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 104, 366, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 104, 384, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 104, 384, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 104, 384, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 104, 390, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 112, 347, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 112, 353, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 112, 351, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 112, 365, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 112, 365, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 112, 365, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 112, 365, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 112, 375, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, -24, 700, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -64, 48, 625, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 16 } }, -136, 32, 625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, 40, 675, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 40, 750, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 40, 725, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -120, 16, 700, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 16, 725, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -8, 1150, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -8, 1150, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, 24, 1100, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
};

/// Image-background selector and sprite ranges for mapped view 2.
static SpriteBatch _gDryfieldTrailerCoachView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 94, 0, 0, { 2, 0 } },
    { 98, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and unscaled sorting depths for mapped view 3.
///
/// The 160 mutable sources use pixel coordinates relative to the view centre;
/// the corresponding batch list partitions the complete source array.
static SpriteSource _gDryfieldTrailerCoachView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 40, -24, 1562, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 0, 465, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 0, 481, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 0, 489, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 490, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 0, 493, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 499, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 0, 502, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 8, 988, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 8, 990, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 8, 451, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 8, 440, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 8, 446, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 8, 453, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 8, 459, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 8, 466, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 473, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 475, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 16, 924, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 16, 921, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 16, 453, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 16, 442, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 16, 435, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 16, 427, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 16, 431, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 16, 437, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 16, 443, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 16, 445, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 462, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 16, 449, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 24, 456, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 24, 445, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 24, 438, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 24, 430, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 24, 423, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 24, 413, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 401, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 400, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 404, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 24, 403, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 32, 458, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 32, 448, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 32, 440, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 32, 433, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 32, 425, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 32, 419, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 32, 402, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 390, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 380, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 32, 379, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 40, 461, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 40, 451, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 40, 443, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 40, 436, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 40, 428, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 40, 416, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 404, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 40, 394, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 40, 382, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 40, 379, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 48, 464, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 454, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 48, 446, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 48, 438, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 48, 431, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 48, 419, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 48, 405, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 394, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 383, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 48, 381, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 56, 467, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 56, 457, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 56, 449, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 441, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 56, 433, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 56, 420, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 407, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 56, 396, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 56, 384, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 382, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 64, 495, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 64, 499, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 64, 484, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 479, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 465, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 64, 423, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 64, 413, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 64, 397, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 64, 386, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 384, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 484, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 72, 468, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 72, 467, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 455, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 72, 452, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 451, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 451, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 399, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 392, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 468, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 482, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 467, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 80, 450, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 441, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 80, 441, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 436, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 434, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 438, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 80, 445, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 473, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 88, 466, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 88, 466, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 88, 454, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 439, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 88, 423, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 417, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 88, 417, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 88, 438, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 461, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 450, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 96, 442, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 96, 442, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 96, 442, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 440, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 425, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 412, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 425, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 442, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 442, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 442, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 104, 420, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 104, 420, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 104, 420, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 420, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 420, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 413, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 420, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 420, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 420, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 420, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 433, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 430, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 426, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 423, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 409, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 406, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 403, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 400, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 400, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 400, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -48, 96, 375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 48, 650, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 48, 450, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -56, 72, 400, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, 56, 450, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 0, 56, 500, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 32, 24, 600, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 0, 64, 450, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, 0, 80, 400, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 0, 104, 375, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
};

/// Image-background selector and sprite ranges for mapped view 3.
static SpriteBatch _gDryfieldTrailerCoachView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 159, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and unscaled sorting depths for mapped view 4.
///
/// The 41 mutable sources use pixel coordinates relative to the view centre;
/// the corresponding batch list partitions the complete source array.
static SpriteSource _gDryfieldTrailerCoachView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -64, 500, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 450, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 88, 362, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 88, 361, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 88, 375, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 88, 394, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 673, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 96, 343, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 96, 360, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 96, 382, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 96, 402, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 104, 332, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 104, 334, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 104, 366, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 104, 389, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 112, 610, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 112, 317, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 112, 340, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 112, 373, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 112, 397, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 96, 475, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, 96, 475, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 80, 550, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 487, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 64, 450, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -8, 450, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -96, 458, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 144, 8, 400, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 48, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 80, 56, 450, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, 104, 375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, 64, 80, 400, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 80, 550, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 56, 450, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 72, 575, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, 24, 450, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 48, 450, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, 48, 587, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, 8, 450, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 16, 80, 400, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
};

/// Image-background selector and sprite ranges for mapped view 4.
static SpriteBatch _gDryfieldTrailerCoachView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-background selector and empty sprite list for mapped view 5.
static SpriteBatch _gDryfieldTrailerCoachView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-background selector and empty sprite list for mapped view 6.
static SpriteBatch _gDryfieldTrailerCoachView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-background selector and empty sprite list for mapped view 7.
static SpriteBatch _gDryfieldTrailerCoachView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and unscaled sorting depths for mapped view 8.
///
/// The 5 mutable sources use pixel coordinates relative to the view centre;
/// the corresponding batch list partitions the complete source array.
static SpriteSource _gDryfieldTrailerCoachView8SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 120, 8, 312, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 128, 8, 312, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 136, 8, 312, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 144, 8, 312, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 152, 8, 312, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
};

/// Image-background selector and sprite ranges for mapped view 8.
static SpriteBatch _gDryfieldTrailerCoachView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldTrailerCoachView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-background selector and empty sprite list for mapped view 9.
static SpriteBatch _gDryfieldTrailerCoachView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-background selector and empty sprite list for mapped view 10.
static SpriteBatch _gDryfieldTrailerCoachView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-background selector and empty sprite list for mapped view 11.
static SpriteBatch _gDryfieldTrailerCoachView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldTrailerCoachSpriteViews[DRYFIELD_TRAILER_COACH_VIEW_COUNT] = {
    { { .empty = _gDryfieldTrailerCoachView1SpriteBatches }, _gDryfieldTrailerCoachView1SpriteBatches, NULL },
    { { .elements = _gDryfieldTrailerCoachView2SpriteSources }, _gDryfieldTrailerCoachView2SpriteBatches, NULL },
    { { .elements = _gDryfieldTrailerCoachView3SpriteSources }, _gDryfieldTrailerCoachView3SpriteBatches, NULL },
    { { .elements = _gDryfieldTrailerCoachView4SpriteSources }, _gDryfieldTrailerCoachView4SpriteBatches, NULL },
    { { .empty = _gDryfieldTrailerCoachView5SpriteBatches }, _gDryfieldTrailerCoachView5SpriteBatches, NULL },
    { { .empty = _gDryfieldTrailerCoachView6SpriteBatches }, _gDryfieldTrailerCoachView6SpriteBatches, NULL },
    { { .empty = _gDryfieldTrailerCoachView7SpriteBatches }, _gDryfieldTrailerCoachView7SpriteBatches, NULL },
    { { .elements = _gDryfieldTrailerCoachView8SpriteSources }, _gDryfieldTrailerCoachView8SpriteBatches, NULL },
    { { .empty = _gDryfieldTrailerCoachView9SpriteBatches }, _gDryfieldTrailerCoachView9SpriteBatches, NULL },
    { { .empty = _gDryfieldTrailerCoachView10SpriteBatches }, _gDryfieldTrailerCoachView10SpriteBatches, NULL },
    { { .empty = _gDryfieldTrailerCoachView11SpriteBatches }, _gDryfieldTrailerCoachView11SpriteBatches, NULL },
};

/// Four linked view boundaries, pairing transitions 2/3 and 3/4.
///
/// The final entry carries LAST. Room setup mutates links, coordinate and enable
/// state; positions use game coordinates and normals use 4096 per unit.
static WorldCollisionTrigger _gDryfieldTrailerCoachViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 3199, -1168, -737, 0 }, { { 13, -1904, -1570, 0 }, { -14, -1904, 1569, 0 }, { 13, 1904, -1570, 0 }, { -14, 1904, 1569, 0 } }, { 4110, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3246, -1120, -801, 0 }, { { 16, -1888, 1619, 0 }, { -16, -1888, -1619, 0 }, { 16, 1888, 1619, 0 }, { -16, 1888, -1619, 0 } }, { -4096, 0, 39, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7918, -1376, -1538, 0 }, { { -2, -2112, 1038, 0 }, { -1, -2112, -1043, 0 }, { -2, 2112, 1038, 0 }, { -1, 2112, -1043, 0 } }, { -4106, 0, -4, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7806, -1376, -1538, 0 }, { { -7, -2112, -1048, 0 }, { -8, -2112, 1031, 0 }, { -7, 2112, -1048, 0 }, { -8, 2112, 1031, 0 } }, { 4097, 0, 1, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed departure slot 1 and junk-yard arrival slot 2, four bits each.
enum { DRYFIELD_TRAILER_COACH_JUNK_YARD_WARP_SLOTS = (1 << 4) | 2 };

/// Twelve interaction quads: departure to the junk yard followed by eleven CAP actions.
///
/// The final entry carries LAST. Room setup mutates links, coordinate and hit
/// state; positions/radii use game coordinates and normals use 4096 per unit.
static WorldCollisionTrigger _gDryfieldTrailerCoachActionTriggers[12] = {
    { NULL, NULL, NULL, { 1536, -48, -784, 0 }, { { -416, 0, -272, 0 }, { 416, 0, -272, 0 }, { -416, 0, 272, 0 }, { 416, 0, 272, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 496, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_JUNK_YARD, DRYFIELD_TRAILER_COACH_JUNK_YARD_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3631, -64, -2321, 0 }, { { -656, 0, 1422, 0 }, { 158, 0, -2906, 0 }, { 323, 0, 1403, 0 }, { 1329, 0, 147, 0 } }, { 0, 4120, 0, 0 }, { 0, 0, -4096, 0 }, 2907, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 160, -64, -656, 0 }, { { -416, 0, -448, 0 }, { 416, 0, -448, 0 }, { -416, 0, 448, 0 }, { 416, 0, 448, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 610, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 128, -64, -1664, 0 }, { { -416, 0, -544, 0 }, { 416, 0, -544, 0 }, { -416, 0, 544, 0 }, { 416, 0, 544, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 683, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1856, -64, -3040, 0 }, { { -768, 0, -272, 0 }, { 768, 0, -272, 0 }, { -768, 0, 272, 0 }, { 768, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9248, -64, -480, 0 }, { { -416, 0, -544, 0 }, { 416, 0, -544, 0 }, { -416, 0, 544, 0 }, { 416, 0, 544, 0 } }, { 0, 4104, 0, 0 }, { -4091, 0, 201, 0 }, 683, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9184, -64, -2224, 0 }, { { -416, 0, -1136, 0 }, { 416, 0, -1136, 0 }, { -416, 0, 1136, 0 }, { 416, 0, 1136, 0 } }, { 0, 4100, 0, 0 }, { -4091, 0, 201, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6640, -64, -1136, 0 }, { { -1456, 0, -320, 0 }, { 1456, 0, -320, 0 }, { -1456, 0, 320, 0 }, { 1456, 0, 320, 0 } }, { 0, 4096, 0, 0 }, { -201, 0, -4091, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6800, -64, -2080, 0 }, { { -1600, 0, -320, 0 }, { 1600, 0, -320, 0 }, { -1600, 0, 320, 0 }, { 1600, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, 4090, 0 }, 1629, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4096, -64, -256, 0 }, { { -1424, 0, -832, 0 }, { 944, 0, -832, 0 }, { -1424, 0, 320, 0 }, { 944, 0, 320, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1649, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3296, -64, -2464, 0 }, { { -1440, 0, -848, 0 }, { 352, 0, -848, 0 }, { -1440, 0, 1680, 0 }, { 352, 0, 1680, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8624, -64, -2496, 0 }, { { -336, 0, -736, 0 }, { 336, 0, -736, 0 }, { -336, 0, 736, 0 }, { 336, 0, 736, 0 } }, { 0, 4102, 0, 0 }, { 4016, 0, 798, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Twelve point lights shared by all trailer-coach views.
///
/// RGB intensities use twelve fractional bits; placement and falloff radii use
/// world-coordinate units. Lighting updates the coordinate caches, parents and
/// attenuation in these room-owned records.
static WorldCoordPointLight _gDryfieldTrailerCoachPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2807, -936, -1738 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3280, 3076, 2870 }, { 0, 0 } }, 1587, 2185 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8192, -2555, -1604 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 2460, 2460 }, { 0, 0 } }, 1120, 1672 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6380, -2555, -1625 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 2460, 2460 }, { 0, 0 } }, 1057, 1718 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2944, -1003, -1164 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3278, 3278, 3278 }, { 0, 0 } }, 1839, 2505 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 490, -1975, -479 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3072 }, { 0, 0 } }, 1107, 1294 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 169, -2006, -1625 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3076 }, { 0, 0 } }, 1112, 1374 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -911, -2035, -2681 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3072 }, { 0, 0 } }, 1288, 1572 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1618, -1020, -130 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2870, 2870, 2870 }, { 0, 0 } }, 2591, 3272 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3750, -1440, -3 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1229, 1229 }, { 0, 0 } }, 2029, 2673 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6865, -1440, 10 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1229, 1229 }, { 0, 0 } }, 1935, 2601 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6885, -1440, -2420 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2666, 2666, 2663 }, { 0, 0 } }, 2646, 3283 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3665, -1440, -2318 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3280, 3280, 3280 }, { 0, 0 } }, 2164, 2832 },
};

/// Room light collection containing point lights only.
static WorldCoordRoomLights _gDryfieldTrailerCoachLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldTrailerCoachPointLights),
    .pointLights           = _gDryfieldTrailerCoachPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// View-count header followed by the eleven views' minimum ambient colours.
///
/// Entries 1..11 use sixteen units per colour level. Luma retains the authored
/// weighted brightness but does not participate in ambient-light clamping.
static const WorldCoordRoomAmbientEntry _gDryfieldTrailerCoachAmbientByView[DRYFIELD_TRAILER_COACH_VIEW_COUNT + 1] SECTION(".data") = {
    { .viewCount = ARRAY_SIZE(_gDryfieldTrailerCoachAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1025, 1025, 1025, 1025 } },
    { .color = { 1025, 1025, 1025, 1025 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1025, 1025, 1025, 1025 } },
    { .color = { 1025, 1025, 1025, 1025 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

enum {
    DRYFIELD_TRAILER_COACH_WALK_FOOTSTEP_CUE2 = 0x1000002D,
    DRYFIELD_TRAILER_COACH_RUN_FOOTSTEP_CUE2  = 0x1000002F,
};

/// Class-6 footstep cue bases in the currently loaded type-1 sound bank.
///
/// Cue 1 uses base + 1; companions add 100. Scripted jumps share the walk pair.
static const WorldCollisionFootstepSounds _gDryfieldTrailerCoachFootstepSounds SECTION(".data") = {
    .walk         = DRYFIELD_TRAILER_COACH_WALK_FOOTSTEP_CUE2,
    .run          = DRYFIELD_TRAILER_COACH_RUN_FOOTSTEP_CUE2,
    .scriptedJump = DRYFIELD_TRAILER_COACH_WALK_FOOTSTEP_CUE2,
};

/// Blocking surface policy shared by classes 0..4 and 7, without footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldTrailerCoachDefaultSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Separate class-5 blocking surface policy, without footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldTrailerCoachSurfaceClass5 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-6 blocking surface policy with walking, running and scripted-jump cues.
static WorldCollisionSurfaceProperties _gDryfieldTrailerCoachFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldTrailerCoachFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldTrailerCoachSurfaceClasses[DRYFIELD_TRAILER_COACH_SURFACE_CLASS_COUNT] = {
    &_gDryfieldTrailerCoachDefaultSurface,
    &_gDryfieldTrailerCoachDefaultSurface,
    &_gDryfieldTrailerCoachDefaultSurface,
    &_gDryfieldTrailerCoachDefaultSurface,
    &_gDryfieldTrailerCoachDefaultSurface,
    &_gDryfieldTrailerCoachSurfaceClass5,
    &_gDryfieldTrailerCoachFootstepSurface,
    &_gDryfieldTrailerCoachDefaultSurface,
};

AreaApplyRec D_dryfield_trailer_coach_80189C50[15] = {
    { 2, 5, 7, 33 },
    { 2, 11, 2, 17 },
    { 2, 11, 7, 33 },
    { 2, 12, 2, 17 },
    { 2, 12, 7, 33 },
    { 2, 16, 2, 17 },
    { 2, 16, 7, 33 },
    { 2, 19, 2, 1 },
    { 2, 20, 2, 1 },
    { 2, 21, 2, 0 },
    { 2, 22, 3, 1 },
    { 2, 24, 3, 1 },
    { 2, 25, 2, 0 },
    { 2, 26, 2, 0 },
    { 255, 0, 0, 0 },
};

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
static s32 _gShopRefillDisplayQuantityFixed = 0;

/// Borrowed weapon-supply catalogue row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalogue must remain loaded throughout the session.
static const EquipmentWeaponSupply* _gShopRefillSupply = NULL;

RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask = { 0 };

RoomCutsceneRec D_dryfield_trailer_coach_80189C9C = { 0 };

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

/// Descriptor of the panel `func_dryfield_trailer_coach_8017FE98` opens.
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

extern UiObjectDesc D_800611E4;

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
static UiList Telephone_Data_80181C6C;

static UiList Telephone_Data_80181CF4;

static UiObjectDesc Telephone_Data_80181C90;

/// List of the menu panel `func_dryfield_trailer_coach_8018181C` draws.
static UiList Telephone_Data_80181C44;

/// Texts of the four menu rows below, and the panels two of them open.
static u8 Telephone_Data_801819F8[];

static u8 Telephone_Data_80181A00[];

static u8 Telephone_Data_80181A0C[];

static u8 Telephone_Data_80181A18[];

static UiObjectDesc Telephone_Data_80181CAC;

static UiObjectDesc Telephone_Data_80181CC8;

/// This room's cutscene sound-task slot. The runner uses `task`.
extern RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask;

extern EvsCommand D_dryfield_trailer_coach_80185AFC[];

extern EvsCommand D_dryfield_trailer_coach_80185C4C[];

extern EvsCommand D_dryfield_trailer_coach_80185D54[];

extern EvsCommand D_dryfield_trailer_coach_80186684[];

extern EvsCommand D_dryfield_trailer_coach_8018681C[];

extern EvsCommand D_dryfield_trailer_coach_80186A74[];

extern EvsCommand D_dryfield_trailer_coach_80186BDC[];

extern EvsCommand D_dryfield_trailer_coach_80186D2C[];

extern EvsCommand D_dryfield_trailer_coach_80187074[];

extern AreaApplyRec D_dryfield_trailer_coach_80189C50[];

/// Second descriptor of the trailer's spawn table (spawned by request 3).
extern TaskDesc D_dryfield_trailer_coach_80184FC0[];

/// The cutscene record this room hands `gRoomCutsceneTaskDescs`.
extern RoomCutsceneRec D_dryfield_trailer_coach_80189C9C;

static void _dryfieldTrailerCoachExitTopicChoice(Task* task);

extern TaskMessageEntry D_dryfield_trailer_coach_80184FA0[];

extern EvsCommand D_dryfield_trailer_coach_801853F4[];

extern EvsCommand D_dryfield_trailer_coach_80185964[];

static void _dryfieldTrailerCoachInitializeRoomTask(Task* task);

static void _dryfieldTrailerCoachOpenTopicChoice(Task* task);

#include "../../shared/shop.inc.c"
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/telephone.inc.c"

void dryfieldTrailerCoachTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

#include "../../shared/room_cutscene_task.inc.c"

/// Starts the first story conversation and commits the progress it establishes.
///
/// Event and skip scripts remain room-owned. Flags, objective 15, scene event 6
/// and the saved-area changes are committed as soon as playback is requested.
static inline void _dryfieldTrailerCoachStartFirstStoryConversation(void)
{
    enum { CONVERSATION_OBJECTIVE                = 15,
           CONVERSATION_SCENE_EVENT              = 6,
           DRIVEWAY_FIRST_CONVERSATION_PROGRESS  = 1,
           COMPANION_FIRST_CONVERSATION_SCHEDULE = 1 };

    evsStartScriptWithSkip(D_dryfield_trailer_coach_80185D54, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                           D_dryfield_trailer_coach_80186684);
    gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS, DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE);
    gameFlagSetNibble(GAME_FLAG_DRIVEWAY_PROGRESS, DRIVEWAY_FIRST_CONVERSATION_PROGRESS);
    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_FIRST_CONVERSATION_SCHEDULE);
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, CONVERSATION_OBJECTIVE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = CONVERSATION_SCENE_EVENT;
    areaApplySavedUpdates(D_dryfield_trailer_coach_80189C50);
}

/// Selects the trailer-coach conversation from CAP choice, progress and wrench collection.
///
/// State 0 holds player control and starts the preparation script; state 1 waits
/// for the session event to end. State 2 starts the shop on CAP key 11, otherwise
/// selects a first/repeat story scene or the gated Monkey Wrench conversation.
/// The first story scene commits progress 2, objective 15, scene event 6 and
/// saved area updates immediately; the next commits progress 3. State 3 releases
/// this selector without waiting for the selected script. Requires loaded room
/// scripts and a live save/session; it owns no separate work allocation.
static void _dryfieldTrailerCoachConversationTask(Task* task)
{
    enum { CONVERSATION_PREPARE,
           CONVERSATION_WAIT,
           CONVERSATION_SELECT,
           CONVERSATION_EXIT,
           CONVERSATION_SHOP_KEY = 11 };

    switch (task->state) {
        case CONVERSATION_PREPARE:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            evsStartScript(D_dryfield_trailer_coach_80185AFC, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case CONVERSATION_WAIT:
            if (gGameSession->eventState == 0) {
                task->state = CONVERSATION_SELECT;
            }
            break;
        case CONVERSATION_SELECT:
            // Select the shop, story progression or wrench conversation after preparation.
            if (capGetVariantKey() == CONVERSATION_SHOP_KEY) {
                evsStartScript(D_dryfield_trailer_coach_80185C4C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) < DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE) {
                _dryfieldTrailerCoachStartFirstStoryConversation();
            } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_MONKEY_WRENCH) == 0 && gameFlagGetNibble(GAME_FLAG_DRYFIELD_TRAILER_COACH_04F) != 0) {
                if (gameFlagGetNibble(GAME_FLAG_0FD) == 0) {
                    gameFlagSetNibble(GAME_FLAG_0FD, 1);
                    evsStartScript(D_dryfield_trailer_coach_80186D2C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                } else {
                    evsStartScript(D_dryfield_trailer_coach_80187074, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                }
            } else if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) == DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE) {
                evsStartScriptWithSkip(D_dryfield_trailer_coach_8018681C, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       D_dryfield_trailer_coach_80186A74);
                gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS, DRYFIELD_TRAILER_COACH_SECOND_CONVERSATION_COMPLETE);
            } else {
                evsStartScript(D_dryfield_trailer_coach_80186BDC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            task->state++;
            break;
        case CONVERSATION_EXIT:
            taskKill(task);
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses every key-item use in this room without consuming the item.
///
/// Handles the inventory's key-item request; `itemId` is a collected-item ID
/// and `unusedSecondArg` is normally zero. All arguments are ignored. The zero
/// result selects the inventory's "No use now" notice.
static s32 _dryfieldTrailerCoachRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { DRYFIELD_TRAILER_COACH_KEY_ITEM_USE_REJECTED = 0 };

    return DRYFIELD_TRAILER_COACH_KEY_ITEM_USE_REJECTED;
}

/// Permits a room transition with the requested destination unchanged.
///
/// Copies the complete eight-byte request into the reply and returns 1 in both
/// query and execute modes. Borrows readable request and writable reply records
/// for synchronous dispatch; they may be the same object. Retains neither
/// pointer and performs no transition effects itself.
static s32 _dryfieldTrailerCoachResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_TRAILER_COACH_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_TRAILER_COACH_TRANSITION_ALLOWED;
}

/// Queues the repeat trailer-coach story scene with its persistent room record.
///
/// The cutscene borrows this record through playback; CAP slot/data resource 1,
/// view 10 and room sound scripts 3/4/5/6 must remain loaded. Texture-page values
/// remain those already in the record. Its later CAP command is 3. The record
/// must not be overwritten while a runner is live; allocation failure is ignored.
static inline void _dryfieldTrailerCoachStartRepeatStoryScene(void)
{
    enum { REPEAT_SCENE_VIEW          = 10,
           REPEAT_SCENE_CAP_SLOT      = 1,
           REPEAT_SCENE_CAP_FILE      = 1,
           REPEAT_SCENE_TASK          = 0,
           REPEAT_SCENE_FOLLOW_UP_CAP = 3 };

    // The cutscene task borrows this persistent record after dispatch returns.
    D_dryfield_trailer_coach_80189C9C.view            = REPEAT_SCENE_VIEW;
    D_dryfield_trailer_coach_80189C9C.capSlot         = REPEAT_SCENE_CAP_SLOT;
    D_dryfield_trailer_coach_80189C9C.capFile         = REPEAT_SCENE_CAP_FILE;
    D_dryfield_trailer_coach_80189C9C.skipScene       = false;
    D_dryfield_trailer_coach_80189C9C.startSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 3);
    D_dryfield_trailer_coach_80189C9C.endSound        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 5);
    D_dryfield_trailer_coach_80189C9C.sceneSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 4);
    D_dryfield_trailer_coach_80189C9C.afterSceneSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 6);
    taskSpawnFromTable(gRoomCutsceneTaskDescs, REPEAT_SCENE_TASK, REPEAT_SCENE_FOLLOW_UP_CAP, &D_dryfield_trailer_coach_80189C9C);
}

/// Handles CAP command indices that start trailer-coach conversations and story scenes.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer `commandIndex`: 3 queues the
/// conversation selector; 14 replaces arrival 2 with arrival 1 in the live save,
/// then runs CAP command 29 once or spawns the repeat scene from a persistent
/// room-owned record (view 10, CAP slot/file 1). The other arguments are ignored.
/// Always returns zero, including unhandled commands and failed task spawns.
/// Keep this overlay and its scripts/scene record loaded while spawned tasks run.
static s32 _dryfieldTrailerCoachHandleCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum { COMMAND_CONVERSATION = 3,
           COMMAND_STORY_SCENE  = 14,
           TASK_CONVERSATION    = 1,
           ARRIVAL_STORY_SCENE  = 2,
           ARRIVAL_DEFAULT      = 1,
           CAP_FIRST_SCENE      = 29 };

    if (commandIndex == COMMAND_CONVERSATION) {
        taskSpawnFromTable(D_dryfield_trailer_coach_80184FC0, TASK_CONVERSATION, 0, 0);
    }
    if (commandIndex == COMMAND_STORY_SCENE) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == ARRIVAL_STORY_SCENE) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = ARRIVAL_DEFAULT;
        }
        if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_FIRST_SCENE, 1);
            capRunCommandWithTransition(CAP_FIRST_SCENE);
            return 0;
        }
        _dryfieldTrailerCoachStartRepeatStoryScene();
        return 0;
    }
    return 0;
}

/// Opens the trailer coach's two-topic choice and advances to its answer wait.
///
/// Owns a primary-heap `RoomOptionDialog` in `task->work` until task teardown.
/// `spawnArg1.value == 1` selects firearms/anything else; every other value
/// selects firearms/shelter. The UI borrows the request and its two linked
/// options until answered; cancellation is disabled. Allocation failure kills
/// the task. Requires the label table and this overlay to stay loaded.
static void _dryfieldTrailerCoachOpenTopicChoice(Task* task)
{
    enum { DRYFIELD_TRAILER_COACH_SECOND_TOPIC_MENU = 1 };
    RoomOptionDialog* dialog;
    UiDialogOption*   option;
    u8* const*        firstMenuCursor;
    u8* const*        topicLabels;
    s32               secondMenuSelector;
    s32               labelIndex;
    s32               optionIndex;

    dialog = memCalloc(sizeof(*dialog), 0);
    if (dialog == NULL) {
        taskKill(task);
        return;
    }

    option = dialog->options;

    // Select one of the two contiguous label pairs without crossing its bounds.
    optionIndex        = 0;
    secondMenuSelector = DRYFIELD_TRAILER_COACH_SECOND_TOPIC_MENU;
    firstMenuCursor    = _gDryfieldTrailerCoachTopicChoiceLabels;
    topicLabels        = firstMenuCursor;
    task->work         = dialog;
    task->exitCallback = _dryfieldTrailerCoachExitTopicChoice;

    for (; optionIndex < ARRAY_SIZE(dialog->options); optionIndex++) {
        if (task->spawnArg1.value == secondMenuSelector) {
            labelIndex   = optionIndex + ARRAY_SIZE(dialog->options);
            option->text = topicLabels[labelIndex];
        } else {
            option->text = *firstMenuCursor;
        }
        option->next = option + 1;
        option++;
        firstMenuCursor++;
    }
    option[-1].next = NULL;

    dialog->request.optionCount = ARRAY_SIZE(dialog->options);
    dialog->request.options     = dialog->options;
    dialog->request.title       = NULL;
    dialog->request.flags       = 0;
    uiSpawnOptionDialog(&dialog->request, 0, 0, 0);
    task->state++;
}

/// Waits for a topic choice and publishes its one-based option number.
///
/// `Task::work` must hold the live `RoomOptionDialog` opened by the preceding
/// state. `spawnArg2.pointer` borrows a writable, word-aligned `s32` that must
/// outlive this wait. A zero answer leaves the destination and state untouched;
/// a nonzero signed-halfword answer is widened to a word before advancing to
/// teardown. The two-option request disables cancellation, so normal answers
/// are 1 or 2. The task owns the dialog allocation, not the result destination.
static void _dryfieldTrailerCoachWaitForTopicChoice(Task* task)
{
    const RoomOptionDialog* dialog;
    s16                     choiceResult;

    dialog       = task->work;
    choiceResult = dialog->request.result;
    if (choiceResult != USER_INTERFACE_RESULT_NONE) {
        s32* choiceDestination = task->spawnArg2.pointer;

        *choiceDestination = choiceResult;
        task->state++;
    }
}

/// Releases the topic-choice task and requests exit from its stage mode.
///
/// Used both as the final state and as the task's exit callback. `taskKill`
/// releases the owned `RoomOptionDialog` before the mode-exit request; the
/// result destination in `spawnArg2.pointer` remains caller-owned.
static void _dryfieldTrailerCoachExitTopicChoice(Task* task)
{
    taskKill(task);
    stageRequestModeTaskExit();
}

/// State table used by the room receiver
/// `dryfieldTrailerCoachRoomTask`.
static const TaskFuncTable3 D_dryfield_trailer_coach_8017D7DC = {
    {
        _dryfieldTrailerCoachInitializeRoomTask,
        _trailerCoachSetDepthShift,
        taskKill,
    },
};

/// State table of the room's two-option choice task, run by
/// `_dryfieldTrailerCoachTopicChoiceTask`: open the dialog, wait for its
/// answer, then kill the task and call `stageRequestModeTaskExit`.
static const TaskFuncTable3 D_dryfield_trailer_coach_8017D7E8 = {
    {
        _dryfieldTrailerCoachOpenTopicChoice,
        _dryfieldTrailerCoachWaitForTopicChoice,
        _dryfieldTrailerCoachExitTopicChoice,
    },
};

/// Runs the trailer's two-topic dialog through opening, choice publication and exit.
///
/// Requires a live task with state in 0..2: open, wait, then stage-mode exit.
/// `spawnArg1.value == 1` selects the second label pair; every other value
/// selects the first. `spawnArg2.pointer` must borrow a writable, word-aligned
/// `s32` until the chosen option (1 or 2) is published. The task owns its dialog
/// work, and exit releases it before requesting stage-mode teardown.
static void _dryfieldTrailerCoachTopicChoiceTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_trailer_coach_8017D7E8;
    stateHandlers.funcs[task->state](task);
}

/// Starts conversation CAP slot 3 with its progress-dependent variant.
///
/// The preparation script calls this without arguments. Progress below 2 selects
/// variant 1; progress 2 or above selects variant 2, using in-place playback.
/// Requires the room's relocated CAP command table to remain loaded.
static void _dryfieldTrailerCoachStartConversationCaption(void)
{
    enum { CAP_CONVERSATION = 3 };
    s32 variantKey;

    variantKey  = gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) >= DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE;
    variantKey += 1;
    capStartSequenceSlot(CAP_CONVERSATION, CAP_PLAYBACK_IN_PLACE, variantKey);
}

/// Registers the trailer-coach room receiver and starts its arrival presentation.
///
/// Installs the room message table, claims `GAME_TASK_SLOT_ROOM` and advances to
/// state 1. Live-save arrival 2 starts the skippable entry scene and resets its
/// follow-up/dialogue state; other arrivals start area music. Requires this room's
/// scripts to remain loaded while playback and message reception are active.
static void _dryfieldTrailerCoachInitializeRoomTask(Task* task)
{
    enum { ARRIVAL_STORY_SCENE  = 2,
           STORY_DIALOGUE_START = 4,
           AREA_MUSIC_START     = 1 };

    task->msgTable = D_dryfield_trailer_coach_80184FA0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == ARRIVAL_STORY_SCENE) {
        evsStartScriptWithSkip(D_dryfield_trailer_coach_801853F4, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_trailer_coach_80185964);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, STORY_DIALOGUE_START);
    } else {
        stageMusicRequestAreaStart(AREA_MUSIC_START);
    }
    task->state = task->state + 1;
}

#include "../../shared/trailer_coach_set_depth_shift.inc.c"

void dryfieldTrailerCoachRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_trailer_coach_8017D7DC;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/glow_draw_star_local.inc.c"

#define GLOW_DRAW_RAY_STAR_HALF_FIRST     1
#define GLOW_DRAW_RAY_STAR_OUTER(p, c, h) setRGB2(p, 0, h, h)
#define GLOW_DRAW_RAY_STAR_INNER(p, c, h) setRGB2(p, 0, c, h)
#define GLOW_DRAW_RAY_STAR_RAY(p, c)      setRGB2(p, 0, c, c)
#include "../../shared/glow_draw_ray_star.inc.c"

void dryfieldTrailerCoachDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_TRAILER_COACH_DIAMOND_GLOW_VIEWS   = (1 << 2) | (1 << 8),
        DRYFIELD_TRAILER_COACH_RAY_GLOW_VIEWS       = 1 << 10,
        DRYFIELD_TRAILER_COACH_GLOW_PULSE_RATE      = 0x60, // Angle units per animation frame; 4096 per turn
        DRYFIELD_TRAILER_COACH_DIAMOND_RADIUS_SCALE = 0xC0, // Pixel half-extent is scale * 32 / (camera Z / 4)
        DRYFIELD_TRAILER_COACH_RAY_RADIUS_SCALE     = 0x30, // Outer pixel radius is scale * 64 / (camera Z / 4)
    };

    s32       viewMask;
    GfxCoord* coord;

    viewMask = 1 << gGameSession->location.loc.view;
    coord    = task->extra.coordBody->coord;
    if (viewMask & DRYFIELD_TRAILER_COACH_DIAMOND_GLOW_VIEWS) {
        _glowDrawStarLocal(coord, &_gDryfieldTrailerCoachGlowLocalPoint,
                           DRYFIELD_TRAILER_COACH_GLOW_PULSE_RATE, DRYFIELD_TRAILER_COACH_DIAMOND_RADIUS_SCALE);
        return;
    }
    if (viewMask & DRYFIELD_TRAILER_COACH_RAY_GLOW_VIEWS) {
        _glowDrawRayStar(coord, &_gDryfieldTrailerCoachGlowLocalPoint,
                         DRYFIELD_TRAILER_COACH_GLOW_PULSE_RATE, DRYFIELD_TRAILER_COACH_RAY_RADIUS_SCALE);
    }
}
