#include "rooms/neo_ark_pavilion.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
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
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
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

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `waterDrawSpinU16` and `waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
// Exported instance: another image refers to this package's copy by name.
#define waterDriftTaskU16FixedCoord neoArkPavilionWaterDriftTaskU16FixedCoord
#define waterRippleTaskFixedCoord   neoArkPavilionWaterRippleTaskFixedCoord
#include "../../shared/water_effects.h"
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_neo_ark_pavilion_80187A1C[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_neo_ark_pavilion_80187A1C_value __asm__("D_neo_ark_pavilion_80187A1C");

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// The room's event task, spawned when its message handler latches an event.
extern TaskDesc D_neo_ark_pavilion_80183864;

/// The room's message table.
extern TaskMessageEntry D_neo_ark_pavilion_80183870[];

/// Offsets from the parent coordinate of the two points whose trails
/// `func_neo_ark_pavilion_80180714` records.
/// The second of those offsets, which the recording frames read by name.

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The save-location record and event the message handler latched for the
/// room's event task.
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

/// Set by the message handler when its last message latched an event and
/// spawned the room's event task; every such message clears it first.

s32 func_neo_ark_pavilion_8017E9EC(Task*, s32, s32, s32);
s32 func_neo_ark_pavilion_8017E9F4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_pavilion_8017EB3C(Task*, s32, s32, s32);
s32 func_neo_ark_pavilion_8017EB78(Task*, s32, s32, s32);

extern WorldCollisionGrid     D_neo_ark_pavilion_801841E4[1];
extern WorldCollisionOccluder D_neo_ark_pavilion_8018798C[1];
extern WorldCollisionTrigger  D_neo_ark_pavilion_80187584[2];
extern WorldCollisionTrigger  D_neo_ark_pavilion_8018772C[4];
extern WorldCollisionTrigger  D_neo_ark_pavilion_8018785C[4];
extern WorldCoordRoomLights   D_neo_ark_pavilion_8018756C[1];

extern TaskDesc D_actor_100400_80147E48;

TaskDesc D_neo_ark_pavilion_8018384C = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_pavilion_80183858 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

TaskDesc D_neo_ark_pavilion_80183864 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_pavilion_80183870[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_pavilion_8017E9F4 },
    { 5105, func_neo_ark_pavilion_8017E9EC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_pavilion_8017EB78 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_pavilion_8017EB3C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

#include "../../shared/room_visual_effects_disc_data.inc.c"

WorldCollisionRoomResources D_neo_ark_pavilion_801838B4[2] = {
    { D_neo_ark_pavilion_801841E4, D_neo_ark_pavilion_80187584, D_neo_ark_pavilion_8018772C, D_neo_ark_pavilion_8018798C },
    { D_neo_ark_pavilion_801841E4, D_neo_ark_pavilion_80187584, D_neo_ark_pavilion_8018785C, D_neo_ark_pavilion_8018798C },
};

WorldCoordRoomLighting D_neo_ark_pavilion_801838D4[2] = {
    { D_neo_ark_pavilion_8018756C, NULL },
    { D_neo_ark_pavilion_8018756C, NULL },
};

u8 D_neo_ark_pavilion_801838E4[8] = {
    1,
    4,
    5,
    2,
    3,
    6,
    7,
    0,
};

u8* D_neo_ark_pavilion_801838EC[2] = {
    gViewIdentityMap,
    D_neo_ark_pavilion_801838E4,
};

ViewCount D_neo_ark_pavilion_801838F4[2] = { 7, 7 };

DirectionWarpEntry D_neo_ark_pavilion_801838F8[3] = {
    { { { .word = 3072 }, 2000, 0, 9504 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 2000, 0, 9504 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -2700, 0, 7650 }, { 0, 0, 0, 0 }, { { .word = 0 }, -2700, 0, 7650 }, { 0, 0, 0, 0 }, 0x550D0004, 0x550D0003, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 3000, 0, 7500 }, { 0, 0, 0, 0 }, { { .word = 0 }, 3000, 0, 7500 }, { 0, 0, 0, 0 }, 0x550D0004, 0x550D0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

// Height override read by the shared waypoint actor.
s16 D_neo_ark_pavilion_801839A0 = 300;

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_pavilion_801839A4[8] = {
    { 3700, 1500, 5900, 0 },
    { -1600, 1500, 6000, 0 },
    { -6400, 1500, 6000, 0 },
    { -6200, 1500, 0x29CC, 0 },
    { -3900, 1500, 0x3458, 0 },
    { 4000, 1500, 0x332C, 0 },
    { 7328, 1500, 0x2BC0, 0 },
    { 8000, 1500, 5800, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_pavilion_801839E4[8] = {
    { -6400, 3000, 0x35E8, 0 },
    { -6400, 3000, 0x2710, 0 },
    { -8000, 3000, 5000, 0 },
    { -9400, 3000, 0x2AF8, 0 },
    { -5000, 3000, 0x38A4, 0 },
    { 7000, 3000, 0x34BC, 0 },
    { -0x34A0, 3000, 7136, 0 },
    { 6700, 3000, 0x3DB8, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_pavilion_80183A24[8] = {
    { -7000, 4500, 3300, 0 },
    { -0x2710, 4500, 7100, 0 },
    { -8600, 4500, 0x3520, 0 },
    { -8000, 4500, 9000, 0 },
    { -6800, 4500, 8500, 0 },
    { -6400, 4500, 3000, 0 },
    { -8100, 4500, 3800, 0 },
    { -0x2B5C, 4500, 5000, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_neo_ark_pavilion_80183A64[4] = {
    D_neo_ark_pavilion_801839A4,
    D_neo_ark_pavilion_801839E4,
    D_neo_ark_pavilion_80183A24,
    D_neo_ark_pavilion_801839A4,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_neo_ark_pavilion_80183A74[8] = {
    { 0, 0, 0, 0 },
    { 6600, 0, 4000, 0 },
    { -6000, 0, 7000, 0 },
    { -6000, 0, 0x2710, 0 },
    { -4000, 0, 0x32C8, 0 },
    { 5800, 0, 0x32C8, 0 },
    { 6600, 0, 5800, 0 },
    { 0, 0, 0, -1 },
};

static SVECTOR _gNeoArkPavilionCollision06C24Normals[8] = {
#include "assets/neo_ark_pavilion_collision_06C24_normals.inc"
};

static SVECTOR _gNeoArkPavilionCollision06C24Verts[102] = {
#include "assets/neo_ark_pavilion_collision_06C24_verts.inc"
};

static WorldCollisionGridFace _gNeoArkPavilionCollision06C24Faces[52] = {
#include "assets/neo_ark_pavilion_collision_06C24_faces.inc"
};

static s16 _gNeoArkPavilionCollision06C24Cells[156] = {
#include "assets/neo_ark_pavilion_collision_06C24_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkPavilionCollision06C24Cells[i])
static s16* _gNeoArkPavilionCollision06C24Table[6] = {
#include "assets/neo_ark_pavilion_collision_06C24_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_pavilion_801841E4[1] = {
    { NULL, _gNeoArkPavilionCollision06C24Normals, _gNeoArkPavilionCollision06C24Verts, _gNeoArkPavilionCollision06C24Faces, _gNeoArkPavilionCollision06C24Table, 5100, -6900, 3, 2, 4000, 52 },
};

ViewCamera D_neo_ark_pavilion_80184208[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x444B, -9500 } }, 329 },
    { { { { -619, 0, -4048 }, { -1181, 3917, 180 }, { 3872, 1195, -592 } }, { 3570, 2500, -0x280A } }, 257 },
    { { { { -703, 0, 4035 }, { 1103, 3939, 192 }, { -3881, 1120, -677 } }, { -3560, 2500, -0x28EB } }, 257 },
    { { { { -619, 0, -4048 }, { -1181, 3917, 180 }, { 3872, 1195, -592 } }, { 3570, 2500, -0x280A } }, 257 },
    { { { { -703, 0, 4035 }, { 1103, 3939, 192 }, { -3881, 1120, -677 } }, { -3560, 2500, -0x28EB } }, 257 },
    { { { { 0, 0, -4096 }, { -680, 4039, 0 }, { 4039, 680, 0 } }, { -2925, 905, -0x2B52 } }, 329 },
    { { { { -4064, 0, 507 }, { 197, 3773, 1580 }, { -467, 1592, -3744 } }, { -4725, 2460, -5305 } }, 289 },
};

SpriteBatch D_neo_ark_pavilion_80184304[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pavilion_80184314[149] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -8, 2183, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 8, 1125, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 8, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 48, 1125, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 975, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -72, 1125, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 755, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, -112, 978, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -72, 810, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -144, -64, 825, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -136, -64, 829, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, -104, 6299, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, -112, 1816, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -104, 1950, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -80, 1968, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -88, 1959, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -96, 1950, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, -40, 2067, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, -32, 2100, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -96, 7554, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -88, 2099, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -104, 5189, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -48, 2225, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -104, 2030, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 64, -104, 1856, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 72, -104, 1804, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -112, 9551, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -104, 1099, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -72, 1250, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1250, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, -8, 1250, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, -8, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 1187, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 1088, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -160, 48, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -152, 24, 1125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 32, 1087, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 32, 1162, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -120, 24, 1350, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 8, 1475, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, 0, 1687, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -24, 2000, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -40, 2062, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -40, 2062, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, -32, 2062, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 2500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -16, 2500, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, -32, 2500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -32, 1875, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -32, 2062, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 1750, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -24, 1625, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1562, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -16, 1500, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 104, -8, 1425, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 112, 0, 1375, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 0, 1312, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 8, 1225, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 16, 1250, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 32, 1250, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -56, 1650, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -40, -72, 1280, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -32, -24, 2437, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, -24, 2437, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, -24, 2392, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -32, 1862, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 16, -72, 1675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 24, -72, 1475, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -64, 1325, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -40, 1875, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -48, 1875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 1875, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -80, 1212, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -80, 1212, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 1500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -87, -7, 2250, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 9, -7, 2330, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -95, 1, 2187, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 1, 1, 2299, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -103, 9, 2077, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 1, 9, 2144, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -111, 17, 2000, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 1, 17, 1993, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -111, 25, 1849, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -7, 25, 1861, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -119, 33, 1740, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -39, 33, 1676, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 41, 33, 1740, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -127, 41, 1649, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -39, 41, 1617, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 49, 41, 1617, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -127, 49, 1517, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -39, 49, 1517, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 49, 49, 1517, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -135, 57, 1458, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -39, 57, 1426, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 57, 57, 1426, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -135, 65, 1399, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -39, 65, 1399, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 65, 65, 1399, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -143, 73, 1317, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -39, 73, 1285, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 73, 73, 1285, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -151, 81, 1283, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -39, 81, 1283, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 81, 1283, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -151, 89, 1224, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -39, 89, 1224, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 89, 1224, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -159, 97, 1154, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 8 } }, -55, 97, 1154, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 97, 1154, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -159, 105, 1085, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 8 } }, -55, 105, 1085, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 105, 1085, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 113, 860, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 8 } }, -55, 113, 887, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -159, 113, 887, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -40, 3750, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -40, 3750, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 3750, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -48, 3750, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, -48, 3750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, -48, 3750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 3750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -48, 3750, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -48, 3750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -48, 3750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -48, 3750, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, -48, 3750, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 136, -24, 3000, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 144, -32, 3000, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -40, 3000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -40, -64, 1500, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -32, -72, 1500, { .fields = { 72, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -24, -80, 1848, { .fields = { 72, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, -16, -80, 1500, { .fields = { 88, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -80, 1500, { .fields = { 32, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -80, 1500, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 24, -64, 1500, { .fields = { 96, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, 32, -56, 1375, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -8, -24, 2571, { .fields = { 8, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -24, 1694, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -24, 1875, { .fields = { 16, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -72, 1662, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -72, 1572, { .fields = { 16, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_pavilion_80184EB8[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 3, 0 } },
    { 32, 30, 0, 0, { 0, 0 } },
    { 62, 15, 0, 0, { 5, 0 } },
    { 77, 43, 0, 0, { 1, 0 } },
    { 120, 15, 0, 0, { 4, 0 } },
    { 135, 14, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pavilion_80184EF8[97] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -55, -8, 2285, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -71, 0, 2250, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 9, 0, 2250, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -79, 8, 2000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 9, 8, 2000, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -87, 16, 1875, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 1, 16, 1875, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -103, 24, 1700, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -31, 24, 1700, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 33, 24, 1700, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -111, 32, 1575, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -39, 32, 1575, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 41, 32, 1575, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -127, 40, 1475, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -47, 40, 1475, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 41, 40, 1475, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -135, 48, 1375, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -47, 48, 1375, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 41, 48, 1375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -151, 56, 1300, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -55, 56, 1300, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 41, 56, 1300, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -151, 64, 1225, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -55, 64, 1225, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 41, 64, 1225, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 72, 1150, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -63, 72, 1150, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 49, 72, 1150, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 80, 1100, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 80, 1100, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 49, 80, 1100, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 88, 1075, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -63, 88, 1075, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 49, 88, 1075, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 96, 1050, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -63, 96, 1050, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 49, 96, 1050, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 104, 1000, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 104, 1000, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 49, 104, 1000, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 49, 112, 950, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 112, 950, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 112, 950, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, -8, 2125, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 0, 2000, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 8, 2125, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 16, 1750, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -160, 24, 1625, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 32, 1663, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 40, 1101, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -40, 2150, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -40, 2134, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 64, 1163, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 88, 1125, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 40, 1125, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 56, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 0, 1125, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, -48, 812, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 144, -48, 1125, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 40, 1125, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 16, 876, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -48, 1125, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -40, 2079, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -48, 1913, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -48, 4604, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -48, 2245, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -160, -16, 1187, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 24, 1187, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 819, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 56, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, 32, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 32, 1125, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 1176, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 32, 1284, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 24, 1386, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 8, 1592, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1870, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -24, 2187, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -8, 2187, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -24, 2187, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -24, -8, 2187, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -24, 2187, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -32, 2187, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -24, 2187, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -32, 2078, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 1964, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -24, 1821, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -16, 1726, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, -16, 1687, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -16, 1687, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -16, 1452, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 0, 1394, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 0, 1307, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1277, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 8, 1196, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -152, 16, 1187, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 56, 1187, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pavilion_8018568C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 43, 0, 0, { 3, 0 } },
    { 43, 9, 0, 0, { 0, 0 } },
    { 52, 16, 0, 0, { 2, 0 } },
    { 68, 29, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pavilion_801856BC[157] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 8, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 8, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 48, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 1125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -72, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -152, -72, 813, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -64, 829, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 755, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -152, -112, 978, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, -104, 6299, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, -112, 1816, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -104, 1950, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, -88, 1962, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -96, 1950, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -40, 2058, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, -32, 2074, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -8, 2158, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -96, 7554, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -88, 2099, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -104, 2066, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -56, 2225, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -104, 2030, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -104, 1854, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -112, 9551, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -104, 1099, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -72, 1250, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, -8, 1250, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, -8, 1250, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 1187, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 1087, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -160, 48, 1125, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -152, 24, 1125, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 1125, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 32, 1086, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 32, 1171, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -120, 24, 1347, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, 8, 1465, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, 0, 1686, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, -24, 1950, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -40, 2000, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -40, 2000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, -32, 2000, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 2500, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -16, 2500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, -32, 2500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -32, 2125, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, -32, 2091, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -24, 2000, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -24, 1875, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1543, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -16, 1856, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 104, -8, 1771, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, -8, 2000, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -8, 2000, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, -16, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, -16, 2000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, -16, 2000, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 152, -16, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, -8, 2250, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, -8, 2330, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -96, 0, 2187, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 0, 0, 2299, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 8, 2077, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 0, 8, 2144, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -112, 16, 2000, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 0, 16, 1993, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -112, 24, 1849, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -32, 24, 1717, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 40, 24, 1861, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -120, 32, 1740, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -32, 32, 1676, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 40, 32, 1740, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -128, 40, 1649, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -40, 40, 1617, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 48, 40, 1553, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -128, 48, 1517, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -40, 48, 1517, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 56, 48, 1517, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -136, 56, 1458, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -48, 56, 1426, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 56, 56, 1426, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -136, 64, 1399, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -48, 64, 1399, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, 56, 64, 1399, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -144, 72, 1317, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -48, 72, 1285, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, 56, 72, 1285, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -152, 80, 1283, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -48, 80, 1283, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 80, 1283, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -152, 88, 1224, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -48, 88, 1224, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 88, 1224, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -160, 96, 1154, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -56, 96, 1154, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 96, 1154, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -160, 104, 1085, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -56, 104, 1085, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 104, 1085, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -160, 112, 887, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -56, 112, 887, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 112, 887, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -56, 1654, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -8, 1330, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -72, 1242, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, -72, 1224, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -24, -80, 1218, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -80, 1219, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -80, 1241, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -16, 1377, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -72, 1483, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -64, 1286, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -16, 1499, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -40, 1933, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -48, 1849, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -48, 1874, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -16, 2336, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -24, 2447, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -24, 2447, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -8, -24, 2419, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 0, -32, 2267, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -32, 1862, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 16, -56, 1646, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -16, 1756, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -40, 3625, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -40, 3625, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 3625, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -48, 3625, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, -48, 3625, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, -48, 3625, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 3625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -48, 3625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -48, 3625, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -48, 3625, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, -48, 3625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, -48, 3625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -24, 3125, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 144, -32, 3125, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -40, 3125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 32, -56, 1335, { .fields = { 104, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 24, -64, 1471, { .fields = { 120, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -72, 1572, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -72, 1662, { .fields = { 16, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 0, -80, 1629, { .fields = { 16, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -8, -80, 1650, { .fields = { 32, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -16, -80, 1646, { .fields = { 32, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, -80, 1674, { .fields = { 40, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -32, -72, 1671, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -40, -64, 1859, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -32, -24, 2491, { .fields = { 40, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 24 } }, -24, -24, 2452, { .fields = { 88, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 40 } }, -16, -32, 2452, { .fields = { 112, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -32, 2556, { .fields = { 48, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -32, 2257, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -24, 1862, { .fields = { 8, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -24, 1694, { .fields = { 32, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_pavilion_80186300[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 3, 0 } },
    { 29, 30, 0, 0, { 0, 0 } },
    { 59, 44, 0, 0, { 5, 0 } },
    { 103, 22, 0, 0, { 1, 0 } },
    { 125, 15, 0, 0, { 4, 0 } },
    { 140, 17, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pavilion_80186340[115] = {
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, -8, 2287, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, 0, 2250, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 8, 0, 2250, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -80, 8, 2000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 8, 8, 2000, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -88, 16, 1875, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 8, 16, 1875, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 24, 1700, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -32, 24, 1700, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, 24, 1700, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -112, 32, 1575, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -40, 32, 1575, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 40, 32, 1575, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -128, 40, 1475, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -48, 40, 1475, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 48, 40, 1475, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -136, 48, 1375, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -56, 48, 1375, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 48, 48, 1375, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -152, 56, 1300, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -64, 56, 1300, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 40, 56, 1300, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -152, 64, 1225, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -64, 64, 1225, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 40, 64, 1225, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 72, 1150, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 72, 1150, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 32, 72, 1150, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 80, 1100, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 80, 1100, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 32, 80, 1100, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 88, 1075, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 88, 1075, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, 32, 88, 1075, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 96, 1050, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 96, 1050, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, 32, 96, 1050, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 104, 1000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 104, 1000, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 32, 104, 1000, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 32, 112, 950, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 112, 950, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 112, 950, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -96, 1835, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -96, 1634, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 1775, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 879, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -16, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -160, 24, 1250, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -152, 24, 1250, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 64, 1250, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 1104, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -104, 1172, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -96, 1903, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -104, 8645, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -56, 4822, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -96, 2141, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -40, 2271, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -88, 2118, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -96, 2072, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -96, 0x27C8, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -88, 1962, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 8694, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -40, 2079, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -40, 1917, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -104, 1938, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -104, 1743, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -112, 7828, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -120, 7779, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -64, 809, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -64, 785, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -64, 1125, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 88, 1125, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 40, 1125, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 0, 1125, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 56, 1125, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 56, 1125, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 0, 1125, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, 0, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -32, 2148, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -32, 2983, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -40, 2251, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -24, 2051, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -152, 16, 1250, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 56, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 8, 1196, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1277, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 0, 1307, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 0, 1394, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -16, 1452, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, -16, 1625, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -16, 1625, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -16, 1726, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -24, 1821, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 2125, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -32, 2125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -32, 2275, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -24, 2259, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 2245, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -8, 2228, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 2983, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -8, 2741, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -8, 2155, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -8, 2741, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -24, 2107, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -24, 2098, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1870, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 8, 1592, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 24, 1386, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 1176, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 32, 1284, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 32, 1125, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, 32, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 56, 1125, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 819, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pavilion_80186C3C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 43, 0, 0, { 3, 0 } },
    { 43, 36, 0, 0, { 0, 0 } },
    { 79, 4, 0, 0, { 2, 0 } },
    { 83, 32, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pavilion_80186C6C[43] = {
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -160, -56, 375, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -160, 32, 375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -144, -56, 375, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -144, 32, 375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -128, -56, 375, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -128, 32, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -112, -56, 375, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -112, 32, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -56, 375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -32, 375, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -96, 32, 375, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -80, 32, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -80, -56, 375, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -64, -56, 375, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -64, 32, 375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -48, -56, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -48, 32, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -32, -56, 375, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -32, 32, 375, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -56, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, 32, 375, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -56, 375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, 32, 375, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -56, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, 32, 375, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -56, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, 32, 375, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 48, -56, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 48, 32, 375, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -56, 375, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -24, 375, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 64, 32, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -56, 375, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 80, 32, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, -56, 375, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -24, 375, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 96, 32, 375, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, -56, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, 32, 375, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 128, -56, 375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 128, 32, 375, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -40, 375, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 144, 32, 456, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pavilion_80186FC8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 43, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pavilion_80186FE0[48] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, -96, 1125, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -88, 1125, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, -40, 1000, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -160, 80, 625, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -160, 64, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -96, 64, 625, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -160, 48, 625, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -96, 48, 625, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -24, 48, 625, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -160, 32, 625, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -88, 32, 625, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -8, 32, 625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -160, 16, 750, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -80, 16, 750, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 16 } }, 8, 16, 750, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -160, 0, 750, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -72, 0, 750, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 16 } }, 16, 0, 750, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -160, -16, 875, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -72, -16, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, 16, -16, 875, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, 72, -80, 1125, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -88, 1125, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -64, 1000, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -48, 1000, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -32, 875, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -16, 750, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 750, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -160, -32, 875, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -72, -32, 875, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 16, -32, 875, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -48, 1000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -112, -48, 1000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -32, -48, 1000, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 32, -48, 1000, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -152, -56, 1000, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -64, 1000, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, -72, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -112, -56, 1000, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -40, -56, 1000, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 24, -56, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -72, -64, 1000, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -8, -64, 1000, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -80, 1125, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, -72, 1125, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, -80, 1125, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, -88, 1125, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pavilion_801873A0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 48, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_pavilion_801873B8[7] = {
    { { .empty = D_neo_ark_pavilion_80184304 }, D_neo_ark_pavilion_80184304, NULL },
    { { .elements = D_neo_ark_pavilion_80184314 }, D_neo_ark_pavilion_80184EB8, NULL },
    { { .elements = D_neo_ark_pavilion_80184EF8 }, D_neo_ark_pavilion_8018568C, NULL },
    { { .elements = D_neo_ark_pavilion_801856BC }, D_neo_ark_pavilion_80186300, NULL },
    { { .elements = D_neo_ark_pavilion_80186340 }, D_neo_ark_pavilion_80186C3C, NULL },
    { { .elements = D_neo_ark_pavilion_80186C6C }, D_neo_ark_pavilion_80186FC8, NULL },
    { { .elements = D_neo_ark_pavilion_80186FE0 }, D_neo_ark_pavilion_801873A0, NULL },
};

WorldCoordLight D_neo_ark_pavilion_8018740C[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_pavilion_8018756C[1] = {
    { ARRAY_SIZE(D_neo_ark_pavilion_8018740C), D_neo_ark_pavilion_8018740C, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_pavilion_80187584[2] = {
    { NULL, NULL, NULL, { -323, -1568, 9452, 0 }, { { -4, -1904, -3817, 0 }, { 4, -1904, 3818, 0 }, { -4, 1904, -3817, 0 }, { 4, 1904, 3818, 0 } }, { 4096, 0, -5, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -57, -1600, 9531, 0 }, { { -1, -1904, 3908, 0 }, { 1, -1904, -3908, 0 }, { -1, 1904, 3908, 0 }, { 1, 1904, -3908, 0 } }, { -4101, 0, -2, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_pavilion_8018761C[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pavilion_80187634[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 38, 38, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_203800_8014FD74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pavilion_80187658[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pavilion_80187670[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pavilion_80187694[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pavilion_801876AC[2] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gGolemPawnRookTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_pavilion_801876C4[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B3D0, D_neo_ark_pavilion_8018761C },
    { D_map_neo_ark_8017B410, D_neo_ark_pavilion_80187634 },
    { D_map_neo_ark_8017B470, D_neo_ark_pavilion_80187658 },
    { D_map_neo_ark_8017B4B0, D_neo_ark_pavilion_80187670 },
    { D_map_neo_ark_8017B4E0, D_neo_ark_pavilion_80187694 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B530, D_neo_ark_pavilion_801876AC },
    { NULL, NULL },
};

WorldCollisionTrigger D_neo_ark_pavilion_8018772C[4] = {
    { NULL, NULL, NULL, { 2240, -48, 9504, 0 }, { { -320, 0, -480, 0 }, { 320, 0, -480, 0 }, { -320, 0, 480, 0 }, { 320, 0, 480, 0 } }, { 0, 4116, 0, 0 }, { -4096, 0, 0, 0 }, 576, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3488, -48, 7456, 0 }, { { -1088, 0, -288, 0 }, { 1088, 0, -288, 0 }, { -1088, 0, 288, 0 }, { 1088, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 27, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2896, -48, 7456, 0 }, { { -1072, 0, -256, 0 }, { 1072, 0, -256, 0 }, { -1072, 0, 256, 0 }, { 1072, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4400, -64, 0x2BA0, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_pavilion_8018785C[4] = {
    { NULL, NULL, NULL, { 2240, -48, 9504, 0 }, { { -320, 0, -480, 0 }, { 320, 0, -480, 0 }, { -320, 0, 480, 0 }, { 320, 0, 480, 0 } }, { 0, 4116, 0, 0 }, { -4096, 0, 0, 0 }, 576, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -48, 7456, 0 }, { { -1136, 0, -288, 0 }, { 1136, 0, -288, 0 }, { -1136, 0, 288, 0 }, { 1136, 0, 288, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3008, -48, 7456, 0 }, { { -1024, 0, -256, 0 }, { 1024, 0, -256, 0 }, { -1024, 0, 256, 0 }, { 1024, 0, 256, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4400, -64, 0x2BA0, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_neo_ark_pavilion_8018798C[1] = {
    { NULL, NULL, { 3520, -200, 9440, 0 }, { { -1024, 1224, 0, 0 }, { 1024, 1224, 0, 0 }, { -1024, -1912, 0, 0 }, { 1024, -536, 0, 0 } }, { 0, 0, 4096, 0 }, 2157, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_pavilion_801879C8 = {
    0x1000000D,
    0x1000000F,
    0x1000000D,
};

WorldCollisionSurfaceProperties D_neo_ark_pavilion_801879D4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pavilion_801879DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pavilion_801879E4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_pavilion_801879C8 },
};

WorldCollisionSurfaceProperties* D_neo_ark_pavilion_801879EC[8] = {
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879DC,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879E4,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879D4,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_neo_ark_pavilion_80187A1C[4] = {
    0,
    38,
    -53,
    37,
};

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 NeoArkPavilion_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_neo_ark_pavilion_8017EB80(Task* arg0);
static void           func_neo_ark_pavilion_8017EBEC(Task* task);

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

s32 func_neo_ark_pavilion_8017E9EC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

static __inline__ s32 NeoArkPavilion_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_neo_ark_pavilion_80187A1C_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_neo_ark_pavilion_80183864, 0, 0, 0);
            D_neo_ark_pavilion_80187A1C_value = 1;
        }
        return 2;
    }
    return 1;
}

/// Room message handler for the pavilion's save location: copies the incoming
/// record onto the outgoing one and forwards both to `func_map_neo_ark_80179B14`. Message
/// `0xC` builds the room's event record - cap command 4, flag `0x17E` - and
/// hands it to `NeoArkPavilion_StartEvent`; every other message answers 1.
s32 func_neo_ark_pavilion_8017E9F4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL) {
        return 1;
    }
    event.capCmd   = 4;
    event.stageSnd = 0;
    event.flagId   = GAME_FLAG_PAVILION_TO_SUB_TUNNEL_SCENE;
    event.fade     = 0;
    return NeoArkPavilion_StartEvent(out, &event);
}

/// Room message handler: on message `1`, spawns the pavilion's cap entity —
/// id `5` once flag `0x141` is set, `1` while it is clear.
s32 func_neo_ark_pavilion_8017EB3C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(GAME_FLAG_141) != 0 ? 5 : 1, 1);
    }
    return 0;
}

s32 func_neo_ark_pavilion_8017EB78(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Room entry task tick: installs the room's message table (ids `0x13EE`-`0x13F1`),
/// hands the task to pointer slot 7, queues sound events `0x550D0005` and
/// `0x550D0006`, then advances state.
static void func_neo_ark_pavilion_8017EB80(Task* arg0)
{
    arg0->msgTable = D_neo_ark_pavilion_80183870;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    SndEvt_EnqueueType6(SOUND_NEO_ARK_PAVILION_AMBIENCE_1, 0, 0);
    SndEvt_EnqueueType6(SOUND_NEO_ARK_PAVILION_AMBIENCE_2, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_neo_ark_pavilion_8017EBEC(Task* task)
{
}

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_pavilion_8017EBF4`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_pavilion_8017D628 = {
    { func_neo_ark_pavilion_8017EB80, func_neo_ark_pavilion_8017EBEC, taskKill }
};

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_pavilion_8017D628`, copied to the stack first.
void func_neo_ark_pavilion_8017EBF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_pavilion_8017D628;
    sp.funcs[task->state](task);
}

#include "../../shared/water_ripple_task_fixed_coord.inc.c"

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16_fixed_coord.inc.c"

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

void func_neo_ark_pavilion_8017FC10(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectFlashId        = EFFECT_NEO_ARK_PAVILION_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_NEO_ARK_PAVILION_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_NEO_ARK_PAVILION_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_NEO_ARK_PAVILION_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_NEO_ARK_PAVILION_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_NEO_ARK_PAVILION_ORANGE_BURST_2;
        gRoomEffectWaterRippleId  = EFFECT_NEO_ARK_PAVILION_WATER_RIPPLE;
        gRoomEffectWaterSprayId   = EFFECT_NEO_ARK_PAVILION_WATER_SPRAY;
        arg0->state               = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_pavilion_8017FCB0(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_pavilion_80180714(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_pavilion_80180FFC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_neo_ark_pavilion_80181C44(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_neo_ark_pavilion_8018219C(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_neo_ark_pavilion_80182DFC(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
