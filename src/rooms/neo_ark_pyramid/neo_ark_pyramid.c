#include "rooms/neo_ark_pyramid.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

/// Angle of the room's rotating quad, set by `_neoArkPyramidSetRotationPuzzleAngle`.
extern s32      D_neo_ark_pyramid_801818A4;
extern TaskDesc D_neo_ark_pyramid_8017FC0C;

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF
/// and 0x13F0, closed by a `TASK_MESSAGE_TABLE_END` entry.
extern TaskMessageEntry D_neo_ark_pyramid_8017FBE4[];

/// The two points on the spawner's parent coordinate that the ribbon task
/// trails: the first entry places the effect, the second (reached here both
/// as `[1]` and under its own label) is the ribbon's other edge.

static void _neoArkPyramidSetRotationPuzzleAngle(s32 sweepAngle);
static void _neoArkPyramidInitializeRoom(Task* task);
static void _neoArkPyramidDrawRotationPuzzleState(Task* unusedTask);

/// State handlers of the room's entry task, indexed by its state through
/// `neoArkPyramidRoomTask`: set-up, per-frame draw, then kill.
static const TaskFuncTable3 D_neo_ark_pyramid_8017D5C4 = {
    { _neoArkPyramidInitializeRoom, _neoArkPyramidDrawRotationPuzzleState, taskKill }
};

static void _neoArkPyramidRotationPuzzleTask(Task* task);
static s32  _neoArkPyramidRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _neoArkPyramidResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkPyramidIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _neoArkPyramidHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Room message carrying the integer item ID selected in the key-item menu.
enum { NEO_ARK_PYRAMID_MESSAGE_USE_KEY_ITEM = 0x13F1 };

extern WorldCollisionGrid     D_neo_ark_pyramid_801802C4[1];
extern WorldCollisionOccluder D_neo_ark_pyramid_80181790[3];
extern WorldCollisionTrigger  D_neo_ark_pyramid_801812B0[6];
extern WorldCollisionTrigger  D_neo_ark_pyramid_80181478[7];
extern WorldCoordRoomLights   D_neo_ark_pyramid_80181298[1];

TaskMessageEntry D_neo_ark_pyramid_8017FBE4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkPyramidResolveRoomVariant },
    { NEO_ARK_PYRAMID_MESSAGE_USE_KEY_ITEM, _neoArkPyramidRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkPyramidHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkPyramidIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_pyramid_8017FC0C = { { { TASK_BODY_NONE, 32 } }, _neoArkPyramidRotationPuzzleTask, { .value = 0 } };

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_pyramid_8017FC28[2] = {
    { D_neo_ark_pyramid_801802C4, D_neo_ark_pyramid_801812B0, D_neo_ark_pyramid_80181478, D_neo_ark_pyramid_80181790 },
    { D_neo_ark_pyramid_801802C4, D_neo_ark_pyramid_801812B0, D_neo_ark_pyramid_80181478, D_neo_ark_pyramid_80181790 },
};

WorldCoordRoomLighting D_neo_ark_pyramid_8017FC48[2] = {
    { D_neo_ark_pyramid_80181298, NULL },
    { D_neo_ark_pyramid_80181298, NULL },
};

u8 D_neo_ark_pyramid_8017FC58[8] = {
    1,
    2,
    3,
    6,
    5,
    4,
    7,
    8,
};

u8* D_neo_ark_pyramid_8017FC60[2] = {
    gViewIdentityMap,
    D_neo_ark_pyramid_8017FC58,
};

ViewCount D_neo_ark_pyramid_8017FC68[2] = { 8, 8 };

DirectionWarpEntry D_neo_ark_pyramid_8017FC6C[2] = {
    { { { .word = 2048 }, -2944, 0, -2035 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -2944, 0, -2035 }, { 0, 0, 0, 0 }, 0x55200002, 0x55200001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 3200, -650, -7490 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4200, -1160, -7500 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkPyramidCollision02D04Normals[13] = {
#include "assets/neo_ark_pyramid_collision_02D04_normals.inc"
};

static SVECTOR _gNeoArkPyramidCollision02D04Verts[66] = {
#include "assets/neo_ark_pyramid_collision_02D04_verts.inc"
};

static WorldCollisionGridFace _gNeoArkPyramidCollision02D04Faces[36] = {
#include "assets/neo_ark_pyramid_collision_02D04_faces.inc"
};

static s16 _gNeoArkPyramidCollision02D04Cells[194] = {
#include "assets/neo_ark_pyramid_collision_02D04_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkPyramidCollision02D04Cells[i])
static s16* _gNeoArkPyramidCollision02D04Table[15] = {
#include "assets/neo_ark_pyramid_collision_02D04_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_pyramid_801802C4[1] = {
    { NULL, _gNeoArkPyramidCollision02D04Normals, _gNeoArkPyramidCollision02D04Verts, _gNeoArkPyramidCollision02D04Faces, _gNeoArkPyramidCollision02D04Table, 4000, 0x2CEC, 5, 3, 4000, 36 },
};

ViewCamera D_neo_ark_pyramid_801802E8[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5270, 0x7530, 7060 } }, 380 },
    { { { { 3888, 0, 1288 }, { -275, 4001, 830 }, { -1258, -875, 3798 } }, { 2039, 468, 5719 } }, 230 },
    { { { { 2546, 0, 3208 }, { 47, 4095, -37 }, { -3208, 60, 2545 } }, { -1654, 855, 7910 } }, 230 },
    { { { { -1401, 0, -3848 }, { 212, 4089, -77 }, { 3842, -226, -1399 } }, { 2273, 757, 5456 } }, 230 },
    { { { { -405, 0, -4075 }, { -324, 4083, 32 }, { 4062, 325, -404 } }, { -570, 1119, 8915 } }, 230 },
    { { { { -1401, 0, -3848 }, { 212, 4089, -77 }, { 3842, -226, -1399 } }, { 2273, 757, 5456 } }, 230 },
    { { { { -1554, 0, 3789 }, { 807, 4001, 331 }, { -3702, 872, -1518 } }, { 765, 870, 6068 } }, 257 },
    { { { { -2878, 0, 2914 }, { 16, 4095, 16 }, { -2914, 23, -2878 } }, { 1595, 667, 6136 } }, 312 },
};

SpriteBatch D_neo_ark_pyramid_80180408[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_pyramid_80180418[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_80180428[12] = {
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -112, -48, 1050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -96, -48, 1075, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -40, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 8, 1606, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 0, 1611, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 0, 1621, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 1588, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -8, 1611, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 96, -8, 1611, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -8, 1611, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, -16, 1611, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 40, 1611, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_80180518[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_80180530[43] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1405, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 16, 1399, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 16, 1399, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 24, 1395, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 1395, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 192 } }, -160, -120, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -120, -120, 1625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -104, -104, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, -104, 1625, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -120, 1625, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -88, 1625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -80, 1625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 24, 1625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1395, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -24, 24, 1437, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 8, 1589, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 16, 1503, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 0, 1651, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -8, 1745, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -16, 1824, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -48, -24, 1963, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -48, -32, 2033, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -48, -40, 2182, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, -48, 2324, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -56, -56, 2624, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, -64, 2681, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 32, 1426, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 32, 1426, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 40, 1393, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 1425, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 32, 1391, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 32, 1390, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -16, 1562, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1466, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 0, 1389, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 8, 1339, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1298, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1254, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1263, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1263, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1273, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_8018088C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 1, 0 } },
    { 34, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_801808AC[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 24, 723, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -96, 1184, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, -112, 1268, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -128, -120, 1336, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -112, -120, 1845, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -96, -120, 1098, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 0, 772, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 24, 708, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 56, 639, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 606, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 0, 774, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 24, 713, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 56, 642, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 80, 589, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 0, 756, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 24, 719, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 56, 645, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 80, 593, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 760, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 24, 711, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 56, 627, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 80, 585, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 0, 764, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 24, 715, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_80180A8C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_80180AA4[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1405, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 192 } }, -160, -120, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -120, -120, 1625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -104, -104, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, -104, 1625, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -120, 1625, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -88, 1625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -80, 1625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 24, 1625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1395, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -24, 24, 1437, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 8, 1589, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 16, 1503, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 0, 1651, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -8, 1745, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -16, 1824, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -48, -24, 1963, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -48, -32, 2033, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -48, -40, 2182, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, -48, 2324, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -56, -56, 2624, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, -64, 2681, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 32, 1426, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 40, 1393, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 32, 1426, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 1426, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 32, 1391, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 32, 1390, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 16, 1399, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 24, 1395, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -16, 1562, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1466, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 0, 1389, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 8, 1339, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1298, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1254, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1263, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1263, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1273, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_80180DD8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 1, 0 } },
    { 32, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_pyramid_80180DF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_pyramid_80180E08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_pyramid_80180E18[8] = {
    { { .empty = D_neo_ark_pyramid_80180408 }, D_neo_ark_pyramid_80180408, NULL },
    { { .empty = D_neo_ark_pyramid_80180418 }, D_neo_ark_pyramid_80180418, NULL },
    { { .elements = D_neo_ark_pyramid_80180428 }, D_neo_ark_pyramid_80180518, NULL },
    { { .elements = D_neo_ark_pyramid_80180530 }, D_neo_ark_pyramid_8018088C, NULL },
    { { .elements = D_neo_ark_pyramid_801808AC }, D_neo_ark_pyramid_80180A8C, NULL },
    { { .elements = D_neo_ark_pyramid_80180AA4 }, D_neo_ark_pyramid_80180DD8, NULL },
    { { .empty = D_neo_ark_pyramid_80180DF8 }, D_neo_ark_pyramid_80180DF8, NULL },
    { { .empty = D_neo_ark_pyramid_80180E08 }, D_neo_ark_pyramid_80180E08, NULL },
};

WorldCoordPointLight D_neo_ark_pyramid_80180E78[11] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3490, -2000, -3010 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -80, -4000, -6040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4120, -2000, -5600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2560, -2000, -9990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3940, -2000, -0x28E6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5480, -2000, -0x2B16 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1900, -2000, -7420 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2760, -2000, -5140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1360, -2000, -6580 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4890, -2000, -4660 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -230, -2000, -7890 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
};

WorldCoordRoomLights D_neo_ark_pyramid_80181298[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_pyramid_80180E78), D_neo_ark_pyramid_80180E78, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_pyramid_801812B0[6] = {
    { NULL, NULL, NULL, { -2792, -4000, -3605, 0 }, { { 2775, -4752, 1502, 0 }, { -2776, -4752, -1503, 0 }, { 2775, 4752, 1502, 0 }, { -2776, 4752, -1503, 0 } }, { -1953, 0, 3606, 0 }, { 0, 0, 4096, 0 }, 5701, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2914, -4033, -3426, 0 }, { { -2775, -4752, -1502, 0 }, { 2776, -4752, 1503, 0 }, { -2775, 4752, -1502, 0 }, { 2776, 4752, 1503, 0 } }, { 1952, 0, -3608, 0 }, { 0, 0, 4096, 0 }, 5701, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -449, -4064, -7490, 0 }, { { 1127, -4752, 3686, 0 }, { -1148, -4752, -3699, 0 }, { 1127, 4752, 3686, 0 }, { -1148, 4752, -3699, 0 } }, { -3931, 0, 1210, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -544, -3936, -7296, 0 }, { { -1147, -4752, -3698, 0 }, { 1128, -4752, 3687, 0 }, { -1147, 4752, -3698, 0 }, { 1128, 4752, 3687, 0 } }, { 3930, 0, -1211, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2766, -3968, -0x2BF1, 0 }, { { 32, -4752, 2884, 0 }, { -31, -4752, -2884, 0 }, { 32, 4752, 2884, 0 }, { -31, 4752, -2884, 0 } }, { -4106, 0, 44, 0 }, { 0, 0, 4096, 0 }, 5538, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2559, -4032, -0x2BC0, 0 }, { { -31, -4752, -2884, 0 }, { 32, -4752, 2884, 0 }, { -31, 4752, -2884, 0 }, { 32, 4752, 2884, 0 } }, { 4105, 0, -46, 0 }, { 0, 0, 4096, 0 }, 5538, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_pyramid_80181478[7] = {
    { NULL, NULL, NULL, { 3005, -3872, -7457, 0 }, { { -13, -5344, 1023, 0 }, { 14, -5344, -1022, 0 }, { -13, 5344, 1023, 0 }, { 14, 5344, -1022, 0 } }, { -4109, 0, -55, 0 }, { -4092, -201, -14, 0 }, 5418, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 20, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2272, -64, -7472, 0 }, { { 416, 14, -702, 0 }, { 416, -13, 703, 0 }, { -416, 14, -702, 0 }, { -416, -13, 703, 0 } }, { 0, 4103, 71, 0 }, { -4096, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_FACING, 71, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3104, -658, -7488, 0 }, { { 416, -2, -974, 0 }, { 416, 3, 975, 0 }, { -416, -2, -974, 0 }, { -416, 3, 975, 0 } }, { 0, 4099, -21, 0 }, { 4091, 0, -201, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2945, -48, -1888, 0 }, { { 975, -2, 416, 0 }, { -974, 3, 416, 0 }, { 975, -2, -416, 0 }, { -974, 3, -416, 0 } }, { 10, 4099, 0, 0 }, { -201, 0, -4091, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, -5985, 0 }, { { 963, -2, -443, 0 }, { -346, 3, 1002, 0 }, { 346, -2, -1001, 0 }, { -963, 3, 443, 0 } }, { 0, 4099, -11, 0 }, { 3405, 0, 2275, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6558, -64, -5760, 0 }, { { 416, 14, -894, 0 }, { 416, -13, 895, 0 }, { -416, 14, -894, 0 }, { -416, -13, 895, 0 } }, { 0, 4106, 56, 0 }, { -4096, 0, 0, 0 }, 985, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6528, -64, -9792, 0 }, { { 416, 14, -1278, 0 }, { 416, -13, 1279, 0 }, { -416, 14, -1278, 0 }, { -416, -13, 1279, 0 } }, { 0, 4119, 39, 0 }, { -4096, 0, 0, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_pyramid_8018168C[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_801816A4[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_801816BC[3] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { 26, 26, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202600_801528D4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_801816E0[3] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { 20, 20, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_302000_80177DF0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_80181704[3] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_pyramid_80181728[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C580, D_neo_ark_pyramid_8018168C },
    { D_map_neo_ark_8017C620, D_neo_ark_pyramid_801816A4 },
    { D_map_neo_ark_8017C690, D_neo_ark_pyramid_801816BC },
    { D_map_neo_ark_8017C720, D_neo_ark_pyramid_801816E0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017C760, D_neo_ark_pyramid_80181704 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_pyramid_80181790[3] = {
    { NULL, NULL, { 6416, -1104, -6464, 0 }, { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, { 0, 0, 4097, 0 }, 4664, 1, 0 },
    { NULL, NULL, { 6400, -1088, -8384, 0 }, { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, { 0, 0, 4097, 0 }, 4664, 1, 0 },
    { NULL, NULL, { 1536, -1984, -5744, 0 }, { { -5776, 0, 4448, 0 }, { 5776, 0, 4448, 0 }, { -5776, 0, -2464, 0 }, { 5776, 0, -6432, 0 } }, { 0, -4110, 0, 0 }, 8628, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_pyramid_80181844 = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionFootstepSounds D_neo_ark_pyramid_80181850 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_8018185C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_80181864[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_8018186C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_80181874[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_pyramid_80181844 },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_8018187C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_pyramid_80181850 },
};

WorldCollisionSurfaceProperties* D_neo_ark_pyramid_80181884[8] = {
    D_neo_ark_pyramid_8018185C,
    D_neo_ark_pyramid_80181864,
    D_neo_ark_pyramid_8018186C,
    D_neo_ark_pyramid_80181874,
    D_neo_ark_pyramid_8018187C,
    D_neo_ark_pyramid_8018185C,
    D_neo_ark_pyramid_8018185C,
    D_neo_ark_pyramid_8018185C,
};

s32 D_neo_ark_pyramid_801818A4 = 0;

static void _neoArkPyramidDrawRotationPuzzleQuad(s32 angle);

/// Restores the room view and player presentation after a rotation-puzzle exit.
///
/// Requires the live player and held event state. Clears the room's event gates
/// before resuming player control and automatic model drawing, then kills the
/// bodyless puzzle task. The caller must have finished CAP playback.
static inline void _neoArkPyramidRestoreRoomControl(Task* task)
{
    enum { NEO_ARK_PYRAMID_ROTATION_ROOM_VIEW = 3 };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = NEO_ARK_PYRAMID_ROTATION_ROOM_VIEW;
    gGameSession->hideHud                                      = 0;
    gGameSession->eventState                                   = 0;
    gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    taskKill(task);
}

/// Runs the rotation puzzle's prompt, angle sweep and return to room control.
///
/// Starts with a bodyless state-0 task after the player has been held and hidden.
/// View 8 shows the puzzle. Each accepted prompt sweeps one twelfth-turn in
/// four-unit steps (4096 units per turn), using `killCountdown` as a halfword
/// angle accumulator. Four completed steps play the completion CAP; variant
/// key 12 cancels. Both exits select view 3 and restore actors, HUD and player.
/// Requires the room, CAP and drawing resources to remain loaded until teardown.
static void _neoArkPyramidRotationPuzzleTask(Task* task)
{
    enum { NEO_ARK_PYRAMID_ROTATION_PREPARE          = 0,
           NEO_ARK_PYRAMID_ROTATION_PROMPT           = 1,
           NEO_ARK_PYRAMID_ROTATION_WAIT_PROMPT      = 2,
           NEO_ARK_PYRAMID_ROTATION_BEGIN_SWEEP      = 3,
           NEO_ARK_PYRAMID_ROTATION_SWEEP            = 4,
           NEO_ARK_PYRAMID_ROTATION_WAIT_COMPLETION  = 5,
           NEO_ARK_PYRAMID_ROTATION_EXIT             = 10,
           NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW      = 8,
           NEO_ARK_PYRAMID_ROTATION_PROMPT_COMMAND   = 1,
           NEO_ARK_PYRAMID_ROTATION_COMPLETE_COMMAND = 2,
           NEO_ARK_PYRAMID_ROTATION_CANCEL_KEY       = 12,
           NEO_ARK_PYRAMID_ROTATION_ANGLE_STEP       = 4,
           NEO_ARK_PYRAMID_ROTATION_SWEEP_LIMIT      = 342,
           NEO_ARK_PYRAMID_ROTATION_REQUIRED_STEPS   = 4,
           NEO_ARK_PYRAMID_ROTATION_KEEP_ACTORS_HELD = 1 };
    u16 sweepAngle;

    switch (task->state) {
        case NEO_ARK_PYRAMID_ROTATION_PREPARE:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW;
            gGameSession->hideHud                                      = 1;
            gGameSession->eventState                                   = 1;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state++;
            break;
        case NEO_ARK_PYRAMID_ROTATION_PROMPT:
            capRunCommand(NEO_ARK_PYRAMID_ROTATION_PROMPT_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case NEO_ARK_PYRAMID_ROTATION_WAIT_PROMPT:
            D_80115690 = NEO_ARK_PYRAMID_ROTATION_KEEP_ACTORS_HELD;
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case NEO_ARK_PYRAMID_ROTATION_BEGIN_SWEEP:
            if (capGetVariantKey() == NEO_ARK_PYRAMID_ROTATION_CANCEL_KEY) {
                task->state = NEO_ARK_PYRAMID_ROTATION_EXIT;
                break;
            }
            sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE, 0, 0);
            task->killCountdown = 0;
            task->state++;
            break;
        case NEO_ARK_PYRAMID_ROTATION_SWEEP:
            // Commit the completed turn before settling the displayed angle.
            sweepAngle          = task->killCountdown + NEO_ARK_PYRAMID_ROTATION_ANGLE_STEP;
            task->killCountdown = sweepAngle;
            if ((s16)sweepAngle >= NEO_ARK_PYRAMID_ROTATION_SWEEP_LIMIT) {
                gameFlagSetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT, gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) + 1);
                _neoArkPyramidSetRotationPuzzleAngle(0);
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) >= NEO_ARK_PYRAMID_ROTATION_REQUIRED_STEPS) {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE_DONE, 0, 0);
                    capRunCommand(NEO_ARK_PYRAMID_ROTATION_COMPLETE_COMMAND, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                } else {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE_STOP, 0, 0);
                    task->state = NEO_ARK_PYRAMID_ROTATION_PROMPT;
                }
            } else {
                _neoArkPyramidSetRotationPuzzleAngle((s16)sweepAngle);
            }
            break;
        case NEO_ARK_PYRAMID_ROTATION_WAIT_COMPLETION:
            D_80115690 = NEO_ARK_PYRAMID_ROTATION_KEEP_ACTORS_HELD;
            if (capIsBusy() == 0) {
                task->state = NEO_ARK_PYRAMID_ROTATION_EXIT;
            }
            break;
        case NEO_ARK_PYRAMID_ROTATION_EXIT:
            _neoArkPyramidRestoreRoomControl(task);
            break;
    }
}

/// Draws the rotation puzzle's 174-pixel square about the screen origin.
///
/// `angle` uses 4096 units per turn; sine and cosine have twelve fractional bits.
/// Requires the puzzle's 8-bit texture at VRAM (896, 0), palette at (0, 255),
/// ordering-table entry 12 and room for one `POLY_FT4` in the frame arena.
/// Raw texture colour is used without shading or semitransparency.
static void _neoArkPyramidDrawRotationPuzzleQuad(s32 angle)
{
    enum { NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT        = 87,
           NEO_ARK_PYRAMID_PUZZLE_TRIG_FRACTION_BITS = 12,
           NEO_ARK_PYRAMID_PUZZLE_UV_MIN             = 1,
           NEO_ARK_PYRAMID_PUZZLE_UV_MAX             = NEO_ARK_PYRAMID_PUZZLE_UV_MIN + 2 * NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT,
           NEO_ARK_PYRAMID_PUZZLE_TEXTURE_8_BIT      = 1,
           NEO_ARK_PYRAMID_PUZZLE_OT_INDEX           = 12 };

    POLY_FT4* quad;
    DVECTOR   sourceCorners[4];
    DVECTOR   rotatedCorners[4];
    s32       cornerIndex;

    // One loop statement capturing sourceCorners, rotatedCorners, angle and cornerIndex.
    // Writes four rotated pixel pairs and leaves cornerIndex at the source array's end.
#define NEO_ARK_PYRAMID_ROTATE_PUZZLE_CORNERS()                                                                                                                                    \
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(sourceCorners); cornerIndex++) {                                                                                                \
        rotatedCorners[cornerIndex].vx = (sourceCorners[cornerIndex].vx * rcos(angle) - sourceCorners[cornerIndex].vy * rsin(angle)) >> NEO_ARK_PYRAMID_PUZZLE_TRIG_FRACTION_BITS; \
        rotatedCorners[cornerIndex].vy = (sourceCorners[cornerIndex].vx * rsin(angle) + sourceCorners[cornerIndex].vy * rcos(angle)) >> NEO_ARK_PYRAMID_PUZZLE_TRIG_FRACTION_BITS; \
    }

    // Rotate screen-space corners, narrowing the Q12 result to signed pixels.
    sourceCorners[0].vx = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[0].vy = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[1].vx = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[1].vy = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[2].vx = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[2].vy = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[3].vx = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[3].vy = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    NEO_ARK_PYRAMID_ROTATE_PUZZLE_CORNERS();
#undef NEO_ARK_PYRAMID_ROTATE_PUZZLE_CORNERS
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    setShadeTex(quad, true);
    quad->x0    = rotatedCorners[0].vx;
    quad->y0    = rotatedCorners[0].vy;
    quad->x1    = rotatedCorners[1].vx;
    quad->y1    = rotatedCorners[1].vy;
    quad->x2    = rotatedCorners[2].vx;
    quad->y2    = rotatedCorners[2].vy;
    quad->x3    = rotatedCorners[3].vx;
    quad->y3    = rotatedCorners[3].vy;
    quad->u0    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->v0    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->u1    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->v1    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->u2    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->v2    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->u3    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->v3    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->clut  = getClut(0, 255);
    quad->tpage = getTPage(NEO_ARK_PYRAMID_PUZZLE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0);
    addPrim(gGpuCurrentOt + NEO_ARK_PYRAMID_PUZZLE_OT_INDEX, quad);
}

/// Refuses every key-item use in the pyramid, leaving the item and room unchanged.
///
/// Handles message 0x13F1 with an integer `itemId`; both payload words are ignored.
/// Returns zero so the item menu displays its unavailable-use response.
static s32 _neoArkPyramidRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Resolves a Neo Ark destination room for a pyramid transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
static s32 _neoArkPyramidResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_PYRAMID_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_PYRAMID_TRANSITION_ALLOWED;
}

/// Ignores CAP room commands and returns zero without changing the pyramid.
///
/// Handles `ROOM_MESSAGE_COMMAND`; the integer command and second word are ignored.
static s32 _neoArkPyramidIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    return 0;
}

/// Opens the rotation puzzle or its completed-puzzle CAP event.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request
/// and ignored second word. Action 1 settles the displayed angle, then starts
/// CAP event 3 at exactly four completed turns, or holds and hides the player
/// before spawning the puzzle task. Other actions do nothing; returns zero.
static s32 _neoArkPyramidHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { NEO_ARK_PYRAMID_ACTION_ROTATION_PUZZLE  = 1,
           NEO_ARK_PYRAMID_ROTATION_COMPLETE_TURNS = 4,
           NEO_ARK_PYRAMID_COMPLETED_PUZZLE_EVENT  = 3 };

    if (request->actionId == NEO_ARK_PYRAMID_ACTION_ROTATION_PUZZLE) {
        _neoArkPyramidSetRotationPuzzleAngle(0);
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) == NEO_ARK_PYRAMID_ROTATION_COMPLETE_TURNS) {
            capSpawnEventIfIdle(NEO_ARK_PYRAMID_COMPLETED_PUZZLE_EVENT, CAP_EVENT_PAUSE_ACTORS);
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            taskSpawnFromTable(&D_neo_ark_pyramid_8017FC0C, 0, 0, 0);
        }
    }
    return 0;
}

/// Sets the rotation puzzle's screen angle from completed steps and the current sweep.
///
/// Angles use 4096 units per turn. Each completed step advances one twelfth of
/// a turn, with step four aligned at zero. `sweepAngle` adds a signed angle
/// within the current step; pass zero to settle on the completed-step angle.
/// The stored angle is signed and is not normalized to a single turn.
static void _neoArkPyramidSetRotationPuzzleAngle(s32 sweepAngle)
{
    enum { NEO_ARK_PYRAMID_ROTATION_ALIGNED_STEP     = 4,
           NEO_ARK_PYRAMID_ROTATION_ANGLE_TURN_SHIFT = 12,
           NEO_ARK_PYRAMID_ROTATION_STEPS_PER_TURN   = 12 };

    // Scale the signed completed-step offset before dividing; add the sweep last.
    D_neo_ark_pyramid_801818A4 = (((gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) - NEO_ARK_PYRAMID_ROTATION_ALIGNED_STEP) << NEO_ARK_PYRAMID_ROTATION_ANGLE_TURN_SHIFT) / NEO_ARK_PYRAMID_ROTATION_STEPS_PER_TURN) + sweepAngle;
}

/// Installs the pyramid message receiver and advances to puzzle drawing.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _neoArkPyramidInitializeRoom(Task* task)
{
    task->msgTable = D_neo_ark_pyramid_8017FBE4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Draws the rotation puzzle during the room task's per-frame state.
///
/// Only active view 8 displays the quad, using the angle last established by
/// `_neoArkPyramidSetRotationPuzzleAngle`. Requires the frame's primitive arena,
/// ordering table and puzzle texture to be ready. The task argument is unused.
static void _neoArkPyramidDrawRotationPuzzleState(Task* unusedTask)
{
    enum { NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW = 8 };

    if (gGameSession->location.loc.view == NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW) {
        _neoArkPyramidDrawRotationPuzzleQuad(D_neo_ark_pyramid_801818A4);
    }
}

void neoArkPyramidRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_neo_ark_pyramid_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

void neoArkPyramidConfigureEffectsTask(Task* task)
{
    enum { NEO_ARK_PYRAMID_EFFECTS_INITIALIZE,
           NEO_ARK_PYRAMID_EFFECTS_CONFIGURED };

    if (task->state == NEO_ARK_PYRAMID_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId               = EFFECT_NEO_ARK_PYRAMID_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_NEO_ARK_PYRAMID_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_NEO_ARK_PYRAMID_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        task->state                      = NEO_ARK_PYRAMID_EFFECTS_CONFIGURED;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void neoArkPyramidRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void neoArkPyramidRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void neoArkPyramidRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
