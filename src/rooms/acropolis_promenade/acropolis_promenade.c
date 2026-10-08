#include "rooms/acropolis_promenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/bridge_model.h"
#include "../../shared/acropolis_glows.h"

extern EvsCommand   D_acropolis_promenade_80180F00[];
extern EvsCommand   D_acropolis_promenade_80181068[];
extern s32          D_acropolis_promenade_80181140;
extern s32          D_acropolis_promenade_80181144;
extern RoomEventMsg D_acropolis_promenade_801862D0;
extern Task*        D_acropolis_promenade_801862D8;

/// The room task's message table, installed by its setup state: it answers
/// room-transition requests, room commands, room actions, key-item use and
/// room sound commands, and returns zero for every other message.
extern TaskMessageEntry D_acropolis_promenade_80180E74[];
/// Spawn table holding the prop task, terminated by an all-ones `flags`.
extern TaskDesc D_acropolis_promenade_80180EA4[];
/// The room's task table: the streamed-scene task is entry 2, and entries 3
/// and 4 are the fade tasks it spawns.
extern TaskDesc D_acropolis_promenade_80181148[];

/// Per-frame path the promenade's streamed scene walks the player's matrix
/// along, indexed backwards by `0x45 - gCdCmdQueue.movieFrame`, plus the script
/// pair the scene runs.
extern SVECTOR                   D_acropolis_promenade_80181184[];
extern PadScriptCmd              D_acropolis_promenade_80186224[6];
extern PadScriptVibrationSegment D_acropolis_promenade_8018623C[2];

extern EffectUnitQuadCorner D_acropolis_promenade_80181AE4[];

extern u16 D_acropolis_promenade_80181B74;
extern u16 D_acropolis_promenade_80181B76;
extern u16 D_acropolis_promenade_80181B78[];

static void func_acropolis_promenade_8017D9E0(Task* arg0);
static void _acropolisPromenadeUpdateBridgeVisibility(Task* task);

void        func_acropolis_promenade_8017DB9C(Task*);
static void _acropolisPromenadeMovieSkipFadeOutTask(Task* task);
static void _acropolisPromenadeSceneFadeInTask(Task* task);

extern WorldCollisionGrid    D_acropolis_promenade_801823DC[1];
extern WorldCollisionGrid    D_acropolis_promenade_80182BD0[1];
extern WorldCollisionTrigger D_acropolis_promenade_80182BF4[6];
extern WorldCollisionTrigger D_acropolis_promenade_80182DBC[6];
extern WorldCoordRoomLights  D_acropolis_promenade_80183A08[1];

extern AnimationPlayRequest D_acropolis_promenade_80180EBC;

extern SpriteBatch  D_acropolis_promenade_80183A20[2];
extern SpriteBatch  D_acropolis_promenade_80183FF8[11];
extern SpriteBatch  D_acropolis_promenade_801841A4[5];
extern SpriteBatch  D_acropolis_promenade_8018462C[9];
extern SpriteBatch  D_acropolis_promenade_80184CF0[9];
extern SpriteBatch  D_acropolis_promenade_80185224[9];
extern SpriteBatch  D_acropolis_promenade_801854EC[8];
extern SpriteBatch  D_acropolis_promenade_801858C4[9];
extern SpriteBatch  D_acropolis_promenade_80185F24[10];
extern SpriteBatch  D_acropolis_promenade_80185F74[2];
extern SpriteBatch  D_acropolis_promenade_80185F84[2];
extern SpriteBatch  D_acropolis_promenade_80185F94[2];
extern SpriteSource D_acropolis_promenade_80183A30[74];
extern SpriteSource D_acropolis_promenade_80184050[17];
extern SpriteSource D_acropolis_promenade_801841CC[56];
extern SpriteSource D_acropolis_promenade_80184674[83];
extern SpriteSource D_acropolis_promenade_80184D38[63];
extern SpriteSource D_acropolis_promenade_8018526C[32];
extern SpriteSource D_acropolis_promenade_8018552C[46];
extern SpriteSource D_acropolis_promenade_8018590C[78];
s32                 func_acropolis_promenade_8017D70C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32          _acropolisPromenadeRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);
s32                 func_acropolis_promenade_8017D8E0(Task*, s32, s32, s32);
static s32          _acropolisPromenadeHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);
static s32          _acropolisPromenadeIgnoreRoomAction(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
void                func_acropolis_promenade_8017D988(Task*);

/// Room-local binding of the key-item-use request; the first payload is an item ID.
enum { ACROPOLIS_PROMENADE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

static TmdBone _gAcropolisPromenadeAcropolisBridgeModel0AD9CSkeleton[1] = {
#include "assets/acropolis_bridge_model_0AD9C_skeleton.inc"
};

static u32 _gAcropolisPromenadeAcropolisBridgeModel0AD9CPartVerts[1] = {
#include "assets/acropolis_bridge_model_0AD9C_partVerts.inc"
};

static SVECTOR _gAcropolisPromenadeAcropolisBridgeModel0AD9CVerts[171] = {
#include "assets/acropolis_bridge_model_0AD9C_verts.inc"
};

static u32 _gAcropolisPromenadeAcropolisBridgeModel0AD9CStream[691] = {
#include "assets/acropolis_bridge_model_0AD9C_stream.inc"
};

static TmdSource _gAcropolisPromenadeAcropolisBridgeModel0AD9C = {
    0,
    5480,
    0,
    1,
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CPartVerts,
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CVerts,
    &_gAcropolisPromenadeAcropolisBridgeModel0AD9CVerts[171],
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CSkeleton,
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CStream,
};

TaskMessageEntry D_acropolis_promenade_80180E74[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_promenade_8017D70C },
    { ROOM_MESSAGE_COMMAND, func_acropolis_promenade_8017D8E0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisPromenadeIgnoreRoomAction },
    { ACROPOLIS_PROMENADE_MESSAGE_USE_KEY_ITEM, _acropolisPromenadeRefuseKeyItem },
    { ROOM_MESSAGE_SOUND, _acropolisPromenadeHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_promenade_80180EA4[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_acropolis_promenade_8017D988, { .model = &_gAcropolisPromenadeAcropolisBridgeModel0AD9C } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_acropolis_promenade_80180EBC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_promenade_80180ED0[2] = {
    { { -1454, 171, -1843, 0 }, { 0, 2048, 0, 0 } },
    { { -999, 171, -2799, 0 }, { 0, 1324, 0, 0 } },
};

EvsCommand D_acropolis_promenade_80180F00[15] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_promenade_80180EBC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x510B000C }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x510B000F }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_promenade_80181068[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_acropolis_promenade_80181140 = 0;

s32 D_acropolis_promenade_80181144 = 0;

TaskDesc D_acropolis_promenade_80181148[5] = {
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_promenade_8017DB9C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeMovieSkipFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeSceneFadeInTask, { .value = 0 } },
};

SVECTOR D_acropolis_promenade_80181184[300] = {
#include "assets/acropolis_promenade_path_03BC4.inc"
};

EffectUnitQuadCorner D_acropolis_promenade_80181AE4[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

// Indexed views below share one contiguous table.
SVECTOR D_acropolis_promenade_80181AF4[3] = {
    { -4700, -1000, -620, 0 },
    { -3920, -1494, -3450, 0 },
    { -6900, -1494, -3564, 0 },
};

SVECTOR D_acropolis_promenade_80181B0C[1] = {
    { -6900, 300, -2543, 0 },
};

SVECTOR D_acropolis_promenade_80181B14[12] = {
    { 810, -3000, -3720, 0 },
    { 650, -3000, -4000, 0 },
    { 810, -3000, -4270, 0 },
    { -2470, -2050, -740, 0 },
    { -2520, -2050, -3120, 0 },
    { -16000, -1700, -1330, 0 },
    { -16000, -1700, -2700, 0 },
    { 1680, -1700, 10080, 0 },
    { 1680, -1700, 8430, 0 },
    { 1640, -360, 7020, 0 },
    { -2150, -360, 8460, 0 },
    { -3400, -370, 2300, 0 },
};

u16 D_acropolis_promenade_80181B74 = 32;

u16 D_acropolis_promenade_80181B76 = 32;

u16 D_acropolis_promenade_80181B78[12] = {
    442,
    440,
    440,
    401,
    401,
    18,
    18,
    478,
    510,
    64,
    512,
    32,
};

WorldCollisionRoomResources D_acropolis_promenade_80181B90[2] = {
    { D_acropolis_promenade_801823DC, D_acropolis_promenade_80182BF4, D_acropolis_promenade_80182DBC, NULL },
    { D_acropolis_promenade_80182BD0, D_acropolis_promenade_80182BF4, D_acropolis_promenade_80182DBC, NULL },
};

u8 D_acropolis_promenade_80181BB0[16] = {
    1,
    2,
    3,
    8,
    9,
    6,
    7,
    4,
    5,
    10,
    11,
    12,
    13,
    0,
    0,
    0,
};

u8* D_acropolis_promenade_80181BC0[2] = {
    gViewIdentityMap,
    D_acropolis_promenade_80181BB0,
};

ViewCount D_acropolis_promenade_80181BC8[2] = { 13, 13 };

WorldCoordRoomLighting D_acropolis_promenade_80181BCC[2] = {
    { D_acropolis_promenade_80183A08, NULL },
    { D_acropolis_promenade_80183A08, NULL },
};

DirectionWarpEntry D_acropolis_promenade_80181BDC[5] = {
    { { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 373, 42, -4230 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 373, 42, -4230 }, { 0, 0, 0, 0 }, 0x510B0006, 0x510B0005, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 493 },
    { { { .word = 1024 }, -0x3D4E, 0, -2098 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -0x3D4E, 0, -2098 }, { 0, 0, 0, 0 }, 0x510B0008, 0x510B0007, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -2197, 44, -1977 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2197, 44, -1977 }, { 0, 0, 0, 0 }, 0x510B000E, 0x510B000D, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 491 },
};

static SVECTOR _gAcropolisPromenadeCollision04E1CNormals[30] = {
#include "assets/acropolis_promenade_collision_04E1C_normals.inc"
};

static SVECTOR _gAcropolisPromenadeCollision04E1CVerts[93] = {
#include "assets/acropolis_promenade_collision_04E1C_verts.inc"
};

static WorldCollisionGridFace _gAcropolisPromenadeCollision04E1CFaces[38] = {
#include "assets/acropolis_promenade_collision_04E1C_faces.inc"
};

static s16 _gAcropolisPromenadeCollision04E1CCells[144] = {
#include "assets/acropolis_promenade_collision_04E1C_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisPromenadeCollision04E1CCells[i])
static s16* _gAcropolisPromenadeCollision04E1CTable[10] = {
#include "assets/acropolis_promenade_collision_04E1C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_promenade_801823DC[1] = {
    { NULL, _gAcropolisPromenadeCollision04E1CNormals, _gAcropolisPromenadeCollision04E1CVerts, _gAcropolisPromenadeCollision04E1CFaces, _gAcropolisPromenadeCollision04E1CTable, 2780, 8590, 2, 5, 4000, 38 },
};

static SVECTOR _gAcropolisPromenadeCollision05610Normals[30] = {
#include "assets/acropolis_promenade_collision_05610_normals.inc"
};

static SVECTOR _gAcropolisPromenadeCollision05610Verts[103] = {
#include "assets/acropolis_promenade_collision_05610_verts.inc"
};

static WorldCollisionGridFace _gAcropolisPromenadeCollision05610Faces[46] = {
#include "assets/acropolis_promenade_collision_05610_faces.inc"
};

static s16 _gAcropolisPromenadeCollision05610Cells[172] = {
#include "assets/acropolis_promenade_collision_05610_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisPromenadeCollision05610Cells[i])
static s16* _gAcropolisPromenadeCollision05610Table[10] = {
#include "assets/acropolis_promenade_collision_05610_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_promenade_80182BD0[1] = {
    { NULL, _gAcropolisPromenadeCollision05610Normals, _gAcropolisPromenadeCollision05610Verts, _gAcropolisPromenadeCollision05610Faces, _gAcropolisPromenadeCollision05610Table, 2780, 8590, 2, 5, 4000, 46 },
};

WorldCollisionTrigger D_acropolis_promenade_80182BF4[6] = {
    { NULL, NULL, NULL, { -802, -2544, 510, 0 }, { { -1820, -4175, 362, 0 }, { 1820, -4209, -362, 0 }, { -1820, 4209, 362, 0 }, { 1820, 4175, -362, 0 } }, { -800, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -804, -2432, 91, 0 }, { { 1820, -4175, -361, 0 }, { -1820, -4209, 362, 0 }, { 1820, 4209, -361, 0 }, { -1820, 4175, 362, 0 } }, { 797, 0, 4017, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -288, -2272, 4160, 0 }, { { -1856, -4175, 0, 0 }, { 1856, -4209, 0, 0 }, { -1856, 4209, 0, 0 }, { 1856, 4175, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -256, -2432, 3681, 0 }, { { 1856, -4175, 0, 0 }, { -1856, -4209, 0, 0 }, { 1856, 4209, 0, 0 }, { -1856, 4175, 0, 0 } }, { 0, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -997, -2624, -5124, 0 }, { { 1843, -4175, -185, 0 }, { -1850, -4209, 179, 0 }, { 1843, 4209, -185, 0 }, { -1850, 4175, 179, 0 } }, { 401, 0, 4076, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -999, -2560, -4839, 0 }, { { -1854, -4175, 90, 0 }, { 1851, -4209, -93, 0 }, { -1854, 4209, 90, 0 }, { 1851, 4175, -93, 0 } }, { -203, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_promenade_80182DBC[6] = {
    { NULL, NULL, NULL, { 989, -112, 9028, 0 }, { { -1024, 0, -1024, 0 }, { 1024, 0, -1024, 0 }, { -1024, 0, 1024, 0 }, { 1024, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1448, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 67, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 752, -112, -3903, 0 }, { { -496, 0, -1215, 0 }, { 496, 0, -1215, 0 }, { -496, 0, 1216, 0 }, { 496, 0, 1216, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1408, -105, 7680, 0 }, { { -1024, 0, -1088, 0 }, { 1024, 0, -1088, 0 }, { -1024, 0, 1088, 0 }, { 1024, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, 201, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2432, -96, -8512, 0 }, { { -1024, 0, -1024, 0 }, { 1568, 0, -1024, 0 }, { -1024, 0, 1472, 0 }, { 1568, 0, 1472, 0 } }, { 0, 4097, 0, 0 }, { 600, 0, 4051, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2208, -128, -2048, 0 }, { { -576, 0, -1024, 0 }, { 576, 0, -1024, 0 }, { -576, 0, 1024, 0 }, { 576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4094, 0, -1, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1152, -64, -2608, 0 }, { { -2336, 0, -272, 0 }, { 2336, 0, -752, 0 }, { -2336, 0, 752, 0 }, { 2336, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 6, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_promenade_80182F84[2] = {
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_301100_80177400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_promenade_80182F9C[3] = {
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_200700_80150C80 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_promenade_80182FC0[3] = {
    { 55, 55, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105500_8013A8DC },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_promenade_80182FE4[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_promenade_80182FFC[3] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_promenade_80183020[17] = {
    { NULL, NULL },
    { D_map_akropolis_8017B82C, D_acropolis_promenade_80182F84 },
    { D_map_akropolis_8017B85C, D_acropolis_promenade_80182F9C },
    { D_map_akropolis_8017B8DC, D_acropolis_promenade_80182FC0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B98C, D_acropolis_promenade_80182FE4 },
    { D_map_akropolis_8017B9AC, D_acropolis_promenade_80182FFC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Authored point lights for model shading in every promenade view.
///
/// Positions and falloff radii use world units; RGB intensities have 12 fractional
/// bits (`ONE` is full strength). The room overlay owns this writable storage:
/// coordinate updates set its view parent and cached transforms, and lighting
/// queries overwrite attenuation. Borrowed pointers expire when the room unloads.
static WorldCoordPointLight _gAcropolisPromenadePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1367, -507, 6799 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, 3686, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -335, -2500, 3516 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 0,
        .outer = 0x2B5C,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1970, -507, 8207 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, 3686, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1342, -2000, 0x2854 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 200,
        .outer = 9000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -2500, -3995 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 500,
        .outer = 0x3714,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2333, -2000, -763 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 3276, 2867 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2333, -2000, -3192 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 3276, 2867 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6854, -1500, -3373 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 3955,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9907, -1500, -3373 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x324D, -1500, -3373 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x33A3, -1500, -719 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2808, -1500, -719 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7195, -1500, -719 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3604, 2457, 2457 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x38AB, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2D1E, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5464, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 5179,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8422, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8422, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5464, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 501,
        .outer = 3961,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2D1E, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x38AB, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3D7E, 2000, -3035 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, 3276 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3D7E, 2000, -994 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, 3276 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -2500, -8518 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 500,
        .outer = 0x3714,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4630, -1500, -3219 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 0,
        .outer = 2,
    },
};

WorldCoordRoomLights D_acropolis_promenade_80183A08[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisPromenadePointLights), _gAcropolisPromenadePointLights, 0, NULL },
};

SpriteBatch D_acropolis_promenade_80183A20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80183A30[74] = {
    { 141, 0x3FC0, { .fields = { 72, 24 } }, -160, 96, 450, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -160, -32, 450, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 24, 450, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -120, 427, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -32, -112, 435, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -152, -120, 875, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -120, -104, 951, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, -80, 988, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -80, 1148, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -72, 1096, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -56, 1040, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -48, 994, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, -72, -48, 963, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -40, -56, 950, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -40, -120, 950, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -32, -104, 950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -72, 950, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -80, -24, 973, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -144, -24, 875, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 875, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -104, 8, 925, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, 8, 925, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 32, 925, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -64, 56, 925, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 875, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, 32, 875, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 48, 875, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -112, 56, 925, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, -40, 875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -120, 487, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -48, 950, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -160, -8, 462, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -112, 8, 462, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -128, 64, 462, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 462, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, -152, 96, 450, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -160, -112, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -144, -112, 450, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -136, -120, 450, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -88, -120, 437, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, -120, 625, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -120, 750, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -32, -120, 875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -24, -96, 925, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -88, 925, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 80, 465, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, -32, 950, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -96, 0, 950, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 509, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 524, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -96, -120, 974, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -104, -24, 962, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 128 } }, 104, -40, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -32, 1750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 80, -8, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -160, -120, 950, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 56 } }, -144, -120, 950, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 72 } }, -128, -120, 950, { .fields = { 8, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 88 } }, -104, -120, 950, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 80 } }, -40, -120, 950, { .fields = { 104, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 72 } }, -160, -32, 876, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 80 } }, -144, -24, 876, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 88 } }, -128, -8, 876, { .fields = { 8, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 72 } }, -104, 8, 926, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 8 } }, -8, -48, 925, { .fields = { 64, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 16, 32 } }, -160, -120, 500, { .fields = { 104, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 40, 24 } }, -144, -120, 500, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 40, 16 } }, -104, -120, 500, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 32, 40 } }, -64, -120, 750, { .fields = { 120, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 80 } }, -32, -120, 875, { .fields = { 80, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 104 } }, -160, -8, 463, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 24, 96 } }, -128, -8, 463, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 64 } }, -104, 8, 463, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_promenade_80183FF8[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 2, 0, 0, { 5, 0 } },
    { 5, 25, 0, 0, { 4, 0 } },
    { 30, 16, 0, 0, { 6, 0 } },
    { 46, 3, 0, 0, { 1, 0 } },
    { 49, 4, 0, 0, { 7, 0 } },
    { 53, 3, 0, 0, { 3, 0 } },
    { 56, 9, 0, 0, { 8, 0 } },
    { 65, 9, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80184050[17] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -48, 1525, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -40, 1568, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 120 } }, -160, -120, 1450, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 104 } }, -160, 0, 1450, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -112, -120, 1450, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -112, 0, 1450, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, 0, 1525, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, 0, 1558, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -120, 1083, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 80, -120, 988, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 40, 930, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 40, 925, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 80, -96, 968, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 0, 925, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 24, 2499, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 24, 2498, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 24, 2449, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_801841A4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 6, 0, 0, { 2, 0 } },
    { 14, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_801841CC[56] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 64, 1067, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -160, -112, 587, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -152, -112, 659, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -144, -112, 731, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -136, -112, 800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -128, -112, 877, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -120, -112, 954, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -112, -112, 1112, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -56, 1112, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 24, 591, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 24, 664, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 707, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 24, 800, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 24, 882, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, 24, 959, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 1087, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 1250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, -112, -88, 1314, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -96, -72, 1826, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -88, -40, 2217, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -72, 1465, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -32, 2776, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -32, 2844, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -32, 2932, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -32, 2842, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 16, -56, 1500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 24, -72, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1400, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 32, 720, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, 32, 715, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 32, 707, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 104, 609, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -120, 595, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 152, 8, 583, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, 24, 741, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 16, 2404, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -48, 2192, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -16, 2214, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 32, 1700, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 40, 1600, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 48, 1450, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 56, 1300, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 72, 1075, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 80, 975, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, 88, 862, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 96, 787, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 104, 750, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 112, 700, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, 64, 1175, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 16, 1250, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 32, 1250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 40, 2200, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, 8, 2200, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 16, 2200, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 16, 2200, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 16, 2200, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_8018462C[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 9, 0, 0, { 4, 0 } },
    { 25, 10, 0, 0, { 1, 0 } },
    { 35, 3, 0, 0, { 5, 0 } },
    { 38, 11, 0, 0, { 0, 0 } },
    { 49, 2, 0, 0, { 6, 0 } },
    { 51, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80184674[83] = {
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, -80, 3667, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -80, 3667, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -80, 3667, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -152, 80, 625, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -112, 88, 412, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -80, 88, 609, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -56, 80, 618, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, 72, 637, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, 8, 56, 631, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 625, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 56 } }, 48, 64, 625, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 96, 48, 625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 128, 40, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 936, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 56, 940, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 830, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 64, 854, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 64, 911, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 64, 972, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 64, 882, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 795, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 821, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 72, 924, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 72, 849, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 72, 851, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 72, 865, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 72, 625, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 96, 625, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 112, 625, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, 112, 625, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 625, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 80, 685, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 88, 725, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -56, 88, 739, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 850, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -24, 1500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 144 } }, 40, -112, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 2200, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -8, 1975, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 0, 1752, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -32, 1975, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -144, -40, 1975, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -96, -48, 1760, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -40, 3311, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, -32, 2853, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, 80, 974, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 88, 925, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 96, 900, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 104, 875, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 32, 1272, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 24, 1355, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 1467, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 8, 1622, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 40, 1170, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1094, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 56, 1036, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 64, 1006, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 975, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 2750, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, -64, 1100, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -160, 8, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -152, 40, 1100, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -120, 301, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -72, -64, 3148, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -80, -96, 2375, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -88, -104, 1906, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -96, -104, 1375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -104, -104, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -144, -104, 855, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -152, -104, 755, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -160, -104, 855, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -112, -104, 1031, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, -80, 1025, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -128, 0, 811, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -120, -8, 850, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -128, -80, 850, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -136, 8, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -88, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -112, -8, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -152, 48, 1025, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -160, 56, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 64, 1000, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 875, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_80184CF0[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 22, 0, 0, { 4, 0 } },
    { 35, 2, 0, 0, { 1, 0 } },
    { 37, 22, 0, 0, { 5, 0 } },
    { 59, 3, 0, 0, { 0, 0 } },
    { 62, 17, 0, 0, { 6, 0 } },
    { 79, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80184D38[63] = {
    { 143, 0x3FC0, { .fields = { 24, 176 } }, -160, -120, 1475, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -136, -56, 1500, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 96 } }, -120, -48, 1525, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, -96, -40, 1525, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 8, -64, 1525, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 40, -72, 1525, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -48, 1525, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 72, -24, 1525, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 88, -48, 1525, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 96, -24, 1525, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 112, -48, 1525, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 120, -32, 1525, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 112 } }, -40, -64, 1525, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -72, -8, 750, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -48, 8, 750, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -24, 40, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 24, 8, 750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 64 } }, 56, -48, 1150, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -24, 1150, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, -48, 1150, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 64, 16, 1125, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, -104, -8, 750, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 24, 788, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 32 } }, 80, -120, 461, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 32 } }, 64, -88, 457, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, 72, -56, 465, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 32 } }, 88, -32, 472, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 104, 0, 452, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 96 } }, 88, 24, 416, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 24, 825, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -72, 40, 825, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -80, 64, 825, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, 72, 825, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 72, 825, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -32, 80, 825, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 32, 825, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 24, 80, 825, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 96, 825, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 920, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, 40, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, 40, 800, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 88, 889, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -48, 96, 857, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 825, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, 40, 798, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, 96, 818, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 32, 40, 868, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 112, 800, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 56, 700, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -80, 112, 869, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, 56, 775, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 80, 739, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 112, 450, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 88, 609, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -152, 88, 638, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -144, 88, 639, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 80, 677, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 80, 707, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 72, 780, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 829, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, 96, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 80, 580, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, 80, 671, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_80185224[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 9, 0, 0, { 4, 0 } },
    { 22, 7, 0, 0, { 1, 0 } },
    { 29, 9, 0, 0, { 5, 0 } },
    { 38, 9, 0, 0, { 0, 0 } },
    { 47, 4, 0, 0, { 6, 0 } },
    { 51, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_8018526C[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 550, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, -40, -120, 550, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -112, 550, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -72, -96, 550, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 112 } }, -56, -120, 500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 104 } }, -8, -120, 500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -72, -80, 500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -56, 500, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 72, -104, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, 24, -72, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 48 } }, 64, -56, 750, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 120 } }, -160, -120, 475, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, 40, 875, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 120 } }, -160, 0, 875, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, 96, 137, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -88, 8, 750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 48, 8, 1096, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, 8, 1050, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, 16, 1012, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 72, 24, 975, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 950, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 88, 32, 935, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, 32, 925, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 112, 24, 913, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, 24, 925, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 144, 24, 950, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 72 } }, -72, -80, 487, { .fields = { 16, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 80, 64 } }, -8, -80, 487, { .fields = { 0, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 56 } }, 72, -80, 487, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 88, 40 } }, -80, -120, 487, { .fields = { 56, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 80, 40 } }, 8, -120, 487, { .fields = { 80, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_promenade_801854EC[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 5, 0, 0, { 0, 0 } },
    { 9, 2, 0, 0, { 5, 0 } },
    { 11, 5, 0, 0, { 1, 0 } },
    { 16, 11, 0, 0, { 4, 0 } },
    { 27, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_8018552C[46] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -48, 1125, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -96, 250, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -152, -104, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -144, -96, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -136, -96, 775, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -128, -88, 850, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -120, -96, 875, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -112, -96, 1057, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 168 } }, -104, -96, 1087, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 40, 1087, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -48, 1125, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -96, 1125, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -56, 1600, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -112, -64, 1600, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, -96, -64, 1600, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, -64, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -32, 2500, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -56, -24, 3050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, -24, 3075, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -24, 3100, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 152, -120, 700, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 144, -32, 700, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 144, 32, 700, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, 120, 32, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 24, -56, 2000, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 16, 2025, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 40, 1775, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 40, 1775, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 48, 1512, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 48, 1512, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 56, 1275, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 56, 1275, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 64, 1175, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 104, 750, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 112, 700, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 64, 994, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -128, 72, 1075, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, 80, 975, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -144, 88, 862, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -152, 96, 787, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -160, 16, 1087, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 32, 1087, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 32, 1087, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -136, -56, 1600, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -64, 1750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 112 } }, -96, -64, 1750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_801858C4[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 8, 0, 0, { 4, 0 } },
    { 20, 4, 0, 0, { 1, 0 } },
    { 24, 2, 0, 0, { 5, 0 } },
    { 26, 14, 0, 0, { 0, 0 } },
    { 40, 3, 0, 0, { 6, 0 } },
    { 43, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_8018590C[78] = {
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -152, 80, 600, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -112, 88, 600, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -56, 80, 600, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, 72, 600, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 8, 56, 600, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 600, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 56 } }, 48, 64, 600, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 80 } }, 96, 40, 600, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 936, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 56, 940, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 830, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 64, 854, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 64, 911, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 64, 972, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 64, 882, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 795, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 821, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 72, 924, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 72, 849, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 72, 851, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 72, 865, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -112, 72, 625, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, 80, 612, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -72, 80, 612, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 104, 612, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, 104, 612, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 88, 596, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 88, 792, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 80, 849, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 80, 865, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 661, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 40, -112, 1350, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 32, -24, 1375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, -48, 3275, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -144, -40, 3250, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -32, 3075, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -24, 2600, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -16, 2275, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -8, 1975, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, 0, 1800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 8, 1752, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 16, 1622, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -152, 88, 974, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -152, 96, 925, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -152, 104, 900, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 112, 875, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 24, 1467, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 1355, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 40, 1272, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1170, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 56, 1094, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 64, 1036, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 1006, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 80, 975, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -160, 8, 1125, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 72 } }, -160, -64, 1125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 48, 900, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -160, 72, 850, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 800, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -80, 3825, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -80, 3825, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -80, 3800, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, -96, 3000, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -96, 2500, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -112, -96, 2500, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -96, 2500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -72, 1665, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -24, 1865, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -88, -104, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -96, -104, 1625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -104, -104, 1550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -112, -104, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -120, -80, 1300, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -128, -80, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 208 } }, -136, -88, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -144, -88, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -152, -88, 900, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -160, -88, 800, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_80185F24[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 6, 0 } },
    { 8, 23, 0, 0, { 1, 0 } },
    { 31, 2, 0, 0, { 4, 0 } },
    { 33, 21, 0, 0, { 0, 0 } },
    { 54, 2, 0, 0, { 5, 0 } },
    { 56, 3, 0, 0, { 3, 0 } },
    { 59, 7, 0, 0, { 7, 0 } },
    { 66, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185F74[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185F84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185F94[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185FA4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_promenade_80185FB4[13] = {
    { { .empty = D_acropolis_promenade_80183A20 }, D_acropolis_promenade_80183A20, NULL },
    { { .elements = D_acropolis_promenade_80183A30 }, D_acropolis_promenade_80183FF8, NULL },
    { { .elements = D_acropolis_promenade_80184050 }, D_acropolis_promenade_801841A4, NULL },
    { { .elements = D_acropolis_promenade_801841CC }, D_acropolis_promenade_8018462C, NULL },
    { { .elements = D_acropolis_promenade_80184674 }, D_acropolis_promenade_80184CF0, NULL },
    { { .elements = D_acropolis_promenade_80184D38 }, D_acropolis_promenade_80185224, NULL },
    { { .elements = D_acropolis_promenade_8018526C }, D_acropolis_promenade_801854EC, NULL },
    { { .elements = D_acropolis_promenade_8018552C }, D_acropolis_promenade_801858C4, NULL },
    { { .elements = D_acropolis_promenade_8018590C }, D_acropolis_promenade_80185F24, NULL },
    { { .empty = D_acropolis_promenade_80185F74 }, D_acropolis_promenade_80185F74, NULL },
    { { .empty = D_acropolis_promenade_80185F84 }, D_acropolis_promenade_80185F84, NULL },
    { { .empty = D_acropolis_promenade_80185F94 }, D_acropolis_promenade_80185F94, NULL },
    { { .empty = D_acropolis_promenade_80183A20 }, D_acropolis_promenade_80183A20, NULL },
};

ViewCamera D_acropolis_promenade_80186050[13] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 607, 0x7530, -6098 } }, 499 },
    { { { { -3956, 0, -1059 }, { 241, 3987, -903 }, { 1031, -935, -3852 } }, { 372, 488, -0x2DD6 } }, 230 },
    { { { { 3958, 0, -1053 }, { 187, 4030, 703 }, { 1036, -728, 3895 } }, { 1617, 462, 2297 } }, 230 },
    { { { { 3947, 0, -1092 }, { -16, 4095, -57 }, { 1092, 60, 3947 } }, { 1820, 1322, 7620 } }, 230 },
    { { { { 3977, 0, -977 }, { -306, 3889, -1247 }, { 928, 1284, 3776 } }, { 1842, 2326, 0x29C5 } }, 230 },
    { { { { 1010, 0, -3969 }, { 299, 4084, 76 }, { 3957, -309, 1007 } }, { 9221, 696, 3327 } }, 230 },
    { { { { -1131, 0, -3936 }, { -1680, 3703, 483 }, { 3559, 1748, -1023 } }, { 1524, 2795, -0x2739 } }, 230 },
    { { { { 4061, 0, -530 }, { 2, 4095, 21 }, { 530, -21, 4061 } }, { 1461, 1274, 7835 } }, 230 },
    { { { { 3977, 0, -977 }, { -306, 3889, -1247 }, { 928, 1284, 3776 } }, { 1842, 2326, 0x29C5 } }, 230 },
    { { { { -1592, 0, 3773 }, { 1874, 3555, 790 }, { -3275, 2034, -1381 } }, { 112, 2168, -7895 } }, 230 },
    { { { { -1151, 0, 3930 }, { 1811, 3634, 530 }, { -3488, 1887, -1022 } }, { 1036, 1013, 7696 } }, 230 },
    { { { { 4078, 0, -382 }, { -286, 2709, -3058 }, { 252, 3072, 2697 } }, { 1927, 1344, -1461 } }, 230 },
    { { { { 3076, 0, -2704 }, { -116, 4092, -133 }, { 2701, 177, 3073 } }, { 1791, 1265, 5187 } }, 257 },
};

PadScriptCmd D_acropolis_promenade_80186224[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 22), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_promenade_8018623C[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186244 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186250 = {
    0x10000031,
    0x10000033,
    0x10000031,
};

WorldCollisionFootstepSounds D_acropolis_promenade_8018625C = {
    0x10000025,
    0x10000027,
    0x10000025,
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186268 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186274 = {
    0x10000025,
    0x10000027,
    0x10000025,
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186280[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186288[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_80186244 },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186290[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_80186250 },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186298[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_8018625C },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_801862A0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_80186268 },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_801862A8[1] = {
    { 1, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_SUPPRESS_PUSHBACK, &D_acropolis_promenade_80186274 },
};

WorldCollisionSurfaceProperties* D_acropolis_promenade_801862B0[8] = {
    D_acropolis_promenade_80186280,
    D_acropolis_promenade_80186288,
    D_acropolis_promenade_80186290,
    D_acropolis_promenade_80186298,
    D_acropolis_promenade_801862A0,
    D_acropolis_promenade_80186280,
    D_acropolis_promenade_80186280,
    D_acropolis_promenade_801862A8,
};

RoomEventMsg D_acropolis_promenade_801862D0;

Task* D_acropolis_promenade_801862D8;

static void func_acropolis_promenade_8017D5E4(Task* task);

/// Per-frame state of the room task. The first frame the session's warp is 4
/// it spawns the streamed-scene task (entry 2 of the task table), once. While
/// the location's place is 1 it keeps `flowFlags` at 0xA and runs a latch on
/// `gSceneCombatState.signals.bytes.battlePhase`: when that flag drops after having been 1, a sound
/// event is queued, and once the session's `battleResetPending` is then non-zero,
/// `evsStartScriptWithSkip` is called with the room's two data blocks.
static void func_acropolis_promenade_8017D5E4(Task* task)
{
    u8 temp;
    u8 f0;

    if (D_acropolis_promenade_80181140 == 0) {
        if (gGameSession->location.loc.warp == 4) {
            D_acropolis_promenade_80181140 = 1;
            taskSpawnFromTable(D_acropolis_promenade_80181148, 2, 0, 0);
        }
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == 6) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 5;
    }
    temp = gGameSession->location.loc.variant;
    if (temp == 1) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY);
        f0                      = gSceneCombatState.signals.bytes.battlePhase;
        if (f0 == temp) {
            D_acropolis_promenade_80181144 = f0;
        }
        if ((D_acropolis_promenade_80181144 == temp) && (f0 != D_acropolis_promenade_80181144)) {
            D_acropolis_promenade_80181144 = 2;
            sndEvtRequestMidiStop(0, 0x3C);
        }
        if ((D_acropolis_promenade_80181144 == 2) && (gGameSession->battleResetPending != 0)) {
            D_acropolis_promenade_80181144 = 0;
            evsStartScriptWithSkip(D_acropolis_promenade_80180F00, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_promenade_80181068);
        }
    }
}

/// Message gate for the promenade's three hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Message 0xA answers with the `warp` refusal code 1 while the disc has no
/// `.STR` movie file (`gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0`) or nibble 1 is
/// not yet at 4; the first pass at 4 advances it to 5 instead of refusing.
/// Message 0xC, while nibble 2 is still 0, refuses with code 3, latches the
/// answered record into `D_acropolis_promenade_801862D0` for the room's own
/// script to pick up, and arms `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent` with 4. Message 0xE spawns the
/// capsule sequence the first time (nibble 2 still 0) and afterwards reports
/// through `room` whether nibble 2 has reached 3.
///
/// `queryOnly` non-zero means "report only", which suppresses every side effect.
s32 func_acropolis_promenade_8017D70C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    u16          msgId;

    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_OBSERVATORY && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0) {
            out->warp = 1;
        }
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == 4) {
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, 5);
        } else {
            out->warp = 1;
        }
    }
    if (in->areaId == GAME_AREA_ACROPOLIS_SANCTUARY && gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->warp                                           = 3;
            D_acropolis_promenade_801862D0                      = *out;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 4;
        }
        return 1;
    }
    msgId = in->areaId;
    if (msgId == 0xE) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                capSpawnEventIfIdle(2, CAP_EVENT_PAUSE_ACTORS);
                gameFlagSetNibbleIfPresent(in->flagId, 2);
            }
            return 0;
        }
        if (in->areaId == msgId) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 3) {
                    out->room = 2;
                } else {
                    out->room = 1;
                }
            }
        }
    }
    return 1;
}

/// Refuses every key-item-use request in the promenade.
///
/// The first payload is the selected inventory item ID; the second is unused.
/// Ignores all arguments and returns zero, selecting the item's refusal text.
static s32 _acropolisPromenadeRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    enum { ACROPOLIS_PROMENADE_KEY_ITEM_REFUSED = 0 };
    return ACROPOLIS_PROMENADE_KEY_ITEM_REFUSED;
}

s32 func_acropolis_promenade_8017D8E0(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        if (areaGetCurrentObjectState(0x15) != 2) {
            capStartSequenceSlot(5, 1, 0);
        } else {
            capRunCommandWithTransition(9);
        }
    }
    return 0;
}

/// Ignores the promenade's room-action request.
///
/// The first payload transports a borrowed `DirectionActionRequest` address;
/// the second is zero. Neither is read, so the unused payloads remain words.
/// Leaves the result unset; callers must discard the dispatch result.
static s32 _acropolisPromenadeIgnoreRoomAction(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
}

/// Queues promenade sound-bank entries for direct and completed CAP sound cues.
///
/// Cue 10 starts entry 9; cue 103 (CAP command 3 completed) starts entry 10.
/// Uses each script's base mix and ignores other cues and queue failures.
/// Receiver, ID and second payload are unused; always returns zero.
static s32 _acropolisPromenadeHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_PROMENADE_SOUND_CUE_ENTRY_9            = 10,
        ACROPOLIS_PROMENADE_SOUND_CUE_CAP_COMMAND_3_DONE = 103
    };

    switch (soundCue) {
        case ACROPOLIS_PROMENADE_SOUND_CUE_ENTRY_9:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 9), 0, 0);
            break;
        case ACROPOLIS_PROMENADE_SOUND_CUE_CAP_COMMAND_3_DONE:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 0x0A), 0, 0);
            break;
    }
    return 0;
}

/// State table of the room task, run by `func_acropolis_promenade_8017DA4C`.
static const TaskFuncTable3 D_acropolis_promenade_8017D5C4 = {
    { func_acropolis_promenade_8017D9E0, func_acropolis_promenade_8017D5E4, taskKill },
};

/// State table of the prop task, run by `func_acropolis_promenade_8017D988`.
static const TaskFuncTable3 D_acropolis_promenade_8017D5D0 = {
    { _bridgeModelSetup, _acropolisPromenadeUpdateBridgeVisibility, taskKill },
};

/// Runs the prop task's current state (`_bridgeModelSetup`,
/// `_acropolisPromenadeUpdateBridgeVisibility`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017D988(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5D0;
    sp.funcs[task->state](task);
}

static void func_acropolis_promenade_8017D9E0(Task* arg0)
{
    arg0->msgTable = D_acropolis_promenade_80180E74;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    D_acropolis_promenade_801862D8 = taskSpawnFromTable(D_acropolis_promenade_80180EA4, 0, 0, 0);
    arg0->state                    = (s32)(arg0->state + 1);
    D_80115598                     = 1;
}

/// Runs the room task's current state (`func_acropolis_promenade_8017D9E0`,
/// `func_acropolis_promenade_8017D5E4`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017DA4C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/bridge_model_setup.inc.c"

/// Hides the bridge model in promenade view 5 and enables it in the other views.
///
/// Requires the model task's initialized TMD body. Replaces its draw flags and
/// marks the first coordinate dirty so the next draw recomposes its transform.
static void _acropolisPromenadeUpdateBridgeVisibility(Task* task)
{
    enum { ACROPOLIS_PROMENADE_BRIDGE_HIDDEN_VIEW = 5 };
    TmdObject* bridge;
    GfxCoord*  coord;

    bridge = task->extra.tmd;
    coord  = bridge->coords;
    if (viewGetMappedIndex() == ACROPOLIS_PROMENADE_BRIDGE_HIDDEN_VIEW) {
        bridge->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        bridge->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// The promenade's streamed-scene task. State 0 allocates the `RoomMoviePathWork`
/// block, cues the stream (slot-6 msg 0xFA4), captures slot 3 and the player's
/// coordinate matrix in the block, and republishes the player's weapon to slot
/// 3 with a 0x3E8 record. State 1 waits for the stream to come up
/// (`gCdCmdQueue::movieReady`), then starts the script pair and adopts its task
/// as a child. State 2 drives the ride: every frame it moves the player's
/// matrix to the path entry the stream's countdown selects, starts a skip task
/// once on a Start press (`padIsStartPressed`, entry 3 of the room's task table)
/// and, when that task finishes, warps slot 3 with a 0x3E9 placement and spawns
/// entry 4 instead; once the countdown is within 6 frames of the end it sends
/// the same placement as a 0x3F2 and moves on either way. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), releases event HUD
/// suppression (`CAP_CONTROL_MESSAGE_SHOW_HUD`), records the room in the save
/// and kills the task.
void func_acropolis_promenade_8017DB9C(Task* task)
{
    AnimationPlayRequest rec;
    ActorTransform       place;
    s32                  killed;
    RoomMoviePathWork*   work;
    CdCmdQueue*          queue;
    s32                  weaponId;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case 0:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            rec.source.index                             = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId                              = 1;
            rec.blend                                    = ANIMATION_BLEND_RESET;
            rec.blendFrames                              = 0;
            rec.enableWorldCollision                     = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(D_acropolis_promenade_80186224,
                                                               D_acropolis_promenade_8018623C);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->playerMtx->t[0] = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vx;
            work->playerMtx->t[1] = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vy;
            work->playerMtx->t[2] = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vz - 0xC8;
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &killed) != 0) {
                    place.pos.vx = 0x282;
                    place.pos.vy = 0x29;
                    place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vz - 0xC8;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
                    taskSpawnFromTable(D_acropolis_promenade_80181148, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_promenade_80181148, 3, 0, 0);
                work->skipFadeStarted = 1;
            }
            if ((0x45 - queue->movieFrame) < 6) {
                place.pos.vx = 0x282;
                place.pos.vy = 0x29;
                place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vz - 0xC8;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

/// Darkens the promenade movie before its skip transition.
///
/// Start this bodyless task with `killCountdown` zero. Draws subtractive grey
/// levels 0, 32, ... 224, advancing the signed 16-bit counter after each draw.
/// At 256 requests deferred teardown with result zero; the spawning movie
/// task polls that completion before placing the player and starting fade-in.
/// Requires a live overlay and the current frame's primitive arena.
static void _acropolisPromenadeMovieSkipFadeOutTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_STEP = 32,
        ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = (u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_END) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the scene after a movie skip by reducing a subtractive black overlay.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws grey
/// levels 255, 223, ... 31 on successive frames, advancing the signed counter
/// by 32 after each draw and releasing the task when it reaches 256.
static void _acropolisPromenadeSceneFadeInTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_FADE_STEP = 32,
        ACROPOLIS_PROMENADE_FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = ~(u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + ACROPOLIS_PROMENADE_FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= ACROPOLIS_PROMENADE_FADE_END) {
        taskKill(task);
    }
}

/// Per-frame effect spawner for the promenade. `D_acropolis_promenade_80181B74`
/// / `_80181B76` and the twelve-entry mask table `_80181B78` are per-view bit
/// masks: bit `view - 1` of an entry says whether that emitter is visible from
/// the camera `viewGetMappedIndex` reports, and the parallel twelve-entry
/// `_80181B14` array holds each emitter's offset from the room's coordinate
/// frame. View 7 spawns nothing.
void func_acropolis_promenade_8017E03C(Task* task)
{
    GfxCoord*   coord;
    EffectWork* work;
    u8          view;
    s32         i;
    s32         mask;
    s16         prev;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    view  = viewGetMappedIndex();
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    work->age++;
    if (view == 7) {
        return;
    }
    mask = 1 << (view - 1);
    if (D_acropolis_promenade_80181B74 & mask) {
        effectSpawn((EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), coord, (s32)(work->age), &D_acropolis_promenade_80181AF4[1]);
        effectSpawn((EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), coord, (s32)(work->age), &D_acropolis_promenade_80181AF4[2]);
        effectSpawn(EFFECT_ACROPOLIS_PROMENADE_GROUND_GLOW, coord, (s32)(work->age), &D_acropolis_promenade_80181B0C[0]);
        glowDrawTintedDiscNoBias(&D_acropolis_promenade_80181AF4[0], 0x100, 0x5C40);
    }
    for (i = 0; i < 3; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, coord, 0, &D_acropolis_promenade_80181B14[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, coord, 1, &D_acropolis_promenade_80181B14[i]);
        }
        if (D_acropolis_promenade_80181B78[i + 2] & mask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, coord, 2, &D_acropolis_promenade_80181B14[i + 2]);
        }
        if (D_acropolis_promenade_80181B78[i + 4] & mask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, coord, 1, &D_acropolis_promenade_80181B14[i + 4]);
        }
        if (D_acropolis_promenade_80181B78[i + 6] & mask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, coord, 2, &D_acropolis_promenade_80181B14[i + 6]);
        }
    }
    if (D_acropolis_promenade_80181B78[11] & mask) {
        effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, coord, 1, &D_acropolis_promenade_80181B14[11]);
    }
    if (D_acropolis_promenade_80181B76 & mask) {
        prev = work->scale;
        if (prev != view) {
            for (i = 0; i < 0x28; i++) {
                effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, coord, (s32)(view), NULL);
            }
        } else {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, coord, (s32)(prev), NULL);
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, coord, (s32)(prev), NULL);
        }
    }
    work->scale = view;
}

/// Seeds one screen drip's pixel position, lifetime, width and descent period.
///
/// Requires a live writable task and its zero-age effect work. Sets `move.vx`
/// to X in 0..239 pixels, `move.vy` to Y in 176..239 pixels, `scale` to the
/// lifetime cutoff in 30..119 frames, `angle` to width in 16..79 pixels and
/// `period` to 1..4 frames per row. Advances task state without changing age.
/// Consumes five shared LCG draws in X/Y/lifetime/width/period order, using
/// unsigned lifetime reduction. Borrows both objects and retains no pointer.
static __inline__ void _acropolisPromenadeInitializeScreenDrip(Task* task, EffectWork* work)
{
    enum {
        ACROPOLIS_PROMENADE_DRIP_COLUMN_COUNT          = 240,
        ACROPOLIS_PROMENADE_DRIP_FIRST_ROW             = 176,
        ACROPOLIS_PROMENADE_DRIP_ROW_RANGE             = 64,
        ACROPOLIS_PROMENADE_DRIP_MIN_LIFETIME_FRAMES   = 30,
        ACROPOLIS_PROMENADE_DRIP_LIFETIME_RANGE_FRAMES = 90,
        ACROPOLIS_PROMENADE_DRIP_MIN_WIDTH_PIXELS      = 16,
        ACROPOLIS_PROMENADE_DRIP_WIDTH_RANGE_PIXELS    = 64,
        ACROPOLIS_PROMENADE_DRIP_MAX_FRAMES_PER_ROW    = 4
    };
    u16 randomBits;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = (gRandomLcgState >> 16) % ACROPOLIS_PROMENADE_DRIP_COLUMN_COUNT;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_ROW_RANGE - 1)) + ACROPOLIS_PROMENADE_DRIP_FIRST_ROW;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomBits      = gRandomLcgState >> 16;
    work->scale     = (u32)randomBits % ACROPOLIS_PROMENADE_DRIP_LIFETIME_RANGE_FRAMES + ACROPOLIS_PROMENADE_DRIP_MIN_LIFETIME_FRAMES;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->angle     = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_WIDTH_RANGE_PIXELS - 1)) + ACROPOLIS_PROMENADE_DRIP_MIN_WIDTH_PIXELS;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->period    = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_MAX_FRAMES_PER_ROW - 1)) + 1;
    task->state++;
}

void acropolisPromenadeScreenDripTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_PROMENADE_DRIP_DEPTH_FIRST_ROW      = 176,
        ACROPOLIS_PROMENADE_DRIP_LAST_SOURCE_ROW      = 239,
        ACROPOLIS_PROMENADE_DRIP_START_DEPTH          = 0x500,
        ACROPOLIS_PROMENADE_DRIP_DEPTH_PER_ROW        = 10,
        ACROPOLIS_PROMENADE_DRIP_OT_DEPTH_SHIFT       = 4
    };
    EffectWork* work;
    RECT        sourceStrip;
    DR_MOVE*    movePacket;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         onScreen;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_PROMENADE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // The work fields hold pixels and frame counts for this screen-space effect.
        if (work->age == 0) {
            _acropolisPromenadeInitializeScreenDrip(task, work);
        }
        y        = work->move.vy + work->age / work->period;
        x        = work->move.vx;
        depth    = ACROPOLIS_PROMENADE_DRIP_START_DEPTH - (y - ACROPOLIS_PROMENADE_DRIP_DEPTH_FIRST_ROW) * ACROPOLIS_PROMENADE_DRIP_DEPTH_PER_ROW;
        onScreen = y < ACROPOLIS_PROMENADE_DRIP_LAST_SOURCE_ROW;
        // Copy one row down within the current buffer, sorting by its screen row.
        if (onScreen) {
            sourceStrip.x  = x;
            sourceStrip.y  = y + bufferY;
            sourceStrip.w  = work->angle;
            sourceStrip.h  = 1;
            movePacket     = gGpuPrimCursor;
            gGpuPrimCursor = movePacket + 1;
            SetDrawMove(movePacket, &sourceStrip, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> ACROPOLIS_PROMENADE_DRIP_OT_DEPTH_SHIFT), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && onScreen) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Binds the shared star glow to the promenade's exported `void (Task*)` callback.
///
/// Define an unqualified function identifier before including the star fragment;
/// the fragment defines that function and undefines this binding afterwards.
#define GLOW_STAR_TASK acropolisPromenadeGlowStarTask
/// Stores the star core's pixel half-height separately from its equal half-width.
///
/// Numeric configuration for `acropolis_glows_star.inc.c`: nonzero stores
/// `0x1680 / otz` in `OverlaySpriteScratch::cornerDy` and uses it for the core's
/// Y extent; zero or omission reuses `cornerDx` for both axes. The Promenade enables
/// the separate storage; the Bridge omits it. The fragment undefines the
/// switch after compiling this instance.
#define GLOW_STAR_STORE_HALF_HEIGHT 1
#include "../../shared/acropolis_glows_star.inc.c"

/// Places one corner of the ground glow in the task's composed coordinate space.
///
/// `cornerIndex` is 0..3 in GPU strip order; the local XZ half-side is 768
/// game-coordinate units. Requires a live word-aligned scratch block and a
/// coordinate whose `workm` is composed, with 12-fractional-bit rotation.
/// Rotates the local corner through GTE IR, then adds the cached translation,
/// retaining the low 16 bits of each component. Leaves `depth` and the vector's
/// fourth halfword untouched; changes GTE rotation, V0 and result registers.
/// Borrows both objects and retains no pointer.
static __inline__ void _acropolisPromenadeTransformGroundGlowCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, const GfxCoord* coord)
{
    enum { ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE = 768 };
    quadScratch->vertices[cornerIndex].vx     = D_acropolis_promenade_80181AE4[cornerIndex].axis0Sign * ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE;
    (&quadScratch->vertices[cornerIndex])->vy = 0;
    (&quadScratch->vertices[cornerIndex])->vz = D_acropolis_promenade_80181AE4[cornerIndex].axis1Sign * ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE;
    // Rotate the local corner before adding the composition-root translation.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx     += coord->workm.t[0];
    (&quadScratch->vertices[cornerIndex])->vy += coord->workm.t[1];
    (&quadScratch->vertices[cornerIndex])->vz += coord->workm.t[2];
}

/// Reserves and initializes one ground-glow quad in the current frame's packet arena.
///
/// Requires aligned space for a `POLY_FT4` at `gGpuPrimCursor`. Advances the
/// cursor and returns the packet with its DMA length and textured-quad code
/// set; coordinates, colour and texture fields remain uninitialized. Does not
/// check capacity. The caller fills and links it; storage is borrowed from the
/// current frame arena and must live until GPU completion.
static __inline__ POLY_FT4* _acropolisPromenadeReserveGroundGlowQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

void acropolisPromenadeGroundGlowTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_GROUND_GLOW_DEPTH_BIAS       = 32,
        ACROPOLIS_PROMENADE_GROUND_GLOW_MIN_DEPTH        = 17,
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V        = 16,
        ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL       = 39,
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_CLUT     = getClut(16, 270),
        ACROPOLIS_PROMENADE_GROUND_GLOW_GREY_LEVEL_COUNT = 16
    };
    GfxCoord*                 coord;
    EffectWork*               work;
    EffectQuadCornersScratch* quadScratch;
    POLY_FT4*                 quad;
    s32                       cornerIndex;
    s32                       grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    work->age   = task->spawnArg1.value;
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    // Rotate the local XZ square, then translate its signed 16-bit corners.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_acropolis_promenade_80181AE4); cornerIndex++) {
        _acropolisPromenadeTransformGroundGlowCorner(quadScratch, cornerIndex, coord);
    }
    // Project into the packet; a rejected quad still consumes packet space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    quad = _acropolisPromenadeReserveGroundGlowQuad();
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    quad->u0 = 0;
    quad->v0 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V;
    quad->u1 = ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->v1 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V;
    quad->u2 = 0;
    quad->v2 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V + ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->u3 = ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->v3 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V + ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->depth);
    quadScratch->depth += ACROPOLIS_PROMENADE_GROUND_GLOW_DEPTH_BIAS;
    if (quadScratch->depth >= ACROPOLIS_PROMENADE_GROUND_GLOW_MIN_DEPTH) {
        quad->tpage     = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_PAGE;
        quad->clut      = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_CLUT;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_GROUND_GLOW_GREY_LEVEL_COUNT - 1);
        quad->r0        = grey;
        quad->g0        = grey;
        quad->b0        = grey;
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
    effectKillTask(work, task);
}

/// Binds the included lamp glow to the promenade's exported `void (Task*)` callback.
///
/// Define this function identifier immediately before the lamp fragment, which
/// undefines it. No arguments, conditional alternatives or tokens are constructed.
#define GLOW_LAMP_TASK acropolisPromenadeGlowLampTask
#include "../../shared/acropolis_glows_lamp.inc.c"

#include "../../shared/glow_draw_tinted_disc_no_bias.inc.c"
