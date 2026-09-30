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
#include "gameplay/direction_input.h"
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

#include "rooms/rooms_shared_8017dcb8.h"

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} NeoArkNorthPromenadeHaloStorage;
STATIC_ASSERT_SIZEOF(NeoArkNorthPromenadeHaloStorage, 20);
extern NeoArkNorthPromenadeHaloStorage D_neo_ark_north_promenade_80181D90;

extern GpMsgEntry D_neo_ark_north_promenade_80181D68[];

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `D_neo_ark_north_promenade_80181DA4[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_north_promenade_8017D67C(Task* task);
static void func_neo_ark_north_promenade_8017D6C0(Task* task);
static void func_neo_ark_north_promenade_8017DA7C(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_neo_ark_north_promenade_8017DD40(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_north_promenade_8017E164(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_north_promenade_8017EA3C(GfxCoord* coord, s16 size);
static void func_neo_ark_north_promenade_8017EF68(GfxCoord* arg0, s32 arg1);
static void func_neo_ark_north_promenade_8017F2E0(GfxCoord* arg0, s16 arg1, u8* arg2);
static void func_neo_ark_north_promenade_80180078(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_north_promenade_801804A4(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_north_promenade_80180D28(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_north_promenade_801813A8(GfxCoord* arg0, s16 arg1, u8* arg2);

/// State table of the room's message-driven task, indexed by `Task::state`:
/// install the message table, idle, then kill the task.
static const TaskFuncTable3 D_neo_ark_north_promenade_8017D5C4 = {
    { func_neo_ark_north_promenade_8017D67C, func_neo_ark_north_promenade_8017D6C0, taskKill },
};

extern GpGridParams   D_neo_ark_north_promenade_801823EC[1];
extern GpObj3A        D_neo_ark_north_promenade_8018328C[1];
extern GpObj4C        D_neo_ark_north_promenade_80182DB4[8];
extern GpObj4C        D_neo_ark_north_promenade_801830C4[6];
extern GpRoomCoordSet D_neo_ark_north_promenade_80182D9C[1];

s32 func_neo_ark_north_promenade_8017D5D0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_north_promenade_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_north_promenade_8017D66C(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_north_promenade_8017D674(Task*, s32, GpMessageArg, GpMessageArg);

GpMsgEntry D_neo_ark_north_promenade_80181D68[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_north_promenade_8017D5D8 },
    { 5105, func_neo_ark_north_promenade_8017D5D0 },
    { 5103, func_neo_ark_north_promenade_8017D674 },
    { 5104, func_neo_ark_north_promenade_8017D66C },
    { 0x7FFFFFFF, NULL },
};

NeoArkNorthPromenadeHaloStorage D_neo_ark_north_promenade_80181D90 = { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 473 };

SVECTOR D_neo_ark_north_promenade_80181DA4[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_neo_ark_north_promenade_80181DB4[1] = {
    { D_neo_ark_north_promenade_801823EC, D_neo_ark_north_promenade_80182DB4, D_neo_ark_north_promenade_801830C4, D_neo_ark_north_promenade_8018328C },
};

GpRoomCoordRec D_neo_ark_north_promenade_80181DC4[1] = {
    { D_neo_ark_north_promenade_80182D9C, NULL },
};

u8* D_neo_ark_north_promenade_80181DCC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_north_promenade_80181DD0[1] = {
    { { .bytes = { 6, 0 } } },
};

GpWarpRec D_neo_ark_north_promenade_80181DD4[2] = {
    { { .words = { 3072, 0x2A43, 0, 1354 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x2A43, 0, 1354 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 0, 0 },
    { { .words = { 1024, 1193, 0, 0x2C19 } }, { 0, 0, 0, 0 }, { .words = { 1024, 1193, 0, 0x2C19 } }, { 0, 0, 0, 0 }, 0x550A0002, 0x550A0001, 0x550A0003, 2, 0, 434 },
};

SVECTOR D_neo_ark_north_promenade_80181E44[12] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_normals.inc"
};

SVECTOR D_neo_ark_north_promenade_80181EA4[79] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_verts.inc"
};

GpGridFace D_neo_ark_north_promenade_8018211C[31] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_faces.inc"
};

s16 D_neo_ark_north_promenade_80182290[142] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_north_promenade_80182290[i])
s16* D_neo_ark_north_promenade_801823AC[16] = {
#include "assets/neo_ark_north_promenade_collision_04E2C_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_north_promenade_801823EC[1] = {
    { NULL, D_neo_ark_north_promenade_80181E44, D_neo_ark_north_promenade_80181EA4, D_neo_ark_north_promenade_8018211C, D_neo_ark_north_promenade_801823AC, -700, -700, 4, 4, 4000, 31 },
};

GpViewRec D_neo_ark_north_promenade_80182410[6] = {
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

GpSprtElem D_neo_ark_north_promenade_80182518[36] = {
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
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, -24, 1590, { .fields = { 0, 48 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -112, -24, 1602, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, -24, 1890, { .fields = { 112, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, -72, 1590, { .fields = { 80, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, -112, -72, 1602, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, -72, 1890, { .fields = { 112, 144 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -160, -120, 1590, { .fields = { 96, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -112, -120, 1602, { .fields = { 88, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -64, -120, 1890, { .fields = { 40, 144 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 40, 32 } }, -40, -120, 2000, { .fields = { 40, 104 } }, 128, 128, 128, 2 },
};

SpriteBatch D_neo_ark_north_promenade_801827E8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 13, 0, 0, { 2, 0 } },
    { 26, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_north_promenade_80182810[23] = {
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
    { 143, 0x4000, { .fields = { 24, 40 } }, -160, -96, 1775, { .fields = { 56, 184 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -136, -88, 1775, { .fields = { 56, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -112, -64, 1775, { .fields = { 32, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -88, -48, 1775, { .fields = { 32, 176 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -64, -32, 1775, { .fields = { 32, 216 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -40, -16, 1775, { .fields = { 56, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -16, 0, 1775, { .fields = { 32, 96 } }, 128, 128, 128, 2 },
};

SpriteBatch D_neo_ark_north_promenade_801829DC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 15, 0, 0, { 2, 0 } },
    { 16, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_north_promenade_80182A04[32] = {
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
    { 142, 0x4000, { .fields = { 40, 48 } }, 0, 8, 825, { .fields = { 72, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 0, -8, 825, { .fields = { 104, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 32 } }, -48, 8, 825, { .fields = { 16, 32 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -48, -40, 825, { .fields = { 64, 48 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -96, -40, 825, { .fields = { 64, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 32 } }, -96, -72, 825, { .fields = { 16, 168 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 40, 24 } }, -136, -40, 825, { .fields = { 24, 200 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 40, 32 } }, -136, -72, 825, { .fields = { 24, 64 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -136, -96, 825, { .fields = { 32, 224 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 40 } }, -160, -112, 825, { .fields = { 64, 136 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 32 } }, -160, -72, 825, { .fields = { 48, 0 } }, 128, 128, 128, 2 },
};

SpriteBatch D_neo_ark_north_promenade_80182C84[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_north_promenade_80182CA4[6] = {
    { { .empty = D_neo_ark_north_promenade_801824E8 }, D_neo_ark_north_promenade_801824E8, NULL },
    { { .empty = D_neo_ark_north_promenade_801824F8 }, D_neo_ark_north_promenade_801824F8, NULL },
    { { .empty = D_neo_ark_north_promenade_80182508 }, D_neo_ark_north_promenade_80182508, NULL },
    { { .elements = D_neo_ark_north_promenade_80182518 }, D_neo_ark_north_promenade_801827E8, NULL },
    { { .elements = D_neo_ark_north_promenade_80182810 }, D_neo_ark_north_promenade_801829DC, NULL },
    { { .elements = D_neo_ark_north_promenade_80182A04 }, D_neo_ark_north_promenade_80182C84, NULL },
};

GpLight D_neo_ark_north_promenade_80182CEC[2] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 392, -740, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4997, 4915, 4894, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -332, 260, -220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1310, 1290, 1228, { 0, 0 } },
};

GpRoomCoordSet D_neo_ark_north_promenade_80182D9C[1] = {
    { 2, D_neo_ark_north_promenade_80182CEC, 0, NULL, 0, NULL },
};

GpObj4C D_neo_ark_north_promenade_80182DB4[8] = {
    { NULL, NULL, NULL, { 4829, -1568, 0x2D0C, 0 }, { { 60, -1904, -3017, 0 }, { -60, -1904, 3018, 0 }, { 60, 1904, -3017, 0 }, { -60, 1904, 3018, 0 } }, { 4096, 0, 81, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 4967, -1600, 0x2CDB, 0 }, { { -65, -1904, 2884, 0 }, { 65, -1904, -2884, 0 }, { -65, 1904, 2884, 0 }, { 65, 1904, -2884, 0 } }, { -4109, 0, -93, 0 }, { 0, 0, 4096, 0 }, 3453, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 8592, -1441, 0x2C30, 0 }, { { -112, -1904, 3056, 0 }, { 112, -1904, -3056, 0 }, { -112, 1904, 3056, 0 }, { 112, 1904, -3056, 0 } }, { -4099, 0, -151, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 8352, -1601, 0x2C30, 0 }, { { 128, -1904, -3040, 0 }, { -128, -1904, 3040, 0 }, { 128, 1904, -3040, 0 }, { -128, 1904, 3040, 0 } }, { 4092, 0, 172, 0 }, { 0, 0, 4096, 0 }, 3584, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1440, 5856, 0 }, { { 3775, -1904, -166, 0 }, { -3781, -1904, 160, 0 }, { 3775, 1904, -166, 0 }, { -3781, 1904, 160, 0 } }, { 176, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 0x2B11, -1504, 6031, 0 }, { { -3767, -1904, 156, 0 }, { 3757, -1904, -166, 0 }, { -3767, 1904, 156, 0 }, { 3757, 1904, -166, 0 } }, { -176, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 9024, -1472, 2096, 0 }, { { -3690, -1904, -736, 0 }, { 3686, -1904, 732, 0 }, { -3690, 1904, -736, 0 }, { 3686, 1904, 732, 0 } }, { 799, 0, -4022, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 9249, -1504, 2112, 0 }, { { 3276, -1904, 650, 0 }, { -3280, -1904, -654, 0 }, { 3276, 1904, 650, 0 }, { -3280, 1904, -654, 0 } }, { -801, 0, 4020, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 5, 6, 129, 0 },
};

GpAreaTmdRec D_neo_ark_north_promenade_80183014[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_north_promenade_8018302C[2] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_north_promenade_80183044[2] = {
    { 56, 56, 0, 0, { 0, 0 }, D_801482C0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_north_promenade_8018305C[13] = {
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

GpObj4C D_neo_ark_north_promenade_801830C4[6] = {
    { NULL, NULL, NULL, { 0x2D20, -1104, 1504, 0 }, { { 0, 1088, -896, 0 }, { 0, -1088, -896, 0 }, { 0, 1088, 896, 0 }, { 0, -1088, 896, 0 } }, { 4098, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1408, 0, 7, 18, 3, 0 },
    { NULL, NULL, NULL, { 976, -48, 0x2D00, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { 4096, 0, 0, 0 }, 1476, 0, 11, 33, 2, 0 },
    { NULL, NULL, NULL, { 0x2A60, -64, 1536, 0 }, { { -256, 0, -720, 0 }, { 256, 0, -720, 0 }, { -256, 0, 720, 0 }, { 256, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 762, 1, 21, 64, 2, 0 },
    { NULL, NULL, NULL, { 4624, -64, 9952, 0 }, { { -3840, 0, -560, 0 }, { 3840, 0, -560, 0 }, { -3840, 0, 560, 0 }, { 3840, 0, 560, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 3873, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 7088, -64, 0x3280, 0 }, { { -6448, 0, -560, 0 }, { 6448, 0, -560, 0 }, { -6448, 0, 560, 0 }, { 6448, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 6456, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x31A7, -64, 0x2E77, 0 }, { { -2805, 0, 441, 0 }, { -835, 0, -887, 0 }, { -2641, 0, 1549, 0 }, { 6283, 0, -1101, 0 } }, { 0, 4115, 0, 0 }, { -3290, 0, -2441, 0 }, 6374, 2, 2, 0, 130, 0 },
};

GpObj3A D_neo_ark_north_promenade_8018328C[1] = {
    { NULL, NULL, { 6384, -1520, 5040, 0 }, { { -3056, 2672, -4368, 0 }, { 3056, 2672, 4368, 0 }, { -3056, -2672, -4368, 0 }, { 3056, -2672, 4368, 0 } }, { -3361, 0, 2350, 0 }, { 60, 23 }, 129, 0 },
};

s32 D_neo_ark_north_promenade_801832C8[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_neo_ark_north_promenade_801832D4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_north_promenade_801832DC[1] = {
    { 0, 0, 1, 0, D_neo_ark_north_promenade_801832C8 },
};

GpRoomParamRec D_neo_ark_north_promenade_801832E4[1] = {
    { 0, 1, 0, 0, D_neo_ark_north_promenade_801832C8 },
};

GpRoomParamRec* D_neo_ark_north_promenade_801832EC[8] = {
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
s32 func_neo_ark_north_promenade_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_north_promenade_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != 0xB) {
        return 1;
    }
    if (GameFlag_GetNibble(0xF6) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_neo_ark_north_promenade_8017D66C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_north_promenade_8017D674(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's message-driven task: parks the room's message table
/// in `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
static void func_neo_ark_north_promenade_8017D67C(Task* task)
{
    task->msgTable = D_neo_ark_north_promenade_80181D68;
    Game_SetPtrSlot(task, 7);
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
        D_80115728  = 0x6024F;
        D_80115744  = 0x6025B;
        D_8011573C  = 0x60266;
        D_80115720  = 0x60272;
        D_80115758  = 0x601D8;
        D_8011572C  = 0x601F4;
        D_80115750  = 0x60210;
        arg0->state = 1;
    }
}

/// Drifting mote: moves the effect's coordinate vertically and draws it every
/// other tick. State 1 ramps brightness up before fading; state 2 holds its
/// initial brightness until the fade. Releases the work block when dark or
/// once the room's event state reaches 4.
void func_neo_ark_north_promenade_8017D7B0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_neo_ark_north_promenade_8017DA7C(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_neo_ark_north_promenade_8017DA7C(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void func_neo_ark_north_promenade_8017DA7C(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;

    row           = arg2 >> 12;
    arg2         &= 0xFFF;
    pal           = arg3 >> 12;
    arg3         &= 0xFF;
    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim.
static void func_neo_ark_north_promenade_8017DD40(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   outer;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
static void func_neo_ark_north_promenade_8017E164(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Expanding halo: state 0 parks the effect frame on its anchor and derives
/// the fade step from the spawn argument, state 1 draws the halo (plus a
/// half-bright echo on odd ticks) and a ring while the level ramps up, and
/// state 2 fades it back out through the afterglow before releasing the work
/// block.
void func_neo_ark_north_promenade_8017E4F8(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GfxCoord*   coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.coordBody->coord;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot                 = (GpMtxWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00_m01        = 0x1000;
                rot->m02_m10        = 0;
                rot->m11_m12        = 0x1000;
                rot->m20_m21        = 0;
                rot->m22            = 0x1000;
                coord->coord.t[0]   = mem->pos.vx;
                coord->coord.t[1]   = mem->pos.vy;
                coord->coord.t[2]   = mem->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                shift                 = arg0->spawnArg1.halves.high;
                mem->index            = shift;
                arg0->spawnArg1.value = arg0->spawnArg1.halves.low;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> D_neo_ark_north_promenade_80181D90.entries[mem->index].r;
                rgb[1]                 = mem->scale >> D_neo_ark_north_promenade_80181D90.entries[mem->index].g;
                rgb[2]                 = mem->scale >> D_neo_ark_north_promenade_80181D90.entries[mem->index].b;
                func_neo_ark_north_promenade_8017E164(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_neo_ark_north_promenade_8017E164(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_neo_ark_north_promenade_8017DD40(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_neo_ark_north_promenade_80181D90.entries[mem->index].r;
                    rgb[1] = mem->scale >> D_neo_ark_north_promenade_80181D90.entries[mem->index].g;
                    rgb[2] = mem->scale >> D_neo_ark_north_promenade_80181D90.entries[mem->index].b;
                    func_neo_ark_north_promenade_8017F2E0(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Twin-ring burst: seeds two levels on the first tick, then each frame draws
/// the halo and a second ring at the growing radius while a wider, dimmer
/// echo fades out behind them, releasing the work block once the main level
/// runs down.
void func_neo_ark_north_promenade_8017E890(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.coordBody->coord;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_neo_ark_north_promenade_8017E164(coord, (s16)(step * 2), rgb);
        func_neo_ark_north_promenade_8017EA3C(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_neo_ark_north_promenade_8017DD40(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_neo_ark_north_promenade_8017EA3C(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    shifted                               = intensity << 0x10;
    light->head.g                         = shifted >> 0x11;
    light->head.b                         = shifted >> 0x12;
    light->head.u.at.local.t[0]           = coord->coord.t[0];
    light->head.u.at.local.t[1]           = coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                           = random;
    block                                 = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                         = coord->workm.t[0];
    block->vec.vy                         = coord->workm.t[1];
    block->vec.vz                         = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_neo_ark_north_promenade_8017EF68(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_neo_ark_north_promenade_8017EF68(GfxCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges: sixteen around the circle, alternating full radius at half
/// intensity and half radius at full intensity, then four spikes a quarter
/// turn apart, two reaching the full radius and two twice it. `arg1` sizes it
/// in world units scaled by depth; every wedge fades to black at its rim.
static void func_neo_ark_north_promenade_8017F2E0(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Spark emitter: each tick turns the effect's angle on by a random 0x200..0x3FF,
/// builds a velocity on a 3/16-scaled circle in X and Z with a downward Y that
/// grows with age, and spawns a child effect at the task's coordinate frame.
/// Releases its work block after 0x15 ticks, or once the room's event state
/// reaches 4.
void func_neo_ark_north_promenade_8017FCA0(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.coordBody->coord;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}

/// A flash effect task. State 1 ramps its level up over `spawnArg1` ticks,
/// drawing two fans and an inward-shrinking ring in a colour derived from the
/// level, and queues a fade quad in that colour when it peaks; state 2 fades
/// out through the star draw before the work block is released.
void func_neo_ark_north_promenade_8017FDD4(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    u8         rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        switch (arg0->state) {
            case 0:
                mem->scale  = 0;
                mem->angle  = 0x80;
                mem->step   = 0x100 / arg0->spawnArg1.value;
                arg0->state = 1;
                break;
            case 1:
                mem->scale += mem->step;
                mem->angle += mem->step;
                arg0->spawnArg1.value--;
                rgb[0] = mem->scale;
                rgb[1] = mem->scale >> 2;
                rgb[2] = mem->scale >> 1;
                func_neo_ark_north_promenade_801804A4(coord, mem->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_north_promenade_801804A4(coord, (s16)((u16)mem->angle * 2), rgb);
                func_neo_ark_north_promenade_80180078(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    rgb[0]      = mem->scale;
                    rgb[1]      = mem->scale >> 2;
                    rgb[2]      = mem->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale;
                    rgb[1] = mem->scale >> 2;
                    rgb[2] = mem->scale >> 1;
                    func_neo_ark_north_promenade_801813A8(coord, mem->angle * 3, rgb);
                    mem->scale -= 0x10;
                    mem->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(mem, arg0);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim. The same ring as
/// `func_neo_ark_north_promenade_8017DD40`, with its scratch block laid out
/// differently.
static void func_neo_ark_north_promenade_80180078(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                next;
    s32                outer;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
static void func_neo_ark_north_promenade_801804A4(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomFanScratch);
}

/// Twin smoke trail. State 0 allocates sixteen `GfxCoord`s, eight per
/// trail, and seeds them all from the two spawn offsets so each trail starts
/// collapsed on its origin. State 1 advances one slot of each trail per frame,
/// re-derives all sixteen against the view and draws them. The task frees
/// itself once its age reaches the spawn argument, and idles while the room's
/// event state is 2 or more.
void func_neo_ark_north_promenade_80180838(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_neo_ark_north_promenade_80181DA4[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_north_promenade_80181DA4[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_north_promenade_80181DA4[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_north_promenade_80181DA4[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_neo_ark_north_promenade_80181DA4[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_north_promenade_80180D28(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade. Dropped when `gte_stflg` is negative.
static void func_neo_ark_north_promenade_80180D28(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Spark burst. State 0 fires the burst's effect, then a non-zero spawn
/// argument starts a stream of jittered sparks (state 1) and a zero one a pair
/// of rings whose radius grows and brightness falls each frame (state 2).
/// Either way the task reaches state 3 after seven frames and releases its
/// work block, or earlier once the room's event state reaches 4.
void func_neo_ark_north_promenade_80181120(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_neo_ark_north_promenade_80180078(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_north_promenade_80180078(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges: sixteen around the circle, alternating full radius at half
/// intensity and half radius at full intensity, then four spikes a quarter
/// turn apart, two reaching the full radius and two twice it. `arg1` sizes it
/// in world units scaled by depth; every wedge fades to black at its rim.
static void func_neo_ark_north_promenade_801813A8(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
