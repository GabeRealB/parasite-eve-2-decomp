#include "rooms/neo_ark_forest_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/stdio.h>

#include "common.h"

#include "neo_ark_forest_zone_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"
#define ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/roaming_enemies.h"

static void _roamerArmPoolB(Task* task);
static void _roamerArmPoolA(Task* task);

static s32  _roamerAmbushMsg(Task* task, s32 messageId, struct ActorCommand* msg, s32 unusedArg);
static void _roamerBankRetreat(Task* task, s32 messageId, s32 hp, s32 unusedArg);

/// Enemy parameters shared by the spawn-slot controllers.
extern EnemyParams  gRoamerParams;
extern DamageAttack D_neo_ark_forest_zone_80182D04[6];

/// How many spawns each session slot arms, indexed by
/// `gGameSession->location.loc.variant`, for the second and the first arming task
/// respectively; zero disables that task's work in the slot.
extern u8 gRoamerArmCountsB[];
extern u8 gRoamerArmCountsA[];

/// How many of the pending spawn slots are armed and scanned.
extern s16 gRoamerReserveCount;

/// Set when a spawn slot was filled while the `gSceneCombatState` reference could
/// not yet be released; the first arming task's tick releases it later.
extern s16 gRoamerReleasePending;

/// Message tables the two arming tasks install in `Task::msgTable`.
extern TaskMessageEntry gRoamerMsgTableA[];
extern TaskMessageEntry gRoamerMsgTableB[];

/// Spawn placements of the first and the second arming task, indexed by the
/// placement request minus one.
extern RoamerSpawnPoint gRoamerSpawnPointsA[];
extern RoamerSpawnPoint D_neo_ark_forest_zone_80182DE8[5];

/// `gSceneCombatState.battleRefs` as seen on the previous frame.
extern s16 gRoamerPrevBattleRefs;

static void _roamerTickPoolB(Task* unusedTask);
static void _neoArkForestZoneFinishRoamerPoolAState(Task* task);

/// State table of the first arming task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D5E8 = { {
    _roamerArmPoolA,
    _roamerTickPoolA,
    _neoArkForestZoneFinishRoamerPoolAState,
    taskKill,
} };

static s32 _neoArkForestZoneIgnorePoolAActorCommand(Task* unusedTask, s32 unusedMessageId, const ActorCommand* unusedCommand, s32 unusedSecondArg);
static s32 _roamerExtendCooldown(Task* unusedTask, s32 unusedMessageId, s32 unusedEventValue, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolA(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolB(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid    _gNeoArkForestZoneCollisionGrid;
extern WorldCollisionTrigger D_neo_ark_forest_zone_801826B4[6];
extern WorldCollisionTrigger D_neo_ark_forest_zone_801829D0[10];
static WorldCoordRoomLights  _gNeoArkForestZoneRoomLights;

static void _neoArkForestZoneRoamerPoolATask(Task* task);
static void _neoArkForestZoneRoamerPoolBTask(Task* task);

static AnimationPackedPose _gNeoArkForestZoneAnimation04364Bank1[6] = {
#include "assets/neo_ark_forest_zone_animation_04364_bank1.inc"
};

static AnimationPackedRotation _gNeoArkForestZoneAnimation04364Bank4[64] = {
#include "assets/neo_ark_forest_zone_animation_04364_bank4.inc"
};

static AnimationRecord _gNeoArkForestZoneAnimation04364Records[141] = {
#include "assets/neo_ark_forest_zone_animation_04364_records.inc"
};

static u16 _gNeoArkForestZoneAnimation04364Indices[20] = {
#include "assets/neo_ark_forest_zone_animation_04364_indices.inc"
};

static AnimationSet _gNeoArkForestZoneAnimation04364 = {
    _gNeoArkForestZoneAnimation04364Records,
    _gNeoArkForestZoneAnimation04364Indices,
    { NULL, _gNeoArkForestZoneAnimation04364Bank1, NULL, NULL, _gNeoArkForestZoneAnimation04364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gNeoArkForestZoneAnimation047D4Bank1[10] = {
#include "assets/neo_ark_forest_zone_animation_047D4_bank1.inc"
};

static AnimationPackedRotation _gNeoArkForestZoneAnimation047D4Bank4[95] = {
#include "assets/neo_ark_forest_zone_animation_047D4_bank4.inc"
};

static AnimationRecord _gNeoArkForestZoneAnimation047D4Records[139] = {
#include "assets/neo_ark_forest_zone_animation_047D4_records.inc"
};

static u16 _gNeoArkForestZoneAnimation047D4Indices[20] = {
#include "assets/neo_ark_forest_zone_animation_047D4_indices.inc"
};

static AnimationSet _gNeoArkForestZoneAnimation047D4 = {
    _gNeoArkForestZoneAnimation047D4Records,
    _gNeoArkForestZoneAnimation047D4Indices,
    { NULL, _gNeoArkForestZoneAnimation047D4Bank1, NULL, NULL, _gNeoArkForestZoneAnimation047D4Bank4, NULL, NULL, NULL },
};

TaskDesc D_neo_ark_forest_zone_80181DBC = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_forest_zone_80181DC8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, neoArkForestZoneResolveTransition },
    { NEO_ARK_FOREST_ZONE_MESSAGE_USE_KEY_ITEM, neoArkForestZoneRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, neoArkForestZoneHandleAction },
    { ROOM_MESSAGE_COMMAND, neoArkForestZoneIgnoreCommandMessage },
    { ROOM_MESSAGE_ACTOR_EVENT, neoArkForestZoneForwardActorEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_neo_ark_forest_zone_80181DF8[2] = {
    &_gNeoArkForestZoneAnimation047D4,
    &_gNeoArkForestZoneAnimation04364,
};

AnimationBankCopyRequest D_neo_ark_forest_zone_80181E00 = { { .sets = D_neo_ark_forest_zone_80181DF8 }, ARRAY_SIZE(D_neo_ark_forest_zone_80181DF8) };

AnimationPlayRequest D_neo_ark_forest_zone_80181E08 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_neo_ark_forest_zone_80181E1C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

s32 D_neo_ark_forest_zone_80181E30 = 2821;

ActorCommand D_neo_ark_forest_zone_80181E34 = { { .loc = { 5, 11 } }, 1 };

s32 D_neo_ark_forest_zone_80181E38 = 0x20B05;

AnimationPlayRequest D_neo_ark_forest_zone_80181E3C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_neo_ark_forest_zone_80181E50 = { { 2888, 128, -95, 0 }, { 0, -1024, 0, 0 } };

Task* D_neo_ark_forest_zone_80181E68 = NULL;

EvsCommand D_neo_ark_forest_zone_80181E6C[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_neo_ark_forest_zone_80181E00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_forest_zone_80181E50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_forest_zone_80181E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x550B000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_forest_zone_80181E34 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_forest_zone_80181E08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .pointer = &D_neo_ark_forest_zone_80181E38 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = neoArkForestZoneStartRoamerAmbush }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_forest_zone_801820A4[1] = {
    { &_gNeoArkForestZoneCollisionGrid, D_neo_ark_forest_zone_801826B4, D_neo_ark_forest_zone_801829D0, NULL },
};

WorldCoordRoomLighting gNeoArkForestZoneRoomLighting[1] = {
    { &_gNeoArkForestZoneRoomLights, NULL },
};

u8* gNeoArkForestZoneViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_forest_zone_801820C0[1] = { 6 };

DirectionWarpEntry D_neo_ark_forest_zone_801820C4[3] = {
    { { { .word = 3072 }, 8995, 0, -128 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 8995, 0, -128 }, { 0, 0, 0, 0 }, 0x550B0002, 0x550B0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POWER_PLANT_1 },
    { { { .word = 1024 }, -7000, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -7000, 0, 0 }, { 0, 0, 0, 0 }, 0x550B000C, 0x550B000B, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -3000, 0, -770 }, { 0, 0, 0, 0 }, { { .word = 0 }, -3000, 0, -770 }, { 0, 0, 0, 0 }, 0x550B0004, 0x550B0003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space face normals of the forest zone box, with ONE per unit.
static SVECTOR _gNeoArkForestZoneCollisionGridNormals[] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_normals.inc"
};

/// Corners of the room box in signed game coordinates.
static SVECTOR _gNeoArkForestZoneCollisionGridVerts[] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_verts.inc"
};

/// Six box faces: class 1 on the Y=0 floor and class 0 on every other face.
static WorldCollisionGridFace _gNeoArkForestZoneCollisionGridFaces[] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_faces.inc"
};

/// Serialized signed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gNeoArkForestZoneCollisionGridCells[] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_cells.inc"
};

// The generated table supplies element offsets into the face-index storage.
#define GRID_CELL(i) (&_gNeoArkForestZoneCollisionGridCells[i])
/// Five X-major cells in one Z row, borrowing terminated face-index lists.
static s16* _gNeoArkForestZoneCollisionGridTable[] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_table.inc"
};
#undef GRID_CELL

/// Forest zone collision box and its five-by-one XZ cell index.
///
/// Geometry spans X=-8000..10000, Y=-5000..0 and Z=-1300..1300 in room units.
/// Cells start at (-8000,-1300) and have 4000-unit sides. Room activation binds
/// the mutable descriptor to the current view; the mesh stays owned by this overlay.
static WorldCollisionGrid _gNeoArkForestZoneCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkForestZoneCollisionGridNormals,
    .vertices    = _gNeoArkForestZoneCollisionGridVerts,
    .faces       = _gNeoArkForestZoneCollisionGridFaces,
    .cellFaceIds = _gNeoArkForestZoneCollisionGridTable,
    .xBias       = 8000,
    .zBias       = 1300,
    .cellCountX  = ARRAY_SIZE(_gNeoArkForestZoneCollisionGridTable),
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkForestZoneCollisionGridFaces),
};

ViewCamera gNeoArkForestZoneViewCameras[NEO_ARK_FOREST_ZONE_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 78, -4095 }, { 0, 4095, 78 } }, { -1000, 30000, 575 } }, 380 },
    { { { { -1370, 0, -3859 }, { -488, 4063, 173 }, { 3828, 518, -1359 } }, { -3940, 1460, -810 } }, 230 },
    { { { { -1441, 0, -3833 }, { 76, 4095, -28 }, { 3833, -81, -1441 } }, { 1990, 870, -910 } }, 230 },
    { { { { -1589, 0, -3775 }, { 258, 4086, -108 }, { 3766, -280, -1585 } }, { 6420, 670, -1080 } }, 230 },
    { { { { -899, 0, 3996 }, { -547, 4057, -123 }, { -3958, -560, -890 } }, { 2120, 500, -690 } }, 230 },
    { { { { -905, 0, 3994 }, { -83, 4095, -19 }, { -3993, -86, -905 } }, { -1010, 1270, -860 } }, 329 },
};

/// View 1's background selector and terminator; no source sprites are selected.
///
/// The zero first count retains decoded image strips. The source slot retains
/// this list's address without treating it as a SpriteSource array.
static SpriteBatch _gNeoArkForestZoneView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Forest foreground texture bindings and opaque, neutral source modulation.
enum {
    NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE      = getTPage(1, 0, 960, 0),
    NEO_ARK_FOREST_ZONE_SPRITE_CLUT              = getClut(0, 255),
    NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR     = 128,
    NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION = 0,
};

/// View 2's four foreground rectangles at sorting depth 1000.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled OT sorting values. Texture page and CLUT values encode GPU addresses.
static SpriteSource _gNeoArkForestZoneView2SpriteSources[] = {
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, -104, 1000, { .fields = { 120, 240 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 80, 88 } }, -160, -120, 1000, { .fields = { 48, 0 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 48, 72 } }, -160, -32, 1000, { .fields = { 80, 168 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 96, 80 } }, -160, 40, 1000, { .fields = { 32, 88 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
};

/// View 2's image-strip selector, full source range [0,4) and terminator.
static SpriteBatch _gNeoArkForestZoneView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkForestZoneView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's five foreground rectangles at their individual OT sorting depths.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. All sources use opaque, neutral modulation.
static SpriteSource _gNeoArkForestZoneView3SpriteSources[] = {
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 72, 1604, { .fields = { 120, 224 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 104, 32 } }, -160, -120, 1550, { .fields = { 24, 192 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 96, 48 } }, -160, -88, 1571, { .fields = { 32, 144 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 80, 80 } }, -160, -40, 1611, { .fields = { 48, 0 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 48, 64 } }, -160, 40, 1502, { .fields = { 80, 80 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
};

/// View 3's image-strip selector, full source range [0,5) and terminator.
static SpriteBatch _gNeoArkForestZoneView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkForestZoneView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's five foreground rectangles at sorting depth 1050.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. All sources use opaque, neutral modulation.
static SpriteSource _gNeoArkForestZoneView4SpriteSources[] = {
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 96, 96 } }, 64, -120, 1050, { .fields = { 32, 0 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 64, 48 } }, 96, -24, 1050, { .fields = { 64, 96 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 80, 8 } }, 80, 24, 1050, { .fields = { 48, 184 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 64, 32 } }, 96, 32, 1050, { .fields = { 64, 144 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 64, 1050, { .fields = { 112, 176 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
};

/// View 4's image-strip selector, full source range [0,5) and terminator.
static SpriteBatch _gNeoArkForestZoneView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkForestZoneView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's seven foreground rectangles at their individual OT sorting depths.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. All sources use opaque, neutral modulation.
static SpriteSource _gNeoArkForestZoneView5SpriteSources[] = {
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, -40, 1342, { .fields = { 120, 240 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 96, 48 } }, 64, -40, 1125, { .fields = { 16, 144 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 104, 48 } }, 56, 8, 1125, { .fields = { 8, 192 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 80, 64 } }, 80, 56, 1125, { .fields = { 32, 80 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 104, 80 } }, 24, -120, 1314, { .fields = { 24, 0 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 16, 80 } }, 128, -120, 1125, { .fields = { 112, 80 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_FOREST_ZONE_SPRITE_TEXTURE_PAGE, NEO_ARK_FOREST_ZONE_SPRITE_CLUT, { .fields = { 16, 80 } }, 144, -120, 1125, { .fields = { 112, 160 } }, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_FOREST_ZONE_SPRITE_OPAQUE_MODULATION },
};

/// View 5's image-strip selector, full source range [0,7) and terminator.
static SpriteBatch _gNeoArkForestZoneView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkForestZoneView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's background selector and terminator; no source sprites are selected.
///
/// The zero first count retains decoded image strips. The source slot retains
/// this list's address without treating it as a SpriteSource array.
static SpriteBatch _gNeoArkForestZoneView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkForestZoneSpriteViews[NEO_ARK_FOREST_ZONE_VIEW_COUNT] = {
    { { .empty = _gNeoArkForestZoneView1SpriteBatches }, _gNeoArkForestZoneView1SpriteBatches, NULL },
    { { .elements = _gNeoArkForestZoneView2SpriteSources }, _gNeoArkForestZoneView2SpriteBatches, NULL },
    { { .elements = _gNeoArkForestZoneView3SpriteSources }, _gNeoArkForestZoneView3SpriteBatches, NULL },
    { { .elements = _gNeoArkForestZoneView4SpriteSources }, _gNeoArkForestZoneView4SpriteBatches, NULL },
    { { .elements = _gNeoArkForestZoneView5SpriteSources }, _gNeoArkForestZoneView5SpriteBatches, NULL },
    { { .empty = _gNeoArkForestZoneView6SpriteBatches }, _gNeoArkForestZoneView6SpriteBatches, NULL },
};

/// Room point lights: one admitted in every view and one admitted only in view 6.
///
/// Positions and falloff radii use room-coordinate units; RGB has 12 fractional
/// bits. Coordinate binding/composition and attenuation queries modify the records.
static WorldCoordPointLight _gNeoArkForestZonePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4220, -1742, 0 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3641, 3828, 3683 },
        },
        .inner = 6362,
        .outer = 15904,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -252, -969, 623 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = 6,
                           } },
            .color     = { ONE, 4100, ONE },
        },
        .inner = 1601,
        .outer = 2781,
    },
};

/// The room's two point lights, with no directional or cone light arrays.
///
/// Borrows writable light records for this overlay's lifetime; the count is in elements.
static WorldCoordRoomLights _gNeoArkForestZoneRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkForestZonePointLights),
    .pointLights           = _gNeoArkForestZonePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger D_neo_ark_forest_zone_801826B4[6] = {
    { NULL, NULL, NULL, { 6141, -2640, 12, 0 }, { { -242, -2976, -3011, 0 }, { 231, -2976, 3006, 0 }, { -242, 2976, -3011, 0 }, { 231, 2976, 3006, 0 } }, { 4092, 0, -323, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6407, -2672, -38, 0 }, { { 217, -2976, 2875, 0 }, { -219, -2976, -2877, 0 }, { 217, 2976, 2875, 0 }, { -219, 2976, -2877, 0 } }, { -4085, 0, 309, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 303, -2625, -17, 0 }, { { -927, -3088, -2918, 0 }, { 922, -3088, 2912, 0 }, { -927, 3088, -2918, 0 }, { 922, 3088, 2912, 0 } }, { 3905, 0, -1239, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 511, -2674, -49, 0 }, { { 900, -2976, 2902, 0 }, { -907, -2976, -2908, 0 }, { 900, 2977, 2902, 0 }, { -907, 2977, -2908, 0 } }, { -3927, 0, 1221, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4322, -2641, 94, 0 }, { { -289, -3104, -3782, 0 }, { 264, -3104, 3755, 0 }, { -289, 3105, -3782, 0 }, { 264, 3105, 3755, 0 } }, { 4086, 0, -301, 0 }, { 0, 0, 4096, 0 }, 4884, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4176, -2657, 110, 0 }, { { 192, -3056, 3755, 0 }, { -200, -3056, -3764, 0 }, { 192, 3057, 3755, 0 }, { -200, 3057, -3764, 0 } }, { -4098, 0, 213, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource keys matched by the forest zone's placement tables.
enum {
    NEO_ARK_FOREST_ZONE_RESOURCE_HORNED_STRANGER       = 13,
    NEO_ARK_FOREST_ZONE_RESOURCE_EXTRA_HORNED_STRANGER = 10,
    NEO_ARK_FOREST_ZONE_RESOURCE_BLACK_BEETLE          = 38,
    NEO_ARK_FOREST_ZONE_RESOURCE_PAWN_GOLEM            = 20,
    NEO_ARK_FOREST_ZONE_RESOURCE_EXTRA_PAWN_GOLEM      = 56,
};

/// Variant 1's two Horned Stranger resource recipes, keyed by placement entry ID.
///
/// Selects actor packages 401300 and 356100, descriptor zero, through file-group
/// bases 40 and 30. The final AREA_PLACEMENT_END record ends loading/spawn searches.
static AreaResource _gNeoArkForestZoneVariant1Resources[] = {
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_HORNED_STRANGER, .fileNumber = 13, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401300HornedStrangerTaskDesc },
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_EXTRA_HORNED_STRANGER, .fileNumber = 561, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor356100HornedStrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Horned Stranger resource recipe for its two entry-13 placements.
///
/// Selects actor package 401300, descriptor zero, with an ending record.
static AreaResource _gNeoArkForestZoneVariant2Resources[] = {
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_HORNED_STRANGER, .fileNumber = 13, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401300HornedStrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's Horned Stranger resource recipe, ending at AREA_PLACEMENT_END.
///
/// Selects actor package 401300, descriptor zero. Only entry ID 13 matches;
/// this resource table's termination is independent of the placement table's walk.
static AreaResource _gNeoArkForestZoneVariant3Resources[] = {
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_HORNED_STRANGER, .fileNumber = 13, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401300HornedStrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's Black Beetle resource recipe for its ten entry-38 placements.
///
/// Selects actor package 103800, descriptor zero, with an ending record.
static AreaResource _gNeoArkForestZoneVariant4Resources[] = {
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_BLACK_BEETLE, .fileNumber = 38, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 5's Horned Stranger and Pawn GOLEM resource recipes.
///
/// Placement IDs 13 and 20 select actor packages 401300 and 302000 respectively,
/// each at descriptor zero. The table ends at AREA_PLACEMENT_END.
static AreaResource _gNeoArkForestZoneVariant5Resources[] = {
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_HORNED_STRANGER, .fileNumber = 13, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401300HornedStrangerTaskDesc },
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_PAWN_GOLEM, .fileNumber = 20, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = gActor302000PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's two Pawn GOLEM resource recipes, keyed by placement entry ID.
///
/// IDs 20 and 56 select actor packages 102000 and 205600 respectively, each at
/// descriptor zero. The table ends at AREA_PLACEMENT_END.
static AreaResource _gNeoArkForestZoneVariant11Resources[] = {
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_PAWN_GOLEM, .fileNumber = 20, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102000PawnGolemTasks },
    { .entryId = NEO_ARK_FOREST_ZONE_RESOURCE_EXTRA_PAWN_GOLEM, .fileNumber = 56, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor205600PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaResource D_neo_ark_forest_zone_80182930[2] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401300HornedStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_neo_ark_forest_zone_80182948[2] = {
    { 13, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_neo_ark_forest_zone_80182968[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B190, _gNeoArkForestZoneVariant1Resources },
    { D_map_neo_ark_8017B1C0, _gNeoArkForestZoneVariant2Resources },
    { D_map_neo_ark_8017B1F0, _gNeoArkForestZoneVariant3Resources },
    { D_map_neo_ark_8017B210, _gNeoArkForestZoneVariant4Resources },
    { D_map_neo_ark_8017B2C0, _gNeoArkForestZoneVariant5Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_neo_ark_forest_zone_80182948, D_neo_ark_forest_zone_80182930 },
    { D_map_neo_ark_8017B300, _gNeoArkForestZoneVariant11Resources },
    { NULL, NULL },
};

WorldCollisionTrigger D_neo_ark_forest_zone_801829D0[10] = {
    { NULL, NULL, NULL, { 9488, -48, 0, 0 }, { { -688, 0, -1024, 0 }, { 688, 0, -1024, 0 }, { -688, 0, 1024, 0 }, { 688, 0, 1024, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7680, -48, -16, 0 }, { { -592, 0, -880, 0 }, { 592, 0, -880, 0 }, { -592, 0, 880, 0 }, { 592, 0, 880, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2480, -48, -928, 0 }, { { 1040, 0, -400, 0 }, { 1008, 0, 400, 0 }, { -1008, 0, -400, 0 }, { -1040, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6768, -64, -96, 0 }, { { -480, 0, -1792, 0 }, { 224, 0, -1792, 0 }, { -224, 0, 1792, 0 }, { 480, 0, 1792, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 1854, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4399, -64, -33, 0 }, { { -1851, 0, -1772, 0 }, { 1541, 0, -1796, 0 }, { -1541, 0, 1796, 0 }, { 1851, 0, 1772, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -626, -64, 46, 0 }, { { -1476, 0, -1706, 0 }, { 371, 0, -1800, 0 }, { -370, 0, 1801, 0 }, { 1477, 0, 1707, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6624, -64, -16, 0 }, { { -528, 0, -1744, 0 }, { 528, 0, -1744, 0 }, { -528, 0, 1744, 0 }, { 528, 0, 1744, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1819, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 4, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4959, -64, 0, 0 }, { { -720, 0, -1696, 0 }, { 464, 0, -1696, 0 }, { -464, 0, 1696, 0 }, { 720, 0, 1696, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1841, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 5, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -64, 1184, 0 }, { { 9328, 0, -400, 0 }, { 9296, 0, 400, 0 }, { -9296, 0, -400, 0 }, { -9328, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4097, 0 }, 9328, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, -1120, 0 }, { { 9328, 0, -400, 0 }, { 9296, 0, 400, 0 }, { -9296, 0, -400, 0 }, { -9328, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4095, 0 }, 9328, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    NEO_ARK_FOREST_ZONE_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
    NEO_ARK_FOREST_ZONE_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3B,
};

/// Walking, running and scripted-jump footstep bases for the class-1 floor.
///
/// Walking and scripted jumps share a cue pair; running uses the following pair.
/// Consumers borrow the cues while this overlay and its type-1 bank are loaded.
static WorldCollisionFootstepSounds _gNeoArkForestZoneFootstepSounds = {
    .walk         = NEO_ARK_FOREST_ZONE_FOOTSTEP_WALK,
    .run          = NEO_ARK_FOREST_ZONE_FOOTSTEP_RUN,
    .scriptedJump = NEO_ARK_FOREST_ZONE_FOOTSTEP_WALK,
};

/// Classes 0 and 2..7: probe blocking, weapon impacts and pushback without footsteps.
static WorldCollisionSurfaceProperties _gNeoArkForestZoneSolidSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1: probe blocking, weapon impacts, pushback and the floor's footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkForestZoneSolidFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkForestZoneFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkForestZoneSurfaceProperties[NEO_ARK_FOREST_ZONE_SURFACE_CLASS_COUNT] = {
    &_gNeoArkForestZoneSolidSurface,
    &_gNeoArkForestZoneSolidFootstepSurface,
    &_gNeoArkForestZoneSolidSurface,
    &_gNeoArkForestZoneSolidSurface,
    &_gNeoArkForestZoneSolidSurface,
    &_gNeoArkForestZoneSolidSurface,
    &_gNeoArkForestZoneSolidSurface,
    &_gNeoArkForestZoneSolidSurface,
};

DamageAttack D_neo_ark_forest_zone_80182D04[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams gRoamerParams = { D_neo_ark_forest_zone_80182D04, 420, 115, 200, 5, 100, 10, 100, 10 };

// Retained numeric records following the enemy parameters.
u16 D_neo_ark_forest_zone_80182D2C[3][4] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

u8 gRoamerArmCountsB[16] = {
    0,
    1,
    3,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 gRoamerArmCountsA[14] = {
    0,
    3,
    2,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Signed countdown shared by the pools, gating spawn requests and battle reset.
///
/// Active per-frame states decrement positive values; zero is ready and -1
/// pauses the countdown. Arming or ambush restart sets 90 frames, spawning or
/// retreat adds 90, and battle completion sets 150. Arithmetic stores narrow
/// back to a signed halfword.
static s16 _gRoamerCooldownFrames = ROAMER_INITIAL_COOLDOWN_FRAMES;

s16 gRoamerReserveCount = 0;

/// One-based spawn-point selector pending for the next active pool tick.
///
/// Zero means none. Room actions select 1..5 here; pool A uses that table row,
/// while pool B uses its fifth row for selectors above four. Each active tick
/// clears the selector even if no enemy was revived. The latch also clears it
/// for repeated actions or while the cooldown is not zero.
static s16 _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;

/// Last room-action ID observed by either pool's spawn-request latch.
///
/// Starts at zero and records the borrowed request's byte even when cooldown
/// suppresses it, so the same action must change before it can request a spawn.
static s16 _gRoamerLastActionId = ROAMER_SPAWN_POINT_NONE;

s16 gRoamerReleasePending = 0;

TaskMessageEntry gRoamerMsgTableA[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolA },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerBankRetreat },
    { ACTOR_COMMAND_MESSAGE_APPLY, _neoArkForestZoneIgnorePoolAActorCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint gRoamerSpawnPointsA[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
};

s16 gRoamerPrevBattleRefs = 0;

TaskMessageEntry gRoamerMsgTableB[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolB },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerExtendCooldown },
    { ACTOR_COMMAND_MESSAGE_APPLY, _roamerAmbushMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint D_neo_ark_forest_zone_80182DE8[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

s16 D_neo_ark_forest_zone_80182E10[4] = {
    0x7FFF,
    0x7FFF,
    0x7FFF,
    0x7FFF,
};

TaskDesc D_neo_ark_forest_zone_80182E18 = { { { TASK_BODY_NONE, 32 } }, _neoArkForestZoneRoamerPoolBTask, { .value = 0 } };

TaskDesc D_neo_ark_forest_zone_80182E24 = { { { TASK_BODY_NONE, 32 } }, _neoArkForestZoneRoamerPoolATask, { .value = 0 } };

static void _neoArkForestZoneFinishRoamerPoolBState(Task* task);

#include "../../shared/roaming_enemies_bank_retreat.inc.c"

#include "../../shared/roaming_enemies_seed_reserve_hp.inc.c"

#include "../../shared/roaming_enemies_arm_pool_a.inc.c"

#include "../../shared/roaming_enemies_tick_pool_a.inc.c"

#include "../../shared/roaming_enemies_ambush_msg.inc.c"

#include "../../shared/roaming_enemies_arm_pool_b.inc.c"

/// Updates the shared roaming-enemy pool feeding the forest zone.
///
/// Requires initialized reserve count 0..5, a location variant in 0..15 and live
/// placed-actor/model storage. An unarmed variant returns without clearing its
/// request. Active updates persist reserve losses when battle references fall to
/// zero, count down the shared cooldown and service a deferred battle reset.
/// A nonzero one-based selector revives at most one dormant actor among slots 0..1.
/// Selectors 1..4 choose their rows; every other nonzero selector chooses row 5.
/// HP is tested as signed 16-bit data; zero and negative slots are unavailable.
/// Placement uses parent-coordinate units at Y=0 and 4096 yaw units per turn.
/// Each active update clears the request even when no enemy can be revived.
static void _roamerTickPoolB(Task* unusedTask)
{
    enum {
        ROAMER_POOL_B_RESERVE_SLOTS   = ARRAY_SIZE(gRoamerReserveHp),
        ROAMER_POOL_B_PLACED_ACTORS   = 2,
        ROAMER_POOL_B_HP_DORMANT      = -999,
        ROAMER_POOL_B_COMMAND_LEAP_IN = 11,
    };
    s16    actorIndex;
    s16    bankedCount;
    s32    previousKills;
    s32    persistedReserve;
    Enemy* enemy;
    s16    reserveIndex;
    s16    reserveSlot;

/// Places a revived actor without caching or reordering the placed-actor lookups.
///
/// actorIndex and pointIndex are side-effect-free indices in 0..1 and 0..4,
/// evaluated repeatedly. dirtyBeforeTurn is a constant 0 or 1 preserving the
/// row's dirty-stamp ordering. Uses the borrowed forest spawn-point table and
/// expands to complete statements in a switch arm.
#define ROAMER_PLACE_POOL_B_ENEMY(actorIndex, pointIndex, dirtyBeforeTurn)                                          \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[pointIndex].x; \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[1] = 0;                                            \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[pointIndex].z; \
    if (dirtyBeforeTurn) {                                                                                          \
        sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                   \
    }                                                                                                               \
    gfxRotMatrixY(&sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord,                                      \
                  D_neo_ark_forest_zone_80182DE8[pointIndex].yaw, GRAPHICS_ROTATION_REPLACE);                       \
    if (!(dirtyBeforeTurn)) {                                                                                       \
        sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                   \
    }

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (_gRoamerCooldownFrames > ROAMER_COOLDOWN_READY) {
        _gRoamerCooldownFrames--;
    }
    // Persist losses only on the transition out of a live battle.
    if (gSceneCombatState.battleRefs == 0 && gRoamerPrevBattleRefs > 0) {
        persistedReserve = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        bankedCount      = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if ((s16)gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", persistedReserve - bankedCount);
        previousKills    = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B);
        persistedReserve = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        bankedCount      = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if ((s16)gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B, previousKills + (persistedReserve - bankedCount));
        bankedCount = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if ((s16)gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, bankedCount);
        areaSyncLocationVariant(&gGameSession->location.loc);
        _gRoamerCooldownFrames = ROAMER_POST_BATTLE_COOLDOWN_FRAMES;
    }
    gRoamerPrevBattleRefs = gSceneCombatState.battleRefs;
    // Defer reward/reset cleanup until the post-battle cooldown expires.
    if (gGameSession->battleResetPending == 1 && _gRoamerCooldownFrames == ROAMER_COOLDOWN_READY) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.peTargetCount             = 0;
        gSceneCombatState.battleRefs                = 0;
        gSceneCombatState.expReward                 = 0;
        gSceneCombatState.bpReward                  = 0;
        gSceneCombatState.mpReward                  = 0;
        gGameSession->battleResetPending            = 0;
    }
    // Consume one reserve slot and request the revived actor's leap-in.
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED && _gRoamerPendingSpawnPoint != ROAMER_SPAWN_POINT_NONE) {
        gRoamerCommand.context.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
        gRoamerCommand.context.loc.area  = GAME_AREA_NEO_ARK_FOREST_ZONE;
        gRoamerCommand.command           = ROAMER_POOL_B_COMMAND_LEAP_IN;
        for (actorIndex = 0; actorIndex < ROAMER_POOL_B_PLACED_ACTORS; actorIndex++) {
            if (sceneFindPlacedActor(actorIndex) == 0) {
                break;
            }
            enemy = sceneFindPlacedActor(actorIndex)->spawnArg2.pointer;
            if (enemy == NULL) {
                break;
            }
            if (enemy->hp == ROAMER_POOL_B_HP_DORMANT) {
                for (reserveIndex = 0; reserveIndex < gRoamerReserveCount; reserveIndex++) {
                    if ((s16)gRoamerReserveHp[reserveIndex] > 0) {
                        enemy->hp                      = gRoamerReserveHp[reserveIndex];
                        enemy->reactionFlags           = 0;
                        gRoamerReserveHp[reserveIndex] = 0;
                        break;
                    }
                }
                if (enemy->hp > 0) {
                    sceneAcquireBattleRef(0);
                    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(actorIndex), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    switch ((s16)(_gRoamerPendingSpawnPoint - 1)) {
                        case 0:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 0, 1);
                            break;
                        case 1:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 1, 1);
                            break;
                        case 2:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 2, 0);
                            break;
                        case 3:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 3, 0);
                            break;
                        case 4:
                        default:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 4, 0);
                            break;
                    }
                }
                break;
            }
        }
    }
    _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;
#undef ROAMER_PLACE_POOL_B_ENEMY
}

/// Ignores actor commands sent to the forest's first reserve-enemy pool.
///
/// Returns zero without changing the pool or receiver. All arguments are
/// ignored; the borrowed command is neither accessed nor retained.
static s32 _neoArkForestZoneIgnorePoolAActorCommand(Task* unusedTask, s32 unusedMessageId, const ActorCommand* unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Selects the pool-A room-action latch emitted by the shared fragment.
///
/// The callback type and inclusion requirements are documented in
/// `roaming_enemies_latch_request.inc.c`.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolA
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the forest pool-A controller from its finishing state to teardown.
///
/// Called in state 2; state 3 releases the controller on the next dispatch.
/// This callback neither clears reserves nor releases the placed enemy tasks.
static void _neoArkForestZoneFinishRoamerPoolAState(Task* task)
{
    task->state = task->state + 1;
}

/// Dispatches the forest's roaming-enemy pool-A controller for this visit.
///
/// Start a bodyless task in state 0; states 0..3 arm the reserve, process spawn
/// requests, advance to teardown and release the controller. Active states need
/// location variant 0..13 and live placed actors. Both pools share reserve HP
/// and cooldown storage; this controller does not own the placed actor tasks.
static void _neoArkForestZoneRoamerPoolATask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_forest_zone_8017D5E8;
    handlers.funcs[task->state](task);
}

#include "../../shared/roaming_enemies_extend_cooldown.inc.c"

/// Selects the private pool-B room-action latch for this fragment inclusion.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolB
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the forest pool-B controller from its finishing state to teardown.
///
/// State 2 only increments the task's signed state; state 3 releases it on the
/// next dispatch. This state does not clear shared reserves or release actors.
static void _neoArkForestZoneFinishRoamerPoolBState(Task* task)
{
    task->state = task->state + 1;
}

/// State table of the second arming task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D634 = { {
    _roamerArmPoolB,
    _roamerTickPoolB,
    _neoArkForestZoneFinishRoamerPoolBState,
    taskKill,
} };

/// Dispatches the forest's roaming-enemy pool-B controller for this visit.
///
/// Requires a live task with state 0..3: arm the shared reserve, process
/// cooldowns and spawn requests, advance to teardown, then release the task.
/// Arming and active updates require a location variant in 0..15. Pool A and
/// pool B share reserve HP and cooldown storage; the controller owns no actors.
static void _neoArkForestZoneRoamerPoolBTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_forest_zone_8017D634;
    handlers.funcs[task->state](task);
}
