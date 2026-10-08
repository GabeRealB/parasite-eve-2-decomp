#include "rooms/dryfield_night_water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor_146300.h"
#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"
#include "../../shared/water_tank.h"

extern WorldCollisionGrid D_dryfield_night_water_tank_8017E08C;
extern WorldCollisionGrid D_dryfield_night_water_tank_8017F4B0;

/// `AREA_APPLY_END`-terminated `AreaApplyRec` list the room applies when the scripted end
/// of the visit fires.
extern AreaApplyRec D_dryfield_night_water_tank_801808B0[];

/// Main-executable halfword the second state waits on before it may advance.

/// Event scripts handed to `evsStartScriptWithSkip`: the one it starts and its skip
/// target.
extern EvsCommand D_actor_146300_80137C28[];
extern EvsCommand D_actor_146300_80138570[];

/// Message table of the night water-tank room, 0x13EE..0x13F1 with the
/// `TASK_MESSAGE_TABLE_END` terminator: `_dryfieldNightWaterTankResolveRoomEvent`,
/// `_dryfieldNightWaterTankRejectKeyItemUse`, `..._8017D76C` and `..._8017D73C`.
extern TaskMessageEntry D_dryfield_night_water_tank_8017DFE8[];

/// Task descriptor tables spawned by the room entry task, each one entry and
/// the 0xFFFF terminator: `8017E010` runs the exit task
/// `_dryfieldNightWaterTankBattleAftermathTask`, `8017EE28` the tank model's
/// update `waterTankSwayTask`.
extern TaskDesc D_dryfield_night_water_tank_8017E010[];
extern TaskDesc D_dryfield_night_water_tank_8017EE28[];

/// Absolute import: the shared room script descriptor 0x8013788C, spawned by
/// entry 0 in the handler below.
extern TaskDesc D_actor_146300_8013788C;

/// Model/lighting records the handler below toggles on message 3 and 4.
extern EvsCommand D_dryfield_night_water_tank_8017DDD8[];
extern EvsCommand D_dryfield_night_water_tank_8017DEE0[];

/// The tank's wobble spring: `8017EE40` is the accumulated yaw handed to
/// `gfxRotMatrixY` (`>> 8`), `8017EE44` its velocity, `8017EE48` the yaw it
/// steps toward and `8017EE4C` the target that step chases.

static void _dryfieldNightWaterTankRestoreTankCollision(s32 offsetTank);

extern WorldCollisionGrid     D_dryfield_night_water_tank_8017F4B0;
extern WorldCollisionOccluder D_dryfield_night_water_tank_801807CC[2];
extern WorldCollisionTrigger  D_dryfield_night_water_tank_8018038C[4];
extern WorldCollisionTrigger  D_dryfield_night_water_tank_801804BC[8];
extern WorldCoordRoomLights   D_dryfield_night_water_tank_80180374[1];
static TmdSource              _gDryfieldNightWaterTankModel00FF8;

static s32  _dryfieldNightWaterTankRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldNightWaterTankResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldNightWaterTankCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32  _dryfieldNightWaterTankRoomActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldNightWaterTankBattleAftermathTask(Task* task);

/// Inventory's request to use a collected key item in this room.
enum { DRYFIELD_NIGHT_WATER_TANK_MESSAGE_USE_KEY_ITEM = 0x13F1 };

ActorTransform D_dryfield_night_water_tank_8017DD94 = { { 820, -0x4010, 884, 0 }, { 0, 2560, 0, 0 } };

ActorTransform D_dryfield_night_water_tank_8017DDAC = { { 1868, -0x2EE0, 1740, 0 }, { 0, 512, 0, 0 } };

AnimationPlayRequest D_dryfield_night_water_tank_8017DDC4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_dryfield_night_water_tank_8017DDD8[11] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_tank_8017DDC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_water_tank_8017DD94 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_STAGE_SOUND, { .value = 0x52150001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_water_tank_8017DEE0[11] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_tank_8017DDC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_water_tank_8017DDAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_STAGE_SOUND, { .value = 0x52150001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_dryfield_night_water_tank_8017DFE8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightWaterTankResolveRoomEvent },
    { DRYFIELD_NIGHT_WATER_TANK_MESSAGE_USE_KEY_ITEM, _dryfieldNightWaterTankRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightWaterTankRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightWaterTankCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_water_tank_8017E010[2] = {
    { { { TASK_BODY_NONE, 32 } }, _dryfieldNightWaterTankBattleAftermathTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static SVECTOR _gDryfieldNightWaterTankCollision00ACCNormals[2] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_normals.inc"
};

static SVECTOR _gDryfieldNightWaterTankCollision00ACCVerts[6] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightWaterTankCollision00ACCFaces[2] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_faces.inc"
};

static s16 _gDryfieldNightWaterTankCollision00ACCCells[4] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightWaterTankCollision00ACCCells[i])
static s16* _gDryfieldNightWaterTankCollision00ACCTable[1] = {
#include "assets/dryfield_night_water_tank_collision_00ACC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_water_tank_8017E08C = { NULL, _gDryfieldNightWaterTankCollision00ACCNormals, _gDryfieldNightWaterTankCollision00ACCVerts, _gDryfieldNightWaterTankCollision00ACCFaces, _gDryfieldNightWaterTankCollision00ACCTable, -1016, 2444, 1, 1, 4000, 2 };

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

TaskDesc D_dryfield_night_water_tank_8017EE28[2] = {
    { { { TASK_BODY_TMD, 192 } }, waterTankSwayTask, { .model = &_gDryfieldNightWaterTankModel00FF8 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gWaterTankYaw = 0;

s32 gWaterTankYawSpeed = 0;

s32 gWaterTankYawStep = 0;

s32 gWaterTankYawTarget = 0;

WorldCoordRoomLighting D_dryfield_night_water_tank_8017EE50[1] = {
    { D_dryfield_night_water_tank_80180374, NULL },
};

WorldCollisionRoomResources D_dryfield_night_water_tank_8017EE58[1] = {
    { &D_dryfield_night_water_tank_8017F4B0, D_dryfield_night_water_tank_8018038C, D_dryfield_night_water_tank_801804BC, D_dryfield_night_water_tank_801807CC },
};

u8 D_dryfield_night_water_tank_8017EE68[12] = {
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

u8* D_dryfield_night_water_tank_8017EE74[1] = {
    D_dryfield_night_water_tank_8017EE68,
};

ViewCount D_dryfield_night_water_tank_8017EE78[1] = { 10 };

DirectionWarpEntry D_dryfield_night_water_tank_8017EE7C[2] = {
    { { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldNightWaterTankCollision01EF0Normals[16] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_normals.inc"
};

static SVECTOR _gDryfieldNightWaterTankCollision01EF0Verts[78] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightWaterTankCollision01EF0Faces[38] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_faces.inc"
};

static s16 _gDryfieldNightWaterTankCollision01EF0Cells[126] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightWaterTankCollision01EF0Cells[i])
static s16* _gDryfieldNightWaterTankCollision01EF0Table[4] = {
#include "assets/dryfield_night_water_tank_collision_01EF0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_water_tank_8017F4B0 = { NULL, _gDryfieldNightWaterTankCollision01EF0Normals, _gDryfieldNightWaterTankCollision01EF0Verts, _gDryfieldNightWaterTankCollision01EF0Faces, _gDryfieldNightWaterTankCollision01EF0Table, 3500, 3300, 2, 2, 4000, 38 };

ViewCamera D_dryfield_night_water_tank_8017F4D4[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 1429, 0, -3838 }, { -2523, 3086, -939 }, { 2892, 2692, 1077 } }, { 5050, 0x3E4E, 1880 } }, 257 },
    { { { { -853, 0, 4006 }, { 1212, 3903, 258 }, { -3818, 1239, -813 } }, { -5920, 0x37FA, -1350 } }, 257 },
    { { { { -1180, 0, 3922 }, { 173, 4092, 52 }, { -3918, 180, -1179 } }, { -4220, 0x42EA, -1350 } }, 230 },
    { { { { 2889, 0, 2902 }, { 1979, 2995, -1970 }, { -2123, 2793, 2113 } }, { -2370, 0x3719, 3415 } }, 329 },
    { { { { -2497, 0, 3246 }, { 3119, 1132, 2399 }, { -897, 3936, -690 } }, { 570, 0x42AE, 1550 } }, 380 },
};

SpriteBatch D_dryfield_night_water_tank_8017F5AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_tank_8017F5BC[46] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -120, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -120, 1125, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, -120, 1112, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -48, -120, 937, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, -120, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -16, -120, 912, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, -120, 1052, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, -120, 1012, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -120, 987, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, -120, 975, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -120, 912, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 16, -120, 925, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 32, -120, 937, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -120, 975, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -120, 987, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 64, -120, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, -120, 1062, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 80, -120, 1112, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -120, 1127, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -120, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -40, 1400, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -40, 1350, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -40, 1287, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -40, 1237, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -40, 1237, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -40, 1287, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -40, 1350, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1437, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 8, 1000, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 8, 1000, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 16, 1000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 16, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, 8, 1000, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 16, 1000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, 16, 1000, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, 40, 875, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 48, 875, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 64, 875, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 64, 875, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 40, 875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 32, 875, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 24, 875, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 16, 875, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 16, 875, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 875, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 0, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_tank_8017F954[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_tank_8017F974[69] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -120, 1125, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -120, 1164, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -120, 1112, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -120, 1077, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -120, 1062, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -120, 1000, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -120, 1125, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -120, 1175, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 80, -120, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, -56, 1500, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -56, 1500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -56, 1375, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -120, 1063, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -120, 1036, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -32, -120, 1020, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -120, 1013, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -120, 1018, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 1030, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -120, 1051, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -32, 1375, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -32, 1250, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -32, -32, 1250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -32, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 0, -32, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -32, 1250, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -32, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 48, -56, 1375, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 56, -56, 1375, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 64, -56, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, -56, 1375, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -24, 750, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -24, 750, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -136, 8, 750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 8, 750, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 8, 750, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 40, 750, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 16, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 750, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 16, 750, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 750, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 16, 750, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 16, 750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 48, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 750, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 56, 750, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -16, 16, 750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, 16, 750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 750, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, 24, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 32, 24, 750, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 56, 750, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, 56, 750, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 16, 750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 72, 32, 750, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, 48, 750, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 64, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 750, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, 0, 750, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, -8, 750, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 32, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 16, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 24, 750, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, 16, 750, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_tank_8017FED8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 1, 0 } },
    { 30, 39, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_tank_8017FEF8[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 96, 625, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 8, 625, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 48, 8, 625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 24, 625, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 24, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 80, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, 56, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 56, 625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 72, 625, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_tank_8017FFAC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_tank_8017FFC4[24] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -48, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -144, -32, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -16, 250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 0, 250, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 0, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 0, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 0, 250, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 250, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 56, 250, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 64, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -64, 64, 250, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 64, 250, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 250, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, 0, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, 64, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 0, 250, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 0, 250, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 0, 250, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 0, 250, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 0, 250, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 64, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 64, 250, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 64, 250, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 64, 250, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_tank_801801A4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_tank_801801BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_water_tank_801801CC[6] = {
    { { .empty = D_dryfield_night_water_tank_8017F5AC }, D_dryfield_night_water_tank_8017F5AC, NULL },
    { { .elements = D_dryfield_night_water_tank_8017F5BC }, D_dryfield_night_water_tank_8017F954, NULL },
    { { .elements = D_dryfield_night_water_tank_8017F974 }, D_dryfield_night_water_tank_8017FED8, NULL },
    { { .elements = D_dryfield_night_water_tank_8017FEF8 }, D_dryfield_night_water_tank_8017FFAC, NULL },
    { { .elements = D_dryfield_night_water_tank_8017FFC4 }, D_dryfield_night_water_tank_801801A4, NULL },
    { { .empty = D_dryfield_night_water_tank_801801BC }, D_dryfield_night_water_tank_801801BC, NULL },
};

WorldCoordLight D_dryfield_night_water_tank_80180214[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 100, 100, 100 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 100, 100, 100 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 100, 100, 100 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -5000, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 413, 823, 823 }, { 0, 0 } },
};

WorldCoordRoomLights D_dryfield_night_water_tank_80180374[1] = {
    { ARRAY_SIZE(D_dryfield_night_water_tank_80180214), D_dryfield_night_water_tank_80180214, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_water_tank_8018038C[4] = {
    { NULL, NULL, NULL, { -864, -0x32D0, 2464, 0 }, { { 0, -1968, -1024, 0 }, { 0, -1968, 1024, 0 }, { 0, 1968, -1024, 0 }, { 0, 1968, 1024, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2217, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -576, -0x3310, 2464, 0 }, { { 0, -1904, 1024, 0 }, { 0, -1904, -1024, 0 }, { 0, 1904, 1024, 0 }, { 0, 1904, -1024, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 768, -0x32D0, -2368, 0 }, { { 0, -1936, 1024, 0 }, { 0, -1936, -1024, 0 }, { 0, 1936, 1024, 0 }, { 0, 1936, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2187, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 544, -0x3330, -2336, 0 }, { { 0, -1904, -1024, 0 }, { 0, -1904, 1024, 0 }, { 0, 1904, -1024, 0 }, { 0, 1904, 1024, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_water_tank_801804BC[8] = {
    { NULL, NULL, NULL, { -2449, -0x2F60, 640, 0 }, { { -728, 0, -256, 0 }, { 336, 0, -584, 0 }, { -624, 0, 680, 0 }, { 1016, 0, 160, 0 } }, { 0, 4096, 0, 0 }, { 401, 0, 4076, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2064, -0x2F20, -832, 0 }, { { -1448, 0, -160, 0 }, { 1088, 0, -680, 0 }, { -1376, 0, 776, 0 }, { 1736, 0, 64, 0 } }, { 0, 4105, 0, 0 }, { 401, 0, 4076, 0 }, 1736, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1620, -0x4040, -1538, 0 }, { { 985, 0, -789, 0 }, { 1431, 0, 313, 0 }, { -780, 0, 951, 0 }, { 316, 0, 1350, 0 } }, { 0, 4107, 0, 0 }, { 401, 0, 4076, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 1, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1488, -0x2F23, 16, 0 }, { { 231, 0, 659, 0 }, { 229, 0, -659, 0 }, { 1395, 0, 739, 0 }, { 1411, 0, -737, 0 } }, { 0, 4103, 0, 0 }, { 4094, 0, 0, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -656, -0x2F21, 2160, 0 }, { { -841, 0, 1091, 0 }, { -619, 0, -1091, 0 }, { 611, 0, 1171, 0 }, { 851, 0, -1169, 0 } }, { 0, 4100, 0, 0 }, { 4094, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1471, -0x2F40, 1471, 0 }, { { -485, 0, 321, 0 }, { 259, 0, -507, 0 }, { 13, 0, 1473, 0 }, { 1559, 0, -134, 0 } }, { 0, 4100, 0, 0 }, { 2750, 0, 3035, 0 }, 1562, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1039, -0x4040, 1063, 0 }, { { 756, 0, -376, 0 }, { -379, 0, 767, 0 }, { 543, 0, -928, 0 }, { -920, 0, 537, 0 } }, { 0, 4102, 0, 0 }, { -3166, 0, -2599, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 576, -0x2F20, -1984, 0 }, { { 445, 0, 788, 0 }, { -788, 0, -221, 0 }, { 1897, 0, 504, 0 }, { -50, 0, -1395, 0 } }, { 0, 4102, 0, 0 }, { 4094, 0, 0, 0 }, 1962, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_water_tank_8018071C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_water_tank_80180728[3] = {
    { 143, 463, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_146300_801427C8 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_water_tank_8018074C[2] = {
    { 143, 463, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_146300_801427C8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_water_tank_80180764[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C228, D_dryfield_night_water_tank_8018071C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017C238, D_dryfield_night_water_tank_80180728 },
    { D_map_dryfield_full_8017C288, D_dryfield_night_water_tank_8018074C },
    { NULL, NULL },
};

WorldCollisionOccluder D_dryfield_night_water_tank_801807CC[2] = {
    { NULL, NULL, { -256, -0x35A0, 0, 0 }, { { -2080, 2560, 0, 0 }, { 2080, 2560, 0, 0 }, { -2080, -2560, 0, 0 }, { 2080, -2560, 0, 0 } }, { 0, 0, 4098, 0 }, 3298, 1, 0 },
    { NULL, NULL, { 0, -0x35A0, 0, 0 }, { { 0, 2560, 1840, 0 }, { 0, 2560, -1840, 0 }, { 0, -2560, 1840, 0 }, { 0, -2560, -1840, 0 } }, { 4113, 0, 0, 0 }, 3145, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_dryfield_night_water_tank_80180844 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_night_water_tank_80180850 = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionFootstepSounds D_dryfield_night_water_tank_8018085C = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionSurfaceProperties D_dryfield_night_water_tank_80180868[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_tank_80180870[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_tank_80180878[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_tank_80180844 },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_tank_80180880[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_tank_80180850 },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_tank_80180888[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_tank_8018085C },
};

WorldCollisionSurfaceProperties* D_dryfield_night_water_tank_80180890[8] = {
    D_dryfield_night_water_tank_80180868,
    D_dryfield_night_water_tank_80180870,
    D_dryfield_night_water_tank_80180868,
    D_dryfield_night_water_tank_80180878,
    D_dryfield_night_water_tank_80180880,
    D_dryfield_night_water_tank_80180888,
    D_dryfield_night_water_tank_80180868,
    D_dryfield_night_water_tank_80180868,
};

AreaApplyRec D_dryfield_night_water_tank_801808B0[2] = {
    { 3, 21, 11, 0 },
    { 255, 0, 0, 0 },
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
        DRYFIELD_NIGHT_WATER_TANK_FOLLOW_UP_CLEAR   = 0,
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
                areaApplySavedUpdates(D_dryfield_night_water_tank_801808B0);
                gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE);
                gameFlagSetNibble(GAME_FLAG_083, 1);
                evsStartScriptWithSkip(D_actor_146300_80137C28, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_146300_80138570);
                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, DRYFIELD_NIGHT_WATER_TANK_FOLLOW_UP_CLEAR);
                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, DRYFIELD_NIGHT_WATER_TANK_STORY_DIALOGUE);
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
    enum { DRYFIELD_NIGHT_WATER_TANK_KEY_ITEM_USE_REJECTED = 0 };

    return DRYFIELD_NIGHT_WATER_TANK_KEY_ITEM_USE_REJECTED;
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

    if ((gGameSession->location.loc.variant != DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) || (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE)) {
        if (request->actionId == DRYFIELD_NIGHT_WATER_TANK_ACTION_VIEW_4) {
            evsStartScript(D_dryfield_night_water_tank_8017DDD8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        if (request->actionId == DRYFIELD_NIGHT_WATER_TANK_ACTION_VIEW_3) {
            evsStartScript(D_dryfield_night_water_tank_8017DEE0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
    }
    if (request->actionId == DRYFIELD_NIGHT_WATER_TANK_ACTION_HANDOVER) {
        roomVariant = gGameSession->location.loc.variant;
        if ((u32)(roomVariant - DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) < (u32)DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT_COUNT) {
            if ((roomVariant != DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) || (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= DRYFIELD_NIGHT_WATER_TANK_HANDOVER_DONE)) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(&D_actor_146300_8013788C, 0, 0, 0);
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

    task->msgTable = D_dryfield_night_water_tank_8017DFE8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_dryfield_night_water_tank_8017EE28, 0, 0, 0);
    if ((u32)(gGameSession->location.loc.variant - DRYFIELD_NIGHT_WATER_TANK_BATTLE_VARIANT) < (u32)DRYFIELD_NIGHT_WATER_TANK_RESTORE_VARIANT_COUNT) {
        _dryfieldNightWaterTankRestoreTankCollision(0);
    }
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_WATER_TANK_BATTLE_VARIANT) {
        taskSpawnFromTable(D_dryfield_night_water_tank_8017E010, 0, 0, 0);
    }
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_WATER_TANK_HANDOVER_VARIANT) {
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

    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_WATER_TANK_ICE_BAG_HELD_VARIANT) {
        inventoryResetIceBagTimer();
    }
}

/// The room task's three states, run from a stack copy by
/// `dryfieldNightWaterTankRoomTask`: the entry tick, the per-frame
/// state, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_tank_8017D5C4 = {
    { _dryfieldNightWaterTankInitializeRoom, _dryfieldNightWaterTankHoldIceBagTimerState, taskKill },
};

void dryfieldNightWaterTankRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_water_tank_8017D5C4;
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

    roomGrid     = &D_dryfield_night_water_tank_8017F4B0;
    tankTemplate = &D_dryfield_night_water_tank_8017E08C;

    // Restores the reserved obstacle prefix without rebuilding the room's cells.
    // Grid arguments are stable pointers, evaluated repeatedly; index is writable
    // s32 scratch. The enclosing constants select two faces/normals and six vertices.
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
