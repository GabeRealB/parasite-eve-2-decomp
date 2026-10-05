#include "rooms/neo_ark_north_promenade.h"

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
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"

extern TaskMessageEntry D_neo_ark_north_promenade_80181D68[];

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `RoomFx_TrailOffsets[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_north_promenade_8017D67C(Task* task);
static void func_neo_ark_north_promenade_8017D6C0(Task* task);

/// State table of the room's message-driven task, indexed by `Task::state`:
/// install the message table, idle, then kill the task.
static const TaskFuncTable3 D_neo_ark_north_promenade_8017D5C4 = {
    { func_neo_ark_north_promenade_8017D67C, func_neo_ark_north_promenade_8017D6C0, taskKill },
};

extern WorldCollisionGrid     D_neo_ark_north_promenade_801823EC[1];
extern WorldCollisionOccluder D_neo_ark_north_promenade_8018328C[1];
extern WorldCollisionTrigger  D_neo_ark_north_promenade_80182DB4[8];
extern WorldCollisionTrigger  D_neo_ark_north_promenade_801830C4[6];
extern WorldCoordRoomLights   D_neo_ark_north_promenade_80182D9C[1];

s32 func_neo_ark_north_promenade_8017D5D0(Task*, s32, s32, s32);
s32 func_neo_ark_north_promenade_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_north_promenade_8017D66C(Task*, s32, s32, s32);
s32 func_neo_ark_north_promenade_8017D674(Task*, s32, s32, s32);

TaskMessageEntry D_neo_ark_north_promenade_80181D68[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_north_promenade_8017D5D8 },
    { 5105, func_neo_ark_north_promenade_8017D5D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_north_promenade_8017D674 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_north_promenade_8017D66C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x01D9 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_north_promenade_80181DB4[1] = {
    { D_neo_ark_north_promenade_801823EC, D_neo_ark_north_promenade_80182DB4, D_neo_ark_north_promenade_801830C4, D_neo_ark_north_promenade_8018328C },
};

WorldCoordRoomLighting D_neo_ark_north_promenade_80181DC4[1] = {
    { D_neo_ark_north_promenade_80182D9C, NULL },
};

u8* D_neo_ark_north_promenade_80181DCC[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_north_promenade_80181DD0[1] = { 6 };

DirectionWarpEntry D_neo_ark_north_promenade_80181DD4[2] = {
    { { { .word = 3072 }, 0x2A43, 0, 1354 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2A43, 0, 1354 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 1193, 0, 0x2C19 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1193, 0, 0x2C19 }, { 0, 0, 0, 0 }, 0x550A0002, 0x550A0001, 0x550A0003, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POWER_PLANT_1 },
};

static SVECTOR _gNeoArkNorthPromenadeCollision04E2CNormals[12] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_normals.inc"
};

static SVECTOR _gNeoArkNorthPromenadeCollision04E2CVerts[79] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_verts.inc"
};

static WorldCollisionGridFace _gNeoArkNorthPromenadeCollision04E2CFaces[31] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_faces.inc"
};

static s16 _gNeoArkNorthPromenadeCollision04E2CCells[142] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkNorthPromenadeCollision04E2CCells[i])
static s16* _gNeoArkNorthPromenadeCollision04E2CTable[16] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_north_promenade_801823EC[1] = {
    { NULL, _gNeoArkNorthPromenadeCollision04E2CNormals, _gNeoArkNorthPromenadeCollision04E2CVerts, _gNeoArkNorthPromenadeCollision04E2CFaces, _gNeoArkNorthPromenadeCollision04E2CTable, -700, -700, 4, 4, 4000, 31 },
};

ViewCamera D_neo_ark_north_promenade_80182410[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x58D6, -7000 } }, 289 },
    { { { { -462, 0, 4069 }, { -44, 4095, -5 }, { -4069, -45, -462 } }, { -8700, 1302, -0x2EE0 } }, 289 },
    { { { { -413, 0, 4075 }, { -220, 4089, -22 }, { -4069, -221, -413 } }, { -0x319C, 1202, -0x2E18 } }, 289 },
    { { { { 4068, 0, 473 }, { -10, 4094, 93 }, { -472, -93, 4067 } }, { -0x2E7C, 1302, -2000 } }, 225 },
    { { { { -4045, 0, 643 }, { -7, 4095, -44 }, { -643, -44, -4044 } }, { -0x2E7C, 1302, -9500 } }, 235 },
    { { { { -4037, 0, 687 }, { 72, 4073, 423 }, { -683, 429, -4015 } }, { -0x2EE0, 1502, -5800 } }, 225 },
};

SpriteBatch D_neo_ark_north_promenade_801824E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_north_promenade_801824F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_north_promenade_80182508[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_north_promenade_80182518[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -88, 1587, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 16, 1587, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, 16, 1600, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -72, 16, 1887, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -104, -32, 1600, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 1887, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -80, 1887, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, -80, 1600, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, -80, 1587, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -112, -120, 1600, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -72, -120, 1887, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -40, -120, 2000, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -152, -120, 1587, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 0, 1600, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 0, 1612, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 8, 1912, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -48, 1600, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -48, 1612, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -64, -48, 1912, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -96, 1600, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -96, 1612, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -64, -96, 1912, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, -104, 1912, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, -104, 1612, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, -120, 1612, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, -120, 1600, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, -24, 1590, { .fields = { 0, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 48 } }, -112, -24, 1602, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, -24, 1890, { .fields = { 112, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, -72, 1590, { .fields = { 80, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, -112, -72, 1602, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, -72, 1890, { .fields = { 112, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 48 } }, -160, -120, 1590, { .fields = { 96, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 48 } }, -112, -120, 1602, { .fields = { 88, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -64, -120, 1890, { .fields = { 40, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 32 } }, -40, -120, 2000, { .fields = { 40, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_north_promenade_801827E8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 13, 0, 0, { 2, 0 } },
    { 26, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_north_promenade_80182810[23] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 750, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -104, 1812, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -8, 1813, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, 48, 1862, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -112, 48, 1862, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -64, 48, 1862, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 48, 1862, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 0, 1812, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 0, 1812, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -64, 0, 1812, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -16, 0, 1812, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -48, 1812, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -48, 1812, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -64, -40, 1812, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -112, -72, 1812, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -96, 1812, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -160, -96, 1775, { .fields = { 56, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -136, -88, 1775, { .fields = { 56, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -112, -64, 1775, { .fields = { 32, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -88, -48, 1775, { .fields = { 32, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -64, -32, 1775, { .fields = { 32, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -40, -16, 1775, { .fields = { 56, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -16, 0, 1775, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_north_promenade_801829DC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 15, 0, 0, { 2, 0 } },
    { 16, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_north_promenade_80182A04[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -32, 825, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 825, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 72, 825, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -64, 72, 825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -16, 72, 825, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, 72, 825, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 825, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 825, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -64, 24, 825, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -16, 24, 825, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, 24, 825, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 32, -8, 825, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 825, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 825, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -64, -24, 825, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -16, -24, 825, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 825, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 825, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -64, -64, 825, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 825, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -112, -96, 825, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 48 } }, 0, 8, 825, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, 0, -8, 825, { .fields = { 104, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 32 } }, -48, 8, 825, { .fields = { 16, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 48 } }, -48, -40, 825, { .fields = { 64, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 48 } }, -96, -40, 825, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 32 } }, -96, -72, 825, { .fields = { 16, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 24 } }, -136, -40, 825, { .fields = { 24, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 32 } }, -136, -72, 825, { .fields = { 24, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -136, -96, 825, { .fields = { 32, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 40 } }, -160, -112, 825, { .fields = { 64, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 32 } }, -160, -72, 825, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_north_promenade_80182C84[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_north_promenade_80182CA4[6] = {
    { { .empty = D_neo_ark_north_promenade_801824E8 }, D_neo_ark_north_promenade_801824E8, NULL },
    { { .empty = D_neo_ark_north_promenade_801824F8 }, D_neo_ark_north_promenade_801824F8, NULL },
    { { .empty = D_neo_ark_north_promenade_80182508 }, D_neo_ark_north_promenade_80182508, NULL },
    { { .elements = D_neo_ark_north_promenade_80182518 }, D_neo_ark_north_promenade_801827E8, NULL },
    { { .elements = D_neo_ark_north_promenade_80182810 }, D_neo_ark_north_promenade_801829DC, NULL },
    { { .elements = D_neo_ark_north_promenade_80182A04 }, D_neo_ark_north_promenade_80182C84, NULL },
};

WorldCoordLight D_neo_ark_north_promenade_80182CEC[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 392, -740, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4997, 4915, 4894 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -332, 260, -220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1290, 1228 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_north_promenade_80182D9C[1] = {
    { ARRAY_SIZE(D_neo_ark_north_promenade_80182CEC), D_neo_ark_north_promenade_80182CEC, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_north_promenade_80182DB4[8] = {
    { NULL, NULL, NULL, { 4829, -1568, 0x2D0C, 0 }, { { 60, -1904, -3017, 0 }, { -60, -1904, 3018, 0 }, { 60, 1904, -3017, 0 }, { -60, 1904, 3018, 0 } }, { 4096, 0, 81, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4967, -1600, 0x2CDB, 0 }, { { -65, -1904, 2884, 0 }, { 65, -1904, -2884, 0 }, { -65, 1904, 2884, 0 }, { 65, 1904, -2884, 0 } }, { -4109, 0, -93, 0 }, { 0, 0, 4096, 0 }, 3453, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8592, -1441, 0x2C30, 0 }, { { -112, -1904, 3056, 0 }, { 112, -1904, -3056, 0 }, { -112, 1904, 3056, 0 }, { 112, 1904, -3056, 0 } }, { -4099, 0, -151, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8352, -1601, 0x2C30, 0 }, { { 128, -1904, -3040, 0 }, { -128, -1904, 3040, 0 }, { 128, 1904, -3040, 0 }, { -128, 1904, 3040, 0 } }, { 4092, 0, 172, 0 }, { 0, 0, 4096, 0 }, 3584, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1440, 5856, 0 }, { { 3775, -1904, -166, 0 }, { -3781, -1904, 160, 0 }, { 3775, 1904, -166, 0 }, { -3781, 1904, 160, 0 } }, { 176, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2B11, -1504, 6031, 0 }, { { -3767, -1904, 156, 0 }, { 3757, -1904, -166, 0 }, { -3767, 1904, 156, 0 }, { 3757, 1904, -166, 0 } }, { -176, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9024, -1472, 2096, 0 }, { { -3690, -1904, -736, 0 }, { 3686, -1904, 732, 0 }, { -3690, 1904, -736, 0 }, { 3686, 1904, 732, 0 } }, { 799, 0, -4022, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9249, -1504, 2112, 0 }, { { 3276, -1904, 650, 0 }, { -3280, -1904, -654, 0 }, { 3276, 1904, 650, 0 }, { -3280, 1904, -654, 0 } }, { -801, 0, 4020, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_north_promenade_80183014[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_north_promenade_8018302C[2] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_north_promenade_80183044[2] = {
    { 56, 56, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105600_801482C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_north_promenade_8018305C[13] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B0F0, D_neo_ark_north_promenade_80183014 },
    { D_map_neo_ark_8017B120, D_neo_ark_north_promenade_8018302C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B160, D_neo_ark_north_promenade_80183044 },
    { NULL, NULL },
};

WorldCollisionTrigger D_neo_ark_north_promenade_801830C4[6] = {
    { NULL, NULL, NULL, { 0x2D20, -1104, 1504, 0 }, { { 0, 1088, -896, 0 }, { 0, -1088, -896, 0 }, { 0, 1088, 896, 0 }, { 0, -1088, 896, 0 } }, { 4098, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1408, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 18, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 976, -48, 0x2D00, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { 4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2A60, -64, 1536, 0 }, { { -256, 0, -720, 0 }, { 256, 0, -720, 0 }, { -256, 0, 720, 0 }, { 256, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 762, WORLD_COLLISION_TRIGGER_ACTION_FACING, 21, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4624, -64, 9952, 0 }, { { -3840, 0, -560, 0 }, { 3840, 0, -560, 0 }, { -3840, 0, 560, 0 }, { 3840, 0, 560, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 3873, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7088, -64, 0x3280, 0 }, { { -6448, 0, -560, 0 }, { 6448, 0, -560, 0 }, { -6448, 0, 560, 0 }, { 6448, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 6456, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x31A7, -64, 0x2E77, 0 }, { { -2805, 0, 441, 0 }, { -835, 0, -887, 0 }, { -2641, 0, 1549, 0 }, { 6283, 0, -1101, 0 } }, { 0, 4115, 0, 0 }, { -3290, 0, -2441, 0 }, 6374, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_neo_ark_north_promenade_8018328C[1] = {
    { NULL, NULL, { 6384, -1520, 5040, 0 }, { { -3056, 2672, -4368, 0 }, { 3056, 2672, 4368, 0 }, { -3056, -2672, -4368, 0 }, { 3056, -2672, 4368, 0 } }, { -3361, 0, 2350, 0 }, 5948, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_north_promenade_801832C8 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_neo_ark_north_promenade_801832D4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_north_promenade_801832DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_north_promenade_801832C8 },
};

WorldCollisionSurfaceProperties D_neo_ark_north_promenade_801832E4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_north_promenade_801832C8 },
};

WorldCollisionSurfaceProperties* D_neo_ark_north_promenade_801832EC[8] = {
    D_neo_ark_north_promenade_801832D4,
    D_neo_ark_north_promenade_801832DC,
    D_neo_ark_north_promenade_801832E4,
    D_neo_ark_north_promenade_801832D4,
    D_neo_ark_north_promenade_801832D4,
    D_neo_ark_north_promenade_801832D4,
    D_neo_ark_north_promenade_801832D4,
    D_neo_ark_north_promenade_801832D4,
};

/// Message handler the room's message table names for one of its entries:
/// accepts the message and does nothing.
s32 func_neo_ark_north_promenade_8017D5D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_neo_ark_north_promenade_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != GAME_AREA_NEO_ARK_FOREST_ZONE) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_UNLOCKED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_neo_ark_north_promenade_8017D66C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_neo_ark_north_promenade_8017D674(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room's message-driven task: parks the room's message table
/// in `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
static void func_neo_ark_north_promenade_8017D67C(Task* task)
{
    task->msgTable = D_neo_ark_north_promenade_80181D68;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

static void func_neo_ark_north_promenade_8017D6C0(Task* task)
{
}

/// Dispatches the room's message-driven task through its three-state table,
/// copied onto the stack before the call.
void func_neo_ark_north_promenade_8017D6C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_north_promenade_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_north_promenade_8017D720(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_NEO_ARK_NORTH_PROMENADE_MOTE;
        gRoomEffectHaloId         = EFFECT_NEO_ARK_NORTH_PROMENADE_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_NEO_ARK_NORTH_PROMENADE_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_NEO_ARK_NORTH_PROMENADE_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_NEO_ARK_NORTH_PROMENADE_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_NEO_ARK_NORTH_PROMENADE_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_NEO_ARK_NORTH_PROMENADE_SPARK_BURST;
        arg0->state               = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

void func_neo_ark_north_promenade_8017D7B0(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_neo_ark_north_promenade_8017E4F8(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_neo_ark_north_promenade_8017E890(Task* arg0)
{
    _roomVisualEffectsHaloOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_neo_ark_north_promenade_8017FCA0(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_north_promenade_8017FDD4(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_north_promenade_80180838(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_north_promenade_80181120(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
