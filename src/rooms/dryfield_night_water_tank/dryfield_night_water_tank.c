#include "rooms/dryfield_night_water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
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

/// Event scripts handed to `func_800E8634`: the one it starts and its skip
/// target.
extern EvsCommand D_actor_146300_80137C28[];
extern EvsCommand D_actor_146300_80138570[];

/// Message table of the night water-tank room, 0x13EE..0x13F1 with the
/// `TASK_MESSAGE_TABLE_END` terminator: `func_dryfield_night_water_tank_8017D714`,
/// `..._8017D70C`, `..._8017D76C` and `..._8017D73C`.
extern TaskMessageEntry D_dryfield_night_water_tank_8017DFE8[];

/// Task descriptor tables spawned by the room entry task, each one entry and
/// the 0xFFFF terminator: `8017E010` runs the exit task
/// `func_dryfield_night_water_tank_8017D5D0`, `8017EE28` the tank model's
/// update `waterTankSwayTask`.
extern TaskDesc D_dryfield_night_water_tank_8017E010[];
extern TaskDesc D_dryfield_night_water_tank_8017EE28[];

/// Absolute import: 0x8013224C has no name in main or gameplay, so the call is
/// emitted against bare address, the way the other rooms' `func_8013...` are.
extern void func_actor_146300_8013224C(void);

/// Absolute import: the shared room script descriptor 0x8013788C, spawned by
/// entry 0 in the handler below.
extern TaskDesc D_actor_146300_8013788C;

/// Model/lighting records the handler below toggles on message 3 and 4.
extern EvsCommand D_dryfield_night_water_tank_8017DDD8[];
extern EvsCommand D_dryfield_night_water_tank_8017DEE0[];

/// The tank's wobble spring: `8017EE40` is the accumulated yaw handed to
/// `gfxRotMatrixY` (`>> 8`), `8017EE44` its velocity, `8017EE48` the yaw it
/// steps toward and `8017EE4C` the target that step chases.

static void func_dryfield_night_water_tank_8017D9DC(s32 arg0);

extern WorldCollisionGrid     D_dryfield_night_water_tank_8017F4B0;
extern WorldCollisionOccluder D_dryfield_night_water_tank_801807CC[2];
extern WorldCollisionTrigger  D_dryfield_night_water_tank_8018038C[4];
extern WorldCollisionTrigger  D_dryfield_night_water_tank_801804BC[8];
extern WorldCoordRoomLights   D_dryfield_night_water_tank_80180374[1];
static TmdSource              _gDryfieldNightWaterTankModel00FF8;

s32  func_dryfield_night_water_tank_8017D70C(Task*, s32, s32, s32);
s32  func_dryfield_night_water_tank_8017D714(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_night_water_tank_8017D73C(Task*, s32, s32, s32);
s32  func_dryfield_night_water_tank_8017D76C(Task*, s32, RoomEventMsg*, s32);
void func_dryfield_night_water_tank_8017D5D0(Task*);

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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_water_tank_8017D714 },
    { 5105, func_dryfield_night_water_tank_8017D70C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_water_tank_8017D76C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_water_tank_8017D73C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_water_tank_8017E010[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_night_water_tank_8017D5D0, { .value = 0 } },
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

static void func_dryfield_night_water_tank_8017D870(Task* task);
static void func_dryfield_night_water_tank_8017D94C(Task* task);

/// Exit task of the night water-tank room, in the shape the other rooms' wait
/// tasks have: three states on `Task::state`. State 0 raises bit 0x80 of
/// `gGameSession::flowFlags` once `gSceneCombatState` has reached 1, then advances;
/// state 1 advances to 2 as soon as the halfword at `gSceneCombatState.battleRefs` clears; state
/// 2 runs the room's ending -- apply the area records, set flags 0x7B, 0x83,
/// 0x155 and 3, spawn the script `func_800E8634` is handed -- and kills the
/// task, or, while `gGameSession::battleResetPending` is still clear, just ticks
/// `Task::killCountdown` down and waits for another frame.
void func_dryfield_night_water_tank_8017D5D0(Task* task)
{
    SVECTOR3 unused; // never referenced; only reserves the frame slot the ROM has

    switch (task->state) {
        case 0:
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                gGameSession->flowFlags = gGameSession->flowFlags | GAME_SESSION_FLOW_REEQUIP_WEAPON;
                task->state             = task->state + 1;
                return;
            }
            return;
        case 1:
            if (gSceneCombatState.battleRefs == 0) {
                task->state = 2;
                return;
            }
            break;
        case 2:
            if (gGameSession->battleResetPending != 0) {
                Gp_ApplyAreaRecs(D_dryfield_night_water_tank_801808B0);
                gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, 2);
                gameFlagSetNibble(GAME_FLAG_083, 1);
                func_800E8634(D_actor_146300_80137C28, 0, D_actor_146300_80138570);
                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0xE);
                taskKill(task);
                return;
            }
            task->killCountdown = task->killCountdown - 1;
            break;
    }
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_water_tank_8017D70C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record it is handed onto the outgoing one and answers 1.
s32 func_dryfield_night_water_tank_8017D714(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_night_water_tank_8017D73C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0xE) {
        Gp_StartCapSlot(0xE, 1, 1);
    }
    return 0;
}

s32 func_dryfield_night_water_tank_8017D76C(Task* arg0, s32 arg1, RoomEventMsg* in, s32 arg3)
{
    u8 temp_v1;

    if ((gGameSession->location.loc.variant != 0xA) || (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= 2)) {
        if (in->warp == 3) {
            func_800E8614(D_dryfield_night_water_tank_8017DDD8, 0);
        }
        if (in->warp == 4) {
            func_800E8614(D_dryfield_night_water_tank_8017DEE0, 0);
        }
    }
    if (in->warp == 5) {
        temp_v1 = gGameSession->location.loc.variant;
        if ((u32)(temp_v1 - 0xA) < 2U) {
            if ((temp_v1 != 0xA) || (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= 2)) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_actor_146300_8013788C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(0x17);
            }
        }
    }
    return 0;
}

/// Room entry task tick, the shape the other dryfield rooms' entry tasks have:
/// publish the message table the room's handlers hang off (0x13EE..0x13F1) in
/// `Task::msgTable`, claim game pointer slot 7, spawn the tank model's task
/// from `8017EE28`, then branch on the visit sub-id
/// (`gGameSession::location.loc.variant`).
///
/// Sub-ids 0xA and 0xB -- the two visits that reach this room -- both run the
/// prop updater `func_dryfield_night_water_tank_8017D9DC` on its zero argument;
/// 0xA additionally spawns the exit task from `8017E010`, and 0xB, the visit
/// the room is announced into, hands over to `func_actor_146300_8013224C` instead. The state
/// advances on every path.
static void func_dryfield_night_water_tank_8017D870(Task* task)
{
    task->msgTable = D_dryfield_night_water_tank_8017DFE8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    Task_SpawnFromTable(D_dryfield_night_water_tank_8017EE28, 0, 0, 0);
    if ((u32)(gGameSession->location.loc.variant - 0xA) < 2U) {
        func_dryfield_night_water_tank_8017D9DC(0);
    }
    if (gGameSession->location.loc.variant == 0xA) {
        Task_SpawnFromTable(D_dryfield_night_water_tank_8017E010, 0, 0, 0);
    }
    if (gGameSession->location.loc.variant == 0xB) {
        func_actor_146300_8013224C();
    }
    task->state = task->state + 1;
}

/// The room task's second state, run every frame after the entry tick: marks
/// the play time while the visit sub-id is 0xB.
static void func_dryfield_night_water_tank_8017D94C(Task* task)
{
    if (gGameSession->location.loc.variant == 0xB) {
        Gp_MarkPlayTime();
    }
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_night_water_tank_8017D984`: the entry tick, the per-frame
/// state, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_tank_8017D5C4 = {
    { func_dryfield_night_water_tank_8017D870, func_dryfield_night_water_tank_8017D94C, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_water_tank_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_tank_8017D5C4;
    sp.funcs[task->state](task);
}

/// Restores the room's layout lists from their template, then offsets the six
/// `field_8` coordinates by (0, 0, -0xC8) when `arg0` is non-zero.
static void func_dryfield_night_water_tank_8017D9DC(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_dryfield_night_water_tank_8017F4B0;
    src = &D_dryfield_night_water_tank_8017E08C;

    for (i = 0; i < 2; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }

    for (i = 0; i < 6; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
        d.vz = 0;
    } else {
        d.vx = 0;
        d.vy = 0;
        d.vz = -0xC8;
    }

    for (i = 0; i < 6; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}

#include "../../shared/water_tank_sway_task.inc.c"

void func_dryfield_night_water_tank_8017DD8C(Task* unused)
{
}
