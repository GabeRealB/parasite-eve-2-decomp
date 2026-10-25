#include "rooms/dryfield_night_driveway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_01500.h"
#include "actors/actor_02500.h"
#include "actors/actor_03700.h"

#include "gameplay/animation.h"
#include "gameplay/player_actor.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"
#include "../../shared/dryfield_driveway.h"

static void _drivewaySetViewDirty(s16 viewDirty);

static u8 _gRoomVariantDrivewayFactoryEventRequested;

/// Spawn argument of the helper task 0x31 the event task starts.
extern RoomFadeStorage gRoomEventFade;

static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void _dryfieldNightDrivewayInitializeRoom(Task* task);
static void _dryfieldNightDrivewayRoomIdle(Task* task);

enum {
    DRYFIELD_NIGHT_DRIVEWAY_ENCOUNTER_WAVE_BEGIN = 1,
    DRYFIELD_NIGHT_DRIVEWAY_PLAYER_STATE_RUNNING = 0,
};

static WorldCollisionGrid         _gDryfieldNightDrivewayCollisionGrid;
static WorldCollisionOccluder     _gDryfieldNightDrivewaySightOccluders[2];
static WorldCollisionTrigger      _gDryfieldNightDrivewayViewBoundaryTriggers[6];
static WorldCollisionTrigger      _gDryfieldNightDrivewayActionTriggers[4];
static WorldCoordRoomAmbientEntry _gDryfieldNightDrivewayAmbientByView[11];
static WorldCoordRoomLights       _gDryfieldNightDrivewayRoomLights;

static AnimationSet _gDryfieldNightDrivewayAnimation01870;
static AnimationSet _gDryfieldNightDrivewayAnimation01A84;
static AnimationSet _gDryfieldNightDrivewayAnimation01D64;
static s32          _dryfieldNightDrivewayRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32          _dryfieldNightDrivewayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandMode);
static s32          _dryfieldNightDrivewayIgnoreDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static void         _dryfieldNightDrivewaySetEncounterWave(s32 waveStage);
static void         _dryfieldNightDrivewaySetPlayerStateHold(u8 hold);

/// Bodyless priority-32 recipe for the staged factory-transition scene.
///
/// Spawns state 0 with unused arguments. The room-owned destination and event
/// snapshots must remain stable until the callback requests session reload.
static TaskDesc _gRoomEventStagedTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

static AnimationPackedPose _gDryfieldNightDrivewayAnimation0150CBank1[10] = {
#include "assets/dryfield_night_driveway_animation_0150C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation0150CBank4[95] = {
#include "assets/dryfield_night_driveway_animation_0150C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation0150CRecords[139] = {
#include "assets/dryfield_night_driveway_animation_0150C_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation0150CIndices[20] = {
#include "assets/dryfield_night_driveway_animation_0150C_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation0150C = {
    _gDryfieldNightDrivewayAnimation0150CRecords,
    _gDryfieldNightDrivewayAnimation0150CIndices,
    { NULL, _gDryfieldNightDrivewayAnimation0150CBank1, NULL, NULL, _gDryfieldNightDrivewayAnimation0150CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDrivewayAnimation01870Bank1[6] = {
#include "assets/dryfield_night_driveway_animation_01870_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation01870Bank4[75] = {
#include "assets/dryfield_night_driveway_animation_01870_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation01870Records[104] = {
#include "assets/dryfield_night_driveway_animation_01870_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation01870Indices[20] = {
#include "assets/dryfield_night_driveway_animation_01870_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation01870 = {
    _gDryfieldNightDrivewayAnimation01870Records,
    _gDryfieldNightDrivewayAnimation01870Indices,
    { NULL, _gDryfieldNightDrivewayAnimation01870Bank1, NULL, NULL, _gDryfieldNightDrivewayAnimation01870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDrivewayAnimation01A84Bank1[2] = {
#include "assets/dryfield_night_driveway_animation_01A84_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation01A84Bank4[24] = {
#include "assets/dryfield_night_driveway_animation_01A84_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation01A84Records[83] = {
#include "assets/dryfield_night_driveway_animation_01A84_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation01A84Indices[20] = {
#include "assets/dryfield_night_driveway_animation_01A84_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation01A84 = {
    _gDryfieldNightDrivewayAnimation01A84Records,
    _gDryfieldNightDrivewayAnimation01A84Indices,
    { NULL, _gDryfieldNightDrivewayAnimation01A84Bank1, NULL, NULL, _gDryfieldNightDrivewayAnimation01A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDrivewayAnimation01D64Bank1[5] = {
#include "assets/dryfield_night_driveway_animation_01D64_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation01D64Bank4[60] = {
#include "assets/dryfield_night_driveway_animation_01D64_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation01D64Records[89] = {
#include "assets/dryfield_night_driveway_animation_01D64_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation01D64Indices[20] = {
#include "assets/dryfield_night_driveway_animation_01D64_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation01D64 = {
    _gDryfieldNightDrivewayAnimation01D64Records,
    _gDryfieldNightDrivewayAnimation01D64Indices,
    { NULL, _gDryfieldNightDrivewayAnimation01D64Bank1, NULL, NULL, _gDryfieldNightDrivewayAnimation01D64Bank4, NULL, NULL, NULL },
};

/// Driveway scene recipes: slot 0 wire-rope blackout, slot 1 bat encounter, slot 2 end.
///
/// Both live recipes are bodyless priority-32 tasks with unused spawn arguments.
/// The controllers borrow this overlay's scripts and resources through completion.
static TaskDesc _gDryfieldNightDrivewaySceneTasks[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _drivewayBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _drivewayCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Encounter scene extension: player clip 47 and a null entry for clip 48.
///
/// The copy includes both words. The clip data stays borrowed from this room
/// while the equipped weapon bank plays the extension.
static AnimationSet* _gDryfieldNightDrivewayEncounterAnimationBankExtension[2] = {
    &_gDryfieldNightDrivewayAnimation0150C,
    NULL,
};

/// Installs the encounter scene's two extension words in the player bank.
///
/// Dispatch borrows the request synchronously; playback keeps borrowing
/// the installed room clip after the request has been consumed.
static AnimationBankCopyRequest _gDryfieldNightDrivewayEncounterAnimationBankCopy = {
    .sets  = _gDryfieldNightDrivewayEncounterAnimationBankExtension,
    .count = ARRAY_SIZE(_gDryfieldNightDrivewayEncounterAnimationBankExtension),
};

/// Restarts the encounter scene's player extension clip 47 with grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayEncounterPlayerAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 48; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_driveway_8017F394 = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 7,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the encounter scene's player into clip 32 over five frames, dropping grid participation.
///
/// The event interpreter copies this request and substitutes the equipped
/// weapon's bank selector before dispatch. The stored selector is not used.
/// This retained daytime sequence's controller is spawned only in stage 2.
static AnimationPlayRequest _gDryfieldNightDrivewayEncounterPlayerAnimation32 = {
    .source               = { .index = 1 },
    .animationId          = 32,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced transform-shaped record; its intended actor and placement role are unproven.
static ActorTransform D_dryfield_night_driveway_8017F3BC = {
    .pos = { -1067, 0, 1407, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Bat-encounter sequence synchronized to the three cues of CAP command 4.
///
/// Installs player clips 47/48, plays clip 32, releases the bat waves at the
/// first cue, plays clip 47 at the second and engages combat at the third.
/// The final message returns player control after an update boundary. Keep
/// the script and borrowed animation data loaded through playback.
/// Retained daytime sequence: the resolver only spawns it in stage 2.
static EvsCommand _gDryfieldNightDrivewayEncounterScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRIVEWAY_CAP_ENCOUNTER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightDrivewayEncounterAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayEncounterPlayerAnimation32 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 0x0C) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightDrivewaySetEncounterWave }, { .value = DRYFIELD_NIGHT_DRIVEWAY_ENCOUNTER_WAVE_BEGIN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayEncounterPlayerAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRIVEWAY_SCRIPT_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restarts player clip 1 without blending or world-grid participation.
///
/// Event playback copies this request and replaces source.index with the
/// equipped weapon's bank. The request is borrowed only for that copy.
static AnimationPlayRequest _gDryfieldNightDrivewayPlayerAnimation1 = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts player clip 24 at the blackout scene's final CAP cue, without grid participation.
///
/// The event interpreter copies this request and substitutes the equipped
/// weapon's bank selector before dispatch. The stored selector is not used.
/// The nighttime room has no path that spawns this retained blackout controller.
static AnimationPlayRequest _gDryfieldNightDrivewayBlackoutPlayerAnimation24 = {
    .source               = { .index = 1 },
    .animationId          = 24,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Wire-rope sequence synchronized to the three cues of CAP command 3.
///
/// Starts a one-frame subtractive fade, resumes player updates and display,
/// plays clip 1, requests saved-view reload at the second cue, then returns
/// the fade over thirty frames and plays clip 24 before restoring control.
/// The script and requests stay borrowed after the controller task has ended.
/// Retained daytime sequence: this room's key-item receiver refuses all items.
static EvsCommand _gDryfieldNightDrivewayBlackoutScript[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRIVEWAY_CAP_BLACKOUT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldNightDrivewaySetPlayerStateHold }, { .value = DRYFIELD_NIGHT_DRIVEWAY_PLAYER_STATE_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = DRIVEWAY_DISPLAY_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 9) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _drivewaySetViewDirty }, { .value = DRIVEWAY_SAVED_VIEW_RELOAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = DRIVEWAY_BLACKOUT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRIVEWAY_BLACKOUT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayBlackoutPlayerAnimation24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Wire-rope skip continuation that reloads the view and restores player control.
///
/// Waits eight frames plus one update boundary behind a primary fade, marks
/// the saved view for reload and cleans up CAP/scene state. After the fade
/// returns and another eight frames pass, the player leaves scripted control.
/// Borrowed by event playback until END; timing and cleanup order are retained.
/// The night room has no path that starts its blackout controller.
static EvsCommand _gDryfieldNightDrivewayBlackoutSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = DRIVEWAY_FADE_USE_DEFAULT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _drivewaySetViewDirty }, { .value = DRIVEWAY_SAVED_VIEW_RELOAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Night-driveway room receiver: departure resolution, refused item use, ignored actions and sound cues.
///
/// Installed for the room task's lifetime. The final sentinel makes other
/// message IDs return zero; payload meanings belong to the selected handler.
static TaskMessageEntry _gDryfieldNightDrivewayRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveDriveway },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightDrivewayRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightDrivewayIgnoreDirectionAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightDrivewayIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _drivewayScriptSound },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Initial player placement for the warp-4 companion arrival scene.
///
/// World XYZ and Euler angles in 4096 units per turn; placement consumes both
/// vectors synchronously. The final player placement retains XYZ and changes yaw.
static ActorTransform _gDryfieldNightDrivewayCompanionArrivalPlayerInitialTransform = {
    .pos = { -700, 0, 1320, 0 },
    .rot = { 0, 1365, 0, 0 },
};

/// Final player placement and yaw for the view-4 companion arrival scene.
///
/// Normal playback uses the yaw for the final turn; skipping uses the whole
/// placement. XYZ is in world units and yaw is in 4096 units per turn.
/// Message dispatch consumes the borrowed values synchronously.
static ActorTransform _gDryfieldNightDrivewayCompanionArrivalPlayerFinalTransform = {
    .pos = { -700, 0, 1320, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 },
};

/// Initial companion placement for the warp-4 arrival scene, before the first run.
///
/// Placement consumes world XYZ and Euler angles in 4096 units per turn
/// synchronously; it retains no pointer to this record.
static ActorTransform _gDryfieldNightDrivewayCompanionArrivalCompanionInitialTransform = {
    .pos = { 3300, 0, 790, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 },
};

/// Companion's first run destination and subsequent target yaw in the arrival scene.
///
/// Running copies world XYZ; the later turn copies yaw in 4096 units per turn.
/// Both messages consume their borrowed values synchronously.
static ActorTransform _gDryfieldNightDrivewayCompanionArrivalCompanionFirstStopTransform = {
    .pos = { 100, 0, 790, 0 },
    .rot = { 0, 3413, 0, 0 },
};

/// Companion's final run destination and skip placement for the view-4 arrival scene.
///
/// Running copies world XYZ and ignores rotation; skipping also applies the
/// stored Euler yaw, in 4096 units per turn. Dispatch borrows this record only
/// while the movement handler copies its destination or places the actor.
static ActorTransform _gDryfieldNightDrivewayCompanionArrivalCompanionFinalTransform = {
    .pos = { -3000, 0, 790, 0 },
    .rot = { 0, 3413, 0, 0 },
};

/// Initial player placement in the unstarted view-2 companion scene.
///
/// Placement consumes world XYZ and Euler angles in 4096 units per turn
/// synchronously. This is a separate record from the equal final placement.
static ActorTransform _gDryfieldNightDrivewayCompanionView2PlayerInitialTransform = {
    .pos = { -9527, 0, -1500, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Final player yaw and skip placement for the unstarted view-2 companion scene.
///
/// Normal playback copies only yaw; the skip stream places world XYZ and all
/// Euler angles. Angles use 4096 units per turn; neither message retains this record.
static ActorTransform _gDryfieldNightDrivewayCompanionView2PlayerFinalTransform = {
    .pos = { -9527, 0, -1500, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Initial companion placement in the unstarted view-2 scene, facing the player.
///
/// Placement consumes world XYZ and Euler angles in 4096 units per turn
/// synchronously; it retains no pointer to this record.
static ActorTransform _gDryfieldNightDrivewayCompanionView2CompanionInitialTransform = {
    .pos = { -3800, 0, -1500, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 },
};

/// Companion's first run destination and subsequent target yaw in the view-2 scene.
///
/// Running copies world XYZ; the later turn copies yaw in 4096 units per turn.
/// Both messages consume their borrowed values synchronously.
static ActorTransform _gDryfieldNightDrivewayCompanionView2CompanionFirstStopTransform = {
    .pos = { -8800, 0, -1500, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 },
};

/// Companion's final run destination and skip placement in the view-2 scene.
///
/// Running copies world XYZ and ignores rotation; the skip placement also
/// consumes Euler angles in 4096 units per turn. Dispatch retains no record pointer.
static ActorTransform _gDryfieldNightDrivewayCompanionView2CompanionFinalTransform = {
    .pos = { -3800, 0, -1500, 0 },
    .rot = { 0, 3413, 0, 0 },
};

/// Player extension clips 47..49 for the two companion scenes, followed by null.
///
/// The ten-word bank copy reads six words beyond this four-entry table:
/// all five words of the neighbouring undispatched play request and the stored
/// bank selector of `_gDryfieldNightDrivewayCompanionScenePlayerAnimation47`.
/// Preserve that source order and copied span. Only entries 47..49 contain
/// playable clip pointers; their data stays borrowed from the loaded room.
static AnimationSet* _gDryfieldNightDrivewayCompanionSceneAnimationBankExtension[4] = {
    &_gDryfieldNightDrivewayAnimation01870,
    &_gDryfieldNightDrivewayAnimation01A84,
    &_gDryfieldNightDrivewayAnimation01D64,
    NULL,
};

/// Undispatched clip-47 request whose five words are copied into the player bank.
///
/// The companion-scene copy reads this entire neighbouring record into bank
/// words 51..55, then the following request's selector into word 56. These
/// values are not clip pointers. Preserve the separate records and their order:
/// the ten-word transfer crosses the four-entry source table's established end.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionSceneCopiedAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// First player extension clip in both companion scenes: clip 47, with grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionScenePlayerAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Second player extension clip in both companion scenes: clip 48, with grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionScenePlayerAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Final player extension clip in both companion scenes: clip 49, with grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionScenePlayerAnimation49 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

// Stored raw-word transfer length; the clip table itself has four entries.
enum { DRYFIELD_NIGHT_DRIVEWAY_COMPANION_SCENE_COPY_WORD_COUNT = 10 };

/// Installs ten source words for the companion scenes in the player bank.
///
/// This is a stored transfer length, including six words after the clip table,
/// rather than its element count. Dispatch consumes the request synchronously.
static AnimationBankCopyRequest _gDryfieldNightDrivewayCompanionSceneAnimationBankCopy = {
    .sets  = _gDryfieldNightDrivewayCompanionSceneAnimationBankExtension,
    .count = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_SCENE_COPY_WORD_COUNT,
};

/// Restarts companion clip 1 at the beginning of both companion scenes, without grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionSceneCompanionAnimation1 = {
    .source               = { .index = 6 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts companion clip 38 after player clip 48 in both scenes, without grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionSceneCompanionAnimation38 = {
    .source               = { .index = 6 },
    .animationId          = 38,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts companion clip 7 after the first scripted run and turn, without grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionSceneCompanionAnimation7 = {
    .source               = { .index = 6 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts companion clip 8 after clip 38 in both scenes, without grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionSceneCompanionAnimation8 = {
    .source               = { .index = 6 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts companion clip 9 after clip 7 in both scenes, without grid participation.
///
/// Event playback copies the request and resolves its stored bank selector
/// against the current weapon or companion before synchronous dispatch.
static AnimationPlayRequest _gDryfieldNightDrivewayCompanionSceneCompanionAnimation9 = {
    .source               = { .index = 6 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion approach clip 4 and arrival clip 9 for all four scene runs.
///
/// IDs select the companion's current bank. The handler reads this borrowed
/// pair synchronously and retains the low 16 bits of each ID.
static GameActorMoveAnim _gDryfieldNightDrivewayCompanionSceneRunAnimations = {
    .approachAnimId = 4,
    .arrivalAnimId  = 9,
};

enum {
    DRYFIELD_NIGHT_DRIVEWAY_COMPANION_SCENE_CAP_COMMAND  = 5,
    DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_SCENE_VIEW = 8,
    DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_FINAL_VIEW = 4,
    DRYFIELD_NIGHT_DRIVEWAY_COMPANION_VIEW2_SCENE_VIEW   = 2,
    DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_SOUND      = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_DRIVEWAY, 13),
    DRYFIELD_NIGHT_DRIVEWAY_FADE_DEFAULT_FRAMES          = 0, // Start with the interpreter's seven-frame ramp
    DRYFIELD_NIGHT_DRIVEWAY_FADE_KEEP_FRAMES             = 0, // Return without replacing the current ramp duration
    DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE             = 0, // A zero WAIT_FRAMES still yields before the next command
};

/// Skip stream for the warp-4 companion arrival: places both actors and restores view 4 and control.
///
/// Fifteen commands including END. Borrowed by the interpreter through script
/// completion; the loaded room also owns every referenced placement and request.
static EvsCommand _gDryfieldNightDrivewayCompanionArrivalSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = DRYFIELD_NIGHT_DRIVEWAY_FADE_DEFAULT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalPlayerFinalTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalCompanionFinalTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_FINAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Warp-4 companion arrival scene: CAP command 5, two companion runs and player clips 47..49.
///
/// Fifty-one commands including END. The room starts this only with a live
/// companion; playback uses view 8 and returns to view 4. The interpreter borrows
/// this stream and its room resources through normal or skip completion.
/// Normal area scheduling supplies the family-2 companion, which implements
/// the required run and motion-pending messages.
static EvsCommand _gDryfieldNightDrivewayCompanionArrivalScript[] = {
    // Start CAP and install the player clips before staging the hidden actors.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightDrivewayCompanionSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalPlayerInitialTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalCompanionInitialTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // The first run and turn are synchronized with CAP and player playback.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalCompanionFirstStopTransform } }, { .message = { .moveAnimation = &_gDryfieldNightDrivewayCompanionSceneRunAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionScenePlayerAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionScenePlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalCompanionFirstStopTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalCompanionFinalTransform } }, { .message = { .moveAnimation = &_gDryfieldNightDrivewayCompanionSceneRunAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionScenePlayerAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionArrivalPlayerFinalTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Hide the final staging while restoring the view, weapons and actor control.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_ARRIVAL_FINAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = DRYFIELD_NIGHT_DRIVEWAY_FADE_KEEP_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Unstarted skip-style stream placing both actors at the view-2 scene's final transforms.
///
/// Fourteen commands including END restore view 2 and scripted control. Nothing
/// installs this stream as a skip target; its intended start condition is unproven.
static EvsCommand _gDryfieldNightDrivewayCompanionView2SkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = DRYFIELD_NIGHT_DRIVEWAY_FADE_DEFAULT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2PlayerFinalTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2CompanionFinalTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_VIEW2_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Unstarted view-2 companion scene: CAP command 5, two companion runs and player clips 47..49.
///
/// Forty-nine commands including END restore view 2, weapons and scripted control.
/// Nothing starts this stream; its intended trigger and pairing with the adjacent
/// skip-style stream are unproven. Its payloads borrow this room's resources.
static EvsCommand _gDryfieldNightDrivewayCompanionView2Script[] = {
    // Start CAP and install the player clips before staging the hidden actors.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightDrivewayCompanionSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_VIEW2_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = DRYFIELD_NIGHT_DRIVEWAY_FADE_KEEP_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2PlayerInitialTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2CompanionInitialTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // The first run and turn are synchronized with CAP and player playback.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2CompanionFirstStopTransform } }, { .message = { .moveAnimation = &_gDryfieldNightDrivewayCompanionSceneRunAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2CompanionFirstStopTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionScenePlayerAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionScenePlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionSceneCompanionAnimation8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2CompanionFinalTransform } }, { .message = { .moveAnimation = &_gDryfieldNightDrivewayCompanionSceneRunAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDrivewayCompanionScenePlayerAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_DRIVEWAY_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gDryfieldNightDrivewayCompanionView2PlayerFinalTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Hide the final staging while restoring the view, weapons and actor control.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DRIVEWAY_COMPANION_VIEW2_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = DRYFIELD_NIGHT_DRIVEWAY_FADE_KEEP_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Three light-shaft endpoint pairs in signed world-coordinate units.
///
/// Pair 0 is drawn in logical views 2/3/9/10, pair 1 in 3/4/7/10,
/// and pair 2 in view 5. Each pair supplies two consecutive SVECTORs.
static SVECTOR _gDryfieldNightDrivewayLightShaftPoints[][2] = {
    { { -0x27B0, -2800, -1210, 0 }, { -0x27B0, -3100, -1640, 0 } },
    { { -3960, -2070, 890, 0 }, { -3960, -2070, 140, 0 } },
    { { 5740, -2160, 440, 0 }, { 5740, -2430, 900, 0 } },
};

WorldCoordRoomLighting gDryfieldNightDrivewayRoomLighting[2] = {
    { &_gDryfieldNightDrivewayRoomLights, _gDryfieldNightDrivewayAmbientByView },
    { &_gDryfieldNightDrivewayRoomLights, _gDryfieldNightDrivewayAmbientByView },
};

WorldCollisionRoomResources gDryfieldNightDrivewayRoomCollisionResources[2] = {
    { .grid = &_gDryfieldNightDrivewayCollisionGrid, .viewBoundaryTriggers = _gDryfieldNightDrivewayViewBoundaryTriggers, .actionTriggers = _gDryfieldNightDrivewayActionTriggers, .occluders = _gDryfieldNightDrivewaySightOccluders },
    { .grid = &_gDryfieldNightDrivewayCollisionGrid, .viewBoundaryTriggers = _gDryfieldNightDrivewayViewBoundaryTriggers, .actionTriggers = _gDryfieldNightDrivewayActionTriggers, .occluders = _gDryfieldNightDrivewaySightOccluders },
};

/// Camera/image/sprite indices for room 2's ten logical views.
///
/// Indexed by logical view minus one; all ten active entries are 1-based.
/// Two trailing zero bytes are retained; their purpose is unproven.
static u8 _gDryfieldNightDrivewayRoom2ViewMap[12] = {
    1,
    9,
    10,
    4,
    5,
    6,
    4,
    8,
    2,
    3,
    0,
    0,
};

u8* gDryfieldNightDrivewayViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldNightDrivewayRoom2ViewMap,
};

ViewCount gDryfieldNightDrivewayViewCounts[2] = { 10, 10 };

DirectionWarpEntry gDryfieldNightDrivewayWarpEntries[4] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, 0x53190004, 0x53190003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 474 },
    { { { .word = ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT }, -1300, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -9500, 4, -1418 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -9500, 4, -1418 }, { 0, 0, 0, 0 }, 0x53190002, 0x53190001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 482 },
    { { { .word = ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT }, -1300, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

enum {
    DRYFIELD_NIGHT_DRIVEWAY_GRID_CELL_COUNT_X = 7,
    DRYFIELD_NIGHT_DRIVEWAY_GRID_CELL_COUNT_Z = 3,
};

/// Thirteen room-space normals for the driveway mesh; 4096 represents unit length.
static SVECTOR _gDryfieldNightDrivewayCollisionGridNormals[13] = {
#include "assets/dryfield_night_driveway_collision_0364C_normals.inc"
};

/// Sixty-four mesh vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightDrivewayCollisionGridVerts[64] = {
#include "assets/dryfield_night_driveway_collision_0364C_verts.inc"
};

/// Twenty-three mesh quads, indexing the vertex/normal pools and surface classes 0..2.
static WorldCollisionGridFace _gDryfieldNightDrivewayCollisionGridFaces[23] = {
#include "assets/dryfield_night_driveway_collision_0364C_faces.inc"
};

/// Packed signed face-index lists; each nonempty cell ends with -1.
static s16 _gDryfieldNightDrivewayCollisionGridCells[154] = {
#include "assets/dryfield_night_driveway_collision_0364C_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldNightDrivewayCollisionGridCells` (154 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_NIGHT_DRIVEWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightDrivewayCollisionGridCells[(halfwordOffset)])

/// X-major cell pointers into the face-index lists; NULL denotes the final empty cell.
static s16* _gDryfieldNightDrivewayCollisionGridTable[DRYFIELD_NIGHT_DRIVEWAY_GRID_CELL_COUNT_X * DRYFIELD_NIGHT_DRIVEWAY_GRID_CELL_COUNT_Z] = {
#include "assets/dryfield_night_driveway_collision_0364C_table.inc"
};
#undef DRYFIELD_NIGHT_DRIVEWAY_CELL_FACE_LIST_AT_OFFSET

/// Driveway collision mesh shared by rooms 1 and 2, with an X-major 7 by 3 cell index.
///
/// The grid origin is world X/Z (-11030, -5210), with 4000-unit square cells.
/// Its 23 quads select room surface classes 0..2. Activation binds the view
/// coordinate; keep the descriptor and all five pools loaded while active.
static WorldCollisionGrid _gDryfieldNightDrivewayCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightDrivewayCollisionGridNormals,
    .vertices    = _gDryfieldNightDrivewayCollisionGridVerts,
    .faces       = _gDryfieldNightDrivewayCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightDrivewayCollisionGridTable,
    .xBias       = 11030,
    .zBias       = 5210,
    .cellCountX  = DRYFIELD_NIGHT_DRIVEWAY_GRID_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_NIGHT_DRIVEWAY_GRID_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightDrivewayCollisionGridFaces),
};

ViewCamera gDryfieldNightDrivewayViewCameras[10] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 1950, 0x7530, -2670 } }, 380 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { -781, 0, -4020 }, { -1703, 3710, 331 }, { 3642, 1735, -707 } }, { 1820, 2220, -2312 } }, 230 },
    { { { { 287, 0, 4085 }, { 2747, 3031, -193 }, { -3024, 2754, 213 } }, { 29, 1719, -2342 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { 1822, 0, -3668 }, { -2650, 2831, -1317 }, { 2535, 2959, 1260 } }, { 1959, 2418, -462 } }, 289 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
};

// Sprite pages are 8-bit textures at VRAM Y 0; modulation 128 is neutral.
enum {
    DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TEXTURE_8BIT  = 1,
    DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896     = getTPage(DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960     = getTPage(DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR = 128,
};

/// Empty sprite-batch list retaining the decoded background in mapped view 1.
static SpriteBatch _gDryfieldNightDrivewayView1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 9 mutable sprite rectangles for mapped view 2.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// Its batch list selects ranges in source elements.
static SpriteSource _gDryfieldNightDrivewayView2SpriteSources[9] = {
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 2, retaining its decoded background.
///
/// The first zero-count record selects background image strips; a terminal
/// SPRITE_BATCH_END record ends the list. Serialized metadata is retained.
static SpriteBatch _gDryfieldNightDrivewayView2SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 1, 0 } },
    { .firstSprite = 5, .spriteCount = 4 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 17 mutable sprite rectangles for mapped view 3.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// Its batch list selects ranges in source elements.
static SpriteSource _gDryfieldNightDrivewayView3SpriteSources[17] = {
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 72, 500, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 3, retaining its decoded background.
///
/// The first zero-count record selects background image strips; a terminal
/// SPRITE_BATCH_END record ends the list. Serialized metadata is retained.
static SpriteBatch _gDryfieldNightDrivewayView3SpriteBatches[7] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 6 },
    { .firstSprite = 6, .spriteCount = 6, .field_6 = { 3, 0 } },
    { .firstSprite = 12, .spriteCount = 1, .field_6 = { 2, 0 } },
    { .firstSprite = 13, .spriteCount = 2, .field_6 = { 4, 0 } },
    { .firstSprite = 15, .spriteCount = 2, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Pixel clip and depth boundary for restoring full-screen drawing in mapped view 3.
///
/// The 222-by-239 clip starts at the draw-buffer origin, and depth 925
/// restores full-screen drawing. The second record is the list terminator.
static SpriteDrawArea _gDryfieldNightDrivewayView3DrawAreas[2] = {
    { .clipRect = { 0, 0, 222, 239 }, .restoreDepth = 925 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// 54 mutable sprite rectangles for mapped view 4.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// Its batch list selects ranges in source elements.
static SpriteSource _gDryfieldNightDrivewayView4SpriteSources[54] = {
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 64, 72 } }, -160, -72, 860, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 88, 72 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 128 } }, -40, -120, 1800, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 128 } }, -8, -120, 1825, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 128 } }, 24, -120, 1825, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 32, 24 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 72 } }, 40, -88, 825, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 80 } }, 40, -16, 875, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 16 } }, 104, -16, 825, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 24 } }, 96, 48, 850, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, -104, 762, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -80, 851, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -56, 837, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, -40, 845, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, -64, 845, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, -56, 845, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, -16, 1000, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 8 } }, 56, -24, 845, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, -40, 845, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 24 } }, 88, -56, 837, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, -32, 837, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 48 } }, 80, -16, 837, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 32 } }, 80, -96, 845, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 112, 507, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, 40, 8, 900, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, 56, 16, 875, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, 16, 875, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, 88, 16, 875, { .fields = { 0, 192 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 56 } }, 104, 8, 850, { .fields = { 0, 136 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 32 } }, 88, 0, 1064, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 32 } }, 72, 0, 1064, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 24 } }, 104, 8, 1064, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 0, 1064, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, 0, 1064, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 4, retaining its decoded background.
///
/// The first zero-count record selects background image strips; a terminal
/// SPRITE_BATCH_END record ends the list. Serialized metadata is retained.
static SpriteBatch _gDryfieldNightDrivewayView4SpriteBatches[13] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 3, .field_6 = { 1, 0 } },
    { .firstSprite = 3, .spriteCount = 0, .field_6 = { 6, 0 } },
    { .firstSprite = 3, .spriteCount = 3, .field_6 = { 2, 0 } },
    { .firstSprite = 6, .spriteCount = 3, .field_6 = { 7, 0 } },
    { .firstSprite = 9, .spriteCount = 7, .field_6 = { 5, 0 } },
    { .firstSprite = 16, .spriteCount = 9, .field_6 = { 8, 0 } },
    { .firstSprite = 25, .spriteCount = 3, .field_6 = { 4, 0 } },
    { .firstSprite = 28, .spriteCount = 12, .field_6 = { 9, 0 } },
    { .firstSprite = 40, .spriteCount = 4 },
    { .firstSprite = 44, .spriteCount = 5, .field_6 = { 10, 0 } },
    { .firstSprite = 49, .spriteCount = 5, .field_6 = { 3, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 12 mutable sprite rectangles for mapped view 5.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// Its batch list selects ranges in source elements.
static SpriteSource _gDryfieldNightDrivewayView5SpriteSources[12] = {
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 0, 969, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 8 } }, -88, 8, 948, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 8 } }, -88, 16, 888, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 8 } }, -88, 24, 895, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 8 } }, -88, 32, 890, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 8 } }, -88, 40, 885, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 48, 8 } }, -88, 48, 880, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 8 } }, -88, 56, 880, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 32 } }, -96, 32, 650, { .fields = { 120, 40 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 80, 650, { .fields = { 120, 104 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 40 } }, -88, 32, 650, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 32 } }, -88, 72, 681, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 5, retaining its decoded background.
///
/// The first zero-count record selects background image strips; a terminal
/// SPRITE_BATCH_END record ends the list. Serialized metadata is retained.
static SpriteBatch _gDryfieldNightDrivewayView5SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = 8, .spriteCount = 4 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 6.
static SpriteBatch _gDryfieldNightDrivewayView6SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 7.
static SpriteBatch _gDryfieldNightDrivewayView7SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 8.
static SpriteBatch _gDryfieldNightDrivewayView8SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 9 mutable sprite rectangles for mapped view 9.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// Its batch list selects ranges in source elements.
static SpriteSource _gDryfieldNightDrivewayView9SpriteSources[9] = {
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 9, retaining its decoded background.
///
/// The first zero-count record selects background image strips; a terminal
/// SPRITE_BATCH_END record ends the list. Serialized metadata is retained.
static SpriteBatch _gDryfieldNightDrivewayView9SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 1, 0 } },
    { .firstSprite = 5, .spriteCount = 4 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 17 mutable sprite rectangles for mapped view 10.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// Its batch list selects ranges in source elements.
static SpriteSource _gDryfieldNightDrivewayView10SpriteSources[17] = {
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 72, 500, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_DRIVEWAY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_CLUT, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_DRIVEWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 10, retaining its decoded background.
///
/// The first zero-count record selects background image strips; a terminal
/// SPRITE_BATCH_END record ends the list. Serialized metadata is retained.
static SpriteBatch _gDryfieldNightDrivewayView10SpriteBatches[7] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 6 },
    { .firstSprite = 6, .spriteCount = 6, .field_6 = { 3, 0 } },
    { .firstSprite = 12, .spriteCount = 1, .field_6 = { 2, 0 } },
    { .firstSprite = 13, .spriteCount = 2, .field_6 = { 4, 0 } },
    { .firstSprite = 15, .spriteCount = 2, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Pixel clip and depth boundary for restoring full-screen drawing in mapped view 10.
///
/// The 222-by-239 clip starts at the draw-buffer origin, and depth 925
/// restores full-screen drawing. The second record is the list terminator.
static SpriteDrawArea _gDryfieldNightDrivewayView10DrawAreas[2] = {
    { .clipRect = { 0, 0, 222, 239 }, .restoreDepth = 925 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

SpriteView gDryfieldNightDrivewaySpriteViews[10] = {
    { { .empty = _gDryfieldNightDrivewayView1SpriteBatches }, _gDryfieldNightDrivewayView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDrivewayView2SpriteSources }, _gDryfieldNightDrivewayView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDrivewayView3SpriteSources }, _gDryfieldNightDrivewayView3SpriteBatches, _gDryfieldNightDrivewayView3DrawAreas },
    { { .elements = _gDryfieldNightDrivewayView4SpriteSources }, _gDryfieldNightDrivewayView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDrivewayView5SpriteSources }, _gDryfieldNightDrivewayView5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightDrivewayView6SpriteBatches }, _gDryfieldNightDrivewayView6SpriteBatches, NULL },
    { { .empty = _gDryfieldNightDrivewayView7SpriteBatches }, _gDryfieldNightDrivewayView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightDrivewayView8SpriteBatches }, _gDryfieldNightDrivewayView8SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDrivewayView9SpriteSources }, _gDryfieldNightDrivewayView9SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDrivewayView10SpriteSources }, _gDryfieldNightDrivewayView10SpriteBatches, _gDryfieldNightDrivewayView10DrawAreas },
};

/// Six mutable directed view boundaries: 2/3, 3/4 and 4/5 in both directions.
///
/// The LAST-marked sixth record is included. Room setup binds every quad to the
/// current view and links it until a view change or room teardown clears the list.
static WorldCollisionTrigger _gDryfieldNightDrivewayViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -6369, -1536, -1504, 0 }, { { 0, -2560, 1024, 0 }, { 0, -2560, -1024, 0 }, { 0, 2560, 1024, 0 }, { 0, 2560, -1024, 0 } }, { -4095, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2757, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6816, -1472, -1568, 0 }, { { 0, -2496, -1024, 0 }, { 0, -2496, 1024, 0 }, { 0, 2496, -1024, 0 }, { 0, 2496, 1024, 0 } }, { ONE, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2697, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2770, -1600, -856, 0 }, { { -1521, -2624, 0, 0 }, { 1522, -2624, 1, 0 }, { -1521, 2624, 0, 0 }, { 1522, 2624, 1, 0 } }, { 1, 0, -4103, 0 }, { 0, 0, ONE, 0 }, 3029, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2675, -1376, -1017, 0 }, { { 1554, -2400, 1, 0 }, { -1553, -2400, 0, 0 }, { 1554, 2400, 1, 0 }, { -1553, 2400, 0, 0 } }, { -3, 0, 4097, 0 }, { 0, 0, ONE, 0 }, 2850, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 294, -1520, 1474, 0 }, { { -435, -2544, 2215, 0 }, { 435, -2544, -2214, 0 }, { -435, 2544, 2215, 0 }, { 435, 2544, -2214, 0 } }, { -4034, 0, -793, 0 }, { 0, 0, ONE, 0 }, 3396, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -26, -1488, 1301, 0 }, { { 435, -2512, -2213, 0 }, { -435, -2512, 2214, 0 }, { 435, 2512, -2213, 0 }, { -435, 2512, 2214, 0 } }, { 4031, 0, 792, 0 }, { 0, 0, ONE, 0 }, 3367, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Eight authored point lights shared by both driveway rooms.
///
/// Positions and falloff radii are world units; RGB intensities use Q12.
/// All views admit every light. Coordinate composition and attenuation queries
/// update this storage while the room overlay is loaded.
static WorldCoordPointLight _gDryfieldNightDrivewayPointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2950, -2500, -1400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7437, -2500, -1400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4803, -2500, -1400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1618, -1633, -537 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3352, -2500, 705 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1510, -2500, 2719 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1670, -2500, 2899 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4860, -2500, 705 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, ONE }, { 0, 0 } }, 1000, 3000 },
};

/// Both rooms' authored light collection: eight points, no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightDrivewayRoomLights = {
    .pointLightCount = ARRAY_SIZE(_gDryfieldNightDrivewayPointLights),
    .pointLights     = _gDryfieldNightDrivewayPointLights,
};

/// Three warp actions and CAP command 7, terminated by the included LAST record.
///
/// Warp targets are areas 23, 32 and 2 with packed endpoint selectors 0x11,
/// 0x21 and 0x37. The area-32 action admits proximity or facing; the others
/// require facing. Room setup binds and links this mutable four-record array.
static WorldCollisionTrigger _gDryfieldNightDrivewayActionTriggers[4] = {
    { NULL, NULL, NULL, { -3680, -48, 624, 0 }, { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { ONE, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 0x11, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1488, -48, 2496, 0 }, { { -1200, 0, -1216, 0 }, { 1200, 0, -1216, 0 }, { -1200, 0, 1024, 0 }, { 1200, 0, 1024, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 1707, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 0x21, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9664, -48, -1440, 0 }, { { -352, 0, -352, 0 }, { 352, 0, -352, 0 }, { -352, 0, 352, 0 }, { 352, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { ONE, 0, 0, 0 }, 497, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 0x37, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5984, -64, 1344, 0 }, { { -352, 0, -1024, 0 }, { 352, 0, -1024, 0 }, { -352, 0, 1024, 0 }, { 352, 0, 1024, 0 } }, { 0, 4094, 0, 0 }, { -ONE, 0, 0, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Bat resource and end marker for placement variant 1's thirteen spawns.
///
/// Entry 37 loads actor-slot-1 file 103700 and spawns descriptor 0. The room
/// and actor descriptor must stay loaded through loading and spawning.
static AreaResource _gDryfieldNightDrivewayVariant1Resources[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bat resource and end marker for placement variant 3's twelve spawns.
///
/// Entry 37 uses the same file 103700 and descriptor 0 as variant 1, but
/// belongs to a separate placement layout. Keep both resource owners loaded.
static AreaResource _gDryfieldNightDrivewayVariant3Resources[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Mind Suckler and Scorpion resources for placement variant 21, then the end marker.
///
/// Entries 15 and 25 load actor-slot files 101500 and 202500 respectively,
/// spawning descriptor 0 for five Mind Sucklers and three Scorpions. Keep the
/// room resource list and borrowed actor descriptor tables loaded through use.
static AreaResource _gDryfieldNightDrivewayVariant21Resources[3] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor02500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightDrivewayAreaVariants[22] = {
    [1]  = { .placements = gMapDryfieldFullDrivewayVariant1Placements, .resources = _gDryfieldNightDrivewayVariant1Resources },
    [3]  = { .placements = gMapDryfieldFullDrivewayVariant3Placements, .resources = _gDryfieldNightDrivewayVariant3Resources },
    [21] = { .placements = gMapDryfieldFullDrivewayVariant21Placements, .resources = _gDryfieldNightDrivewayVariant21Resources },
};

/// Two mutable room-space sight-blocking quads shared by both driveway rooms.
///
/// LAST includes the second quad. Positions and radii use game units and normals
/// use 4096 per unit; the stored low flag bit has no established meaning.
/// Room setup links and enables both records; keep them loaded until unlinked.
static WorldCollisionOccluder _gDryfieldNightDrivewaySightOccluders[2] = {
    { NULL, NULL, { -5648, -2128, 1792, 0 }, { { -1648, 3152, 2624, 0 }, { 1648, 3152, -2624, 0 }, { -1648, -3152, 2624, 0 }, { 1648, -3152, -2624, 0 } }, { 3483, 0, 2187, 0 }, 4404, 1, 0 },
    { NULL, NULL, { 1824, -2160, -2656, 0 }, { { -1648, 3184, 2624, 0 }, { 1648, 3184, -2624, 0 }, { -1648, -3184, 2624, 0 }, { 1648, -3184, -2624, 0 } }, { 3478, 0, 2184, 0 }, 4434, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Minimum ambient RGB levels for logical views 1..10, shared by both rooms.
///
/// Entry zero holds the view count. Colour levels use 16 units per 8-bit
/// channel step; each colour record retains its stored weighted luma.
static WorldCoordRoomAmbientEntry _gDryfieldNightDrivewayAmbientByView[11] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightDrivewayAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 616, 617 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 615, 617, 616, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

enum {
    DRYFIELD_NIGHT_DRIVEWAY_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 29,
    DRYFIELD_NIGHT_DRIVEWAY_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 31,
};

/// Surface-class 2's footstep bases in the currently loaded type-1 sound bank.
///
/// Walk and scripted-jump cue 2 use entry 29; running uses entry 31.
/// Cue 1 adds one and companion playback adds 100 to a nonzero base.
static WorldCollisionFootstepSounds _gDryfieldNightDrivewayFootstepSounds = {
    .walk         = DRYFIELD_NIGHT_DRIVEWAY_FOOTSTEP_WALK,
    .run          = DRYFIELD_NIGHT_DRIVEWAY_FOOTSTEP_RUN,
    .scriptedJump = DRYFIELD_NIGHT_DRIVEWAY_FOOTSTEP_WALK,
};

/// Blocks probes, permits weapon impacts and applies pushback, without footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightDrivewayDefaultSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface-class 1 policy: probe passage, no weapon impacts, with pushback retained.
static WorldCollisionSurfaceProperties _gDryfieldNightDrivewayProbePassSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface-class 2 policy: blocking contact with weapon impacts, pushback and footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightDrivewayFootstepSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightDrivewayFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightDrivewaySurfacePropertyTable[8] = {
    &_gDryfieldNightDrivewayDefaultSurface,
    &_gDryfieldNightDrivewayProbePassSurface,
    &_gDryfieldNightDrivewayFootstepSurface,
    &_gDryfieldNightDrivewayDefaultSurface,
    &_gDryfieldNightDrivewayDefaultSurface,
    &_gDryfieldNightDrivewayDefaultSurface,
    &_gDryfieldNightDrivewayDefaultSurface,
    &_gDryfieldNightDrivewayDefaultSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Last factory-gate evaluation's staged spawn indication (0 none, 1 requested).
///
/// Queries also clear it. Execution raises it after the spawn call even if
/// allocation fails; there is no reader or completion reset in either carrier.
static u8 _gRoomVariantDrivewayFactoryEventRequested = 0;

/// Unreferenced stored byte 2 after the event flag; its role is unproven.
static u8 D_dryfield_night_driveway_80182121 = 2;

/// Unreferenced stored zero byte; its role is unproven.
static u8 D_dryfield_night_driveway_80182122 = 0;

/// Unreferenced stored zero byte; its role is unproven.
static u8 D_dryfield_night_driveway_80182123 = 0;

RoomLatchedEvent gRoomEventLatched = { 0 };

#include "../../shared/room/room_event_staged_task.inc.c"

/// Night-driveway room states: initialize (0), idle (1), release (2).
///
/// Copied as three callback pointers before dispatch by `Task::state`, which
/// must be in 0..2. There is no terminator or bounds check; the last state kills
/// the task. Keep the table and room callbacks loaded through dispatch.
static const TaskFuncTable3 _gDryfieldNightDrivewayRoomStates = {
    { _dryfieldNightDrivewayInitializeRoom, _dryfieldNightDrivewayRoomIdle, taskKill },
};

/// Supplies the night room's scene task table for driveway departure resolution.
///
/// Expands to a private three-entry `TaskDesc` array, passed as `TaskDesc*`.
/// Slot 1 is the retained bodyless priority-32 scripted encounter, eligible only in
/// `GAME_STAGE_DRYFIELD`; slot 0 is the blackout and slot 2 the terminator.
/// Spawning reads the selected recipe synchronously with unused payload words.
/// Keep its callback code and borrowed resources loaded while used. This
/// identifier binding has no arguments or side effects and is required only
/// by the following resolver fragment.
#define ROOM_VARIANT_DRIVEWAY_SCENE_TASK_TABLE _gDryfieldNightDrivewaySceneTasks
#include "../../shared/dryfield_driveway_resolve.inc.c"
#undef ROOM_VARIANT_DRIVEWAY_SCENE_TASK_TABLE

/// Selects this room's retained wire-rope blackout entry stream for the shared controller.
///
/// Expands to a private END-terminated `EvsCommand` array, passed as
/// `EvsCommand*` with `DRIVEWAY_BLACKOUT_SKIP_SCRIPT` as its skip continuation.
/// Playback borrows the stream and its resources until the interpreter ends,
/// beyond the controller task's lifetime. The night room's key-item receiver
/// refuses all items, so it does not start this controller. This identifier
/// binding has no arguments or side effects and is needed only by the following fragment.
#define DRIVEWAY_BLACKOUT_SCRIPT _gDryfieldNightDrivewayBlackoutScript
/// Selects the retained wire-rope blackout's input-skip continuation for the shared controller.
///
/// Expands to this room's private nine-command, END-terminated `EvsCommand`
/// array, passed as `EvsCommand*` to `evsStartScriptWithSkip`. It reloads the
/// saved view and restores player control behind a fade. Playback borrows the
/// stream until the interpreter ends, beyond the controller task's lifetime.
/// The night room's key-item receiver does not start this controller. This
/// identifier binding has no arguments or side effects; define it only around
/// the blackout fragment, alongside `DRIVEWAY_BLACKOUT_SCRIPT`.
#define DRIVEWAY_BLACKOUT_SKIP_SCRIPT _gDryfieldNightDrivewayBlackoutSkipScript
#include "../../shared/dryfield_driveway_blackout.inc.c"
#undef DRIVEWAY_BLACKOUT_SKIP_SCRIPT
#undef DRIVEWAY_BLACKOUT_SCRIPT

/// Selects this room's retained bat-encounter stream for the shared controller.
///
/// Expands to a private END-terminated `EvsCommand` array, passed as
/// `EvsCommand*` to `evsStartScript` with HUD hide/restore and no input skip
/// target. The interpreter borrows the stream and callbacks until it ends;
/// the installed player animation data stays borrowed during bank playback.
/// The script releases the bat waves and restores player control; the resolver
/// starts this retained daytime scene only in `GAME_STAGE_DRYFIELD`.
/// This identifier binding has no arguments or side effects and is needed
/// only by the following fragment.
#define DRIVEWAY_ENCOUNTER_SCRIPT _gDryfieldNightDrivewayEncounterScript
#include "../../shared/dryfield_driveway_cutscene.inc.c"
#undef DRIVEWAY_ENCOUNTER_SCRIPT

static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Sets the actor 03700 encounter's entrance and wave stage from an event script.
///
/// Stores the low signed byte of `waveStage` without clamping. Stage 0 is initial,
/// 1 begins progression, and stages 2..5 release successive placement-gated waves.
/// The driveway script supplies `DRYFIELD_NIGHT_DRIVEWAY_ENCOUNTER_WAVE_BEGIN`;
/// actor tasks advance the later stages.
static void _dryfieldNightDrivewaySetEncounterWave(s32 waveStage)
{
    gSceneCombatState.actor03700Wave = waveStage;
}

#include "../../shared/dryfield_driveway_set_view_dirty.inc.c"

/// Sets whether the player's actor-state update is held during the blackout scene.
///
/// Zero resumes state updates; every other byte holds them. The script resumes
/// updates after the blackout task held them. Pending displacement, coordinate
/// composition and collision processing still run while state updates are held.
static void _dryfieldNightDrivewaySetPlayerStateHold(u8 hold)
{
    gPlayerActorStateTickHeld = hold;
}

#include "../../shared/dryfield_driveway_script_sound.inc.c"

/// Refuses every key-item use request in the night driveway.
///
/// All arguments are ignored. Returns zero so the inventory shows that the
/// collected `itemId` cannot be used here; `unusedArg` is the sender's zero word.
static s32 _dryfieldNightDrivewayRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Ignores room commands from CAP and direction control, returning zero.
///
/// All arguments are ignored; the senders do not inspect the result.
static s32 _dryfieldNightDrivewayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandMode)
{
    return 0;
}

/// Ignores direction-trigger action requests, returning zero.
///
/// `request` is borrowed until dispatch returns and is neither read nor retained.
/// All arguments are ignored; the sender supplies zero for `unusedArg` and does
/// not inspect the result.
static s32 _dryfieldNightDrivewayIgnoreDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs the room receiver and starts the companion arrival scene for warp 4.
///
/// Advances to idle. The session's room slot borrows the live task; the room
/// and its normal/skip scripts must remain loaded through event completion.
static void _dryfieldNightDrivewayInitializeRoom(Task* task)
{
    enum { COMPANION_SCENE_ARRIVAL_WARP = 4 };

    task->msgTable = _gDryfieldNightDrivewayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) && (gGameSession->location.warp == COMPANION_SCENE_ARRIVAL_WARP)) {
        evsStartScriptWithSkip(_gDryfieldNightDrivewayCompanionArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldNightDrivewayCompanionArrivalSkipScript);
    }
    task->state = task->state + 1;
}

/// Keeps the room task idle after initialization while its message table remains live.
///
/// Ignores `task` and leaves its state unchanged.
static void _dryfieldNightDrivewayRoomIdle(Task* task)
{
    // Retain the idle state's unused 16-byte stack frame.
    byte unusedStackFrame[16];
}

void dryfieldNightDrivewayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightDrivewayRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_shaft.inc.c"

void dryfieldNightDrivewayDrawLightShaftsTask(Task* unusedTask)
{
    // Pixel radius is this scale times 64 divided by camera Z / 4.
    enum { DRYFIELD_NIGHT_DRIVEWAY_SHAFT_RADIUS_SCALE = 0x180 };

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->location.view) {
        case 2:
        case 9:
            _glowDrawShaft(_gDryfieldNightDrivewayLightShaftPoints[0], DRYFIELD_NIGHT_DRIVEWAY_SHAFT_RADIUS_SCALE);
            break;
        case 4:
        case 7:
            _glowDrawShaft(_gDryfieldNightDrivewayLightShaftPoints[1], DRYFIELD_NIGHT_DRIVEWAY_SHAFT_RADIUS_SCALE);
            break;
        case 5:
            _glowDrawShaft(_gDryfieldNightDrivewayLightShaftPoints[2], DRYFIELD_NIGHT_DRIVEWAY_SHAFT_RADIUS_SCALE);
            break;
        case 3:
        case 10:
            _glowDrawShaft(_gDryfieldNightDrivewayLightShaftPoints[0], DRYFIELD_NIGHT_DRIVEWAY_SHAFT_RADIUS_SCALE);
            _glowDrawShaft(_gDryfieldNightDrivewayLightShaftPoints[1], DRYFIELD_NIGHT_DRIVEWAY_SHAFT_RADIUS_SCALE);
            break;
    }
}
