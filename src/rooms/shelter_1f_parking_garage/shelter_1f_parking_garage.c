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
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
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
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_1f_parking_garage_80181984[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_1f_parking_garage_80181984_value __asm__("D_shelter_1f_parking_garage_80181984");

extern TaskDesc   D_shelter_1f_parking_garage_80180BA0;
extern TaskDesc   D_shelter_1f_parking_garage_80180BAC;
extern GpMsgEntry D_shelter_1f_parking_garage_80180BB8[];
extern TaskDesc   D_shelter_1f_parking_garage_80180BE0;
extern SVECTOR    D_shelter_1f_parking_garage_80180BFC[];
extern SVECTOR    D_shelter_1f_parking_garage_80180C4C[];

/// Offsets from the parent coordinate of the two trail heads the smoke-trail
/// task follows. The second is also reached under its own name.

extern GpFadeWork   D_shelter_1f_parking_garage_80181974;
extern GpFadeWork   D_shelter_1f_parking_garage_80181978;
extern RoomEventMsg D_shelter_1f_parking_garage_8018197C;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomDeparture value;
    u8            retained[4];
} Shelter1fParkingGarageStorage1988;
STATIC_ASSERT_SIZEOF(Shelter1fParkingGarageStorage1988, 16);

extern Shelter1fParkingGarageStorage1988 D_shelter_1f_parking_garage_80181988;
extern RoomLatchedEvent                  D_shelter_1f_parking_garage_80181998;

static s32  func_shelter_1f_parking_garage_8017D6AC(RoomEventMsg* in, RoomEventMsg* out);
static void func_shelter_1f_parking_garage_8017DE9C(Task* task);
static void func_shelter_1f_parking_garage_8017DF04(Task* task);
static void func_shelter_1f_parking_garage_8017E080(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_1f_parking_garage_8017E868(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

extern GpGridParams               D_shelter_1f_parking_garage_80180FE8[1];
extern GpObj4C                    D_shelter_1f_parking_garage_801815F8[4];
extern GpObj4C                    D_shelter_1f_parking_garage_80181728[5];
extern WorldCoordRoomAmbientEntry D_shelter_1f_parking_garage_801818A4[5];
extern GpRoomCoordSet             D_shelter_1f_parking_garage_801815E0[1];

s32  func_shelter_1f_parking_garage_8017DCEC(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_1f_parking_garage_8017DCF4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_1f_parking_garage_8017DE44(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_1f_parking_garage_8017DE4C(Task*, s32, DirectionActionRequest* request, TaskMessageArg);
void func_shelter_1f_parking_garage_8017D7E8(Task*);
void func_shelter_1f_parking_garage_8017D958(Task*);
void func_shelter_1f_parking_garage_8017DAF0(Task*);

TaskDesc D_shelter_1f_parking_garage_80180BA0 = { 0, 32, func_shelter_1f_parking_garage_8017D7E8, { .model = NULL } };

TaskDesc D_shelter_1f_parking_garage_80180BAC = { 0, 32, func_shelter_1f_parking_garage_8017D958, { .model = NULL } };

GpMsgEntry D_shelter_1f_parking_garage_80180BB8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_parking_garage_8017DCF4 },
    { 5105, func_shelter_1f_parking_garage_8017DCEC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_parking_garage_8017DE4C },
    { 5104, func_shelter_1f_parking_garage_8017DE44 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_1f_parking_garage_80180BE0 = { 0, 32, func_shelter_1f_parking_garage_8017DAF0, { .model = NULL } };

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

GpRoomObjRec D_shelter_1f_parking_garage_80180C64[1] = {
    { D_shelter_1f_parking_garage_80180FE8, D_shelter_1f_parking_garage_801815F8, D_shelter_1f_parking_garage_80181728, NULL },
};

GpRoomCoordRec D_shelter_1f_parking_garage_80180C74[1] = {
    { D_shelter_1f_parking_garage_801815E0, D_shelter_1f_parking_garage_801818A4 },
};

u8* D_shelter_1f_parking_garage_80180C7C[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_parking_garage_80180C80[1] = {
    { { .bytes = { 4, 0 } } },
};

GpWarpRec D_shelter_1f_parking_garage_80180C84[2] = {
    { { .words = { 3072, 8564, 0, -1341 } }, { 0, 0, 0, 0 }, { .words = { 3072, 9100, 0, -1560 } }, { 0, 0, 0, 0 }, 0x55010003, 0x55010004, 0, 2, 0, 436 },
    { { .words = { 2048, 2000, 0, 1700 } }, { 0, 0, 0, 0 }, { .words = { 2048, 2000, 0, 1700 } }, { 0, 0, 0, 0 }, 0x55010002, 0x55010001, 0, 4, 0, 0 },
};

SVECTOR D_shelter_1f_parking_garage_80180CF4[11] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_normals.inc"
};

SVECTOR D_shelter_1f_parking_garage_80180D4C[34] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_verts.inc"
};

GpGridFace D_shelter_1f_parking_garage_80180E5C[13] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_faces.inc"
};

s16 D_shelter_1f_parking_garage_80180EF8[88] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_1f_parking_garage_80180EF8[i])
s16* D_shelter_1f_parking_garage_80180FA8[16] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_1f_parking_garage_80180FE8[1] = {
    { NULL, D_shelter_1f_parking_garage_80180CF4, D_shelter_1f_parking_garage_80180D4C, D_shelter_1f_parking_garage_80180E5C, D_shelter_1f_parking_garage_80180FA8, 500, 6250, 4, 4, 4000, 13 },
};

GpViewRec D_shelter_1f_parking_garage_8018100C[4] = {
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

GpSprtRec D_shelter_1f_parking_garage_80181430[4] = {
    { { .empty = D_shelter_1f_parking_garage_8018109C }, D_shelter_1f_parking_garage_8018109C, NULL },
    { { .elements = D_shelter_1f_parking_garage_801810AC }, D_shelter_1f_parking_garage_801811B0, NULL },
    { { .elements = D_shelter_1f_parking_garage_801811D0 }, D_shelter_1f_parking_garage_80181400, NULL },
    { { .empty = D_shelter_1f_parking_garage_80181420 }, D_shelter_1f_parking_garage_80181420, NULL },
};

GpPointLight D_shelter_1f_parking_garage_80181460[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2500, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2130, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 1638, 1638 }, { 0, 0 } }, 1000, 2001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2500, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 4096 },
};

GpRoomCoordSet D_shelter_1f_parking_garage_801815E0[1] = {
    { 0, NULL, 4, D_shelter_1f_parking_garage_80181460, 0, NULL },
};

GpObj4C D_shelter_1f_parking_garage_801815F8[4] = {
    { NULL, NULL, NULL, { 3453, -3344, 314, 0 }, { { -503, -3680, -4249, 0 }, { 498, -3680, 4245, 0 }, { -503, 3680, -4249, 0 }, { 498, 3680, 4245, 0 } }, { 4068, 0, -480, 0 }, { 0, 0, 4096, 0 }, 5632, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 3640, -3392, 347, 0 }, { { 479, -3696, 4156, 0 }, { -488, -3696, -4164, 0 }, { 479, 3696, 4156, 0 }, { -488, 3696, -4164, 0 } }, { -4077, 0, 473, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 7056, -3297, 2400, 0 }, { { 432, -3760, 1920, 0 }, { -432, -3760, -1920, 0 }, { 432, 3760, 1920, 0 }, { -432, 3760, -1920, 0 } }, { -3997, 0, 899, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 6943, -3394, 2560, 0 }, { { -448, -3696, -2000, 0 }, { 448, -3696, 2000, 0 }, { -448, 3697, -2000, 0 }, { 448, 3697, 2000, 0 } }, { 4004, 0, -898, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 2, 129, 0 },
};

GpObj4C D_shelter_1f_parking_garage_80181728[5] = {
    { NULL, NULL, NULL, { 8560, -64, -928, 0 }, { { -1680, 0, -480, 0 }, { 1680, 0, -480, 0 }, { -1680, 0, 480, 0 }, { 1680, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1745, 5, 10, 20, 2, 0 },
    { NULL, NULL, NULL, { 2016, -48, 1568, 0 }, { { -496, 0, -256, 0 }, { 496, 0, -256, 0 }, { -496, 0, 256, 0 }, { 496, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 557, 0, 5, 33, 2, 0 },
    { NULL, NULL, NULL, { -96, -64, -80, 0 }, { { -496, 0, -1904, 0 }, { 496, 0, -1904, 0 }, { -496, 0, 1904, 0 }, { 496, 0, 1904, 0 } }, { 0, 4100, 0, 0 }, { 4090, 0, 200, 0 }, 1966, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 8544, 0, 1024, 0 }, { { -1680, 0, -480, 0 }, { 1680, 0, -480, 0 }, { -1680, 0, 480, 0 }, { 1680, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1745, 5, 10, 20, 2, 0 },
    { NULL, NULL, NULL, { 0x2870, -64, 0, 0 }, { { -576, 0, -2656, 0 }, { 576, 0, -2656, 0 }, { -576, 0, 2656, 0 }, { 576, 0, 2656, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 2709, 2, 4, 0, 130, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_parking_garage_801818A4[5] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_parking_garage_801818A4) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 250, 250, 250, 250 } },
};

GpAreaTmdRec D_shelter_1f_parking_garage_801818CC[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_1f_parking_garage_801818D8[12] = {
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

s32 D_shelter_1f_parking_garage_80181938[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_shelter_1f_parking_garage_80181944[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_1f_parking_garage_8018194C[1] = {
    { 0, 0, 1, 0, D_shelter_1f_parking_garage_80181938 },
};

GpRoomParamRec* D_shelter_1f_parking_garage_80181954[8] = {
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_8018194C,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
    D_shelter_1f_parking_garage_80181944,
};

GpFadeWork D_shelter_1f_parking_garage_80181974 = { 0 };

GpFadeWork D_shelter_1f_parking_garage_80181978 = { 0 };

RoomEventMsg D_shelter_1f_parking_garage_8018197C = { 0 };

u8 D_shelter_1f_parking_garage_80181984[4] = {
    0,
    25,
    36,
    75,
};

Shelter1fParkingGarageStorage1988 D_shelter_1f_parking_garage_80181988;

RoomLatchedEvent D_shelter_1f_parking_garage_80181998;

static __inline__ s32 _shelter1fParkingGarageStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->queryOnly` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelter1fParkingGarageStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_parking_garage_80181984_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_1f_parking_garage_8018197C = *dst;
            D_shelter_1f_parking_garage_80181998 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BAC, 0, 0, 0);
            D_shelter_1f_parking_garage_80181984_value = 1;
        }
        return 2;
    }
    return 1;
}

/// Answers the progress query `in->msgId` in `out->room`, unless
/// `in->queryOnly` is set. Six queries have an answer, each read from a
/// game-flag nibble: 2 answers 2 once nibble 0x10F is set and 3 once nibble
/// 0x11A reaches 2; 5, 41 and 45 answer nibbles 0xA4, 0xB6 and 0xB7 plus one;
/// 16 answers 3 once nibble 0x7A reaches 6; and 20 maps nibble 0xF4's values
/// 0-3 to 1, 6, 7 and 8 (1 otherwise). Any other query leaves `out`
/// untouched. Always returns 1.
static s32 func_shelter_1f_parking_garage_8017D6AC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (in->areaId) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    out->room = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    out->room = 3;
                }
                break;
            case 5:
                out->room = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    out->room = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        out->room = 1;
                        break;
                    case 1:
                        out->room = 6;
                        break;
                    case 2:
                        out->room = 7;
                        break;
                    case 3:
                        out->room = 8;
                        break;
                    default:
                        out->room = 1;
                        break;
                }
                break;
            case 45:
                out->room = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                out->room = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// The room's exit task, run on the record published in
/// `D_shelter_1f_parking_garage_80181988.value`. State 0 sends the record's
/// `facing` to the slot-3 game pointer as message 0x3EE, going straight to
/// state 2 when it is -1; state 1 waits until that pointer answers 0x3F0
/// with 0. States 2 and 3 play the record's sound event, if any, and wait for
/// it to go quiet. State 4 queues type-7 sound event 0x80000000, commits the
/// record's stage, area, warp and room to the save data, spawns task type
/// 0x11 and kills itself.
void func_shelter_1f_parking_garage_8017D7E8(Task* arg0)
{
    ActorTransform msg;
    void*          slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = D_shelter_1f_parking_garage_80181988.value.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsgPtr(slot, 0x3EE, &msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_1f_parking_garage_80181988.value.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_1f_parking_garage_80181988.value.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_1f_parking_garage_80181988.value.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.spriteVariant        = 1;
            Mc_SaveData[0].state.at4.loc.stage = D_shelter_1f_parking_garage_80181988.value.stage;
            Mc_SaveData[0].state.at4.loc.area  = D_shelter_1f_parking_garage_80181988.value.area;
            Mc_SaveData[0].state.at4.loc.warp  = D_shelter_1f_parking_garage_80181988.value.warp;
            Mc_SaveData[0].state.at4.loc.room  = D_shelter_1f_parking_garage_80181988.value.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room's event task, spawned when the message handler starts an event.
/// State 0 runs the latched event's CAP command; state 1 waits for it and,
/// when the event's `fade` asks for it, spawns helper task 0x31; states 2
/// and 3 play the event's stage sound, if any, and wait for it; state 4
/// commits the latched message's area, warp and room to the save data, spawns
/// task type 0x11 and kills itself.
void func_shelter_1f_parking_garage_8017D958(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_parking_garage_80181998.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_parking_garage_80181998.fade != 0) {
                    D_shelter_1f_parking_garage_80181974.field_0 = 0;
                    D_shelter_1f_parking_garage_80181974.field_1 = 0;
                    D_shelter_1f_parking_garage_80181974.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_1f_parking_garage_80181974);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_parking_garage_80181998.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_parking_garage_80181998.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_parking_garage_80181998.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_1f_parking_garage_8018197C.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_1f_parking_garage_8018197C.warp;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_1f_parking_garage_8018197C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Task body that holds `Gp_StateF0.field_4` set while the caption plays. On caption
/// key 0xB it spawns the 0x31 task and, 30 frames later, advances flag nibble
/// 0x4B from 9 to 0xA, publishes `D_shelter_1f_parking_garage_80181988.value` and
/// spawns entry 0 of `D_shelter_1f_parking_garage_80180BA0`. Any other key
/// clears `Gp_StateF0.field_4`, restores the weapon and ends the task.
void func_shelter_1f_parking_garage_8017DAF0(Task* task)
{
    RoomDeparture  rec;
    RoomEventMsg   msg;
    RoomDeparture* p;
    s32            (*handler)(RoomEventMsg*, RoomEventMsg*);

    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                D_shelter_1f_parking_garage_80181978.field_0 = 0;
                D_shelter_1f_parking_garage_80181978.field_1 = 0;
                D_shelter_1f_parking_garage_80181978.field_2 = 0x1E;
                Task_Spawn(1, 0x31, 0, &D_shelter_1f_parking_garage_80181978);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                Gp_StateF0.field_4 = 0;
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (GameFlag_GetNibble(0x4B) == 9) {
                    GameFlag_SetNibble(0x4B, 0xA);
                }
                handler      = func_shelter_1f_parking_garage_8017D6AC;
                rec.stage    = 4;
                rec.area     = 0x14;
                rec.room     = 1;
                rec.warp     = 2;
                rec.sndEvent = 0x55010004;
                rec.facing   = -1;
                Gp_MsgPlayerWeapon(0);
                p             = &rec;
                msg.areaId    = p->area;
                msg.warp      = p->warp;
                msg.room      = p->room;
                msg.queryOnly = ROOM_EVENT_EXECUTE;
                handler(&msg, &msg);
                p->area                                    = msg.areaId;
                p->warp                                    = msg.warp;
                p->room                                    = msg.room;
                D_shelter_1f_parking_garage_80181988.value = rec;
                Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BA0, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

s32 func_shelter_1f_parking_garage_8017DCEC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
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
    if (in->areaId != 5) {
        return 1;
    }
    event.capCmd   = 3;
    event.stageSnd = 0x55010001;
    event.flagId   = 0x159;
    event.fade     = 0;
    return _shelter1fParkingGarageStartEvent(out, &event);
}

s32 func_shelter_1f_parking_garage_8017DE44(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_1f_parking_garage_8017DE4C(Task* task, s32 msgId, DirectionActionRequest* request, TaskMessageArg arg3)
{
    if (request->actionId == 0xA) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(2);
        Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BE0, 0, 0, 0);
    }
    return 0;
}

static void func_shelter_1f_parking_garage_8017DE9C(Task* task)
{
    task->msgTable = D_shelter_1f_parking_garage_80180BB8;
    Game_SetPtrSlot(task, 7);
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
        D_80115758  = 0x601D6;
        D_8011572C  = 0x601F2;
        D_80115750  = 0x6020E;
        arg0->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_parking_garage_80180BFC;
            func_shelter_1f_parking_garage_8017E080(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_parking_garage_8017E080(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_parking_garage_8017E080(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_parking_garage_8017E080(&p[8], 0x200, 0, 0x210);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_1f_parking_garage_80180C4C;
            func_shelter_1f_parking_garage_8017E868(&p[0], 0x300, 0x200);
            func_shelter_1f_parking_garage_8017E080(&p[-12], 0x200, 0x800, 0x210);
            func_shelter_1f_parking_garage_8017E080(&p[-6], 0x200, 0, 0x210);
            break;
        }
    }
}

/// Draws a capsule-shaped glow between the world point `arg0` and the one
/// after it: a half-disc of gouraud wedges around each end and a band joining
/// them, lit along the centre line and black at the rim. Nothing is drawn
/// unless the second point's OTZ is at least 0x11. `arg1` is the half-extent
/// (the on-screen radius is `(s16)arg1 * 64 / otz`), `arg2` the capsule's
/// angle, and `arg3` the colour: a red byte at bits 8-15 and two-bit green
/// and blue at bits 4 and 0, each scaled by a blend that flickers with the
/// frame counter.
static void func_shelter_1f_parking_garage_8017E080(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 3);
        base      = (s16)arg2;
        b         = blend * (arg3 & 3);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a round glow at the world point `worldPoint`: projected through
/// `gGfxViewCoord.workm` and, when its OTZ is at least 0x11, four gouraud
/// wedges lit at the centre and black at the rim. `radiusScale` is the half-extent
/// (the on-screen radius is `(s16)radiusScale * 64 / otz`) and `packedColor` the colour: a
/// red byte at bits 8-15 and two-bit green and blue at bits 4 and 0, each
/// scaled by a blend that flickers with the frame counter.
///
/// `worldPoint` uses world coordinates; `radiusScale` is narrowed to signed 16 bits
/// before division by camera depth/4. Angles use 4096 units per turn and the
/// trigonometric coordinates use a 12-bit fractional scale. Colour bytes wrap.
static void func_shelter_1f_parking_garage_8017E868(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH       = 17,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE = 0x20,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP = 8,
        ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT      = 12,
        ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN       = 0x1000,
    };

    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    DisplayState*      displayBase;
    DisplayState*      ds;
    s32                radius;
    s32                angle;
    s32                halfStepAngle;
    s32                nextAngle;
    s32                shiftedColor;
    u8                 brightness;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(*block));
        block   = (RoomDraw25Scratch*)tmp;
    }

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - sizeof(*block)))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - sizeof(*block)))->otz >= ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * 64) / ((RoomDraw25Scratch*)(head - sizeof(*block)))->otz;
        displayBase   = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayBase->animFrame & 1) * ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP) | ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE;
        r             = brightness * (shiftedColor >> 24);
        g             = brightness * ((shiftedColor >> 20) & 3);
        b             = brightness * (packedColor & 3);
        angle         = 0;
        ds            = displayBase;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + 0x200;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            nextAngle     = angle + 0x400;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (angle < ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN);
    }
    SCRATCH_POP_BYTES(sizeof(*block));
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_1f_parking_garage_8017EC0C(Task* arg0)
{
    RoomFx_FlashTask(arg0);
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
