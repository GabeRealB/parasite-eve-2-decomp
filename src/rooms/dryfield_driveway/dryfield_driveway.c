#include "rooms/dryfield_driveway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_00100.h"
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
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_events.h"
#include "../../shared/dryfield_driveway.h"

static void _drivewaySetViewDirty(s16 viewDirty);

static u8 _gRoomVariantDrivewayFactoryEventRequested;

extern RoomFadeStorage  gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static WorldCollisionGrid    _gDryfieldDrivewayCollisionGrid;
static WorldCollisionTrigger _gDryfieldDrivewayViewBoundaryTriggers[6];
static WorldCollisionTrigger _gDryfieldDrivewayActionTriggers[11];
static WorldCoordRoomLights  _gDryfieldDrivewayRoomLights;
static s32                   _dryfieldDrivewayUseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32                   _dryfieldDrivewayIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg);
static s32                   _dryfieldDrivewayIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedRequestWord, s32 unusedSecondArg);
static void                  _dryfieldDrivewaySetEncounterWave(s32 waveStage);
static void                  _dryfieldDrivewayIdleRoomTask(Task* unusedTask);
static void                  _dryfieldDrivewaySetPlayerUpdateHold(u8 holdPlayerUpdate);
static void                  _dryfieldDrivewayInitRoomTask(Task* task);

/// The script releases actor 03700's staged entrance before combat waves advance.
enum { DRYFIELD_DRIVEWAY_ENCOUNTER_WAVE_BEGIN = 1 };

static AnimationPackedPose _gDryfieldDrivewayAnimation00D08Bank1[10] = {
#include "assets/dryfield_driveway_animation_00D08_bank1.inc"
};

static AnimationPackedRotation _gDryfieldDrivewayAnimation00D08Bank4[95] = {
#include "assets/dryfield_driveway_animation_00D08_bank4.inc"
};

static AnimationRecord _gDryfieldDrivewayAnimation00D08Records[139] = {
#include "assets/dryfield_driveway_animation_00D08_records.inc"
};

static u16 _gDryfieldDrivewayAnimation00D08Indices[20] = {
#include "assets/dryfield_driveway_animation_00D08_indices.inc"
};

static AnimationSet _gDryfieldDrivewayAnimation00D08 = {
    _gDryfieldDrivewayAnimation00D08Records,
    _gDryfieldDrivewayAnimation00D08Indices,
    { NULL, _gDryfieldDrivewayAnimation00D08Bank1, NULL, NULL, _gDryfieldDrivewayAnimation00D08Bank4, NULL, NULL, NULL },
};

/// Bodyless priority-32 recipe for the staged factory-transition scene.
///
/// Spawns state 0 with unused arguments. The room-owned destination and event
/// snapshots must remain stable until the callback requests session reload.
static TaskDesc _gRoomEventStagedTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Driveway scene recipes: slot 0 wire-rope blackout, slot 1 bat encounter, slot 2 end.
///
/// Both live recipes are bodyless priority-32 tasks with unused spawn arguments.
/// The controllers borrow this overlay's scripts and resources through completion.
static TaskDesc _gDryfieldDrivewaySceneTasks[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _drivewayBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _drivewayCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Encounter extension words: clip 47's borrowed set followed by a null clip 48.
///
/// Both words are copied into the equipped-weapon bank. The room clip and its
/// data must remain loaded while that bank is used for playback.
static AnimationSet* _gDryfieldDrivewayEncounterAnimationBankExtension[2] = {
    &_gDryfieldDrivewayAnimation00D08,
    NULL,
};

/// Copies the encounter's two extension words into the player's equipped-weapon bank.
///
/// The request and source words are read during synchronous dispatch; the
/// installed clip pointer remains borrowed from this overlay during playback.
static AnimationBankCopyRequest _gDryfieldDrivewayEncounterAnimationBankCopy = {
    .sets  = _gDryfieldDrivewayEncounterAnimationBankExtension,
    .count = ARRAY_SIZE(_gDryfieldDrivewayEncounterAnimationBankExtension),
};

/// Restarts the encounter player's extension clip 47 with world-grid participation.
///
/// Event playback copies the request and overwrites the stored selector with
/// the equipped-weapon bank before synchronous dispatch at the second CAP cue.
static AnimationPlayRequest _gDryfieldDrivewayEncounterPlayerAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 48; its receiver and playback role are unproven.
static AnimationPlayRequest D_dryfield_driveway_8017E344 = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 7,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Blends the encounter player into clip 32 over five frames without world-grid participation.
///
/// Borrowed for the event interpreter's copy, which substitutes the equipped
/// weapon's bank selector before synchronous dispatch. The stored selector is unused.
static AnimationPlayRequest _gDryfieldDrivewayEncounterPlayerAnimation32 = {
    .source               = { .index = 1 },
    .animationId          = 32,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced transform-shaped record; its actor, type identity and placement role are unproven.
static ActorTransform D_dryfield_driveway_8017E36C = {
    .pos = { -1067, 0, 1407, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Bat-encounter sequence synchronized to the three cues of CAP command 4.
///
/// Installs player clips 47/48, plays clip 32, releases the bat waves at the
/// first cue, plays clip 47 at the second and engages combat at the third.
/// The final message returns player control after an update boundary. Keep
/// the script and borrowed animation data loaded through playback.
static EvsCommand _gDryfieldDrivewayEncounterScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRIVEWAY_CAP_ENCOUNTER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldDrivewayEncounterAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDrivewayEncounterPlayerAnimation32 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 0x0C) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDrivewaySetEncounterWave }, { .value = DRYFIELD_DRIVEWAY_ENCOUNTER_WAVE_BEGIN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDrivewayEncounterPlayerAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRIVEWAY_SCRIPT_WAIT_NEXT_UPDATE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Restarts player clip 1 at blackout entry without blending or world-grid participation.
///
/// The interpreter borrows the request for its copy and substitutes the
/// equipped-weapon bank selector before synchronous dispatch.
static AnimationPlayRequest _gDryfieldDrivewayBlackoutPlayerAnimation1 = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts player clip 24 at the blackout's final CAP cue without world-grid participation.
///
/// The interpreter borrows the request for its copy and substitutes the
/// equipped-weapon bank selector before synchronous dispatch.
static AnimationPlayRequest _gDryfieldDrivewayBlackoutPlayerAnimation24 = {
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
static EvsCommand _gDryfieldDrivewayBlackoutScript[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRIVEWAY_CAP_BLACKOUT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldDrivewaySetPlayerUpdateHold }, { .value = DRIVEWAY_PLAYER_UPDATE_RUN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = DRIVEWAY_DISPLAY_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDrivewayBlackoutPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 9) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _drivewaySetViewDirty }, { .value = DRIVEWAY_SAVED_VIEW_RELOAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = DRIVEWAY_BLACKOUT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRIVEWAY_BLACKOUT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDrivewayBlackoutPlayerAnimation24 }, { .value = 0 } },
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
static EvsCommand _gDryfieldDrivewayBlackoutSkipScript[9] = {
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

/// Driveway room receiver: departure scenes, wire-rope use, ignored actions and CAP sounds.
///
/// Installed for the room task's lifetime; the final sentinel returns zero
/// for unsupported message IDs. Payload contracts belong to each selected handler.
static TaskMessageEntry _gDryfieldDrivewayRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveDriveway },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldDrivewayUseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldDrivewayIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldDrivewayIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _drivewayScriptSound },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gDryfieldDrivewayRoomCollisionResources[2] = {
    { &_gDryfieldDrivewayCollisionGrid, _gDryfieldDrivewayViewBoundaryTriggers, _gDryfieldDrivewayActionTriggers, NULL },
    { &_gDryfieldDrivewayCollisionGrid, _gDryfieldDrivewayViewBoundaryTriggers, _gDryfieldDrivewayActionTriggers, NULL },
};

/// Logical-view to resource-slot map after the wire rope is used.
///
/// The first seven bytes map views 1..7; the retained eighth zero is outside
/// that domain, not a list terminator. Logical view 4 selects resource slot 7.
static u8 _gDryfieldDrivewayAfterRopeViewMap[8] = {
    1,
    2,
    3,
    7,
    5,
    6,
    4,
    0,
};

u8* gDryfieldDrivewayRoomViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldDrivewayAfterRopeViewMap,
};

WorldCoordRoomLighting gDryfieldDrivewayRoomLighting[2] = {
    { &_gDryfieldDrivewayRoomLights, NULL },
    { &_gDryfieldDrivewayRoomLights, NULL },
};

ViewCount gDryfieldDrivewayRoomViewCounts[2] = { 7, 7 };

enum {
    DRYFIELD_DRIVEWAY_WARP_YAW_QUARTER_TURN      = 1024,
    DRYFIELD_DRIVEWAY_WARP1_MAP_FLAG             = 474,
    DRYFIELD_DRIVEWAY_WARP3_MAP_FLAG             = 482,
    DRYFIELD_DRIVEWAY_FACTORY_WARP_ENDPOINTS     = (1 << 4) | 1,
    DRYFIELD_DRIVEWAY_WATER_HOLE_WARP_ENDPOINTS  = (2 << 4) | 1,
    DRYFIELD_DRIVEWAY_MAIN_STREET_WARP_ENDPOINTS = (3 << 4) | 7,
    DRYFIELD_DRIVEWAY_ACTION_CAP_COMMAND         = 7,
};

DirectionWarpEntry gDryfieldDrivewayWarpEntries[3] = {
    { { { .word = DRYFIELD_DRIVEWAY_WARP_YAW_QUARTER_TURN }, -3400, 0, 600 }, { 0, 0, 0, 0 }, { { .word = DRYFIELD_DRIVEWAY_WARP_YAW_QUARTER_TURN }, -3400, 0, 1250 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 4), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 3), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DRYFIELD_DRIVEWAY_WARP1_MAP_FLAG },
    { { { .word = ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT }, -1300, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = DRYFIELD_DRIVEWAY_WARP_YAW_QUARTER_TURN }, 200, 0, 1650 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = DRYFIELD_DRIVEWAY_WARP_YAW_QUARTER_TURN }, -9500, 4, -1418 }, { 0, 0, 0, 0 }, { { .word = DRYFIELD_DRIVEWAY_WARP_YAW_QUARTER_TURN }, 200, 0, 1650 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_DRIVEWAY_WARP3_MAP_FLAG },
};

/// Room-space collision normals, with `ONE` representing unit length.
static SVECTOR _gDryfieldDrivewayCollisionGridNormals[13] = {
#include "assets/dryfield_driveway_collision_017B4_normals.inc"
};

/// Collision vertices in signed room-space game coordinates.
static SVECTOR _gDryfieldDrivewayCollisionGridVerts[64] = {
#include "assets/dryfield_driveway_collision_017B4_verts.inc"
};

/// Indexed collision quads and their room-local surface classes.
static WorldCollisionGridFace _gDryfieldDrivewayCollisionGridFaces[23] = {
#include "assets/dryfield_driveway_collision_017B4_faces.inc"
};

/// Signed face-index lists ending with `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldDrivewayCollisionGridCells[154] = {
#include "assets/dryfield_driveway_collision_017B4_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldDrivewayCollisionGridCells` (154 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_DRIVEWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldDrivewayCollisionGridCells[(halfwordOffset)])

/// X-major face-list pointers for the seven by three XZ cells; NULL is empty.
static s16* _gDryfieldDrivewayCollisionGridTable[21] = {
#include "assets/dryfield_driveway_collision_017B4_table.inc"
};
#undef DRYFIELD_DRIVEWAY_CELL_FACE_LIST_AT_OFFSET

/// Driveway collision mesh and its seven by three XZ cell index.
///
/// Cells are 4000 game units wide, with origin (-11030, -5210) on XZ.
/// Both room selectors borrow this mutable grid; setup binds its view coordinate.
/// The descriptor, mesh pools and cell lists remain live while this overlay is loaded.
static WorldCollisionGrid _gDryfieldDrivewayCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldDrivewayCollisionGridNormals,
    .vertices    = _gDryfieldDrivewayCollisionGridVerts,
    .faces       = _gDryfieldDrivewayCollisionGridFaces,
    .cellFaceIds = _gDryfieldDrivewayCollisionGridTable,
    .xBias       = 0x2B16,
    .zBias       = 5210,
    .cellCountX  = 7,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldDrivewayCollisionGridFaces),
};

enum {
    DRYFIELD_DRIVEWAY_BAT_RESOURCE_ID = 37,
    DRYFIELD_DRIVEWAY_BAT_FILE_NUMBER = 37,
};

/// Bat file and task selection for placement variant 1, followed by the end record.
///
/// Resource 37 serves all 25 bat placements; base group 10 and file number 37
/// select actor file 103700. The resource and borrowed task descriptor must stay loaded.
static AreaResource _gDryfieldDrivewayBatResources[2] = {
    { DRYFIELD_DRIVEWAY_BAT_RESOURCE_ID, DRYFIELD_DRIVEWAY_BAT_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Empty, terminated resource list paired with driveway placement variant 2.
static AreaResource _gDryfieldDrivewayVariant2Resources[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource ID and file number for the regular Desert Chaser in placement variant 3.
enum {
    DRYFIELD_DRIVEWAY_DESERT_CHASER_RESOURCE_ID = 1,
    DRYFIELD_DRIVEWAY_DESERT_CHASER_FILE_NUMBER = 1,
};

/// Desert Chaser file and task recipe for driveway placement variant 3, then the end record.
///
/// Placement entry 1 selects file 400100 (base group 40, file number 1) and
/// task descriptor 0. The room recipe and actor descriptor remain borrowed
/// during loading and spawning; the directory's publication does not prove reachability.
static AreaResource _gDryfieldDrivewayVariant3Resources[2] = {
    {
        .entryId        = DRYFIELD_DRIVEWAY_DESERT_CHASER_RESOURCE_ID,
        .fileNumber     = DRYFIELD_DRIVEWAY_DESERT_CHASER_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor00100DesertChaserTaskDesc,
    },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldDrivewayAreaVariants[11] = {
    { NULL, NULL },
    { gMapDryfieldDrivewayVariant1Placements, _gDryfieldDrivewayBatResources },
    { gMapDryfieldDrivewayVariant2Placements, _gDryfieldDrivewayVariant2Resources },
    { gMapDryfieldDrivewayVariant3Placements, _gDryfieldDrivewayVariant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

ViewCamera gDryfieldDrivewayViewCameras[7] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 1950, 0x7530, -2670 } }, 380 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { -781, 0, -4020 }, { -1703, 3710, 331 }, { 3642, 1735, -707 } }, { 1820, 2220, -2312 } }, 230 },
    { { { { 287, 0, 4085 }, { 2747, 3031, -193 }, { -3024, 2754, 213 } }, { 29, 1719, -2342 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
};

/// Empty mapped-view-1 sprite list retaining decoded background strips.
static SpriteBatch _gDryfieldDrivewayView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Nine depth-sorted texture rectangles for mapped view 2, in drawing-origin pixels.
static SpriteSource _gDryfieldDrivewayView2SpriteSources[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
};

/// Two source ranges covering mapped view 2, after the background-strip selector.
static SpriteBatch _gDryfieldDrivewayView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, ARRAY_SIZE(_gDryfieldDrivewayView2SpriteSources) - 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seventeen depth-sorted texture rectangles for mapped view 3, in drawing-origin pixels.
static SpriteSource _gDryfieldDrivewayView3SpriteSources[17] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

/// Five source ranges covering mapped view 3, after the background-strip selector.
static SpriteBatch _gDryfieldDrivewayView3SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 6, 0, 0, { 3, 0 } },
    { 12, 1, 0, 0, { 2, 0 } },
    { 13, 2, 0, 0, { 4, 0 } },
    { 15, ARRAY_SIZE(_gDryfieldDrivewayView3SpriteSources) - 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped-view-3 clip in draw-buffer pixels, restored to full screen at depth 925.
///
/// The second record ends the list; clipping affects depth-sorted scene objects too.
static SpriteDrawArea _gDryfieldDrivewayView3SpriteDrawAreas[2] = {
    { { 0, 0, 231, 239 }, 925 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Fifty depth-sorted texture rectangles for mapped view 4, before using the rope.
static SpriteSource _gDryfieldDrivewayView4SpriteSources[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1125, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 8, 1125, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -160, -72, 885, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -40, -120, 1800, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -16, -120, 1825, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 8, -120, 1850, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 850, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -16, 876, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 850, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 48, 495, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -104, 758, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 575, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 908, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 883, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 883, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 883, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, 8, 883, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 1064, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 1064, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, 0, 1064, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 0, 1064, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 8, 1064, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 80, -96, 885, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, 40, 871, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 48, 883, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 56, 883, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 88, 48, 883, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 883, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
};

/// Mapped-view-4 ranges covering all fifty sources, including one retained empty range.
static SpriteBatch _gDryfieldDrivewayView4SpriteBatches[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 7, 0 } },
    { 5, 0, 0, 0, { 2, 0 } },
    { 5, 3, 0, 0, { 6, 0 } },
    { 8, 3, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 9, 0 } },
    { 18, 9, 0, 0, { 0, 0 } },
    { 27, 3, 0, 0, { 10, 0 } },
    { 30, 4, 0, 0, { 5, 0 } },
    { 34, 5, 0, 0, { 8, 0 } },
    { 39, 5, 0, 0, { 4, 0 } },
    { 44, 1, 0, 0, { 11, 0 } },
    { 45, ARRAY_SIZE(_gDryfieldDrivewayView4SpriteSources) - 45, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Twelve depth-sorted texture rectangles for mapped view 5, in drawing-origin pixels.
static SpriteSource _gDryfieldDrivewayView5SpriteSources[12] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 8, 948, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 16, 888, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 24, 895, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 969, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 32, 890, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 40, 885, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 48, 880, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 56, 880, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 32, 650, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 80, 650, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, 32, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 72, 681, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
};

/// Two source ranges covering mapped view 5, after the background-strip selector.
static SpriteBatch _gDryfieldDrivewayView5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, ARRAY_SIZE(_gDryfieldDrivewayView5SpriteSources) - 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-6 sprite list retaining decoded background strips.
static SpriteBatch _gDryfieldDrivewayView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sixty depth-sorted texture rectangles for the after-rope version of logical view 4.
///
/// Room selector 2 maps that logical view to resource slot 7.
static SpriteSource _gDryfieldDrivewayView7SpriteSources[60] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1125, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 8, 1125, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -160, -72, 885, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -40, -120, 1800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -16, -120, 1825, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 8, -120, 1664, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 850, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -16, 875, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 850, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 48, 850, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -104, 758, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -80, 851, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -56, 837, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -96, 847, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -64, 845, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -56, 837, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -32, 837, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -56, 845, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -40, 845, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -24, 845, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 1082, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, -16, 837, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 507, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 908, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 883, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 883, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 883, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 8, 858, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 1064, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 1064, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, 0, 1064, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 0, 1064, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 8, 1064, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 40, 883, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 48, 883, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 56, 883, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 48, 883, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 883, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
};

/// Mapped-view-7 ranges covering all sixty sources, including one retained empty range.
static SpriteBatch _gDryfieldDrivewayView7SpriteBatches[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 7, 0 } },
    { 5, 0, 0, 0, { 2, 0 } },
    { 5, 3, 0, 0, { 6, 0 } },
    { 8, 3, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 9, 0 } },
    { 18, 9, 0, 0, { 0, 0 } },
    { 27, 3, 0, 0, { 10, 0 } },
    { 30, 11, 0, 0, { 5, 0 } },
    { 41, 4, 0, 0, { 8, 0 } },
    { 45, 5, 0, 0, { 4, 0 } },
    { 50, 5, 0, 0, { 11, 0 } },
    { 55, ARRAY_SIZE(_gDryfieldDrivewayView7SpriteSources) - 55, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldDrivewaySpriteViews[7] = {
    { { .empty = _gDryfieldDrivewayView1SpriteBatches }, _gDryfieldDrivewayView1SpriteBatches, NULL },
    { { .elements = _gDryfieldDrivewayView2SpriteSources }, _gDryfieldDrivewayView2SpriteBatches, NULL },
    { { .elements = _gDryfieldDrivewayView3SpriteSources }, _gDryfieldDrivewayView3SpriteBatches, _gDryfieldDrivewayView3SpriteDrawAreas },
    { { .elements = _gDryfieldDrivewayView4SpriteSources }, _gDryfieldDrivewayView4SpriteBatches, NULL },
    { { .elements = _gDryfieldDrivewayView5SpriteSources }, _gDryfieldDrivewayView5SpriteBatches, NULL },
    { { .empty = _gDryfieldDrivewayView6SpriteBatches }, _gDryfieldDrivewayView6SpriteBatches, NULL },
    { { .elements = _gDryfieldDrivewayView7SpriteSources }, _gDryfieldDrivewayView7SpriteBatches, NULL },
};

/// Six mutable directed boundaries for logical views 2/3, 3/4 and 4/5.
///
/// Source/destination pairs share the room's view space; the final boundary
/// carries LAST. Setup binds coordinates and list links, and collision updates
/// hit latches. Keep the room storage loaded until all triggers are unlinked.
static WorldCollisionTrigger _gDryfieldDrivewayViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -6369, -1536, -1504, 0 }, { { 0, -2560, 1024, 0 }, { 0, -2560, -1024, 0 }, { 0, 2560, 1024, 0 }, { 0, 2560, -1024, 0 } }, { -4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6816, -1472, -1568, 0 }, { { 0, -2496, -1024, 0 }, { 0, -2496, 1024, 0 }, { 0, 2496, -1024, 0 }, { 0, 2496, 1024, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2770, -1600, -856, 0 }, { { -1521, -2624, 0, 0 }, { 1522, -2624, 1, 0 }, { -1521, 2624, 0, 0 }, { 1522, 2624, 1, 0 } }, { 1, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2675, -1376, -1017, 0 }, { { 1554, -2400, 1, 0 }, { -1553, -2400, 0, 0 }, { 1554, 2400, 1, 0 }, { -1553, 2400, 0, 0 } }, { -3, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 294, -1520, 1474, 0 }, { { -435, -2544, 2215, 0 }, { 435, -2544, -2214, 0 }, { -435, 2544, 2215, 0 }, { 435, 2544, -2214, 0 } }, { -4034, 0, -793, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -26, -1488, 1301, 0 }, { { 435, -2512, -2213, 0 }, { -435, -2512, 2214, 0 }, { 435, 2512, -2213, 0 }, { -435, 2512, 2214, 0 } }, { 4031, 0, 792, 0 }, { 0, 0, 4096, 0 }, 3367, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Twelve mutable point lights shared by both driveway room selectors and all views.
///
/// Placement and falloff radii use game coordinates; RGB intensity has twelve
/// fractional bits. Room binding and shading update their parents, caches and attenuation.
static WorldCoordPointLight _gDryfieldDrivewayPointLights[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x28AE, -2000, 2764 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 1966, 2048 }, { 0, 0 } }, 0x61A7, 0x61A8 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4000, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 4104 }, { 0, 0 } }, 499, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4990, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4098, 4107 }, { 0, 0 } }, 339, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5990, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4106, 4108, 4107 }, { 0, 0 } }, 438, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6892, -2000, 2764 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 1966, 2048 }, { 0, 0 } }, 0x61A7, 0x61A8 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4508, 4508, 4506 }, { 0, 0 } }, 737, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4099, 4097 }, { 0, 0 } }, 499, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4106, 4122, 4158 }, { 0, 0 } }, 398, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2990, -2000, 2220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4112, ONE, 4105 }, { 0, 0 } }, 498, 2000 },
};

/// Shared driveway light collection: twelve point lights, no directional or cone lights.
static WorldCoordRoomLights _gDryfieldDrivewayRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldDrivewayPointLights),
    .pointLights           = _gDryfieldDrivewayPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Eleven mutable driveway action regions, including four wire-rope event regions.
///
/// Six regions warp to the factory, water hole or main street, one runs CAP 7,
/// and four expose the room-event sentinel. Warp parameters pack 1-based current
/// and destination endpoints in the high and low nibbles. The final region
/// carries LAST; setup and hit tests mutate coordinates, links, enables and latches.
/// Coordinates use room game units and normals use 4096 per unit. Keep storage
/// loaded until unlinked; both room selectors share these records.
static WorldCollisionTrigger _gDryfieldDrivewayActionTriggers[11] = {
    { NULL, NULL, NULL, { -3680, -48, 480, 0 }, { { -352, 0, -640, 0 }, { 352, 0, -640, 0 }, { -352, 0, 640, 0 }, { 352, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_FACTORY, DRYFIELD_DRIVEWAY_FACTORY_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1488, -48, 2400, 0 }, { { -1104, 0, -1120, 0 }, { 1104, 0, -1120, 0 }, { -1104, 0, 1120, 0 }, { 1104, 0, 1120, 0 } }, { 0, 4117, 0, 0 }, { 4096, 0, 0, 0 }, 1567, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_HOLE, DRYFIELD_DRIVEWAY_WATER_HOLE_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9664, -48, -1440, 0 }, { { -352, 0, -352, 0 }, { 352, 0, -352, 0 }, { -352, 0, 352, 0 }, { 352, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 497, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MAIN_STREET, DRYFIELD_DRIVEWAY_MAIN_STREET_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5984, -64, 1296, 0 }, { { -352, 0, -1072, 0 }, { 352, 0, -1072, 0 }, { -352, 0, 1072, 0 }, { 352, 0, 1072, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_DRIVEWAY_ACTION_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2208, -64, 2720, 0 }, { { -400, 0, -832, 0 }, { 400, 0, -832, 0 }, { -400, 0, 832, 0 }, { 400, 0, 832, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 923, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_HOLE, DRYFIELD_DRIVEWAY_WATER_HOLE_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -768, -64, 2720, 0 }, { { -400, 0, -800, 0 }, { 400, 0, -800, 0 }, { -400, 0, 800, 0 }, { 400, 0, 800, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_HOLE, DRYFIELD_DRIVEWAY_WATER_HOLE_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1456, -64, 1648, 0 }, { { -672, 0, -400, 0 }, { 672, 0, -400, 0 }, { -672, 0, 400, 0 }, { 672, 0, 400, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 781, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_HOLE, DRYFIELD_DRIVEWAY_WATER_HOLE_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1504, -64, 1632, 0 }, { { -1072, 0, -1008, 0 }, { 1104, 0, -1008, 0 }, { -1104, 0, 400, 0 }, { 1072, 0, 400, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2208, -64, 2512, 0 }, { { -1040, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -1072, 0, 1024, 0 }, { 368, 0, 1024, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1481, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -768, -64, 2464, 0 }, { { -400, 0, -1024, 0 }, { 912, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 912, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1504, -64, 2400, 0 }, { { -1744, 0, -1760, 0 }, { 1712, 0, -1760, 0 }, { -1744, 0, 1120, 0 }, { 1712, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 2468, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    DRYFIELD_DRIVEWAY_FOOTSTEP_WALK_REQUEST = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1D,
    DRYFIELD_DRIVEWAY_FOOTSTEP_RUN_REQUEST  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1F,
};

/// Paired type-1 sound requests for surface class 2's walking, running and scripted jumps.
///
/// Cue 2 uses each base; cue 1 uses base + 1. Scripted jumps share the walk pair.
/// Requests select the loaded type-1 sound bank; the surface borrows this room-owned record.
static WorldCollisionFootstepSounds _gDryfieldDrivewayFootstepSounds = {
    .walk         = DRYFIELD_DRIVEWAY_FOOTSTEP_WALK_REQUEST,
    .run          = DRYFIELD_DRIVEWAY_FOOTSTEP_RUN_REQUEST,
    .scriptedJump = DRYFIELD_DRIVEWAY_FOOTSTEP_WALK_REQUEST,
};

/// Blocking, impact-enabled surface policy without footstep sounds, for classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gDryfieldDrivewayBlockingSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-1 policy: probes pass, weapon impacts are disabled, pushback remains enabled.
static WorldCollisionSurfaceProperties _gDryfieldDrivewayProbePassSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-2 blocking, impact-enabled surface policy with paired footstep sounds.
static WorldCollisionSurfaceProperties _gDryfieldDrivewayFootstepSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldDrivewayFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldDrivewaySurfacePropertiesByClass[8] = {
    &_gDryfieldDrivewayBlockingSurfaceProperties,
    &_gDryfieldDrivewayProbePassSurfaceProperties,
    &_gDryfieldDrivewayFootstepSurfaceProperties,
    &_gDryfieldDrivewayBlockingSurfaceProperties,
    &_gDryfieldDrivewayBlockingSurfaceProperties,
    &_gDryfieldDrivewayBlockingSurfaceProperties,
    &_gDryfieldDrivewayBlockingSurfaceProperties,
    &_gDryfieldDrivewayBlockingSurfaceProperties,
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

/// Unreferenced byte initialized to 2; its role and relationship to adjacent storage are unproven.
static u8 D_dryfield_driveway_80180691 = 2;

/// Unreferenced byte initialized to 240; its role and relationship to adjacent storage are unproven.
static u8 D_dryfield_driveway_80180692 = 240;

/// Unreferenced byte initialized to 207; its role and relationship to adjacent storage are unproven.
static u8 D_dryfield_driveway_80180693 = 207;

RoomLatchedEvent gRoomEventLatched = { 0 };

#include "../../shared/room/room_event_staged_task.inc.c"

/// Supplies the daytime room's scene task table for driveway departure resolution.
///
/// Expands to a private three-entry `TaskDesc` array, passed as `TaskDesc*`.
/// Slot 1 is the bodyless priority-32 scripted encounter; slot 0 is the wire-rope
/// blackout and slot 2 the terminator. Spawning reads the selected recipe
/// synchronously with unused payload words. Keep its callback code and borrowed
/// resources loaded while used. This identifier binding has no arguments or
/// side effects and is required only by the following resolver fragment.
#define ROOM_VARIANT_DRIVEWAY_SCENE_TASK_TABLE _gDryfieldDrivewaySceneTasks
#include "../../shared/dryfield_driveway_resolve.inc.c"
#undef ROOM_VARIANT_DRIVEWAY_SCENE_TASK_TABLE

/// Selects the wire-rope blackout entry stream for this room's shared controller.
///
/// Expands to a private END-terminated `EvsCommand` array, passed as
/// `EvsCommand*` with `DRIVEWAY_BLACKOUT_SKIP_SCRIPT` as its skip continuation.
/// Playback borrows the stream and its resources until the interpreter ends,
/// beyond the controller task's lifetime. This identifier binding has no
/// arguments or side effects and is needed only by the following fragment.
#define DRIVEWAY_BLACKOUT_SCRIPT _gDryfieldDrivewayBlackoutScript
/// Selects the wire-rope blackout's input-skip continuation for the shared controller.
///
/// Expands to this room's private nine-command, END-terminated `EvsCommand`
/// array, passed as `EvsCommand*` to `evsStartScriptWithSkip`. It reloads the
/// saved view and restores player control behind a fade. Playback borrows the
/// stream until the interpreter ends, beyond the controller task's lifetime.
/// This identifier binding has no arguments or side effects; define it only
/// around the blackout fragment, alongside `DRIVEWAY_BLACKOUT_SCRIPT`.
#define DRIVEWAY_BLACKOUT_SKIP_SCRIPT _gDryfieldDrivewayBlackoutSkipScript
#include "../../shared/dryfield_driveway_blackout.inc.c"
#undef DRIVEWAY_BLACKOUT_SKIP_SCRIPT
#undef DRIVEWAY_BLACKOUT_SCRIPT

/// Selects this room's bat-encounter command stream for the shared controller.
///
/// Expands to a private END-terminated `EvsCommand` array, passed as
/// `EvsCommand*` to `evsStartScript` with HUD hide/restore and no input skip
/// target. The interpreter borrows the stream and callbacks until it ends;
/// the installed player animation data stays borrowed during bank playback.
/// The script releases the bat waves and restores player control. This
/// identifier binding has no arguments or side effects and is needed only
/// by the following fragment.
#define DRIVEWAY_ENCOUNTER_SCRIPT _gDryfieldDrivewayEncounterScript
#include "../../shared/dryfield_driveway_cutscene.inc.c"
#undef DRIVEWAY_ENCOUNTER_SCRIPT

/// Sets the actor 03700 encounter's entrance and wave stage from an event script.
///
/// Stores the argument's low signed byte without clamping: 0 is the initial stage,
/// 1 begins wave progression, and 2..5 release successive waves. The script passes
/// `DRYFIELD_DRIVEWAY_ENCOUNTER_WAVE_BEGIN`; actor tasks advance the later stages.
static void _dryfieldDrivewaySetEncounterWave(s32 waveStage)
{
    gSceneCombatState.actor03700Wave = waveStage;
}

#include "../../shared/dryfield_driveway_set_view_dirty.inc.c"

/// Sets the player tick hold during the driveway blackout scene.
///
/// The script supplies the low unsigned byte of its argument: 0 runs the
/// player tick, nonzero holds it. Movement and collision processing still run.
static void _dryfieldDrivewaySetPlayerUpdateHold(u8 holdPlayerUpdate)
{
    gPlayerActorStateTickHeld = holdPlayerUpdate;
}

#include "../../shared/dryfield_driveway_script_sound.inc.c"

#include "../../shared/room/room_event_trigger_hit.inc.c"

/// Uses the wire rope at the driveway's room-event region to start the blackout.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; itemId is a collected-item ID and the
/// second payload word is unused. Requires progress 1 and a hit room-event
/// region. Advances progress to 2, selects room 2 in session and live save,
/// hides the HUD and blocks other events. Returns the used-notice reply on
/// success, or the refused reply without side effects. The blackout task
/// clears the rope's collected bit on its later update.
static s32 _dryfieldDrivewayUseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_DRIVEWAY_PROGRESS_ROPE_READY = 1,
        DRYFIELD_DRIVEWAY_PROGRESS_ROPE_USED  = 2,
        DRYFIELD_DRIVEWAY_ROOM_AFTER_ROPE     = 2,
        DRYFIELD_DRIVEWAY_BLACKOUT_TASK       = 0,
        DRYFIELD_DRIVEWAY_HIDE_HUD            = 1,
        DRYFIELD_DRIVEWAY_EVENT_ACTIVE        = 1,
    };

    if (itemId == INVENTORY_COLLECTION_ID_WIRE_ROPE) {
        if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) == DRYFIELD_DRIVEWAY_PROGRESS_ROPE_READY) {
            if (_roomEventTriggerHit() != 0) {
                gameFlagSetNibble(GAME_FLAG_DRIVEWAY_PROGRESS, DRYFIELD_DRIVEWAY_PROGRESS_ROPE_USED);
                taskSpawnFromTableOnDefaultList(_gDryfieldDrivewaySceneTasks, DRYFIELD_DRIVEWAY_BLACKOUT_TASK, 0, 0);
                gGameSession->location.roomState = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = DRYFIELD_DRIVEWAY_ROOM_AFTER_ROPE);
                gGameSession->hideHud            = DRYFIELD_DRIVEWAY_HIDE_HUD;
                gGameSession->eventState         = DRYFIELD_DRIVEWAY_EVENT_ACTIVE;
                return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
            }
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Answers `ROOM_MESSAGE_COMMAND` with zero without executing a room command.
///
/// All four callback arguments are ignored; no task or payload is accessed.
static s32 _dryfieldDrivewayIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg)
{
    return 0;
}

/// Answers `DIRECTION_MESSAGE_ROOM_ACTION` with zero without executing an action.
///
/// The borrowed action request stays an unused argument word; it is never
/// dereferenced or retained. All other callback arguments are ignored too.
static s32 _dryfieldDrivewayIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedRequestWord, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the driveway's message handlers and registers its live room task.
///
/// Requires state 0; advances to idle state 1 without allocating work.
static void _dryfieldDrivewayInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldDrivewayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room task idle after its message table and room slot are installed.
///
/// Ignores the task argument and leaves the task in this state until another
/// owner changes its state or destroys it.
static void _dryfieldDrivewayIdleRoomTask(Task* unusedTask)
{
    // Retain the target's unused 16-byte stack frame; it has no accessed payload.
    char stackReservation[0x10];
}

/// Room-task handlers for state 0 initialize, 1 idle and 2 destroy.
///
/// A complete by-value copy preserves the three callback addresses on the
/// stack. Dispatch requires state 0..2 and a live task; there is no terminator.
static const TaskFuncTable3 _gDryfieldDrivewayRoomTaskStates = {
    {
        _dryfieldDrivewayInitRoomTask,
        _dryfieldDrivewayIdleRoomTask,
        taskKill,
    },
};

void dryfieldDrivewayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldDrivewayRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void dryfieldDrivewayEnableAmbientEffectsTask(Task* unusedTask)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
