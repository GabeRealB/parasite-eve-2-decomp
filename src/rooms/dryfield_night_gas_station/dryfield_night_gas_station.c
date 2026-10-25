#include "rooms/dryfield_night_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00100.h"
#include "actors/actor_01500.h"
#include "actors/actor_01600.h"
#include "actors/actor_02300.h"
#include "actors/actor_05700.h"
#include "actors/actor_400600.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"
#include "../../shared/gas_station_sounds.h"

static Task* _gDryfieldNightGasStationCompanionHeadAimTask;
static Task* _gDryfieldNightGasStationRedStreakTask;

static WorldCollisionGrid _gDryfieldNightGasStationRooms1To3CollisionGrid;

static Task* _gDryfieldNightGasStationSpriteFlickerTask;

#include "../../shared/room/telephone.h"

static void _dryfieldNightGasStationResetCutsceneObstacleCollision(s32 displaceObstacle);
static void _dryfieldNightGasStationDrawTrackRedStreak(s32 frameIndex);
static void _dryfieldNightGasStationDrawCompanionRedStreak(s32 unusedFrame);
static void _dryfieldNightGasStationClearCutsceneTaskHandles(void);
static void _dryfieldNightGasStationRestoreBalconySpriteVisibility(void);
static void _dryfieldNightGasStationSetFlickerSpritesVisible(s16 visible);

/// Commands for the player-to-companion head-aim blend task.
enum {
    DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_STOP       = -1,
    DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_OFF        = 0,
    DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_ON         = 1,
    DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_BLEND_STEP = ONE / 16,
    DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_MAX_YAW    = 0x300,
    DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_MAX_PITCH  = 0x10
};

/// States of the local cutscene tasks; the companion streak has an initialization tick.
enum {
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_EXIT    = -1,
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_RUNNING = 0,
    DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_INIT = 0,
    DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_DRAW = 1
};

/// Only this obstacle prefix is restored; the rest of the live room grid is retained.
enum {
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_FACE_COUNT     = 4,
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_VERTEX_COUNT   = 8,
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_DISPLACEMENT_Y = 3000
};

/// Red-streak rendering uses angle units of 1/4096 turn and fixed ordering depth.
enum {
    DRYFIELD_NIGHT_GAS_STATION_TRACK_YAW              = -0x262,
    DRYFIELD_NIGHT_GAS_STATION_RED_STREAK_OT_DEPTH    = 10,
    DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_PART  = 8,
    DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_TICKS = 100
};

/// Visibility arguments for the cutscene's background batches.
enum {
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_SPRITES_SHOWN,
    DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_SPRITES_HIDDEN
};

/// Appearance arguments shared by the timed flashes and the flicker task.
enum {
    DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_OFF,
    DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_ON
};

/// Progress values controlling the actor event and its deferred follow-up scene.
enum {
    DRYFIELD_NIGHT_GAS_STATION_PROGRESS_INITIAL,
    DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_PENDING,
    DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_STARTED
};

/// Script selectors for the three flashes around placed actor 2; other values hide them.
enum {
    DRYFIELD_NIGHT_GAS_STATION_FLASH_OFF = -1,
    DRYFIELD_NIGHT_GAS_STATION_FLASH_FIRST,
    DRYFIELD_NIGHT_GAS_STATION_FLASH_SECOND,
    DRYFIELD_NIGHT_GAS_STATION_FLASH_THIRD
};

static void _dryfieldNightGasStationCompanionHeadAimTask(Task* task);

static AnimationSet _gDryfieldNightGasStationAnimation070B4;
static AnimationSet _gDryfieldNightGasStationAnimation073A4;
static AnimationSet _gDryfieldNightGasStationAnimation077A4;
static AnimationSet _gDryfieldNightGasStationAnimation07A58;
static AnimationSet _gDryfieldNightGasStationAnimation07CFC;

static AnimationSet _gDryfieldNightGasStationAnimation07F78;
static AnimationSet _gDryfieldNightGasStationAnimation08148;
static AnimationSet _gDryfieldNightGasStationAnimation08318;
static AnimationSet _gDryfieldNightGasStationAnimation0885C;
static AnimationSet _gDryfieldNightGasStationAnimation09344;
static AnimationSet _gDryfieldNightGasStationAnimation09D80;
static AnimationSet _gDryfieldNightGasStationAnimation0A3FC;
static AnimationSet _gDryfieldNightGasStationAnimation0A698;
static AnimationSet _gDryfieldNightGasStationAnimation0AA3C;
static AnimationSet _gDryfieldNightGasStationAnimation0AF98;

static WorldCollisionGrid         _gDryfieldNightGasStationRoom4CollisionGrid;
static WorldCollisionTrigger      _gDryfieldNightGasStationViewBoundaryTriggers[11];
static WorldCollisionTrigger      _gDryfieldNightGasStationActionTriggers[16];
static WorldCoordRoomAmbientEntry _gDryfieldNightGasStationAmbientByView[22];
static WorldCoordRoomLights       _gDryfieldNightGasStationRoom1Lights;
static WorldCoordRoomLights       _gDryfieldNightGasStationRooms2To4Lights;
static void                       _dryfieldNightGasStationSetCutsceneFlash(s32 flashIndex);
static void                       _dryfieldNightGasStationStageSceneAudioStart(void);
static void                       _dryfieldNightGasStationEnqueueScenePlayback(void);
static void                       _dryfieldNightGasStationFinishScenePlayback(void);
static void                       _dryfieldNightGasStationCancelScenePlayback(void);
static void                       _dryfieldNightGasStationSpawnCompanionHeadAimTask(void);
static void                       _dryfieldNightGasStationSetCompanionHeadAim(s32 aimMode);
static void                       _dryfieldNightGasStationEngageBattle(s32 unusedArg);
static void                       _dryfieldNightGasStationSpawnTrackRedStreakTask(void);
static void                       _dryfieldNightGasStationStopRedStreakTask(void);
static void                       _dryfieldNightGasStationSpawnSpriteFlickerTask(void);
static void                       _dryfieldNightGasStationStopSpriteFlicker(void);
static void                       _dryfieldNightGasStationSpawnCompanionRedStreakTask(void);
static void                       _dryfieldNightGasStationStopCompanionRedStreakTask(void);
static void                       _dryfieldNightGasStationCancelCutsceneEffects(void);
static void                       _dryfieldNightGasStationSetCutsceneSpritesHidden(s32 hiddenArgumentWord);

static void _dryfieldNightGasStationSetPlayerUpdateHold(u8 holdPlayerUpdate);

static s32 _dryfieldNightGasStationUseJerryCanMsg(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _dryfieldNightGasStationCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightGasStationCompanionActionMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _dryfieldNightGasStationArmFollowupSceneMsg(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _dryfieldNightGasStationStartJerryCanCutsceneTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

/// Message routes installed on the night gas station's registered room task.
///
/// Six synchronous handlers cover departures, key items, companion actions,
/// CAP commands, sound cues and actor events. The final end marker makes
/// unsupported messages return zero. The table and callbacks stay loaded
/// for the room task's lifetime; each handler interprets its two payload words.
static TaskMessageEntry _gDryfieldNightGasStationRoomMessages[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGasStationMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGasStationUseJerryCanMsg },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightGasStationCompanionActionMsg },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightGasStationCommandMsg },
    { ROOM_MESSAGE_SOUND, _gasStationCueSoundMsg },
    { ROOM_MESSAGE_ACTOR_EVENT, _dryfieldNightGasStationArmFollowupSceneMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Authored settings for the jerry-can scene and the companion's reentry idle request.
enum {
    DRYFIELD_NIGHT_GAS_STATION_IDLE_ANIMATION_ID           = 1,
    DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_CAP_SEQUENCE      = 19,
    DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_SKIP_CAP_SEQUENCE = 20,
    DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_SOUND             = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 12)
};

/// One bodyless jerry-can scene starter followed by the task-table terminator.
///
/// Key-item acceptance spawns this priority-32 task on the default list.
/// Spawning reads the descriptor synchronously; its callback must remain
/// loaded until the one-shot task starts the script and releases itself.
static TaskDesc _gDryfieldNightGasStationJerryCanTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldNightGasStationStartJerryCanCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Restarts the player's equipped-bank idle pose during the jerry-can scene.
///
/// The event interpreter copies the request and replaces source.index with
/// the current weapon bank. Playback enables grid collision and borrows the
/// loaded bank's animation data; the stored selector 1 is not used.
static AnimationPlayRequest _gDryfieldNightGasStationPlayerWeaponIdleAnimation = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_GAS_STATION_IDLE_ANIMATION_ID,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Restarts the waiting companion's idle pose on reentry after the follow-up scene.
///
/// Room initialization writes the live companion bank to source.index before
/// synchronous dispatch. The request remains writable; playback borrows the
/// bank's animation data and disables grid collision at the waiting placement.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionReentryIdleAnimation = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_GAS_STATION_IDLE_ANIMATION_ID,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Jerry-can scene that resumes display and player updates around CAP sequence 19.
///
/// Runs after the starter consumes the jerry can and blanks the display.
/// CAP cues time sound-bank entry 12 and the fade return, then ordinary player
/// control resumes. Commands and payloads are borrowed until execution ends.
static EvsCommand _gDryfieldNightGasStationJerryCanScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldNightGasStationSetPlayerUpdateHold }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path that cleans up the jerry-can scene and starts CAP sequence 20.
///
/// Fades through scene cleanup, returns to display, and ends scripted player
/// control before the replacement CAP sequence. The overlay owns the commands
/// until the event task reaches the end marker.
static EvsCommand _gDryfieldNightGasStationJerryCanSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_SKIP_CAP_SEQUENCE }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Outward room-space normals of the cutscene obstacle's four walls; ONE is unit length.
static SVECTOR _gDryfieldNightGasStationCompanionWaitingCollisionGridNormals[4] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_normals.inc"
};

/// Corners of the obstacle box at X 13620..13931, Y -1823..50, Z -312..301.
static SVECTOR _gDryfieldNightGasStationCompanionWaitingCollisionGridVerts[8] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_verts.inc"
};

/// Four vertical wall quads using blocking surface class zero, with no top or bottom.
static WorldCollisionGridFace _gDryfieldNightGasStationCompanionWaitingCollisionGridFaces[4] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_faces.inc"
};

/// One cell's four face IDs followed by WORLD_COLLISION_GRID_CELL_END and a retained unused zero.
static s16 _gDryfieldNightGasStationCompanionWaitingCollisionGridCells[6] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightGasStationCompanionWaitingCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..5 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_GAS_STATION_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightGasStationCompanionWaitingCollisionGridCells[(halfwordOffset)])
/// Directory of the obstacle template's single XZ cell.
static s16* _gDryfieldNightGasStationCompanionWaitingCollisionGridTable[1] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_table.inc"
};
#undef DRYFIELD_NIGHT_GAS_STATION_CELL_FACE_LIST_AT_OFFSET

/// Collision-source box enclosing the companion's stationary waiting placement.
///
/// Only four faces, four normal XYZs and eight vertex XYZs are copied. The
/// template is never activated; its one 4000-unit cell starts at (13620, -312).
/// Whether the box represents the companion or a nearby fixture is unproven.
/// All pools belong to this overlay and only the rooms 1..3 live grid is patched.
static WorldCollisionGrid _gDryfieldNightGasStationCompanionWaitingCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightGasStationCompanionWaitingCollisionGridNormals,
    .vertices    = _gDryfieldNightGasStationCompanionWaitingCollisionGridVerts,
    .faces       = _gDryfieldNightGasStationCompanionWaitingCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightGasStationCompanionWaitingCollisionGridTable,
    .xBias       = -13620,
    .zBias       = 312,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightGasStationCompanionWaitingCollisionGridFaces),
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation070B4Bank1[6] = {
#include "assets/dryfield_night_gas_station_animation_070B4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation070B4Bank4[46] = {
#include "assets/dryfield_night_gas_station_animation_070B4_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation070B4Records[109] = {
#include "assets/dryfield_night_gas_station_animation_070B4_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation070B4Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_070B4_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation070B4 = {
    _gDryfieldNightGasStationAnimation070B4Records,
    _gDryfieldNightGasStationAnimation070B4Indices,
    { NULL, _gDryfieldNightGasStationAnimation070B4Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation070B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation073A4Bank1[5] = {
#include "assets/dryfield_night_gas_station_animation_073A4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation073A4Bank4[48] = {
#include "assets/dryfield_night_gas_station_animation_073A4_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation073A4Records[105] = {
#include "assets/dryfield_night_gas_station_animation_073A4_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation073A4Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_073A4_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation073A4 = {
    _gDryfieldNightGasStationAnimation073A4Records,
    _gDryfieldNightGasStationAnimation073A4Indices,
    { NULL, _gDryfieldNightGasStationAnimation073A4Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation073A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation077A4Bank1[10] = {
#include "assets/dryfield_night_gas_station_animation_077A4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation077A4Bank4[85] = {
#include "assets/dryfield_night_gas_station_animation_077A4_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation077A4Records[121] = {
#include "assets/dryfield_night_gas_station_animation_077A4_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation077A4Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_077A4_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation077A4 = {
    _gDryfieldNightGasStationAnimation077A4Records,
    _gDryfieldNightGasStationAnimation077A4Indices,
    { NULL, _gDryfieldNightGasStationAnimation077A4Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation077A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation07A58Bank1[4] = {
#include "assets/dryfield_night_gas_station_animation_07A58_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation07A58Bank4[49] = {
#include "assets/dryfield_night_gas_station_animation_07A58_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation07A58Records[92] = {
#include "assets/dryfield_night_gas_station_animation_07A58_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation07A58Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_07A58_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation07A58 = {
    _gDryfieldNightGasStationAnimation07A58Records,
    _gDryfieldNightGasStationAnimation07A58Indices,
    { NULL, _gDryfieldNightGasStationAnimation07A58Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation07A58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation07CFCBank1[5] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation07CFCBank4[32] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation07CFCRecords[102] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation07CFCIndices[20] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation07CFC = {
    _gDryfieldNightGasStationAnimation07CFCRecords,
    _gDryfieldNightGasStationAnimation07CFCIndices,
    { NULL, _gDryfieldNightGasStationAnimation07CFCBank1, NULL, NULL, _gDryfieldNightGasStationAnimation07CFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation07F78Bank1[2] = {
#include "assets/dryfield_night_gas_station_animation_07F78_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation07F78Bank4[32] = {
#include "assets/dryfield_night_gas_station_animation_07F78_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation07F78Records[101] = {
#include "assets/dryfield_night_gas_station_animation_07F78_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation07F78Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_07F78_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation07F78 = {
    _gDryfieldNightGasStationAnimation07F78Records,
    _gDryfieldNightGasStationAnimation07F78Indices,
    { NULL, _gDryfieldNightGasStationAnimation07F78Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation07F78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation08148Bank1[2] = {
#include "assets/dryfield_night_gas_station_animation_08148_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation08148Bank4[30] = {
#include "assets/dryfield_night_gas_station_animation_08148_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation08148Records[60] = {
#include "assets/dryfield_night_gas_station_animation_08148_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation08148Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_08148_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation08148 = {
    _gDryfieldNightGasStationAnimation08148Records,
    _gDryfieldNightGasStationAnimation08148Indices,
    { NULL, _gDryfieldNightGasStationAnimation08148Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation08148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation08318Bank1[2] = {
#include "assets/dryfield_night_gas_station_animation_08318_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation08318Bank4[30] = {
#include "assets/dryfield_night_gas_station_animation_08318_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation08318Records[60] = {
#include "assets/dryfield_night_gas_station_animation_08318_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation08318Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_08318_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation08318 = {
    _gDryfieldNightGasStationAnimation08318Records,
    _gDryfieldNightGasStationAnimation08318Indices,
    { NULL, _gDryfieldNightGasStationAnimation08318Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation08318Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation0885CBank1[9] = {
#include "assets/dryfield_night_gas_station_animation_0885C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation0885CBank4[127] = {
#include "assets/dryfield_night_gas_station_animation_0885C_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation0885CRecords[163] = {
#include "assets/dryfield_night_gas_station_animation_0885C_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation0885CIndices[20] = {
#include "assets/dryfield_night_gas_station_animation_0885C_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation0885C = {
    _gDryfieldNightGasStationAnimation0885CRecords,
    _gDryfieldNightGasStationAnimation0885CIndices,
    { NULL, _gDryfieldNightGasStationAnimation0885CBank1, NULL, NULL, _gDryfieldNightGasStationAnimation0885CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation09344Bank1[24] = {
#include "assets/dryfield_night_gas_station_animation_09344_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation09344Bank4[263] = {
#include "assets/dryfield_night_gas_station_animation_09344_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation09344Records[343] = {
#include "assets/dryfield_night_gas_station_animation_09344_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation09344Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_09344_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation09344 = {
    _gDryfieldNightGasStationAnimation09344Records,
    _gDryfieldNightGasStationAnimation09344Indices,
    { NULL, _gDryfieldNightGasStationAnimation09344Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation09344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation09D80Bank1[23] = {
#include "assets/dryfield_night_gas_station_animation_09D80_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation09D80Bank4[243] = {
#include "assets/dryfield_night_gas_station_animation_09D80_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation09D80Records[323] = {
#include "assets/dryfield_night_gas_station_animation_09D80_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation09D80Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_09D80_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation09D80 = {
    _gDryfieldNightGasStationAnimation09D80Records,
    _gDryfieldNightGasStationAnimation09D80Indices,
    { NULL, _gDryfieldNightGasStationAnimation09D80Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation09D80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation0A3FCBank1[10] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation0A3FCBank4[142] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation0A3FCRecords[223] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation0A3FCIndices[20] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation0A3FC = {
    _gDryfieldNightGasStationAnimation0A3FCRecords,
    _gDryfieldNightGasStationAnimation0A3FCIndices,
    { NULL, _gDryfieldNightGasStationAnimation0A3FCBank1, NULL, NULL, _gDryfieldNightGasStationAnimation0A3FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation0A698Bank1[2] = {
#include "assets/dryfield_night_gas_station_animation_0A698_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation0A698Bank4[31] = {
#include "assets/dryfield_night_gas_station_animation_0A698_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation0A698Records[110] = {
#include "assets/dryfield_night_gas_station_animation_0A698_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation0A698Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_0A698_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation0A698 = {
    _gDryfieldNightGasStationAnimation0A698Records,
    _gDryfieldNightGasStationAnimation0A698Indices,
    { NULL, _gDryfieldNightGasStationAnimation0A698Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation0A698Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation0AA3CBank1[2] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation0AA3CBank4[72] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation0AA3CRecords[135] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation0AA3CIndices[20] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation0AA3C = {
    _gDryfieldNightGasStationAnimation0AA3CRecords,
    _gDryfieldNightGasStationAnimation0AA3CIndices,
    { NULL, _gDryfieldNightGasStationAnimation0AA3CBank1, NULL, NULL, _gDryfieldNightGasStationAnimation0AA3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGasStationAnimation0AF98Bank1[9] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGasStationAnimation0AF98Bank4[130] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_bank4.inc"
};

static AnimationRecord _gDryfieldNightGasStationAnimation0AF98Records[166] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_records.inc"
};

static u16 _gDryfieldNightGasStationAnimation0AF98Indices[20] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_indices.inc"
};

static AnimationSet _gDryfieldNightGasStationAnimation0AF98 = {
    _gDryfieldNightGasStationAnimation0AF98Records,
    _gDryfieldNightGasStationAnimation0AF98Indices,
    { NULL, _gDryfieldNightGasStationAnimation0AF98Bank1, NULL, NULL, _gDryfieldNightGasStationAnimation0AF98Bank4, NULL, NULL, NULL },
};

/// vx bit that hides the flicker sprites while remaining part of the path coordinate.
enum { DRYFIELD_NIGHT_GAS_STATION_TRACK_FLICKER_HIDDEN_BIT = 1 };

/// One hundred signed local-position samples for the cutscene's red streak and sprite flicker.
///
/// The streak plays each sample once per task update, rotating by -610/4096
/// turn around Y and adding its room-space offset. Independently, the flicker
/// task loops the same samples: vx bit zero set hides its sprites, clear shows
/// them. XYZ use game-coordinate units; the fourth halfword is retained zero.
static SVECTOR _gDryfieldNightGasStationRedStreakTrack[100] = {
#include "assets/dryfield_night_gas_station_motion_0AFC0.inc"
};

static void _dryfieldNightGasStationTrackRedStreakTask(Task* task);
static void _dryfieldNightGasStationSpriteFlickerTask(Task* task);
static void _dryfieldNightGasStationCompanionRedStreakTask(Task* task);

/// Indexed recipes for head aim toward the companion, track streak, sprite flicker and companion streak.
///
/// Slots 0..3 have priority `TASK_PRIORITY_CONTROL` and no terminator. Only the streaks allocate
/// coordinate bodies. Spawning borrows a recipe synchronously; callbacks and
/// the actor models or sprite resources they borrow must remain loaded.
static TaskDesc _gDryfieldNightGasStationCutsceneTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldNightGasStationCompanionHeadAimTask, { .value = 0 } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_CONTROL } }, _dryfieldNightGasStationTrackRedStreakTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldNightGasStationSpriteFlickerTask, { .value = 0 } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_CONTROL } }, _dryfieldNightGasStationCompanionRedStreakTask, { .value = 0 } },
};

/// Player weapon-bank slots 47..52: a null slot followed by five cutscene clips.
static AnimationSet* _gDryfieldNightGasStationPlayerCutsceneAnimationSets[6] = {
    NULL,
    &_gDryfieldNightGasStationAnimation070B4,
    &_gDryfieldNightGasStationAnimation073A4,
    &_gDryfieldNightGasStationAnimation077A4,
    &_gDryfieldNightGasStationAnimation07A58,
    &_gDryfieldNightGasStationAnimation07CFC,
};

/// Installs the player's six cutscene extension words in its current weapon bank.
///
/// Copies slots 47..52 synchronously; later bank slots are retained. Requests
/// borrow the source table during dispatch, and playback borrows its clip
/// descriptors and data for as long as the room overlay remains loaded.
static AnimationBankCopyRequest _gDryfieldNightGasStationPlayerCutsceneAnimationBankCopy = {
    .sets  = _gDryfieldNightGasStationPlayerCutsceneAnimationSets,
    .count = ARRAY_SIZE(_gDryfieldNightGasStationPlayerCutsceneAnimationSets),
};

/// Unused bank-1 clip-1 reset request retained before the player cutscene requests.
///
/// Requests enabled world collision; no script or message selects this record.
static AnimationPlayRequest _gDryfieldNightGasStationUnusedBank1Animation1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player cutscene request for animation 48 in the equipped weapon's bank.
///
/// Blends for 20 frames and enables world collision. Event execution copies
/// the request and overwrites its stored bank selector before synchronous
/// dispatch; the loaded weapon bank and its animation data must remain live.
static AnimationPlayRequest _gDryfieldNightGasStationPlayerWeaponAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player clip 49 for the follow-up conversation, blended over 20 frames with grid collision.
///
/// This selects the second playable entry of the installed player cutscene
/// extension. Event execution copies the request and resolves the equipped
/// bank; that bank and its borrowed animation data must stay loaded.
static AnimationPlayRequest _gDryfieldNightGasStationPlayerWeaponAnimation49 = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player clip 50 for the later conversation cue, blended over 20 frames with grid collision.
///
/// This selects the third playable entry of the installed player cutscene
/// extension. Event execution copies the request and resolves the equipped
/// bank; that bank and its borrowed animation data must stay loaded.
static AnimationPlayRequest _gDryfieldNightGasStationPlayerWeaponAnimation50 = {
    .source               = { .index = 1 },
    .animationId          = 50,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unused bank-1 clip-51 request retained among the player cutscene requests.
///
/// Requests a ten-frame blend and enabled world collision; no script selects it.
static AnimationPlayRequest _gDryfieldNightGasStationUnusedBank1Animation51Request = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Restarts player clip 52 at the encounter's battle placement with grid collision.
///
/// This selects the last playable entry of the installed player cutscene
/// extension. Event execution copies the request and resolves the equipped
/// bank; that bank and its borrowed animation data must stay loaded.
static AnimationPlayRequest _gDryfieldNightGasStationPlayerWeaponAnimation52 = {
    .source               = { .index = 1 },
    .animationId          = 52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Restarts player clip 48 without blending on the follow-up conversation's skip path.
///
/// Clip 48 is the first playable entry of the installed player cutscene
/// extension. Event execution copies the request and resolves the equipped
/// bank; playback enables grid collision and borrows the loaded animation data.
static AnimationPlayRequest _gDryfieldNightGasStationPlayerWeaponAnimation48Reset = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player placement for the introduction and its final restoration or skip.
///
/// Position is room-space XYZ in game units; rotation is Euler angles in
/// 1/4096 turns. Placement messages borrow the record synchronously.
static ActorTransform _gDryfieldNightGasStationPlayerIntroTransform = {
    .pos = { 12100, 0, -3180, 0 },
    .rot = { 0, 1444, 0, 0 },
};

/// Player placement at the start of the encounter scene's opening walk.
///
/// Position is room-space XYZ in game units; Euler angles use 4096 units
/// per turn. Placement copies both vectors during synchronous dispatch.
static ActorTransform _gDryfieldNightGasStationPlayerCutsceneWalkStartTransform = {
    .pos = { 1630, 0, -4332, 0 },
    .rot = { 0, 1024, 0, 0 },
};

/// Player destination for the encounter scene's opening walk along positive room X.
///
/// Position is room-space XYZ in game units. Move-to dispatch copies XYZ
/// and leaves the authored Euler angles, in 4096 units per turn, unread;
/// the movement state turns toward the destination itself.
static ActorTransform _gDryfieldNightGasStationPlayerCutsceneWalkTargetTransform = {
    .pos = { 2670, 0, -4332, 0 },
    .rot = { 0, 1024, 0, 0 },
};

/// Player's final cutscene placement before battle, shared by normal and skip paths.
///
/// Position is room-space XYZ in game units; rotation is Euler angles in
/// 1/4096 turns. Placement messages borrow the record synchronously.
static ActorTransform _gDryfieldNightGasStationPlayerBattleTransform = {
    .pos = { 7767, 0, -2200, 0 },
    .rot = { 0, 1376, 0, 0 },
};

/// Opening-walk clips: weapon-bank walk 2 followed by cutscene arrival clip 51.
///
/// Move-to dispatch consumes both IDs synchronously, narrowing them to
/// halfwords. Clip 51 is the fourth playable player cutscene extension entry;
/// the selected bank and animation data must remain loaded through playback.
static GameActorMoveAnim _gDryfieldNightGasStationPlayerCutsceneWalkAnimations = {
    .approachAnimId = 2,
    .arrivalAnimId  = 51,
};

/// Companion bank slots 47..57: a null slot followed by ten cutscene clips.
static AnimationSet* _gDryfieldNightGasStationCompanionCutsceneAnimationSets[11] = {
    NULL,
    &_gDryfieldNightGasStationAnimation07F78,
    &_gDryfieldNightGasStationAnimation08148,
    &_gDryfieldNightGasStationAnimation08318,
    &_gDryfieldNightGasStationAnimation0885C,
    &_gDryfieldNightGasStationAnimation09344,
    &_gDryfieldNightGasStationAnimation09D80,
    &_gDryfieldNightGasStationAnimation0A3FC,
    &_gDryfieldNightGasStationAnimation0A698,
    &_gDryfieldNightGasStationAnimation0AA3C,
    &_gDryfieldNightGasStationAnimation0AF98,
};

/// Installs eleven cutscene extension words in the selected companion's animation bank.
///
/// Copies slots 47..57 synchronously; later bank slots are retained. Requests
/// borrow the source table during dispatch, and playback borrows its clip
/// descriptors and data for as long as the room overlay remains loaded.
static AnimationBankCopyRequest _gDryfieldNightGasStationCompanionCutsceneAnimationBankCopy = {
    .sets  = _gDryfieldNightGasStationCompanionCutsceneAnimationSets,
    .count = ARRAY_SIZE(_gDryfieldNightGasStationCompanionCutsceneAnimationSets),
};

/// Restarts companion clip 48 without blending on the follow-up conversation's skip path.
///
/// This selects the first playable companion cutscene extension entry and
/// disables grid collision. Event execution copies the request and resolves
/// the companion bank; its borrowed animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation48Reset = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion cutscene clip 48, interpolated over 20 frames with grid collision disabled.
///
/// Event scripts copy this request and replace source.index with the current
/// companion's bank. Clip 48 is the first playable set in the room's installed
/// extension; the authored selector 1 is not used by these dispatches.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 49 at the first conversation cue, blended over 20 frames with grid collision.
///
/// Selects the second playable companion cutscene extension entry. Event
/// execution copies the request and resolves the live companion bank;
/// that bank and its borrowed animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation49 = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion clip 50 at the next conversation cue, blended over 20 frames with grid collision.
///
/// Selects the third playable companion cutscene extension entry. Event
/// execution copies the request and resolves the live companion bank;
/// that bank and its borrowed animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation50 = {
    .source               = { .index = 1 },
    .animationId          = 50,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion clip 51 paired with player clip 50, blended over 20 frames with grid collision.
///
/// Selects the fourth playable companion cutscene extension entry. Event
/// execution copies the request and resolves the live companion bank;
/// that bank and its borrowed animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation51 = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion clip 52 at the final conversation cue, blended over 20 frames with grid collision.
///
/// Selects the fifth playable companion cutscene extension entry. Event
/// execution copies the request and resolves the live companion bank;
/// that bank and its borrowed animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation52 = {
    .source               = { .index = 1 },
    .animationId          = 52,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unused bank-1 clip-53 request retained among the companion cutscene requests.
///
/// Requests a twenty-frame blend and enabled world collision; no script selects it.
static AnimationPlayRequest _gDryfieldNightGasStationUnusedBank1Animation53Request = {
    .source               = { .index = 1 },
    .animationId          = 53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion clip 54 for the room-action interaction, blended over eight frames without grid collision.
///
/// Selects the seventh playable companion cutscene extension entry. Event
/// execution copies the request and resolves the live companion bank;
/// that bank and its borrowed animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation54 = {
    .source               = { .index = 1 },
    .animationId          = 54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion clip 55 at conversation setup, blended over 20 frames with grid collision.
///
/// Selects the eighth playable companion cutscene extension entry before
/// placement at the conversation position. Event execution copies the request
/// and resolves the companion bank; its animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation55 = {
    .source               = { .index = 1 },
    .animationId          = 55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unused bank-1 clip-56 request retained among the companion cutscene requests.
///
/// Requests a twenty-frame blend and enabled world collision; no script selects it.
static AnimationPlayRequest _gDryfieldNightGasStationUnusedBank1Animation56Request = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Restarts companion clip 57 at the encounter's battle placement with grid collision.
///
/// Selects the last playable companion cutscene extension entry before the
/// attached red streak. Event execution copies the request and resolves
/// the companion bank; its animation data must remain loaded.
static AnimationPlayRequest _gDryfieldNightGasStationCompanionAnimation57 = {
    .source               = { .index = 1 },
    .animationId          = 57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion placement for the follow-up conversation before it returns to the waiting position.
///
/// Position is room-space XYZ in game units; Euler angles use 4096 units
/// per turn. Placement copies both vectors during synchronous dispatch.
static ActorTransform _gDryfieldNightGasStationCompanionConversationTransform = {
    .pos = { 12700, 0, -2450, 0 },
    .rot = { 0, 1480, 0, 0 },
};

/// Companion waiting placement after the introduction and on later room reentry.
///
/// Position is room-space XYZ in game units; rotation is Euler angles in
/// 1/4096 turns. Placement messages borrow the record synchronously.
static ActorTransform _gDryfieldNightGasStationCompanionWaitingTransform = {
    .pos = { 13770, 0, -298, 0 },
    .rot = { 0, 2292, 0, 0 },
};

/// Companion's final cutscene placement before battle, shared by normal and skip paths.
///
/// Position is room-space XYZ in game units; rotation is Euler angles in
/// 1/4096 turns. Placement messages borrow the record synchronously.
static ActorTransform _gDryfieldNightGasStationCompanionBattleTransform = {
    .pos = { 7335, 0, -3520, 0 },
    .rot = { 0, 1024, 0, 0 },
};

/// Scavenger command IDs used by this room's scene broadcasts.
///
/// The actor interprets cues 1..4 by placement variant; it ignores the context
/// tags. The final-wave command is defined beside its separate request below.
enum {
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_1         = 1,
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_2         = 2,
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_3         = 3,
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_4         = 4,
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_REFRESH_COLOR = 5,
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_HIDE          = 7,
    DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_DESTROY       = 8,
};

/// First encounter cue: start variant-specific Scavenger animations and hide variant 3.
///
/// Refreshes colour, seeds variant 2's clip-9 countdown with 0..19,
/// starts request 17 for variants 3/4, and relocates and hides variant 3.
/// The scene manager borrows this command during synchronous broadcast.
static ActorCommand _gDryfieldNightGasStationScavengerCue1Command = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_1,
};

/// Second encounter cue: change Scavenger poses and reveal placement variant 3.
///
/// Refreshes colour, starts request 26 for variant 1, seeds variant 2's
/// clip-9 countdown with 0..19, and shows variant 3 with automatic buffers.
/// The scene manager borrows this command during synchronous broadcast.
static ActorCommand _gDryfieldNightGasStationScavengerCue2Command = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_2,
};

/// Third encounter cue: reveal Scavenger variants 1..4 and restart their staged poses.
///
/// Refreshes colour, starts request 9 for variant 1, and seeds variant 2's
/// clip-9 countdown with 0..19. The scene manager borrows the command
/// during synchronous broadcast before the baked-track red streak begins.
static ActorCommand _gDryfieldNightGasStationScavengerCue3Command = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_3,
};

/// Fourth encounter cue: clear variant 1's animation and schedule clip 7 for variants 2/4.
///
/// Refreshes colour, restarts variant 2's animation request 3 and sets its
/// scripted countdown to 10; variant 4's countdown is set to 7. The scene
/// manager borrows this command during synchronous broadcast.
static ActorCommand _gDryfieldNightGasStationScavengerCue4Command = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_CUE_4,
};

/// Requests a Scavenger colour refresh after the encounter's red streak ends.
///
/// Leaves animation and visibility intact. The scene manager borrows this
/// command during synchronous broadcast to its actor children.
static ActorCommand _gDryfieldNightGasStationScavengerRefreshColorCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_REFRESH_COLOR,
};

enum { DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_BEGIN_FINAL_WAVE = 6 };

/// Broadcast command that releases the Scavengers' final wave after the cutscene or its skip.
///
/// Scavengers refresh colour, remove placement variant 3, show variants 2 and
/// 4 and select wave 3. Scene dispatch borrows this record synchronously.
static ActorCommand _gDryfieldNightGasStationScavengerFinalWaveCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_BEGIN_FINAL_WAVE,
};

/// Hides Scavenger placement variants 1..4 before the companion's red streak.
///
/// Requests colour refresh and disables active drawing and automatic buffers
/// for these variants. The scene manager borrows this command during broadcast;
/// the actors remain live until a later reveal, wave or destruction command.
static ActorCommand _gDryfieldNightGasStationScavengerHideCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_HIDE,
};

/// Releases Scavenger battle references and rewards, then destroys the actors after the follow-up.
///
/// Both the conversation and its skip broadcast this command synchronously.
/// Each receiver releases its enemy task and decrements the live Scavenger count.
static ActorCommand _gDryfieldNightGasStationScavengerDestroyCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION } },
    .command = DRYFIELD_NIGHT_GAS_STATION_SCAVENGER_COMMAND_DESTROY,
};

/// Stream selection key for the first-visit encounter scene, evs3_51_11.
///
/// Group 3 and stream ID 51 select a current-folder stream with qualifier 11;
/// the second qualifier is implicitly zero. The event task and CAP playback
/// borrow this overlay-owned key until the selected scene finishes or is cancelled.
static EvsSceneKey _gDryfieldNightGasStationEncounterSceneKey = {
    .group    = 3,
    .streamId = 51,
    .subId    = 11,
};

/// CAP command slots selected by the room's scripted presentations.
enum {
    DRYFIELD_NIGHT_GAS_STATION_FOLLOWUP_CAP_COMMAND              = 13,
    DRYFIELD_NIGHT_GAS_STATION_COMPANION_INTERACTION_CAP_COMMAND = 14,
    DRYFIELD_NIGHT_GAS_STATION_ENCOUNTER_CAP_COMMAND             = 22,
};

/// Restores the player and companion cutscene idle poses after the actor event.
///
/// Waits 60 callback ticks, installs both extension banks and starts clip 48.
/// HUD visibility is retained; both receivers and the borrowed clip data must be live.
static EvsCommand _gDryfieldNightGasStationRestoreActorAnimationsScript[6] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationPlayerCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationCompanionCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation48 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the deferred post-encounter conversation and clears its remaining actors.
///
/// Fades to view 5, runs CAP command 13 and cue-timed animation changes, then
/// places the companion at its waiting point, removes Scavengers, restores view 2
/// and clears the obstacle. Commands, both receivers and payload data stay live
/// until interpreter cleanup. The paired skip stream converges on the same room setup.
static EvsCommand _gDryfieldNightGasStationFollowupScript[44] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationPlayerCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationCompanionCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 10 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationPlayerIntroTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationCompanionConversationTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationCancelCutsceneEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_GAS_STATION_FOLLOWUP_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationCompanionWaitingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerDestroyCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationResetCutsceneObstacleCollision }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skips the follow-up conversation and restores its final room setup.
///
/// Restores actor placement and idle poses, clears ambient colour and collision,
/// cancels scene effects, removes Scavengers and marks view 2 dirty under a fade.
/// Borrowed room payloads and both actor receivers must remain live through cleanup.
static EvsCommand _gDryfieldNightGasStationFollowupSkipScript[21] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationPlayerIntroTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationCompanionWaitingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationPlayerCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationCompanionCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation48Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation48Reset }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationResetCutsceneObstacleCollision }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationCancelCutsceneEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerDestroyCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the companion interaction available after the follow-up scene starts.
///
/// Installs both extension banks, starts CAP command 14, advances the poses
/// on CAP cues and returns player control. Requires both live actor receivers;
/// the interpreter and animation playback borrow this overlay through completion.
static EvsCommand _gDryfieldNightGasStationCompanionInteractionScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationPlayerCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationCompanionCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_GAS_STATION_COMPANION_INTERACTION_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the first-visit Scavenger encounter before engaging battle.
///
/// Stages actor movement, CAP command 22 and streamed scene 51/11, coordinating
/// sprite flicker, head aim, flashes and two red-streak tasks on CAP cues. Ends
/// with the final actor wave and view 3. Both actors, callback code, command and
/// payload storage must stay live through playback. The companion and track
/// streaks are stopped after CAP waits; their lifetimes depend on those cue timings.
static EvsCommand _gDryfieldNightGasStationEncounterScript[81] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationPlayerCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightGasStationCompanionCutsceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 10 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationPlayerCutsceneWalkStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gDryfieldNightGasStationPlayerCutsceneWalkTargetTransform } }, { .message = { .moveAnimation = &_gDryfieldNightGasStationPlayerCutsceneWalkAnimations } } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationSpawnCompanionHeadAimTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationSpawnSpriteFlickerTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneSpritesHidden }, { .value = DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_SPRITES_HIDDEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gDryfieldNightGasStationEncounterSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationStageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_GAS_STATION_ENCOUNTER_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerCue1Command } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationEnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneSpritesHidden }, { .value = DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_SPRITES_SHOWN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationStopSpriteFlicker }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerCue2Command } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_THIRD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCutsceneFlash }, { .value = DRYFIELD_NIGHT_GAS_STATION_FLASH_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCompanionHeadAim }, { .value = DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_ON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationPlayerBattleTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationPlayerWeaponAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationCompanionBattleTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightGasStationCompanionAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerHideCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationSpawnCompanionRedStreakTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCompanionHeadAim }, { .value = DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationStopCompanionRedStreakTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerCue3Command } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationSpawnTrackRedStreakTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerCue4Command } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationStopRedStreakTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerRefreshColorCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationFinishScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCompanionHeadAim }, { .value = DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerFinalWaveCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skips the encounter presentation and commits its battle-ready state.
///
/// Cancels the stream, places both actors, starts the final wave, stops scene
/// effects and engages battle under the primary fade. Commands, callbacks and
/// borrowed actor payloads stay loaded until interpreter cleanup.
static EvsCommand _gDryfieldNightGasStationEncounterSkipScript[22] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationCancelScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationPlayerBattleTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightGasStationCompanionBattleTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightGasStationScavengerFinalWaveCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationSetCompanionHeadAim }, { .value = DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationStopRedStreakTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldNightGasStationStopSpriteFlicker }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldNightGasStationEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Authored positions for the room's capsule glows, flares and scattered effects.
///
/// XYZ use game-coordinate units. Indices 0..9 are five consecutive world-space
/// capsule endpoint pairs; 10..18 are nine world-space flare centres. The view
/// masks use those same indices. Indices 19..20 are independently jittered X/Z
/// puff, fire and smoke origins in the effect task's frame, without view masks.
/// All fourth halfwords retain their authored zero.
static SVECTOR _gDryfieldNightGasStationAmbientEffectAnchors[21] = {
    { 13040, -3360, 310, 0 },
    { 11600, -3360, 310, 0 },
    { 13040, -3500, 130, 0 },
    { 11600, -3500, 130, 0 },
    { 3370, -2310, -170, 0 },
    { 2660, -2310, -170, 0 },
    { -300, -3250, -4100, 0 },
    { -300, -3190, -4610, 0 },
    { -6010, -2870, -2940, 0 },
    { -6010, -2870, -4150, 0 },
    { 9000, -4360, -400, 0 },
    { 6300, -4360, -400, 0 },
    { 10320, -4110, -5150, 0 },
    { -6930, -4850, -3150, 0 },
    { -6910, -1700, -3150, 0 },
    { 17890, -1800, 6000, 0 },
    { 19180, -1800, 6000, 0 },
    { 28870, -1800, 6000, 0 },
    { 30150, -1800, 6000, 0 },
    { 16540, -250, -2000, 0 },
    { 15810, -250, -3200, 0 },
};

/// Two bright capsule-glow endpoint pairs in world game-coordinate units.
///
/// Both pairs draw in mapped views 2, 3, 13 and 14. The drawer borrows each
/// complete pair during projection; the points remain owned by this overlay.
static const SVECTOR _gDryfieldNightGasStationBrightCapsuleEndpoints[2][2] SECTION(".data") = {
    { { 13330, -3360, 320, 0 }, { 14630, -3360, 320, 0 } },
    { { 13330, -3500, 140, 0 }, { 14630, -3500, 140, 0 } },
};

/// Mapped-view bitsets selecting the 19 ordinary glow anchors.
///
/// Bit n selects mapped view n (0..31). Entries 0..9 parallel five capsule
/// endpoint pairs; only each even entry is tested. Entries 10..18 select
/// individual flares. Scatter anchors 19/20 and the bright pairs use no entry.
static const u32 _gDryfieldNightGasStationGlowViewMasks[19] SECTION(".data") = {
    (1u << 2) | (1u << 3) | (1u << 13) | (1u << 14),
    (1u << 2) | (1u << 3) | (1u << 13) | (1u << 14),
    (1u << 2) | (1u << 3) | (1u << 13) | (1u << 14),
    (1u << 2) | (1u << 3) | (1u << 13) | (1u << 14),
    (1u << 4) | (1u << 6) | (1u << 11) | (1u << 16) | (1u << 20),
    (1u << 4) | (1u << 6) | (1u << 11) | (1u << 16) | (1u << 20),
    (1u << 4) | (1u << 11) | (1u << 20),
    (1u << 4) | (1u << 11) | (1u << 20),
    (1u << 4) | (1u << 11) | (1u << 20),
    (1u << 4) | (1u << 11) | (1u << 20),
    (1u << 3) | (1u << 14),
    (1u << 3) | (1u << 14),
    (1u << 3) | (1u << 14),
    (1u << 4) | (1u << 11) | (1u << 20),
    (1u << 4) | (1u << 11) | (1u << 20),
    (1u << 2) | (1u << 13),
    (1u << 2) | (1u << 13),
    (1u << 2) | (1u << 13),
    (1u << 2) | (1u << 13),
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting gDryfieldNightGasStationRoomLighting[4] = {
    { &_gDryfieldNightGasStationRoom1Lights, _gDryfieldNightGasStationAmbientByView },
    { &_gDryfieldNightGasStationRooms2To4Lights, _gDryfieldNightGasStationAmbientByView },
    { &_gDryfieldNightGasStationRooms2To4Lights, _gDryfieldNightGasStationAmbientByView },
    { &_gDryfieldNightGasStationRooms2To4Lights, _gDryfieldNightGasStationAmbientByView },
};

WorldCollisionRoomResources gDryfieldNightGasStationRoomCollisionResources[4] = {
    { &_gDryfieldNightGasStationRooms1To3CollisionGrid, _gDryfieldNightGasStationViewBoundaryTriggers, _gDryfieldNightGasStationActionTriggers, NULL },
    { &_gDryfieldNightGasStationRooms1To3CollisionGrid, _gDryfieldNightGasStationViewBoundaryTriggers, _gDryfieldNightGasStationActionTriggers, NULL },
    { &_gDryfieldNightGasStationRooms1To3CollisionGrid, _gDryfieldNightGasStationViewBoundaryTriggers, _gDryfieldNightGasStationActionTriggers, NULL },
    { &_gDryfieldNightGasStationRoom4CollisionGrid, _gDryfieldNightGasStationViewBoundaryTriggers, _gDryfieldNightGasStationActionTriggers, NULL },
};

/// Room 1's logical views 1..21 mapped to camera, image and sprite indices.
///
/// Several logical views share an index. The three trailing zero bytes are
/// retained storage outside the 21 live view slots; this map has no terminator.
static u8 _gDryfieldNightGasStationRoom1ViewMap[24] = {
    1,
    13,
    14,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    19,
    19,
    18,
    15,
    16,
    17,
    10,
    19,
    12,
    21,
    0,
    0,
    0,
};

/// Room 2's logical views 1..21 mapped to camera, image and sprite indices.
///
/// Views 12 and 13 share index 12; views 10 and 14 share index 10. The three
/// trailing zero bytes are retained storage outside the live view slots.
static u8 _gDryfieldNightGasStationRoom2ViewMap[24] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    12,
    10,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

/// Room 3's logical-view map, exchanging room 2's indices for views 4 and 11.
///
/// Logical views 1..21 select 1-based camera, image and sprite indices. The
/// three trailing zero bytes are retained storage outside the live view slots.
static u8 _gDryfieldNightGasStationRoom3ViewMap[24] = {
    1,
    2,
    3,
    11,
    5,
    6,
    7,
    8,
    9,
    10,
    4,
    12,
    12,
    10,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

/// Room 4's logical-view map, selecting indices 20 and 4 for views 4 and 11.
///
/// Logical views 1..21 select 1-based camera, image and sprite indices. The
/// three trailing zero bytes are retained storage outside the live view slots.
static u8 _gDryfieldNightGasStationRoom4ViewMap[24] = {
    1,
    2,
    3,
    20,
    5,
    6,
    7,
    8,
    9,
    10,
    4,
    12,
    12,
    10,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

u8* gDryfieldNightGasStationViewMaps[4] = {
    _gDryfieldNightGasStationRoom1ViewMap,
    _gDryfieldNightGasStationRoom2ViewMap,
    _gDryfieldNightGasStationRoom3ViewMap,
    _gDryfieldNightGasStationRoom4ViewMap,
};

enum { DRYFIELD_NIGHT_GAS_STATION_LOGICAL_VIEW_COUNT = 21 };

ViewCount gDryfieldNightGasStationViewCounts[4] = {
    DRYFIELD_NIGHT_GAS_STATION_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_GAS_STATION_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_GAS_STATION_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_GAS_STATION_LOGICAL_VIEW_COUNT,
};

enum {
    DRYFIELD_NIGHT_GAS_STATION_MAIN_STREET_WARP_ID    = 2,
    DRYFIELD_NIGHT_GAS_STATION_GENERAL_STORE_WARP_ID  = 3,
    DRYFIELD_NIGHT_GAS_STATION_MAIN_STREET_MAP_FLAG   = 488,
    DRYFIELD_NIGHT_GAS_STATION_GENERAL_STORE_MAP_FLAG = 489,
};

DirectionWarpEntry gDryfieldNightGasStationWarpEntries[3] = {
    { { { .word = 2816 }, 0x3848, 0, -2630 }, { 0, 0, 0, 0 }, { { .word = 2816 }, 0x3848, 0, -1440 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 294, -5, -3482 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 294, -5, -4466 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 2), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 1), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_GAS_STATION_MAIN_STREET_MAP_FLAG },
    { { { .word = 2048 }, 3039, 0, -433 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 4316, 0, -535 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 4), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 3), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 15), 6, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_GAS_STATION_GENERAL_STORE_MAP_FLAG },
};

/// Room-space unit normals for the rooms 1..3 grid, with four cutscene-obstacle slots.
static SVECTOR _gDryfieldNightGasStationRooms1To3CollisionGridNormals[34] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_normals.inc"
};

/// Room-space vertices for the rooms 1..3 grid; the first eight are restored for the cutscene obstacle.
static SVECTOR _gDryfieldNightGasStationRooms1To3CollisionGridVerts[144] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_verts.inc"
};

/// Indexed room surfaces for rooms 1..3, with four initially inactive obstacle faces.
static WorldCollisionGridFace _gDryfieldNightGasStationRooms1To3CollisionGridFaces[65] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_faces.inc"
};

/// Packed signed face-index lists for the rooms 1..3 grid, each ending with -1.
///
/// Cell-table pointers select list starts; every list includes obstacle faces
/// 0..3 so restoring that geometry does not require rebuilding the index.
static s16 _gDryfieldNightGasStationRooms1To3CollisionGridCells[456] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightGasStationRooms1To3CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..455 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_GAS_STATION_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightGasStationRooms1To3CollisionGridCells[(halfwordOffset)])
/// X-major cell-list directory for six X cells by four Z cells.
static s16* _gDryfieldNightGasStationRooms1To3CollisionGridTable[24] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_table.inc"
};
#undef DRYFIELD_NIGHT_GAS_STATION_CELL_FACE_LIST_AT_OFFSET

/// Live room collision mesh shared by night gas station room variants 1..3.
///
/// The grid has 65 faces and six by four XZ cells, each 4000 game units wide,
/// with origin (-5043, -6369). Activation binds its view coordinate. The room
/// overlay owns all pools; only the four-face, four-normal, eight-vertex obstacle
/// prefix is restored or displaced during cutscenes. The remaining geometry and
/// cell lists stay intact; room variant 4 selects a separate mesh.
static WorldCollisionGrid _gDryfieldNightGasStationRooms1To3CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightGasStationRooms1To3CollisionGridNormals,
    .vertices    = _gDryfieldNightGasStationRooms1To3CollisionGridVerts,
    .faces       = _gDryfieldNightGasStationRooms1To3CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightGasStationRooms1To3CollisionGridTable,
    .xBias       = 5043,
    .zBias       = 6369,
    .cellCountX  = 6,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightGasStationRooms1To3CollisionGridFaces),
};

/// Room-space face normals for room variant 4; ONE is unit length.
static SVECTOR _gDryfieldNightGasStationRoom4CollisionGridNormals[32] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_normals.inc"
};

/// Room variant 4's collision vertices in signed game-coordinate units.
static SVECTOR _gDryfieldNightGasStationRoom4CollisionGridVerts[135] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_verts.inc"
};

/// Indexed collision triangles and quads for room variant 4.
static WorldCollisionGridFace _gDryfieldNightGasStationRoom4CollisionGridFaces[64] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_faces.inc"
};

/// Packed room-4 cell face-ID lists, each terminated by WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldNightGasStationRoom4CollisionGridCells[370] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightGasStationRoom4CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..369 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_GAS_STATION_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightGasStationRoom4CollisionGridCells[(halfwordOffset)])
/// X-major directory for room variant 4's six X cells by four Z cells.
static s16* _gDryfieldNightGasStationRoom4CollisionGridTable[24] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_table.inc"
};
#undef DRYFIELD_NIGHT_GAS_STATION_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh used only by night gas station room variant 4.
///
/// Contains 64 faces and six by four XZ cells, each 4000 game units wide,
/// starting at (-5043, -6369). Activation binds the view coordinate; the
/// room overlay owns the descriptor and every borrowed geometry/index pool.
static WorldCollisionGrid _gDryfieldNightGasStationRoom4CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightGasStationRoom4CollisionGridNormals,
    .vertices    = _gDryfieldNightGasStationRoom4CollisionGridVerts,
    .faces       = _gDryfieldNightGasStationRoom4CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightGasStationRoom4CollisionGridTable,
    .xBias       = 5043,
    .zBias       = 6369,
    .cellCountX  = 6,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightGasStationRoom4CollisionGridFaces),
};

ViewCamera gDryfieldNightGasStationViewCameras[21] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x2710, 0x7530, 1000 } }, 380 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { -1914, 0, -3621 }, { -313, 4080, 165 }, { 3607, 354, -1907 } }, { -0x2C6B, 987, 1522 } }, 230 },
    { { { { 3396, 0, 2288 }, { -180, 4083, 267 }, { -2281, -322, 3386 } }, { -5346, 792, 3101 } }, 230 },
    { { { { 1185, 0, -3920 }, { -2756, 2912, -833 }, { 2787, 2879, 843 } }, { -0x30B0, 2176, 5231 } }, 312 },
    { { { { 3985, 0, 945 }, { 289, 3899, -1220 }, { -900, 1254, 3793 } }, { -4447, 1449, 747 } }, 230 },
    { { { { 1718, 0, 3718 }, { 463, 4064, -214 }, { -3688, 510, 1705 } }, { -0x4844, 1820, 5790 } }, 789 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { -752, 0, -4026 }, { 331, 4082, -61 }, { 4012, -337, -750 } }, { -0x2ED6, 720, 1750 } }, 329 },
    { { { { 3629, 0, 1898 }, { -87, 4091, 166 }, { -1896, -188, 3625 } }, { -9070, 1190, 6110 } }, 541 },
    { { { { -915, 0, -3992 }, { 584, 4051, -134 }, { 3949, -600, -905 } }, { -0x2E7C, 820, 2240 } }, 380 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 1015, 0, 3968 }, { 1689, 3706, -432 }, { -3590, 1743, 919 } }, { -6015, 2185, 5635 } }, 312 },
};

/// Authored source RGB and command flags before cached raw-texture packets are built.
enum {
    DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR = 128,
    DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE        = 0,
};

/// Mapped view 1's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's 70 textured rectangles and sorting depths, selected by its batches.
///
/// Positions and sizes use pixels relative to the view drawing origin. All
/// entries start with neutral RGB and opaque modulation; cached packets force
/// raw texture. The room overlay owns the sources while sprites are in use.
static SpriteSource _gDryfieldNightGasStationView2SpriteSources[70] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -48, 2385, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -48, 2386, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -48, 2382, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2350, { .fields = { 80, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2291, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -40, 2293, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -40, 2303, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -40, 2322, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -40, 1992, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -40, 1949, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -40, 1880, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -40, 1832, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2275, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2208, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -32, 2200, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -32, 2164, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -32, 2153, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -32, 1741, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -32, 1717, { .fields = { 80, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -32, 1718, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -32, 1875, { .fields = { 80, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -32, 1828, { .fields = { 80, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -32, 1747, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2235, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -24, 1917, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -24, 2082, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -24, 1986, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -24, 1891, { .fields = { 88, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -24, 1700, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -24, 1695, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -24, 1584, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 1563, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -16, 1615, { .fields = { 88, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -16, 1609, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -16, 1578, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 1529, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 1485, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -16, 1523, { .fields = { 88, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -16, 1503, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -8, 1528, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -8, 1537, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -8, 1478, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -8, 1484, { .fields = { 88, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -24, 2415, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -24, 2381, { .fields = { 104, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 2141, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -24, 2172, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -24, 2059, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -24, 1710, { .fields = { 120, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -24, 1645, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -8, 1582, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1538, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 0, 1517, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 0, 1519, { .fields = { 96, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 1492, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -8, 1505, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -8, 1550, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 1583, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -16, 1613, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 8, 1061, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 40, 1063, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -56, 2975, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 2950, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -48, 2934, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -48, 2933, { .fields = { 104, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 2823, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 2743, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 16, -48, 2646, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, -48, 2528, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, -40, 2645, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 2's two sprite ranges, covering all 70 sources after the image-strip selector.
///
/// The terminal record ends traversal. Both ranges start visible and included
/// in cached packet processing; the final two bytes retain unproven metadata.
static SpriteBatch _gDryfieldNightGasStationView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 61, 0, 0, { 1, 0 } },
    { 61, ARRAY_SIZE(_gDryfieldNightGasStationView2SpriteSources) - 61, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 3's 22 textured rectangles and sorting depths, selected by its batches.
///
/// Positions and sizes use pixels relative to the view drawing origin. Sources
/// start with neutral RGB and opaque modulation and live with the room overlay.
static SpriteSource _gDryfieldNightGasStationView3SpriteSources[22] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 8, 2387, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 0, 2334, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 0, 2362, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 0, 3735, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3749, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 0, 3581, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 3394, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 3258, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 3126, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 3124, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 3080, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 0, 3133, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 0, 2949, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, 0, 4750, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 0, 4250, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3937, { .fields = { 112, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 0, 4125, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 2109, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 8, 1954, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 8, 1899, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 16, 1933, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 1952, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 3's three sprite ranges, covering all 22 sources after the image-strip selector.
///
/// The terminal record ends traversal. Ranges start visible and included in
/// cached packet processing; the final two bytes retain unproven metadata.
static SpriteBatch _gDryfieldNightGasStationView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 4, 0, 0, { 2, 0 } },
    { 17, ARRAY_SIZE(_gDryfieldNightGasStationView3SpriteSources) - 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 4's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5's 86 textured rectangles and sorting depths, selected by its batches.
///
/// Positions and sizes use pixels relative to the view drawing origin. All
/// entries start with neutral RGB; sources 63..76 are semitransparent and the
/// rest are opaque. Storage lives with the loaded room overlay.
static SpriteSource _gDryfieldNightGasStationView5SpriteSources[86] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, 16, 998, { .fields = { 16, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 16, 1075, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 8, 1012, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 0, 979, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -8, 941, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -8, 924, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -8, 912, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -16, 1000, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -16, 937, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -24, 1062, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, -24, 1125, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -40, -32, 1250, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -24, -40, 1212, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 8, -40, 750, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 40, -40, 750, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -24, 759, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 80, -32, 734, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 120, -32, 708, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -32, 1031, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -32, 1082, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -32, 1094, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -32, 1126, { .fields = { 8, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -32, 1156, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -32, 1191, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -32, 1241, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 1274, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -24, 1000, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -24, 995, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -24, 1018, { .fields = { 8, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -24, 1042, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -24, 1067, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 1098, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -24, 1132, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -24, 1175, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -24, 1226, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -24, 1291, { .fields = { 8, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -24, 1264, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -16, 979, { .fields = { 0, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -16, 979, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -16, 1000, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -16, 1023, { .fields = { 0, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -16, 1047, { .fields = { 0, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 1078, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -16, 1112, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -16, 1154, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -16, 1250, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -16, 1250, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -8, 962, { .fields = { 0, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -8, 965, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -8, 986, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -8, 1010, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -8, 1032, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 0, 1115, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 930, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 0, 930, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 0, 888, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -8, 857, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -16, 826, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -24, 800, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, -24, 775, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -40, -32, 750, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -32, -32, 775, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -32, 800, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 8, 16 } }, -72, -8, 917, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, -64, -16, 847, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -56, -24, 875, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -48, -24, 846, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, -40, -32, 825, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, -32, -32, 797, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, -24, -32, 803, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 8 } }, -80, -8, 975, { .fields = { 32, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 8 } }, -72, -8, 983, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 16 } }, -64, -16, 927, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 16 } }, -56, -24, 912, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 24 } }, -48, -32, 1225, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 32 } }, -16, -40, 1233, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 48, 24 } }, 0, -40, 1225, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, -24, 1750, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 56, 8 } }, -160, -16, 1750, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 72, 8 } }, -160, -8, 1750, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 80, 8 } }, -160, 0, 1750, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 48, 8 } }, -160, 8, 1750, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 24, 8 } }, -160, 16, 1750, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 56, 8 } }, -160, 24, 1750, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 32, 1750, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 40, 1750, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 5's six sprite ranges, covering all 86 sources after the image-strip selector.
///
/// The terminal record ends traversal. Ranges start visible and included in
/// cached packet processing. The cutscene toggles batch 6's final nine sprites;
/// the final two bytes of each record retain unproven metadata.
static SpriteBatch _gDryfieldNightGasStationView5SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 37, 0, 0, { 0, 0 } },
    { 55, 8, 0, 0, { 5, 0 } },
    { 63, 7, 0, 0, { 1, 0 } },
    { 70, 7, 0, 0, { 4, 0 } },
    { 77, ARRAY_SIZE(_gDryfieldNightGasStationView5SpriteSources) - 77, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 7's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 8's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 9's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 10's empty sprite list, selecting decoded background image strips.
///
/// The source slot retains this same list address; its records select no sprites.
static SpriteBatch _gDryfieldNightGasStationView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 11's 47 textured rectangles and sorting depths, including the distant balcony.
///
/// Position and size use pixels relative to the drawing origin; depth is an
/// unscaled ordering value. Sources start with neutral RGB and opaque modulation;
/// cached packets force raw texture. The room overlay owns their storage.
static SpriteSource _gDryfieldNightGasStationView11SpriteSources[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 2553, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 2276, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 2617, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -96, 0x61A7, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 2422, { .fields = { 16, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -104, 2423, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -80, 2672, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -48, 2664, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 32, 2662, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 48, 2603, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -104, 2681, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -24, 2535, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, -24, 2657, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -120, 2750, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -120, 2540, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, -120, 2626, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -40, -120, 2785, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 8, -120, 2981, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 16, 2526, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -160, -32, 1084, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -152, -24, 1200, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -144, -24, 1313, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -16, 1577, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -128, -8, 1831, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 0, 2075, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 24, 2440, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -80, 24, 2698, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 80, 40 } }, -16, 24, 2725, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, -16, 1550, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -56, 2601, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -48, 2598, { .fields = { 112, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 2550, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, -32, 2505, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, -24, 2648, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, -16, 2666, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, -8, 2675, { .fields = { 0, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 0, 2673, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 8, 2646, { .fields = { 120, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 16, 2558, { .fields = { 120, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -16, 24, 2749, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, 32, 2718, { .fields = { 104, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 40, 2740, { .fields = { 104, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 48, 2676, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, 56, 2655, { .fields = { 120, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -16, 4287, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 16, 24 } }, -40, -16, 0xA881, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4040, { .fields = { 64, 16 } }, -88, -8, 4112, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 11's source ranges, ending with the saved-damage balcony sprite and a terminator.
///
/// The leading zero count selects background image strips. Batch 6 is writable
/// visibility for the balcony; hiding it retains its cached packet position.
static SpriteBatch _gDryfieldNightGasStationView11SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 11, 0, 0, { 0, 0 } },
    { 29, 15, 0, 0, { 5, 0 } },
    { 44, 1, 0, 0, { 1, 0 } },
    { 45, 1, 0, 0, { 4, 0 } },
    { 46, ARRAY_SIZE(_gDryfieldNightGasStationView11SpriteSources) - 46, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 12's empty sprite list, selecting decoded background image strips.
///
/// Its source slot retains this same list address; the zero range is not a terminator.
static SpriteBatch _gDryfieldNightGasStationView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 13's 90 textured rectangles and sorting depths.
///
/// Position and size use pixels relative to the drawing origin; depth is an
/// unscaled ordering value. RGB starts neutral; sources 84..89 request
/// semitransparency. The room overlay owns the sources while packets use them.
static SpriteSource _gDryfieldNightGasStationView13SpriteSources[90] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -56, 3250, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 0x61A7, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -48, 3184, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -48, 3183, { .fields = { 80, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 3000, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 2875, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -48, 2750, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -48, 2750, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, -48, 2750, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 8, 1111, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 40, 1163, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -40, 1925, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -40, 1834, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -32, 1784, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -32, 1988, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -32, 1978, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -24, 1722, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -24, 1965, { .fields = { 56, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -24, 1956, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -24, 1720, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 1828, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -16, 1725, { .fields = { 120, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -8, 1677, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -8, 1657, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 1625, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 1610, { .fields = { 120, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 1576, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -8, 1566, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -8, 1525, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -8, 1525, { .fields = { 104, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 1558, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -16, 1588, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -16, 1615, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -16, 1555, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -16, 1523, { .fields = { 56, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -48, 2357, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -48, 2380, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -48, 2384, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2296, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2291, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -40, 2293, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -40, 2303, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -40, 2320, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2286, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2208, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -32, 2200, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -32, 2263, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2235, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -40, 1836, { .fields = { 48, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, -40, 1992, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -32, 1751, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, -32, 2191, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -32, 1759, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -24, 2016, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, -24, 2055, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -24, 1747, { .fields = { 64, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -16, 2015, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -16, 1809, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -16, 1736, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -24, 2415, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -24, 2381, { .fields = { 96, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -24, 2141, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 8, 32 } }, -64, -56, 3125, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 16 } }, -56, -48, 3000, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 16, 24 } }, -32, -48, 2809, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 8, 16 } }, -16, -48, 2808, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 16, 24 } }, -8, -48, 2875, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 8, 32 } }, 8, -48, 2750, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 16, 24 } }, 56, -48, 2625, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 16, 24 } }, 16, -48, 2625, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 24 } }, 32, -48, 2625, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -48, 2770, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -40, 2750, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -32, -40, 2925, { .fields = { 64, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -32, 2531, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -24, 2526, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -16, 2497, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -32, -32, 2964, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -32, -24, 2816, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 32, 8 } }, -32, -16, 2506, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -32, 3108, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -24, 2801, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -48, -8, 2471, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 24, 8 } }, -24, -8, 2329, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 16, 8 } }, 48, -32, 1907, { .fields = { 72, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 8 } }, 64, -32, 1950, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 8 } }, 40, -24, 1909, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 8 } }, 56, -24, 1784, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 8 } }, 72, -24, 1848, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 24, 8 } }, 48, -16, 2027, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Mapped view 13's nine source ranges followed by SPRITE_BATCH_END.
///
/// The first zero count selects background image strips. Cutscenes change
/// visibility of batches 6 and 7, covering sources 62..83 without discarding packets.
static SpriteBatch _gDryfieldNightGasStationView13SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 6, 0 } },
    { 9, 26, 0, 0, { 1, 0 } },
    { 35, 13, 0, 0, { 4, 0 } },
    { 48, 11, 0, 0, { 0, 0 } },
    { 59, 3, 0, 0, { 5, 0 } },
    { 62, 9, 0, 0, { 3, 0 } },
    { 71, 13, 0, 0, { 7, 0 } },
    { 84, ARRAY_SIZE(_gDryfieldNightGasStationView13SpriteSources) - 84, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 14's 54 textured rectangles and sorting depths.
///
/// Position and size use pixels relative to the drawing origin; depth is an
/// unscaled ordering value. RGB starts neutral; sources 30..34 request
/// semitransparency. The room overlay owns the sources while packets use them.
static SpriteSource _gDryfieldNightGasStationView14SpriteSources[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 8, 2387, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 0, 2334, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 0, 2362, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 0, 3735, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3749, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 0, 3581, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 3394, { .fields = { 96, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 3113, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 3126, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 3124, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 3080, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 0, 3133, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 0, 2949, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, 0, 4250, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 0, 4250, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3937, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 0, 4125, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 2109, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 8, 1954, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 8, 1899, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 16, 1933, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 1952, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 3500, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 3394, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 3133, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 3101, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 0, 2925, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 2955, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 0, 3008, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 0, 2949, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 8, 8 } }, 64, -8, 3202, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, -8, 3225, { .fields = { 88, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 56, 0, 3083, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 64, 0, 3070, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, 0, 3133, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 16 } }, -8, 0, 4305, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 8, 24 } }, 0, 0, 4378, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 24, 8 } }, 8, 8, 4500, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 24, 8 } }, 8, 16, 3566, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 32, 16 } }, -24, 0, 4225, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 24, 16 } }, 8, 0, 4225, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 8, 24 } }, 32, 0, 4175, { .fields = { 104, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4040, { .fields = { 16, 16 } }, 40, 0, 3850, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 24 } }, 24, 0, 3735, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 24 } }, 32, 0, 3749, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 24 } }, 40, 0, 3581, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 24 } }, 48, 0, 3369, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 32 } }, 56, -8, 3100, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 16 } }, 64, 8, 2879, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 32 } }, -16, -16, 3427, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 32 } }, -8, -16, 3470, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 96, 8 } }, -32, 16, 3684, { .fields = { 0, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 80, 8 } }, -24, 24, 2781, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 48, 8 } }, 8, 32, 2449, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 14's ten source ranges followed by SPRITE_BATCH_END.
///
/// The first zero count selects background image strips. Cutscenes hide batches
/// 6 and 7; timed flashes and flicker change batches 8 and 9. Hidden ranges retain packets.
static SpriteBatch _gDryfieldNightGasStationView14SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 4, 0, 0, { 5, 0 } },
    { 17, 5, 0, 0, { 4, 0 } },
    { 22, 8, 0, 0, { 6, 0 } },
    { 30, 5, 0, 0, { 1, 0 } },
    { 35, 4, 0, 0, { 7, 0 } },
    { 39, 4, 0, 0, { 3, 0 } },
    { 43, 6, 0, 0, { 8, 0 } },
    { 49, ARRAY_SIZE(_gDryfieldNightGasStationView14SpriteSources) - 49, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 15's 143 textured rectangles and sorting depths.
///
/// Position and size use pixels relative to the drawing origin; depth is an
/// unscaled ordering value. RGB starts neutral; sources 84..94 request
/// semitransparency. The room overlay owns the sources while packets use them.
static SpriteSource _gDryfieldNightGasStationView15SpriteSources[143] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, 56, 951, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 48, 945, { .fields = { 112, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -48, 24, 797, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 24, 801, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -8, 8, 850, { .fields = { 80, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, 16, 925, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 16, 700, { .fields = { 16, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 16, 975, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, 16, 1000, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 1025, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 48, 8, 1050, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 0, 1087, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -24, 1076, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 80, -24, 850, { .fields = { 120, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -32, 675, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 80, 8, 1007, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 0, 675, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 104, -8, 675, { .fields = { 120, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -32, 675, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -88, 40, 950, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 32, 954, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 56, 750, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 56, 48, 749, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 40, 752, { .fields = { 8, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 24, 750, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 88, 0, 675, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 104, -8, 675, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, -16, 688, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -16, 691, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -8, 675, { .fields = { 24, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1000, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 40, 801, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 650, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 96, 72, 633, { .fields = { 88, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, 40, 635, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 112, 24, 610, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, 16, 598, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, 8, 580, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -32, 628, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -24, 628, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 1036, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -8, 1075, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, -8, 1109, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1137, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, -8, 1192, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 0, 992, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 0, 968, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 0, 993, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 0, 1028, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 0, 1073, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 0, 1138, { .fields = { 40, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 0, 1194, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 8, 950, { .fields = { 40, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 8, 936, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 8, 959, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 8, 992, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 8, 1027, { .fields = { 48, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 8, 1094, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 8, 1000, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 16, 923, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 913, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 16, 930, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 16, 963, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 16, 1000, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 24, 864, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 24, 884, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 24, 908, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 24, 919, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 32, 843, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 32, 815, { .fields = { 48, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 633, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 32, 765, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -8, 24, 726, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 0, 0, 687, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 16, -8, 748, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 737, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -16, 707, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, -16, 679, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -16, 633, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 632, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 16, 32, 647, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 48, 24, 611, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 56, 614, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, 24, 610, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4000, { .fields = { 16, 32 } }, -8, 8, 776, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 56 } }, 8, -16, 775, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, 40, -16, 803, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, 64, -24, 807, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 16 } }, -8, 16, 776, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 24 } }, 8, 0, 1000, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 48, 40 } }, 32, -16, 1025, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 32 } }, 80, -16, 1000, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 40 } }, 96, -24, 715, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, 128, -24, 722, { .fields = { 96, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, 152, -24, 721, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4080, { .fields = { 48, 48 } }, -160, 8, 1583, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 16, 64 } }, -112, 0, 1582, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 56, 1861, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 56, 8 } }, -160, 64, 1587, { .fields = { 0, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 64, 8 } }, -160, 72, 1321, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 32, 16 } }, -88, 8, 1544, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 48, 16 } }, -88, 24, 1532, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 40, 16 } }, -96, 40, 1381, { .fields = { 32, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 88, 1030, { .fields = { 8, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 48, 8 } }, -160, 80, 1155, { .fields = { 0, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 32 } }, -64, 40, 838, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 24 } }, -8, 32, 725, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 32, 40 } }, -48, 32, 765, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x40C0, { .fields = { 8, 16 } }, -88, 48, 932, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 24 } }, -80, 48, 867, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 32 } }, -16, 32, 756, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, 0, 8, 1094, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, 8, 8, 760, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -8, 16, 972, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, 0, 16, 769, { .fields = { 24, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -24, 24, 762, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -16, 24, 762, { .fields = { 40, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -8, 24, 787, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -40, 32, 761, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -32, 32, 838, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -24, 32, 794, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -96, -8, 1000, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -80, -8, 1038, { .fields = { 24, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -64, -8, 1084, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -48, -8, 1128, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -32, -8, 1171, { .fields = { 24, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -88, 0, 950, { .fields = { 24, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -72, 0, 976, { .fields = { 24, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -56, 0, 1019, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -40, 0, 1060, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -24, 0, 1120, { .fields = { 24, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -8, 0, 1192, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -80, 8, 920, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -64, 8, 925, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -48, 8, 937, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -32, 8, 975, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -16, 8, 1025, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -72, 16, 897, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -56, 16, 903, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -40, 16, 924, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -24, 16, 954, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -56, 24, 775, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -40, 24, 787, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 15's eleven source ranges followed by SPRITE_BATCH_END.
///
/// The first zero count selects image strips. Cutscenes hide batch 7; flashes
/// and flicker change batches 8..10. Batch 10 is a retained zero-length range
/// at the source array's end and is independently writable, not a terminator.
static SpriteBatch _gDryfieldNightGasStationView15SpriteBatches[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 6, 0 } },
    { 21, 9, 0, 0, { 1, 0 } },
    { 30, 40, 0, 0, { 8, 0 } },
    { 70, 14, 0, 0, { 0, 0 } },
    { 84, 4, 0, 0, { 5, 0 } },
    { 88, 7, 0, 0, { 2, 0 } },
    { 95, 10, 0, 0, { 9, 0 } },
    { 105, 6, 0, 0, { 4, 0 } },
    { 111, ARRAY_SIZE(_gDryfieldNightGasStationView15SpriteSources) - 111, 0, 0, { 7, 0 } },
    { ARRAY_SIZE(_gDryfieldNightGasStationView15SpriteSources), 0, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 16's empty sprite list, selecting decoded background image strips.
///
/// Its source slot retains this same list address; the zero range is not a terminator.
static SpriteBatch _gDryfieldNightGasStationView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 17's 161 textured rectangles and sorting depths.
///
/// Position and size use pixels relative to the drawing origin; depth is an
/// unscaled ordering value. RGB starts neutral; sources 103..118 request
/// semitransparency. The room overlay owns the sources while packets use them.
static SpriteSource _gDryfieldNightGasStationView17SpriteSources[161] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 16, 933, { .fields = { 16, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 824, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 789, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 64, 768, { .fields = { 40, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 64, 768, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 64, 781, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, 64, 1137, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 56, 696, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -72, 48, 713, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 740, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, 48, 741, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 692, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, 48, 671, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, 48, 651, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -24, 48, 952, { .fields = { 112, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -16, 48, 956, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -8, 48, 971, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -8, 16, 960, { .fields = { 120, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, 8, 973, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 16, 48, 952, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, 40, 972, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 8, 681, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 32, 40, 959, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 72, 0, 632, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -8, 582, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 588, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 104, 0, 605, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, 0, 570, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, 32, 951, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 48, 8, 632, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 16, 625, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 24, 625, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 104, 747, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 96, 741, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 8, 80, 819, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, 64, 705, { .fields = { 56, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, 64, 697, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 32, 40, 671, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, 24, 645, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 24, 640, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, 24, 648, { .fields = { 24, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 64, 24, 645, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 24, 637, { .fields = { 8, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 24, 649, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 608, { .fields = { 88, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 24, 1027, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 24, 1074, { .fields = { 24, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 32, 977, { .fields = { 24, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 929, { .fields = { 24, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 890, { .fields = { 24, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 32, 1010, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 40, 958, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 48, 918, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 24, 1120, { .fields = { 24, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 32, 1050, { .fields = { 24, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 40, 991, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 48, 942, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 24, 1142, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 32, 1102, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 32, 1195, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 40, 1025, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 40, 992, { .fields = { 48, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 936, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 902, { .fields = { 16, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 975, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -128, 56, 888, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 64, 846, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 1170, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 72, 808, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 80, 776, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 871, { .fields = { 8, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, 64, 944, { .fields = { 16, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 917, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 984, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 48, 923, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 48, 881, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 56, 1005, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, 56, 1108, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1153, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 921, { .fields = { 40, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 603, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 48, 56, 588, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 72, 48, 559, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 88, 534, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, 40, 578, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 64, 8, 609, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 8, 605, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 651, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, 64, 683, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, 56, 666, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, 48, 653, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 32, 657, { .fields = { 120, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 24, 681, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 645, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 624, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, 16, 599, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -16, 16, 601, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 16, 606, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 72, 589, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 72, 570, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 72, 578, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -32, 56, 614, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 779, { .fields = { 112, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 8, 32 } }, -72, 48, 663, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -64, 32, 676, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -56, 24, 687, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 64 } }, -32, 16, 646, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, 16, 631, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, -88, 56, 768, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, -16, 16, 631, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -48, 24, 673, { .fields = { 32, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 16 } }, -88, 56, 697, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 16 } }, -64, 48, 715, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, -48, 24, 923, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 32 } }, -24, 24, 947, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 40 } }, -8, 16, 933, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, 24, 16, 650, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 48 } }, 48, 8, 625, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, 80, 8, 625, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 8, 8 } }, -136, 56, 2317, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 8 } }, -160, 96, 1058, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 48, 1373, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 56, 1372, { .fields = { 40, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 64, 1365, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 48, 8 } }, -160, 72, 1021, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 80, 1340, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 24, 8 } }, -160, 88, 1094, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 40 } }, -160, 80, 799, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x40C0, { .fields = { 16, 48 } }, -104, 72, 744, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 40 } }, -136, 80, 760, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x40C0, { .fields = { 8, 40 } }, -112, 80, 762, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 24, 1020, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 32, 965, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 40, 925, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 48, 875, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -152, 56, 877, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 24, 1065, { .fields = { 56, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 32, 1005, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 40, 950, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 48, 907, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 24, 1116, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 32, 1028, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 40, 973, { .fields = { 32, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 48, 937, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -128, 56, 876, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -104, 56, 911, { .fields = { 40, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -136, 64, 843, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -120, 64, 762, { .fields = { 40, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -128, 72, 770, { .fields = { 40, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -104, 64, 762, { .fields = { 40, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x40C0, { .fields = { 16, 16 } }, -112, 72, 745, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -96, 72, 778, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 64, 694, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 56, 912, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 48, 940, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 40, 976, { .fields = { 48, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 32, 1002, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -64, 32, 740, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 32, 8 } }, -64, 40, 711, { .fields = { 24, 56 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 32, 8 } }, -64, 48, 696, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 24, 24 } }, -104, 80, 662, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
};

/// Mapped view 17's eleven source ranges followed by SPRITE_BATCH_END.
///
/// The first zero count selects image strips. Cutscenes hide batch 7; flashes
/// and flicker change batches 8..10. Hidden ranges retain their cached packet positions.
static SpriteBatch _gDryfieldNightGasStationView17SpriteBatches[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 6, 0 } },
    { 32, 12, 0, 0, { 1, 0 } },
    { 44, 43, 0, 0, { 8, 0 } },
    { 87, 16, 0, 0, { 0, 0 } },
    { 103, 8, 0, 0, { 5, 0 } },
    { 111, 8, 0, 0, { 2, 0 } },
    { 119, 8, 0, 0, { 9, 0 } },
    { 127, 4, 0, 0, { 4, 0 } },
    { 131, 29, 0, 0, { 7, 0 } },
    { 160, ARRAY_SIZE(_gDryfieldNightGasStationView17SpriteSources) - 160, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 18's empty sprite list, selecting decoded background image strips.
///
/// Its source slot retains this same list address; the zero range is not a terminator.
static SpriteBatch _gDryfieldNightGasStationView18SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 19's empty sprite list, selecting decoded background image strips.
///
/// Its source slot retains this same list address; the zero range is not a terminator.
static SpriteBatch _gDryfieldNightGasStationView19SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 20's 57 textured rectangles and sorting depths, including the distant balcony.
///
/// Position and size use pixels relative to the drawing origin; depth is an
/// unscaled ordering value. RGB starts neutral; source 56 requests
/// semitransparency. The room overlay owns the sources while packets use them.
static SpriteSource _gDryfieldNightGasStationView20SpriteSources[57] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 2553, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 2526, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 2617, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 2422, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -104, 2423, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -96, 0x61A7, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -80, 2672, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -48, 2664, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 32, 2662, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 48, 2603, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -120, 2750, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -104, 2681, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, -24, 2657, { .fields = { 88, 8 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -24, 2535, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -120, 2540, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, -120, 2626, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -40, -120, 2785, { .fields = { 40, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 8, -120, 2981, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 16, 2526, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -160, -32, 1084, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -152, -24, 1200, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -144, -24, 1313, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -16, 1577, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -128, -8, 1831, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 0, 2075, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 24, 2440, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -80, 24, 2691, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 80, 40 } }, -16, 24, 2725, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, -16, 1550, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -56, 2601, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -48, 2598, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 2550, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, -32, 2505, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, -24, 2648, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, -16, 2666, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, -8, 2675, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 0, 2673, { .fields = { 80, 16 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 8, 2646, { .fields = { 72, 24 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 16, 2558, { .fields = { 48, 88 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -16, 24, 2749, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, 32, 2718, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 40, 2740, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 48, 2676, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, 56, 2675, { .fields = { 80, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -16, 4287, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4000, { .fields = { 16, 24 } }, -40, -16, 0xD08F, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4040, { .fields = { 64, 16 } }, -88, -8, 4112, { .fields = { 72, 104 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 8 } }, -120, 64, 1430, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 40 } }, -56, 16, 1392, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 16, 72 } }, -72, 0, 1396, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 8, 64 } }, -80, 0, 1384, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 24, 56 } }, -104, 0, 1424, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 16, 64 } }, -120, 0, 1425, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 143, 0x4080, { .fields = { 16, 64 } }, -136, 8, 1425, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 72, 32 } }, -128, 16, 1401, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x4080, { .fields = { 72, 16 } }, -128, 0, 1450, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_OPAQUE },
    { 142, 0x40C0, { .fields = { 72, 24 } }, -128, 0, 1407, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_GAS_STATION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Mapped view 20's ten source ranges followed by SPRITE_BATCH_END.
///
/// The leading zero count selects background image strips. Saved balcony
/// damage changes batch 6's visibility without discarding its cached packet.
static SpriteBatch _gDryfieldNightGasStationView20SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { 18, 11, 0, 0, { 5, 0 } },
    { 29, 15, 0, 0, { 4, 0 } },
    { 44, 1, 0, 0, { 6, 0 } },
    { 45, 1, 0, 0, { 1, 0 } },
    { 46, 1, 0, 0, { 7, 0 } },
    { 47, 7, 0, 0, { 3, 0 } },
    { 54, 2, 0, 0, { 8, 0 } },
    { 56, ARRAY_SIZE(_gDryfieldNightGasStationView20SpriteSources) - 56, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 21's empty sprite list, selecting decoded background image strips.
///
/// Its source slot retains this same list address; the zero range is not a terminator.
static SpriteBatch _gDryfieldNightGasStationView21SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightGasStationSpriteViews[21] = {
    { { .empty = _gDryfieldNightGasStationView1SpriteBatches }, _gDryfieldNightGasStationView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView2SpriteSources }, _gDryfieldNightGasStationView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView3SpriteSources }, _gDryfieldNightGasStationView3SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView4SpriteBatches }, _gDryfieldNightGasStationView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView5SpriteSources }, _gDryfieldNightGasStationView5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView6SpriteBatches }, _gDryfieldNightGasStationView6SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView7SpriteBatches }, _gDryfieldNightGasStationView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView8SpriteBatches }, _gDryfieldNightGasStationView8SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView9SpriteBatches }, _gDryfieldNightGasStationView9SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView10SpriteBatches }, _gDryfieldNightGasStationView10SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView11SpriteSources }, _gDryfieldNightGasStationView11SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView12SpriteBatches }, _gDryfieldNightGasStationView12SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView13SpriteSources }, _gDryfieldNightGasStationView13SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView14SpriteSources }, _gDryfieldNightGasStationView14SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView15SpriteSources }, _gDryfieldNightGasStationView15SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView16SpriteBatches }, _gDryfieldNightGasStationView16SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView17SpriteSources }, _gDryfieldNightGasStationView17SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView18SpriteBatches }, _gDryfieldNightGasStationView18SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView19SpriteBatches }, _gDryfieldNightGasStationView19SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGasStationView20SpriteSources }, _gDryfieldNightGasStationView20SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGasStationView21SpriteBatches }, _gDryfieldNightGasStationView21SpriteBatches, NULL },
};

/// Room 1's authored point lights for the nighttime Dryfield gas station.
///
/// All eight lights accept every view. Positions and falloff radii use integer
/// world units; RGB intensity has 12 fractional bits (`ONE` is full strength).
/// The loaded overlay owns this writable storage: coordinate updates and light
/// queries overwrite transform caches and attenuation.
static WorldCoordPointLight _gDryfieldNightGasStationRoom1PointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 656, -2500, -4504 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3353, -2000, 174 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6775, -2000, 174 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8271, -2000, 174 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2E46, -2500, -534 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x35FC, -2500, -544 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9661, -2500, -5043 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1861, 6306 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x40BE, -1000, -1697 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 2688, 1778 }, { 0, 0 } }, 4242, 6423 },
};

/// Room 1's collection of eight point lights, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightGasStationRoom1Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightGasStationRoom1PointLights),
    .pointLights           = _gDryfieldNightGasStationRoom1PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Authored point lights shared by nighttime Dryfield gas station room variants 2–4.
///
/// All seven lights accept every view. Positions and falloff radii use integer
/// world units; RGB intensity has 12 fractional bits (`ONE` is full strength).
/// The loaded overlay owns this writable storage: coordinate updates and light
/// queries overwrite transform caches and attenuation.
static WorldCoordPointLight _gDryfieldNightGasStationRooms2To4PointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 656, -2500, -4504 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3353, -2000, 174 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6775, -2000, 174 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8271, -2000, 174 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2E46, -2500, -534 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x35FC, -2500, -544 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9661, -2500, -5043 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1861, 5080 },
};

/// Seven point lights shared by room variants 2..4, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightGasStationRooms2To4Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightGasStationRooms2To4PointLights),
    .pointLights           = _gDryfieldNightGasStationRooms2To4PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Eleven directed view-boundary quads shared by all four room variants.
///
/// Pairs connect views 2/3, 3/4 and 4/6; the remaining boundary goes from 5
/// to 2. Setup binds and links every record through the final LAST-marked
/// element. Coordinates and link/hit fields are mutable overlay-owned storage.
static WorldCollisionTrigger _gDryfieldNightGasStationViewBoundaryTriggers[11] = {
    { NULL, NULL, NULL, { 5791, -2544, -3505, 0 }, { { 147, -3568, 2869, 0 }, { -146, -3568, -2868, 0 }, { 147, 3568, 2869, 0 }, { -146, 3568, -2868, 0 } }, { -4101, 0, 209, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5503, -2544, -3521, 0 }, { { -146, -3568, -2868, 0 }, { 147, -3568, 2869, 0 }, { -146, 3568, -2868, 0 }, { 147, 3568, 2869, 0 } }, { 4100, 0, -210, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4319, -2544, -1009, 0 }, { { -210, -3568, -804, 0 }, { 211, -3568, 805, 0 }, { -210, 3568, -804, 0 }, { 211, 3568, 805, 0 } }, { 3963, 0, -1038, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2752, -2528, -1264, 0 }, { { -1410, -3552, 524, 0 }, { 1411, -3552, -523, 0 }, { -1410, 3552, 524, 0 }, { 1411, 3552, -523, 0 } }, { -1432, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2623, -2608, -1345, 0 }, { { 1716, -3632, -661, 0 }, { -1716, -3632, 662, 0 }, { 1716, 3632, -661, 0 }, { -1716, 3632, 662, 0 } }, { 1474, 0, 3824, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -2624, -1297, 0 }, { { 283, -3648, 1064, 0 }, { -283, -3648, -1064, 0 }, { 283, 3648, 1064, 0 }, { -283, 3648, -1064, 0 } }, { -3966, 0, 1054, 0 }, { 0, 0, 4096, 0 }, 3805, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28FF, -2608, -1952, 0 }, { { -933, -3632, -2562, 0 }, { 911, -3632, 2537, 0 }, { -933, 3632, -2562, 0 }, { 911, 3632, 2537, 0 } }, { 3855, 0, -1395, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A06, -2592, -1961, 0 }, { { 875, -3616, 2397, 0 }, { -875, -3616, -2396, 0 }, { 875, 3616, 2397, 0 }, { -875, 3616, -2396, 0 } }, { -3858, 0, 1407, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x361F, -2624, -2482, 0 }, { { 857, -3616, -1637, 0 }, { -856, -3616, 1638, 0 }, { 857, 3616, -1637, 0 }, { -856, 3616, 1638, 0 } }, { 3633, 0, 1900, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x29DD, -2624, -5092, 0 }, { { -867, -3616, 888, 0 }, { 867, -3616, -888, 0 }, { -867, 3616, 888, 0 }, { 867, 3616, -888, 0 } }, { -2937, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28AD, -2688, -5043, 0 }, { { 899, -3616, -888, 0 }, { -899, -3616, 888, 0 }, { 899, 3616, -888, 0 }, { -899, 3616, 888, 0 } }, { 2895, 0, 2931, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Sixteen interaction quads shared by all four room variants.
///
/// Warp, CAP and room-action selectors interpret their parameter bytes;
/// warp parameters pack current/destination endpoint indices by nibble.
/// Setup binds, links and enables records through the final LAST-marked
/// element. Geometry is in game units; link, coordinate and hit fields are
/// mutable overlay-owned storage and must survive until unlinked.
static WorldCollisionTrigger _gDryfieldNightGasStationActionTriggers[16] = {
    { NULL, NULL, NULL, { 2528, -108, -332, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, (DRYFIELD_NIGHT_GAS_STATION_GENERAL_STORE_WARP_ID << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -80, -3616, 0 }, { { 384, 0, -1024, 0 }, { 384, 0, 1024, 0 }, { -384, 0, -1024, 0 }, { -384, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, (DRYFIELD_NIGHT_GAS_STATION_MAIN_STREET_WARP_ID << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3430, -96, -224, 0 }, { { -1776, 0, -384, 0 }, { 1776, 0, -384, 0 }, { -1776, 0, 384, 0 }, { 1776, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x33D2, -96, -5400, 0 }, { { -1173, 0, 517, 0 }, { 466, 0, -1005, 0 }, { -594, 0, 1118, 0 }, { 1301, 0, -628, 0 } }, { 0, 4096, 0, 0 }, { -3290, 0, -2441, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 30, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7552, -64, -1296, 0 }, { { -1776, 0, -624, 0 }, { 1776, 0, -624, 0 }, { -1776, 0, 624, 0 }, { 1776, 0, 624, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -4096, 0 }, 1881, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4400, -96, 240, 0 }, { { -736, 0, -1392, 0 }, { 736, 0, -1392, 0 }, { -736, 0, -272, 0 }, { 736, 0, -272, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 1572, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x367F, -64, -3585, 0 }, { { -756, 0, -780, 0 }, { 1209, 0, 1140, 0 }, { -1432, 0, 333, 0 }, { -331, 0, 1261, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1659, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4120, -64, 0, 0 }, { { -736, 0, -624, 0 }, { 736, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x366F, -64, 191, 0 }, { { 1312, 0, -2172, 0 }, { 1289, 0, 271, 0 }, { -1368, 0, -2173, 0 }, { -1365, 0, 272, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 14, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x31E9, -64, -4760, 0 }, { { -685, 0, -131, 0 }, { -70, 0, -693, 0 }, { 54, 0, 694, 0 }, { 701, 0, 132, 0 } }, { 0, 4102, 0, 0 }, { -2752, 0, 3034, 0 }, 712, WORLD_COLLISION_TRIGGER_ACTION_CAP, 30, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4608, -96, -4960, 0 }, { { -1024, 0, -1120, 0 }, { 1024, 0, -1120, 0 }, { -1024, 0, 1120, 0 }, { 1024, 0, 1120, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1514, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3C4F, -64, -1537, 0 }, { { -365, 0, -1008, 0 }, { 960, 0, 522, 0 }, { -959, 0, -521, 0 }, { 366, 0, 1009, 0 } }, { 0, 4099, 0, 0 }, { -3290, 0, 2440, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 543, -64, -1793, 0 }, { { -1401, 0, -1255, 0 }, { 1880, 0, 103, 0 }, { -1879, 0, -102, 0 }, { 1402, 0, 1256, 0 } }, { 0, 4108, 0, 0 }, { 2751, 0, -3035, 0 }, 1881, WORLD_COLLISION_TRIGGER_ACTION_CAP, 27, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x31C0, -64, -384, 0 }, { { -2416, 0, -704, 0 }, { 2416, 0, -704, 0 }, { -2416, 0, 704, 0 }, { 2416, 0, 704, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2508, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3660, -64, -1152, 0 }, { { -1024, 0, -624, 0 }, { 1088, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -608, 0 }, { { -480, 0, -560, 0 }, { 480, 0, -560, 0 }, { -480, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 735, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement variant 1's Scavenger file and task recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 16 serves twelve placements. The loaded room owns this list and
/// borrows the actor package's descriptor through loading and spawning.
static AreaResource _gDryfieldNightGasStationVariant1Resources[2] = {
    { .entryId = 16, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement variant 2's Zebra Stalker file and task recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 6 serves two placements. The loaded room owns this list and
/// borrows the actor package's descriptor through loading and spawning.
static AreaResource _gDryfieldNightGasStationVariant2Resources[2] = {
    { .entryId = 6, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement variant 3's Desert Chaser file and task recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 1 serves two placements. The loaded room owns this list and
/// borrows the actor package's descriptor through loading and spawning.
static AreaResource _gDryfieldNightGasStationVariant3Resources[2] = {
    { .entryId = 1, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement variant 7's two Rook GOLEM resource recipes, followed by AREA_PLACEMENT_END.
///
/// Entries 23 and 57 each serve one placement and select their actor package's
/// first descriptor. The room-owned list borrows both tables through loading and spawning.
static AreaResource _gDryfieldNightGasStationVariant7Resources[3] = {
    { .entryId = 23, .fileNumber = 23, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102300RookGolemTasks },
    { .entryId = 57, .fileNumber = 57, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor05700RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement variant 11's Zebra Stalker and Mind Suckler recipes, followed by AREA_PLACEMENT_END.
///
/// Entry 6 serves one Zebra Stalker; entry 15 serves three Mind Sucklers.
/// The room-owned list borrows both actor descriptors through loading and spawning.
static AreaResource _gDryfieldNightGasStationVariant11Resources[3] = {
    { .entryId = 6, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = 15, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightGasStationAreaVariants[12] = {
    [1]  = { .placements = gMapDryfieldFullGasStationVariant1Placements, .resources = _gDryfieldNightGasStationVariant1Resources },
    [2]  = { .placements = gMapDryfieldFullGasStationVariant2Placements, .resources = _gDryfieldNightGasStationVariant2Resources },
    [3]  = { .placements = gMapDryfieldFullGasStationVariant3Placements, .resources = _gDryfieldNightGasStationVariant3Resources },
    [7]  = { .placements = gMapDryfieldFullGasStationVariant7Placements, .resources = _gDryfieldNightGasStationVariant7Resources },
    [11] = { .placements = gMapDryfieldFullGasStationVariant11Placements, .resources = _gDryfieldNightGasStationVariant11Resources },
};

/// Ambient minima for logical views 1..21, shared by all four room variants.
///
/// Entry zero is the count header. Colours use 16 units per 8-bit level;
/// views 3, 4 and 6 are brighter than the other views. The fourth component
/// stores authored brightness and is not read by model lighting.
static WorldCoordRoomAmbientEntry _gDryfieldNightGasStationAmbientByView[22] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightGasStationAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 618, 615, 618, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Cue-2 footstep bases in the currently loaded type-1 sound bank.
///
/// Cue 1 adds one; companion playback adds 100. Surfaces 1 and 4 share a set.
enum {
    DRYFIELD_NIGHT_GAS_STATION_SURFACES_1_AND_4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_NIGHT_GAS_STATION_SURFACES_1_AND_4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    DRYFIELD_NIGHT_GAS_STATION_SURFACE_2_WALK_SOUND        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1D,
    DRYFIELD_NIGHT_GAS_STATION_SURFACE_2_RUN_SOUND         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1F,
    DRYFIELD_NIGHT_GAS_STATION_SURFACE_3_WALK_SOUND        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    DRYFIELD_NIGHT_GAS_STATION_SURFACE_3_RUN_SOUND         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
};

/// Footstep cue bases shared by surface classes 1 and 4; scripted jumps reuse walking.
static WorldCollisionFootstepSounds _gDryfieldNightGasStationSurfaces1And4FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_GAS_STATION_SURFACES_1_AND_4_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_GAS_STATION_SURFACES_1_AND_4_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_GAS_STATION_SURFACES_1_AND_4_WALK_SOUND,
};

/// Surface class 2's footstep cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gDryfieldNightGasStationSurface2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_GAS_STATION_SURFACE_2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_GAS_STATION_SURFACE_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_GAS_STATION_SURFACE_2_WALK_SOUND,
};

/// Surface class 3's footstep cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gDryfieldNightGasStationSurface3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_GAS_STATION_SURFACE_3_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_GAS_STATION_SURFACE_3_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_GAS_STATION_SURFACE_3_WALK_SOUND,
};

/// Blocking policy without footstep cues, shared by surface classes 0 and 5..7.
static WorldCollisionSurfaceProperties _gDryfieldNightGasStationSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1 passes probes and ignores weapon impacts while applying pushback.
static WorldCollisionSurfaceProperties _gDryfieldNightGasStationSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightGasStationSurfaces1And4FootstepSounds,
};

/// Surface class 2 blocks probes, allows weapon impacts and has its own footstep pair.
static WorldCollisionSurfaceProperties _gDryfieldNightGasStationSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightGasStationSurface2FootstepSounds,
};

/// Surface class 3 blocks probes, allows weapon impacts and has its own footstep pair.
static WorldCollisionSurfaceProperties _gDryfieldNightGasStationSurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightGasStationSurface3FootstepSounds,
};

/// Surface class 4 blocks probes and allows weapon impacts, sharing class 1's footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightGasStationSurface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightGasStationSurfaces1And4FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightGasStationSurfacePropertiesByClass[8] = {
    &_gDryfieldNightGasStationSilentSurfaceProperties,
    &_gDryfieldNightGasStationSurface1Properties,
    &_gDryfieldNightGasStationSurface2Properties,
    &_gDryfieldNightGasStationSurface3Properties,
    &_gDryfieldNightGasStationSurface4Properties,
    &_gDryfieldNightGasStationSilentSurfaceProperties,
    &_gDryfieldNightGasStationSilentSurfaceProperties,
    &_gDryfieldNightGasStationSilentSurfaceProperties,
};

/// Empty saved-area update list retained at the follow-up scene transition.
///
/// Its sole record is `AREA_APPLY_END`, so the synchronous apply call changes no
/// area state. Route-opening game flags are committed separately by the caller.
static const AreaApplyRec _gDryfieldNightGasStationFollowupAreaUpdates[1] SECTION(".data") = {
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Borrowed handle of the player's head-aim task toward the companion, or NULL.
///
/// Every spawn overwrites it, including with NULL on failure. Explicit stop
/// and callback teardown clear it; room initialization clears it without
/// killing a task, so no previous task may remain then. The scheduler owns it.
static Task* _gDryfieldNightGasStationCompanionHeadAimTask = NULL;

/// Borrowed handle shared by the baked-track and companion-attached red streaks.
///
/// Spawning either replaces it, including with NULL on failure. Stop requests
/// set the task's exit state and clear the handle before deferred teardown.
/// Automatic expiry after 100 draws leaves this handle intact; a non-NULL
/// value alone does not establish liveness. The scheduler owns the task.
static Task* _gDryfieldNightGasStationRedStreakTask;

/// Borrowed handle of the looping cutscene sprite-flicker controller, or NULL.
///
/// Each spawn overwrites it, including on failure. The stop callback clears
/// it immediately; the task hides sprites and releases itself on its next
/// update. Room initialization requires the previous controller to be absent.
static Task* _gDryfieldNightGasStationSpriteFlickerTask;

static void _dryfieldNightGasStationInitializeRoomTask(Task* task);
static void _dryfieldNightGasStationUpdateRoomTask(Task* task);

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x35\x96"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

void dryfieldNightGasStationTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

/// Registers the room's messages and prepares the companion and first-visit scene.
///
/// State zero advances to the room's update state. Recorded follow-up progress
/// restores the companion's placement and collision-disabled pose on reentry.
/// First entry records its objective and companion schedule even if the
/// companion is absent; only a live companion enables the skippable intro.
static void _dryfieldNightGasStationInitializeRoomTask(Task* task)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_INTRO_OBJECTIVE          = 0x12,
           DRYFIELD_NIGHT_GAS_STATION_COMPANION_INTRO_SCHEDULE = 2 };

    task->msgTable = _gDryfieldNightGasStationRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) >= DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_STARTED) && (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL)) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldNightGasStationCompanionWaitingTransform, 0);
        companionWriteAnimationBankIndex(&_gDryfieldNightGasStationCompanionReentryIdleAnimation.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &_gDryfieldNightGasStationCompanionReentryIdleAnimation, 0);
        _dryfieldNightGasStationResetCutsceneObstacleCollision(0);
    }
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_FIRST_VISIT) == 0) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_GAS_STATION_FIRST_VISIT, 1);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_GAS_STATION_INTRO_OBJECTIVE);
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, DRYFIELD_NIGHT_GAS_STATION_COMPANION_INTRO_SCHEDULE);
        _dryfieldNightGasStationClearCutsceneTaskHandles();
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            evsStartScriptWithSkip(_gDryfieldNightGasStationEncounterScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldNightGasStationEncounterSkipScript);
        }
    }
    task->state += 1;
    // Route completed CAP room commands back through the room's sound handler.
    gCapRoomCompletionSoundsEnabled = CAP_ROOM_COMPLETION_SOUNDS_ENABLED;
}

#include "../../shared/room/room_variants_gas_station.inc.c"

#include "../../shared/gas_station_sounds_cue.inc.c"

/// Tests whether the active trigger list contains a hit room-event action.
///
/// Returns 1 for the first room action with event ID 255 and nonzero hit,
/// otherwise 0, including an empty list. Traverses live, NULL-terminated
/// collision records without consuming or clearing the hit. The jerry-can
/// handler uses this latch independently of ordinary action-input dispatch.
static inline s32 _dryfieldNightGasStationHasRoomEventTriggerHit(void)
{
    const WorldCollisionTrigger* trigger;

    trigger = gWorldCollisionActionTriggerListHead;
    while (trigger != NULL) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return 1;
        }
        trigger = trigger->next;
    }
    return 0;
}

/// Accepts the jerry can at an active room-event trigger and starts its cutscene task.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; itemId is a collection ID, not an
/// inventory-row index. Returns `ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE` on a hit
/// and `ROOM_KEY_ITEM_USE_REFUSED` otherwise. Acceptance holds event handling
/// and hides the HUD before spawning; it does not test spawn success.
static s32 _dryfieldNightGasStationUseJerryCanMsg(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    if (itemId == INVENTORY_COLLECTION_ID_JERRY_CAN) {
        if (_dryfieldNightGasStationHasRoomEventTriggerHit()) {
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            taskSpawnFromTableOnDefaultList(_gDryfieldNightGasStationJerryCanTaskDescriptors, 0, 0, 0);
            return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Selects the night gas station's CAP sequences for room command messages.
///
/// Commands 1 and 5 request the transition sequence and gasoline-dependent
/// dialogue. Command 23 examines room variant 4, advancing its saved response
/// when the SUV key has been collected. Other commands do nothing. The receiver,
/// message ID and second payload are ignored; every path returns zero.
static s32 _dryfieldNightGasStationCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        CAP_TRANSITION_COMMAND   = 1,
        GASOLINE_CHECK_COMMAND   = 5,
        EXAMINE_COMMAND          = 23,
        EXAMINE_ROOM             = 4,
        EXAMINE_INITIAL          = 0,
        EXAMINE_FIRST_KEY_VISIT  = 1,
        EXAMINE_REPEAT_KEY_VISIT = 2,
        CAP_TRANSITION_SEQUENCE  = 17,
        GASOLINE_SEQUENCE        = 18,
        FIRST_KEY_SEQUENCE       = 32,
        REPEAT_KEY_SEQUENCE      = 31,
    };
    s16 gasolineCollected;

    if (commandId == CAP_TRANSITION_COMMAND) {
        capRunCommandWithTransition(CAP_TRANSITION_SEQUENCE);
    }
    if (commandId == GASOLINE_CHECK_COMMAND) {
        if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
            // Preserve the jerry-can query even though it does not select the reply.
            inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN);
            gasolineCollected = 0;
        } else {
            gasolineCollected = 1;
        }
        capStartSequenceSlot(GASOLINE_SEQUENCE, 1, gasolineCollected);
    }
    if ((commandId == EXAMINE_COMMAND) && (gGameSession->location.roomState == EXAMINE_ROOM)) {
        if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_SUV_KEY) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_EXAMINE_STATE) == EXAMINE_INITIAL) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_GAS_STATION_EXAMINE_STATE, EXAMINE_FIRST_KEY_VISIT);
            } else {
                gameFlagSetNibble(GAME_FLAG_NIGHT_GAS_STATION_EXAMINE_STATE, EXAMINE_REPEAT_KEY_VISIT);
            }
        }
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_EXAMINE_STATE) != EXAMINE_INITIAL ? (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_EXAMINE_STATE) == EXAMINE_FIRST_KEY_VISIT ? FIRST_KEY_SEQUENCE : REPEAT_KEY_SEQUENCE) : commandId, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

/// Starts the companion interaction after the follow-up scene has begun.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`, borrowing a non-NULL request
/// during dispatch. Only action 14 with a live companion is accepted; all
/// paths return zero, and the other callback arguments are ignored.
static s32 _dryfieldNightGasStationCompanionActionMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_COMPANION_ACTION = 14 };

    if ((request->actionId == DRYFIELD_NIGHT_GAS_STATION_COMPANION_ACTION) && (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) && (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) >= DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_STARTED)) {
        evsStartScript(_gDryfieldNightGasStationCompanionInteractionScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Arms the deferred follow-up scene on the room's first actor event.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT`, ignoring both payload words and
/// returning zero. Marks progress pending, restores actor animations, requests
/// weapon re-equipping, opens the two blocked routes and fades MIDI sequence
/// zero over 100 ticks. Further events leave that work untouched.
static s32 _dryfieldNightGasStationArmFollowupSceneMsg(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_FOLLOWUP_MIDI_FADE_TICKS = 100 };

    if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) == DRYFIELD_NIGHT_GAS_STATION_PROGRESS_INITIAL) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS, DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_PENDING);
        evsStartScript(_gDryfieldNightGasStationRestoreActorAnimationsScript, EVENT_SCRIPT_HUD_KEEP);
        gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        areaApplySavedUpdates(_gDryfieldNightGasStationFollowupAreaUpdates);
        gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED, 0);
        gameFlagSetNibble(GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED, 0);
        sndEvtRequestMidiStop(0, DRYFIELD_NIGHT_GAS_STATION_FOLLOWUP_MIDI_FADE_TICKS);
    }
    return 0;
}

/// Starts the jerry-can scene under a blank display and releases this one-shot task.
///
/// Holds the player's ordinary update, hides the HUD and clears the collected
/// jerry can before starting the skippable script. Records the next objective
/// and saved audio scene event. The script owns subsequent display and hold
/// restoration; this bodyless task ends immediately.
static void _dryfieldNightGasStationStartJerryCanCutsceneTask(Task* task)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_OBJECTIVE   = 0x16,
           DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_AUDIO_EVENT = 4 };

    gGameSession->hideHud     = 1;
    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
    SetDispMask(0);
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN);
    evsStartScriptWithSkip(_gDryfieldNightGasStationJerryCanScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldNightGasStationJerryCanSkipScript);
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_OBJECTIVE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = DRYFIELD_NIGHT_GAS_STATION_JERRY_CAN_AUDIO_EVENT;
    taskKill(task);
}

/// Starts the pending follow-up scene once event handling and the attachment wheel are free.
///
/// Progress advances before starting the skippable script, preventing another
/// room update from starting it again. The task argument is unused.
static void _dryfieldNightGasStationUpdateRoomTask(Task* task)
{
    s32 progress;

    if (gGameSession->eventState == 0) {
        progress = gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS);
        if ((progress == DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_PENDING) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL)) {
            gameFlagSetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS, DRYFIELD_NIGHT_GAS_STATION_PROGRESS_FOLLOWUP_STARTED);
            evsStartScriptWithSkip(_gDryfieldNightGasStationFollowupScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldNightGasStationFollowupSkipScript);
        }
    }
}

/// Holds or resumes the player's ordinary state update during the room cutscene.
///
/// Zero resumes the update; any nonzero byte holds it. Pending displacement,
/// collision and other work outside the ordinary state dispatcher still run.
static void _dryfieldNightGasStationSetPlayerUpdateHold(u8 holdPlayerUpdate)
{
    gPlayerActorStateTickHeld = holdPlayerUpdate;
}

/// Room-task state handlers: 0 initialize, 1 update deferred scenes, 2 release.
///
/// The dispatcher copies all three callback pointers before indexing by
/// `Task::state`, which must be in 0..2. There is no terminator or bounds check;
/// callbacks must remain loaded throughout the task's lifetime.
static const TaskFuncTable3 _gDryfieldNightGasStationRoomTaskStates = {
    {
        _dryfieldNightGasStationInitializeRoomTask,
        _dryfieldNightGasStationUpdateRoomTask,
        taskKill,
    },
};

/// Room-space translation of the rotated baked red-streak track, in game-coordinate units.
static const SVECTOR _gDryfieldNightGasStationTrackRedStreakWorldOffset = { 0x3B23, -0x498, -0xD76, 0 };

/// Start of the companion red streak in model-part-8 space, in game-coordinate units.
///
/// The other endpoint adds 400 to local Y before both points are transformed.
static const SVECTOR _gDryfieldNightGasStationCompanionRedStreakLocalStart = { -0x1E, 0x122, 0x28, 0 };

void dryfieldNightGasStationRoomTask(Task* task)
{
    TaskFuncTable3 roomStates;

    roomStates = _gDryfieldNightGasStationRoomTaskStates;
    _dryfieldNightGasStationRestoreBalconySpriteVisibility();
    roomStates.funcs[task->state](task);
}

/// Restores the cutscene obstacle in the live room collision grid.
///
/// Copies four faces and normal XYZs and eight vertex XYZs from the obstacle
/// template. A nonzero `displaceObstacle` then adds 3000 room-coordinate units
/// to vertex Y. Other faces, cell lists and SVECTOR fourth halfwords are retained.
/// Both grids and their geometry must remain loaded and writable as appropriate.
static void _dryfieldNightGasStationResetCutsceneObstacleCollision(s32 displaceObstacle)
{
    WorldCollisionGrid* liveGrid;
    WorldCollisionGrid* obstacleTemplate;
    SVECTOR             displacement;
    s32                 index;

    liveGrid         = &_gDryfieldNightGasStationRooms1To3CollisionGrid;
    obstacleTemplate = &_gDryfieldNightGasStationCompanionWaitingCollisionGrid;

    // Restore the obstacle without rebuilding the grid cell lists.
    for (index = 0; index < DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_FACE_COUNT; index++) {
        liveGrid->normals[index].vx = obstacleTemplate->normals[index].vx;
        liveGrid->normals[index].vy = obstacleTemplate->normals[index].vy;
        liveGrid->normals[index].vz = obstacleTemplate->normals[index].vz;
        liveGrid->faces[index]      = obstacleTemplate->faces[index];
    }

    for (index = 0; index < DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx = obstacleTemplate->vertices[index].vx;
        liveGrid->vertices[index].vy = obstacleTemplate->vertices[index].vy;
        liveGrid->vertices[index].vz = obstacleTemplate->vertices[index].vz;
    }

    if (displaceObstacle == 0) {
        displacement.vx = 0;
        displacement.vy = 0;
    } else {
        displacement.vx = 0;
        displacement.vy = DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_DISPLACEMENT_Y;
    }
    displacement.vz = 0;

    for (index = 0; index < DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_OBSTACLE_VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx += displacement.vx;
        liveGrid->vertices[index].vy += displacement.vy;
        liveGrid->vertices[index].vz += displacement.vz;
    }
}

/// Draws an additive red point and flickering streak at one baked-track sample.
///
/// `frameIndex` must be in 0..99. The track is rotated and translated into
/// room coordinates. GPU packets borrow the current frame arena and remain
/// live until drawing completes; the caller must provide capacity for a TILE,
/// TILE_1, LINE_G2 and two draw-mode packets. Changes the GTE and SDK RNG state.
static void _dryfieldNightGasStationDrawTrackRedStreak(s32 frameIndex)
{
    SVECTOR   worldOffset;
    MATRIX    trackRotation;
    SVECTOR   worldPoint;
    long      screenXY;
    long      perspective;
    long      projectionFlags;
    u16       coreX;
    u16       coreY;
    u16       streakTipX;
    u16       streakTipY;
    u16       streakTailX;
    SVECTOR*  trackPoint;
    s32       drawValue;
    TILE*     core;
    TILE_1*   centrePixel;
    LINE_G2*  streak;
    DR_TPAGE* drawMode;

    /// Converts the baked track sample to room coordinates and projects it into the view.
    ///
    /// Captures trackRotation, trackPoint, worldOffset, worldPoint, screenXY, perspective
    /// and projectionFlags; writes the last four and changes the GTE. Rotation
    /// narrows XYZ to s16 before the room-offset additions narrow them again.
    /// `screenXY` packs signed pixel X/Y; `perspective` is the SDK interpolation
    /// output and `projectionFlags` the GTE FLAG word. No arguments.
    /// Use only as a complete statement sequence inside an explicit braced block.
#define DRYFIELD_NIGHT_GAS_STATION_PROJECT_TRACK_RED_POINT() \
    ApplyMatrixSV(&trackRotation, trackPoint, &worldPoint);  \
    SetRotMatrix(&gGfxViewCoord.workm);                      \
    SetTransMatrix(&gGfxViewCoord.workm);                    \
    worldPoint.vx += worldOffset.vx;                         \
    worldPoint.vy += worldOffset.vy;                         \
    worldPoint.vz += worldOffset.vz;                         \
    RotTransPers(&worldPoint, &screenXY, &perspective, &projectionFlags)

    // Place the baked local track in room coordinates before projecting.
    worldOffset = _gDryfieldNightGasStationTrackRedStreakWorldOffset;
    gfxSetRotIdentity(&trackRotation);
    RotMatrixY((s16)(DRYFIELD_NIGHT_GAS_STATION_TRACK_YAW), &trackRotation);
    trackPoint = &_gDryfieldNightGasStationRedStreakTrack[frameIndex];
    DRYFIELD_NIGHT_GAS_STATION_PROJECT_TRACK_RED_POINT();
    if (projectionFlags >= 0) {
        drawValue      = DRYFIELD_NIGHT_GAS_STATION_RED_STREAK_OT_DEPTH;
        coreX          = screenXY;
        coreY          = screenXY >> 16;
        core           = gGpuPrimCursor;
        gGpuPrimCursor = core + 1;
        setTile(core);
        setSemiTrans(core, 1);
        core->x0 = coreX - 1;
        core->y0 = coreY - 1;
        setRGB0(core, 0x80, 0, 0);
        core->w = 3;
        core->h = 3;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(drawValue), core);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 1, 0, getTPage(0, GPU_BLEND_ADD, 320, 0));
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(drawValue), drawMode);
        centrePixel    = gGpuPrimCursor;
        gGpuPrimCursor = centrePixel + 1;
        setTile1(centrePixel);
        setSemiTrans(centrePixel, 1);
        centrePixel->x0 = coreX;
        centrePixel->y0 = coreY;
        setRGB0(centrePixel, 0xFF, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(drawValue), centrePixel);

        // Reproject the tip and retain its Y for the screen-space streak tail.
        gfxSetRotIdentity(&trackRotation);
        drawValue = DRYFIELD_NIGHT_GAS_STATION_TRACK_YAW;
        RotMatrixY((s16)(drawValue), &trackRotation);
        DRYFIELD_NIGHT_GAS_STATION_PROJECT_TRACK_RED_POINT();
#undef DRYFIELD_NIGHT_GAS_STATION_PROJECT_TRACK_RED_POINT
        streakTipX     = screenXY;
        streakTipY     = screenXY >> 16;
        worldPoint.vx -= 0x3E8;
        worldPoint.vy += 0x1F4;
        worldPoint.vz += 0x1F4;
        RotTransPers(&worldPoint, &screenXY, &perspective, &projectionFlags);
        streakTailX    = screenXY;
        streak         = gGpuPrimCursor;
        gGpuPrimCursor = streak + 1;
        setLineG2(streak);
        setSemiTrans(streak, 1);
        streak->x0 = streakTailX;
        streak->y0 = streakTipY + 0xA;
        streak->x1 = streakTipX;
        streak->y1 = streakTipY;
        setRGB1(streak, rand() % 60 + 0x50, 0, 0);
        drawValue = DRYFIELD_NIGHT_GAS_STATION_RED_STREAK_OT_DEPTH;
        setRGB0(streak, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(drawValue), streak);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 1, 0, getTPage(0, GPU_BLEND_ADD, 320, 0));
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(drawValue), drawMode);
    }
}

/// Draws an additive flickering red streak attached to companion model part 8.
///
/// `unusedFrame` is ignored.
/// Requires a live companion TMD with at least nine coordinates. The two local
/// endpoints differ by 400 game-coordinate units along Y. Consumes a LINE_G2
/// and a draw-mode packet from the current frame arena when projection succeeds;
/// the packets must survive GPU drawing. Changes the GTE and SDK RNG state.
static void _dryfieldNightGasStationDrawCompanionRedStreak(s32 unusedFrame)
{
    MATRIX    partRotation;
    SVECTOR   partPosition;
    SVECTOR   worldStart;
    SVECTOR   worldEnd;
    SVECTOR   localOffset;
    long      screenXY;
    long      perspective;
    long      startFlags;
    long      endFlags;
    u16       startX;
    u16       startY;
    u16       endX;
    u16       endY;
    GfxCoord* companionParts;
    LINE_G2*  streak;
    DR_TPAGE* drawMode;

    /// Converts a companion-part local endpoint into signed-halfword room coordinates.
    ///
    /// endpoint is an SVECTOR lvalue evaluated four times; pass a stable local.
    /// Captures partRotation, localOffset and partPosition. Rotation narrows
    /// XYZ first, then each position addition narrows again. Writes endpoint
    /// XYZ and changes the GTE. Use as a complete statement
    /// sequence inside an explicit braced block.
#define DRYFIELD_NIGHT_GAS_STATION_TRANSFORM_COMPANION_STREAK_POINT(endpoint) \
    ApplyMatrixSV(&partRotation, &localOffset, &(endpoint));                  \
    (endpoint).vx += partPosition.vx;                                         \
    (endpoint).vy += partPosition.vy;                                         \
    (endpoint).vz += partPosition.vz

    // Transform both endpoints from companion-part coordinates into the room.
    localOffset = _gDryfieldNightGasStationCompanionRedStreakLocalStart;
    gfxSetRotIdentity(&partRotation);
    companionParts = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    gfxComposeNodeWorldTransform(&companionParts[DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_PART], &partRotation, &partPosition);
    DRYFIELD_NIGHT_GAS_STATION_TRANSFORM_COMPANION_STREAK_POINT(worldStart);
    localOffset.vy += 0x190;
    DRYFIELD_NIGHT_GAS_STATION_TRANSFORM_COMPANION_STREAK_POINT(worldEnd);
#undef DRYFIELD_NIGHT_GAS_STATION_TRANSFORM_COMPANION_STREAK_POINT
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    RotTransPers(&worldStart, &screenXY, &perspective, &startFlags);
    startX = screenXY;
    startY = screenXY >> 16;
    RotTransPers(&worldEnd, &screenXY, &perspective, &endFlags);
    endX = screenXY;
    endY = screenXY >> 16;
    if (startFlags >= 0 && endFlags >= 0) {
        streak         = gGpuPrimCursor;
        gGpuPrimCursor = streak + 1;
        setLineG2(streak);
        setSemiTrans(streak, 1);
        streak->x0 = startX;
        streak->y0 = startY;
        streak->x1 = endX;
        streak->y1 = endY;
        // The negative red value deliberately wraps when stored in the GPU byte.
        setRGB0(streak, rand() % 100 - 0x7E, 0, 0);
        setRGB1(streak, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(DRYFIELD_NIGHT_GAS_STATION_RED_STREAK_OT_DEPTH), streak);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 1, 0, getTPage(0, GPU_BLEND_ADD, 320, 0));
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(DRYFIELD_NIGHT_GAS_STATION_RED_STREAK_OT_DEPTH), drawMode);
    }
}

/// Spawns a cutscene flash around placed actor 2 and shows its background flicker sprites.
///
/// Selectors 0..2 choose a local offset and flash size; any other selector
/// hides the sprites. If the placed actor is absent, neither action occurs.
/// A live actor must have a TMD root. Spawn offsets use its local coordinate
/// units; the child snapshots placement during `effectSpawn`.
static void _dryfieldNightGasStationSetCutsceneFlash(s32 flashIndex)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_FLASH_ANCHOR_INDEX = 2,
           DRYFIELD_NIGHT_GAS_STATION_FIRST_FLASH_SIZE   = 0x300,
           DRYFIELD_NIGHT_GAS_STATION_LATER_FLASH_SIZE   = 0x200 };
    Enemy*    anchorEnemy;
    GfxCoord* anchorCoord;
    SVECTOR   spawnOffset;

    anchorEnemy = sceneFindEnemyByPlaceKey(gGameSession->location.area | ((gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | (DRYFIELD_NIGHT_GAS_STATION_FLASH_ANCHOR_INDEX << ENEMY_PLACE_INDEX_SHIFT)));
    if (anchorEnemy != NULL) {
        anchorCoord = anchorEnemy->task->extra.tmd->coords;
        switch (flashIndex) {
            case DRYFIELD_NIGHT_GAS_STATION_FLASH_FIRST:
                spawnOffset.vx = 0;
                spawnOffset.vy = -0x64;
                spawnOffset.vz = -0x12C;
                effectSpawn(EFFECT_FLASH_BURST, anchorCoord, DRYFIELD_NIGHT_GAS_STATION_FIRST_FLASH_SIZE, &spawnOffset);
                _dryfieldNightGasStationSetFlickerSpritesVisible(DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_ON);
                break;

            case DRYFIELD_NIGHT_GAS_STATION_FLASH_SECOND:
                spawnOffset.vx = 0xC8;
                spawnOffset.vy = -0x64;
                spawnOffset.vz = -0xC8;
                effectSpawn(EFFECT_FLASH_BURST, anchorCoord, DRYFIELD_NIGHT_GAS_STATION_LATER_FLASH_SIZE, &spawnOffset);
                _dryfieldNightGasStationSetFlickerSpritesVisible(DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_ON);
                break;

            case DRYFIELD_NIGHT_GAS_STATION_FLASH_THIRD:
                spawnOffset.vx = -0x64;
                spawnOffset.vy = -0x64;
                spawnOffset.vz = -0xC8;
                effectSpawn(EFFECT_FLASH_BURST, anchorCoord, DRYFIELD_NIGHT_GAS_STATION_LATER_FLASH_SIZE, &spawnOffset);
                _dryfieldNightGasStationSetFlickerSpritesVisible(DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_ON);
                break;

            default:
                _dryfieldNightGasStationSetFlickerSpritesVisible(DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_OFF);
                break;
        }
    }
}

/// Stages the selected cutscene's deferred audio-start request.
///
/// The selected scene and playback buffers must survive later CD dispatch;
/// this callback leaves committing the request to the scene controller.
static void _dryfieldNightGasStationStageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Queues playback of the selected cutscene scene/audio session.
///
/// The selection and prepared buffers must survive consumption; the CD request
/// ring must have capacity when a scene slot is selected.
static void _dryfieldNightGasStationEnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes cutscene streaming and restores the random state saved at selection.
///
/// Requires a prior successful scene selection. Buffer, task and CD-request
/// teardown remain with their owners.
static void _dryfieldNightGasStationFinishScenePlayback(void)
{
    streamFinishScene();
}

/// Cancels the cutscene CD request and finishes its streaming session.
///
/// Requires a prior successful scene selection. Restores saved random state
/// immediately; queued CD cancellation completes on subsequent dispatches.
static void _dryfieldNightGasStationCancelScenePlayback(void)
{
    cdCmdCancelScene();
}

/// Spawns and retains the player's cutscene head aim toward the companion.
///
/// Each call replaces the saved aim handle, including on allocation failure.
/// The script must release an earlier aim task before calling again. Both actor
/// models and this overlay must remain loaded while the task borrows them.
static void _dryfieldNightGasStationSpawnCompanionHeadAimTask(void)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_TASK_INDEX = 0 };

    _gDryfieldNightGasStationCompanionHeadAimTask = taskSpawnFromTable(_gDryfieldNightGasStationCutsceneTaskDescriptors, DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_TASK_INDEX, 0, NULL);
}

/// Enables, fades out or stops the player's head aim toward the companion.
///
/// `aimMode` is 1 to ramp in or 0 to ramp out. Any other value kills the tracked
/// aim task and clears its handle; a missing task is a no-op. The task borrows
/// both actors' model storage while running.
static void _dryfieldNightGasStationSetCompanionHeadAim(s32 aimMode)
{
    if (_gDryfieldNightGasStationCompanionHeadAimTask == NULL) {
        return;
    }
    switch (aimMode) {
        case DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_OFF:
        case DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_ON:
            _gDryfieldNightGasStationCompanionHeadAimTask->spawnArg1.value = aimMode;
            break;
        default:
            taskKill(_gDryfieldNightGasStationCompanionHeadAimTask);
            _gDryfieldNightGasStationCompanionHeadAimTask = NULL;
            break;
    }
}

/// Ramps the player's head-aim blend toward the companion over sixteen active ticks.
///
/// State zero runs; other states exit. Nonzero `spawnArg1.value` ramps toward
/// ONE and zero ramps toward zero. `killCountdown` holds the 0..ONE blend weight.
/// The event-script pause byte suspends updates and teardown. Both live actor
/// models must have the head chain expected by `animationAimHeadAtTask`; losing
/// the player ends the task, while the companion must remain available.
static void _dryfieldNightGasStationCompanionHeadAimTask(Task* task)
{
    Task* playerTask;
    u16   nextBlendWeight;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gEvsPlaybackPaused == 0) {
        if (playerTask == NULL) {
            task->state = DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_EXIT;
        }
        if (task->state == DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_RUNNING) {
            // killCountdown stores the 1/4096 blend weight, not remaining time.
            if (task->spawnArg1.value != 0) {
                nextBlendWeight     = task->killCountdown + DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_BLEND_STEP;
                task->killCountdown = nextBlendWeight;
                if ((s16)nextBlendWeight >= (ONE + 1)) {
                    task->killCountdown = ONE;
                }
            } else {
                nextBlendWeight     = task->killCountdown - DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_BLEND_STEP;
                task->killCountdown = nextBlendWeight;
                if ((s16)nextBlendWeight < 0) {
                    task->killCountdown = 0;
                }
            }
            animationAimHeadAtTask(playerTask, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_MAX_YAW, DRYFIELD_NIGHT_GAS_STATION_HEAD_AIM_MAX_PITCH, task->killCountdown);
            return;
        }
        taskKill(task);
        _gDryfieldNightGasStationCompanionHeadAimTask = NULL;
    }
}

/// Engages the room's idle battle phase after the cutscene.
///
/// `unusedArg` is ignored by the combat API and retained for event callbacks;
/// other battle phases and combat fields are retained.
static void _dryfieldNightGasStationEngageBattle(s32 unusedArg)
{
    sceneEngageBattle(unusedArg);
}

/// Spawns and retains the cutscene's baked-track red streak.
///
/// Owns a coordinate body and plays 100 samples once. Each call replaces the
/// shared streak handle, including on allocation failure. The script must stop
/// an earlier streak first and clear this handle before automatic expiry;
/// expiry releases the task without clearing the saved handle.
static void _dryfieldNightGasStationSpawnTrackRedStreakTask(void)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_TRACK_STREAK_TASK_INDEX = 1 };

    _gDryfieldNightGasStationRedStreakTask = taskSpawnFromTable(_gDryfieldNightGasStationCutsceneTaskDescriptors, DRYFIELD_NIGHT_GAS_STATION_TRACK_STREAK_TASK_INDEX, 0, NULL);
}

/// Requests deferred teardown of the room's current red-streak effect task.
///
/// Either tracked streak task exits on its next update. Clears the room's
/// handle immediately; the task retains ownership of its coordinate body.
static void _dryfieldNightGasStationStopRedStreakTask(void)
{
    if (_gDryfieldNightGasStationRedStreakTask != NULL) {
        _gDryfieldNightGasStationRedStreakTask->state = DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_EXIT;
        _gDryfieldNightGasStationRedStreakTask        = NULL;
    }
}

/// Plays the 100-sample baked red-streak track once, then tears down its task.
///
/// Starts with state zero and `killCountdown` zero; the counter is the frame
/// index and must stay nonnegative. An external nonzero state ends playback.
/// Owns its coordinate body, released by normal task teardown. Scripts must
/// stop the tracked task before automatic expiry, which leaves its handle intact.
static void _dryfieldNightGasStationTrackRedStreakTask(Task* task)
{
    s16 frameIndex;

    if (task->state == DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_RUNNING) {
        frameIndex = task->killCountdown;
        if (frameIndex < (s32)ARRAY_SIZE(_gDryfieldNightGasStationRedStreakTrack)) {
            _dryfieldNightGasStationDrawTrackRedStreak(frameIndex);
            task->killCountdown = task->killCountdown + 1;
            return;
        }
    }
    taskKill(task);
}

/// Spawns and retains the cutscene's looping sprite-visibility controller.
///
/// Loops the 100-sample track's visibility bit. Each call replaces the flicker
/// handle, including on allocation failure; the script must stop any earlier
/// controller first. Requires the overlay and flicker sprite resources until
/// the stop request hides the sprites and releases the task.
static void _dryfieldNightGasStationSpawnSpriteFlickerTask(void)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_SPRITE_FLICKER_TASK_INDEX = 2 };

    _gDryfieldNightGasStationSpriteFlickerTask = taskSpawnFromTable(_gDryfieldNightGasStationCutsceneTaskDescriptors, DRYFIELD_NIGHT_GAS_STATION_SPRITE_FLICKER_TASK_INDEX, 0, NULL);
}

/// Requests the cutscene sprite-flicker task's exit and releases its tracked handle.
///
/// Advances the active task out of state zero; its next update hides the flicker
/// sprites and tears it down. A missing handle is a no-op.
static void _dryfieldNightGasStationStopSpriteFlicker(void)
{
    if (_gDryfieldNightGasStationSpriteFlickerTask != NULL) {
        _gDryfieldNightGasStationSpriteFlickerTask->state++;
        _gDryfieldNightGasStationSpriteFlickerTask = NULL;
    }
}

/// Loops the baked track's flicker flag, publishing only visibility changes.
///
/// Starts in state zero with killCountdown zero. The counter indexes the
/// 100-frame track; spawnArg1.value caches inverse vx bit 0 (0 hide, 1 show).
/// Increments narrow to signed halfwords before testing wrap. A nonzero state
/// hides the sprites and tears down this bodyless task; its tracked room handle
/// must already have been cleared by the stopping script callback.
static void _dryfieldNightGasStationSpriteFlickerTask(Task* task)
{
    s16 nextFrameIndex;
    s32 visible;

    if (task->state == DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_RUNNING) {
        visible = (_gDryfieldNightGasStationRedStreakTrack[task->killCountdown].vx & DRYFIELD_NIGHT_GAS_STATION_TRACK_FLICKER_HIDDEN_BIT) ^ DRYFIELD_NIGHT_GAS_STATION_TRACK_FLICKER_HIDDEN_BIT;
        if (task->spawnArg1.value != visible) {
            task->spawnArg1.value = visible;
            _dryfieldNightGasStationSetFlickerSpritesVisible((s16)task->spawnArg1.value);
        }
        nextFrameIndex      = (u16)task->killCountdown + 1;
        task->killCountdown = nextFrameIndex;
        if (nextFrameIndex >= (s32)ARRAY_SIZE(_gDryfieldNightGasStationRedStreakTrack)) {
            task->killCountdown = 0;
        }
    } else {
        _dryfieldNightGasStationSetFlickerSpritesVisible(DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_OFF);
        taskKill(task);
    }
}

/// Spawns and retains the cutscene's red streak attached to the companion.
///
/// Uses the same handle as the baked-track streak, replacing it even on spawn
/// failure. The script must stop an earlier streak first and clear this handle
/// before automatic expiry at 100 active ticks. The live companion model and
/// overlay must outlast the task's drawing and coordinate-body teardown.
static void _dryfieldNightGasStationSpawnCompanionRedStreakTask(void)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_TASK_INDEX = 3 };

    _gDryfieldNightGasStationRedStreakTask = taskSpawnFromTable(_gDryfieldNightGasStationCutsceneTaskDescriptors, DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_TASK_INDEX, 0, NULL);
}

/// Requests deferred teardown of the cutscene's tracked companion red streak.
///
/// Clears the borrowed room handle immediately. The streak task releases its
/// coordinate body on its next update; a missing handle is a no-op.
static void _dryfieldNightGasStationStopCompanionRedStreakTask(void)
{
    if (_gDryfieldNightGasStationRedStreakTask != NULL) {
        _gDryfieldNightGasStationRedStreakTask->state = DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_EXIT;
        _gDryfieldNightGasStationRedStreakTask        = NULL;
    }
}

/// Draws the companion's red streak for 100 task updates, including initialization.
///
/// Starts in state zero with `killCountdown` zero and a live companion model.
/// Seeds SDK rand() with 1 on initialization, then draws in state one; the signed
/// counter measures elapsed ticks. Other states request normal task teardown,
/// including release of this task's coordinate body. Scripts must stop the tracked
/// task before automatic expiry, which leaves its room handle intact.
static void _dryfieldNightGasStationCompanionRedStreakTask(Task* task)
{
    s16 elapsedTicks;

    switch (task->state) {
        case DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_INIT:
            // Seed once so the cutscene streak has a repeatable intensity sequence.
            srand(1);
            task->state += 1;
            /* fallthrough */
        case DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_DRAW:
            elapsedTicks = task->killCountdown;
            if (elapsedTicks < DRYFIELD_NIGHT_GAS_STATION_COMPANION_STREAK_TICKS) {
                _dryfieldNightGasStationDrawCompanionRedStreak(elapsedTicks);
                task->killCountdown = task->killCountdown + 1;
                return;
            }
        default:
            taskKill(task);
            return;
    }
}

/// Locks attachment actions and requests cancellation of all counted room effects.
///
/// Requires a live room-effect controller. Cancellation is deferred to its
/// update; this callback does not free effect tasks or their work directly.
static void _dryfieldNightGasStationCancelCutsceneEffects(void)
{
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}

/// Clears the three borrowed cutscene task handles before first-visit spawning.
///
/// Does not stop or free a task. Requires those tracked tasks to be absent.
static void _dryfieldNightGasStationClearCutsceneTaskHandles(void)
{
    _gDryfieldNightGasStationCompanionHeadAimTask = NULL;
    _gDryfieldNightGasStationRedStreakTask        = NULL;
    _gDryfieldNightGasStationSpriteFlickerTask    = NULL;
}

/// Applies the hide flag to the background groups temporarily suppressed by the cutscene.
///
/// Borrows the night gas station's writable sprite-view table. Views 4, 12,
/// 13, 14 and 16 must provide batches through indexes 6, 7, 7, 7 and 7.
/// The byte flag is 0 to show or nonzero to hide; other batches stay intact.
static inline void _dryfieldNightGasStationWriteCutsceneSpriteVisibility(SpriteView* views, u8 hidden)
{
    SpriteBatch* batches;

    batches           = views[4].batches;
    batches[6].hidden = hidden;
    batches           = views[12].batches;
    batches[6].hidden = hidden;
    batches[7].hidden = hidden;
    batches           = views[13].batches;
    batches[6].hidden = hidden;
    batches[7].hidden = hidden;
    batches           = views[14].batches;
    batches[7].hidden = hidden;
    batches           = views[16].batches;
    batches[7].hidden = hidden;
}

/// Hides or restores the background sprite groups temporarily suppressed by the cutscene.
///
/// The low byte of `hiddenArgumentWord` is 0 to show or 1 to hide; other
/// values leave the batches untouched. Requires the night gas station's
/// sprite-view tables to be loaded and selected by the current session.
static void _dryfieldNightGasStationSetCutsceneSpritesHidden(s32 hiddenArgumentWord)
{
    GameLocationKey* location;
    SpriteView*      views;
    s32              hidden;

    location = &gGameSession->location;
    views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    hidden   = hiddenArgumentWord & 0xFF;

    switch (hidden) {
        case DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_SPRITES_SHOWN:
            _dryfieldNightGasStationWriteCutsceneSpriteVisibility(views, 0);
            break;
        case DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_SPRITES_HIDDEN:
            _dryfieldNightGasStationWriteCutsceneSpriteVisibility(views, hidden);
            break;
    }
}

/// Restores the gas station's distant balcony sprites from saved section-8 damage.
///
/// Called each room-task update. Section state 0 shows and 1 hides batch 6
/// of sprite-view slots 10 and 19; other states leave them untouched. Requires
/// the current room's first sprite-variant table to contain both writable
/// batches. This changes visibility without discarding their cached packets.
static void _dryfieldNightGasStationRestoreBalconySpriteVisibility(void)
{
    enum { DRYFIELD_NIGHT_GAS_STATION_BALCONY_INTACT           = 0,
           DRYFIELD_NIGHT_GAS_STATION_BALCONY_ALTERED          = 1,
           DRYFIELD_NIGHT_GAS_STATION_BALCONY_FIRST_VIEW_SLOT  = 10,
           DRYFIELD_NIGHT_GAS_STATION_BALCONY_SECOND_VIEW_SLOT = 19,
           DRYFIELD_NIGHT_GAS_STATION_BALCONY_BATCH            = 6 };
    GameLocationKey* location     = &gGameSession->location;
    SpriteView*      views        = gSpriteAreaTables[location->stage - 1][0].areaViews[location->area - 1];
    s32              sectionState = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_8_STATE);

    switch (sectionState) {
        case DRYFIELD_NIGHT_GAS_STATION_BALCONY_INTACT:
            views[DRYFIELD_NIGHT_GAS_STATION_BALCONY_FIRST_VIEW_SLOT].batches[DRYFIELD_NIGHT_GAS_STATION_BALCONY_BATCH].hidden  = 0;
            views[DRYFIELD_NIGHT_GAS_STATION_BALCONY_SECOND_VIEW_SLOT].batches[DRYFIELD_NIGHT_GAS_STATION_BALCONY_BATCH].hidden = 0;
            break;
        case DRYFIELD_NIGHT_GAS_STATION_BALCONY_ALTERED:
            views[DRYFIELD_NIGHT_GAS_STATION_BALCONY_FIRST_VIEW_SLOT].batches[DRYFIELD_NIGHT_GAS_STATION_BALCONY_BATCH].hidden  = sectionState;
            views[DRYFIELD_NIGHT_GAS_STATION_BALCONY_SECOND_VIEW_SLOT].batches[DRYFIELD_NIGHT_GAS_STATION_BALCONY_BATCH].hidden = sectionState;
            break;
    }
}

/// Shows or hides the background flicker batches used by the timed flashes.
///
/// `visible` is 0 to hide or 1 to show; other signed-halfword values leave
/// the batches untouched. Requires the night gas station's sprite-view tables
/// to be loaded and selected by the current session.
static void _dryfieldNightGasStationSetFlickerSpritesVisible(s16 visible)
{
    /// Applies a hidden flag to the eight background batches used by the timed flashes.
    ///
    /// viewArray supplies writable views through slot 16; slots 13, 14 and 16
    /// need batches through 9, 10 and 10 respectively. Both it and hiddenValue
    /// are evaluated repeatedly and must have no side effects. hiddenValue is
    /// 0 visible or 1 hidden. batchCursor is writable SpriteBatch* scratch,
    /// left pointing at slot 16's batch array. Captures no caller identifiers.
#define DRYFIELD_NIGHT_GAS_STATION_WRITE_FLICKER_SPRITE_VISIBILITY(viewArray, batchCursor, hiddenValue)                                                 \
    do {                                                                                                                                                \
        enum { DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_VIEW  = 13,                                                                                     \
               DRYFIELD_NIGHT_GAS_STATION_FLICKER_SECOND_VIEW = 14,                                                                                     \
               DRYFIELD_NIGHT_GAS_STATION_FLICKER_THIRD_VIEW  = 16,                                                                                     \
               DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH = 8 };                                                                                    \
        (batchCursor)                                                            = (viewArray)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_VIEW].batches;  \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH].hidden     = (hiddenValue);                                                       \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH + 1].hidden = (hiddenValue);                                                       \
        (batchCursor)                                                            = (viewArray)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_SECOND_VIEW].batches; \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH].hidden     = (hiddenValue);                                                       \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH + 1].hidden = (hiddenValue);                                                       \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH + 2].hidden = (hiddenValue);                                                       \
        (batchCursor)                                                            = (viewArray)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_THIRD_VIEW].batches;  \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH].hidden     = (hiddenValue);                                                       \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH + 1].hidden = (hiddenValue);                                                       \
        (batchCursor)[DRYFIELD_NIGHT_GAS_STATION_FLICKER_FIRST_BATCH + 2].hidden = (hiddenValue);                                                       \
    } while (0)

    GameLocationKey* location = &gGameSession->location;
    SpriteView*      views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*     batches;

    switch (visible) {
        case DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_OFF:
            DRYFIELD_NIGHT_GAS_STATION_WRITE_FLICKER_SPRITE_VISIBILITY(views, batches, 1);
            break;
        case DRYFIELD_NIGHT_GAS_STATION_FLICKER_SPRITES_ON:
            DRYFIELD_NIGHT_GAS_STATION_WRITE_FLICKER_SPRITE_VISIBILITY(views, batches, 0);
            break;
    }
#undef DRYFIELD_NIGHT_GAS_STATION_WRITE_FLICKER_SPRITE_VISIBILITY
}

void dryfieldNightGasStationAmbientEffectsTask(Task* task)
{
    /// Chooses an ambient spawn position around one of the room's two scatter anchors.
    ///
    /// `effectWork` supplies writable EffectWork; `pointIndex` is 19 or 20.
    /// Both arguments are evaluated repeatedly and must be stable and side-effect-free.
    /// Captures the anchor table and this function's scatter constants. Two ordered
    /// LCG draws add X/Z jitter -255..256 parent-coordinate units, preserving anchor
    /// Y. XYZ narrow to s16 and move.pad is retained. Expands to one block.
#define DRYFIELD_NIGHT_GAS_STATION_CHOOSE_SCATTER_OFFSET(effectWork, pointIndex)                                                                                                                                       \
    {                                                                                                                                                                                                                  \
        RANDOM_LCG_ADVANCE();                                                                                                                                                                                          \
        (effectWork)->move.vx = _gDryfieldNightGasStationAmbientEffectAnchors[(pointIndex)].vx - (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_GAS_STATION_SCATTER_RANDOM_MASK) + DRYFIELD_NIGHT_GAS_STATION_SCATTER_OFFSET_MAX; \
        (effectWork)->move.vy = _gDryfieldNightGasStationAmbientEffectAnchors[(pointIndex)].vy;                                                                                                                        \
        RANDOM_LCG_ADVANCE();                                                                                                                                                                                          \
        (effectWork)->move.vz = _gDryfieldNightGasStationAmbientEffectAnchors[(pointIndex)].vz - (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_GAS_STATION_SCATTER_RANDOM_MASK) + DRYFIELD_NIGHT_GAS_STATION_SCATTER_OFFSET_MAX; \
    }

    enum {
        DRYFIELD_NIGHT_GAS_STATION_CAPSULE_ANCHOR_FIRST       = 0,
        DRYFIELD_NIGHT_GAS_STATION_CAPSULE_ANCHOR_END         = 10,
        DRYFIELD_NIGHT_GAS_STATION_FLARE_ANCHOR_FIRST         = 10,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_ANCHOR_FIRST       = 19,
        DRYFIELD_NIGHT_GAS_STATION_EXTRA_CAPSULE_VIEWS        = (1 << 2) | (1 << 3) | (1 << 13) | (1 << 14),
        DRYFIELD_NIGHT_GAS_STATION_CAPSULE_RADIUS_SCALE       = 0x180,
        DRYFIELD_NIGHT_GAS_STATION_CAPSULE_DIM_RGB_NIBBLES    = 0x222,
        DRYFIELD_NIGHT_GAS_STATION_CAPSULE_BRIGHT_RGB_NIBBLES = 0x444,
        DRYFIELD_NIGHT_GAS_STATION_FLARE_TEXTURE_INDEX        = 0,
        DRYFIELD_NIGHT_GAS_STATION_FLARE_RADIUS_SCALE         = 0x380,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_RANDOM_MASK        = 0x1FF,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_OFFSET_MAX         = 0x100,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_CHOICE_COUNT       = 3,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_PUFF               = 0,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_FIRE               = 1,
        // Rising additive puff, size 0x300; fire uses the same size.
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_PUFF_ARG  = 0x10300,
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_FIRE_SIZE = 0x300,
        // Subtractive smoke, size 0x500, three ticks/cell and default rise step 1.
        DRYFIELD_NIGHT_GAS_STATION_SCATTER_SMOKE_ARG = 0xC0013500
    };
    EffectWork* work;
    GfxCoord*   spawnCoord;
    s32         viewMask;
    s32         anchorIndex;

    // Reuse scale as the initial-progress latch, angle as a choice and move as a spawn offset.
    work       = task->spawnArg2.pointer;
    spawnCoord = task->extra.coordBody->coord;
    viewMask   = 1 << viewGetMappedIndex();
    if (task->state == DRYFIELD_NIGHT_GAS_STATION_CUTSCENE_TASK_RUNNING) {
        gRoomEffectFlashId               = EFFECT_DRYFIELD_NIGHT_GAS_STATION_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_DRYFIELD_NIGHT_GAS_STATION_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_DRYFIELD_NIGHT_GAS_STATION_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    }
    // Capsule pairs and individual flares use the current mapped view's bit.
    for (anchorIndex = DRYFIELD_NIGHT_GAS_STATION_CAPSULE_ANCHOR_FIRST; anchorIndex < DRYFIELD_NIGHT_GAS_STATION_CAPSULE_ANCHOR_END; anchorIndex += 2) {
        if (viewMask & _gDryfieldNightGasStationGlowViewMasks[anchorIndex]) {
            _glowDrawCapsule(&_gDryfieldNightGasStationAmbientEffectAnchors[anchorIndex], DRYFIELD_NIGHT_GAS_STATION_CAPSULE_RADIUS_SCALE, DRYFIELD_NIGHT_GAS_STATION_CAPSULE_DIM_RGB_NIBBLES);
        }
    }
    if (viewMask & DRYFIELD_NIGHT_GAS_STATION_EXTRA_CAPSULE_VIEWS) {
        _glowDrawCapsule(_gDryfieldNightGasStationBrightCapsuleEndpoints[0], DRYFIELD_NIGHT_GAS_STATION_CAPSULE_RADIUS_SCALE, DRYFIELD_NIGHT_GAS_STATION_CAPSULE_BRIGHT_RGB_NIBBLES);
        _glowDrawCapsule(_gDryfieldNightGasStationBrightCapsuleEndpoints[1], DRYFIELD_NIGHT_GAS_STATION_CAPSULE_RADIUS_SCALE, DRYFIELD_NIGHT_GAS_STATION_CAPSULE_BRIGHT_RGB_NIBBLES);
    }
    for (anchorIndex = DRYFIELD_NIGHT_GAS_STATION_FLARE_ANCHOR_FIRST; anchorIndex < (s32)ARRAY_SIZE(_gDryfieldNightGasStationGlowViewMasks); anchorIndex++) {
        if (viewMask & _gDryfieldNightGasStationGlowViewMasks[anchorIndex]) {
            _glowDrawFlare(&_gDryfieldNightGasStationAmbientEffectAnchors[anchorIndex], DRYFIELD_NIGHT_GAS_STATION_FLARE_TEXTURE_INDEX, DRYFIELD_NIGHT_GAS_STATION_FLARE_RADIUS_SCALE);
        }
    }
    // Only initial progress allows fire; later visits need this task's earlier latch to emit smoke.
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) == DRYFIELD_NIGHT_GAS_STATION_PROGRESS_INITIAL) {
        work->scale = 1;
        if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
            for (anchorIndex = DRYFIELD_NIGHT_GAS_STATION_SCATTER_ANCHOR_FIRST; anchorIndex < (s32)ARRAY_SIZE(_gDryfieldNightGasStationAmbientEffectAnchors); anchorIndex++) {
                RANDOM_LCG_ADVANCE();
                work->angle = RANDOM_LCG_DRAW() % DRYFIELD_NIGHT_GAS_STATION_SCATTER_CHOICE_COUNT;
                DRYFIELD_NIGHT_GAS_STATION_CHOOSE_SCATTER_OFFSET(work, anchorIndex);
                if (work->angle == DRYFIELD_NIGHT_GAS_STATION_SCATTER_PUFF) {
                    effectSpawn(EFFECT_ADDITIVE_PUFF, spawnCoord, DRYFIELD_NIGHT_GAS_STATION_SCATTER_PUFF_ARG, &work->move);
                } else if (work->angle == DRYFIELD_NIGHT_GAS_STATION_SCATTER_FIRE) {
                    effectSpawn(EFFECT_FIRE_BURST, spawnCoord, DRYFIELD_NIGHT_GAS_STATION_SCATTER_FIRE_SIZE, &work->move);
                } else {
                    RANDOM_LCG_ADVANCE();
                    if ((u16)(RANDOM_LCG_DRAW() % DRYFIELD_NIGHT_GAS_STATION_SCATTER_CHOICE_COUNT) == 0) {
                        effectSpawn(EFFECT_SMOKE_PUFF, spawnCoord, DRYFIELD_NIGHT_GAS_STATION_SCATTER_SMOKE_ARG, &work->move);
                    }
                }
            }
        }
    } else if (work->scale != 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        for (anchorIndex = DRYFIELD_NIGHT_GAS_STATION_SCATTER_ANCHOR_FIRST; anchorIndex < (s32)ARRAY_SIZE(_gDryfieldNightGasStationAmbientEffectAnchors); anchorIndex++) {
            RANDOM_LCG_ADVANCE();
            if ((u16)(RANDOM_LCG_DRAW() % DRYFIELD_NIGHT_GAS_STATION_SCATTER_CHOICE_COUNT) == 0) {
                DRYFIELD_NIGHT_GAS_STATION_CHOOSE_SCATTER_OFFSET(work, anchorIndex);
                effectSpawn(EFFECT_SMOKE_PUFF, spawnCoord, DRYFIELD_NIGHT_GAS_STATION_SCATTER_SMOKE_ARG, &work->move);
            }
        }
    }
#undef DRYFIELD_NIGHT_GAS_STATION_CHOOSE_SCATTER_OFFSET
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void dryfieldNightGasStationRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void dryfieldNightGasStationRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void dryfieldNightGasStationRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
