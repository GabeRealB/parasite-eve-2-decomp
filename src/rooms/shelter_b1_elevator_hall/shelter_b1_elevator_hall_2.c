#include "rooms/shelter_b1_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_elevator_hall_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/shelter_elevator.h"

#define D_shelter_b1_elevator_hall_80182D04 (D_shelter_b1_elevator_hall_80182CF4 + 2)
#define D_shelter_b1_elevator_hall_80182D14 (D_shelter_b1_elevator_hall_80182CF4 + 4)
#define D_shelter_b1_elevator_hall_80182D24 (D_shelter_b1_elevator_hall_80182CF4 + 6)
#define D_shelter_b1_elevator_hall_80182D34 (D_shelter_b1_elevator_hall_80182CF4 + 8)
#define D_shelter_b1_elevator_hall_80182D84 (D_shelter_b1_elevator_hall_80182CF4 + 18)

/// Per-index shifts applied to the flash brightness, one per colour channel.

/// Offsets of the two trail anchors from the effect's parent; the second entry
/// is also reached through its own name.

// Indexed views below share one contiguous table.
extern WorldCollisionGrid     D_shelter_b1_elevator_hall_80183414[1];
extern WorldCollisionOccluder D_shelter_b1_elevator_hall_80184748[1];
extern WorldCollisionTrigger  D_shelter_b1_elevator_hall_80184288[10];
extern WorldCollisionTrigger  D_shelter_b1_elevator_hall_80184580[6];
extern WorldCoordRoomLights   D_shelter_b1_elevator_hall_80184270[1];

TaskDesc D_shelter_b1_elevator_hall_80182CAC = { { { TASK_BODY_NONE, 32 } }, shelterElevatorTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_elevator_hall_80182CB8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_elevator_hall_8017D810 },
    { 5105, func_shelter_b1_elevator_hall_8017DB54 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_elevator_hall_8017DB64 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_elevator_hall_8017DB5C },
    { ROOM_MESSAGE_SOUND, func_shelter_b1_elevator_hall_8017DB6C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_elevator_hall_80182CE8 = { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_elevator_hall_8017D99C, { .value = 0 } };

SVECTOR D_shelter_b1_elevator_hall_80182CF4[28] = {
    { -8833, -620, -1755, 0 },
    { -8166, -620, -1755, 0 },
    { -4828, -620, -1755, 0 },
    { -4165, -620, -1755, 0 },
    { -828, -620, -1755, 0 },
    { -165, -620, -1755, 0 },
    { 2165, -620, -1755, 0 },
    { 2833, -620, -1755, 0 },
    { 6165, -620, -1755, 0 },
    { 6833, -620, -1755, 0 },
    { 9165, -620, -1755, 0 },
    { 9833, -620, -1755, 0 },
    { -828, -620, 1759, 0 },
    { -165, -620, 1759, 0 },
    { 2165, -620, 1759, 0 },
    { 2833, -620, 1759, 0 },
    { 4726, -620, 2658, 0 },
    { 4726, -620, 3269, 0 },
    { 10749, -620, 2564, 0 },
    { 10749, -620, 3176, 0 },
    { -4125, -2399, 1652, 0 },
    { -3753, -2399, 1652, 0 },
    { 10537, -2376, -252, 0 },
    { 10537, -2376, -734, 0 },
    { 10575, -2526, 3841, 0 },
    { 10575, -2526, 4301, 0 },
    { -10963, -998, 1157, 0 },
    { -10695, -998, 1157, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x90EE }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting D_shelter_b1_elevator_hall_80182DF8[1] = {
    { D_shelter_b1_elevator_hall_80184270, NULL },
};

WorldCollisionRoomResources D_shelter_b1_elevator_hall_80182E00[1] = {
    { D_shelter_b1_elevator_hall_80183414, D_shelter_b1_elevator_hall_80184288, D_shelter_b1_elevator_hall_80184580, D_shelter_b1_elevator_hall_80184748 },
};

u8* D_shelter_b1_elevator_hall_80182E10[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_elevator_hall_80182E14[1] = { 9 };

DirectionWarpEntry D_shelter_b1_elevator_hall_80182E18[4] = {
    { { { .word = 1024 }, -0x29CC, 0, 520 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -0x29CC, 0, 520 }, { 0, 0, 0, 0 }, 0x54090006, 0x54090007, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, -3864, 0, 1073 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -3864, 0, 1073 }, { 0, 0, 0, 0 }, 0x54090002, 0x54090001, 0x54090003, 3, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1CD },
    { { { .word = 3072 }, 9950, 0, -290 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9950, 0, -290 }, { 0, 0, 0, 0 }, 0x54090009, 0x54090008, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1BB },
    { { { .word = 3072 }, 9950, 0, 3740 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9950, 0, 3740 }, { 0, 0, 0, 0 }, 0x54090005, 0x54090004, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB1ElevatorHallCollision05E54Normals[8] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_normals.inc"
};

static SVECTOR _gShelterB1ElevatorHallCollision05E54Verts[58] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_verts.inc"
};

static WorldCollisionGridFace _gShelterB1ElevatorHallCollision05E54Faces[32] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_faces.inc"
};

static s16 _gShelterB1ElevatorHallCollision05E54Cells[170] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1ElevatorHallCollision05E54Cells[i])
static s16* _gShelterB1ElevatorHallCollision05E54Table[14] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_elevator_hall_80183414[1] = {
    { NULL, _gShelterB1ElevatorHallCollision05E54Normals, _gShelterB1ElevatorHallCollision05E54Verts, _gShelterB1ElevatorHallCollision05E54Faces, _gShelterB1ElevatorHallCollision05E54Table, 0x33A6, 1641, 7, 2, 4000, 32 },
};

ViewCamera D_shelter_b1_elevator_hall_80183438[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 20, 0x7530, -1740 } }, 329 },
    { { { { 1125, 0, 3938 }, { 1740, 3674, -497 }, { -3532, 1810, 1009 } }, { 5854, 2660, 842 } }, 230 },
    { { { { 1121, 0, 3939 }, { 444, 4069, -126 }, { -3914, 462, 1114 } }, { 561, 1452, 741 } }, 230 },
    { { { { 1056, 0, 3957 }, { 339, 4080, -90 }, { -3942, 350, 1052 } }, { -2895, 1369, 969 } }, 230 },
    { { { { 897, 0, -3996 }, { -262, 4087, -58 }, { 3987, 269, 895 } }, { 2381, 1413, 650 } }, 230 },
    { { { { -1383, 0, -3855 }, { -354, 4078, 127 }, { 3839, 376, -1377 } }, { -8739, 1597, -288 } }, 257 },
    { { { { 3754, 0, -1637 }, { -52, 4093, -121 }, { 1636, 132, 3752 } }, { -5306, 1248, 1319 } }, 230 },
    { { { { -3762, 0, -1618 }, { -138, 4080, 322 }, { 1613, 351, -3748 } }, { -5568, 1552, -4937 } }, 207 },
    { { { { 4070, 0, 459 }, { 337, 2784, -2984 }, { -312, 3003, 2767 } }, { 0x29F8, 1700, -374 } }, 257 },
};

SpriteBatch D_shelter_b1_elevator_hall_8018357C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_elevator_hall_8018358C[47] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 8, 1138, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -64, 1018, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -96, -120, 3430, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -128, -120, 984, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, -64, 1000, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -32, 3744, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 8, 3708, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -120, 0, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -80, 0, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -24, 1106, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -40, 0, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -8, 1123, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 16, 1170, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, -32, 1033, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, -16, 1126, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -16, 1113, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 8, 1164, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -32, 1042, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, -16, 1170, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -80, 952, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -64, 1048, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 968, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 32, 0x30D4, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 32, 0x30D4, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 32, 0, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 24, 1145, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 16, 1121, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 8, 1102, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 8, 1101, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -16, 0, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, -16, 1123, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -8, 1121, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -8, 1121, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, -8, 1119, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 0, 1088, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 1121, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 16, 1123, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -128, -72, 1053, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -128, 24, 1141, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -112, -40, 1064, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -128, -40, 1031, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -112, -8, 1136, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -128, -8, 1167, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -104, -40, 1055, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -104, -8, 1139, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -160, -40, 1128, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, -160, -80, 1112, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_elevator_hall_80183938[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 2, 0 } },
    { 22, 15, 0, 0, { 1, 0 } },
    { 37, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_elevator_hall_80183960[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_elevator_hall_80183970[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_elevator_hall_80183980[9] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, -48, 1375, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 0, 1375, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, 0, 1650, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -48, 1625, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 0, 1730, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -48, 1674, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -48, 1863, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 0, 1887, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 16, 1892, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_elevator_hall_80183A34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b1_elevator_hall_80183A4C[2] = {
    { { 150, 64, 84, 92 }, 1870 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b1_elevator_hall_80183A60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_elevator_hall_80183A70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_elevator_hall_80183A80[27] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 8, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -104, 0, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -96, 0, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -120, 650, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, -104, 669, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -96, 647, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, -80, 649, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, -72, 655, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -64, 460, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -8, 494, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 40, 624, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 80, 517, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 72, 608, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 32, 676, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, -16, 499, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, -64, 492, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, -64, 645, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 56, 679, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 56, 700, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, 16, 768, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -24, 681, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -64, 673, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -64, 0, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -32, 716, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 72, 712, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 16, 696, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, -24, 516, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_elevator_hall_80183C9C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_elevator_hall_80183CB4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_elevator_hall_80183CC4[9] = {
    { { .empty = D_shelter_b1_elevator_hall_8018357C }, D_shelter_b1_elevator_hall_8018357C, NULL },
    { { .elements = D_shelter_b1_elevator_hall_8018358C }, D_shelter_b1_elevator_hall_80183938, NULL },
    { { .empty = D_shelter_b1_elevator_hall_80183960 }, D_shelter_b1_elevator_hall_80183960, NULL },
    { { .empty = D_shelter_b1_elevator_hall_80183970 }, D_shelter_b1_elevator_hall_80183970, NULL },
    { { .elements = D_shelter_b1_elevator_hall_80183980 }, D_shelter_b1_elevator_hall_80183A34, D_shelter_b1_elevator_hall_80183A4C },
    { { .empty = D_shelter_b1_elevator_hall_80183A60 }, D_shelter_b1_elevator_hall_80183A60, NULL },
    { { .empty = D_shelter_b1_elevator_hall_80183A70 }, D_shelter_b1_elevator_hall_80183A70, NULL },
    { { .elements = D_shelter_b1_elevator_hall_80183A80 }, D_shelter_b1_elevator_hall_80183C9C, NULL },
    { { .empty = D_shelter_b1_elevator_hall_80183CB4 }, D_shelter_b1_elevator_hall_80183CB4, NULL },
};

WorldCoordPointLight D_shelter_b1_elevator_hall_80183D30[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -494, -601, -1312 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1443, -601, -1312 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6522, -601, -211 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9439, -601, -491 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9174, -601, 1918 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5099, -601, 2541 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1370, -601, 1216 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -103, -601, 1216 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2827, -2031, 94 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4197, -5422, 94 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 7641 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2390, -5242, 94 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3532, 3583, 3528 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8692, -5422, 1815 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 7341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8238, -601, -1312 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4076, -601, -1312 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
};

WorldCoordRoomLights D_shelter_b1_elevator_hall_80184270[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_b1_elevator_hall_80183D30), D_shelter_b1_elevator_hall_80183D30, 0, NULL },
};

WorldCollisionTrigger D_shelter_b1_elevator_hall_80184288[10] = {
    { NULL, NULL, NULL, { -8070, -1296, 92, 0 }, { { -364, -2112, -2398, 0 }, { 345, -2112, 2382, 0 }, { -364, 2112, -2398, 0 }, { 345, 2112, 2382, 0 } }, { 4066, 0, -604, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7952, -1360, 16, 0 }, { { 337, -2112, 2391, 0 }, { -340, -2112, -2393, 0 }, { 337, 2112, 2391, 0 }, { -340, 2112, -2393, 0 } }, { -4071, 0, 575, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2752, -1441, 0, 0 }, { { 332, -2112, 2386, 0 }, { -345, -2112, -2396, 0 }, { 332, 2112, 2386, 0 }, { -345, 2112, -2396, 0 } }, { -4069, 0, 575, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3040, -1441, 0, 0 }, { { -356, -2112, -2392, 0 }, { 353, -2112, 2389, 0 }, { -356, 2112, -2392, 0 }, { 353, 2112, 2389, 0 } }, { 4067, 0, -604, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7824, -1313, 1792, 0 }, { { 3280, -2112, 194, 0 }, { -3309, -2112, -233, 0 }, { 3280, 2112, 194, 0 }, { -3309, 2112, -233, 0 } }, { -266, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 3916, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7944, -1248, 1894, 0 }, { { -3173, -2112, -227, 0 }, { 3172, -2112, 227, 0 }, { -3173, 2112, -227, 0 }, { 3172, 2112, 227, 0 } }, { 292, 0, -4087, 0 }, { 0, 0, 4096, 0 }, 3814, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 32, -1248, 0, 0 }, { { 0, -2112, -2416, 0 }, { 0, -2112, 2416, 0 }, { 0, 2112, -2416, 0 }, { 0, 2112, 2416, 0 } }, { 4110, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 256, -1248, 0, 0 }, { { -16, -2112, 2416, 0 }, { 16, -2112, -2416, 0 }, { -16, 2112, 2416, 0 }, { 16, 2112, -2416, 0 } }, { -4111, 0, -28, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5009, -1408, 0, 0 }, { { -13, -2112, -2417, 0 }, { 12, -2112, 2416, 0 }, { -13, 2112, -2417, 0 }, { 12, 2112, 2416, 0 } }, { 4111, 0, -22, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 8, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5184, -1248, 0, 0 }, { { -16, -2112, 2416, 0 }, { 16, -2112, -2416, 0 }, { -16, 2112, 2416, 0 }, { 16, 2112, -2416, 0 } }, { -4111, 0, -28, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 5, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_elevator_hall_80184580[6] = {
    { NULL, NULL, NULL, { -0x2A80, -48, 320, 0 }, { { -448, 0, -544, 0 }, { 448, 0, -544, 0 }, { -448, 0, 544, 0 }, { 448, 0, 544, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 704, WORLD_COLLISION_TRIGGER_ACTION_WARP, 8, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4064, -48, 1072, 0 }, { { -896, 0, -400, 0 }, { 896, 0, -400, 0 }, { -896, 0, 400, 0 }, { 896, 0, 400, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 981, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2790, -48, -288, 0 }, { { -480, 0, -560, 0 }, { 480, 0, -560, 0 }, { -480, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 735, WORLD_COLLISION_TRIGGER_ACTION_WARP, 26, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2780, -48, 4064, 0 }, { { -480, 0, -560, 0 }, { 480, 0, -560, 0 }, { -480, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 735, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6224, -64, 4128, 0 }, { { -1136, 0, -544, 0 }, { 1136, 0, -544, 0 }, { -1136, 0, 544, 0 }, { 1136, 0, 544, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, -4096, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7488, -64, 4256, 0 }, { { -592, 0, -544, 0 }, { 592, 0, -544, 0 }, { -592, 0, 544, 0 }, { 592, 0, 544, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b1_elevator_hall_80184748[1] = {
    { NULL, NULL, { 2768, -1360, 3664, 0 }, { { -1968, -2384, 1936, 0 }, { 1968, -2384, -1936, 0 }, { -1968, 2384, 1936, 0 }, { 1968, 2384, -1936, 0 } }, { -2879, 0, -2926, 0 }, 3647, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_shelter_b1_elevator_hall_80184784[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_elevator_hall_8018479C[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_elevator_hall_801847B4[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_elevator_hall_801847CC[3] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205600_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_elevator_hall_801847F0[4] = {
    { 6, 0, 0, -700, 0, 0, 1024, 0, 0, 2, 0 },
    { 6, 0, 0, 6050, 0, 3250, 1700, 0, 0, 2, 0 },
    { 6, 0, 0, -2150, 0, -500, 3700, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_elevator_hall_80184830[3] = {
    { 3, 0, 0, -5500, 0, 0, 3072, 0, 0, 2, 0 },
    { 3, 0, 1, -2500, 0, 0, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_elevator_hall_80184860[11] = {
    { 21, 0, 0, 0, -2000, 1500, 2048, 0, 0, 2, 0 },
    { 21, 0, 0, 3000, -2000, 1500, 2048, 0, 0, 2, 2 },
    { 21, 0, 0, 0, -2000, -1500, 0, 0, 0, 2, 0 },
    { 21, 0, 0, 3000, -2000, -1500, 0, 0, 0, 2, 2 },
    { 21, 0, 0, 8000, -2000, -1500, 0, 0, 0, 2, 0 },
    { 21, 0, 0, 7000, -2000, -1500, 0, 0, 0, 2, 0 },
    { 21, 0, 0, 6000, -2000, -1500, 0, 0, 0, 2, 0 },
    { 21, 0, 0, 8000, -2000, 4900, 2048, 0, 0, 2, 0 },
    { 21, 0, 0, 7000, -2000, 4900, 2048, 0, 0, 2, 0 },
    { 21, 0, 0, 6000, -2000, 4900, 2048, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_elevator_hall_80184910[3] = {
    { 23, 2, 1, 9000, 0, 1500, 0, 0, 0, 2, 0 },
    { 56, 7, 1, 5000, 0, 0, 3072, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_elevator_hall_80184940[12] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_elevator_hall_801847F0, D_shelter_b1_elevator_hall_80184784 },
    { D_shelter_b1_elevator_hall_80184830, D_shelter_b1_elevator_hall_8018479C },
    { D_shelter_b1_elevator_hall_80184860, D_shelter_b1_elevator_hall_801847B4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_elevator_hall_80184910, D_shelter_b1_elevator_hall_801847CC },
};

WorldCollisionFootstepSounds D_shelter_b1_elevator_hall_801849A0 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b1_elevator_hall_801849AC = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b1_elevator_hall_801849B8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_elevator_hall_801849C0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_elevator_hall_801849A0 },
};

WorldCollisionSurfaceProperties D_shelter_b1_elevator_hall_801849C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_elevator_hall_801849AC },
};

WorldCollisionSurfaceProperties* D_shelter_b1_elevator_hall_801849D0[8] = {
    D_shelter_b1_elevator_hall_801849B8,
    D_shelter_b1_elevator_hall_801849C0,
    D_shelter_b1_elevator_hall_801849C8,
    D_shelter_b1_elevator_hall_801849B8,
    D_shelter_b1_elevator_hall_801849B8,
    D_shelter_b1_elevator_hall_801849B8,
    D_shelter_b1_elevator_hall_801849B8,
    D_shelter_b1_elevator_hall_801849B8,
};

RoomFadeStorage D_shelter_b1_elevator_hall_801849F0;

/// On the task's first tick stores seven room-specific values into resident
/// gameplay globals, then draws the `_glowDrawCapsule`
/// placements the current camera view shows. Views 2 and 9 share their last
/// placement, `D_shelter_b1_elevator_hall_80182CF4[26]`.
void func_shelter_b1_elevator_hall_8017DC80(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B1_ELEVATOR_HALL_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B1_ELEVATOR_HALL_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_ELEVATOR_HALL_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_ELEVATOR_HALL_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B1_ELEVATOR_HALL_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_ELEVATOR_HALL_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_ELEVATOR_HALL_SPARK_BURST;
        arg0->state               = 1;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            _glowDrawCapsule(&D_shelter_b1_elevator_hall_80182CF4[0], 0x180, 0x444);
            _glowDrawCapsule(&D_shelter_b1_elevator_hall_80182CF4[26], 0x180, 0x44);
            break;
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D04;
            _glowDrawCapsule(&p[0], 0x180, 0x444);
            _glowDrawCapsule(&p[18], 0x200, 0x421);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D14;
            _glowDrawCapsule(&p[0], 0x180, 0x444);
            _glowDrawCapsule(&p[8], 0x180, 0x444);
            _glowDrawCapsule(&p[16], 0x200, 0x421);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D24;
            _glowDrawCapsule(&p[0], 0x180, 0x444);
            _glowDrawCapsule(&p[6], 0x180, 0x444);
            _glowDrawCapsule(&p[8], 0x180, 0x444);
            _glowDrawCapsule(&p[12], 0x180, 0x444);
            _glowDrawCapsule(&p[16], 0x200, 0x421);
            break;
        }
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D84;
            _glowDrawCapsule(&p[0], 0x180, 0x444);
            _glowDrawCapsule(&p[6], 0x200, 0x421);
            break;
        }
        case 8: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D34;
            _glowDrawCapsule(&p[0], 0x180, 0x444);
            _glowDrawCapsule(&p[2], 0x180, 0x444);
            _glowDrawCapsule(&p[14], 0x200, 0x421);
            break;
        }
        case 9:
            _glowDrawCapsule(&D_shelter_b1_elevator_hall_80182CF4[26], 0x180, 0x44);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b1_elevator_hall_8017E6F4(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b1_elevator_hall_8017F43C(Task* arg0)
{
    _roomVisualEffectsHaloTask(arg0);
}

void func_shelter_b1_elevator_hall_8017F7D4(Task* arg0)
{
    _roomVisualEffectsHaloOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b1_elevator_hall_80180BE4(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_elevator_hall_80180D18(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_elevator_hall_8018177C(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_elevator_hall_80182064(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
