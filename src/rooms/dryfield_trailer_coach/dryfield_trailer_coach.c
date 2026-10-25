#include "rooms/dryfield_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_420700.h"

#include "actors/actor_420700.h"

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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
// Symbol is `RoomCutsceneSoundTaskStorage`; the runner reads `task`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask.task
#include "../../shared/room/room_cutscene.h"
#include "../../shared/effects/glow_draw.h"
static void _roomCutsceneTask(Task* task);

/// Selects the daytime trailer coach view that uses the finer depth shift.
///
/// Keep this binding through the depth-shift implementation fragment.
#define DRYFIELD_TIME DRYFIELD_DAY
#include "../../shared/trailer_coach.h"

static void _trailerCoachSetDepthShift(Task* unusedTask);

static void _roomCutsceneSoundTask(Task* task);

static s32 _gShopPreviewRowId;

#include "../../shared/room/telephone.h"

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

#include "../../shared/room/shop.h"

static s32  _dryfieldTrailerCoachRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldTrailerCoachResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _dryfieldTrailerCoachWaitForTopicChoice(Task* task);
static s32  _dryfieldTrailerCoachHandleCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedSecondArg);
static void _dryfieldTrailerCoachConversationTask(Task* task);
static void _dryfieldTrailerCoachTopicChoiceTask(Task* task);

/// Story milestones selecting this room's conversation and caption variants.
enum { DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE  = 2,
       DRYFIELD_TRAILER_COACH_SECOND_CONVERSATION_COMPLETE = 3 };

static AnimationSet                     _gDryfieldTrailerCoachAnimation07668;
static AnimationSet                     _gDryfieldTrailerCoachAnimation07994;
static WorldCollisionGrid               _gDryfieldTrailerCoachRoomCollisionGrid;
static WorldCollisionTrigger            _gDryfieldTrailerCoachViewBoundaryTriggers[4];
static WorldCollisionTrigger            _gDryfieldTrailerCoachActionTriggers[12];
static const WorldCoordRoomAmbientEntry _gDryfieldTrailerCoachAmbientByView[DRYFIELD_TRAILER_COACH_VIEW_COUNT + 1];
static WorldCoordRoomLights             _gDryfieldTrailerCoachLights;
static void                             _dryfieldTrailerCoachStartConversationCaption(void);

#include "../../shared/room/shop_data.inc.c"

TaskDesc gDryfieldTrailerCoachShopSessionTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = 192 } },
    .callback = _shopSessionTask,
    .data     = { .value = 0 },
};

#include "assets/rooms/item_box/model.inc"
DEFINE_ALIAS(_gItemBoxModel, gDryfieldTrailerCoachItemBoxModel);

#include "../../shared/room/telephone_data.inc.c"

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

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Room receiver's transition, key-item rejection and CAP-command handlers.
///
/// The live room task borrows this table and its callbacks until teardown.
/// The fourth entry terminates lookup; unsupported messages return zero.
static const TaskMessageEntry _gDryfieldTrailerCoachRoomMessages[4] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldTrailerCoachResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldTrailerCoachRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _dryfieldTrailerCoachHandleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { DRYFIELD_TRAILER_COACH_INTERACTION_TASK_COUNT = 2 };

/// Bodyless topic-choice and conversation-selector task recipes, at indices 0 and 1.
///
/// There is no terminator. Topic choice takes a label-pair selector in spawnArg1
/// (1 firearms/anything else, otherwise firearms/shelter) and borrows a writable
/// s32 result in spawnArg2 until publication; its launcher is unproven.
/// The room's conversation command spawns entry 1 with both arguments zero.
/// Spawning copies a recipe synchronously; keep this room's callbacks loaded
/// until task completion. The spawner's interface requires a mutable table.
static TaskDesc _gDryfieldTrailerCoachInteractionTaskDescs[DRYFIELD_TRAILER_COACH_INTERACTION_TASK_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldTrailerCoachTopicChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldTrailerCoachConversationTask, { .value = 0 } },
};

/// Places the player for trailer-coach conversations in room coordinates.
///
/// XYZ are game-coordinate units; yaw is -2560 with 4096 units per turn.
/// Scripts borrow this unchanged transform through synchronous placement.
static const ActorTransform _gDryfieldTrailerCoachPlayerConversationTransform SECTION(".data") = { { 4870, 0, -900, 0 }, { 0, -2560, 0, 0 } };

/// Places the player before conversation extension clip 54.
///
/// XYZ use room game-coordinate units; yaw is -2560 with 4096 units per turn.
/// The placement message borrows the unchanged transform through dispatch.
static const ActorTransform _gDryfieldTrailerCoachPlayerClip54Transform SECTION(".data") = {
    .pos = { 5270, 0, -500, 0 },
    .rot = { 0, -2560, 0, 0 },
};

/// Restores the player placement after a conversation or skipped scene.
///
/// Shares the conversation position and uses yaw +2560, in 4096 units per turn.
/// The transform is borrowed unchanged through synchronous placement.
static const ActorTransform _gDryfieldTrailerCoachPlayerAfterConversationTransform SECTION(".data") = { { 4870, 0, -900, 0 }, { 0, 2560, 0, 0 } };

/// Retained 24-byte record with no established consumer or role.
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

/// Clip keys carried by this room's player, companion and Gary requests.
enum {
    DRYFIELD_TRAILER_COACH_COMPANION_CLIP_9   = 9,
    DRYFIELD_TRAILER_COACH_COMPANION_CLIP_1   = 1,
    DRYFIELD_TRAILER_COACH_PLAYER_CLIP_47     = 47,
    DRYFIELD_TRAILER_COACH_PLAYER_CLIP_48     = 48,
    DRYFIELD_TRAILER_COACH_PLAYER_CLIP_49     = 49,
    DRYFIELD_TRAILER_COACH_PLAYER_CLIP_50     = 50,
    DRYFIELD_TRAILER_COACH_PLAYER_CLIP_52     = 52,
    DRYFIELD_TRAILER_COACH_PLAYER_CLIP_54     = 54,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_1   = 1,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_6   = 6,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_7   = 7,
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_8   = 8,
    DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_3 = 3,
    DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_7 = 7,
};

/// Resets companion clip 9 and drops world-grid participation.
///
/// The event interpreter resolves the companion animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachCompanionClip9ResetRequest SECTION(".data") = {
    .source               = { .index = 6 },
    .animationId          = DRYFIELD_TRAILER_COACH_COMPANION_CLIP_9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets companion clip 1 and drops world-grid participation.
///
/// The event interpreter resolves the companion animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachCompanionClip1ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_COMPANION_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained 20-byte record with no established consumer or playback owner.
AnimationPlayRequest D_dryfield_trailer_coach_80185074 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

enum { DRYFIELD_TRAILER_COACH_PLAYER_CLIP_58 = 58,
       DRYFIELD_TRAILER_COACH_PLAYER_CLIP_59 = 59,
       DRYFIELD_TRAILER_COACH_PLAYER_CLIP_60 = 60,
       DRYFIELD_TRAILER_COACH_PLAYER_CLIP_61 = 61,
       DRYFIELD_TRAILER_COACH_PLAYER_CLIP_62 = 62,
       DRYFIELD_TRAILER_COACH_PLAYER_CLIP_63 = 63 };

/// Resets player extension clip 58 and restores world-grid participation on arrival.
///
/// The event interpreter borrows this template and resolves the equipped bank
/// in a local copy. Its stored source index is ignored; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip58ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player extension clip 59 and restores world-grid participation on arrival.
///
/// The interpreter copies this borrowed template before replacing its source
/// index with the equipped bank. Reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip59ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player extension clip 60 and restores world-grid participation on arrival.
///
/// The interpreter copies this borrowed template before replacing its source
/// index with the equipped bank. Reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip60ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_60,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player extension clip 61 and restores world-grid participation on arrival.
///
/// The interpreter copies this borrowed template before replacing its source
/// index with the equipped bank. Reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip61ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_61,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum {
    DRYFIELD_TRAILER_COACH_UNUSED_PLAYER_TEMPLATE_BANK = 1,
};

/// Retained, unused player reset request for room extension clip 47.
///
/// Stores bank selector 1, zero blend frames and enabled grid collision.
/// The room installs this clip in the player extension, but no script selects
/// this record; indexed event playback resolves the equipped bank in a copy.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerUnusedClip47ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_UNUSED_PLAYER_TEMPLATE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player clip 47 and restores world-grid participation.
///
/// The event interpreter resolves the equipped animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip47ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player clip 48 and restores world-grid participation.
///
/// The event interpreter resolves the equipped animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip48ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player clip 49 and restores world-grid participation.
///
/// The event interpreter resolves the equipped animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip49ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player clip 50 and drops world-grid participation.
///
/// The event interpreter resolves the equipped animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip50ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

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

/// Resets player clip 52 and restores world-grid participation.
///
/// The event interpreter resolves the equipped animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip52ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum { DRYFIELD_TRAILER_COACH_PLAYER_CLIP_53 = 53 };

/// Retained, unused player reset request for room extension clip 53.
///
/// Stores bank selector 1, zero blend frames and enabled grid collision.
/// The room installs this clip in the player extension, but no script selects
/// this record; indexed event playback resolves the equipped bank in a copy.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerUnusedClip53ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_UNUSED_PLAYER_TEMPLATE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Resets player clip 54 and drops world-grid participation.
///
/// The event interpreter resolves the equipped animation bank in a local copy;
/// the stored source index is ignored. Scripts borrow this unchanged template
/// through dispatch; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip54ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_AUTO = 0 };

/// Restores Gary's automatic head turn toward the player after scripted turns.
///
/// The trailer-coach command namespace selects the actor's head-turn handler.
/// Selecting automatic mode preserves the current turn weight; later updates
/// adjust it from player facing and scene control. Borrowed through dispatch.
static const ActorCommand _gDryfieldTrailerCoachGaryAutoHeadTurnCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH } },
    .command = DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_AUTO,
};

enum { DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_PLAYER     = 1,
       DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_RELEASE    = 2,
       DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_ROOM_FOCUS = 3 };

/// Makes Gary turn his head toward the player, starting with zero turn weight.
///
/// The stage/area namespace selects Gary's head-turn handler. The script and
/// actor borrow this unchanged command through synchronous dispatch.
static const ActorCommand _gDryfieldTrailerCoachGaryTurnHeadToPlayerCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH } },
    .command = DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_PLAYER,
};

/// Releases Gary's head turn toward the player, starting with full turn weight.
///
/// Subsequent actor updates reduce the weight to zero. The stage/area namespace
/// selects Gary's handler; this unchanged command is borrowed through dispatch.
static const ActorCommand _gDryfieldTrailerCoachGaryReleaseHeadTurnCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH } },
    .command = DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_RELEASE,
};

/// Makes Gary turn his head toward the room focus, starting with zero turn weight.
///
/// The actor supplies the focus in room coordinates. The stage/area namespace
/// selects Gary's handler; this unchanged command is borrowed through dispatch.
static const ActorCommand _gDryfieldTrailerCoachGaryTurnHeadToRoomFocusCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH } },
    .command = DRYFIELD_TRAILER_COACH_GARY_HEAD_TURN_ROOM_FOCUS,
};

enum {
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_0 = 0,
};

/// Retained, unused Gary bank-0 clip-0 reset request.
///
/// Stores zero blend frames and disabled collision; no script selects it.
/// Base-bank slot 0 is NULL, so this is not a playable clip request.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryUnusedBaseClip0ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Gary Douglas base-bank clip 1 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Reset ignores blendFrames; the receiver does not read the collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip1ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_2 = 2,
};

/// Retained, unused Gary bank-0 clip-2 reset request.
///
/// Stores zero blend frames and disabled collision; no script selects it.
/// Gary maps this selector/ID to loaded local set 2;
/// interpolation uses his fixed eight-frame duration.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryUnusedBaseClip2ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_3 = 3,
};

/// Retained, unused Gary bank-0 clip-3 reset request.
///
/// Stores zero blend frames and disabled collision; no script selects it.
/// Gary maps this selector/ID to loaded local set 3;
/// interpolation uses his fixed eight-frame duration.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryUnusedBaseClip3ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

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

/// Blends Gary Douglas to base-bank clip 6 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Blending uses eight frames regardless of blendFrames; the collision word is unread.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip6BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Gary Douglas base-bank clip 7 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Reset ignores blendFrames; the receiver does not read the collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip7ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Gary Douglas base-bank clip 8 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Reset ignores blendFrames; the receiver does not read the collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip8ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

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

enum { DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_10  = 10,
       DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_4 = 4,
       DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_5 = 5,
       DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_6 = 6 };

/// Blends Gary Douglas to base-bank clip 10 during the arrival conversation.
///
/// The actor borrows this request synchronously and blends over eight frames,
/// ignoring blendFrames and the world-collision word.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBaseClip10BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained, unused Gary bank-0 clip-8 blend request.
///
/// Stores zero blend frames and disabled collision; no script selects it.
/// Gary maps this selector/ID to loaded local set 8;
/// interpolation uses his fixed eight-frame duration.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryUnusedBaseClip8BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BASE_BANK },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BASE_CLIP_8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_0 = 0,
};

/// Retained, unused Gary bank-1 clip-0 reset request.
///
/// Stores zero blend frames and disabled collision; no script selects it.
/// Gary maps this selector/ID to loaded local set 10;
/// interpolation uses his fixed eight-frame duration.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryUnusedBank1Clip0ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

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

/// Resets Gary Douglas bank-1 clip 2 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Reset ignores blendFrames; the receiver does not read the collision word.
/// Bank 1 selects actor animation-set entry 12.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip2ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Gary Douglas to bank-1 clip 3 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Blending uses eight frames regardless of blendFrames; the collision word is unread.
/// Bank 1 selects actor animation-set entry 13.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip3BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Gary Douglas to bank-1 clip 4 in the first story conversation.
///
/// Bank 1 selects animation-set entry 14. The actor borrows this request
/// synchronously and uses eight blend frames; blendFrames and collision are unread.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip4BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Gary Douglas bank-1 clip 5 in the first story conversation.
///
/// Bank 1 selects animation-set entry 15. The actor borrows this request
/// synchronously; reset ignores blendFrames and the collision word is unread.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip5ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Gary Douglas bank-1 clip 6 in the second story conversation.
///
/// Bank 1 selects animation-set entry 16. The actor borrows this request
/// synchronously; reset ignores blendFrames and the collision word is unread.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip6ResetRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends Gary Douglas to bank-1 clip 7 during conversations.
///
/// The actor borrows this request through synchronous dispatch.
/// Blending uses eight frames regardless of blendFrames; the collision word is unread.
/// Bank 1 selects actor animation-set entry 17.
static const AnimationPlayRequest _gDryfieldTrailerCoachGaryBank1Clip7BlendRequest SECTION(".data") = {
    .source               = { .index = DRYFIELD_TRAILER_COACH_GARY_BANK_1 },
    .animationId          = DRYFIELD_TRAILER_COACH_GARY_BANK_1_CLIP_7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

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

/// Resets player extension clip 62 and drops world-grid participation in the wrench conversation.
///
/// The interpreter copies this borrowed template before resolving the equipped
/// bank. The clip uses the room's animation extension; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip62ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_62,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets player extension clip 63 and drops world-grid participation in the wrench conversation.
///
/// The interpreter copies this borrowed template before resolving the equipped
/// bank. The clip uses the room's animation extension; reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldTrailerCoachPlayerClip63ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_TRAILER_COACH_PLAYER_CLIP_63,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Seventeen player extension slots for clip ids 47..63, borrowed from this room and Gary's actor package.
///
/// Slots for ids 55..57 are NULL. The copy request transfers 32 words: it reads
/// 15 words past this table through the following copy request, three labels,
/// their alignment bytes and the first topic-choice pointer. This is retained
/// original behavior; those extra words are not playable animation sets.
/// Both overlays must remain loaded while the installed clip pointers are used.
static AnimationSet* const _gDryfieldTrailerCoachPlayerAnimationExtensionSets[17] SECTION(".data") = { &gDryfieldTrailerCoachPlayerEvent47Clip, &gDryfieldTrailerCoachPlayerEvent48Clip, &gDryfieldTrailerCoachPlayerEvent49Clip, &gDryfieldTrailerCoachPlayerEvent50Clip, &gDryfieldTrailerCoachPlayerConversationClip, &gDryfieldTrailerCoachPlayerEvent52Clip, &gDryfieldTrailerCoachPlayerEvent53Clip, &gDryfieldTrailerCoachPlayerEvent54Clip, NULL, NULL, NULL, &gDryfieldTrailerCoachPlayerEvent58Clip, &gDryfieldTrailerCoachPlayerEvent59Clip, &gDryfieldTrailerCoachPlayerEvent60Clip, &gDryfieldTrailerCoachPlayerEvent61Clip, &_gDryfieldTrailerCoachAnimation07994, &_gDryfieldTrailerCoachAnimation07668 };

/// Copies 32 source words into the equipped player's extension slots starting at 47.
///
/// The count is the destination capacity, not the seventeen playable source slots.
/// The request is borrowed synchronously; the copied animation pointers remain
/// borrowed for playback. See `_gDryfieldTrailerCoachPlayerAnimationExtensionSets`
/// for the retained read across adjacent objects.
static const AnimationBankCopyRequest _gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest SECTION(".data") = {
    .sets  = _gDryfieldTrailerCoachPlayerAnimationExtensionSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Null-terminated Shift-JIS topic label "銃器について (About firearms)".
static u8 _gDryfieldTrailerCoachFirearmsTopicLabel[] = "\x8F\x65\x8A\xED\x82\xC9\x82\xC2\x82\xA2\x82\xC4";

/// Null-terminated Shift-JIS topic label "シェルターについて (About the shelter)".
static u8 _gDryfieldTrailerCoachShelterTopicLabel[] = "\x83\x56\x83\x46\x83\x8B\x83\x5E\x81\x5B\x82\xC9\x82\xC2\x82\xA2\x82\xC4";

/// Null-terminated Shift-JIS topic label "他に何か？ (Anything else?)".
static u8 _gDryfieldTrailerCoachAnythingElseTopicLabel[] = "\x91\xBC\x82\xC9\x89\xBD\x82\xA9\x81\x48";

/// Borrowed labels for the firearms/shelter and firearms/anything-else choices.
///
/// The two contiguous pairs occupy entries 0..1 and 2..3. The option dialog
/// borrows the selected labels until answered; keep this overlay loaded.
static u8* const _gDryfieldTrailerCoachTopicChoiceLabels[4] SECTION(".data") = { _gDryfieldTrailerCoachFirearmsTopicLabel, _gDryfieldTrailerCoachShelterTopicLabel, _gDryfieldTrailerCoachFirearmsTopicLabel, _gDryfieldTrailerCoachAnythingElseTopicLabel };

/// CAP slots and view selected by this room's event streams.
enum {
    DRYFIELD_TRAILER_COACH_CAP_ARRIVAL                    = 1,
    DRYFIELD_TRAILER_COACH_CAP_CONVERSATION               = 3,
    DRYFIELD_TRAILER_COACH_CAP_SHOP                       = 4,
    DRYFIELD_TRAILER_COACH_CAP_FIRST_STORY_CONVERSATION   = 5,
    DRYFIELD_TRAILER_COACH_CAP_SECOND_STORY_CONVERSATION  = 6,
    DRYFIELD_TRAILER_COACH_CAP_REPEAT_CONVERSATION        = 7,
    DRYFIELD_TRAILER_COACH_CAP_FIRST_WRENCH_CONVERSATION  = 27,
    DRYFIELD_TRAILER_COACH_CAP_REPEAT_WRENCH_CONVERSATION = 28,
    DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW              = 3,
    DRYFIELD_TRAILER_COACH_SHOP_STOCK_SELECTOR            = 0x20,
};

/// Presents the skippable story arrival used by saved arrival 2.
///
/// Starts CAP slot 1, synchronizes player and Gary clips with its cues, then
/// restores view 3, weapons, ambient lighting and actor control. Extension
/// animation banks must be installed before the clips play.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachArrivalScript[58] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachCompanionClip9ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachCompanionClip9ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip58ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip59ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip6BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip7ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip8ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip10BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip60ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryTurnHeadToPlayerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip61ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryReleaseHeadTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryAutoHeadTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restores gameplay presentation when the story arrival is skipped.
///
/// Under a primary fade, restores player placement/idle, Gary idle, weapons,
/// ambient lighting and companion control; cleans up the scene, selects
/// view 3, releases player control and starts area music.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachArrivalSkipScript[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Prepares Gary dialogue and waits for its topic or shop choice.
///
/// Starts CAP slot 3, suppresses the HUD and steps Gary through base clips
/// 6, 7 and 8. After the CAP cue, starts progress-selected dialogue with
/// the no-argument caption callback and waits for the answer.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachConversationPrepareScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip6BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip7ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip8ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldTrailerCoachStartConversationCaption }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Opens the daytime Dryfield shop after the conversation choice.
///
/// Starts CAP slot 4 and invokes shopOpenSession with stock set 0x20 and
/// initial category 0 (weapons). Restores Gary idle and releases player
/// scripted control after the shop request; the callback result is discarded.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachShopConversationScript[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_SHOP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = DRYFIELD_TRAILER_COACH_SHOP_STOCK_SELECTOR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Presents the first story conversation with Gary.
///
/// Starts CAP slot 5 and synchronizes player, companion and Gary animation
/// with its cues. Restores view 3, actor control, weapons, placement and
/// ambient lighting. Progress and saved-area updates are committed by the caller.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachFirstStoryConversationScript[98] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_FIRST_STORY_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachCompanionClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 75 }, { .value = 75 }, { .value = 75 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip49ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip47ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 0x7) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip52ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip3BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip49ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip48ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip4BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip7BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip49ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip51ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip7BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 0xd) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip50ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerClip54Transform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip54ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 89 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restores gameplay presentation when the first story conversation is skipped.
///
/// Under a primary fade, restores player placement/idle, Gary idle, weapons,
/// ambient lighting and companion control; cleans up the scene, selects
/// view 3, releases player control and starts area music.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachFirstStoryConversationSkipScript[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAfterConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Presents the second story conversation with Gary.
///
/// Starts CAP slot 6, turns Gary toward the room focus and resets bank-1
/// clip 6 at the CAP cue. Restores automatic head turning, Gary idle, view 3,
/// ambient lighting and both actors' control. The caller commits progress 3.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachSecondStoryConversationScript[25] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_SECOND_STORY_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachCompanionClip9ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryTurnHeadToRoomFocusCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 0xe) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip6ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryAutoHeadTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip4ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restores gameplay presentation when the second story conversation is skipped.
///
/// Under a primary fade, restores Gary/player idle, weapons, ambient lighting
/// and companion control; cleans up the scene, selects view 3 and releases
/// player control. This skip stream makes no area-music start request.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachSecondStoryConversationSkipScript[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip5ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_TRAILER_COACH_CONVERSATION_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Presents Gary's ordinary conversation after both story conversations.
///
/// Starts CAP slot 7, plays the Gary bank-1 greeting and return-to-idle
/// sequence, waits for its CAP cue and releases player scripted control.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachRepeatConversationScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_REPEAT_CONVERSATION }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Presents the first conversation gated by the uncollected Monkey Wrench.
///
/// Starts CAP slot 27, coordinates Gary clips with player extension clips
/// 63 and 62, then releases player scripted control. The caller sets the
/// one-time dialogue flag before requesting playback.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachFirstWrenchConversationScript[35] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_FIRST_WRENCH_CONVERSATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &_gDryfieldTrailerCoachPlayerAnimationExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip1ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip63ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip7BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip1ResetRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBank1Clip2BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldTrailerCoachPlayerClip62ResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldTrailerCoachGaryBaseClip1ResetRequest } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Presents the repeated conversation gated by the uncollected Monkey Wrench.
///
/// Starts CAP slot 28, plays Gary's greeting and return-to-idle sequence,
/// waits for the CAP cue and releases player scripted control.
///
/// Ends with EVENT_SCRIPT_OPCODE_END. The interpreter borrows the stream and
/// referenced room/actor resources through playback; keep them loaded and
/// unchanged. Frames are interpreter updates. Its API takes a mutable stream.
static EvsCommand _gDryfieldTrailerCoachRepeatWrenchConversationScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_TRAILER_COACH_CAP_REPEAT_WRENCH_CONVERSATION }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
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

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldTrailerCoachRoomCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..57 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_TRAILER_COACH_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldTrailerCoachRoomCollisionGridCells[(halfwordOffset)])
static s16* _gDryfieldTrailerCoachRoomCollisionGridTable[3] = {
#include "assets/dryfield_trailer_coach_collision_0A0F4_table.inc"
};
#undef DRYFIELD_TRAILER_COACH_CELL_FACE_LIST_AT_OFFSET

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

enum {
    DRYFIELD_TRAILER_COACH_GARY_RESOURCE_ENTRY  = 106,
    DRYFIELD_TRAILER_COACH_GARY_FILE_NUMBER     = 207,
    DRYFIELD_TRAILER_COACH_GARY_BODY_DESCRIPTOR = 0,
};

/// Gary Douglas's loading and spawn recipe for the default area variant.
///
/// Placement entry 106 selects global-library file 420700 and descriptor 0.
/// One resource is followed by `AREA_PLACEMENT_END`. The loader and spawner borrow
/// this table; keep the room and actor descriptors loaded through their use.
static AreaResource _gDryfieldTrailerCoachGaryResources[2] = {
    {
        .entryId        = DRYFIELD_TRAILER_COACH_GARY_RESOURCE_ENTRY,
        .fileNumber     = DRYFIELD_TRAILER_COACH_GARY_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = DRYFIELD_TRAILER_COACH_GARY_BODY_DESCRIPTOR,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor420700GaryDouglasTaskDescriptors,
    },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldTrailerCoachAreaVariants[DRYFIELD_TRAILER_COACH_AREA_VARIANT_COUNT] = {
    { NULL, NULL },
    { gMapDryfieldTrailerCoachGaryPlacements, _gDryfieldTrailerCoachGaryResources },
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

enum { DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET = 1 };

/// Saved Dryfield placement and map-mark updates committed by the first story conversation.
///
/// Fourteen updates precede AREA_APPLY_END. Policy selects normal/Scavenger or
/// Bounty/Nightmare layouts where needed; accepted entries also remove saved
/// enemy poses. Applied synchronously when playback is requested, even if skipped.
static const AreaApplyRec _gDryfieldTrailerCoachFirstConversationAreaUpdates[15] SECTION(".data") = {
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_2, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_2, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_G_R_KITCHEN, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 2, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_TRAILER_COACH_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 2, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_JUNK_YARD, 2, AREA_APPLY_MODE_ALWAYS },
    { .stage = AREA_APPLY_END },
};

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
static s32 _gShopRefillDisplayQuantityFixed = 0;

/// Borrowed weapon-supply catalog row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalog must remain loaded throughout the session.
static const EquipmentWeaponSupply* _gShopRefillSupply = NULL;

RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask = { 0 };

/// Persistent presentation record for the repeat trailer-coach story scene.
///
/// The room command configures view 10, CAP slot/file 1 and four area sounds
/// before spawning the cutscene. The runner borrows this record until completion;
/// keep the overlay loaded and do not overwrite it while a runner is live.
/// Texture-page fields retain zero, selecting the runner's default page.
static RoomCutsceneRec _gDryfieldTrailerCoachRepeatStoryScene = { 0 };

/// This room's cutscene sound-task slot. The runner uses `task`.
extern RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask;

static void _dryfieldTrailerCoachExitTopicChoice(Task* task);

static void _dryfieldTrailerCoachInitializeRoomTask(Task* task);

static void _dryfieldTrailerCoachOpenTopicChoice(Task* task);

/// Retained byte following this room's weapon-supply refill title terminator.
///
/// One-byte string-literal binding for `shop.inc.c`; label drawing does not read it.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\xEF"
#include "../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xD0\xFF"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void dryfieldTrailerCoachTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

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

    evsStartScriptWithSkip(_gDryfieldTrailerCoachFirstStoryConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                           _gDryfieldTrailerCoachFirstStoryConversationSkipScript);
    gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS, DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE);
    gameFlagSetNibble(GAME_FLAG_DRIVEWAY_PROGRESS, DRIVEWAY_FIRST_CONVERSATION_PROGRESS);
    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_FIRST_CONVERSATION_SCHEDULE);
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, CONVERSATION_OBJECTIVE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = CONVERSATION_SCENE_EVENT;
    areaApplySavedUpdates(_gDryfieldTrailerCoachFirstConversationAreaUpdates);
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
            evsStartScript(_gDryfieldTrailerCoachConversationPrepareScript, EVENT_SCRIPT_HUD_KEEP);
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
                evsStartScript(_gDryfieldTrailerCoachShopConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) < DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE) {
                _dryfieldTrailerCoachStartFirstStoryConversation();
            } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_MONKEY_WRENCH) == 0 && gameFlagGetNibble(GAME_FLAG_DRYFIELD_TRAILER_COACH_04F) != 0) {
                if (gameFlagGetNibble(GAME_FLAG_0FD) == 0) {
                    gameFlagSetNibble(GAME_FLAG_0FD, 1);
                    evsStartScript(_gDryfieldTrailerCoachFirstWrenchConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                } else {
                    evsStartScript(_gDryfieldTrailerCoachRepeatWrenchConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                }
            } else if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) == DRYFIELD_TRAILER_COACH_FIRST_CONVERSATION_COMPLETE) {
                evsStartScriptWithSkip(_gDryfieldTrailerCoachSecondStoryConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       _gDryfieldTrailerCoachSecondStoryConversationSkipScript);
                gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS, DRYFIELD_TRAILER_COACH_SECOND_CONVERSATION_COMPLETE);
            } else {
                evsStartScript(_gDryfieldTrailerCoachRepeatConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            task->state++;
            break;
        case CONVERSATION_EXIT:
            taskKill(task);
            break;
    }
}

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Refuses every key-item use in this room without consuming the item.
///
/// Handles the inventory's key-item request; `itemId` is a collected-item ID
/// and `unusedSecondArg` is normally zero. All arguments are ignored. The zero
/// result selects the inventory's "No use now" notice.
static s32 _dryfieldTrailerCoachRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
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
    _gDryfieldTrailerCoachRepeatStoryScene.view            = REPEAT_SCENE_VIEW;
    _gDryfieldTrailerCoachRepeatStoryScene.capSlot         = REPEAT_SCENE_CAP_SLOT;
    _gDryfieldTrailerCoachRepeatStoryScene.capFile         = REPEAT_SCENE_CAP_FILE;
    _gDryfieldTrailerCoachRepeatStoryScene.skipScene       = false;
    _gDryfieldTrailerCoachRepeatStoryScene.startSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 3);
    _gDryfieldTrailerCoachRepeatStoryScene.endSound        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 5);
    _gDryfieldTrailerCoachRepeatStoryScene.sceneSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 4);
    _gDryfieldTrailerCoachRepeatStoryScene.afterSceneSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 6);
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, REPEAT_SCENE_TASK, REPEAT_SCENE_FOLLOW_UP_CAP, &_gDryfieldTrailerCoachRepeatStoryScene);
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
        taskSpawnFromTable(_gDryfieldTrailerCoachInteractionTaskDescs, TASK_CONVERSATION, 0, 0);
    }
    if (commandIndex == COMMAND_STORY_SCENE) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == ARRIVAL_STORY_SCENE) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = ARRIVAL_DEFAULT;
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

/// Room receiver states: initialize, update view-dependent depth scale, then kill.
///
/// Indexed by Task::state in 0..2 without a bounds check. Initialization enters
/// state 1, whose depth update does not advance the task state.
/// The copied callback table and its code remain loaded with this room.
static const TaskFuncTable3 _gDryfieldTrailerCoachRoomStates = {
    {
        _dryfieldTrailerCoachInitializeRoomTask,
        _trailerCoachSetDepthShift,
        taskKill,
    },
};

/// Topic-choice states: open the dialog, publish its answer, then release it and exit.
///
/// The dispatcher indexes these three callbacks by `Task::state` in 0..2.
/// Each callback borrows the live task; the table stays loaded with this room.
static const TaskFuncTable3 _gDryfieldTrailerCoachTopicChoiceStates = {
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

    stateHandlers = _gDryfieldTrailerCoachTopicChoiceStates;
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

    task->msgTable = _gDryfieldTrailerCoachRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == ARRIVAL_STORY_SCENE) {
        evsStartScriptWithSkip(_gDryfieldTrailerCoachArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldTrailerCoachArrivalSkipScript);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, STORY_DIALOGUE_START);
    } else {
        stageMusicRequestAreaStart(AREA_MUSIC_START);
    }
    task->state = task->state + 1;
}

#include "../../shared/trailer_coach_set_depth_shift.inc.c"

void dryfieldTrailerCoachRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldTrailerCoachRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_star_local.inc.c"

/// Computes the ray star's signed half intensity before its first disc packet.
///
/// Bind 0 or 1 before the fragment; undefined means 0. Disc colours receive
/// the early half intensity, while rays halve the saved full intensity. This
/// mode takes precedence over unsigned ray intensity and is cleared afterwards.
#define GLOW_DRAW_RAY_STAR_EARLY_SIGNED_HALF_INTENSITY 1
/// Sets the outer ray-star disc's centre to half-intensity cyan.
///
/// Writes only vertex 2's RGB bytes of a writable `POLY_G4*`. Green and blue
/// use the supplied `halfIntensity`, already halved and normally 0..120;
/// stores retain its low byte. Arguments must have no side effects:
/// `primitive` is evaluated three times and `halfIntensity` twice. Use as a
/// statement. Requires `setRGB2` from `psyq/libgpu.h`. Define before including
/// `glow_draw_ray_star.inc.c`, which undefines this binding afterwards.
#define GLOW_DRAW_RAY_STAR_OUTER(primitive, halfIntensity) (setRGB2((primitive), 0, (halfIntensity), (halfIntensity)))
/// Sets the inner ray-star disc's centre to green with half-intensity blue.
///
/// Writes only vertex 2's RGB bytes of a writable `POLY_G4*`. Green uses
/// `intensity`, normally 0..240; blue uses the supplied `halfIntensity`,
/// normally 0..120. Stores retain each value's low byte.
/// Arguments must have no side effects: `primitive` is evaluated three
/// times and each intensity once. Use as a statement. Requires `setRGB2`
/// from `psyq/libgpu.h`. Define before including `glow_draw_ray_star.inc.c`,
/// which undefines this binding afterwards.
#define GLOW_DRAW_RAY_STAR_INNER(primitive, intensity, halfIntensity) (setRGB2((primitive), 0, (intensity), (halfIntensity)))
/// Sets the ray-star rays' centres to half-intensity cyan.
///
/// Writes only vertex 2's RGB bytes of a writable `POLY_G4*`. Green and blue
/// use the supplied `halfIntensity`, already halved and normally 0..120; red
/// is zero. Stores retain the intensity's low byte. Arguments must have no
/// side effects: `primitive` is evaluated three times and `halfIntensity`
/// twice. Use as a statement. Requires `setRGB2` from `psyq/libgpu.h`.
/// Define before including `glow_draw_ray_star.inc.c`, which undefines this
/// binding afterwards.
#define GLOW_DRAW_RAY_STAR_RAY(primitive, halfIntensity) (setRGB2((primitive), 0, (halfIntensity), (halfIntensity)))
#include "../../shared/effects/glow_draw_ray_star.inc.c"

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

    viewMask = 1 << gGameSession->location.view;
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
