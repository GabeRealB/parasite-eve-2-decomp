#include "rooms/neo_ark_south_promenade.h"

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
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

/// The room's message table, which the message-driven task installs.
extern TaskMessageEntry D_neo_ark_south_promenade_8017F6B4[];

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `RoomFx_TrailOffsets[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_south_promenade_8017D62C(Task* task);
static void _neoArkSouthPromenadeIdleRoomTask(Task* task);

/// State table of the room's message-driven task, indexed by `Task::state`:
/// install the message table, idle, then kill the task.
static const TaskFuncTable3 D_neo_ark_south_promenade_8017D5C4 = {
    { func_neo_ark_south_promenade_8017D62C, _neoArkSouthPromenadeIdleRoomTask, taskKill },
};

static s32 _neoArkSouthPromenadeRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_neo_ark_south_promenade_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _neoArkSouthPromenadeIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _neoArkSouthPromenadeIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Requests use of the selected key item; the room replies with zero to refuse it.
enum { NEO_ARK_SOUTH_PROMENADE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

extern WorldCollisionGrid     D_neo_ark_south_promenade_8017FD8C[1];
extern WorldCollisionOccluder D_neo_ark_south_promenade_8018094C[1];
extern WorldCollisionTrigger  D_neo_ark_south_promenade_801804E8[6];
extern WorldCollisionTrigger  D_neo_ark_south_promenade_801806B0[6];
extern WorldCoordRoomLights   D_neo_ark_south_promenade_801804D0[1];

TaskMessageEntry D_neo_ark_south_promenade_8017F6B4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_south_promenade_8017D5D8 },
    { NEO_ARK_SOUTH_PROMENADE_MESSAGE_USE_KEY_ITEM, _neoArkSouthPromenadeRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkSouthPromenadeIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _neoArkSouthPromenadeIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting D_neo_ark_south_promenade_8017F6EC[1] = {
    { D_neo_ark_south_promenade_801804D0, NULL },
};

WorldCollisionRoomResources D_neo_ark_south_promenade_8017F6F4[1] = {
    { D_neo_ark_south_promenade_8017FD8C, D_neo_ark_south_promenade_801804E8, D_neo_ark_south_promenade_801806B0, D_neo_ark_south_promenade_8018094C },
};

u8* D_neo_ark_south_promenade_8017F704[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_south_promenade_8017F708[1] = { 5 };

DirectionWarpEntry D_neo_ark_south_promenade_8017F70C[2] = {
    { { { .word = 1024 }, 1137, 0, 2500 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1137, 0, 2500 }, { 0, 0, 0, 0 }, 0x55130002, 0x55130001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x2A30, 0, 0x3200 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2A30, 0, 0x3200 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkSouthPromenadeCollision027CCNormals[13] = {
#include "assets/neo_ark_south_promenade_collision_027CC_normals.inc"
};

static SVECTOR _gNeoArkSouthPromenadeCollision027CCVerts[81] = {
#include "assets/neo_ark_south_promenade_collision_027CC_verts.inc"
};

static WorldCollisionGridFace _gNeoArkSouthPromenadeCollision027CCFaces[35] = {
#include "assets/neo_ark_south_promenade_collision_027CC_faces.inc"
};

static s16 _gNeoArkSouthPromenadeCollision027CCCells[158] = {
#include "assets/neo_ark_south_promenade_collision_027CC_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkSouthPromenadeCollision027CCCells[i])
static s16* _gNeoArkSouthPromenadeCollision027CCTable[16] = {
#include "assets/neo_ark_south_promenade_collision_027CC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_south_promenade_8017FD8C[1] = {
    { NULL, _gNeoArkSouthPromenadeCollision027CCNormals, _gNeoArkSouthPromenadeCollision027CCVerts, _gNeoArkSouthPromenadeCollision027CCFaces, _gNeoArkSouthPromenadeCollision027CCTable, -700, -500, 4, 4, 4000, 35 },
};

ViewCamera D_neo_ark_south_promenade_8017FDB0[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x58D6, -7000 } }, 289 },
    { { { { 4002, 0, 871 }, { 35, 4092, -163 }, { -871, 167, 3998 } }, { -0x3070, 1502, -3900 } }, 257 },
    { { { { -3965, 0, 1025 }, { 54, 4090, 210 }, { -1023, 217, -3960 } }, { -0x31E2, 1650, -0x2BC0 } }, 257 },
    { { { { -954, 0, 3983 }, { 197, 4090, 47 }, { -3978, 203, -953 } }, { -0x3070, 1690, -3200 } }, 207 },
    { { { { -870, 0, 4002 }, { 776, 4018, 168 }, { -3926, 794, -853 } }, { -8000, 2042, -3300 } }, 207 },
};

SpriteBatch D_neo_ark_south_promenade_8017FE64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_south_promenade_8017FE74[25] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 24, 912, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 912, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 56, 900, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 16, 900, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -48, 1856, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1837, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1837, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1837, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, -112, 1837, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -88, 1837, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -40, 1837, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 8, 1837, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, 8, 1856, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -40, 1856, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -16, -16, 1856, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -16, 8, 1856, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 136, -64, 1905, { .fields = { 120, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 112, -48, 1905, { .fields = { 112, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, 0, -8, 1905, { .fields = { 24, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 16, -24, 1905, { .fields = { 40, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 40, -40, 1905, { .fields = { 8, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 64, -56, 1905, { .fields = { 8, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 88, -72, 1905, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 112, -88, 1905, { .fields = { 24, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 136, -104, 1905, { .fields = { 0, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_south_promenade_80180068[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 12, 0, 0, { 2, 0 } },
    { 16, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_south_promenade_80180090[39] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 1862, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -120, 1625, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 1837, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 8, 1837, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -24, 1625, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 80, -72, 1625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -88, 1625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -72, 1837, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 48, -120, 1837, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 2306, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1862, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, 8, 1862, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 8, 1862, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -40, 1862, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1862, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1862, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1862, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1862, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1862, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 64, -88, 1862, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -120, 2175, { .fields = { 32, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 56, 8, 1862, { .fields = { 56, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -16, 1625, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -64, 1625, { .fields = { 32, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -112, 1625, { .fields = { 8, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, 112, -120, 1625, { .fields = { 56, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -16, 1812, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -64, 1812, { .fields = { 40, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, -64, 1862, { .fields = { 64, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -112, 1812, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 8 } }, 64, -120, 1750, { .fields = { 16, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 24 } }, 48, -112, 1862, { .fields = { 64, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, 48, -120, 1862, { .fields = { 40, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_south_promenade_8018039C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 12, 0, 0, { 2, 0 } },
    { 26, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_south_promenade_801803C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_south_promenade_801803D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_south_promenade_801803E4[5] = {
    { { .empty = D_neo_ark_south_promenade_8017FE64 }, D_neo_ark_south_promenade_8017FE64, NULL },
    { { .elements = D_neo_ark_south_promenade_8017FE74 }, D_neo_ark_south_promenade_80180068, NULL },
    { { .elements = D_neo_ark_south_promenade_80180090 }, D_neo_ark_south_promenade_8018039C, NULL },
    { { .empty = D_neo_ark_south_promenade_801803C4 }, D_neo_ark_south_promenade_801803C4, NULL },
    { { .empty = D_neo_ark_south_promenade_801803D4 }, D_neo_ark_south_promenade_801803D4, NULL },
};

WorldCoordLight D_neo_ark_south_promenade_80180420[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 392, -740, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4997, 4915, 4894 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -332, 260, -220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1290, 1228 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_south_promenade_801804D0[1] = {
    { ARRAY_SIZE(D_neo_ark_south_promenade_80180420), D_neo_ark_south_promenade_80180420, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_south_promenade_801804E8[6] = {
    { NULL, NULL, NULL, { 5005, -1568, 2682, 0 }, { { 44, -1904, -2937, 0 }, { -44, -1904, 2938, 0 }, { 44, 1904, -2937, 0 }, { -44, 1904, 2938, 0 } }, { 4109, 0, 60, 0 }, { 0, 0, 4096, 0 }, 3500, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5272, -1600, 2635, 0 }, { { -17, -1904, 3156, 0 }, { 17, -1904, -3155, 0 }, { -17, 1904, 3156, 0 }, { 17, 1904, -3155, 0 } }, { -4101, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3683, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9087, -1441, 2401, 0 }, { { 309, -1904, 2737, 0 }, { -308, -1904, -2736, 0 }, { 309, 1904, 2737, 0 }, { -308, 1904, -2736, 0 } }, { -4071, 0, 458, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8784, -1601, 2400, 0 }, { { -323, -1904, -2719, 0 }, { 322, -1904, 2718, 0 }, { -323, 1904, -2719, 0 }, { 322, 1904, 2718, 0 } }, { 4084, 0, -485, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2C11, -1440, 7472, 0 }, { { 3787, -1904, 557, 0 }, { -3788, -1904, -558, 0 }, { 3787, 1904, 557, 0 }, { -3788, 1904, -558, 0 } }, { -597, 0, 4053, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2BD1, -1504, 7632, 0 }, { { -3440, -1904, -505, 0 }, { 3439, -1904, 504, 0 }, { -3440, 1904, -505, 0 }, { 3439, 1904, 504, 0 } }, { 594, 0, -4057, 0 }, { 0, 0, 4096, 0 }, 3957, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_south_promenade_801806B0[6] = {
    { NULL, NULL, NULL, { 1104, -48, 2416, 0 }, { { -496, 0, -1104, 0 }, { 496, 0, -1104, 0 }, { -496, 0, 1104, 0 }, { 496, 0, 1104, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D20, -1248, 0x3070, 0 }, { { 0, 1280, -1072, 0 }, { 0, -1280, -1072, 0 }, { 0, 1280, 1072, 0 }, { 0, -1280, 1072, 0 } }, { 4106, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1668, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 35, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2A30, -64, 0x30A0, 0 }, { { -288, 0, -592, 0 }, { 288, 0, -592, 0 }, { -288, 0, 592, 0 }, { 288, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_FACING, 24, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6464, -64, 912, 0 }, { { -5648, 0, -480, 0 }, { 5648, 0, -480, 0 }, { -5648, 0, 480, 0 }, { 5648, 0, 480, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 5655, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4720, -64, 3936, 0 }, { { -3872, 0, -480, 0 }, { 3872, 0, -480, 0 }, { -3872, 0, 480, 0 }, { 3872, 0, 480, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 3899, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B2F, -64, 1375, 0 }, { { -1874, 0, -1634, 0 }, { 2368, 0, 812, 0 }, { -2367, 0, -811, 0 }, { 1875, 0, 1635, 0 } }, { 0, 4099, 0, 0 }, { -2276, 0, 3406, 0 }, 2495, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_south_promenade_80180878[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_south_promenade_80180884[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_south_promenade_80180890[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_south_promenade_801808A8[2] = {
    { 56, 56, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105600_801482C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_south_promenade_801808C0[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202300_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_south_promenade_801808E4[13] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BDB0, D_neo_ark_south_promenade_80180878 },
    { D_map_neo_ark_8017BDC0, D_neo_ark_south_promenade_80180884 },
    { D_map_neo_ark_8017BDD0, D_neo_ark_south_promenade_80180890 },
    { D_map_neo_ark_8017BE40, D_neo_ark_south_promenade_801808A8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BE70, D_neo_ark_south_promenade_801808C0 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_south_promenade_8018094C[1] = {
    { NULL, NULL, { 4912, -1568, 7344, 0 }, { { -4560, 2496, 2832, 0 }, { 4560, 2496, -2832, 0 }, { -4560, -2496, 2832, 0 }, { 4560, -2496, -2832, 0 } }, { 2162, 0, 3481, 0 }, 5904, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_south_promenade_80180988 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_neo_ark_south_promenade_80180994[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_south_promenade_8018099C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_south_promenade_80180988 },
};

WorldCollisionSurfaceProperties D_neo_ark_south_promenade_801809A4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_south_promenade_80180988 },
};

WorldCollisionSurfaceProperties* D_neo_ark_south_promenade_801809AC[8] = {
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_8018099C,
    D_neo_ark_south_promenade_801809A4,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
};

/// Refuses every key-item use request in this room, returning zero to the item menu.
///
/// `itemId` is the selected inventory item; all arguments are ignored.
static s32 _neoArkSouthPromenadeRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { KEY_ITEM_REFUSED = 0 };

    return KEY_ITEM_REFUSED;
}

/// Message handler the room's message table names for one of its entries:
/// copies the incoming message onto the outgoing one, passes both to
/// `mapNeoArkResolveRoomVariant` and returns 1.
s32 func_neo_ark_south_promenade_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    return 1;
}

/// Ignores room commands from CAP and facing triggers, returning zero.
///
/// The command selector and its argument are unused in this room.
static s32 _neoArkSouthPromenadeIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room actions from direction triggers, returning zero.
///
/// `request` is borrowed for synchronous dispatch and is neither read nor retained.
static s32 _neoArkSouthPromenadeIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// State 0 of the room's message-driven task: parks the room's message table
/// in `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
static void func_neo_ark_south_promenade_8017D62C(Task* task)
{
    task->msgTable = D_neo_ark_south_promenade_8017F6B4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Keeps the room task idle in state 1 while its message table remains available.
static void _neoArkSouthPromenadeIdleRoomTask(Task* task)
{
}

/// Dispatches the room's message-driven task through its three-state table,
/// copied onto the stack before the call.
void func_neo_ark_south_promenade_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_south_promenade_8017D5C4;
    sp.funcs[task->state](task);
}

void neoArkSouthPromenadeRegisterGolemEffectsTask(Task* task)
{
    enum { REGISTER_EFFECTS,
           EFFECTS_REGISTERED };

    if (task->state == REGISTER_EFFECTS) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_SOUTH_PROMENADE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_SOUTH_PROMENADE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_SOUTH_PROMENADE_SPARK_BURST;
        task->state             = EFFECTS_REGISTERED;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void neoArkSouthPromenadeRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void neoArkSouthPromenadeRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_south_promenade_8017EA6C(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
