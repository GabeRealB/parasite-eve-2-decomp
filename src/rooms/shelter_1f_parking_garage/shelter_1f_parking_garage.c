#include "rooms/shelter_1f_parking_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
// This room's fade symbol is the ScreenFade itself, with no following word.
#define ROOM_EVENT_FADE gRoomEventFade
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_variants.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_1f_parking_garage_80181984[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_1f_parking_garage_80181984_value __asm__("D_shelter_1f_parking_garage_80181984");

extern TaskDesc         D_shelter_1f_parking_garage_80180BA0;
extern TaskDesc         D_shelter_1f_parking_garage_80180BAC;
extern TaskMessageEntry D_shelter_1f_parking_garage_80180BB8[];
extern TaskDesc         D_shelter_1f_parking_garage_80180BE0;
extern SVECTOR          D_shelter_1f_parking_garage_80180BFC[];
extern SVECTOR          D_shelter_1f_parking_garage_80180C4C[];

/// Offsets from the parent coordinate of the two trail heads the smoke-trail
/// task follows. The second is also reached under its own name.

extern ScreenFade       gRoomEventFade;
extern ScreenFade       D_shelter_1f_parking_garage_80181978;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomDeparture    gRoomDeparture;
extern RoomLatchedEvent gRoomEventLatched;

static void func_shelter_1f_parking_garage_8017DE9C(Task* task);
static void func_shelter_1f_parking_garage_8017DF04(Task* task);

extern WorldCollisionGrid         D_shelter_1f_parking_garage_80180FE8[1];
extern WorldCollisionTrigger      D_shelter_1f_parking_garage_801815F8[4];
extern WorldCollisionTrigger      D_shelter_1f_parking_garage_80181728[5];
extern WorldCoordRoomAmbientEntry D_shelter_1f_parking_garage_801818A4[5];
extern WorldCoordRoomLights       D_shelter_1f_parking_garage_801815E0[1];

s32  func_shelter_1f_parking_garage_8017DCEC(Task*, s32, s32, s32);
s32  func_shelter_1f_parking_garage_8017DCF4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_1f_parking_garage_8017DE44(Task*, s32, s32, s32);
s32  func_shelter_1f_parking_garage_8017DE4C(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void func_shelter_1f_parking_garage_8017DAF0(Task*);

TaskDesc D_shelter_1f_parking_garage_80180BA0 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

TaskDesc D_shelter_1f_parking_garage_80180BAC = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_1f_parking_garage_80180BB8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_parking_garage_8017DCF4 },
    { 5105, func_shelter_1f_parking_garage_8017DCEC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_parking_garage_8017DE4C },
    { ROOM_MESSAGE_COMMAND, func_shelter_1f_parking_garage_8017DE44 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_1f_parking_garage_80180BE0 = { { { TASK_BODY_NONE, 32 } }, func_shelter_1f_parking_garage_8017DAF0, { .value = 0 } };

SVECTOR D_shelter_1f_parking_garage_80180BEC[2] = {
    { 400, -3090, 1790, 0 },
    { 1600, -3090, 1790, 0 },
};

SVECTOR D_shelter_1f_parking_garage_80180BFC[10] = {
    { 4400, -3090, 1790, 0 },
    { 5600, -3090, 1790, 0 },
    { 8400, -3090, 1790, 0 },
    { 9600, -3090, 1790, 0 },
    { 400, -3090, -1790, 0 },
    { 1600, -3090, -1790, 0 },
    { 4400, -3090, -1790, 0 },
    { 5600, -3090, -1790, 0 },
    { 8400, -3090, -1790, 0 },
    { 9600, -3090, -1790, 0 },
};

SVECTOR D_shelter_1f_parking_garage_80180C4C[1] = {
    { 2000, -2130, 2230, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_shelter_1f_parking_garage_80180C64[1] = {
    { D_shelter_1f_parking_garage_80180FE8, D_shelter_1f_parking_garage_801815F8, D_shelter_1f_parking_garage_80181728, NULL },
};

WorldCoordRoomLighting D_shelter_1f_parking_garage_80180C74[1] = {
    { D_shelter_1f_parking_garage_801815E0, D_shelter_1f_parking_garage_801818A4 },
};

u8* D_shelter_1f_parking_garage_80180C7C[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_parking_garage_80180C80[1] = { 4 };

DirectionWarpEntry D_shelter_1f_parking_garage_80180C84[2] = {
    { { { .word = 3072 }, 8564, 0, -1341 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9100, 0, -1560 }, { 0, 0, 0, 0 }, 0x55010003, 0x55010004, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING },
    { { { .word = 2048 }, 2000, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 2000, 0, 1700 }, { 0, 0, 0, 0 }, 0x55010002, 0x55010001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelter1fParkingGarageCollision03A28Normals[11] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_normals.inc"
};

static SVECTOR _gShelter1fParkingGarageCollision03A28Verts[34] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_verts.inc"
};

static WorldCollisionGridFace _gShelter1fParkingGarageCollision03A28Faces[13] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_faces.inc"
};

static s16 _gShelter1fParkingGarageCollision03A28Cells[88] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fParkingGarageCollision03A28Cells[i])
static s16* _gShelter1fParkingGarageCollision03A28Table[16] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_1f_parking_garage_80180FE8[1] = {
    { NULL, _gShelter1fParkingGarageCollision03A28Normals, _gShelter1fParkingGarageCollision03A28Verts, _gShelter1fParkingGarageCollision03A28Faces, _gShelter1fParkingGarageCollision03A28Table, 500, 6250, 4, 4, 4000, 13 },
};

ViewCamera D_shelter_1f_parking_garage_8018100C[4] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 1016, 0, -3967 }, { 94, 4094, 24 }, { 3966, -97, 1016 } }, { -140, 1010, 1370 } }, 257 },
    { { { { 270, 0, -4087 }, { -2037, 3551, -134 }, { 3543, 2041, 234 } }, { -4450, 3000, -1030 } }, 257 },
    { { { { 829, 0, 4011 }, { 1836, 3641, -379 }, { -3565, 1875, 737 } }, { -6440, 3350, 790 } }, 257 },
};

SpriteBatch D_shelter_1f_parking_garage_8018109C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_parking_garage_801810AC[13] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 8, 2250, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -16, 2125, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 16, 2125, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, -16, 1975, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 0, 1975, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 8, 1875, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 8, 1875, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -24, 8, 1875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 8, 1875, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -16, 2000, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -8, -16, 1937, { .fields = { 104, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 16 } }, 8, -16, 1925, { .fields = { 88, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, 40, -16, 1950, { .fields = { 120, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_1f_parking_garage_801811B0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_parking_garage_801811D0[28] = {
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 40, -64, 1125, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, -56, 1375, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -48, 1500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -48, 1375, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 40, -48, 950, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -48, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -48, 950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, -48, 950, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -48, 950, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -40, 950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, 0, 875, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 64, 56, 875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 104, 56, 875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, 40, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, 32, 750, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 136, 16, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 0, 875, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 0, 875, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 875, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 0, 875, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 128, -40, 950, { .fields = { 64, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 120, -40, 950, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 112, -40, 966, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 104, -40, 870, { .fields = { 56, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 96, -40, 900, { .fields = { 56, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, -40, 900, { .fields = { 56, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 56, 32 } }, 32, -40, 900, { .fields = { 8, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_1f_parking_garage_80181400[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_parking_garage_80181420[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_1f_parking_garage_80181430[4] = {
    { { .empty = D_shelter_1f_parking_garage_8018109C }, D_shelter_1f_parking_garage_8018109C, NULL },
    { { .elements = D_shelter_1f_parking_garage_801810AC }, D_shelter_1f_parking_garage_801811B0, NULL },
    { { .elements = D_shelter_1f_parking_garage_801811D0 }, D_shelter_1f_parking_garage_80181400, NULL },
    { { .empty = D_shelter_1f_parking_garage_80181420 }, D_shelter_1f_parking_garage_80181420, NULL },
};

WorldCoordPointLight D_shelter_1f_parking_garage_80181460[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2500, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2130, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 1638, 1638 }, { 0, 0 } }, 1000, 2001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2500, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 4096 },
};

WorldCoordRoomLights D_shelter_1f_parking_garage_801815E0[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_1f_parking_garage_80181460), D_shelter_1f_parking_garage_80181460, 0, NULL },
};

WorldCollisionTrigger D_shelter_1f_parking_garage_801815F8[4] = {
    { NULL, NULL, NULL, { 3453, -3344, 314, 0 }, { { -503, -3680, -4249, 0 }, { 498, -3680, 4245, 0 }, { -503, 3680, -4249, 0 }, { 498, 3680, 4245, 0 } }, { 4068, 0, -480, 0 }, { 0, 0, 4096, 0 }, 5632, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3640, -3392, 347, 0 }, { { 479, -3696, 4156, 0 }, { -488, -3696, -4164, 0 }, { 479, 3696, 4156, 0 }, { -488, 3696, -4164, 0 } }, { -4077, 0, 473, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7056, -3297, 2400, 0 }, { { 432, -3760, 1920, 0 }, { -432, -3760, -1920, 0 }, { 432, 3760, 1920, 0 }, { -432, 3760, -1920, 0 } }, { -3997, 0, 899, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6943, -3394, 2560, 0 }, { { -448, -3696, -2000, 0 }, { 448, -3696, 2000, 0 }, { -448, 3697, -2000, 0 }, { 448, 3697, 2000, 0 } }, { 4004, 0, -898, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_1f_parking_garage_80181728[5] = {
    { NULL, NULL, NULL, { 8560, -64, -928, 0 }, { { -1680, 0, -480, 0 }, { 1680, 0, -480, 0 }, { -1680, 0, 480, 0 }, { 1680, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1745, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2016, -48, 1568, 0 }, { { -496, 0, -256, 0 }, { 496, 0, -256, 0 }, { -496, 0, 256, 0 }, { 496, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 557, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -96, -64, -80, 0 }, { { -496, 0, -1904, 0 }, { 496, 0, -1904, 0 }, { -496, 0, 1904, 0 }, { 496, 0, 1904, 0 } }, { 0, 4100, 0, 0 }, { 4090, 0, 200, 0 }, 1966, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8544, 0, 1024, 0 }, { { -1680, 0, -480, 0 }, { 1680, 0, -480, 0 }, { -1680, 0, 480, 0 }, { 1680, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1745, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2870, -64, 0, 0 }, { { -576, 0, -2656, 0 }, { 576, 0, -2656, 0 }, { -576, 0, 2656, 0 }, { 576, 0, 2656, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 2709, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_parking_garage_801818A4[5] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_parking_garage_801818A4) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 250, 250, 250, 250 } },
};

AreaResource D_shelter_1f_parking_garage_801818CC[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_1f_parking_garage_801818D8[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AEC0, D_shelter_1f_parking_garage_801818CC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_shelter_1f_parking_garage_80181938 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_shelter_1f_parking_garage_80181944[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_1f_parking_garage_8018194C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_1f_parking_garage_80181938 },
};

WorldCollisionSurfaceProperties* D_shelter_1f_parking_garage_80181954[8] = {
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_8018194C,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
};

ScreenFade gRoomEventFade = { 0 };

ScreenFade D_shelter_1f_parking_garage_80181978 = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_1f_parking_garage_80181984[4] = {
    0,
    25,
    36,
    75,
};

RoomDeparture gRoomDeparture;

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 _shelter1fParkingGarageStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->queryOnly` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelter1fParkingGarageStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_parking_garage_80181984_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
            }
            taskSpawnFromTable(&D_shelter_1f_parking_garage_80180BAC, 0, 0, 0);
            D_shelter_1f_parking_garage_80181984_value = 1;
        }
        return 2;
    }
    return 1;
}

#include "../../shared/room_variants_shelter.inc.c"

#include "../../shared/room_event_departure_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

/// Task body that holds `gSceneCombatState.actorControl` set while the caption plays. On caption
/// key 0xB it spawns the 0x31 task and, 30 frames later, advances flag nibble
/// 0x4B from 9 to 0xA, publishes `gRoomDeparture` and
/// spawns entry 0 of `D_shelter_1f_parking_garage_80180BA0`. Any other key
/// clears `gSceneCombatState.actorControl`, restores the weapon and ends the task.
void func_shelter_1f_parking_garage_8017DAF0(Task* task)
{
    RoomDeparture       rec;
    RoomEventMsg        msg;
    RoomDeparture*      p;
    RoomVariantResolver handler;

    switch (task->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                D_shelter_1f_parking_garage_80181978.blend      = SCREEN_FADE_SUBTRACT;
                D_shelter_1f_parking_garage_80181978.phase      = SCREEN_FADE_RUNNING;
                D_shelter_1f_parking_garage_80181978.rampFrames = 0x1E;
                Task_Spawn(1, 0x31, 0, &D_shelter_1f_parking_garage_80181978);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == 9) {
                    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0xA);
                }
                handler      = roomVariantResolveShelter;
                rec.stage    = GAME_STAGE_MINE_SHELTER;
                rec.area     = GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING;
                rec.room     = 1;
                rec.warp     = 2;
                rec.sndEvent = 0x55010004;
                rec.facing   = ROOM_DEPARTURE_SKIP_FACING;
                Gp_MsgPlayerWeapon(0);
                p             = &rec;
                msg.areaId    = p->area;
                msg.warp      = p->warp;
                msg.room      = p->room;
                msg.queryOnly = ROOM_EVENT_EXECUTE;
                handler(&msg, &msg);
                p->area        = msg.areaId;
                p->warp        = msg.warp;
                p->room        = msg.room;
                gRoomDeparture = rec;
                taskSpawnFromTable(&D_shelter_1f_parking_garage_80180BA0, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

s32 func_shelter_1f_parking_garage_8017DCEC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_neo_ark_80179B14`. Message 5 starts the room's event on flag 0x159; any
/// other message answers 1.
s32 func_shelter_1f_parking_garage_8017DCF4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != GAME_AREA_SHELTER_1F_AIRLOCK) {
        return 1;
    }
    event.capCmd   = 3;
    event.stageSnd = 0x55010001;
    event.flagId   = GAME_FLAG_1F_GARAGE_TO_AIRLOCK_SCENE;
    event.fade     = 0;
    return _shelter1fParkingGarageStartEvent(out, &event);
}

s32 func_shelter_1f_parking_garage_8017DE44(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_1f_parking_garage_8017DE4C(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0xA) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(2);
        taskSpawnFromTable(&D_shelter_1f_parking_garage_80180BE0, 0, 0, 0);
    }
    return 0;
}

static void func_shelter_1f_parking_garage_8017DE9C(Task* task)
{
    task->msgTable = D_shelter_1f_parking_garage_80180BB8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.warp == 1) {
        Gp_RunCapCmd1(5);
    }
    task->state = task->state + 1;
}

/// State table of the room's controller task
/// `func_shelter_1f_parking_garage_8017DF14`: set up the room, then idle.
static const TaskFuncTable3 D_shelter_1f_parking_garage_8017D6A0 = { {
    func_shelter_1f_parking_garage_8017DE9C,
    func_shelter_1f_parking_garage_8017DF04,
    taskKill,
} };

/// Idle state of the room's controller task: does nothing. The 0x10-byte
/// frame is the compiler's, kept for an unused local.
static void func_shelter_1f_parking_garage_8017DF04(Task* task)
{
    char pad[0x10];
}

/// The room's controller task: copies its three-entry state table to the
/// stack and runs the entry for the current state.
void func_shelter_1f_parking_garage_8017DF14(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_parking_garage_8017D6A0;
    sp.funcs[task->state](task);
}

void func_shelter_1f_parking_garage_8017DF6C(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        gRoomEffectFlashId      = EFFECT_SHELTER_1F_PARKING_GARAGE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_PARKING_GARAGE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_PARKING_GARAGE_SPARK_BURST;
        arg0->state             = 1;
    }

    view = viewGetMappedIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_parking_garage_80180BFC;
            glowDrawAngledCapsule(&p[0], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[2], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[6], 0x200, 0, 0x210);
            glowDrawAngledCapsule(&p[8], 0x200, 0, 0x210);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_1f_parking_garage_80180C4C;
            glowDrawFactorDisc(&p[0], 0x300, 0x200);
            glowDrawAngledCapsule(&p[-12], 0x200, 0x800, 0x210);
            glowDrawAngledCapsule(&p[-6], 0x200, 0, 0x210);
            break;
        }
    }
}

#include "../../shared/glow_draw_angled_capsule.inc.c"

#include "../../shared/glow_draw_factor_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_1f_parking_garage_8017EC0C(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_1f_parking_garage_8017F670(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_1f_parking_garage_8017FF58(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
