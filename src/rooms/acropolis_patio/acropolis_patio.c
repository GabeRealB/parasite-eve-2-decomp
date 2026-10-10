#include "rooms/acropolis_patio.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern TaskMessageEntry D_acropolis_patio_8018028C[6];
extern s32              D_acropolis_patio_80180440;
/// Retained bodyless controller for the opening scene's player head aim.
///
/// Spawned on room-1 initialization while story progress is below 2. Script
/// callbacks require this task to be live and set its phase through this handle.
/// Session rebuild discards the task list and heap; this pointer is not cleared
/// and must not be used again until a new qualifying initialization replaces it.
static Task*      _gAcropolisPatioPlayerHeadAimTask;
extern EvsCommand D_acropolis_patio_80180DEC[];
extern EvsCommand D_acropolis_patio_80180EDC[];
/// Cafeteria arrival ID saved after the door-choice task is spawned.
///
/// This singleton byte is borrowed until the accepted choice commits it to the
/// live save for reload; a later door request overwrites it.
static u8 _gAcropolisPatioCafeteriaArrivalWarp;

/// Cafeteria room selector saved until the accepted door choice requests reload.
///
/// Stores the request's signed room byte as the unsigned saved-location selector,
/// preserving all eight bits. A later door request overwrites this singleton.
static u8         _gAcropolisPatioCafeteriaArrivalRoom;
extern EvsCommand D_acropolis_patio_80180484[];
extern EvsCommand D_acropolis_patio_801806AC[];
extern EvsCommand D_acropolis_patio_8018082C[];
extern EvsCommand D_acropolis_patio_80180C64[];
extern EvsCommand D_acropolis_patio_8018280C[];
extern EvsCommand D_acropolis_patio_80182BE4[];

/// Story thresholds, opening phases and one-shot latches used by the patio receiver.
enum {
    ACROPOLIS_PATIO_STORY_BATTLE_FINISHED    = 2,
    ACROPOLIS_PATIO_STORY_FOLLOW_UP_STARTED  = 3,
    ACROPOLIS_PATIO_STORY_FOUNTAIN_READY     = 5,
    ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD  = 2,
    ACROPOLIS_PATIO_OPENING_SCENE_STARTED    = 3,
    ACROPOLIS_PATIO_OPENING_SCENE_FINISHED   = 4,
    ACROPOLIS_PATIO_SCENE_UNSEEN             = 0,
    ACROPOLIS_PATIO_SCENE_SEEN               = 1,
    ACROPOLIS_PATIO_SECOND_SECURITY_LOCK_BIT = 2,
    ACROPOLIS_PATIO_DOOR_UNLOCKED            = 2,
    ACROPOLIS_PATIO_DOOR_ATTEMPTED           = 1
};

static void _acropolisPatioInitializeRoomTask(Task* task);
static void _acropolisPatioRoomIdleState(Task* task);

/// Key-item use message handled by the patio's room task.
enum { ACROPOLIS_PATIO_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Packed fountain-jet argument fields and the fixed sprite-sheet geometry.
enum {
    ACROPOLIS_PATIO_JET_ANCHOR_MASK   = 0xF,
    ACROPOLIS_PATIO_JET_CELL_SHIFT    = 8,
    ACROPOLIS_PATIO_JET_CELL_MASK     = 3,
    ACROPOLIS_PATIO_JET_SIZE_SHIFT    = 16,
    ACROPOLIS_PATIO_JET_SIZE_MASK     = 0xFFF,
    ACROPOLIS_PATIO_JET_DEFAULT_SIZE  = 640,
    ACROPOLIS_PATIO_JET_MIN_DEPTH     = 0x11,
    ACROPOLIS_PATIO_JET_TEXTURE_PAGE  = 0x2B,
    ACROPOLIS_PATIO_JET_CLUT_Y        = 0x10E,
    ACROPOLIS_PATIO_JET_CELL_SIZE     = 40,
    ACROPOLIS_PATIO_JET_FLICKER_SHIFT = 4
};

/// State table of the room's three-state task dispatcher
/// (`acropolisPatioRoomTask`): the entry tick, an idle state, then
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_patio_8017D5C4 = {
    { _acropolisPatioInitializeRoomTask, _acropolisPatioRoomIdleState, taskKill },
};

static WorldCollisionGrid     _gAcropolisPatioCollisionGrid;
static WorldCollisionOccluder _gAcropolisPatioOccluders[2];
static WorldCollisionTrigger  _gAcropolisPatioViewBoundaryTriggers[14];
static WorldCollisionTrigger  _gAcropolisPatioRoom1ActionTriggers[12];
static WorldCollisionTrigger  _gAcropolisPatioRooms2And3ActionTriggers[12];
static WorldCoordRoomLights   _gAcropolisPatioRoomLights;

static void _acropolisPatioSetBattleActive(s32 battleActive);

extern AnimationPlayRequest D_acropolis_patio_8018270C;
extern AnimationPlayRequest D_acropolis_patio_80182720;
static void                 _acropolisPatioSpawnPlayerTurnLeft(void);
static void                 _acropolisPatioPlayerTurnLeftTask(Task* task);

static void _acropolisPatioSetPlayerHeadAimState(s32 headAimState);
static void _acropolisPatioActivateRoom2(void);
static void _acropolisPatioSetActorControl(u8 actorControl);

static s32  _acropolisPatioResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisPatioHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _acropolisPatioRefuseKeyItemUse(Task* roomTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisPatioHandleSoundMessage(Task* roomTask, s32 messageId, s32 soundCue, s32 unusedArg);
static void _acropolisPatioCafeteriaDoorSceneTask(Task* task);
static s32  _acropolisPatioHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg);
static void _acropolisPatioRecordSceneChoiceTask(Task* task);
static void _acropolisPatioPlayerHeadAimTask(Task* task);

/// States selected by the opening scene's player-head-aim callbacks.
enum {
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE        = 1,
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_HOLD        = 2,
    ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_START_SLIDE = 3,
};

static AnimationPackedPose _gAcropolisPatioAnimation018A0Bank1[2] = {
#include "assets/acropolis_patio_animation_018A0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation018A0Bank4[20] = {
#include "assets/acropolis_patio_animation_018A0_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation018A0Records[57] = {
#include "assets/acropolis_patio_animation_018A0_records.inc"
};

static u16 _gAcropolisPatioAnimation018A0Indices[20] = {
#include "assets/acropolis_patio_animation_018A0_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation018A0 = {
    _gAcropolisPatioAnimation018A0Records,
    _gAcropolisPatioAnimation018A0Indices,
    { NULL, _gAcropolisPatioAnimation018A0Bank1, NULL, NULL, _gAcropolisPatioAnimation018A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation01B48Bank1[2] = {
#include "assets/acropolis_patio_animation_01B48_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation01B48Bank4[41] = {
#include "assets/acropolis_patio_animation_01B48_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation01B48Records[103] = {
#include "assets/acropolis_patio_animation_01B48_records.inc"
};

static u16 _gAcropolisPatioAnimation01B48Indices[20] = {
#include "assets/acropolis_patio_animation_01B48_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation01B48 = {
    _gAcropolisPatioAnimation01B48Records,
    _gAcropolisPatioAnimation01B48Indices,
    { NULL, _gAcropolisPatioAnimation01B48Bank1, NULL, NULL, _gAcropolisPatioAnimation01B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation01CE4Bank1[2] = {
#include "assets/acropolis_patio_animation_01CE4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation01CE4Bank4[20] = {
#include "assets/acropolis_patio_animation_01CE4_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation01CE4Records[57] = {
#include "assets/acropolis_patio_animation_01CE4_records.inc"
};

static u16 _gAcropolisPatioAnimation01CE4Indices[20] = {
#include "assets/acropolis_patio_animation_01CE4_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation01CE4 = {
    _gAcropolisPatioAnimation01CE4Records,
    _gAcropolisPatioAnimation01CE4Indices,
    { NULL, _gAcropolisPatioAnimation01CE4Bank1, NULL, NULL, _gAcropolisPatioAnimation01CE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation022B8Bank1[12] = {
#include "assets/acropolis_patio_animation_022B8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation022B8Bank4[138] = {
#include "assets/acropolis_patio_animation_022B8_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation022B8Records[179] = {
#include "assets/acropolis_patio_animation_022B8_records.inc"
};

static u16 _gAcropolisPatioAnimation022B8Indices[20] = {
#include "assets/acropolis_patio_animation_022B8_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation022B8 = {
    _gAcropolisPatioAnimation022B8Records,
    _gAcropolisPatioAnimation022B8Indices,
    { NULL, _gAcropolisPatioAnimation022B8Bank1, NULL, NULL, _gAcropolisPatioAnimation022B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation02AD0Bank1[19] = {
#include "assets/acropolis_patio_animation_02AD0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation02AD0Bank4[189] = {
#include "assets/acropolis_patio_animation_02AD0_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation02AD0Records[252] = {
#include "assets/acropolis_patio_animation_02AD0_records.inc"
};

static u16 _gAcropolisPatioAnimation02AD0Indices[20] = {
#include "assets/acropolis_patio_animation_02AD0_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation02AD0 = {
    _gAcropolisPatioAnimation02AD0Records,
    _gAcropolisPatioAnimation02AD0Indices,
    { NULL, _gAcropolisPatioAnimation02AD0Bank1, NULL, NULL, _gAcropolisPatioAnimation02AD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation02CA4Bank1[2] = {
#include "assets/acropolis_patio_animation_02CA4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation02CA4Bank4[15] = {
#include "assets/acropolis_patio_animation_02CA4_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation02CA4Records[76] = {
#include "assets/acropolis_patio_animation_02CA4_records.inc"
};

static u16 _gAcropolisPatioAnimation02CA4Indices[20] = {
#include "assets/acropolis_patio_animation_02CA4_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation02CA4 = {
    _gAcropolisPatioAnimation02CA4Records,
    _gAcropolisPatioAnimation02CA4Indices,
    { NULL, _gAcropolisPatioAnimation02CA4Bank1, NULL, NULL, _gAcropolisPatioAnimation02CA4Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_acropolis_patio_8018028C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisPatioResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _acropolisPatioHandleCommandMessage },
    { ACROPOLIS_PATIO_MESSAGE_USE_KEY_ITEM, _acropolisPatioRefuseKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisPatioHandleSoundMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisPatioHandleActionMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Scheduler priorities, clip selectors and blend durations for patio scene records.
enum {
    ACROPOLIS_PATIO_SCENE_TASK_PRIORITY             = 192,
    ACROPOLIS_PATIO_HEAD_AIM_TASK_PRIORITY          = 97,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_47   = ANIMATION_BANK_BASE_SET_COUNT,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_49   = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_50   = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_51   = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_52   = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACROPOLIS_PATIO_FOLLOW_UP_BLEND_FRAMES          = 5,
    ACROPOLIS_PATIO_SWAT_ANIMATION_1                = 1,
    ACROPOLIS_PATIO_SWAT_ANIMATION_2                = 2,
    ACROPOLIS_PATIO_SWAT_ANIMATION_3                = 3,
    ACROPOLIS_PATIO_SWAT_ANIMATION_4                = 4,
    ACROPOLIS_PATIO_SECOND_VISIT_PLAYER_ANIMATION_9 = 9,
    ACROPOLIS_PATIO_SECOND_VISIT_BLEND_FRAMES       = 6,
};

/// Bodyless scene controllers: CAP-choice recording, cafeteria door, then head aim.
///
/// Indices 0..2 select callbacks; the fourth record terminates the table and must
/// never be spawned. The callback code must stay loaded for each task's lifetime.
static TaskDesc _gAcropolisPatioSceneTaskDescriptors[] = {
    { { { TASK_BODY_NONE, ACROPOLIS_PATIO_SCENE_TASK_PRIORITY } }, _acropolisPatioRecordSceneChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_PATIO_SCENE_TASK_PRIORITY } }, _acropolisPatioCafeteriaDoorSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_PATIO_HEAD_AIM_TASK_PRIORITY } }, _acropolisPatioPlayerHeadAimTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Player's initial room-space placement for the opening follow-up scene.
static ActorTransform _gAcropolisPatioFollowUpPlayerStartPlacement = { .pos = { -5255, 1, -124, 0 }, .rot = { 0, -800, 0, 0 } };

/// Destination of the player's scripted walk after the follow-up scene starts.
///
/// Only XYZ is consumed, in room-coordinate units; rotation is unused.
static ActorTransform _gAcropolisPatioFollowUpPlayerWalkDestination = { .pos = { -7195, 1, 484, 0 }, .rot = { 0, -800, 0, 0 } };

/// Player placement beside the SWAT member for the follow-up animation sequence.
static ActorTransform _gAcropolisPatioFollowUpPlayerAnimationPlacement = { .pos = { -6852, 1, 345, 0 }, .rot = { 0, -800, 0, 0 } };

/// Player placement restored by both completion and skipping of the follow-up scene.
static ActorTransform _gAcropolisPatioFollowUpPlayerEndPlacement = { .pos = { -6612, 1, 315, 0 }, .rot = { 0, -800, 0, 0 } };

/// Player placement restored by both completion and skipping of the opening scene.
static ActorTransform _gAcropolisPatioOpeningPlayerEndPlacement = { .pos = { 1452, 1, 593, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Six follow-up player clips copied to equipped-weapon bank slots 47..52.
///
/// The directory is copied as pointer words; sets and tracks stay borrowed from
/// the patio overlay through playback.
static AnimationSet* _gAcropolisPatioFollowUpPlayerAnimationSets[] = {
    &_gAcropolisPatioAnimation018A0,
    &_gAcropolisPatioAnimation01CE4,
    &_gAcropolisPatioAnimation01B48,
    &_gAcropolisPatioAnimation02CA4,
    &_gAcropolisPatioAnimation022B8,
    &_gAcropolisPatioAnimation02AD0,
};

/// Blended follow-up player clip 47 after clip 51's timed lead-in.
///
/// Event dispatch replaces `source.index` with the equipped weapon's bank.
/// The patio extension remains borrowed through playback; grid participation is disabled.
static AnimationPlayRequest _gAcropolisPatioFollowUpPlayerAnimation47Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_47,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_PATIO_FOLLOW_UP_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_acropolis_patio_80180390 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blended follow-up player clip 49, six script ticks after clip 47 starts.
///
/// Event dispatch replaces `source.index` with the equipped weapon's bank.
/// The patio extension remains borrowed through playback; grid participation is disabled.
static AnimationPlayRequest _gAcropolisPatioFollowUpPlayerAnimation49Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_49,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_PATIO_FOLLOW_UP_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blended follow-up player clip 50, selected by the CAP cue after clip 49.
///
/// Event dispatch replaces `source.index` with the equipped weapon's bank.
/// The patio extension remains borrowed through playback; grid participation is disabled.
static AnimationPlayRequest _gAcropolisPatioFollowUpPlayerAnimation50Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_50,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_PATIO_FOLLOW_UP_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// First clip of the follow-up player's animation sequence, bank slot 51.
///
/// Event dispatch replaces `source.index` with the equipped weapon's bank.
/// The patio extension remains borrowed through playback; grid participation is disabled.
static AnimationPlayRequest _gAcropolisPatioFollowUpPlayerAnimation51Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_PATIO_FOLLOW_UP_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Final follow-up player clip, reset after room 2 and the weapon are restored.
///
/// Event dispatch replaces `source.index` with the equipped weapon's bank.
/// The patio extension remains borrowed through playback; grid participation is disabled.
static AnimationPlayRequest _gAcropolisPatioFollowUpPlayerAnimation52Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_FOLLOW_UP_PLAYER_ANIMATION_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the follow-up scene's player clips in the equipped-weapon bank extension.
///
/// Copies all 6 directory words synchronously without starting playback.
/// Copied set pointers and tracks remain borrowed from this overlay.
static AnimationBankCopyRequest _gAcropolisPatioFollowUpPlayerBankCopyRequest = {
    .source    = { .sets = _gAcropolisPatioFollowUpPlayerAnimationSets },
    .wordCount = ARRAY_SIZE(_gAcropolisPatioFollowUpPlayerAnimationSets),
};

/// Equipped-weapon idle clip selected when the patio scripts take player control.
enum { ACROPOLIS_PATIO_PLAYER_IDLE_ANIMATION = 1 };

/// Resets the player to the equipped weapon's idle clip without grid participation.
///
/// Event dispatch copies this request and overwrites `source.index` with the
/// equipped weapon's bank; the stored index is unused. Reset ignores blendFrames.
/// The request lives with the patio overlay and is consumed synchronously.
static AnimationPlayRequest _gAcropolisPatioPlayerIdleAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_PLAYER_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// SWAT member's room-space placement for the opening and follow-up scenes (actor 0).
static ActorTransform _gAcropolisPatioOpeningSwatPlacement = { .pos = { -7960, 0, 320, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// SWAT member's room-space placement restored after scenes and on entry (actor 0).
///
/// Borrowed synchronously by placement messages while the patio overlay is loaded.
static ActorTransform _gAcropolisPatioRestoredSwatPlacement = {
    .pos = { -7720, 0, 290, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

s32 D_acropolis_patio_80180440 = 769;

/// Starts SWAT clip 1 and selects the Grinning Stranger's scripted dormancy.
///
/// Borrowed synchronously in the Acropolis patio command namespace.
static ActorCommand _gAcropolisPatioOpeningActorsStartCommand = {
    .context = { .loc = { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO } },
    .command = ACROPOLIS_PATIO_SWAT_ANIMATION_1,
};

/// Selects SWAT animation 2 during the follow-up scene (placed actor 0).
///
/// Borrowed synchronously in the Acropolis patio command namespace.
static ActorCommand _gAcropolisPatioSwatAnimation2Command = {
    .context = { .loc = { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO } },
    .command = ACROPOLIS_PATIO_SWAT_ANIMATION_2,
};

/// Selects SWAT animation 3 on room entry and in the follow-up scene (actor 0).
///
/// Borrowed synchronously in the Acropolis patio command namespace.
static ActorCommand _gAcropolisPatioSwatAnimation3Command __attribute__((aligned(4))) = {
    .context = { .loc = { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO } },
    .command = ACROPOLIS_PATIO_SWAT_ANIMATION_3,
};

/// Selects SWAT animation 4 after the opening scene and at follow-up start (actor 0).
///
/// Borrowed synchronously in the Acropolis patio command namespace.
static ActorCommand _gAcropolisPatioSwatAnimation4Command = {
    .context = { .loc = { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO } },
    .command = ACROPOLIS_PATIO_SWAT_ANIMATION_4,
};

/// Grinning Stranger's room-space opening placement (variant 1, actor 1).
static ActorTransform _gAcropolisPatioOpeningStrangerPlacement = { .pos = { -7960, 0, 320, 0 }, .rot = { 0, 0, 0, 0 } };

/// Grinning Stranger's placement after the opening scene and on room entry (actor 1).
static ActorTransform _gAcropolisPatioRestoredStrangerPlacement = { .pos = { -7720, 0, 290, 0 }, .rot = { 0, 0, 0, 0 } };

EvsCommand D_acropolis_patio_80180484[23] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51030009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 67 }, { .value = 67 }, { .value = 78 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioOpeningActorsStartCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioOpeningStrangerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioOpeningSwatPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioOpeningActorsStartCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioRestoredSwatPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioSwatAnimation4Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioOpeningPlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioRestoredStrangerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_801806AC[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioRestoredSwatPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioSwatAnimation4Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioOpeningPlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioRestoredStrangerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_8018082C[45] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisPatioFollowUpPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_HOLD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioOpeningSwatPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioSwatAnimation4Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioFollowUpPlayerStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gAcropolisPatioFollowUpPlayerWalkDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5103000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioSwatAnimation2Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioFollowUpPlayerAnimationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioFollowUpPlayerAnimation51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioFollowUpPlayerAnimation47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioFollowUpPlayerAnimation49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioFollowUpPlayerAnimation50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5103000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisPatioSwatAnimation3Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5103000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_START_SLIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisPatioActivateRoom2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioFollowUpPlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioRestoredSwatPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioFollowUpPlayerAnimation52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_80180C64[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioFollowUpPlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisPatioActivateRoom2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetPlayerHeadAimState }, { .value = ACROPOLIS_PATIO_SCRIPT_HEAD_AIM_IDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Scene/audio key for the first cafeteria-door attempt after the patio battle.
///
/// Selects current-folder stream 29, qualifier 11 (second qualifier zero).
/// CAP playback borrows this record while the patio overlay remains loaded.
static EvsSceneKey _gAcropolisPatioCafeteriaDoorSceneKey = { .group = 1, .streamId = 29, .subId = 11 };

EvsCommand D_acropolis_patio_80180DEC[10] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gAcropolisPatioCafeteriaDoorSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _acropolisPatioSetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _acropolisPatioSetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_80180EDC[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _acropolisPatioSetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gAcropolisPatioAnimation03CD0Bank1[6] = {
#include "assets/acropolis_patio_animation_03CD0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation03CD0Bank4[46] = {
#include "assets/acropolis_patio_animation_03CD0_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation03CD0Records[109] = {
#include "assets/acropolis_patio_animation_03CD0_records.inc"
};

static u16 _gAcropolisPatioAnimation03CD0Indices[20] = {
#include "assets/acropolis_patio_animation_03CD0_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation03CD0 = {
    _gAcropolisPatioAnimation03CD0Records,
    _gAcropolisPatioAnimation03CD0Indices,
    { NULL, _gAcropolisPatioAnimation03CD0Bank1, NULL, NULL, _gAcropolisPatioAnimation03CD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation04470Bank1[13] = {
#include "assets/acropolis_patio_animation_04470_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation04470Bank4[179] = {
#include "assets/acropolis_patio_animation_04470_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation04470Records[250] = {
#include "assets/acropolis_patio_animation_04470_records.inc"
};

static u16 _gAcropolisPatioAnimation04470Indices[20] = {
#include "assets/acropolis_patio_animation_04470_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation04470 = {
    _gAcropolisPatioAnimation04470Records,
    _gAcropolisPatioAnimation04470Indices,
    { NULL, _gAcropolisPatioAnimation04470Bank1, NULL, NULL, _gAcropolisPatioAnimation04470Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation0494CBank1[9] = {
#include "assets/acropolis_patio_animation_0494C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation0494CBank4[109] = {
#include "assets/acropolis_patio_animation_0494C_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation0494CRecords[155] = {
#include "assets/acropolis_patio_animation_0494C_records.inc"
};

static u16 _gAcropolisPatioAnimation0494CIndices[20] = {
#include "assets/acropolis_patio_animation_0494C_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation0494C = {
    _gAcropolisPatioAnimation0494CRecords,
    _gAcropolisPatioAnimation0494CIndices,
    { NULL, _gAcropolisPatioAnimation0494CBank1, NULL, NULL, _gAcropolisPatioAnimation0494CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisPatioAnimation04FC8Bank1[11] = {
#include "assets/acropolis_patio_animation_04FC8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisPatioAnimation04FC8Bank4[161] = {
#include "assets/acropolis_patio_animation_04FC8_bank4.inc"
};

static AnimationRecord _gAcropolisPatioAnimation04FC8Records[201] = {
#include "assets/acropolis_patio_animation_04FC8_records.inc"
};

static u16 _gAcropolisPatioAnimation04FC8Indices[20] = {
#include "assets/acropolis_patio_animation_04FC8_indices.inc"
};

static AnimationSet _gAcropolisPatioAnimation04FC8 = {
    _gAcropolisPatioAnimation04FC8Records,
    _gAcropolisPatioAnimation04FC8Indices,
    { NULL, _gAcropolisPatioAnimation04FC8Bank1, NULL, NULL, _gAcropolisPatioAnimation04FC8Bank4, NULL, NULL, NULL },
};

/// Second-visit player bank extension: NULL slot 47, then four clips in slots 48..51.
///
/// The directory is copied as pointer words; sets and tracks stay borrowed from
/// the patio overlay through playback. The NULL entry is part of the copy.
static AnimationSet* _gAcropolisPatioSecondVisitPlayerAnimationSets[] = {
    NULL,
    &_gAcropolisPatioAnimation03CD0,
    &_gAcropolisPatioAnimation04470,
    &_gAcropolisPatioAnimation0494C,
    &_gAcropolisPatioAnimation04FC8,
};

/// Installs the second visit's player clips in the equipped-weapon bank extension.
///
/// Copies all 5 directory words synchronously without starting playback.
/// Copied set pointers and tracks remain borrowed from this overlay.
static AnimationBankCopyRequest _gAcropolisPatioSecondVisitPlayerBankCopyRequest = {
    .source    = { .sets = _gAcropolisPatioSecondVisitPlayerAnimationSets },
    .wordCount = ARRAY_SIZE(_gAcropolisPatioSecondVisitPlayerAnimationSets),
};

AnimationPlayRequest D_acropolis_patio_801825CC[4] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Player clip 9 at the second-visit battle handoff, shared by normal and skipped paths.
///
/// Event dispatch supplies the equipped weapon's bank; this is a base-bank clip,
/// blended over six frames with player grid participation disabled.
static AnimationPlayRequest _gAcropolisPatioSecondVisitPlayerAnimation9Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_PATIO_SECOND_VISIT_PLAYER_ANIMATION_9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_PATIO_SECOND_VISIT_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player's room-space starting placement for the second-visit battle introduction.
static ActorTransform _gAcropolisPatioSecondVisitPlayerStartPlacement = { .pos = { -7100, 0, -1350, 0 }, .rot = { 0, 900, 0, 0 } };

ActorTransform D_acropolis_patio_80182648[2] = {
    { { -6590, 0, -1000, 0 }, { 0, 0, 0, 0 } },
    { { 3900, 0, 2300, 0 }, { 0, 0, 0, 0 } },
};

/// Destination of the player's scripted run during the second-visit introduction.
///
/// Only the leading XYZ words are consumed, in room-coordinate units. Rotation
/// remains in the stored transform but does not control the player's arrival yaw.
static ActorTransform _gAcropolisPatioSecondVisitPlayerRunDestination = { .pos = { -2300, 0, 700, 0 }, .rot = { 0, 1200, 0, 0 } };

/// Player placement restored when the second-visit battle introduction is skipped.
///
/// Shares the run destination's XYZ with a separate arrival yaw.
static ActorTransform _gAcropolisPatioSecondVisitPlayerSkipPlacement = { .pos = { -2300, 0, 700, 0 }, .rot = { 0, -848, 0, 0 } };

AnimationPlayRequest D_acropolis_patio_801826A8[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_acropolis_patio_8018270C = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_acropolis_patio_80182720 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_acropolis_patio_80182734[2] = {
    { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

ActorTransform D_acropolis_patio_8018275C = { { -6100, -3780, -1900, 0 }, { 0, -110, 0, 0 } };

ActorTransform D_acropolis_patio_80182774 = { 0 };

ActorTransform D_acropolis_patio_8018278C = { { -5100, -3780, -1900, 0 }, { 0, 80, 0, 0 } };

ActorTransform D_acropolis_patio_801827A4 = { { 0, 0, 0, 0 }, { 0, 512, 0, 0 } };

/// First Grinning Stranger's battle placement after the second-visit scene (actor 2).
///
/// Both normal completion and skipping restore this room-space transform.
static ActorTransform _gAcropolisPatioSecondVisitStranger1BattlePlacement = { .pos = { -6100, 210, 20, 0 }, .rot = { 0, 896, 0, 0 } };

/// Second Grinning Stranger's battle placement after the second-visit scene (actor 3).
///
/// Both normal completion and skipping restore this room-space transform.
static ActorTransform _gAcropolisPatioSecondVisitStranger2BattlePlacement = { .pos = { -3743, 210, -543, 0 }, .rot = { 0, 512, 0, 0 } };

ActorMotionWalkAnim D_acropolis_patio_801827EC = { .animationId = 7, .nextAnimId = 8 };

ActorCommand D_acropolis_patio_801827F4 = { { .loc = { 1, 3 } }, 0 };

ActorCommand D_acropolis_patio_801827F8 = { { .loc = { 0, 0 } }, 1 };

ActorCommand D_acropolis_patio_801827FC = { { .loc = { 0, 0 } }, 2 };

/// Bodyless controller that turns the live player left by half a turn over sixteen ticks.
///
/// Read synchronously at spawn; the patio callback and live player must remain
/// available until the controller finishes and releases its task.
static TaskDesc _gAcropolisPatioPlayerTurnLeftTaskDescriptor = { { { TASK_BODY_NONE, ACROPOLIS_PATIO_SCENE_TASK_PRIORITY } }, _acropolisPatioPlayerTurnLeftTask, { .value = 0 } };

EvsCommand D_acropolis_patio_8018280C[41] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisPatioSecondVisitPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioSecondVisitPlayerStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_8018275C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_patio_8018278C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_patio_8018270C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_patio_80182720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .transform = &_gAcropolisPatioSecondVisitPlayerRunDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_acropolis_patio_80182774 } }, { .message = { .pointer = &D_acropolis_patio_801827EC } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_acropolis_patio_801827A4 } }, { .message = { .pointer = &D_acropolis_patio_801827EC } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioSecondVisitPlayerAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisPatioSpawnPlayerTurnLeft }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetBattleActive }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioSecondVisitStranger1BattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioSecondVisitStranger2BattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_patio_801827FC } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_patio_80182BE4[21] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetBattleActive }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioSecondVisitStranger1BattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioSecondVisitStranger2BattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisPatioSecondVisitPlayerSkipPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPatioSecondVisitPlayerAnimation9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisPatioSetBattleActive }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room-coordinate centres of the patio's fourteen light glows.
///
/// Indices 0..2 are larger lights and the centres for nearby moving light points;
/// 3..6 are ceiling lights and 7..13 are lower-wall lights. Effects copy XYZ on
/// spawn, and moving points read their centre again when gathering. No sentinel.
/// The loaded patio overlay owns these signed-halfword game coordinates.
static SVECTOR _gAcropolisPatioLightAnchors[] = {
    { 180, -1108, 3610, 0 },
    { -6250, -1108, 2580, 0 },
    { -9310, -1108, 1130, 0 },
    { 1040, -2120, -4500, 0 },
    { -2390, -2120, -4500, 0 },
    { -6360, -2120, -4570, 0 },
    { -7640, -2120, -4570, 0 },
    { 0, -210, 2950, 0 },
    { -2010, -210, 2600, 0 },
    { -4010, -210, 2210, 0 },
    { -6000, -210, 1720, 0 },
    { -8010, -210, 1190, 0 },
    { -8510, -210, 230, 0 },
    { -8520, -210, -1000, 0 },
};

/// Visibility bits for the logical camera views that show a patio light.
enum {
    ACROPOLIS_PATIO_LIGHT_VIEW_3 = 1 << (3 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_4 = 1 << (4 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_5 = 1 << (5 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_7 = 1 << (7 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_8 = 1 << (8 - 1),
    ACROPOLIS_PATIO_LIGHT_VIEW_9 = 1 << (9 - 1),
};

/// Per-light visibility in logical views, parallel to `_gAcropolisPatioLightAnchors`.
///
/// Bit view - 1 is tested against `GameSession.location.loc.view`, independently
/// of the mapped background index. Glows use indices 0..13; moving points use
/// 0..2. Logical views 1..19 are valid and views outside the mask hide the light.
/// Both arrays must remain loaded throughout effect updates. No sentinel.
static u16 _gAcropolisPatioLightViewMasks[] = {
    ACROPOLIS_PATIO_LIGHT_VIEW_3,
    ACROPOLIS_PATIO_LIGHT_VIEW_4,
    ACROPOLIS_PATIO_LIGHT_VIEW_4 | ACROPOLIS_PATIO_LIGHT_VIEW_5,
    ACROPOLIS_PATIO_LIGHT_VIEW_8 | ACROPOLIS_PATIO_LIGHT_VIEW_9,
    ACROPOLIS_PATIO_LIGHT_VIEW_8 | ACROPOLIS_PATIO_LIGHT_VIEW_9,
    ACROPOLIS_PATIO_LIGHT_VIEW_7 | ACROPOLIS_PATIO_LIGHT_VIEW_8,
    ACROPOLIS_PATIO_LIGHT_VIEW_7,
    ACROPOLIS_PATIO_LIGHT_VIEW_3,
    ACROPOLIS_PATIO_LIGHT_VIEW_3,
    ACROPOLIS_PATIO_LIGHT_VIEW_3 | ACROPOLIS_PATIO_LIGHT_VIEW_4,
    ACROPOLIS_PATIO_LIGHT_VIEW_4 | ACROPOLIS_PATIO_LIGHT_VIEW_5,
    ACROPOLIS_PATIO_LIGHT_VIEW_4 | ACROPOLIS_PATIO_LIGHT_VIEW_5,
    ACROPOLIS_PATIO_LIGHT_VIEW_4 | ACROPOLIS_PATIO_LIGHT_VIEW_5,
    ACROPOLIS_PATIO_LIGHT_VIEW_4 | ACROPOLIS_PATIO_LIGHT_VIEW_5 | ACROPOLIS_PATIO_LIGHT_VIEW_7,
};
STATIC_ASSERT(ARRAY_SIZE(_gAcropolisPatioLightViewMasks) == ARRAY_SIZE(_gAcropolisPatioLightAnchors), acropolisPatioLightMaskCount);

WorldCollisionRoomResources gAcropolisPatioRoomCollisionResources[ACROPOLIS_PATIO_ROOM_COUNT] = {
    { .grid = &_gAcropolisPatioCollisionGrid, .viewBoundaryTriggers = _gAcropolisPatioViewBoundaryTriggers, .actionTriggers = _gAcropolisPatioRoom1ActionTriggers, .occluders = _gAcropolisPatioOccluders },
    { .grid = &_gAcropolisPatioCollisionGrid, .viewBoundaryTriggers = _gAcropolisPatioViewBoundaryTriggers, .actionTriggers = _gAcropolisPatioRooms2And3ActionTriggers, .occluders = _gAcropolisPatioOccluders },
    { .grid = &_gAcropolisPatioCollisionGrid, .viewBoundaryTriggers = _gAcropolisPatioViewBoundaryTriggers, .actionTriggers = _gAcropolisPatioRooms2And3ActionTriggers, .occluders = _gAcropolisPatioOccluders },
};

/// Logical-view to camera/image/sprite mapping for patio room 2.
///
/// Views 1..19 index bytes at view - 1. Logical view 4 uses index 17.
/// Other entries are identity mappings. The final zero lies outside the room's
/// reverse-search count and is retained storage, not a map terminator.
/// The loaded patio overlay owns these writable bytes.
static u8 _gAcropolisPatioRoom2ViewMap[20] = {
    1,
    2,
    3,
    17,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

/// Logical-view to camera/image/sprite mapping for patio room 3.
///
/// Views 1..19 index bytes at view - 1. Logical view 4 uses index 17, and logical view 7 uses index 13.
/// Other entries are identity mappings. The final zero lies outside the room's
/// reverse-search count and is retained storage, not a map terminator.
/// The loaded patio overlay owns these writable bytes.
static u8 _gAcropolisPatioRoom3ViewMap[20] = {
    1,
    2,
    3,
    17,
    5,
    6,
    13,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

u8* gAcropolisPatioViewMaps[ACROPOLIS_PATIO_ROOM_COUNT] = {
    gViewIdentityMap,
    _gAcropolisPatioRoom2ViewMap,
    _gAcropolisPatioRoom3ViewMap,
};

ViewCount gAcropolisPatioViewCounts[ACROPOLIS_PATIO_ROOM_COUNT] = {
    ACROPOLIS_PATIO_VIEW_COUNT,
    ACROPOLIS_PATIO_VIEW_COUNT,
    ACROPOLIS_PATIO_VIEW_COUNT,
};

WorldCoordRoomLighting gAcropolisPatioRoomLightingTable[ACROPOLIS_PATIO_ROOM_COUNT] = {
    { &_gAcropolisPatioRoomLights, NULL },
    { &_gAcropolisPatioRoomLights, NULL },
    { &_gAcropolisPatioRoomLights, NULL },
};

/// Door sound requests and map-marker nibbles carried by the patio warp endpoints.
enum {
    ACROPOLIS_PATIO_FOUNTAIN_ARRIVAL_SOUND    = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 2),
    ACROPOLIS_PATIO_FOUNTAIN_DEPARTURE_SOUND  = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 1),
    ACROPOLIS_PATIO_FOUNTAIN_BLOCKED_SOUND    = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 7),
    ACROPOLIS_PATIO_CAFETERIA_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 5),
    ACROPOLIS_PATIO_CAFETERIA_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 4),
    ACROPOLIS_PATIO_CAFETERIA_BLOCKED_SOUND   = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 6),
    ACROPOLIS_PATIO_FOUNTAIN_MAP_FLAG         = 496,
    ACROPOLIS_PATIO_CAFETERIA_MAP_FLAG        = 501,
};

DirectionWarpEntry gAcropolisPatioWarpEntries[ACROPOLIS_PATIO_WARP_COUNT] = {
    {
        .player         = { { .word = 2560 }, 3906, 1, 768 },
        .companion      = { { .word = 2560 }, 3906, 1, 768 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 0 }, 4986, 1, -4079 },
        .companion      = { { .word = 0 }, 4986, 1, -4079 },
        .arrivalSound   = ACROPOLIS_PATIO_FOUNTAIN_ARRIVAL_SOUND,
        .departureSound = ACROPOLIS_PATIO_FOUNTAIN_DEPARTURE_SOUND,
        .blockedSound   = ACROPOLIS_PATIO_FOUNTAIN_BLOCKED_SOUND,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = ACROPOLIS_PATIO_FOUNTAIN_MAP_FLAG,
    },
    {
        .player         = { { .word = 0 }, -7014, -302, -4439 },
        .companion      = { { .word = 0 }, -7014, -302, -4439 },
        .arrivalSound   = ACROPOLIS_PATIO_CAFETERIA_ARRIVAL_SOUND,
        .departureSound = ACROPOLIS_PATIO_CAFETERIA_DEPARTURE_SOUND,
        .blockedSound   = ACROPOLIS_PATIO_CAFETERIA_BLOCKED_SOUND,
        .initialView    = 7,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = ACROPOLIS_PATIO_CAFETERIA_MAP_FLAG,
    },
    {
        .player         = { { .word = 2304 }, 4223, -460, 1263 },
        .companion      = { { .word = 2304 }, 4408, -600, 1264 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_SCRIPTED_PLAYER,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Dimensions of the patio grid's X-major cell directory.
enum { ACROPOLIS_PATIO_GRID_CELLS_X = 7,
       ACROPOLIS_PATIO_GRID_CELLS_Z = 4 };

/// Room-space normals of `_gAcropolisPatioCollisionGrid`, using `ONE` per unit.
static SVECTOR _gAcropolisPatioCollisionGridNormals[33] = {
#include "assets/acropolis_patio_collision_06838_normals.inc"
};

/// Signed room-coordinate vertex pool of `_gAcropolisPatioCollisionGrid`.
static SVECTOR _gAcropolisPatioCollisionGridVerts[201] = {
#include "assets/acropolis_patio_collision_06838_verts.inc"
};

/// Indexed triangles and quads of `_gAcropolisPatioCollisionGrid`.
static WorldCollisionGridFace _gAcropolisPatioCollisionGridFaces[70] = {
#include "assets/acropolis_patio_collision_06838_faces.inc"
};

/// Packed face-index lists of `_gAcropolisPatioCollisionGrid`, each ending at -1.
static s16 _gAcropolisPatioCollisionGridCells[402] = {
#include "assets/acropolis_patio_collision_06838_cells.inc"
};

// Bind the generated directory's constant element offsets to the face-index pool.
#define GRID_CELL(i) (&_gAcropolisPatioCollisionGridCells[i])
/// X-major cell directory of `_gAcropolisPatioCollisionGrid`; NULL cells are empty.
static s16* _gAcropolisPatioCollisionGridTable[ACROPOLIS_PATIO_GRID_CELLS_X * ACROPOLIS_PATIO_GRID_CELLS_Z] = {
#include "assets/acropolis_patio_collision_06838_table.inc"
};
#undef GRID_CELL

/// Collision mesh shared by all three patio rooms, with 4000-unit square XZ cells.
///
/// Room setup binds the writable descriptor to the active view coordinate.
/// Face indices address 201 vertices and 33 normals; cell lists select 70 faces.
/// The descriptor and all five pools must remain loaded while the grid is active.
static WorldCollisionGrid _gAcropolisPatioCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisPatioCollisionGridNormals,
    .vertices    = _gAcropolisPatioCollisionGridVerts,
    .faces       = _gAcropolisPatioCollisionGridFaces,
    .cellFaceIds = _gAcropolisPatioCollisionGridTable,
    .xBias       = 9010,
    .zBias       = 7675,
    .cellCountX  = ACROPOLIS_PATIO_GRID_CELLS_X,
    .cellCountZ  = ACROPOLIS_PATIO_GRID_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisPatioCollisionGridFaces),
};

/// View-crossing quads shared by all patio rooms, ending at the LAST-marked quad.
///
/// Directed logical-view pairs are 2/6, 2/3, 3/4, 4/7, 7/8 (two crossings) and
/// 8/9. Setup binds these room-coordinate quads to the view and mutates their
/// links, flags and hit latches. Keep all fourteen records loaded until unlinked.
static WorldCollisionTrigger _gAcropolisPatioViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 4988, -2288, -1890, 0 }, { { -2356, -3568, 233, 0 }, { 2357, -3568, -233, 0 }, { -2356, 3568, 233, 0 }, { 2357, 3568, -233, 0 } }, { -406, 0, -4106, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5052, -2528, -2276, 0 }, { { 2357, -3552, -233, 0 }, { -2356, -3552, 233, 0 }, { 2357, 3552, -233, 0 }, { -2356, 3552, 233, 0 } }, { 404, 0, 4087, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1938, -2400, 374, 0 }, { { 906, -3568, 2187, 0 }, { -905, -3568, -2187, 0 }, { 906, 3568, 2187, 0 }, { -905, 3568, -2187, 0 } }, { -3811, 0, 1577, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1557, -2464, 439, 0 }, { { -797, -3568, -2230, 0 }, { 797, -3568, 2231, 0 }, { -797, 3568, -2230, 0 }, { 797, 3568, 2231, 0 } }, { 3885, 0, -1389, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3872, -2432, 256, 0 }, { { 0, -3568, -2368, 0 }, { 0, -3568, 2368, 0 }, { 0, 3568, -2368, 0 }, { 0, 3568, 2368, 0 } }, { 4125, 0, 0, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3648, -2400, 192, 0 }, { { 0, -3568, 2368, 0 }, { 0, -3568, -2368, 0 }, { 0, 3568, 2368, 0 }, { 0, 3568, -2368, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7762, -2432, -2162, 0 }, { { 2171, -3568, -129, 0 }, { -2170, -3568, 130, 0 }, { 2171, 3568, -129, 0 }, { -2170, 3568, 130, 0 } }, { 244, 0, ONE, 0 }, { 0, 0, ONE, 0 }, 4159, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7562, -2432, -1771, 0 }, { { -2121, -3568, 239, 0 }, { 2122, -3568, -239, 0 }, { -2121, 3568, 239, 0 }, { 2122, 3568, -239, 0 } }, { -459, 0, -4071, 0 }, { 0, 0, ONE, 0 }, 4127, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6435, -2400, -3410, 0 }, { { -107, -3568, -906, 0 }, { 107, -3568, 906, 0 }, { -107, 3568, -906, 0 }, { 107, 3568, 906, 0 } }, { 4068, 0, -481, 0 }, { 0, 0, ONE, 0 }, 3674, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6294, -2464, -3156, 0 }, { { 48, -3568, 561, 0 }, { -47, -3568, -560, 0 }, { 48, 3568, 561, 0 }, { -47, 3568, -560, 0 } }, { -4086, 0, 345, 0 }, { 0, 0, ONE, 0 }, 3611, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1537, -2496, -3841, 0 }, { { 333, -3568, 1513, 0 }, { -333, -3568, -1513, 0 }, { 333, 3568, 1513, 0 }, { -333, 3568, -1513, 0 } }, { -4003, 0, 880, 0 }, { 0, 0, ONE, 0 }, 3882, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2240, -2432, -3824, 0 }, { { -269, -3568, -1433, 0 }, { 269, -3568, 1433, 0 }, { -269, 3568, -1433, 0 }, { 269, 3568, 1433, 0 } }, { 4034, 0, -759, 0 }, { 0, 0, ONE, 0 }, 3848, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5937, 0, -4513, 0 }, { { 787, -3568, -1098, 0 }, { -786, -3568, 1099, 0 }, { 787, 3568, -1098, 0 }, { -786, 3568, 1099, 0 } }, { 3340, 0, 2391, 0 }, { 0, 0, ONE, 0 }, 3814, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5890, 0, -4385, 0 }, { { -463, -3568, 692, 0 }, { 463, -3568, -692, 0 }, { -463, 3568, 692, 0 }, { 463, 3568, -692, 0 } }, { -3411, 0, -2283, 0 }, { 0, 0, ONE, 0 }, 3656, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Action IDs, CAP slots and packed warp requests used by the patio action quads.
///
/// Warp requests store the patio departure ID in the high nibble and the
/// destination arrival ID in the low nibble. Stair parameters pack surface
/// class 4 above the step count; the control bit selects descent.
enum {
    ACROPOLIS_PATIO_TRIGGER_OPENING_ACTION      = 0,
    ACROPOLIS_PATIO_TRIGGER_FOLLOW_UP_ACTION    = 1,
    ACROPOLIS_PATIO_TRIGGER_SECOND_VISIT_ACTION = 2,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_1       = 1,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_2       = 2,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_5       = 5,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_7       = 7,
    ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_11      = 11,
    ACROPOLIS_PATIO_FOUNTAIN_WARP_REQUEST       = (2 << 4) | 1,
    ACROPOLIS_PATIO_CAFETERIA_WARP_REQUEST      = (3 << 4) | 2,
    ACROPOLIS_PATIO_SQUARE_WARP_REQUEST         = (1 << 4) | 5,
    ACROPOLIS_PATIO_STAIR_ASCEND_PARAMETER      = (4 << 4) | 3,
    ACROPOLIS_PATIO_STAIR_DESCEND_PARAMETER     = (4 << 4) | 2,
    ACROPOLIS_PATIO_STAIR_DESCEND_CONTROL       = 0x100,
};

/// Interaction and automatic-transition quads for patio room 1.
///
/// Includes stairs, fountain/cafeteria exits, CAP interactions and three room
/// action IDs. The twelfth included record carries LAST. Room setup mutates
/// links, view coordinates, enable flags and hit latches; keep the list loaded
/// until unlinked. Positions/radii use room units and normals use `ONE` per unit.
static WorldCollisionTrigger _gAcropolisPatioRoom1ActionTriggers[12] = {
    { NULL, NULL, NULL, { 3903, -96, 895, 0 }, { { -669, 0, -11, 0 }, { 492, 0, -445, 0 }, { -491, 0, 446, 0 }, { 670, 0, 12, 0 } }, { 0, 4109, 0, 0 }, { -1189, 0, -3920, 0 }, 668, WORLD_COLLISION_TRIGGER_ACTION_FACING, ACROPOLIS_PATIO_STAIR_ASCEND_PARAMETER, 16, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -64, -4128, 0 }, { { -711, 0, -432, 0 }, { 697, 0, -432, 0 }, { -728, 0, 433, 0 }, { 713, 0, 433, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_FOUNTAIN, ACROPOLIS_PATIO_FOUNTAIN_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7265, -352, -4704, 0 }, { { -1031, 0, -592, 0 }, { 1017, 0, -592, 0 }, { -1016, 0, 593, 0 }, { 1033, 0, 593, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, ONE, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_CAFETERIA, ACROPOLIS_PATIO_CAFETERIA_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4689, -384, -4768, 0 }, { { -1335, 0, -592, 0 }, { 1321, 0, -592, 0 }, { -1320, 0, 593, 0 }, { 1337, 0, 593, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -384, -3272, 0 }, { { -1431, 0, 184, 0 }, { 1001, 0, -1288, 0 }, { -1448, 0, 1465, 0 }, { 1881, 0, -359, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4097, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3999, -720, 1183, 0 }, { { -1187, -1164, 440, 0 }, { 1204, -1166, -401, 0 }, { -1204, 1166, 402, 0 }, { 1188, 1164, -440, 0 } }, { -1360, -74, -3864, 0 }, { -1189, 0, -3920, 0 }, 1722, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_ACROPOLIS_SQUARE, ACROPOLIS_PATIO_SQUARE_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -603, 1408, 0 }, { { -1021, 0, 181, 0 }, { 908, 0, -509, 0 }, { -907, 0, 510, 0 }, { 1022, 0, -180, 0 } }, { 0, 4099, 0, 0 }, { 1189, 0, 3920, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | ACROPOLIS_PATIO_STAIR_DESCEND_CONTROL | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_STAIR_DESCEND_PARAMETER, 144, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1680, -384, -4256, 0 }, { { -1895, 0, -880, 0 }, { 1881, 0, -880, 0 }, { -1880, 0, 881, 0 }, { 1897, 0, 881, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 2079, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8066, -64, -881, 0 }, { { 621, 0, -386, 0 }, { 639, 0, 374, 0 }, { -637, 0, -372, 0 }, { -620, 0, 387, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, -202, 0 }, 738, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2064, -29, 31, 0 }, { { 325, 0, 1661, 0 }, { -997, 0, -1557, 0 }, { 933, 0, 1461, 0 }, { -261, 0, -1565, 0 } }, { 0, 4115, 0, 0 }, { 3856, 0, -1380, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_TRIGGER_OPENING_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6543, 0, 48, 0 }, { { -749, 0, -1052, 0 }, { 956, 0, -1517, 0 }, { -1372, 0, 1133, 0 }, { 1165, 0, 1436, 0 } }, { 0, 4099, 0, 0 }, { -1189, 0, -3920, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_TRIGGER_FOLLOW_UP_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -7201, -384, -2240, 0 }, { { -1586, 0, -428, 0 }, { 1574, 0, -446, 0 }, { -1572, 0, 446, 0 }, { 1587, 0, 429, 0 } }, { 0, 4112, 0, 0 }, { -1189, 0, -3920, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_TRIGGER_SECOND_VISIT_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and automatic-transition quads shared by patio rooms 2 and 3.
///
/// Uses the later stair geometry, routes CAP command 2 through the room receiver,
/// and replaces room action 1 with weapon-assisted CAP command 11. The twelfth
/// included record carries LAST. These are mutable linked room-coordinate
/// records and must remain loaded until unlinked.
static WorldCollisionTrigger _gAcropolisPatioRooms2And3ActionTriggers[12] = {
    { NULL, NULL, NULL, { 3903, -96, 959, 0 }, { { -669, 0, -11, 0 }, { 492, 0, -445, 0 }, { -491, 0, 446, 0 }, { 670, 0, 12, 0 } }, { 0, 4109, 0, 0 }, { -1189, 0, -3920, 0 }, 668, WORLD_COLLISION_TRIGGER_ACTION_FACING, ACROPOLIS_PATIO_STAIR_ASCEND_PARAMETER, 16, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -64, -4128, 0 }, { { -711, 0, -432, 0 }, { 697, 0, -432, 0 }, { -728, 0, 433, 0 }, { 713, 0, 433, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_FOUNTAIN, ACROPOLIS_PATIO_FOUNTAIN_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7265, -352, -4704, 0 }, { { -1031, 0, -592, 0 }, { 1017, 0, -592, 0 }, { -1016, 0, 593, 0 }, { 1033, 0, 593, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, ONE, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_CAFETERIA, ACROPOLIS_PATIO_CAFETERIA_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4689, -384, -4768, 0 }, { { -1335, 0, -592, 0 }, { 1321, 0, -592, 0 }, { -1320, 0, 593, 0 }, { 1337, 0, 593, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -384, -3272, 0 }, { { -1431, 0, 184, 0 }, { 1001, 0, -1288, 0 }, { -1448, 0, 1465, 0 }, { 1881, 0, -359, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4097, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4128, -464, 1472, 0 }, { { -1021, 0, 181, 0 }, { 908, 0, -509, 0 }, { -907, 0, 510, 0 }, { 1022, 0, -180, 0 } }, { 0, 4099, 0, 0 }, { -1189, 0, -3920, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_ACROPOLIS_SQUARE, ACROPOLIS_PATIO_SQUARE_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4128, -475, 1472, 0 }, { { -1021, 0, 181, 0 }, { 908, 0, -509, 0 }, { -907, 0, 510, 0 }, { 1022, 0, -180, 0 } }, { 0, 4099, 0, 0 }, { 1189, 0, 3920, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | ACROPOLIS_PATIO_STAIR_DESCEND_CONTROL | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_STAIR_DESCEND_PARAMETER, 152, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1680, -384, -4256, 0 }, { { -1895, 0, -880, 0 }, { 1881, 0, -880, 0 }, { -1880, 0, 881, 0 }, { 1897, 0, 881, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 2079, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8066, -64, -849, 0 }, { { 621, 0, -418, 0 }, { 639, 0, 406, 0 }, { -637, 0, -404, 0 }, { -620, 0, 419, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, -202, 0 }, 754, WORLD_COLLISION_TRIGGER_ACTION_CAP, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2064, -29, 31, 0 }, { { 325, 0, 1661, 0 }, { -997, 0, -1557, 0 }, { 933, 0, 1461, 0 }, { -261, 0, -1565, 0 } }, { 0, 4115, 0, 0 }, { 3856, 0, -1380, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_TRIGGER_OPENING_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -8239, 0, 592, 0 }, { { -749, 0, -1052, 0 }, { 1788, 0, -1517, 0 }, { -1372, 0, 1133, 0 }, { 1997, 0, 1436, 0 } }, { 0, 4101, 0, 0 }, { -1189, 0, -3920, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, ACROPOLIS_PATIO_TRIGGER_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7201, -384, -2240, 0 }, { { -1586, 0, -428, 0 }, { 1574, 0, -446, 0 }, { -1572, 0, 446, 0 }, { 1587, 0, 429, 0 } }, { 0, 4112, 0, 0 }, { -1189, 0, -3920, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_PATIO_TRIGGER_SECOND_VISIT_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space line-of-sight blockers shared by all three patio rooms.
///
/// The second included quad carries LAST. Setup mutates links and enable flags;
/// keep the records loaded until unlinked. Low flag bit 0 is retained resource
/// data with no proven interpretation. Positions/radii are game units.
static WorldCollisionOccluder _gAcropolisPatioOccluders[2] = {
    { NULL, NULL, { -1184, -1760, -2112, 0 }, { { -3904, -1984, 0, 0 }, { 3904, -1984, 0, 0 }, { -3904, 1984, 0, 0 }, { 3904, 1984, 0, 0 } }, { 0, 0, -4098, 0 }, 4374, 1, 0 },
    { NULL, NULL, { 2336, -1728, -5888, 0 }, { { 0, -1984, 3904, 0 }, { 0, -1984, -3904, 0 }, { 0, 1984, 3904, 0 }, { 0, 1984, -3904, 0 } }, { -4098, 0, 0, 0 }, 4374, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Actor loading/spawn recipes for patio placement variant 1: Stranger and SWAT.
///
/// Placement IDs 19/107 select decimal files 19/122 in group bases 10/30.
/// The loaded overlay owns this table through its AREA_PLACEMENT_END record;
/// task descriptors are borrowed from the loaded actor packages.
static AreaResource _gAcropolisPatioVariant1ActorResources[] = {
    { .entryId = 19, .fileNumber = 19, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101900GrinningStrangerTaskDesc },
    { .entryId = 107, .fileNumber = 122, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor312200SwatMember1TaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor loading/spawn recipes for patio variant 2's two Stranger packages.
///
/// Placement IDs 10/19 select decimal files 170/19 in group bases 30/10.
/// The overlay-owned list ends at AREA_PLACEMENT_END and borrows the loaded
/// actor task descriptors, each at descriptor index zero.
static AreaResource _gAcropolisPatioVariant2ActorResources[] = {
    { .entryId = 10, .fileNumber = 170, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor317000GrinningStrangerTaskDesc },
    { .entryId = 19, .fileNumber = 19, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101900GrinningStrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor loading/spawn recipes for patio variant 3's rats and moths.
///
/// Placement IDs 7/8 share decimal file 7 in group base 10, with distinct task
/// descriptors. The overlay-owned list ends at AREA_PLACEMENT_END and borrows
/// the loaded actor package's descriptor records.
static AreaResource _gAcropolisPatioVariant3ActorResources[] = {
    { .entryId = 7, .fileNumber = 7, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor00700RatTaskDesc },
    { .entryId = 8, .fileNumber = 7, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor00700MothTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor loading/spawn recipes for patio variant 4's moths and Stranger.
///
/// Placement IDs 8/10 select decimal files 7/10 in group bases 30/40.
/// The overlay-owned list ends at AREA_PLACEMENT_END and borrows the loaded
/// actor task descriptors, each at descriptor index zero.
static AreaResource _gAcropolisPatioVariant4ActorResources[] = {
    { .entryId = 8, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor300700MothTaskDesc },
    { .entryId = 10, .fileNumber = 10, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401000StrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor loading/spawn recipe for patio variant 5's Stranger placements.
///
/// Placement ID 10 selects decimal file 10 in group base 40, descriptor zero.
/// The overlay-owned list ends at AREA_PLACEMENT_END and borrows the loaded
/// actor package's descriptor record.
static AreaResource _gAcropolisPatioVariant5ActorResources[] = {
    { .entryId = 10, .fileNumber = 10, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401000StrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Empty actor-resource list for patio placement variant 10.
///
/// Its sole record is AREA_PLACEMENT_END: the variant's placements have no
/// actor loading/spawn recipes here. The list lives with the patio overlay.
static AreaResource _gAcropolisPatioVariant10ActorResources[] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant D_acropolis_patio_80184A90[12] = {
    { NULL, NULL },
    { gMapAkropolisPatioVariant1Placements, _gAcropolisPatioVariant1ActorResources },
    { gMapAkropolisPatioVariant2Placements, _gAcropolisPatioVariant2ActorResources },
    { gMapAkropolisPatioVariant3Placements, _gAcropolisPatioVariant3ActorResources },
    { gMapAkropolisPatioVariant4Placements, _gAcropolisPatioVariant4ActorResources },
    { gMapAkropolisPatioVariant5Placements, _gAcropolisPatioVariant5ActorResources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapAkropolisPatioVariant10Placements, _gAcropolisPatioVariant10ActorResources },
    { NULL, NULL },
};

/// Sprite-atlas pages: opaque 8-bit textures at VRAM Y 0, sharing CLUT (0, 255).
enum {
    ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_0 = getTPage(1, 0, 832, 0),
    ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1 = getTPage(1, 0, 896, 0),
    ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2 = getTPage(1, 0, 960, 0),
    ACROPOLIS_PATIO_SPRITE_CLUT           = getClut(0, 255),
    ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB    = 128,
    ACROPOLIS_PATIO_SPRITE_OPAQUE         = 0,
};

/// Background-only sprite batches for mapped patio view 1.
///
/// The zero first count keeps decoded image strips enabled; the next record
/// ends the list. The view retains this address in its empty source slot,
/// which supplies no source sprites. Storage lives with the patio overlay.
static SpriteBatch _gAcropolisPatioView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio view 2.
///
/// The 27 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView2SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView2SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 96 } }, -88, -40, 1275, { .fields = { 72, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 104 } }, -120, -40, 1200, { .fields = { 88, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 112 } }, -160, -40, 1150, { .fields = { 88, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 72 } }, -104, -104, 1200, { .fields = { 48, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 48 } }, -120, -96, 1175, { .fields = { 16, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 48 } }, -136, -96, 1150, { .fields = { 24, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -96, 1125, { .fields = { 16, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 56 } }, -160, -112, 1075, { .fields = { 40, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 80 } }, -152, -112, 1100, { .fields = { 64, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 80, 32, 1150, { .fields = { 24, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 48 } }, 88, 32, 1125, { .fields = { 16, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 56, 1125, { .fields = { 32, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, 64, 1100, { .fields = { 16, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 72, 1075, { .fields = { 24, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 96, 32, 1125, { .fields = { 40, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, 104, 32, 1125, { .fields = { 16, 168 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, 120, 72, 1050, { .fields = { 32, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 56, 1075, { .fields = { 104, 224 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 40, 1100, { .fields = { 104, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 88 } }, 112, -48, 1100, { .fields = { 32, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 72 } }, 112, -120, 1100, { .fields = { 24, 168 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 88 } }, 136, -16, 1000, { .fields = { 72, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 96 } }, 144, -16, 975, { .fields = { 80, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 104 } }, 152, -16, 950, { .fields = { 120, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -128, -88, 1250, { .fields = { 88, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 32 } }, -144, -80, 1275, { .fields = { 16, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 64 } }, -160, -88, 1300, { .fields = { 72, 192 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio view 2.
///
/// Ranges index `_gAcropolisPatioView2SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 6, 0, 0, { 3, 0 } },
    { 9, 12, 0, 0, { 2, 0 } },
    { 21, 3, 0, 0, { 4, 0 } },
    { 24, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio view 3.
///
/// The 45 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView3SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView3SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 120, 144 } }, -160, -120, 2625, { .fields = { 104, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 88, 48 } }, -112, -24, 2625, { .fields = { 32, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -24, 2700, { .fields = { 120, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 48 } }, -8, -24, 2750, { .fields = { 8, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 32 } }, -48, -56, 2625, { .fields = { 72, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, -48, 2700, { .fields = { 72, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 32 } }, -48, -48, 2725, { .fields = { 64, 104 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 104 } }, -72, -120, 2650, { .fields = { 56, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, -88, 1650, { .fields = { 80, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, 72, -112, 1625, { .fields = { 88, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 80, -120, 1375, { .fields = { 88, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, -120, 1250, { .fields = { 56, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 64, 16 } }, 96, -120, 1250, { .fields = { 0, 104 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 72, 8 } }, 88, -104, 1375, { .fields = { 48, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_0, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 80, 16 } }, 80, -96, 1500, { .fields = { 112, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_0, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 88, 16 } }, 72, -80, 1625, { .fields = { 112, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_0, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 96, 8 } }, 64, -64, 1650, { .fields = { 104, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 72 } }, 72, -64, 1650, { .fields = { 24, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 8, 2500, { .fields = { 64, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 24 } }, 72, 8, 1650, { .fields = { 64, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 96 } }, 88, -80, 1450, { .fields = { 96, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, 72, 16, 1625, { .fields = { 96, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 32, 1600, { .fields = { 64, 56 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, 88, 32, 1425, { .fields = { 16, 56 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 16, 1600, { .fields = { 56, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, 88, 16, 1450, { .fields = { 16, 72 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 160 } }, 128, -120, 1050, { .fields = { 96, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 8 } }, 120, 40, 1050, { .fields = { 24, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 40 } }, 88, 48, 1150, { .fields = { 96, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 104, 64, 1100, { .fields = { 96, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 80, 1075, { .fields = { 48, 232 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 88, 1050, { .fields = { 104, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 16 } }, 104, 48, 1050, { .fields = { 48, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 64, 1200, { .fields = { 24, 24 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 8 } }, 120, 80, 1100, { .fields = { 56, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 24 } }, 128, 88, 1050, { .fields = { 48, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 80, -16, 1550, { .fields = { 32, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 88, -16, 1525, { .fields = { 24, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 96, -16, 1500, { .fields = { 40, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 48 } }, 96, -8, 1300, { .fields = { 16, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 56 } }, 104, -8, 1300, { .fields = { 112, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 64 } }, 112, -8, 1250, { .fields = { 120, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 64 } }, 128, -8, 1200, { .fields = { 8, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 32 } }, 96, -16, 2500, { .fields = { 64, 72 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 88 } }, 152, 8, 1500, { .fields = { 48, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio view 3.
///
/// Ranges index `_gAcropolisPatioView3SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 2, 0 } },
    { 1, 5, 0, 0, { 7, 0 } },
    { 6, 1, 0, 0, { 6, 0 } },
    { 7, 1, 0, 0, { 8, 0 } },
    { 8, 9, 0, 0, { 0, 0 } },
    { 17, 3, 0, 0, { 9, 0 } },
    { 20, 6, 0, 0, { 5, 0 } },
    { 26, 10, 0, 0, { 10, 0 } },
    { 36, 0, 0, 0, { 1, 0 } },
    { 36, 3, 0, 0, { 11, 0 } },
    { 39, 4, 0, 0, { 4, 0 } },
    { 43, 1, 0, 0, { 12, 0 } },
    { 44, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio views 4 and 17.
///
/// The 15 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView4SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView4SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 32 } }, -152, -104, 1250, { .fields = { 72, 128 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 96 } }, -136, -72, 1250, { .fields = { 104, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, -96, 24, 1450, { .fields = { 104, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 40, 1425, { .fields = { 104, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 8 } }, -152, 24, 1250, { .fields = { 56, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 64, 8 } }, -160, 32, 1200, { .fields = { 48, 224 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 8 } }, -160, 40, 1175, { .fields = { 56, 232 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 16 } }, -160, 48, 1350, { .fields = { 56, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, -8, 1150, { .fields = { 104, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, 24, 1150, { .fields = { 104, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 64 } }, -136, -8, 1100, { .fields = { 112, 168 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 72 } }, -144, -8, 1050, { .fields = { 112, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 80 } }, -152, -8, 1000, { .fields = { 120, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 80 } }, -160, 0, 950, { .fields = { 120, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, -160, 32, 1375, { .fields = { 112, 232 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio views 4 and 17.
///
/// Ranges index `_gAcropolisPatioView4SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 6, 0, 0, { 2, 0 } },
    { 14, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only sprite batches for mapped patio view 5.
///
/// The zero first count keeps decoded image strips enabled; the next record
/// ends the list. The view retains this address in its empty source slot,
/// which supplies no source sprites. Storage lives with the patio overlay.
static SpriteBatch _gAcropolisPatioView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio view 6.
///
/// The 13 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView6SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView6SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 24, 1325, { .fields = { 112, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, -120, 1300, { .fields = { 80, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 64 } }, 128, -96, 1300, { .fields = { 96, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 40 } }, 120, -32, 1300, { .fields = { 96, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 104, 8, 1325, { .fields = { 112, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 24, 1250, { .fields = { 112, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 8, 1250, { .fields = { 64, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, 24, 1225, { .fields = { 80, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, -24, 1200, { .fields = { 120, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, 128, 0, 1200, { .fields = { 112, 104 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 64 } }, 136, -24, 1175, { .fields = { 120, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 72 } }, 144, -24, 1150, { .fields = { 120, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 72 } }, 152, -24, 1125, { .fields = { 120, 72 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio view 6.
///
/// Ranges index `_gAcropolisPatioView6SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio views 7 and 13.
///
/// The 21 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView7SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView7SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 32, 837, { .fields = { 88, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 48, 920, { .fields = { 80, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 88, 862, { .fields = { 80, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, 72, 887, { .fields = { 80, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, -152, 56, 912, { .fields = { 64, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -128, 16, 920, { .fields = { 88, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -128, 56, 912, { .fields = { 48, 232 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -128, 72, 887, { .fields = { 48, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 32 } }, -128, 88, 862, { .fields = { 56, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 32 } }, -88, 88, 862, { .fields = { 80, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -88, 72, 887, { .fields = { 56, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -88, 56, 912, { .fields = { 56, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -88, 40, 925, { .fields = { 56, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -128, -24, 920, { .fields = { 88, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -128, -64, 920, { .fields = { 88, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 40 } }, -128, -104, 920, { .fields = { 96, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -136, -120, 920, { .fields = { 88, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 40, 950, { .fields = { 112, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 0, 950, { .fields = { 112, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -160, 40, 968, { .fields = { 48, 16 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, -152, 24, 968, { .fields = { 64, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio views 7 and 13.
///
/// Ranges index `_gAcropolisPatioView7SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, 2, 0, 0, { 2, 0 } },
    { 19, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio view 8.
///
/// The 56 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView8SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView8SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -152, -120, 725, { .fields = { 16, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 88 } }, -152, -80, 725, { .fields = { 80, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 56 } }, -144, 8, 725, { .fields = { 48, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 64, 775, { .fields = { 80, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, 88, 725, { .fields = { 40, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, -72, 80, 725, { .fields = { 56, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, -80, 96, 700, { .fields = { 24, 232 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 88, 8 } }, -152, 64, 775, { .fields = { 64, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 96, 8 } }, -160, 72, 750, { .fields = { 48, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 88, 16 } }, -160, 80, 725, { .fields = { 64, 232 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 80, 24 } }, -160, 96, 700, { .fields = { 80, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 24 } }, -96, -104, 1375, { .fields = { 0, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 96 } }, -88, -80, 1375, { .fields = { 104, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 32, 1375, { .fields = { 32, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 8 } }, -96, 16, 1400, { .fields = { 88, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 64, 16 } }, -112, 24, 1375, { .fields = { 72, 16 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 64, 16 } }, -112, 40, 1350, { .fields = { 72, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, -72, -80, 2250, { .fields = { 96, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, -64, 2250, { .fields = { 56, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 48 } }, -72, -48, 2250, { .fields = { 40, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 8 } }, -80, 0, 2250, { .fields = { 88, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -80, 8, 2250, { .fields = { 104, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 56 } }, -64, -64, 3125, { .fields = { 104, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -72, -8, 3125, { .fields = { 104, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 56 } }, -80, -8, 1200, { .fields = { 72, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 72 } }, -88, -8, 1150, { .fields = { 120, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 88 } }, -96, -8, 1100, { .fields = { 96, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 80 } }, -120, 0, 1000, { .fields = { 72, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 48 } }, -64, -24, 1950, { .fields = { 64, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 56 } }, -72, -24, 1825, { .fields = { 96, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 48 } }, -80, -16, 1700, { .fields = { 64, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, -56, -24, 2350, { .fields = { 32, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, -24, 2325, { .fields = { 32, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 8 } }, -16, -64, 3125, { .fields = { 8, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 32 } }, -8, -56, 3125, { .fields = { 40, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 16 } }, -8, -24, 3125, { .fields = { 120, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -24, -8, 3125, { .fields = { 96, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 24 } }, -40, -24, 3125, { .fields = { 8, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 32 } }, -56, 40, 850, { .fields = { 16, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, 32, 900, { .fields = { 48, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 48 } }, -56, 8, 950, { .fields = { 48, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 24, 975, { .fields = { 48, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 32 } }, -80, 16, 1000, { .fields = { 32, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -64, 32, 1025, { .fields = { 96, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 24 } }, -72, 8, 1025, { .fields = { 8, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 8 } }, -72, 40, 1025, { .fields = { 104, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 32 } }, 0, 0, 1625, { .fields = { 24, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, -16, 1775, { .fields = { 72, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, 8, 0, 1775, { .fields = { 88, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 16, 1775, { .fields = { 112, 192 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 24 } }, -8, -8, 1950, { .fields = { 0, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, -56, -8, 2200, { .fields = { 120, 128 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -40, -8, 2375, { .fields = { 40, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, -48, -24, 2375, { .fields = { 120, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 8 } }, -48, 0, 2375, { .fields = { 48, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 24 } }, -48, -16, 2375, { .fields = { 8, 24 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio view 8.
///
/// Ranges index `_gAcropolisPatioView8SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 17, 0 } },
    { 11, 6, 0, 0, { 2, 0 } },
    { 17, 5, 0, 0, { 9, 0 } },
    { 22, 2, 0, 0, { 3, 0 } },
    { 24, 4, 0, 0, { 11, 0 } },
    { 28, 3, 0, 0, { 0, 0 } },
    { 31, 2, 0, 0, { 12, 0 } },
    { 33, 4, 0, 0, { 1, 0 } },
    { 37, 1, 0, 0, { 15, 0 } },
    { 38, 2, 0, 0, { 4, 0 } },
    { 40, 3, 0, 0, { 13, 0 } },
    { 43, 3, 0, 0, { 7, 0 } },
    { 46, 1, 0, 0, { 10, 0 } },
    { 47, 3, 0, 0, { 8, 0 } },
    { 50, 1, 0, 0, { 14, 0 } },
    { 51, 1, 0, 0, { 5, 0 } },
    { 52, 3, 0, 0, { 16, 0 } },
    { 55, 1, 0, 0, { 6, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped patio view 9.
///
/// The 47 source elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisPatioView9SpriteBatches` selects their ranges.
/// Storage belongs to the loaded patio overlay and remains writable.
static SpriteSource _gAcropolisPatioView9SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, -88, 300, { .fields = { 112, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 48 } }, -160, -48, 300, { .fields = { 40, 56 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 40 } }, -160, 0, 300, { .fields = { 32, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 80 } }, -160, 40, 300, { .fields = { 64, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, 96, 300, { .fields = { 40, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 8 } }, -120, -120, 1000, { .fields = { 80, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -112, -112, 1000, { .fields = { 104, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 24 } }, -104, -96, 1000, { .fields = { 0, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 80 } }, -104, -72, 1000, { .fields = { 56, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -104, 8, 1000, { .fields = { 112, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 32 } }, -88, 24, 980, { .fields = { 24, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -88, -104, 1575, { .fields = { 104, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 80 } }, -80, -88, 1575, { .fields = { 72, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 32 } }, -96, -8, 1575, { .fields = { 0, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, -104, 1612, { .fields = { 112, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 72 } }, 0, -80, 1612, { .fields = { 56, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 24 } }, -16, -8, 1712, { .fields = { 80, 24 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 88 } }, -88, -8, 850, { .fields = { 96, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 112 } }, -96, -8, 825, { .fields = { 112, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 128 } }, -104, -8, 700, { .fields = { 120, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 120 } }, -112, 0, 625, { .fields = { 120, 128 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 104 } }, -128, 16, 475, { .fields = { 104, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 96 } }, -152, 24, 325, { .fields = { 88, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 64 } }, -72, -32, 1500, { .fields = { 56, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 64 } }, -80, -32, 1375, { .fields = { 96, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 56 } }, -88, -24, 1250, { .fields = { 48, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -48, -32, 1575, { .fields = { 24, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 40 } }, 88, 80, 600, { .fields = { 8, 104 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, 88, 64, 675, { .fields = { 104, 104 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, 80, 64, 675, { .fields = { 104, 216 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 8 } }, 72, 56, 725, { .fields = { 0, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 24 } }, 64, 64, 725, { .fields = { 8, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 8 } }, 48, 8, 875, { .fields = { 80, 184 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 56, 8 } }, 48, 16, 825, { .fields = { 72, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 64, 16 } }, 48, 24, 725, { .fields = { 64, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 24 } }, 72, 40, 825, { .fields = { 64, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 8 } }, 56, 64, 875, { .fields = { 96, 192 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, 72, 750, { .fields = { 88, 152 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, 32, 16, 950, { .fields = { 112, 88 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 32, 875, { .fields = { 0, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 32 } }, -72, 0, 1200, { .fields = { 16, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 0, 1250, { .fields = { 0, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, -32, 1350, { .fields = { 120, 248 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -64, -24, 1350, { .fields = { 88, 168 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, -8, 1350, { .fields = { 104, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 24, 16 } }, -56, 8, 1350, { .fields = { 104, 136 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 32 } }, -56, -16, 1500, { .fields = { 8, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped patio view 9.
///
/// Ranges index `_gAcropolisPatioView9SpriteSources` in elements. The
/// zero first count keeps decoded image strips enabled; `SPRITE_BATCH_END`
/// ends the list. The loaded patio overlay owns these writable records.
static SpriteBatch _gAcropolisPatioView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 2, 0 } },
    { 5, 6, 0, 0, { 7, 0 } },
    { 11, 3, 0, 0, { 6, 0 } },
    { 14, 3, 0, 0, { 8, 0 } },
    { 17, 6, 0, 0, { 0, 0 } },
    { 23, 3, 0, 0, { 9, 0 } },
    { 26, 1, 0, 0, { 5, 0 } },
    { 27, 5, 0, 0, { 10, 0 } },
    { 32, 6, 0, 0, { 1, 0 } },
    { 38, 2, 0, 0, { 11, 0 } },
    { 40, 2, 0, 0, { 4, 0 } },
    { 42, 4, 0, 0, { 12, 0 } },
    { 46, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only sprite batches for mapped patio view 10.
///
/// The zero first count keeps decoded image strips enabled; the next record
/// ends the list. The view retains this address in its empty source slot,
/// which supplies no source sprites. Storage lives with the patio overlay.
static SpriteBatch _gAcropolisPatioView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only sprite batches for mapped patio view 11.
///
/// The zero first count keeps decoded image strips enabled; the next record
/// ends the list. The view retains this address in its empty source slot,
/// which supplies no source sprites. Storage lives with the patio overlay.
static SpriteBatch _gAcropolisPatioView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only sprite batches for mapped patio view 12.
///
/// The zero first count keeps decoded image strips enabled; the next record
/// ends the list. The view retains this address in its empty source slot,
/// which supplies no source sprites. Storage lives with the patio overlay.
static SpriteBatch _gAcropolisPatioView12SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185EF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batches for mapped patio view 14.
///
/// A zero-count first batch enables decoded image strips; the next record ends
/// the list. The view retains this overlay-owned address in `sources.empty`.
static SpriteBatch _gAcropolisPatioView14SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batches for mapped patio view 15.
///
/// A zero-count first batch enables decoded image strips; the next record ends
/// the list. The view retains this overlay-owned address in `sources.empty`.
static SpriteBatch _gAcropolisPatioView15SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batches for mapped patio view 16.
///
/// A zero-count first batch enables decoded image strips; the next record ends
/// the list. The view retains this overlay-owned address in `sources.empty`.
static SpriteBatch _gAcropolisPatioView16SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_patio_80185F38[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Twenty depth-sorted background sprite rectangles for mapped patio view 18.
///
/// Uses opaque 8-bit atlas page 2 and its view-bundle palette at CLUT (0, 255).
/// Positions/dimensions are screen pixels; every rectangle sorts at depth 800.
/// The batch selects the whole array, with no source sentinel. These mutable
/// overlay-owned records must outlive packet initialization and drawing.
static SpriteSource _gAcropolisPatioView18SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, 80, 800, { .fields = { 88, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, 80, 800, { .fields = { 88, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -80, 80, 800, { .fields = { 88, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 40 } }, -40, 80, 800, { .fields = { 120, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 64, 800, { .fields = { 120, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, 40, 800, { .fields = { 88, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, 40, 800, { .fields = { 88, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -80, 40, 800, { .fields = { 80, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, 0, 800, { .fields = { 48, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, 0, 800, { .fields = { 48, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -80, 0, 800, { .fields = { 48, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, -40, 800, { .fields = { 8, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, -40, 800, { .fields = { 48, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -80, -40, 800, { .fields = { 40, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, -80, 800, { .fields = { 16, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, -80, 800, { .fields = { 8, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 40 } }, -80, -80, 800, { .fields = { 56, 120 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, -120, 800, { .fields = { 8, 40 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, -120, 800, { .fields = { 8, 200 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 40 } }, -80, -120, 800, { .fields = { 8, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Background strips plus the full source-sprite range for mapped patio view 18.
///
/// The zero first count enables decoded strips; the second batch selects all
/// twenty source elements, then SPRITE_BATCH_END terminates the list.
/// The patio overlay owns these mutable records through packet use.
static SpriteBatch _gAcropolisPatioView18SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisPatioView18SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Thirty depth-sorted background sprite rectangles for mapped patio view 19.
///
/// Uses opaque 8-bit atlas page 2 and its view-bundle palette at CLUT (0, 255).
/// Positions/dimensions are screen pixels; sorting depths range from 750 to 2000.
/// The batch selects the whole array, with no source sentinel. These mutable
/// overlay-owned records must outlive packet initialization and drawing.
static SpriteSource _gAcropolisPatioView19SpriteSources[] = {
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, -16, 1500, { .fields = { 80, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -120, 750, { .fields = { 64, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -120, 750, { .fields = { 64, 16 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -96, -120, 750, { .fields = { 64, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -104, 750, { .fields = { 88, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -104, 750, { .fields = { 96, 224 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -96, -104, 750, { .fields = { 96, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -88, 750, { .fields = { 88, 128 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -88, 750, { .fields = { 64, 240 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -96, -88, 750, { .fields = { 56, 224 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -72, 1000, { .fields = { 56, 128 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -72, 1000, { .fields = { 64, 192 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 40, 16 } }, -96, -72, 1000, { .fields = { 56, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -56, 1000, { .fields = { 64, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -56, 1000, { .fields = { 64, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, -56, 1000, { .fields = { 80, 64 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -40, 1250, { .fields = { 96, 80 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -40, 1250, { .fields = { 96, 96 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, -40, 1250, { .fields = { 80, 48 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -24, 1500, { .fields = { 96, 0 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -24, 1500, { .fields = { 96, 16 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, -24, 1500, { .fields = { 80, 32 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -8, 1750, { .fields = { 96, 176 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -8, 1750, { .fields = { 96, 192 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, -8, 1750, { .fields = { 80, 208 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, -8, 1750, { .fields = { 120, 160 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 8 } }, -160, 8, 2000, { .fields = { 56, 168 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, 8, 2000, { .fields = { 96, 112 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 8, 2000, { .fields = { 120, 128 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
    { ACROPOLIS_PATIO_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_PATIO_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, 8, 2000, { .fields = { 80, 144 } }, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_NEUTRAL_RGB, ACROPOLIS_PATIO_SPRITE_OPAQUE },
};

/// Background strips plus the full source-sprite range for mapped patio view 19.
///
/// The zero first count enables decoded strips; the second batch selects all
/// thirty source elements, then SPRITE_BATCH_END terminates the list.
/// The patio overlay owns these mutable records through packet use.
static SpriteBatch _gAcropolisPatioView19SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisPatioView19SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisPatioSpriteViews[ACROPOLIS_PATIO_VIEW_COUNT] = {
    { { .empty = _gAcropolisPatioView1SpriteBatches }, _gAcropolisPatioView1SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView2SpriteSources }, _gAcropolisPatioView2SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView3SpriteSources }, _gAcropolisPatioView3SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView4SpriteSources }, _gAcropolisPatioView4SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView5SpriteBatches }, _gAcropolisPatioView5SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView6SpriteSources }, _gAcropolisPatioView6SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView7SpriteSources }, _gAcropolisPatioView7SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView8SpriteSources }, _gAcropolisPatioView8SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView9SpriteSources }, _gAcropolisPatioView9SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView10SpriteBatches }, _gAcropolisPatioView10SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView11SpriteBatches }, _gAcropolisPatioView11SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView12SpriteBatches }, _gAcropolisPatioView12SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView7SpriteSources }, _gAcropolisPatioView7SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView14SpriteBatches }, _gAcropolisPatioView14SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView15SpriteBatches }, _gAcropolisPatioView15SpriteBatches, NULL },
    { { .empty = _gAcropolisPatioView16SpriteBatches }, _gAcropolisPatioView16SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView4SpriteSources }, _gAcropolisPatioView4SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView18SpriteSources }, _gAcropolisPatioView18SpriteBatches, NULL },
    { { .elements = _gAcropolisPatioView19SpriteSources }, _gAcropolisPatioView19SpriteBatches, NULL },
};

/// Point lights shared by the three patio room entries in every view.
///
/// Positions and falloff radii use integer room coordinates; RGB uses 12
/// fractional bits. The loaded overlay owns these writable records: coordinate
/// updates fill their parent and transform caches, and queries overwrite attenuation.
static WorldCoordPointLight _gAcropolisPatioPointLights[] = {
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -700, -200, 2400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -700, -200, 2400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2211, 2129 },
        },
        .inner = 150,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -3147, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -3147, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 200,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5250, -3150, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5250, -3150, -3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2293, 2293, 2211 },
        },
        .inner = 200,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8050, -200, -50 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8050, -200, -50 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -200, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -200, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 5250,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -200, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -200, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2457, 2375 },
        },
        .inner = 150,
        .outer = 4200,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3900, -200, 800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3900, -200, 800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2457, 2375, 2211 },
        },
        .inner = 150,
        .outer = 4200,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5050, -200, -2950 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3194, 3194, 3112 },
        },
        .inner = 250,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5050, -200, -2950 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3194, 3194, 3112 },
        },
        .inner = 250,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1050, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1050, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2867, 2785, 2785 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1600, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1600, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2867, 2785, 2785 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4300, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4300, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2949, 2867, 2867 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7200, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 3031, 2949, 2949 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7200, -3490, -2150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color     = { 2949, 2867, 2867 },
        },
        .inner = 500,
        .outer = 5000,
    },
};

/// Point-light collection shared by all three patio rooms.
///
/// Borrows the patio's 24 mutable point lights, whose view filters apply during
/// shading; there are no directional or cone lights. The record and light array
/// remain live only while the patio overlay is loaded.
static WorldCoordRoomLights _gAcropolisPatioRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisPatioPointLights),
    .pointLights           = _gAcropolisPatioPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

ViewCamera gAcropolisPatioViewCameras[ACROPOLIS_PATIO_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 1800, 25000, 0 } }, 418 },
    { { { { 592, 0, -4052 }, { -336, 4081, -49 }, { 4038, 340, 590 } }, { 1580, 1410, 820 } }, 257 },
    { { { { 808, 0, -4015 }, { -361, 4079, -72 }, { 3999, 368, 805 } }, { 6730, 1610, 770 } }, 230 },
    { { { { 752, 0, 4026 }, { 459, 4069, -85 }, { -3999, 467, 747 } }, { 120, 1700, 660 } }, 257 },
    { { { { 613, 0, 4049 }, { 902, 3992, -136 }, { -3947, 912, 597 } }, { 2789, 1999, 659 } }, 257 },
    { { { { -3922, 0, -1180 }, { -135, 4069, 449 }, { 1173, 469, -3896 } }, { -3690, 1390, -1590 } }, 257 },
    { { { { -4025, 0, 756 }, { 139, 4025, 744 }, { -743, 757, -3956 } }, { 6520, 1860, -1120 } }, 246 },
    { { { { -511, 0, -4063 }, { -552, 4057, 69 }, { 4026, 557, -507 } }, { 8420, 1740, 3180 } }, 240 },
    { { { { -708, 0, -4034 }, { -947, 3981, 166 }, { 3921, 961, -688 } }, { 4300, 1970, 2910 } }, 240 },
    { { { { -106, 0, -4094 }, { -2554, 3200, 66 }, { 3199, 2555, -83 } }, { 250, 2210, 2790 } }, 289 },
    { { { { -4064, 0, -505 }, { -41, 4082, 331 }, { 503, 333, -4051 } }, { 5289, 2010, 2850 } }, 257 },
    { { { { -106, 0, -4094 }, { -2554, 3200, 66 }, { 3199, 2555, -83 } }, { 250, 2209, 2789 } }, 289 },
    { { { { -4025, 0, 756 }, { 139, 4025, 744 }, { -743, 757, -3956 } }, { 6520, 1860, -1120 } }, 246 },
    { { { { 1658, 0, -3745 }, { -2445, 3102, -1083 }, { 2836, 2674, 1256 } }, { 8700, 1400, 10 } }, 348 },
    { { { { 2052, 0, -3544 }, { 824, 3983, 477 }, { 3447, -952, 1995 } }, { 8980, 355, 460 } }, 235 },
    { { { { -3617, 0, -1920 }, { 592, 3896, -1116 }, { 1826, -1263, -3441 } }, { 8540, 95, -2040 } }, 289 },
    { { { { 752, 0, 4026 }, { 459, 4069, -85 }, { -3999, 467, 747 } }, { 120, 1700, 660 } }, 257 },
    { { { { 491, 0, 4066 }, { 4013, 659, -485 }, { -654, 4042, 79 } }, { 5020, 6500, 1060 } }, 304 },
    { { { { 362, 0, 4079 }, { -1549, 3789, 137 }, { -3774, -1555, 335 } }, { 220, 450, 460 } }, 216 },
};

/// Cue-2 footstep bases in the loaded type-1 bank; cue 1 uses the next entry.
enum {
    ACROPOLIS_PATIO_CLASS4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    ACROPOLIS_PATIO_CLASS4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
    ACROPOLIS_PATIO_CLASS4_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
};

/// Walk, run and scripted-jump footstep pairs for patio surface class 4.
///
/// Player animation cues select the base or base + 1; companions add 100.
/// The surface policy borrows this record only while the patio overlay is loaded.
static WorldCollisionFootstepSounds _gAcropolisPatioClass4FootstepSounds = {
    .walk         = ACROPOLIS_PATIO_CLASS4_WALK_SOUND,
    .run          = ACROPOLIS_PATIO_CLASS4_RUN_SOUND,
    .scriptedJump = ACROPOLIS_PATIO_CLASS4_JUMP_SOUND,
};

/// Collision policy for patio surface classes 0 and 5..7.
///
/// Blocks probes, allows weapon impacts and applies grid pushback, without footstep cues.
/// The class directory borrows this record for the loaded patio overlay.
static WorldCollisionSurfaceProperties _gAcropolisPatioDefaultSurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for patio surface class 1.
///
/// Passes probes, ignores weapon impacts and applies grid pushback, without footstep cues.
/// The class directory borrows this record for the loaded patio overlay.
static WorldCollisionSurfaceProperties _gAcropolisPatioClass1SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for patio surface class 2.
///
/// Passes probes, ignores weapon impacts and applies grid pushback, without footstep cues.
/// The class directory borrows this record for the loaded patio overlay.
static WorldCollisionSurfaceProperties _gAcropolisPatioClass2SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for patio surface class 3.
///
/// Blocks probes, ignores weapon impacts and applies grid pushback, without footstep cues.
/// The class directory borrows this record for the loaded patio overlay.
static WorldCollisionSurfaceProperties _gAcropolisPatioClass3SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for patio surface class 4.
///
/// Blocks probes, allows weapon impacts and applies grid pushback, with footstep cues.
/// The class directory borrows this record for the loaded patio overlay.
static WorldCollisionSurfaceProperties _gAcropolisPatioClass4SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisPatioClass4FootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisPatioSurfacePropertiesByClass[ACROPOLIS_PATIO_SURFACE_CLASS_COUNT + 1] = {
    &_gAcropolisPatioDefaultSurfaceProperties,
    &_gAcropolisPatioClass1SurfaceProperties,
    &_gAcropolisPatioClass2SurfaceProperties,
    &_gAcropolisPatioClass3SurfaceProperties,
    &_gAcropolisPatioClass4SurfaceProperties,
    &_gAcropolisPatioDefaultSurfaceProperties,
    &_gAcropolisPatioDefaultSurfaceProperties,
    &_gAcropolisPatioDefaultSurfaceProperties,
    NULL,
};

/// Registers the patio room receiver and restores its story-dependent actor setup.
///
/// Requires a live state-0 task and loaded room resources. Before story progress
/// 2, room 1 restores actor 0 and starts the player's head-aim controller; actor 1
/// is restored when present. Placement variants select additional actor commands.
/// Publishes `GAME_TASK_SLOT_ROOM` and advances to idle. The repeated opening-
/// progress reads are retained; command storage is borrowed through dispatch.
static void _acropolisPatioInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_PLAYER_HEAD_AIM_TASK = 2,
    };

    ActorCommand actorCommand;
    Task*        placedActor;

    task->msgTable = D_acropolis_patio_8018028C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_PATIO_STORY_BATTLE_FINISHED) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room == 1) {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLACE, &_gAcropolisPatioRestoredSwatPlacement, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gAcropolisPatioSwatAnimation3Command, 0);
            taskMessageDispatch(sceneFindPlacedActor(0), ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            _gAcropolisPatioPlayerHeadAimTask = taskSpawnFromTable(_gAcropolisPatioSceneTaskDescriptors, ACROPOLIS_PATIO_PLAYER_HEAD_AIM_TASK, 0, 0);
        }
        placedActor = sceneFindPlacedActor(1);
        if (placedActor != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLACE, &_gAcropolisPatioRestoredStrangerPlacement, 0);
        }
    }
    if ((gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD)) {
        placedActor = sceneFindPlacedActor(1);
        if (placedActor != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_COMMAND_MESSAGE_APPLY, &D_acropolis_patio_80180440, 0);
        }
    }
    // Commands are borrowed only through these synchronous dispatches.
    if ((gGameSession->location.loc.variant == 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PATIO_026) == ACROPOLIS_PATIO_SCENE_UNSEEN)) {
        actorCommand.context.loc.stage = GAME_STAGE_ACROPOLIS;
        actorCommand.context.loc.area  = GAME_AREA_ACROPOLIS_PATIO;
        actorCommand.command           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(2), ACTOR_COMMAND_MESSAGE_APPLY, &actorCommand, 0);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(3), ACTOR_COMMAND_MESSAGE_APPLY, &actorCommand, 0);
    }
    task->state = task->state + 1;
}

/// Starts the cafeteria-door choice and records its deferred arrival selectors.
///
/// Borrows a complete transition request for this call; retains no pointer.
/// Marks the Parthenon Key identified and copies warp/room bytes after the
/// spawn, even on allocation failure. The singleton arrival must remain intact
/// until the accepted door scene commits it to the live save for reload.
static inline void _acropolisPatioStartCafeteriaDoorSequence(const RoomEventMsg* request)
{
    enum {
        ACROPOLIS_PATIO_CAFETERIA_DOOR_TASK = 1,
    };

    taskSpawnFromTable(_gAcropolisPatioSceneTaskDescriptors, ACROPOLIS_PATIO_CAFETERIA_DOOR_TASK, 0, 0);
    itemSetIdentified(INVENTORY_COLLECTION_ID_PARTHENON_KEY, 1);
    _gAcropolisPatioCafeteriaArrivalWarp = request->warp;
    _gAcropolisPatioCafeteriaArrivalRoom = request->room;
}

/// Resolves patio exits, including the scripted cafeteria-door departure.
///
/// Copies complete borrowed request/reply records, which may alias. Returns
/// 0 for a blocked exit, 1 for an immediate transition or 2 for a deferred CAP/
/// event/door sequence. Queries suppress effects and leave destination selectors
/// copied. Execution gates cafeteria/fountain exits on story flags, identifies
/// the Parthenon Key before the door sequence and saves its arrival for reload.
/// Allowed cafeteria rooms follow progress; receiver and message ID are unused.
static s32 _acropolisPatioResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_PATIO_TRANSITION_BLOCKED  = 0,
        ACROPOLIS_PATIO_TRANSITION_ALLOWED  = 1,
        ACROPOLIS_PATIO_TRANSITION_DEFERRED = 2,
    };

    s32 transitionResult;
    u16 destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_FOUNTAIN) {
        if ((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_PATIO_SECOND_SECURITY_LOCK_BIT) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            reply->room = 2;
        }
    }
    if (request->areaId == GAME_AREA_ACROPOLIS_CAFETERIA) {
        if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE) < ACROPOLIS_PATIO_DOOR_UNLOCKED) {
            transitionResult = ACROPOLIS_PATIO_TRANSITION_BLOCKED;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(3);
                gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, ACROPOLIS_PATIO_DOOR_ATTEMPTED);
                gameFlagSetNibbleIfPresent(request->flagId, 2);
                return ACROPOLIS_PATIO_TRANSITION_BLOCKED;
            }
            return transitionResult;
        }
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == ACROPOLIS_PATIO_STORY_BATTLE_FINISHED) {
            transitionResult = ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN) == ACROPOLIS_PATIO_SCENE_UNSEEN) {
                    evsStartScriptWithSkip(D_acropolis_patio_80180DEC, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_80180EDC);
                    gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN, ACROPOLIS_PATIO_SCENE_SEEN);
                    return ACROPOLIS_PATIO_TRANSITION_DEFERRED;
                }
                capRunCommandWithTransition(8);
                return ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            }
            return transitionResult;
        }
        if (gameFlagGetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE) == ACROPOLIS_PATIO_DOOR_UNLOCKED) {
            transitionResult = ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Save the requested arrival for the deferred cafeteria reload.
                _acropolisPatioStartCafeteriaDoorSequence(request);
                return ACROPOLIS_PATIO_TRANSITION_DEFERRED;
            }
            return transitionResult;
        }
    }
    if ((request->areaId == GAME_AREA_ACROPOLIS_FOUNTAIN) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_PATIO_STORY_FOUNTAIN_READY)) {
        transitionResult = ACROPOLIS_PATIO_TRANSITION_BLOCKED;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, 2);
            capRunCommandWithTransition(4);
            return ACROPOLIS_PATIO_TRANSITION_BLOCKED;
        }
        return transitionResult;
    }
    if ((request->queryOnly == ROOM_EVENT_EXECUTE) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) == ACROPOLIS_PATIO_OPENING_SCENE_STARTED)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, ACROPOLIS_PATIO_OPENING_SCENE_FINISHED);
    }
    destinationArea  = request->areaId;
    transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
    if (destinationArea == GAME_AREA_ACROPOLIS_CAFETERIA) {
        transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) >= ACROPOLIS_PATIO_STORY_FOLLOW_UP_STARTED) {
                reply->room = destinationArea;
            }
            transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
            if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == ACROPOLIS_PATIO_STORY_BATTLE_FINISHED) {
                reply->room      = 3;
                transitionResult = ACROPOLIS_PATIO_TRANSITION_ALLOWED;
            }
        }
    }
    return transitionResult;
}

/// Transfers the accepted door arrival to captured-frame reload and releases its task.
///
/// Requires the deferred warp/room bytes and a live bodyless door-scene task.
/// Stops nonambient sound before committing the cafeteria destination to the
/// live save; room-byte assignment retains its bit pattern in the unsigned saved
/// selector. Selects sprite variant 1 and kills this task after requesting
/// captured-frame reload, including when reload allocation fails.
static inline void _acropolisPatioCommitCafeteriaDeparture(Task* task)
{
    enum { ACROPOLIS_PATIO_CAFETERIA_SPRITE_VARIANT = 1 };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_ACROPOLIS_CAFETERIA;
    gDisplayState.spriteVariant                                = ACROPOLIS_PATIO_CAFETERIA_SPRITE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = _gAcropolisPatioCafeteriaArrivalWarp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = _gAcropolisPatioCafeteriaArrivalRoom;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Resolves the cafeteria-door choice and reloads the saved destination on acceptance.
///
/// Starts bodyless in state 0 after the transition resolver records warp/room.
/// Holds player control for CAP command 3. Choice key 2 marks the door open and
/// plays patio sound 4; another choice restores control and kills the task.
/// After the sound ends, stops nonambient sound and starts captured-frame reload
/// into the cafeteria, transferring the saved arrival before releasing this task.
static void _acropolisPatioCafeteriaDoorSceneTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_DOOR_SOUND_CUE = 4,
        ACROPOLIS_PATIO_DOOR_OPEN      = 3,
    };

    enum {
        ACROPOLIS_PATIO_DOOR_START          = 0,
        ACROPOLIS_PATIO_DOOR_WAIT_CHOICE    = 1,
        ACROPOLIS_PATIO_DOOR_RESOLVE_CHOICE = 2,
        ACROPOLIS_PATIO_DOOR_WAIT_SOUND     = 3,
        ACROPOLIS_PATIO_DOOR_CAP_COMMAND    = 3,
    };

    s32 state;

    state = task->state;
    switch (state) {
        case ACROPOLIS_PATIO_DOOR_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(ACROPOLIS_PATIO_DOOR_CAP_COMMAND);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_DOOR_WAIT_CHOICE:
            task->state = ACROPOLIS_PATIO_DOOR_RESOLVE_CHOICE;
            return;
        case ACROPOLIS_PATIO_DOOR_RESOLVE_CHOICE:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, ACROPOLIS_PATIO_DOOR_OPEN);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, ACROPOLIS_PATIO_DOOR_SOUND_CUE), 0, 0);
                task->state = task->state + 1;
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
        case ACROPOLIS_PATIO_DOOR_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, ACROPOLIS_PATIO_DOOR_SOUND_CUE)) != 0) {
                return;
            }
            _acropolisPatioCommitCafeteriaDeparture(task);
            return;
    }
}

/// Starts the patio's opening, follow-up or second-visit action script once eligible.
///
/// Borrows a complete read-only `DirectionActionRequest` through synchronous
/// dispatch and retains no pointer. Actions 0/1 advance opening progress; action
/// 1 also waits for placed actor 1 to leave. Action 2 latches the second-visit
/// event. The receiver, ID and second word are unused. The binary leaves the
/// return word unset; the room-action sender ignores it.
static s32 _acropolisPatioHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg)
{
    enum {
        ACROPOLIS_PATIO_ACTION_OPENING      = 0,
        ACROPOLIS_PATIO_ACTION_FOLLOW_UP    = 1,
        ACROPOLIS_PATIO_ACTION_SECOND_VISIT = 2,
    };

    u8 actionId;

    if ((actionRequest->actionId == ACROPOLIS_PATIO_ACTION_OPENING) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < ACROPOLIS_PATIO_OPENING_SCENE_THRESHOLD)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, ACROPOLIS_PATIO_OPENING_SCENE_STARTED);
        evsStartScriptWithSkip(D_acropolis_patio_80180484, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_801806AC);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 3;
        gGameSession->flowFlags                             = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    if ((actionRequest->actionId == ACROPOLIS_PATIO_ACTION_FOLLOW_UP) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) == ACROPOLIS_PATIO_OPENING_SCENE_STARTED) &&
        (taskMessageDispatch(sceneFindPlacedActor(1), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, ACROPOLIS_PATIO_OPENING_SCENE_FINISHED);
        evsStartScriptWithSkip(D_acropolis_patio_8018082C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_80180C64);
    }
    actionId = actionRequest->actionId;
    if ((actionId == ACROPOLIS_PATIO_ACTION_SECOND_VISIT) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PATIO_026) == ACROPOLIS_PATIO_SCENE_UNSEEN) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == actionId)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_PATIO_026, ACROPOLIS_PATIO_SCENE_SEEN);
        evsStartScriptWithSkip(D_acropolis_patio_8018280C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_patio_80182BE4);
    }
    // No result is set; the sender of a room action ignores it.
}
/// Handles patio CAP event commands 1 and 2.
///
/// Command 1 tries event 1 and returns 2. Command 2 tries event 2 only before
/// story progress 2 and returns 0 either way; other commands return 2. Event
/// admission failures are ignored. Receiver, ID and second word are unused.
static s32 _acropolisPatioHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        ACROPOLIS_PATIO_COMMAND_EVENT_1 = 1,
        ACROPOLIS_PATIO_COMMAND_EVENT_2 = 2,
    };

    s32 reply;

    if (commandId == ACROPOLIS_PATIO_COMMAND_EVENT_1) {
        capSpawnEventIfIdle(ACROPOLIS_PATIO_COMMAND_EVENT_1, CAP_EVENT_NO_FLAGS);
    }
    reply = 2;
    if (commandId == ACROPOLIS_PATIO_COMMAND_EVENT_2) {
        reply = gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_PATIO_STORY_BATTLE_FINISHED;
        if (reply != 0) {
            capSpawnEventIfIdle(ACROPOLIS_PATIO_COMMAND_EVENT_2, CAP_EVENT_NO_FLAGS);
            reply = 0;
        }
    }
    return reply;
}

/// Refuses every key-item-use request (message 0x13F1), returning zero to the inventory.
///
/// The receiver, selected item ID and second payload word are unused.
static s32 _acropolisPatioRefuseKeyItemUse(Task* roomTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_PATIO_KEY_ITEM_REFUSED = 0 };

    return ACROPOLIS_PATIO_KEY_ITEM_REFUSED;
}

/// Handles the room sound message: cue 3 queues area-bank entry 3; all cues return zero.
///
/// Queue admission failures are ignored. The receiver and second payload word are unused.
static s32 _acropolisPatioHandleSoundMessage(Task* roomTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum { ACROPOLIS_PATIO_SOUND_CUE = 3 };

    if (soundCue == ACROPOLIS_PATIO_SOUND_CUE) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, ACROPOLIS_PATIO_SOUND_CUE), 0, 0);
    }
    return 0;
}
/// Runs patio CAP command 6 and records choice key 1 in the shared scene flag.
///
/// Starts bodyless in state 0. Advances through one waiting tick, then tests
/// the CAP variant key, conditionally sets `GAME_FLAG_015` and kills the task.
/// The flag's wider story meaning is unproven; other choice keys leave it alone.
static void _acropolisPatioRecordSceneChoiceTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_CAP6_START         = 0,
        ACROPOLIS_PATIO_CAP6_WAIT_CHOICE   = 1,
        ACROPOLIS_PATIO_CAP6_RECORD_CHOICE = 2,
        ACROPOLIS_PATIO_CAP6_COMMAND       = 6,
    };

    s32 state;

    state = task->state;
    switch (state) {
        case ACROPOLIS_PATIO_CAP6_START:
            capRunCommandWithTransition(ACROPOLIS_PATIO_CAP6_COMMAND);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_CAP6_WAIT_CHOICE:
            task->state = ACROPOLIS_PATIO_CAP6_RECORD_CHOICE;
            return;
        case ACROPOLIS_PATIO_CAP6_RECORD_CHOICE:
            if (capGetVariantKey() == 1) {
                gameFlagSetNibble(GAME_FLAG_015, 1);
            }
            taskKill(task);
            return;
    }
}

/// Aims the player's head at a scripted world point, optionally sliding it along Z.
///
/// States 0/1 initialize and idle, 2 holds the point (-8000, 0, 900), 3 starts
/// the slide, and 4 moves it 50 units per tick to Z -3196. `spawnArg1.value`
/// stores the slide distance (0..4096). Requires the live player's five-part
/// head chain; the task neither owns nor releases the player.
static void _acropolisPatioPlayerHeadAimTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_HEAD_AIM_INIT           = 0,
        ACROPOLIS_PATIO_HEAD_AIM_IDLE           = 1,
        ACROPOLIS_PATIO_HEAD_AIM_HOLD           = 2,
        ACROPOLIS_PATIO_HEAD_AIM_START_SLIDE    = 3,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE          = 4,
        ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW        = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH      = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP     = 50,
        ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE = 4096
    };

    GfxCoord focus;        // World point carried in the translation; the other fields are unused
    byte     unused[0x28]; // Frame space no instruction touches; what the original declared here is unproven
    Task*    playerTask;
    s32      slideDistance;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    // The head turn reads nothing of its target frame but the translation, a
    // world point, so the rest of `focus` stays uninitialised.
    focus.coord.t[0] = -0x1F40;
    focus.coord.t[1] = 0;
    focus.coord.t[2] = 0x384;

    switch (task->state) {
        case ACROPOLIS_PATIO_HEAD_AIM_INIT:
            task->spawnArg1.value = 0;
            task->state           = task->state + 1;
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_IDLE:
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_HOLD:
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_START_SLIDE:
            task->spawnArg1.value = 0;
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_PATIO_HEAD_AIM_SLIDE:
            slideDistance         = task->spawnArg1.value + ACROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP;
            task->spawnArg1.value = slideDistance;
            if (slideDistance >= ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE + 1) {
                task->spawnArg1.value = ACROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE;
            }
            focus.coord.t[2] -= task->spawnArg1.value;
            animationAimHeadAtPoint(playerTask, &focus, ACROPOLIS_PATIO_HEAD_AIM_MAX_YAW, ACROPOLIS_PATIO_HEAD_AIM_MAX_PITCH, ONE);
            return;
    }
}

/// Selects the opening scene's player-head-aim phase without resetting its progress.
///
/// The retained aim task must already be spawned and live. Script choices are
/// 1 idle, 2 hold the fixed world point, and 3 begin its Z slide. The task itself
/// resets the slide distance when phase 3 next runs; this setter only stores
/// the supplied signed-word state.
static void _acropolisPatioSetPlayerHeadAimState(s32 headAimState)
{
    _gAcropolisPatioPlayerHeadAimTask->state = headAimState;
}

/// Selects room 2 in the live and saved location and requests room-object relinking.
static void _acropolisPatioActivateRoom2(void)
{
    enum { ACROPOLIS_PATIO_ROOM_AFTER_SCENE = 2 };

    gGameSession->location.loc.room = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACROPOLIS_PATIO_ROOM_AFTER_SCENE;
    gGameSession->roomObjsDirty                                                                  = true;
}

/// Sets the scene's actor update/visibility control from an event-script byte.
///
/// Accepts `SCENE_COMBAT_ACTORS_*`; the patio scripts use paused and running.
static void _acropolisPatioSetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

/// Keeps the room task alive without per-frame work; `task` is unused.
static void _acropolisPatioRoomIdleState(Task* task)
{
    byte unused[0x10]; // Untouched local storage retained for the original stack frame; its role is unproven
}

void acropolisPatioRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_acropolis_patio_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

/// Pauses the patio's actors or resumes them and engages battle.
///
/// Event scripts pass zero to pause, or nonzero to resume actor updates and
/// latch an idle battle as engaged. Pausing leaves the battle phase intact.
/// Requires the live scene-combat state; no task or payload is retained.
static void _acropolisPatioSetBattleActive(s32 battleActive)
{
    if (battleActive != 0) {
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
        sceneEngageBattle(1);
        return;
    }
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
}
/// Starts the live player's half-turn left for the patio event script.
///
/// Allocates a bodyless controller; its sixteen ticks update the player's
/// yaw before teardown. The player must remain live through those updates.
/// Allocation failure leaves no controller and is ignored by the script.
static void _acropolisPatioSpawnPlayerTurnLeft(void)
{
    taskSpawnFromTable(&_gAcropolisPatioPlayerTurnLeftTaskDescriptor, 0, 0, 0);
}

/// Turns the live player left by half a turn over 16 task ticks, then kills itself.
///
/// Yaw uses 4096 units per turn. States 0/1 start and turn; any other state
/// kills this bodyless controller. `killCountdown` holds the remaining ticks.
/// Start in state 0 with player yaw in [-2048, 2048).
static void _acropolisPatioPlayerTurnLeftTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_PLAYER_TURN_INIT     = 0,
        ACROPOLIS_PATIO_PLAYER_TURN_STEP     = 1,
        ACROPOLIS_PATIO_PLAYER_TURN_TICKS    = 16,
        ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP = ACTOR_TRANSFORM_ANGLE_HALF_TURN / ACROPOLIS_PATIO_PLAYER_TURN_TICKS
    };

    GameActor* playerActor;
    s16        yaw;

    playerActor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (task->state) {
        case ACROPOLIS_PATIO_PLAYER_TURN_INIT:
            task->killCountdown = ACROPOLIS_PATIO_PLAYER_TURN_TICKS;
            task->state++;
            /* fallthrough */
        case ACROPOLIS_PATIO_PLAYER_TURN_STEP:
            yaw = playerActor->rotation.vy - ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP;
            if (yaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                yaw = playerActor->rotation.vy + (ACTOR_TRANSFORM_ANGLE_TURN - ACROPOLIS_PATIO_PLAYER_TURN_YAW_STEP);
            }
            playerActor->rotation.vy = yaw;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            taskKill(task);
            break;
    }
}

void acropolisPatioSpawnLightEffectsTask(Task* task)
{
    enum { LIGHT_EMITTER_INIT     = 0,
           LARGE_LIGHT_END        = 3,
           CEILING_LIGHT_END      = 7,
           MOTES_PER_LARGE_LIGHT  = 3,
           LARGE_LIGHT_ARGUMENT   = (768 << ACROPOLIS_PATIO_JET_SIZE_SHIFT) | (2 << ACROPOLIS_PATIO_JET_CELL_SHIFT),
           CEILING_LIGHT_ARGUMENT = 512 << ACROPOLIS_PATIO_JET_SIZE_SHIFT,
           WALL_LIGHT_ARGUMENT    = 1 << ACROPOLIS_PATIO_JET_CELL_SHIFT };
    GfxCoord*   parentCoord;
    EffectWork* scratchWork;
    s32         anchorIndex;
    s32         moteIndex;

/// Chooses a parent-relative mote position around a large-light anchor.
///
/// Requires a writable SVECTOR pointer and an anchor index 0..2. Both arguments
/// must be stable and side-effect-free: position is evaluated six times and
/// anchorIndex three times. Advances gRandomLcgState three times in XYZ order,
/// with jitter -1023..1024 coordinate units and s16 narrowing before and after
/// anchor addition. Writes only XYZ; the following effect spawn copies them.
/// Use as a statement inside a braced block.
#define ACROPOLIS_PATIO_OFFSET_LIGHT_MOTE(position, anchorIndex)                             \
    {                                                                                        \
        enum { MOTE_JITTER_CENTRE = 1024,                                                    \
               MOTE_JITTER_MASK   = 2047 };                                                    \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;    \
        (position)->vx  = MOTE_JITTER_CENTRE - ((gRandomLcgState >> 16) & MOTE_JITTER_MASK); \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;    \
        (position)->vy  = MOTE_JITTER_CENTRE - ((gRandomLcgState >> 16) & MOTE_JITTER_MASK); \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;    \
        (position)->vz  = MOTE_JITTER_CENTRE - ((gRandomLcgState >> 16) & MOTE_JITTER_MASK); \
        (position)->vx += _gAcropolisPatioLightAnchors[(anchorIndex)].vx;                    \
        (position)->vy += _gAcropolisPatioLightAnchors[(anchorIndex)].vy;                    \
        (position)->vz += _gAcropolisPatioLightAnchors[(anchorIndex)].vz;                    \
    }

    scratchWork = task->spawnArg2.pointer;
    parentCoord = task->extra.coordBody->coord;

    if (task->state == LIGHT_EMITTER_INIT) {
        // Seed all fixed light glows before allocating their moving points.
        for (anchorIndex = 0; anchorIndex < LARGE_LIGHT_END; anchorIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, parentCoord, anchorIndex + LARGE_LIGHT_ARGUMENT, &_gAcropolisPatioLightAnchors[anchorIndex]);
        }
        for (anchorIndex = LARGE_LIGHT_END; anchorIndex < CEILING_LIGHT_END; anchorIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, parentCoord, anchorIndex + CEILING_LIGHT_ARGUMENT, &_gAcropolisPatioLightAnchors[anchorIndex]);
        }
        for (anchorIndex = CEILING_LIGHT_END; anchorIndex < ARRAY_SIZE(_gAcropolisPatioLightAnchors); anchorIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET, parentCoord, anchorIndex + WALL_LIGHT_ARGUMENT, &_gAcropolisPatioLightAnchors[anchorIndex]);
        }
        task->state++;
        // Each child snapshots XYZ; these point tasks never read the retained offset pointer.
        for (anchorIndex = 0; anchorIndex < LARGE_LIGHT_END; anchorIndex++) {
            for (moteIndex = 0; moteIndex < MOTES_PER_LARGE_LIGHT; moteIndex++) {
                ACROPOLIS_PATIO_OFFSET_LIGHT_MOTE(&scratchWork->move, anchorIndex);
                effectSpawn(EFFECT_ACROPOLIS_PATIO_FOUNTAIN_MIST, parentCoord, anchorIndex, &scratchWork->move);
            }
        }
    }
#undef ACROPOLIS_PATIO_OFFSET_LIGHT_MOTE
}

/// Positions a patio light glow as a square around its projected screen centre.
///
/// Borrows a writable packet and a separate, read-only `projection` for this
/// call. Only `screenPos` and `halfExtent` need initialization in the projection;
/// both count whole pixels, with a nonnegative half-extent. Centre plus/minus
/// extent must fit signed 32-bit arithmetic. Each edge narrows to signed 16 bits
/// without clipping. Vertices 0..3 are top-left, top-right, bottom-left and
/// bottom-right. Writes only the eight coordinate halfwords.
static inline void _acropolisPatioSetLightGlowBounds(POLY_FT4* glowQuad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left         = projection->screenPos.vx - projection->halfExtent;
    glowQuad->x2 = left;
    glowQuad->x0 = left;
    right        = projection->screenPos.vx + projection->halfExtent;
    glowQuad->x3 = right;
    glowQuad->x1 = right;
    top          = projection->screenPos.vy - projection->halfExtent;
    glowQuad->y1 = top;
    glowQuad->y0 = top;
    bottom       = projection->screenPos.vy + projection->halfExtent;
    glowQuad->y3 = bottom;
    glowQuad->y2 = bottom;
}

/// Projects a patio light's cached view position into a flickering additive glow.
///
/// `composedCoord->workm.t` supplies signed game coordinates, narrowed to s16
/// before projection through `GsWSMATRIX`. The caller supplies GTE projection
/// settings and a complete, word-aligned `spriteScratch` block. FLAG is not
/// tested. `glowWork->angle` selects cell/CLUT 0..2, `scale` is the size scale
/// (1..4095), and `period` is resting grey (48, 64 or 80); odd animation frames
/// add 16. Screen half-extent is scale * 39 / (SZ3 / 4), in integer pixels.
///
/// Reserves one `POLY_FT4` even when depth is below `ACROPOLIS_PATIO_JET_MIN_DEPTH`.
/// Accepted depths wrap into the current 1024-tag ordering table. The arena and
/// sprite texture/CLUT must remain live until GPU completion; inputs and scratch
/// are borrowed only for this call. Changes GTE state and uses the texture page's
/// additive blend mode without allocating a separate draw-mode command.
static inline void _acropolisPatioDrawLightGlow(const GfxCoord* composedCoord, const EffectWork* glowWork, RoomGlowSpriteScratch* spriteScratch)
{
    enum {
        ACROPOLIS_PATIO_LIGHT_GLOW_CLUT_STRIDE             = 16,
        ACROPOLIS_PATIO_LIGHT_GLOW_FRAME_PARITY_MASK       = 1,
        ACROPOLIS_PATIO_LIGHT_GLOW_DEPTH_TO_OT_BYTES_SHIFT = 2
    };

    POLY_FT4* glowQuad;
    u8        greyLevel;

    // Project the narrowed cached view translation into the sprite's centre.
    spriteScratch->worldPos.vx = composedCoord->workm.t[0];
    spriteScratch->worldPos.vy = composedCoord->workm.t[1];
    spriteScratch->worldPos.vz = composedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&spriteScratch->worldPos);
    gte_rtps();
    // Consume a packet even when the depth test will reject the projection.
    glowQuad       = gGpuPrimCursor;
    gGpuPrimCursor = glowQuad + 1;
    setPolyFT4(glowQuad);
    gte_stsxy(&spriteScratch->screenPos);
    gte_stszotz(&spriteScratch->otz);
    if (spriteScratch->otz >= ACROPOLIS_PATIO_JET_MIN_DEPTH) {
        greyLevel       = glowWork->period + (((u8)gDisplayState.animFrame & ACROPOLIS_PATIO_LIGHT_GLOW_FRAME_PARITY_MASK) << ACROPOLIS_PATIO_JET_FLICKER_SHIFT);
        glowQuad->tpage = ACROPOLIS_PATIO_JET_TEXTURE_PAGE;
        setRGB0(glowQuad, greyLevel, greyLevel, greyLevel);
        setSemiTrans(glowQuad, true);
        setClut(glowQuad, glowWork->angle * ACROPOLIS_PATIO_LIGHT_GLOW_CLUT_STRIDE, ACROPOLIS_PATIO_JET_CLUT_Y);
        // Texture endpoints include the final texel of the fixed 40-by-40 cell.
        setUVWH(glowQuad, glowWork->angle * ACROPOLIS_PATIO_JET_CELL_SIZE, 0,
                ACROPOLIS_PATIO_JET_CELL_SIZE - 1, ACROPOLIS_PATIO_JET_CELL_SIZE - 1);

        spriteScratch->halfExtent = (glowWork->scale * (ACROPOLIS_PATIO_JET_CELL_SIZE - 1)) / spriteScratch->otz;
        _acropolisPatioSetLightGlowBounds(glowQuad, spriteScratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)spriteScratch->otz << gDisplayState.otDepthShift) >> ACROPOLIS_PATIO_LIGHT_GLOW_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                glowQuad);
    }
}

void acropolisPatioFountainJetTask(Task* task)
{
    enum { ACROPOLIS_PATIO_JET_INIT = 0 };

    RoomGlowSpriteScratch* block;
    EffectWork*            work;
    GfxCoord*              coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAcropolisPatioLightViewMasks[task->spawnArg1.value & ACROPOLIS_PATIO_JET_ANCHOR_MASK] >> (gGameSession->location.loc.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == ACROPOLIS_PATIO_JET_INIT) {
            // Decode the fixed sheet cell and size only on the first visible tick.
            u8 levels[3] = { 0x50, 0x30, 0x40 };

            if (task->spawnArg1.value & (ACROPOLIS_PATIO_JET_SIZE_MASK << ACROPOLIS_PATIO_JET_SIZE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> ACROPOLIS_PATIO_JET_SIZE_SHIFT) & ACROPOLIS_PATIO_JET_SIZE_MASK;
            } else {
                work->scale = ACROPOLIS_PATIO_JET_DEFAULT_SIZE;
            }
            work->angle           = (task->spawnArg1.value >> ACROPOLIS_PATIO_JET_CELL_SHIFT) & ACROPOLIS_PATIO_JET_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_PATIO_JET_ANCHOR_MASK;
            work->period          = levels[work->angle];
            task->state           = task->state + 1;
        }
        _acropolisPatioDrawLightGlow(coord, work, block);
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// A non-zero padding byte the original toolchain left. Nothing refers to it.
///
/// It fills out the word holding the three grey levels the function above
/// initialises its local array from, so it has to stay directly below that
/// function.
static const u8 D_acropolis_patio_8017D5EB = 0xF2;

/// Projects a cached light-mote position into a random-grey, average-blended pixel.
///
/// Reads only `cachedCoord->workm.t`, in signed game units narrowed to s16.
/// This cache is the snapshot composed before the caller moved the local node;
/// a dirty composition stamp does not invalidate that snapshot for this draw.
/// Requires configured GTE projection settings, `GsWSMATRIX`, and a complete,
/// word-aligned `tileScratch` block. FLAG is not tested; depth is SZ3 / 4.
///
/// The word-aligned arena needs one `TILE_1` even on depth rejection and one
/// additional `DR_TPAGE` at `EFFECT_POINT_TILE_MIN_DEPTH` or further. Only an
/// accepted depth advances the shared LCG, selecting grey 0..191. Queues the
/// blend command before the pixel in the current 1024-tag ordering table and
/// leaves average draw mode active. Packet storage lives until GPU completion;
/// neither input nor scratch is retained. Changes GTE state.
static inline void _acropolisPatioDrawLightMote(const GfxCoord* cachedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        ACROPOLIS_PATIO_LIGHT_MOTE_GREY_LEVELS             = 192,
        ACROPOLIS_PATIO_LIGHT_MOTE_DEPTH_TO_OT_BYTES_SHIFT = 2
    };

    TILE_1* moteTile;
    u32     greyLevel;

    // Draw the pre-movement view cache; the dirty stamp defers recomposition.
    tileScratch->viewPoint.vx = cachedCoord->workm.t[0];
    tileScratch->viewPoint.vy = cachedCoord->workm.t[1];
    tileScratch->viewPoint.vz = cachedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    // Reserve the tile before rejecting depth, without consuming random state.
    moteTile       = gGpuPrimCursor;
    gGpuPrimCursor = moteTile + 1;
    setTile1(moteTile);
    gte_stsxy(&moteTile->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        greyLevel       = gRandomLcgState >> 16;
        greyLevel      %= ACROPOLIS_PATIO_LIGHT_MOTE_GREY_LEVELS;
        setRGB0(moteTile, greyLevel, greyLevel, greyLevel);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)tileScratch->depth << gDisplayState.otDepthShift) >> ACROPOLIS_PATIO_LIGHT_MOTE_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                moteTile);
        // Prepending at the same depth makes the GPU set blending before the tile.
        gpuSetPrimitiveBlendMode(moteTile, GPU_BLEND_AVERAGE, tileScratch->depth);
    }
}

void acropolisPatioFountainMistTask(Task* task)
{
    enum {
        ACROPOLIS_PATIO_MIST_INIT            = 0,
        ACROPOLIS_PATIO_MIST_DRIFT           = 0,
        ACROPOLIS_PATIO_MIST_GATHER          = 1,
        ACROPOLIS_PATIO_MIST_DRIFT_CENTRE    = 16,
        ACROPOLIS_PATIO_MIST_DRIFT_MASK      = 31,
        ACROPOLIS_PATIO_MIST_AIM_SAMPLE_MASK = 3,
        ACROPOLIS_PATIO_MIST_GATHER_WEIGHT   = ONE / 128,
        ACROPOLIS_PATIO_MIST_JITTER_MASK     = 15,
        ACROPOLIS_PATIO_MIST_JITTER_CENTRE   = 8,
        ACROPOLIS_PATIO_MIST_DRIFT_CHANCE    = 120,
        ACROPOLIS_PATIO_MIST_GATHER_CHANCE   = 60
    };

    EffectWork*             work;
    GfxCoord*               coord;
    EffectPointTileScratch* tileScratch;
    SVECTOR*                velocity;
    SVECTOR*                anchors;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAcropolisPatioLightViewMasks[task->spawnArg1.value] >> (gGameSession->location.loc.view - 1)) & 1)) {
        tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
        actorRenderComposeCoord(coord);
        if (task->state == ACROPOLIS_PATIO_MIST_INIT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            task->state++;
        }
        // Gather re-aims on a random quarter of ticks, rather than periodically.
        if (work->index != ACROPOLIS_PATIO_MIST_DRIFT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_AIM_SAMPLE_MASK) == 0) {
                anchors       = _gAcropolisPatioLightAnchors;
                velocity      = &work->move;
                work->move.vx = (u16)anchors[task->spawnArg1.value].vx -
                                (u16)coord->coord.t[0];
                work->move.vy = (u16)anchors[task->spawnArg1.value].vy -
                                (u16)coord->coord.t[1];
                work->move.vz = (u16)anchors[task->spawnArg1.value].vz -
                                (u16)coord->coord.t[2];
                VectorNormalSS(velocity, velocity);
                gte_lddp(ACROPOLIS_PATIO_MIST_GATHER_WEIGHT);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz  -= ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_JITTER_MASK) - ACROPOLIS_PATIO_MIST_JITTER_CENTRE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % ACROPOLIS_PATIO_MIST_DRIFT_CHANCE) == 0) {
                work->index = ACROPOLIS_PATIO_MIST_DRIFT;
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ACROPOLIS_PATIO_MIST_DRIFT_CENTRE - ((gRandomLcgState >> 16) & ACROPOLIS_PATIO_MIST_DRIFT_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % ACROPOLIS_PATIO_MIST_GATHER_CHANCE) == 0) {
                work->index = ACROPOLIS_PATIO_MIST_GATHER;
            }
        }
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        _acropolisPatioDrawLightMote(coord, tileScratch);
        SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    }
}
