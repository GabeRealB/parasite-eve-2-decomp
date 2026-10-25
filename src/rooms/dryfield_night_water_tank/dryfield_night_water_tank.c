#include "rooms/dryfield_night_water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor_146300.h"
#include "actors/actor_01500.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"
#include "../../shared/water_tank.h"

/// Main-executable halfword the second state waits on before it may advance.

static void _dryfieldNightWaterTankRestoreTankCollision(s32 offsetTank);

static WorldCollisionGrid     _gDryfieldNightWaterTankCollisionGrid;
static WorldCollisionOccluder _gDryfieldNightWaterTankSightOccluders[2];
static WorldCollisionTrigger  _gDryfieldNightWaterTankViewBoundaryTriggers[4];
static WorldCollisionTrigger  _gDryfieldNightWaterTankActionTriggers[8];
static WorldCoordRoomLights   _gDryfieldNightWaterTankRoomLights;
static TmdSource              _gDryfieldNightWaterTankModel00FF8;

static s32  _dryfieldNightWaterTankRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldNightWaterTankResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldNightWaterTankCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32  _dryfieldNightWaterTankRoomActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldNightWaterTankBattleAftermathTask(Task* task);

enum {
    DRYFIELD_NIGHT_WATER_TANK_PLAYER_IDLE_ANIMATION = 1,
    DRYFIELD_NIGHT_WATER_TANK_RELOCATION_SOUND      = 0x52150001,
    DRYFIELD_NIGHT_WATER_TANK_STAGE                 = 3,
    DRYFIELD_NIGHT_WATER_TANK_AREA                  = 21,
    DRYFIELD_NIGHT_WATER_TANK_HANDOVER_LAYOUT       = 11,
};

/// Player placement reached by the relocation to logical view 4.
///
/// Position uses room/world game coordinates and Euler angles use 4096 per turn.
/// The script borrows this record until its placement message returns.
static ActorTransform _gDryfieldNightWaterTankView4PlayerTransform = {
    .pos = { 820, -0x4010, 884, 0 },
    .rot = { 0, 2560, 0, 0 },
};

/// Player placement reached by the relocation to logical view 3.
///
/// Position uses room/world game coordinates and Euler angles use 4096 per turn.
/// The script borrows this record until its placement message returns.
static ActorTransform _gDryfieldNightWaterTankView3PlayerTransform = {
    .pos = { 1868, -0x2EE0, 1740, 0 },
    .rot = { 0, 512, 0, 0 },
};

/// Equipped-bank idle request that holds player control during either relocation.
///
/// The event interpreter copies the request and replaces source.index with the
/// current weapon bank. Playback resets the clip and disables world collision;
/// blendFrames is ignored. The room overlay stays loaded through dispatch.
static AnimationPlayRequest _gDryfieldNightWaterTankPlayerIdleAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_WATER_TANK_PLAYER_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Fades out, relocates the player to logical view 4 and restores ordinary control.
///
/// Room action 3 starts these eleven commands, including END. The 32-frame
/// fade brackets placement and a stage-relative sound cue; waits retain their
/// interpreter-update counts. Commands and payloads borrow this loaded overlay.
static EvsCommand _gDryfieldNightWaterTankRelocateToView4Script[11] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightWaterTankPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightWaterTankView4PlayerTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_STAGE_SOUND, { .value = DRYFIELD_NIGHT_WATER_TANK_RELOCATION_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fades out, relocates the player to logical view 3 and restores ordinary control.
///
/// Room action 4 starts these eleven commands, including END. The 32-frame
/// fade brackets placement and a stage-relative sound cue; waits retain their
/// interpreter-update counts. Commands and payloads borrow this loaded overlay.
static EvsCommand _gDryfieldNightWaterTankRelocateToView3Script[11] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightWaterTankPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightWaterTankView3PlayerTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_STAGE_SOUND, { .value = DRYFIELD_NIGHT_WATER_TANK_RELOCATION_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room receivers for transition queries, key-item use, actions and CAP commands.
///
/// Four entries precede TASK_MESSAGE_TABLE_END. The registered room task borrows
/// the table and its callbacks for its lifetime in the loaded room overlay.
static TaskMessageEntry _gDryfieldNightWaterTankRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightWaterTankResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightWaterTankRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightWaterTankRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightWaterTankCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless battle-aftermath controller recipe for placement variant 10.
///
/// Entry 0 runs at priority `TASK_PRIORITY_SCENE`; TASK_DESC_END follows. Spawning consumes the
/// descriptor synchronously, and the callback stays loaded through teardown.
static TaskDesc _gDryfieldNightWaterTankBattleAftermathTasks[2] = {
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, .callback = _dryfieldNightWaterTankBattleAftermathTask, .data = { .value = 0 } },
    { .header = { .fields = { TASK_DESC_END, 0 } }, .callback = NULL, .data = { .model = NULL } },
};

/// Two room-space obstacle normals, with 4096 for unit length.
static SVECTOR _gDryfieldNightWaterTankObstacleCollisionGridNormals[2] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_normals.inc"
};

/// Six room-space vertices copied into the live grid's obstacle prefix.
static SVECTOR _gDryfieldNightWaterTankObstacleCollisionGridVerts[6] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_verts.inc"
};

/// Two surface-class-0 quads indexing the obstacle's six vertices and two normals.
static WorldCollisionGridFace _gDryfieldNightWaterTankObstacleCollisionGridFaces[2] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_faces.inc"
};

/// One face-index list ending in WORLD_COLLISION_GRID_CELL_END, followed by a stored zero.
static s16 _gDryfieldNightWaterTankObstacleCollisionGridCells[4] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_cells.inc"
};

// The generated table uses element offsets into this template's cell list.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightWaterTankObstacleCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..3 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_WATER_TANK_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightWaterTankObstacleCollisionGridCells[(halfwordOffset)])
/// The template's single cell points at the list containing both obstacle faces.
static s16* _gDryfieldNightWaterTankObstacleCollisionGridTable[1] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_table.inc"
};
#undef DRYFIELD_NIGHT_WATER_TANK_CELL_FACE_LIST_AT_OFFSET

/// Collision template restoring the tank obstacle in placement variants 10 and 11.
///
/// Its two faces/normals and six vertex XYZs replace the live room grid's leading
/// geometry. The one-cell index is retained with the asset; this template is
/// never activated or bound to a view. A nonzero restore argument offsets Z by
/// -200 game units. All storage belongs to this room overlay.
static WorldCollisionGrid _gDryfieldNightWaterTankObstacleCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightWaterTankObstacleCollisionGridNormals,
    .vertices    = _gDryfieldNightWaterTankObstacleCollisionGridVerts,
    .faces       = _gDryfieldNightWaterTankObstacleCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightWaterTankObstacleCollisionGridTable,
    .xBias       = -1016,
    .zBias       = 2444,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightWaterTankObstacleCollisionGridFaces),
};

static TmdBone _gDryfieldNightWaterTankModel00FF8Skeleton[1] = {
#include "assets/dryfield_night_water_tank_model_00FF8_skeleton.inc"
};

static u32 _gDryfieldNightWaterTankModel00FF8PartVerts[1] = {
#include "assets/dryfield_night_water_tank_model_00FF8_partVerts.inc"
};

static SVECTOR _gDryfieldNightWaterTankModel00FF8Verts[84] = {
#include "assets/dryfield_night_water_tank_model_00FF8_verts.inc"
};

static SVECTOR _gDryfieldNightWaterTankModel00FF8Normals[72] = {
#include "assets/dryfield_night_water_tank_model_00FF8_normals.inc"
};

static u32 _gDryfieldNightWaterTankModel00FF8Stream[531] = {
#include "assets/dryfield_night_water_tank_model_00FF8_stream.inc"
};

static TmdSource _gDryfieldNightWaterTankModel00FF8 = {
    0,
    3768,
    0,
    1,
    _gDryfieldNightWaterTankModel00FF8PartVerts,
    _gDryfieldNightWaterTankModel00FF8Verts,
    _gDryfieldNightWaterTankModel00FF8Normals,
    _gDryfieldNightWaterTankModel00FF8Skeleton,
    _gDryfieldNightWaterTankModel00FF8Stream,
};

/// Tank sway model recipe spawned on every room entry, followed by TASK_DESC_END.
///
/// Entry 0 attaches the one-part TMD at priority 192. Its borrowed geometry and
/// callback must stay loaded for the spawned task's lifetime.
static TaskDesc _gDryfieldNightWaterTankSwayTasks[2] = {
    { .header = { .fields = { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, .callback = waterTankSwayTask, .data = { .model = &_gDryfieldNightWaterTankModel00FF8 } },
    { .header = { .fields = { TASK_DESC_END, 0 } }, .callback = NULL, .data = { .model = NULL } },
};

s32 gWaterTankYaw = 0;

s32 gWaterTankYawSpeed = 0;

s32 gWaterTankYawAcceleration = 0;

s32 gWaterTankYawTargetAcceleration = 0;

WorldCoordRoomLighting gDryfieldNightWaterTankRoomLighting[1] = {
    { &_gDryfieldNightWaterTankRoomLights, NULL },
};

WorldCollisionRoomResources gDryfieldNightWaterTankCollisionRooms[1] = {
    { .grid                 = &_gDryfieldNightWaterTankCollisionGrid,
      .viewBoundaryTriggers = _gDryfieldNightWaterTankViewBoundaryTriggers,
      .actionTriggers       = _gDryfieldNightWaterTankActionTriggers,
      .occluders            = _gDryfieldNightWaterTankSightOccluders },
};

/// Maps logical views 1..10 to the six camera/image/sprite entries.
///
/// Indexed by logical view minus one; views 6..10 share entry 6. The final two
/// stored zeros are outside the search count, not a terminator or valid mapped indices.
static u8 _gDryfieldNightWaterTankLogicalViewMap[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    6,
    6,
    6,
    6,
    0,
    0,
};

u8* gDryfieldNightWaterTankViewMaps[1] = {
    _gDryfieldNightWaterTankLogicalViewMap,
};

ViewCount gDryfieldNightWaterTankViewCounts[1] = { 10 };

DirectionWarpEntry gDryfieldNightWaterTankWarpEntries[2] = {
    { { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space collision normals, with ONE (4096) for unit length.
static SVECTOR _gDryfieldNightWaterTankCollisionGridNormals[16] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_normals.inc"
};

/// Room-space collision vertices; the leading six are reserved for the tank obstacle.
static SVECTOR _gDryfieldNightWaterTankCollisionGridVerts[78] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_verts.inc"
};

/// Indexed room collision quads; the leading two are rebuilt from the tank template.
static WorldCollisionGridFace _gDryfieldNightWaterTankCollisionGridFaces[38] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_faces.inc"
};

/// Signed face-index lists ending in WORLD_COLLISION_GRID_CELL_END for each cell.
///
/// Two stored zero halfwords after the second and fourth terminators are not list entries.
static s16 _gDryfieldNightWaterTankCollisionGridCells[126] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_cells.inc"
};

// The generated table uses element offsets into the room grid's face-index lists.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightWaterTankCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..125 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_WATER_TANK_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightWaterTankCollisionGridCells[(halfwordOffset)])
/// Four cell lists in X-major order, indexed as x * 2 + z.
static s16* _gDryfieldNightWaterTankCollisionGridTable[4] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_table.inc"
};
#undef DRYFIELD_NIGHT_WATER_TANK_CELL_FACE_LIST_AT_OFFSET

/// Live room collision mesh, including the tank's reserved geometry prefix.
///
/// The 2x2 grid starts at (-3500, -3300) in room X/Z and uses 4000-unit cells.
/// Binding supplies the view coordinate; room entry in variants 10/11 restores
/// the first two faces/normals and six vertices without rebuilding the cell lists.
static WorldCollisionGrid _gDryfieldNightWaterTankCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightWaterTankCollisionGridNormals,
    .vertices    = _gDryfieldNightWaterTankCollisionGridVerts,
    .faces       = _gDryfieldNightWaterTankCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightWaterTankCollisionGridTable,
    .xBias       = 3500,
    .zBias       = 3300,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightWaterTankCollisionGridFaces),
};

ViewCamera gDryfieldNightWaterTankViewCameras[6] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 1429, 0, -3838 }, { -2523, 3086, -939 }, { 2892, 2692, 1077 } }, { 5050, 0x3E4E, 1880 } }, 257 },
    { { { { -853, 0, 4006 }, { 1212, 3903, 258 }, { -3818, 1239, -813 } }, { -5920, 0x37FA, -1350 } }, 257 },
    { { { { -1180, 0, 3922 }, { 173, 4092, 52 }, { -3918, 180, -1179 } }, { -4220, 0x42EA, -1350 } }, 230 },
    { { { { 2889, 0, 2902 }, { 1979, 2995, -1970 }, { -2123, 2793, 2113 } }, { -2370, 0x3719, 3415 } }, 329 },
    { { { { -2497, 0, 3246 }, { 3119, 1132, 2399 }, { -897, 3936, -690 } }, { 570, 0x42AE, 1550 } }, 380 },
};

/// Mapped view 1's image-strip selector and terminal batch, with no source sprites.
static SpriteBatch _gDryfieldNightWaterTankView1SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 2's 46 backdrop rectangles, in screen pixels with unscaled sorting depths.
static SpriteSource _gDryfieldNightWaterTankView2SpriteSources[46] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -104, -120, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -96, -120, 1125, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -88, -120, 1112, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -48, -120, 937, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -32, -120, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -16, -120, 912, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -80, -120, 1052, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -72, -120, 1012, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -64, -120, 987, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -56, -120, 975, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 0, -120, 912, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 16, -120, 925, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 32, -120, 937, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 48, -120, 975, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 56, -120, 987, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 64, -120, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 72, -120, 1062, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 80, -120, 1112, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 88, -120, 1127, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 96, -120, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, -40, 1400, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, -40, 1350, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -64, -40, 1287, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -56, -40, 1237, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 48, -40, 1237, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 56, -40, 1287, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 64, -40, 1350, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 72, -40, 1437, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -144, 8, 1000, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -128, 8, 1000, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -112, 16, 1000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, 16, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -80, 8, 1000, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -64, 16, 1000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -48, 16, 1000, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -40, 40, 875, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -24, 48, 875, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -8, 64, 875, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 64, 875, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 32, 40, 875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 48, 32, 875, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 64, 24, 875, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 80, 16, 875, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 96, 16, 875, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 112, 8, 875, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 128, 0, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

// Number of sources in mapped view 2's first nonempty batch.
enum { DRYFIELD_NIGHT_WATER_TANK_VIEW_2_FIRST_BATCH_COUNT = 28 };

/// Mapped view 2's image-strip selector, two source ranges and terminal batch.
///
/// The ranges cover sources 0..27 and 28..45. The first range retains serialized
/// bytes {1, 0} whose role is unproven; both ranges participate in cached drawing.
static SpriteBatch _gDryfieldNightWaterTankView2SpriteBatches[4] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = DRYFIELD_NIGHT_WATER_TANK_VIEW_2_FIRST_BATCH_COUNT, .field_6 = { 1, 0 } },
    { .firstSprite = DRYFIELD_NIGHT_WATER_TANK_VIEW_2_FIRST_BATCH_COUNT, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterTankView2SpriteSources) - DRYFIELD_NIGHT_WATER_TANK_VIEW_2_FIRST_BATCH_COUNT },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 3's 69 backdrop rectangles from two texture pages, with sorting depths.
static SpriteSource _gDryfieldNightWaterTankView3SpriteSources[69] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -96, -120, 1125, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -88, -120, 1164, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -80, -120, 1112, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -72, -120, 1077, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 48, -120, 1062, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 56, -120, 1000, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 64, -120, 1125, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 72, -120, 1175, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 56 } }, 80, -120, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -88, -56, 1500, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -80, -56, 1500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -72, -56, 1375, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -64, -120, 1063, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -48, -120, 1036, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -32, -120, 1020, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -16, -120, 1013, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 0, -120, 1018, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 16, -120, 1030, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 32, -120, 1051, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -64, -32, 1375, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -48, -32, 1250, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -32, -32, 1250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -16, -32, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 88 } }, 0, -32, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 16, -32, 1250, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 32, -32, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 48, -56, 1375, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, 56, -56, 1375, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, 64, -56, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 72, -56, 1375, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 48, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -152, -24, 750, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -144, -24, 750, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 56 } }, -136, 8, 750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -128, 8, 750, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, 8, 750, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -128, 40, 750, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -96, 16, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, 64, 750, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, 48, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -48, 16, 750, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, 16, 750, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, 16, 750, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 16, 750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -72, 48, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -48, 48, 750, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -32, 56, 750, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -16, 16, 750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -8, 16, 750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 0, 24, 750, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 16, 24, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, 24, 750, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 56, 750, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 16, 56, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 32, 56, 750, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, 16, 750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, 16, 750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, 32, 750, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 56, 48, 750, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 64, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 80, 8, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 96, 0, 750, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 112, 0, 750, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 136, -8, 750, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 80, 40, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 96, 32, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 112, 16, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 128, 24, 750, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 144, 16, 750, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

// Number of sources in mapped view 3's first nonempty batch.
enum { DRYFIELD_NIGHT_WATER_TANK_VIEW_3_FIRST_BATCH_COUNT = 30 };

/// Mapped view 3's image-strip selector, two source ranges and terminal batch.
///
/// The ranges cover sources 0..29 and 30..68. The first range retains serialized
/// bytes {1, 0} whose role is unproven; both ranges participate in cached drawing.
static SpriteBatch _gDryfieldNightWaterTankView3SpriteBatches[4] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = DRYFIELD_NIGHT_WATER_TANK_VIEW_3_FIRST_BATCH_COUNT, .field_6 = { 1, 0 } },
    { .firstSprite = DRYFIELD_NIGHT_WATER_TANK_VIEW_3_FIRST_BATCH_COUNT, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterTankView3SpriteSources) - DRYFIELD_NIGHT_WATER_TANK_VIEW_3_FIRST_BATCH_COUNT },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 4's nine backdrop rectangles, with sorting depth 625 for every source.
static SpriteSource _gDryfieldNightWaterTankView4SpriteSources[9] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 96, 625, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 32, 8, 625, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, 48, 8, 625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 56, 24, 625, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, 24, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 40, 80, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 64, 56, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 88, 56, 625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, 72, 625, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

/// Mapped view 4's image-strip selector, complete source range and terminal batch.
static SpriteBatch _gDryfieldNightWaterTankView4SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterTankView4SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 5's 24 backdrop rectangles, with sorting depth 250 for every source.
static SpriteSource _gDryfieldNightWaterTankView5SpriteSources[24] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -160, -48, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -144, -32, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -128, -16, 250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -112, 0, 250, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -80, 0, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -48, 0, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -16, 0, 250, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -112, 40, 250, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -96, 56, 250, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -80, 64, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -64, 64, 250, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -32, 64, 250, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 64, 250, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 16, 0, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 16, 64, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 32, 0, 250, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 48, 0, 250, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 80, 0, 250, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 112, 0, 250, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 144, 0, 250, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 32, 64, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 64, 64, 250, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 96, 64, 250, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 128, 64, 250, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
};

/// Mapped view 5's image-strip selector, complete source range and terminal batch.
static SpriteBatch _gDryfieldNightWaterTankView5SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterTankView5SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 6's image-strip selector and terminal batch, shared by logical views 6..10.
static SpriteBatch _gDryfieldNightWaterTankView6SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldNightWaterTankSpriteViews[6] = {
    { { .empty = _gDryfieldNightWaterTankView1SpriteBatches }, _gDryfieldNightWaterTankView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWaterTankView2SpriteSources }, _gDryfieldNightWaterTankView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWaterTankView3SpriteSources }, _gDryfieldNightWaterTankView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWaterTankView4SpriteSources }, _gDryfieldNightWaterTankView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWaterTankView5SpriteSources }, _gDryfieldNightWaterTankView5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightWaterTankView6SpriteBatches }, _gDryfieldNightWaterTankView6SpriteBatches, NULL },
};

/// Four directional model lights enabled in every logical view.
///
/// Translation supplies each direction in room axes; RGB uses signed Q12 levels.
/// Coordinate binding, composition and lighting queries require writable storage.
static WorldCoordLight _gDryfieldNightWaterTankDirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 100, 100, 100 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 100, 100, 100 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 100, 100, 100 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -5000, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 413, 823, 823 }, { 0, 0 } },
};

/// The room's directional-light collection, with no point or cone lights.
static WorldCoordRoomLights _gDryfieldNightWaterTankRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gDryfieldNightWaterTankDirectionalLights),
    .directionalLights     = _gDryfieldNightWaterTankDirectionalLights,
    .pointLightCount       = 0,
    .pointLights           = NULL,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Four directed view boundaries switching between logical views 2 and 3.
///
/// Each pair covers one room crossing in opposite directions. Positions/radii
/// use room game units, normals use 4096 per unit, and LAST marks the fourth
/// included record. Room setup mutates links, coordinates, flags and hit latches.
static WorldCollisionTrigger _gDryfieldNightWaterTankViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { -864, -0x32D0, 2464, 0 }, { { 0, -1968, -1024, 0 }, { 0, -1968, 1024, 0 }, { 0, 1968, -1024, 0 }, { 0, 1968, 1024, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2217, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -576, -0x3310, 2464, 0 }, { { 0, -1904, 1024, 0 }, { 0, -1904, -1024, 0 }, { 0, 1904, 1024, 0 }, { 0, 1904, -1024, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 768, -0x32D0, -2368, 0 }, { { 0, -1936, 1024, 0 }, { 0, -1936, -1024, 0 }, { 0, 1936, 1024, 0 }, { 0, 1936, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2187, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 544, -0x3330, -2336, 0 }, { { 0, -1904, -1024, 0 }, { 0, -1904, 1024, 0 }, { 0, 1904, -1024, 0 }, { 0, 1904, 1024, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

// Exit parameter1 packs this area's departure slot 1 and the tower's arrival slot 3.
enum {
    DRYFIELD_NIGHT_WATER_TANK_TOWER_EXIT_AREA           = 20,
    DRYFIELD_NIGHT_WATER_TANK_TOWER_EXIT_ENDPOINTS      = 0x13,
    DRYFIELD_NIGHT_WATER_TANK_RELOCATE_TO_VIEW_4_ACTION = 3,
    DRYFIELD_NIGHT_WATER_TANK_RELOCATE_TO_VIEW_3_ACTION = 4,
    DRYFIELD_NIGHT_WATER_TANK_START_HANDOVER_ACTION     = 5,
};

/// Eight room interactions: the tower exit, CAP commands and room action regions.
///
/// The exit packs departure slot 1 and tower arrival slot 3. CAP slots 12/14
/// and room actions 3/4/5 select dialogue, the two relocations and handover;
/// automatic actions 1/2 are ignored by this room's handler. Positions/radii
/// use room game units and normals use 4096 per unit. LAST includes record 7;
/// links, coordinates, flags and hit latches remain writable while active.
static WorldCollisionTrigger _gDryfieldNightWaterTankActionTriggers[8] = {
    { NULL, NULL, NULL, { -2449, -0x2F60, 640, 0 }, { { -728, 0, -256, 0 }, { 336, 0, -584, 0 }, { -624, 0, 680, 0 }, { 1016, 0, 160, 0 } }, { 0, 4096, 0, 0 }, { 401, 0, 4076, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, DRYFIELD_NIGHT_WATER_TANK_TOWER_EXIT_AREA, DRYFIELD_NIGHT_WATER_TANK_TOWER_EXIT_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2064, -0x2F20, -832, 0 }, { { -1448, 0, -160, 0 }, { 1088, 0, -680, 0 }, { -1376, 0, 776, 0 }, { 1736, 0, 64, 0 } }, { 0, 4105, 0, 0 }, { 401, 0, 4076, 0 }, 1736, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1620, -0x4040, -1538, 0 }, { { 985, 0, -789, 0 }, { 1431, 0, 313, 0 }, { -780, 0, 951, 0 }, { 316, 0, 1350, 0 } }, { 0, 4107, 0, 0 }, { 401, 0, 4076, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, CAP_EVENT_PAUSE_ACTORS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1488, -0x2F23, 16, 0 }, { { 231, 0, 659, 0 }, { 229, 0, -659, 0 }, { 1395, 0, 739, 0 }, { 1411, 0, -737, 0 } }, { 0, 4103, 0, 0 }, { 4094, 0, 0, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -656, -0x2F21, 2160, 0 }, { { -841, 0, 1091, 0 }, { -619, 0, -1091, 0 }, { 611, 0, 1171, 0 }, { 851, 0, -1169, 0 } }, { 0, 4100, 0, 0 }, { 4094, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1471, -0x2F40, 1471, 0 }, { { -485, 0, 321, 0 }, { 259, 0, -507, 0 }, { 13, 0, 1473, 0 }, { 1559, 0, -134, 0 } }, { 0, 4100, 0, 0 }, { 2750, 0, 3035, 0 }, 1562, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_NIGHT_WATER_TANK_RELOCATE_TO_VIEW_4_ACTION, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1039, -0x4040, 1063, 0 }, { { 756, 0, -376, 0 }, { -379, 0, 767, 0 }, { 543, 0, -928, 0 }, { -920, 0, 537, 0 } }, { 0, 4102, 0, 0 }, { -3166, 0, -2599, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_NIGHT_WATER_TANK_RELOCATE_TO_VIEW_3_ACTION, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 576, -0x2F20, -1984, 0 }, { { 445, 0, 788, 0 }, { -788, 0, -221, 0 }, { 1897, 0, 504, 0 }, { -50, 0, -1395, 0 } }, { 0, 4102, 0, 0 }, { 4094, 0, 0, 0 }, 1962, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_NIGHT_WATER_TANK_START_HANDOVER_ACTION, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Empty resource list for placement variant 1, containing only AREA_PLACEMENT_END.
static AreaResource _gDryfieldNightWaterTankDefaultResources[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor resources for placement variant 10's handover figure and Mind Sucklers.
///
/// Placement entry 143 selects file 146300 and figure descriptor 0; entry 15
/// selects base file 201500 and its Mind Suckler descriptor. Additional files
/// follow each placement's low file ID. AREA_PLACEMENT_END follows both entries.
/// Loading and spawning borrow this overlay and the loaded actor descriptors.
static AreaResource _gDryfieldNightWaterTankBattleResources[3] = {
    { .entryId = 143, .fileNumber = 463, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor146300FigureTasks },
    { .entryId = 15, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor resource for placement variant 11's handover figure, then AREA_PLACEMENT_END.
///
/// Entry 143 selects file 146300 and figure descriptor 0. Loading and spawning
/// borrow this overlay and the loaded actor's descriptor table.
static AreaResource _gDryfieldNightWaterTankHandoverResources[2] = {
    { .entryId = 143, .fileNumber = 463, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor146300FigureTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightWaterTankAreaVariants[DRYFIELD_NIGHT_WATER_TANK_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullWaterTankVariant1Placements, .resources = _gDryfieldNightWaterTankDefaultResources },
    [10]                   = { .placements = gMapDryfieldFullWaterTankVariant10Placements, .resources = _gDryfieldNightWaterTankBattleResources },
    [11]                   = { .placements = gMapDryfieldFullWaterTankVariant11Placements, .resources = _gDryfieldNightWaterTankHandoverResources },
};

/// Two perpendicular room-space quads blocking line of sight through the tank area.
///
/// Positions/radii use game units and normals use 4096 per unit. LAST includes
/// the second quad. Room setup mutates their links and enable flags; bit 0's
/// role is unproven and retained. Storage stays live until both are unlinked.
static WorldCollisionOccluder _gDryfieldNightWaterTankSightOccluders[2] = {
    { NULL, NULL, { -256, -0x35A0, 0, 0 }, { { -2080, 2560, 0, 0 }, { 2080, 2560, 0, 0 }, { -2080, -2560, 0, 0 }, { 2080, -2560, 0, 0 } }, { 0, 0, 4098, 0 }, 3298, 1, 0 },
    { NULL, NULL, { 0, -0x35A0, 0, 0 }, { { 0, 2560, 1840, 0 }, { 0, 2560, -1840, 0 }, { 0, -2560, 1840, 0 }, { 0, -2560, -1840, 0 } }, { 4113, 0, 0, 0 }, 3145, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

// Cue-2 base requests in the loaded type-1 bank; cue 1 adds one.
enum {
    DRYFIELD_NIGHT_WATER_TANK_CLASS_3_WALK        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_WATER_TANK_CLASS_3_RUN         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_WATER_TANK_CLASS_4_WALK        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x45,
    DRYFIELD_NIGHT_WATER_TANK_CLASS_4_RUN         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x47,
    DRYFIELD_NIGHT_WATER_TANK_CLASS_5_WALK        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    DRYFIELD_NIGHT_WATER_TANK_CLASS_5_RUN         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    DRYFIELD_NIGHT_WATER_TANK_CLASS_5_STAIR_CLIMB = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

/// Surface class 3's walk/run/stair-climb footstep bases in the loaded sound bank.
///
/// Walk and stair climb share entry 0x35; running uses 0x37. Cue 1 adds one
/// to the selected base, and companion playback adds 100.
static WorldCollisionFootstepSounds _gDryfieldNightWaterTankClass3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_WATER_TANK_CLASS_3_WALK,
    .run          = DRYFIELD_NIGHT_WATER_TANK_CLASS_3_RUN,
    .scriptedJump = DRYFIELD_NIGHT_WATER_TANK_CLASS_3_WALK,
};

/// Surface class 4's walk/run/stair-climb footstep bases in the loaded sound bank.
///
/// Walk and stair climb share entry 0x45; running uses 0x47. Cue 1 adds one
/// to the selected base, and companion playback adds 100.
static WorldCollisionFootstepSounds _gDryfieldNightWaterTankClass4FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_WATER_TANK_CLASS_4_WALK,
    .run          = DRYFIELD_NIGHT_WATER_TANK_CLASS_4_RUN,
    .scriptedJump = DRYFIELD_NIGHT_WATER_TANK_CLASS_4_WALK,
};

/// Surface class 5's walk/run/stair-climb footstep bases in the loaded sound bank.
///
/// Entries 0x15, 0x17 and 0x19 respectively select cue 2; cue 1 adds one and
/// companion playback adds 100. A stored request does not guarantee a bank entry exists.
static WorldCollisionFootstepSounds _gDryfieldNightWaterTankClass5FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_WATER_TANK_CLASS_5_WALK,
    .run          = DRYFIELD_NIGHT_WATER_TANK_CLASS_5_RUN,
    .scriptedJump = DRYFIELD_NIGHT_WATER_TANK_CLASS_5_STAIR_CLIMB,
};

/// Silent blocking contact shared by surface classes 0, 2, 6 and 7.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTankDefaultSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1 passes probes and ignores weapon impacts while retaining pushback.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTankProbePassSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 3 blocks probes, permits impacts and applies pushback with its footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTankClass3Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterTankClass3FootstepSounds,
};

/// Surface class 4 blocks probes, permits impacts and applies pushback with its footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTankClass4Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterTankClass4FootstepSounds,
};

/// Surface class 5 blocks probes, permits impacts and applies pushback with its footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTankClass5Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterTankClass5FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightWaterTankSurfacePropertiesByClass[8] = {
    &_gDryfieldNightWaterTankDefaultSurface,
    &_gDryfieldNightWaterTankProbePassSurface,
    &_gDryfieldNightWaterTankDefaultSurface,
    &_gDryfieldNightWaterTankClass3Surface,
    &_gDryfieldNightWaterTankClass4Surface,
    &_gDryfieldNightWaterTankClass5Surface,
    &_gDryfieldNightWaterTankDefaultSurface,
    &_gDryfieldNightWaterTankDefaultSurface,
};

/// Saved-area update selecting the post-battle handover layout for stage 3 area 21.
///
/// All save modes receive variant 11, reset saved enemy poses and clear the map
/// mark. AREA_APPLY_END follows the update. The aftermath task consumes the
/// list synchronously once the pending battle reset permits its scene.
static AreaApplyRec _gDryfieldNightWaterTankBattleAftermathAreaUpdates[2] = {
    { .stage   = DRYFIELD_NIGHT_WATER_TANK_STAGE,
      .area    = DRYFIELD_NIGHT_WATER_TANK_AREA,
      .variant = DRYFIELD_NIGHT_WATER_TANK_HANDOVER_LAYOUT,
      .policy  = AREA_APPLY_MODE_ALWAYS },
    { .stage = AREA_APPLY_END },
};

static void _dryfieldNightWaterTankInitializeRoom(Task* task);
static void _dryfieldNightWaterTankHoldIceBagTimerState(Task* unusedTask);

/// Starts the water-tank follow-up scene after battle references and reset complete.
///
/// State zero requests weapon re-equipping once battle starts; state 1 waits for
/// all battle references to clear. State 2 waits for the pending battle reset,
/// decrementing the callback-owned kill countdown while waiting, then applies
/// saved area updates, marks handover completion, sets follow-up dialogue and
/// starts the skippable actor scene before killing itself. Requires live room
/// and actor resources through states 0..2; spawn arguments are unused.
static void _dryfieldNightWaterTankBattleAftermathTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_START = 0,
        DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_REFS  = 1,
        DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_RESET = 2,
        DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE     = 2,
        DRYFIELD_NIGHT_WATER_TANK_STORY_DIALOGUE    = 14,
    };

    SVECTOR3 unusedFrame; // Retains the original unused eight-byte stack reservation.

    switch (task->state) {
        case DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_START:
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                gGameSession->flowFlags = gGameSession->flowFlags | GAME_SESSION_FLOW_REEQUIP_WEAPON;
                task->state             = task->state + 1;
                return;
            }
            return;
        case DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_REFS:
            if (gSceneCombatState.battleRefs == 0) {
                task->state = DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_RESET;
                return;
            }
            break;
        case DRYFIELD_NIGHT_WATER_TANK_WAIT_BATTLE_RESET:
            // Publish the handover progress only after the battle reset is ready.
            if (gGameSession->battleResetPending != 0) {
                areaApplySavedUpdates(_gDryfieldNightWaterTankBattleAftermathAreaUpdates);
                gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE);
                gameFlagSetNibble(GAME_FLAG_083, 1);
                evsStartScriptWithSkip(gActor146300WaterTankEventScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor146300WaterTankEventSkipScript);
                gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
                gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, DRYFIELD_NIGHT_WATER_TANK_STORY_DIALOGUE);
                taskKill(task);
                return;
            }
            task->killCountdown = task->killCountdown - 1;
            break;
    }
}

/// Rejects inventory requests to use a key item in the night water-tank room.
///
/// `itemId` is the collected item's inventory ID; all arguments are ignored.
/// Returns 0 so the inventory displays its refusal message. No item is consumed.
static s32 _dryfieldNightWaterTankRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with the requested destination unchanged.
///
/// Copies the complete eight-byte `RoomEventMsg` and returns 1 for query and
/// execution requests alike. `request` and `reply` must be live, non-null records
/// through synchronous dispatch; they may be the same object. `reply` must be
/// writable. No payload is retained and no transition effects are performed.
static s32 _dryfieldNightWaterTankResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_NIGHT_WATER_TANK_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_NIGHT_WATER_TANK_TRANSITION_ALLOWED;
}

/// Starts water-tank CAP slot 14 with explicit variant key 1 for room command 14.
///
/// Handles `ROOM_MESSAGE_COMMAND`; other commands do nothing. Uses queued
/// display-transition playback and bypasses the CAP slot's command opcode.
/// Returns zero regardless of playback starting. Requires the loaded CAP table;
/// receiver, message ID and second payload are unused.
static s32 _dryfieldNightWaterTankCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_WATER_TANK_COMMAND_SEQUENCE = 14,
        DRYFIELD_NIGHT_WATER_TANK_SEQUENCE_VARIANT = 1,
    };

    if (commandId == DRYFIELD_NIGHT_WATER_TANK_COMMAND_SEQUENCE) {
        capStartSequenceSlot(DRYFIELD_NIGHT_WATER_TANK_COMMAND_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, DRYFIELD_NIGHT_WATER_TANK_SEQUENCE_VARIANT);
    }
    return 0;
}

/// Selects the water-tank view scripts or its handover interaction from room triggers.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request only for this call.
/// Actions 3/4 start the view-4/view-3 scripts outside incomplete variant-10
/// handover. Action 5 in variants 10/11 either plays CAP command 23 before the
/// handover or holds player control and spawns its actor scene. Returns zero;
/// receiver, message ID and zero second payload are unused. Request bytes are
/// read unsigned and never retained; requires the loaded room/actor scripts.
static s32 _dryfieldNightWaterTankRoomActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT       = 10,
        DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT_COUNT = 2,
        DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE          = 2,
        DRYFIELD_NIGHT_WATER_TANK_ACTION_VIEW_4          = 3,
        DRYFIELD_NIGHT_WATER_TANK_ACTION_VIEW_3          = 4,
        DRYFIELD_NIGHT_WATER_TANK_ACTION_HANDOVER        = 5,
        DRYFIELD_NIGHT_WATER_TANK_CAP_BEFORE_HANDOVER    = 23,
    };

    u8 roomVariant;

    if ((gGameSession->location.variant != DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) || (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE)) {
        if (request->actionId == DRYFIELD_NIGHT_WATER_TANK_ACTION_VIEW_4) {
            evsStartScript(_gDryfieldNightWaterTankRelocateToView4Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        if (request->actionId == DRYFIELD_NIGHT_WATER_TANK_ACTION_VIEW_3) {
            evsStartScript(_gDryfieldNightWaterTankRelocateToView3Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
    }
    if (request->actionId == DRYFIELD_NIGHT_WATER_TANK_ACTION_HANDOVER) {
        roomVariant = gGameSession->location.variant;
        if ((u32)(roomVariant - DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) < (u32)DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT_COUNT) {
            if ((roomVariant != DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) || (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE)) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(&gActor146300IceBagHandoverTaskDesc, 0, 0, 0);
            } else {
                capRunCommandWithTransition(DRYFIELD_NIGHT_WATER_TANK_CAP_BEFORE_HANDOVER);
            }
        }
    }
    return 0;
}

/// Registers the room receiver, spawns the tank model and restores the active visit.
///
/// State zero restores tank collision in variants 10/11; variant 10 also starts
/// the battle-aftermath controller, while variant 11 restores the handover pose.
/// Advances to state 1 on every path. Requires loaded room/actor resources and
/// a live scene receiver; the registered room task must outlive its slot use.
static void _dryfieldNightWaterTankInitializeRoom(Task* task)
{
    enum {
        DRYFIELD_NIGHT_WATER_TANK_BATTLE_VARIANT        = 10,
        DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT      = 11,
        DRYFIELD_NIGHT_WATER_TANK_RESTORE_VARIANT_COUNT = 2,
    };

    task->msgTable = _gDryfieldNightWaterTankRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(_gDryfieldNightWaterTankSwayTasks, 0, 0, 0);
    if ((u32)(gGameSession->location.variant - DRYFIELD_NIGHT_WATER_TANK_BATTLE_VARIANT) < (u32)DRYFIELD_NIGHT_WATER_TANK_RESTORE_VARIANT_COUNT) {
        _dryfieldNightWaterTankRestoreTankCollision(0);
    }
    if (gGameSession->location.variant == DRYFIELD_NIGHT_WATER_TANK_BATTLE_VARIANT) {
        taskSpawnFromTable(_gDryfieldNightWaterTankBattleAftermathTasks, 0, 0, 0);
    }
    if (gGameSession->location.variant == DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) {
        actor146300RestoreHandoverPose();
    }
    task->state = task->state + 1;
}

/// Holds the Ice Bag's melting timer at the current play time during variant 11.
///
/// Called every frame in room-task state 1. Other variants leave the timer
/// unchanged; the task argument is ignored and the state does not advance.
static void _dryfieldNightWaterTankHoldIceBagTimerState(Task* unusedTask)
{
    enum { DRYFIELD_NIGHT_WATER_TANK_ICE_BAG_HELD_VARIANT = 11 };

    if (gGameSession->location.variant == DRYFIELD_NIGHT_WATER_TANK_ICE_BAG_HELD_VARIANT) {
        inventoryResetIceBagTimer();
    }
}

/// Room-task handlers for initialization, the Ice Bag timer hold and teardown.
///
/// States 0..2 index this three-entry table, with no terminator. Dispatch copies
/// all twelve bytes to the stack before invoking the selected callback.
static const TaskFuncTable3 _gDryfieldNightWaterTankRoomStates = {
    { _dryfieldNightWaterTankInitializeRoom, _dryfieldNightWaterTankHoldIceBagTimerState, taskKill },
};

void dryfieldNightWaterTankRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightWaterTankRoomStates;
    stateHandlers.funcs[task->state](task);
}

/// Restores the tank obstacle's leading geometry in the live room collision grid.
///
/// Copies two faces, normal XYZs and six vertex XYZs from the tank template.
/// Zero `offsetTank` keeps the template position; any nonzero word offsets
/// vertex Z by -200 room-coordinate units. Requires both grids' geometry
/// arrays to cover those prefixes. Other geometry, cell lists and the vectors'
/// fourth halfwords stay intact; this is a partial copy, not a grid reset.
static void _dryfieldNightWaterTankRestoreTankCollision(s32 offsetTank)
{
    enum { DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_FACE_COUNT   = 2,
           DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_VERTEX_COUNT = 6,
           DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_Z_OFFSET     = -200 };
    WorldCollisionGrid*       roomGrid;
    const WorldCollisionGrid* tankTemplate;
    SVECTOR                   offset;
    s32                       geometryIndex;

    roomGrid     = &_gDryfieldNightWaterTankCollisionGrid;
    tankTemplate = &_gDryfieldNightWaterTankObstacleCollisionGrid;

    /// Restores two obstacle faces/normal XYZs and six vertex XYZs in the room grid.
    ///
    /// Grid arguments are stable pointers to disjoint pools, evaluated repeatedly;
    /// `index` is writable s32 scratch left at six. Captures this function's two
    /// geometry counts. Copies complete face records but preserves vector pads,
    /// later geometry and cell lists; both pools must cover the copied prefixes.
#define DRYFIELD_NIGHT_WATER_TANK_COPY_OBSTACLE_GEOMETRY(destination, source, index)              \
    {                                                                                             \
        for ((index) = 0; (index) < DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_FACE_COUNT; (index)++) {   \
            (destination)->normals[(index)].vx = (source)->normals[(index)].vx;                   \
            (destination)->normals[(index)].vy = (source)->normals[(index)].vy;                   \
            (destination)->normals[(index)].vz = (source)->normals[(index)].vz;                   \
            (destination)->faces[(index)]      = (source)->faces[(index)];                        \
        }                                                                                         \
                                                                                                  \
        for ((index) = 0; (index) < DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_VERTEX_COUNT; (index)++) { \
            (destination)->vertices[(index)].vx = (source)->vertices[(index)].vx;                 \
            (destination)->vertices[(index)].vy = (source)->vertices[(index)].vy;                 \
            (destination)->vertices[(index)].vz = (source)->vertices[(index)].vz;                 \
        }                                                                                         \
    }

    DRYFIELD_NIGHT_WATER_TANK_COPY_OBSTACLE_GEOMETRY(roomGrid, tankTemplate, geometryIndex);
#undef DRYFIELD_NIGHT_WATER_TANK_COPY_OBSTACLE_GEOMETRY

    if (offsetTank == 0) {
        offset.vx = 0;
        offset.vy = 0;
        offset.vz = 0;
    } else {
        offset.vx = 0;
        offset.vy = 0;
        offset.vz = DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_Z_OFFSET;
    }

    // Apply the room-axis displacement with the original halfword truncation.
    for (geometryIndex = 0; geometryIndex < DRYFIELD_NIGHT_WATER_TANK_OBSTACLE_VERTEX_COUNT; geometryIndex++) {
        roomGrid->vertices[geometryIndex].vx += offset.vx;
        roomGrid->vertices[geometryIndex].vy += offset.vy;
        roomGrid->vertices[geometryIndex].vz += offset.vz;
    }
}

#include "../../shared/water_tank_sway_task.inc.c"

void dryfieldNightWaterTankNoOpEffectTask(Task* unusedTask)
{
}
