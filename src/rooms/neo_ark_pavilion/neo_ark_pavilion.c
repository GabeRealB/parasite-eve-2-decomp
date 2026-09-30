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

#include "actors/waypoints.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
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
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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
extern GpMsgEntry D_neo_ark_pavilion_80183870[];

/// Offsets from the parent coordinate of the two points whose trails
/// `func_neo_ark_pavilion_80180714` records.
/// The second of those offsets, which the recording frames read by name.

/// Per-tint channel shifts for the glowing disc, indexed by the tint the spawn
/// argument selects.
extern RoomHaloShade D_neo_ark_pavilion_801838A8[];

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage D_neo_ark_pavilion_80187A0C;

/// The save-location record and event the message handler latched for the
/// room's event task.
extern GpSaveLoc        D_neo_ark_pavilion_80187A14;
extern RoomLatchedEvent D_neo_ark_pavilion_80187A20;

/// Set by the message handler when its last message latched an event and
/// spawned the room's event task; every such message clears it first.

static void func_neo_ark_pavilion_8017ED98(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_neo_ark_pavilion_8017F588(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_neo_ark_pavilion_8017F974(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_neo_ark_pavilion_8017FF54(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_pavilion_80180380(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_pavilion_80180C04(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_pavilion_80181284(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_pavilion_801823C0(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_neo_ark_pavilion_80182644(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_pavilion_80182A68(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_pavilion_80182FA8(GfxCoord* coord, s16 size);
static void func_neo_ark_pavilion_801834D4(GfxCoord* arg0, s32 arg1);

void func_neo_ark_pavilion_8017E854(Task*);
s32  func_neo_ark_pavilion_8017E9EC(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_neo_ark_pavilion_8017E9F4(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32  func_neo_ark_pavilion_8017EB3C(Task*, s32, s32, GpMessageArg);
s32  func_neo_ark_pavilion_8017EB78(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_neo_ark_pavilion_801841E4[1];
extern GpObj3A        D_neo_ark_pavilion_8018798C[1];
extern GpObj4C        D_neo_ark_pavilion_80187584[2];
extern GpObj4C        D_neo_ark_pavilion_8018772C[4];
extern GpObj4C        D_neo_ark_pavilion_8018785C[4];
extern GpRoomCoordSet D_neo_ark_pavilion_8018756C[1];

void func_neo_ark_pavilion_8017D660(Task*);
void func_neo_ark_pavilion_8017E2B4(Task*);

extern TaskDesc D_80147E48;

TaskDesc D_neo_ark_pavilion_8018384C = { 0, 192, func_neo_ark_pavilion_8017D660, { .model = NULL } };

TaskDesc D_neo_ark_pavilion_80183858 = { 0, 192, func_neo_ark_pavilion_8017E2B4, { .model = NULL } };

TaskDesc D_neo_ark_pavilion_80183864 = { 0, 32, func_neo_ark_pavilion_8017E854, { .model = NULL } };

GpMsgEntry D_neo_ark_pavilion_80183870[5] = {
    { 5102, func_neo_ark_pavilion_8017E9F4 },
    { 5105, func_neo_ark_pavilion_8017E9EC },
    { 5103, func_neo_ark_pavilion_8017EB78 },
    { 5104, func_neo_ark_pavilion_8017EB3C },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_neo_ark_pavilion_80183898[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

RoomHaloShade D_neo_ark_pavilion_801838A8[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

GpRoomObjRec D_neo_ark_pavilion_801838B4[2] = {
    { D_neo_ark_pavilion_801841E4, D_neo_ark_pavilion_80187584, D_neo_ark_pavilion_8018772C, D_neo_ark_pavilion_8018798C },
    { D_neo_ark_pavilion_801841E4, D_neo_ark_pavilion_80187584, D_neo_ark_pavilion_8018785C, D_neo_ark_pavilion_8018798C },
};

GpRoomCoordRec D_neo_ark_pavilion_801838D4[2] = {
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
    D_8010CAF8,
    D_neo_ark_pavilion_801838E4,
};

GpViewCountRec D_neo_ark_pavilion_801838F4[2] = {
    { { .bytes = { 7, 0 } } },
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_neo_ark_pavilion_801838F8[3] = {
    { { .words = { 3072, 2000, 0, 9504 } }, { 0, 0, 0, 0 }, { .words = { 3072, 2000, 0, 9504 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 0, -2700, 0, 7650 } }, { 0, 0, 0, 0 }, { .words = { 0, -2700, 0, 7650 } }, { 0, 0, 0, 0 }, 0x550D0004, 0x550D0003, 0, 3, 0, 0 },
    { { .words = { 0, 3000, 0, 7500 } }, { 0, 0, 0, 0 }, { .words = { 0, 3000, 0, 7500 } }, { 0, 0, 0, 0 }, 0x550D0004, 0x550D0003, 0, 2, 0, 0 },
};

// Height override read by the shared waypoint actor.
ActorWaypointHeight D_neo_ark_pavilion_801839A0 = { .storage = 300 };

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

SVECTOR D_neo_ark_pavilion_80183AB4[8] = {
#include "assets/neo_ark_pavilion_collision_06C24_normals.inc"
};

SVECTOR D_neo_ark_pavilion_80183AF4[102] = {
#include "assets/neo_ark_pavilion_collision_06C24_verts.inc"
};

GpGridFace D_neo_ark_pavilion_80183E24[52] = {
#include "assets/neo_ark_pavilion_collision_06C24_faces.inc"
};

s16 D_neo_ark_pavilion_80184094[156] = {
#include "assets/neo_ark_pavilion_collision_06C24_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_pavilion_80184094[i])
s16* D_neo_ark_pavilion_801841CC[6] = {
#include "assets/neo_ark_pavilion_collision_06C24_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_pavilion_801841E4[1] = {
    { NULL, D_neo_ark_pavilion_80183AB4, D_neo_ark_pavilion_80183AF4, D_neo_ark_pavilion_80183E24, D_neo_ark_pavilion_801841CC, 5100, -6900, 3, 2, 4000, 52 },
};

GpViewRec D_neo_ark_pavilion_80184208[7] = {
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

GpSprtElem D_neo_ark_pavilion_80184314[149] = {
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
    { 143, 0x4000, { .fields = { 8, 80 } }, -40, -64, 1500, { .fields = { 88, 176 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -32, -72, 1500, { .fields = { 72, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -24, -80, 1848, { .fields = { 72, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 88 } }, -16, -80, 1500, { .fields = { 88, 88 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -80, 1500, { .fields = { 32, 56 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -80, 1500, { .fields = { 40, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 24, -64, 1500, { .fields = { 96, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 32, -56, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 8, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -8, -24, 2571, { .fields = { 8, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -24, 1694, { .fields = { 16, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -24, 1875, { .fields = { 16, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -72, 1662, { .fields = { 8, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -72, 1572, { .fields = { 16, 48 } }, 128, 128, 128, 2 },
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

GpSprtElem D_neo_ark_pavilion_80184EF8[97] = {
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

GpSprtElem D_neo_ark_pavilion_801856BC[157] = {
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
    { 143, 0x4000, { .fields = { 8, 88 } }, 32, -56, 1335, { .fields = { 104, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 24, -64, 1471, { .fields = { 120, 104 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -72, 1572, { .fields = { 8, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -72, 1662, { .fields = { 16, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 0, -80, 1629, { .fields = { 16, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -8, -80, 1650, { .fields = { 32, 56 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -16, -80, 1646, { .fields = { 32, 104 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, -80, 1674, { .fields = { 40, 56 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -32, -72, 1671, { .fields = { 24, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -40, -64, 1859, { .fields = { 88, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -32, -24, 2491, { .fields = { 40, 224 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -24, -24, 2452, { .fields = { 88, 72 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -16, -32, 2452, { .fields = { 112, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -32, 2556, { .fields = { 48, 168 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -32, 2257, { .fields = { 40, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -24, 1862, { .fields = { 8, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -24, 1694, { .fields = { 32, 152 } }, 128, 128, 128, 2 },
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

GpSprtElem D_neo_ark_pavilion_80186340[115] = {
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

GpSprtElem D_neo_ark_pavilion_80186C6C[43] = {
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

GpSprtElem D_neo_ark_pavilion_80186FE0[48] = {
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

GpSprtRec D_neo_ark_pavilion_801873B8[7] = {
    { { .empty = D_neo_ark_pavilion_80184304 }, D_neo_ark_pavilion_80184304, NULL },
    { { .elements = D_neo_ark_pavilion_80184314 }, D_neo_ark_pavilion_80184EB8, NULL },
    { { .elements = D_neo_ark_pavilion_80184EF8 }, D_neo_ark_pavilion_8018568C, NULL },
    { { .elements = D_neo_ark_pavilion_801856BC }, D_neo_ark_pavilion_80186300, NULL },
    { { .elements = D_neo_ark_pavilion_80186340 }, D_neo_ark_pavilion_80186C3C, NULL },
    { { .elements = D_neo_ark_pavilion_80186C6C }, D_neo_ark_pavilion_80186FC8, NULL },
    { { .elements = D_neo_ark_pavilion_80186FE0 }, D_neo_ark_pavilion_801873A0, NULL },
};

GpLight D_neo_ark_pavilion_8018740C[4] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
};

GpRoomCoordSet D_neo_ark_pavilion_8018756C[1] = {
    { 4, D_neo_ark_pavilion_8018740C, 0, NULL, 0, NULL },
};

GpObj4C D_neo_ark_pavilion_80187584[2] = {
    { NULL, NULL, NULL, { -323, -1568, 9452, 0 }, { { -4, -1904, -3817, 0 }, { 4, -1904, 3818, 0 }, { -4, 1904, -3817, 0 }, { 4, 1904, 3818, 0 } }, { 4096, 0, -5, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -57, -1600, 9531, 0 }, { { -1, -1904, 3908, 0 }, { 1, -1904, -3908, 0 }, { -1, 1904, 3908, 0 }, { 1, 1904, -3908, 0 } }, { -4101, 0, -2, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 2, 129, 0 },
};

GpAreaTmdRec D_neo_ark_pavilion_8018761C[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pavilion_80187634[3] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 38, 38, 1, 0, { 0, 0 }, D_8014FD74 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pavilion_80187658[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pavilion_80187670[3] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pavilion_80187694[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pavilion_801876AC[2] = {
    { 57, 57, 0, 0, { 0, 0 }, D_801491F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_pavilion_801876C4[13] = {
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

GpObj4C D_neo_ark_pavilion_8018772C[4] = {
    { NULL, NULL, NULL, { 2240, -48, 9504, 0 }, { { -320, 0, -480, 0 }, { 320, 0, -480, 0 }, { -320, 0, 480, 0 }, { 320, 0, 480, 0 } }, { 0, 4116, 0, 0 }, { -4096, 0, 0, 0 }, 576, 0, 12, 18, 2, 0 },
    { NULL, NULL, NULL, { -3488, -48, 7456, 0 }, { { -1088, 0, -288, 0 }, { 1088, 0, -288, 0 }, { -1088, 0, 288, 0 }, { 1088, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1123, 0, 27, 33, 2, 0 },
    { NULL, NULL, NULL, { 2896, -48, 7456, 0 }, { { -1072, 0, -256, 0 }, { 1072, 0, -256, 0 }, { -1072, 0, 256, 0 }, { 1072, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1101, 2, 3, 1, 2, 0 },
    { NULL, NULL, NULL, { 4400, -64, 0x2BA0, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 814, 2, 1, 255, 130, 0 },
};

GpObj4C D_neo_ark_pavilion_8018785C[4] = {
    { NULL, NULL, NULL, { 2240, -48, 9504, 0 }, { { -320, 0, -480, 0 }, { 320, 0, -480, 0 }, { -320, 0, 480, 0 }, { 320, 0, 480, 0 } }, { 0, 4116, 0, 0 }, { -4096, 0, 0, 0 }, 576, 0, 12, 18, 2, 0 },
    { NULL, NULL, NULL, { -3472, -48, 7456, 0 }, { { -1136, 0, -288, 0 }, { 1136, 0, -288, 0 }, { -1136, 0, 288, 0 }, { 1136, 0, 288, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 1166, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 3008, -48, 7456, 0 }, { { -1024, 0, -256, 0 }, { 1024, 0, -256, 0 }, { -1024, 0, 256, 0 }, { 1024, 0, 256, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1055, 0, 14, 49, 2, 0 },
    { NULL, NULL, NULL, { 4400, -64, 0x2BA0, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 814, 2, 1, 255, 130, 0 },
};

GpObj3A D_neo_ark_pavilion_8018798C[1] = {
    { NULL, NULL, { 3520, -200, 9440, 0 }, { { -1024, 1224, 0, 0 }, { 1024, 1224, 0, 0 }, { -1024, -1912, 0, 0 }, { 1024, -536, 0, 0 } }, { 0, 0, 4096, 0 }, { 109, 8 }, 129, 0 },
};

s32 D_neo_ark_pavilion_801879C8[3] = {
    0x1000000D,
    0x1000000F,
    0x1000000D,
};

GpRoomParamRec D_neo_ark_pavilion_801879D4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_pavilion_801879DC[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_neo_ark_pavilion_801879E4[1] = {
    { 0, 0, 1, 0, D_neo_ark_pavilion_801879C8 },
};

GpRoomParamRec* D_neo_ark_pavilion_801879EC[8] = {
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879DC,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879E4,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879D4,
    D_neo_ark_pavilion_801879D4,
};

RoomFadeStorage D_neo_ark_pavilion_80187A0C = { 0 };

GpSaveLoc D_neo_ark_pavilion_80187A14 = { 0 };

s8 D_neo_ark_pavilion_80187A1C[4] = {
    0,
    38,
    -53,
    37,
};

RoomLatchedEvent D_neo_ark_pavilion_80187A20;

static __inline__ s32 NeoArkPavilion_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event);
static void           func_neo_ark_pavilion_8017EB80(Task* arg0);
static void           func_neo_ark_pavilion_8017EBEC(Task* task);

/// Draws the water-refraction ripple for the views that have one (views of
/// areas 27, 14, 15, 13, 30 and 29; every other view returns at once). The
/// view picks the row range, a split row and x where each row's strip is cut
/// in two, a clip mode, a wave scale and an ordering-table offset. For every
/// row the row vector is rotated through the transposed view matrix to get
/// its ordering-table depth, and one or two `POLY_FT4` strips are linked into
/// `gGpuCurrentOt`, each sampling the other display buffer displaced
/// vertically by a `rsin` / `rcos` wave that fades in over the first 8 rows of
/// the range and of the split. The phases come from `Task::killCountdown`,
/// seeded from `rand()` on the first call and advanced by 0x20 per call while
/// `Gp_StateF0.field_4` is clear.
///
/// Matching note: `wave = w` is written in both arms of the scale test, and
/// the pass-1 fade starts with `v = 0x79`. jump2 merges the two copies and the
/// constant set is deleted, but both change register allocation and
/// scheduling the way the retail code needs.
void func_neo_ark_pavilion_8017D660(Task* task)
{
    s32                   buf;
    s32                   sinArg;
    s32                   cosArg;
    s32                   kind;
    s32                   scale;
    s32                   zoff;
    s32                   split;
    s32                   splitX;
    s32                   otzOff;
    s32                   xLeft0;
    s32                   xRight0;
    s32                   xLeftS;
    s32                   start;
    s32                   end;
    s32                   area;
    POLY_FT4*             prim;
    OverlayRippleScratch* scratch;
    s32                   y;
    s32                   y0;
    s32                   xl;
    s32                   xr;
    s32                   passes;
    s32                   pass;
    s32                   wave;
    s32                   sinv;
    s32                   cosv;
    s32                   w;
    s32                   d;
    s32                   z;
    s32                   otz;
    s32                   v;
    s32                   dy;
    s32                   xv;
    s32                   x;
    s32                   xe;
    s32                   xMin;
    s32                   xMax;
    s32                   fadeLen;
    s32                   one;
    DisplayState*         disp;

    kind    = 0;
    scale   = 0x1000;
    zoff    = 0x21C;
    otzOff  = 0;
    xLeft0  = -0xA0;
    xRight0 = 0xA0;
    xLeftS  = -0xA0;
    split   = 0;
    splitX  = 0;
    buf     = gDisplayState.otBuffer;
    area    = gGameSession->at4.loc.area;
    if (area == 27) {
        otzOff = 10;
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 0x7F;
                end    = 0xF0;
                split  = 0x9F;
                splitX = 0x23;
                break;
            case 3:
                start  = 0x4A;
                end    = 0xF0;
                split  = 0x68;
                splitX = 0x55;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x74;
                splitX = -0xD3;
                break;
            case 5:
                start  = 0x66;
                end    = 0xF0;
                split  = 0x6B;
                splitX = 0xC2;
                break;
            case 6:
                start  = 0x93;
                end    = 0xF0;
                split  = 0xA1;
                splitX = 0xBC;
                break;
            default:
                return;
        }
    } else if (area == 14) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 0x77;
                end    = 0xF0;
                split  = 0xA1;
                splitX = -0x2C;
                break;
            case 3:
                start  = 0x4C;
                end    = 0xF0;
                split  = 0x68;
                splitX = -0x4E;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x3E8;
                kind   = 3;
                otzOff = 10;
                scale  = 0x800;
                break;
            default:
                return;
        }
    } else if (area == 15) {
        if (gGameSession->at4.loc.view == 2) {
            split  = 0x3E8;
            start  = 0x84;
            end    = 0xF0;
            splitX = 0x4B;
            scale  = 0x800;
        } else {
            return;
        }
    } else if (area == 13) {
        otzOff = 10;
        switch (gGameSession->at4.loc.view) {
            case 2:
            case 4:
                start = 0x52;
                end   = 0xF0;
                kind  = 1;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 3:
            case 5:
                start = 0x4C;
                end   = 0xF0;
                kind  = 2;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 6:
                start = 0x63;
                end   = 0xF0;
                split = 0;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            case 7:
                start = 0x35;
                end   = 0xF0;
                kind  = 4;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            default:
                return;
        }
    } else if (area == 30) {
        scale  = 0x800;
        otzOff = -10;
        zoff   = 0x131A;
        switch (gGameSession->at4.loc.view) {
            case 2:
                xLeft0 = 0x3B;
                start  = 0xA9;
                end    = 0xE0;
                split  = -0xC7;
                splitX = -0x43;
                break;
            case 3:
                otzOff  = 10;
                xLeft0  = -0x4F;
                xRight0 = 0x4F;
                xLeftS  = -0x3A;
                start   = 1;
                end     = 0x5E;
                split   = -0x49;
                splitX  = 0x78;
                break;
            case 4:
                xRight0 = -0x3B;
                start   = 0xA9;
                end     = 0xE0;
                split   = -0xC7;
                splitX  = 0x43;
                break;
            case 5:
                start  = 0xA1;
                end    = 0xF0;
                xLeftS = -0x8A;
                splitX = 0x114;
                split  = 0xAC;
                otzOff = 0;
                break;
            default:
                return;
        }
    } else if (area == 29) {
        zoff = 0x8C;
        switch (gGameSession->at4.loc.view) {
            case 6:
                start = 0xA5;
                end   = 0xF0;
                split = 0;
                break;
            case 7:
                start = 0xA4;
                end   = 0xF0;
                split = 0;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    xMin    = -0xA0;
    xMax    = 0xA0;
    fadeLen = 8;
    one     = 1;
    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }
    prim = (POLY_FT4*)Fs_ActorLoadBase2;
    disp = &gDisplayState;
    if (disp->otBuffer != 0) {
        prim += 488;
    }
    prim--;
    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown += 0x20;
    }
    sinArg = task->killCountdown * 2;
    cosArg = task->killCountdown;
    SCRATCH_PUSH(OverlayRippleScratch);
    scratch = SCRATCH_HEAD(OverlayRippleScratch);
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->mtx);
    scratch->origin.vx = gGfxViewCoord.workm.t[0];
    scratch->origin.vy = gGfxViewCoord.workm.t[1];
    scratch->origin.vz = gGfxViewCoord.workm.t[2];
    gfxRotateSv(&scratch->mtx, &scratch->origin);
    scratch->depth  = scratch->origin.vy + zoff;
    scratch->depth *= disp->screenDistance;
    scratch->row.vx = 0;
    scratch->row.vz = disp->screenDistance;
    gte_SetRotMatrix(&scratch->mtx);

    for (y = start; y < end; y++) {
        y0              = y - 0x78;
        scratch->row.vy = y0;
        gte_ldv0(&scratch->row);
        gte_rtv0();
        xl     = xLeft0;
        xr     = xRight0;
        passes = 1;
        if (split > 0) {
            if (y < split + 8) {
                xr = xMax;
                if (splitX > 0) {
                    xl = xLeftS;
                    xr = xl + splitX;
                } else {
                    xl = xr + splitX;
                }
                if (split < y) {
                    passes = 2;
                }
            }
        } else if (split < 0 && -split < y) {
            xr = xMax;
            if (splitX > 0) {
                xl = xLeftS;
                xr = xl + splitX;
            } else {
                xl = xr + splitX;
            }
        }
        sinv  = rsin(sinArg);
        cosv  = rcos(cosArg + 0x134);
        sinv += 0x2000;
        w     = cosv + sinv;
        w   >>= 9;
        if (scale != 0x1000) {
            w    = (w * scale) >> 12;
            wave = w;
        } else {
            wave = w;
        }
        w++;
        if (start != 1) {
            d = y - start;
            if (d < fadeLen) {
                w  = wave >> ((fadeLen - d) >> one);
                w += one;
            }
        }
        gte_stsv(&scratch->rowView);
        if (scratch->rowView.vy > 0) {
            otz   = scratch->depth / scratch->rowView.vy;
            otz >>= 2;
        } else {
            otz = 0x3FFF;
        }
        v    = (y0 + 0x78) + w;
        z    = otz;
        otz  = ((z << gDisplayState.otDepthShift) & 0x3FFF) >> 4;
        otz += otzOff;
        if (v >= 0xEF) {
            v = 0x1DC - v;
        }
        if (kind == 1) {
            if (y < 0x7D) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB3) {
                passes = 2;
                xl     = -0xA0;
                xr     = -0x59;
            } else {
                xl = -0xA0;
                xr = -0x59;
            }
        } else if (kind == 2) {
            if (y < 0x83) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB7) {
                passes = 2;
                xl     = 0x57;
                xr     = 0xA0;
            } else {
                xl = 0x57;
                xr = 0xA0;
            }
        } else if (kind == 3) {
            if (y < 0x43) {
                passes = 1;
                xl     = -0xA0;
                xr     = 0xA0;
            } else {
                passes = 2;
            }
        } else if (kind == 4) {
            passes = 1;
            xr     = 0xA0;
            xl     = -9;
            if (y >= 0x42) {
                xl = -0xA0;
                if (y < 0x4D) {
                    xl = -0x6A;
                }
            }
        }
        for (pass = 0; pass < passes; pass++) {
            dy = y - split;
            if (kind == 1) {
                if (pass != 0) {
                    xl = 0x3C;
                    xr = 0xA0;
                }
            } else if (kind == 2) {
                if (pass == 1) {
                    xl = -0xA0;
                    xr = -0x69;
                }
            } else if (kind == 3) {
                if (pass == 0) {
                    if (y < 0x43) {
                        xl = -0xA0;
                        xr = 0xA0;
                    } else {
                        xl = -0xA0;
                        xr = -0x57;
                    }
                } else {
                    if (y < 0xC1) {
                        xl = 0x5D;
                        xr = 0xA0;
                    } else {
                        xl = 0x2A;
                        xr = 0xA0;
                    }
                }
            } else if (pass == 1) {
                if (dy < fadeLen) {
                    w = wave >> ((fadeLen - dy) >> 1);
                    v = 0x79;
                    v = y0 + (v + w);
                    if (v >= 0xEF) {
                        v = 0x1DC - v;
                    }
                }
                if (splitX > 0) {
                    xv = splitX - 0x140;
                } else {
                    xv = splitX + 0x140;
                }
                xl = xMin;
                if (xv > 0) {
                    xr = xv + xl;
                } else {
                    xr = xMax;
                    xl = xv + xr;
                }
            }
            if (xr > 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                x           = xl;
                if (xl < 0) {
                    x = 0;
                }
                prim->x2 = x;
                prim->x0 = x;
                prim->u2 = x + 0x20;
                prim->u0 = x + 0x20;
                prim->x3 = xr;
                prim->x1 = xr;
                prim->u3 = xr + 0x20;
                prim->u1 = xr + 0x20;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xl <= 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                xe          = xr;
                if (xr > 0) {
                    xe = 0;
                }
                prim->u2 = xl - 0x60;
                prim->u0 = xl - 0x60;
                prim->u3 = (xl - 0x60) + (xe - xl);
                prim->u1 = (xl - 0x60) + (xe - xl);
                prim->x2 = xl;
                prim->x0 = xl;
                prim->x3 = xe;
                prim->x1 = xe;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
        }
        sinArg += 0x1F;
        if (z > 0x300) {
            cosArg += 0xC5 + (z - 0x300) / 4;
        } else {
            cosArg += 0xC5;
        }
    }
    SCRATCH_POP(OverlayRippleScratch);
}

/// Draws a rippling screen-distortion band for certain views of areas 12 and 30
/// (other views return at once): one semi-transparent textured quad per screen
/// row between the view's start and end rows, sampling the other display buffer with a vertical
/// offset from a `rsin` / `rcos` wave (faded over the last 16 rows) and linked
/// into `gGpuCurrentOt` at a depth that decreases row by row. The phases advance
/// from `Task::killCountdown`, which is seeded from `rand()` on the first call.
///
/// Matching note: `spare` is never assigned and `spare >> 16` is always zero.
/// It exists only in the register allocator's view, as the stack slot the
/// retail frame carries.
void func_neo_ark_pavilion_8017E2B4(Task* task)
{
    s32              xLeft  = -0xA0;
    s32              xRight = 0xA0;
    s32              buf    = gDisplayState.otBuffer;
    s32              passes = 1;
    GameLocationKey* loc    = &gGameSession->at4.loc;
    s32              area   = loc->area;
    s32              start;
    s32              end;
    POLY_FT4*        prim;
    u8*              base;
    s32              size;
    s32              sinArg;
    s32              cosArg;
    s32              otz;
    s32              pass;
    s32              y;
    s32              y0;
    s32              wave;
    s32              sinv;
    s32              cosv;
    s32              v;
    s32              x0;
    s32              x1;
    u16              spare;

    if (area == 12) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start = 1;
                end   = 0x3F;
                break;
            case 3:
                start = 1;
                end   = 0x49;
                break;
            case 4:
                start = 1;
                end   = 0x49;
                break;
            case 5:
                start = 1;
                end   = 0x3F;
                break;
            case 8:
                start = 1;
                end   = 0x72;
                break;
            case 9:
                start = 1;
                end   = 0x45;
                break;
            default:
                return;
        }
    } else if (area == 30) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 1;
                end    = 0x40;
                xRight = -0x50;
                break;
            case 4:
                start  = 0x3B;
                end    = 0x55;
                xRight = -0x67;
                break;
            case 5:
                start  = 0x22;
                end    = 0x4A;
                xRight = -0x4E;
                passes = 2;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }

    if (area == 12 && loc->variant == 3) {
        size  = 0x30000 - Fs_ChunkOutputSizes[0];
        size &= ~7;
        base  = (u8*)Fs_ActorLoadBase0 - (size - 0x30000);
        if (size < sizeof(POLY_FT4) * 976) {
            return;
        }
        if (gDisplayState.otBuffer != 0) {
            base += size >> 1;
        }
        prim = (POLY_FT4*)base - 1;
    } else {
        prim = (POLY_FT4*)((u8*)Fs_ActorLoadBase2 + 0x9880);
        if (gDisplayState.otBuffer != 0) {
            prim += 488;
        }
        prim--;
    }

    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown++;
    }
    sinArg = task->killCountdown << 5;
    cosArg = task->killCountdown << 4;
    SCRATCH_PUSH_BYTES(0x40);
    otz = ((0x3FFF << gDisplayState.otDepthShift) & 0x3FFF) >> 4;

    for (pass = 0; pass < passes; pass++) {
        if (pass == 1) {
            start  = 0x2E;
            end    = 0x54;
            xLeft  = 1;
            xRight = 0x55;
        }
        for (y = start; y < end; y++) {
            y0     = y - 0x78;
            sinv   = rsin(sinArg);
            cosv   = rcos(cosArg + 0x134);
            sinv  += 0x2000;
            wave   = cosv + sinv;
            wave >>= 10;
            if (end - 0x10 < y) {
                wave = (wave * (end - y)) >> 4;
            }
            cosv = wave + 0x79;
            v    = y0 + cosv;
            otz--;
            if (v >= 0xEF) {
                v = 0x1DC - v;
            }
            if (v < 0) {
                v = -v;
            }
            if (xRight > 0) {
                x0 = xLeft < 0 ? 0 : xLeft;
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                prim->x2    = x0;
                prim->x0    = x0;
                prim->u2    = x0 + 0x20;
                prim->u0    = x0 + 0x20;
                prim->x3    = xRight;
                prim->x1    = xRight;
                prim->u3    = xRight + 0x20;
                prim->u1    = xRight + 0x20;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xLeft < 0) {
                x1 = xRight;
                if (x1 > 0) {
                    x1 = 0;
                }
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                prim->u2    = xLeft - 0x60;
                prim->u0    = xLeft - 0x60;
                prim->u3    = x1 - 0x60;
                prim->u1    = x1 - 0x60;
                prim->x2    = xLeft;
                prim->x0    = xLeft;
                prim->x3    = x1;
                prim->x1    = x1;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            sinArg += 0x1F + (spare >> 16);
            cosArg += 0xC5;
        }
    }
    SCRATCH_POP_BYTES(0x40);
}

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_neo_ark_pavilion_8017E854(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_neo_ark_pavilion_80187A20.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_neo_ark_pavilion_80187A20.fade != 0) {
                    D_neo_ark_pavilion_80187A0C.fade.field_0 = 0;
                    D_neo_ark_pavilion_80187A0C.fade.field_1 = 0;
                    D_neo_ark_pavilion_80187A0C.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_neo_ark_pavilion_80187A0C.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_neo_ark_pavilion_80187A20.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_neo_ark_pavilion_80187A20.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_neo_ark_pavilion_80187A20.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_neo_ark_pavilion_80187A14.prefix.bytes.field_0;
            Mc_SaveData[0].state.at4.loc.warp = D_neo_ark_pavilion_80187A14.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_neo_ark_pavilion_80187A14.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_pavilion_8017E9EC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

static __inline__ s32 NeoArkPavilion_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event)
{
    D_neo_ark_pavilion_80187A1C_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_neo_ark_pavilion_80187A14 = *dst;
            D_neo_ark_pavilion_80187A20 = *event;
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
s32 func_neo_ark_pavilion_8017E9F4(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (*(u16*)in != 0xC) {
        return 1;
    }
    event.capCmd   = 4;
    event.stageSnd = 0;
    event.flagId   = 0x17E;
    event.fade     = 0;
    return NeoArkPavilion_StartEvent(out, &event);
}

/// Room message handler: on message `1`, spawns the pavilion's cap entity —
/// id `5` once flag `0x141` is set, `1` while it is clear.
s32 func_neo_ark_pavilion_8017EB3C(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x141) != 0 ? 5 : 1, 1);
    }
    return 0;
}

s32 func_neo_ark_pavilion_8017EB78(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room entry task tick: installs the room's message table (ids `0x13EE`-`0x13F1`),
/// hands the task to pointer slot 7, queues sound events `0x550D0005` and
/// `0x550D0006`, then advances state.
static void func_neo_ark_pavilion_8017EB80(Task* arg0)
{
    arg0->msgTable = D_neo_ark_pavilion_80183870;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550D0005, 0, 0);
    SndEvt_EnqueueType6(0x550D0006, 0, 0);
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

/// `Gp_State1C` effect task drawing a growing, fading quad through
/// `func_neo_ark_pavilion_8017ED98`. The first frame sets the brightness to
/// 0x40, takes the size from the spawn parameter's low 12 bits and turns the
/// coordinate to a random Y rotation. Every frame then grows the size by
/// 0x20, draws, and dims by 2, releasing the effect once the brightness falls
/// under 2. The coordinate is never rebuilt, so it keeps the frame the spawner
/// left. Once the room's event state leaves zero it only draws, and releases
/// at state 4.
void func_neo_ark_pavilion_8017EC4C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        func_neo_ark_pavilion_8017ED98(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
        }
        work->angle += 0x20;
        func_neo_ark_pavilion_8017ED98(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
static void func_neo_ark_pavilion_8017ED98(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        tbl   = &D_80111E38[i];
        v     = &block->vec[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&arg0->workm);
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
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// `Gp_State1C` effect task that plays an eight-frame sprite animation. The
/// first frame takes the angle from the spawn parameter's low 12 bits, the
/// frame step from bits 12-15 and, from the top nibble, which of the two
/// sprite drawers to use; a zero velocity is seeded from the spawn kind
/// (random scatter, the stored direction, or none) and scaled to the requested
/// speed. Each later frame draws the sprite, moves the coordinate under a
/// constant downward pull while the speed is non-zero, and advances the frame
/// every `step` ticks, releasing the effect after the eighth. Once the room's
/// event state leaves zero it only draws, and releases at state 4.
void func_neo_ark_pavilion_8017F0CC(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (task->state < 2) {
            func_neo_ark_pavilion_8017F588(coord, (u16)work->index, work->scale, work->angle);
        } else {
            func_neo_ark_pavilion_8017F974(coord, (u16)work->index, work->scale);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_neo_ark_pavilion_8017F588(coord, (u16)work->index, work->scale, work->angle);
            break;
        case 2:
            func_neo_ark_pavilion_8017F974(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent, unshaded
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) rotated about the projected centre.
/// `arg1` selects the 32-texel UV column `(arg1 & 0xFFFF) << 5` at v=0xE0..0xFF.
/// `arg2` is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 31 / otz`. `arg3` is the spin angle, applied at `arg3` and
/// `arg3 + 0x400` through `rsin`/`rcos`.
static void func_neo_ark_pavilion_8017F588(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent, unshaded
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2). `arg1` selects a 56-texel UV tile in
/// a 4-wide 2-row grid starting at v = 0x70: u = `(arg1 & 3) * 56`, v =
/// `0x70 + ((arg1 & 7) >> 2) * 56`. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 55 / otz`. The quad is axis-aligned and
/// 2*radius on a side, shifted up so the projected point sits at
/// three-quarters height (`y0 = sy - r - r/2`, `y2 = sy + r/2`).
static void func_neo_ark_pavilion_8017F974(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    u16            idx;
    u32            cell;
    s32            row;
    u8             u0;
    u8             u1;
    u8             v0;
    u8             v1;

    idx           = arg1;
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
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = idx;
        u0          = (cell & 3) * 0x38;
        row         = ((cell & 7) >> 2) * 0x38;
        v0          = row + 0x70;
        v1          = row + 0xA7;
        u1          = u0 + 0x37;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        block->step = ((s16)arg2 * 55) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

void func_neo_ark_pavilion_8017FC10(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DA;
        D_8011572C  = 0x601F6;
        D_80115750  = 0x60212;
        D_80115734  = 0x60227;
        D_80115730  = 0x60232;
        D_80115754  = 0x6023D;
        D_8011574C  = 0x60176;
        D_80115738  = 0x60177;
        arg0->state = 1;
    }
}

/// `Gp_State1C` effect task for a flash that builds up and bursts. Over
/// `spawnArg1` frames it ramps a brightness from zero to 0x100, drawing two
/// wedge discs and a ring in the colour `(b, b/4, b/2)`; on the last frame it
/// flashes the screen with that colour, then draws the two-ring billboard
/// while dimming by 0x10 a frame, and releases the effect once the brightness
/// is spent. Once the room's event state leaves zero it only waits for state 4
/// to release.
void func_neo_ark_pavilion_8017FCB0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_neo_ark_pavilion_80180380(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_pavilion_80180380(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_pavilion_8017FF54(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_neo_ark_pavilion_80181284(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring of sixteen gouraud quads around the coordinate's projected
/// position, when it projects. The ring runs from radius
/// `(s16)arg1 * 64 / (otz + 1)`, which is black, to
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`, which takes the colour `rgb`.
static void func_neo_ark_pavilion_8017FF54(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
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
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `arg1 * 64 / (otz + 1)`. Only the centre vertex takes the colour
/// `rgb`, so each wedge fades to black at the rim.
static void func_neo_ark_pavilion_80180380(GfxCoord* arg0, s16 arg1, u8* rgb)
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

/// `Gp_State1C` effect task drawing a ribbon between the trails of two points
/// on the spawner's parent coordinate. The first frame allocates sixteen
/// coordinates, places the effect at the first offset, and fills both
/// eight-slot trails with the two points' current view positions. Each later
/// frame records the two points into the next slot, rebuilds all sixteen,
/// draws the ribbon through `func_neo_ark_pavilion_80180C04` and releases the
/// effect once its age reaches `spawnArg1`. It stops advancing once the room's
/// event state reaches 2.
void func_neo_ark_pavilion_80180714(Task* task)
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
                objCoord->coord.t[0]   = D_neo_ark_pavilion_80183898[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_pavilion_80183898[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_pavilion_80183898[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_pavilion_80183898[1];
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
                    SVECTOR* edge    = &D_neo_ark_pavilion_80183898[1];
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
                func_neo_ark_pavilion_80180C04(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a ribbon between two eight-slot coordinate trails as seven gouraud
/// `POLY_G4` quads, walking backwards from slot `arg2`. Each quad spans
/// `workm.t` of two adjacent slots on `arg0` and `arg1`; its newer edge is
/// weighted `0x40 - 9 * i` and its older edge nine less, so the ribbon fades
/// along its length. `arg3` holds the colour as per-channel multipliers of that
/// weight: red from bits 8 up, green from bits 4-5, blue from bits 0-1. A quad
/// the GTE flags as bad is skipped.
static void func_neo_ark_pavilion_80180C04(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
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

/// `Gp_State1C` effect task for a burst. The first frame spawns effect
/// `0x60076` at the coordinate and then, depending on `spawnArg1`, either
/// effect `0x60070` followed by seven frames of randomly aimed `0x60070`
/// sparks, or two `0x6007C` effects followed by seven frames of two expanding
/// rings in a fading `(a, a/2, a/4)` colour. The effect is then released.
/// Once the room's event state leaves zero it only waits for state 4 to
/// release.
void func_neo_ark_pavilion_80180FFC(Task* task)
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
            func_neo_ark_pavilion_8017FF54(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_pavilion_8017FF54(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, draws a star-shaped glow from gouraud
/// `POLY_G4` wedges that are coloured only at the centre and fade to black.
/// `arg1` is a signed half-extent giving an outer radius
/// `arg1 * 64 / (otz + 1)` and an inner one `arg1 * 8 / (otz + 1)`. Eight
/// wedges span the outer radius in half the colour `arg2`, eight more span
/// half of it at full colour, and four long spikes reach twice the outer
/// radius between points on the inner one.
static void func_neo_ark_pavilion_80181284(GfxCoord* arg0, s16 arg1, u8* arg2)
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

/// Glowing disc anchored to its parent at the work block's position. State 1
/// grows the disc and, every fourth tick, spawns the effect `D_80115730` names
/// at a random joint of the player's model and adopts it as a child task;
/// state 2 keeps growing it and adds a half-bright second disc on odd ticks;
/// state 3 drifts the disc away while it fades inside an expanding ring, then
/// releases the work block, as does state 4. The spawn argument picks the
/// disc's colour. Nothing runs while the room's event state is set, and the
/// block is released once that state reaches 4.
void func_neo_ark_pavilion_80181C44(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->parent                    = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->composeStamp              = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].b;
            func_neo_ark_pavilion_80182A68(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].b;
            func_neo_ark_pavilion_80182A68(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_neo_ark_pavilion_80182A68(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_neo_ark_pavilion_801838A8[arg0->spawnArg1.value].b;
            func_neo_ark_pavilion_80182A68(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_neo_ark_pavilion_80182644(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// `Gp_State1C` effect task that carries its coordinate to the coordinate
/// passed in `spawnArg1`. The first frame turns the offset between the two
/// into a per-frame step of 0xCC/0x1000 of it, so twenty frames cover the
/// distance; each later frame moves by that step, draws the next sprite frame
/// every other tick, and releases the effect after twenty ticks. Once the
/// room's event state leaves zero it only waits for state 4 to release.
void func_neo_ark_pavilion_8018219C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0]  += work->pos.vx;
                coord->coord.t[1]  += work->pos.vy;
                coord->coord.t[2]  += work->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_neo_ark_pavilion_801823C0(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_neo_ark_pavilion_801823C0(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = arg0->workm.t[1];
    vz                                      = arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg        = (s16)arg2;
        t           = sarg * 24;
        block->step = (t - sarg) / block->otz;
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
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4`
/// segments forming a ring. The edge at `arg1` is black and the edge at
/// `arg1 + arg2` takes the colour `rgb`, both signed half-extents scaled to
/// the screen as `r * 64 / (otz + 1)`.
static void func_neo_ark_pavilion_80182644(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
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
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `arg1 * 64 / (otz + 1)`. Only the centre vertex takes the colour
/// `rgb`, so each wedge fades to black at the rim.
static void func_neo_ark_pavilion_80182A68(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

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
        block->step = (arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// `Gp_State1C` effect task for a flickering glow. Each frame it grows a size
/// by 0x10 and draws a wedge disc of twice that size and the glow of
/// `func_neo_ark_pavilion_80182FA8`, in a colour `(a, a/2, a/4)` of its main
/// level. A widening ring in the colour of a second level is drawn over them
/// while that level lasts; after it runs out the main level drops by 0x18 a
/// frame and the effect is released once it is spent. Once the room's event
/// state leaves zero it only waits for state 4 to release.
void func_neo_ark_pavilion_80182DFC(Task* arg0)
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
        func_neo_ark_pavilion_80182A68(coord, step * 2, rgb);
        func_neo_ark_pavilion_80182FA8(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_neo_ark_pavilion_80182644(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
static void func_neo_ark_pavilion_80182FA8(GfxCoord* coord, s16 size)
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
            func_neo_ark_pavilion_801834D4(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_neo_ark_pavilion_801834D4(GfxCoord* arg0, s32 arg1)
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
