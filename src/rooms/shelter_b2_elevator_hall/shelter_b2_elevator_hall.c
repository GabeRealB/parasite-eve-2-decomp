#include "rooms/shelter_b2_elevator_hall.h"

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
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
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

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/shelter_elevator.h"

/// Task descriptor `roomEventGate` spawns when a
/// gated event fires.
extern TaskDesc gRoomEventTaskDesc;

/// Message table `_shelterB2ElevatorHallInitRoomTask` installs on its task.
extern TaskMessageEntry D_shelter_b2_elevator_hall_801837A8[];

/// Copy of the message that fired a gated event, kept for the task
/// `roomEventTask` to warp from.
extern RoomEventMsg gRoomEventMsg;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `roomEventTask` plays.
extern RoomEventReq gRoomEventReq;

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.

extern RoomEventActiveBytes gRoomEventActive;

extern TaskDesc D_shelter_b2_elevator_hall_8018379C;
extern SVECTOR  D_shelter_b2_elevator_hall_801837D8[];
extern SVECTOR  D_shelter_b2_elevator_hall_801837F8[];
extern SVECTOR  D_shelter_b2_elevator_hall_80183808[];
extern SVECTOR  D_shelter_b2_elevator_hall_80183868[];
extern SVECTOR  D_shelter_b2_elevator_hall_801838A8[];
extern SVECTOR  D_shelter_b2_elevator_hall_801838B0[];

static void _shelterB2ElevatorHallInitRoomTask(Task* task);
static void _shelterB2ElevatorHallIdleRoomTask(Task* task);

s32        func_shelter_b2_elevator_hall_8017DAD4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB2ElevatorHallRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB2ElevatorHallIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB2ElevatorHallIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _shelterB2ElevatorHallHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);

/// Room message whose first payload word is the key-item ID to use.
enum { SHELTER_B2_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b2_elevator_hall_8018379C = { { { TASK_BODY_NONE, 32 } }, shelterElevatorTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_elevator_hall_801837A8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_elevator_hall_8017DAD4 },
    { SHELTER_B2_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM, _shelterB2ElevatorHallRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2ElevatorHallIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB2ElevatorHallIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _shelterB2ElevatorHallHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b2_elevator_hall_801837D8[4] = {
    { -8291, -620, -1755, 0 },
    { -7626, -620, -1755, 0 },
    { -5333, -620, -1755, 0 },
    { -4665, -620, -1755, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801837F8[2] = {
    { -1331, -620, -1755, 0 },
    { -663, -620, -1755, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_80183808[12] = {
    { 1669, -620, -1755, 0 },
    { 2328, -620, -1755, 0 },
    { 5664, -620, -1755, 0 },
    { 6336, -620, -1755, 0 },
    { 8672, -620, -1755, 0 },
    { 9336, -620, -1755, 0 },
    { -1331, -620, 1757, 0 },
    { -663, -620, 1757, 0 },
    { 1669, -620, 1757, 0 },
    { 2328, -620, 1757, 0 },
    { -9760, -620, -235, 0 },
    { -9760, -620, 368, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_80183868[8] = {
    { 0x280E, -620, 2564, 0 },
    { 0x280E, -620, 3170, 0 },
    { 0x2749, -2342, 3789, 0 },
    { 0x2749, -2342, 4279, 0 },
    { 0x2735, -2342, -255, 0 },
    { 0x2735, -2342, -744, 0 },
    { -5622, -2399, 1652, 0 },
    { -5249, -2399, 1652, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801838A8[1] = {
    { 9936, -1250, 3234, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801838B0[1] = {
    { 9901, -1195, 3234, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { \
    { 0, 1, 2 },                           \
    { 2, 1, 0 },                           \
    { 0, 2, 1 },                           \
}
#define ROOM_FX_HALO_STORAGE_TYPE  RoomFxShade
#define ROOM_FX_HALO_STORAGE_BOUND [3]
#include "../../shared/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawTintedDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b2_elevator_hall_801838DC[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_elevator_hall_801838E0[1] = { 7 };

DirectionWarpEntry D_shelter_b2_elevator_hall_801838E4[3] = {
    { { { .word = 2048 }, -5465, 0, 1040 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -5465, 0, 1040 }, { 0, 0, 0, 0 }, 0x541B0006, 0x541B0005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1C7 },
    { { { .word = 3072 }, 9400, 0, -600 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9400, 0, -600 }, { 0, 0, 0, 0 }, 0x541B0002, 0x541B0001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1BB },
    { { { .word = 3072 }, 9484, 0, 3743 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9484, 0, 3743 }, { 0, 0, 0, 0 }, 0x541B0004, 0x541B0003, 0x541B0008, 6, DIRECTION_WARP_FLAG_NONE, 457 },
};

static SVECTOR _gShelterB2ElevatorHallCollision067F4Normals[7] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_normals.inc"
};

static SVECTOR _gShelterB2ElevatorHallCollision067F4Verts[48] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_verts.inc"
};

static WorldCollisionGridFace _gShelterB2ElevatorHallCollision067F4Faces[28] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_faces.inc"
};

static s16 _gShelterB2ElevatorHallCollision067F4Cells[124] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2ElevatorHallCollision067F4Cells[i])
static s16* _gShelterB2ElevatorHallCollision067F4Table[10] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_elevator_hall_80183DB4 = { NULL, _gShelterB2ElevatorHallCollision067F4Normals, _gShelterB2ElevatorHallCollision067F4Verts, _gShelterB2ElevatorHallCollision067F4Faces, _gShelterB2ElevatorHallCollision067F4Table, 9350, 1481, 5, 2, 4000, 28 };

ViewCamera D_shelter_b2_elevator_hall_80183DD8[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -20, 0x7530, -890 } }, 358 },
    { { { { 1273, 0, 3893 }, { 27, 4095, -8 }, { -3893, 28, 1273 } }, { 2681, 1099, 981 } }, 230 },
    { { { { 1056, 0, 3957 }, { 303, 4083, -81 }, { -3945, 314, 1053 } }, { -2887, 1466, 974 } }, 230 },
    { { { { 1008, 0, -3969 }, { -238, 4088, -60 }, { 3962, 246, 1006 } }, { 2609, 1438, 888 } }, 230 },
    { { { { 1006, 0, -3970 }, { -325, 4082, -82 }, { 3957, 335, 1003 } }, { -1476, 1537, 851 } }, 230 },
    { { { { 3845, 0, -1410 }, { -107, 4084, -293 }, { 1406, 312, 3834 } }, { -4795, 1541, 1484 } }, 230 },
    { { { { -1552, 0, -3790 }, { -270, 4085, 110 }, { 3780, 292, -1548 } }, { -8355, 1552, -250 } }, 257 },
};

SpriteBatch D_shelter_b2_elevator_hall_80183ED4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80183EE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80183EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_elevator_hall_80183F04[10] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -96, -8, 1410, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, -48, 1362, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -64, 0, 1605, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -64, -48, 1559, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, -48, 1616, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, 0, 1672, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -24, -8, 1660, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -48, 1646, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -48, 1815, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -8, 1799, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_elevator_hall_80183FCC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b2_elevator_hall_80183FE4[2] = {
    { { 147, 70, 103, 94 }, 1850 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b2_elevator_hall_80183FF8[12] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 72, 822, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -120, 645, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -112, -120, 780, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -112, -64, 809, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, -24, 825, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 654, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, -64, 651, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 658, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 667, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, 24, 835, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 72, 834, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 72, 822, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_elevator_hall_801840E8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80184100[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80184110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_elevator_hall_80184120[7] = {
    { { .empty = D_shelter_b2_elevator_hall_80183ED4 }, D_shelter_b2_elevator_hall_80183ED4, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80183EE4 }, D_shelter_b2_elevator_hall_80183EE4, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80183EF4 }, D_shelter_b2_elevator_hall_80183EF4, NULL },
    { { .elements = D_shelter_b2_elevator_hall_80183F04 }, D_shelter_b2_elevator_hall_80183FCC, D_shelter_b2_elevator_hall_80183FE4 },
    { { .elements = D_shelter_b2_elevator_hall_80183FF8 }, D_shelter_b2_elevator_hall_801840E8, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80184100 }, D_shelter_b2_elevator_hall_80184100, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80184110 }, D_shelter_b2_elevator_hall_80184110, NULL },
};

WorldCoordPointLight D_shelter_b2_elevator_hall_80184174[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -681, -659, -1106 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -681, -659, 1137 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1648, -659, 1137 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1047, -659, -1192 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6077, -659, -1034 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8608, -659, -855 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8600, -659, 2166 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4834, -659, 2957 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7805, -6652, 1436 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3279, 3368, 3448 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2330, -6652, -263 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3267, 3308, 3588 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5813, -5652, -97 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3366, 3466, 3708 }, { 0, 0 } }, 2000, 0x2801 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8513, -659, 35 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7513, -659, -1106 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4617, -659, -1134 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
};

WorldCoordRoomLights D_shelter_b2_elevator_hall_801846B4 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_elevator_hall_80184174), D_shelter_b2_elevator_hall_80184174, 0, NULL };

WorldCollisionTrigger D_shelter_b2_elevator_hall_801846CC[8] = {
    { NULL, NULL, NULL, { -4678, 64, -197, 0 }, { { 115, -2016, 2533, 0 }, { -124, -2016, -2543, 0 }, { 115, 2016, 2533, 0 }, { -124, 2016, -2543, 0 } }, { -4101, 0, 192, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4865, 0, -144, 0 }, { { -141, -2016, -2631, 0 }, { 126, -2016, 2616, 0 }, { -141, 2016, -2631, 0 }, { 126, 2016, 2616, 0 } }, { 4090, 0, -209, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 64, 0, 0, 0 }, { { -5, -2016, -2627, 0 }, { 5, -2016, 2627, 0 }, { -5, 2016, -2627, 0 }, { 5, 2016, 2627, 0 } }, { 4095, 0, -8, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 256, 0, 0, 0 }, { { -5, -2016, 2541, 0 }, { 5, -2016, -2541, 0 }, { -5, 2016, 2541, 0 }, { 5, 2016, -2541, 0 } }, { -4106, 0, -9, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, 0, 0, 0 }, { { -259, -2016, 2523, 0 }, { 249, -2016, -2533, 0 }, { -259, 2016, 2523, 0 }, { 249, 2016, -2533, 0 } }, { -4084, 0, -412, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4065, 0, 0, 0 }, { { 249, -2016, -2620, 0 }, { -257, -2016, 2609, 0 }, { 249, 2016, -2620, 0 }, { -257, 2016, 2609, 0 } }, { 4076, 0, 394, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7680, 0, 2320, 0 }, { { -3429, -2016, -371, 0 }, { 3429, -2016, 371, 0 }, { -3429, 2016, -371, 0 }, { 3429, 2016, 371, 0 } }, { 441, 0, -4083, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7808, 0, 2097, 0 }, { { 3515, -2016, 381, 0 }, { -3515, -2016, -381, 0 }, { 3515, 2016, 381, 0 }, { -3515, 2016, -381, 0 } }, { -443, 0, 4080, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b2_elevator_hall_8018492C[1] = {
    { NULL, NULL, { 2560, -1344, 3856, 0 }, { { -1664, 2368, 2096, 0 }, { 1664, 2368, -2096, 0 }, { -1664, -2368, 2096, 0 }, { 1664, -2368, -2096, 0 } }, { 3208, 0, 2546, 0 }, 3565, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_elevator_hall_80184968[3] = {
    { NULL, NULL, NULL, { -5632, -48, 1072, 0 }, { { -832, 0, -464, 0 }, { 833, 0, -464, 0 }, { -832, 0, 464, 0 }, { 833, 0, 464, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_WARP, 33, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9712, -48, -464, 0 }, { { 384, 0, -624, 0 }, { 384, 0, 624, 0 }, { -384, 0, -624, 0 }, { -384, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_WARP, 26, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9648, -48, 4272, 0 }, { { 384, 0, -752, 0 }, { 384, 0, 752, 0 }, { -384, 0, -752, 0 }, { -384, 0, 752, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, 28, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_elevator_hall_80184A4C[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_elevator_hall_80184A64[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_elevator_hall_80184A7C[2] = {
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_elevator_hall_80184A94[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_elevator_hall_80184AB8[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_elevator_hall_80184ADC[8] = {
    { 26, 0, 0, 7500, 0, 4150, 1800, 0, 0, 2, 0 },
    { 26, 0, 0, 7000, 0, 1950, 1300, 0, 0, 2, 0 },
    { 26, 0, 0, 5300, 0, 3600, 1700, 0, 0, 2, 0 },
    { 26, 0, 0, 5400, 0, 1700, 1500, 0, 0, 2, 0 },
    { 26, 0, 0, 1850, 0, 350, 1250, 0, 0, 2, 0 },
    { 26, 0, 0, 300, 0, 600, 1200, 0, 0, 2, 0 },
    { 26, 0, 0, 300, 0, -800, 900, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184B5C[3] = {
    { 3, 0, 0, 6500, 0, 3300, 2048, 0, 0, 2, 0 },
    { 3, 0, 1, -4000, 0, 0, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184B8C[4] = {
    { 49, 2, 0, -7800, 0, -200, 1200, 0, 0, 2, 0 },
    { 49, 2, 0, 250, 0, 600, 1600, 0, 0, 2, 0 },
    { 49, 2, 0, 4350, 0, 500, 1800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184BCC[6] = {
    { 21, 3, 0, 7500, -1800, -1400, 0, 0, 0, 2, 0 },
    { 21, 3, 0, 5500, -1800, -1400, 0, 0, 0, 2, 0 },
    { 21, 3, 0, 7500, -1800, 4900, 2048, 0, 0, 2, 0 },
    { 21, 3, 0, 5500, -1800, 4900, 2048, 0, 0, 2, 0 },
    { 57, 3, 1, 6500, 0, 3000, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184C2C[5] = {
    { 21, 3, 0, 8500, -1800, 4900, 2048, 0, 0, 2, 7 },
    { 21, 3, 0, 6500, -1800, 4900, 2048, 0, 0, 2, 7 },
    { 57, 9, 1, -2000, 0, 0, 1024, 0, 2, 4, 0 },
    { 57, 0, 0, 9600, 0, -500, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_elevator_hall_80184C7C[22] = {
    { NULL, NULL },
    { D_shelter_b2_elevator_hall_80184ADC, D_shelter_b2_elevator_hall_80184A4C },
    { D_shelter_b2_elevator_hall_80184B5C, D_shelter_b2_elevator_hall_80184A64 },
    { D_shelter_b2_elevator_hall_80184B8C, D_shelter_b2_elevator_hall_80184A7C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_elevator_hall_80184BCC, D_shelter_b2_elevator_hall_80184A94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_elevator_hall_80184C2C, D_shelter_b2_elevator_hall_80184AB8 },
};

WorldCollisionFootstepSounds D_shelter_b2_elevator_hall_80184D2C = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b2_elevator_hall_80184D38 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b2_elevator_hall_80184D44[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_elevator_hall_80184D4C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_elevator_hall_80184D2C },
};

WorldCollisionSurfaceProperties D_shelter_b2_elevator_hall_80184D54[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_elevator_hall_80184D38 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_elevator_hall_80184D5C[8] = {
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D4C,
    D_shelter_b2_elevator_hall_80184D54,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 253, 152, 217 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/shelter_elevator_task.inc.c"

/// Message handler: copies the incoming message to `out` and forwards both to
/// `mapShelterRoomVariantResolve`. Messages 0x21 and 0x1C build a request for the gate
/// `roomEventGate` (nibble 0xAB with no collected bit, and
/// nibble 0xA9 with collected bit 0x21, which also sets item-seen bit 0x121 when the
/// gate reports the event fired). Message 0x1A
/// answers 0 and, unless `in->queryOnly` asks for a dry run, either sets the
/// message's nibble and runs CAP command 4 while nibble 0xBA is clear, or runs
/// CAP command 5 and spawns the room's task once it is set. Anything else
/// answers 1.
s32 func_shelter_b2_elevator_hall_8017DAD4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        req.capCmd        = 1;
        req.missingCapCmd = 1;
        req.firstSnd      = 0x541B0007;
        req.secondSnd     = 0x541B0005;
        req.flagId        = GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED;
        req.collectedBit  = 0;
        return roomEventGate(&req, out);
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY) {
        req.capCmd        = 3;
        req.missingCapCmd = 2;
        req.firstSnd      = 0x541B0009;
        req.secondSnd     = 0x541B0003;
        req.flagId        = GAME_FLAG_B2_HALL_SOUTH_WALKWAY_DOOR_UNLOCKED;
        req.collectedBit  = 0x21;
        ret               = roomEventGate(&req, out);
        if (gRoomEventActive.eventStarted != 0) {
            itemSetIdentified(0x121, 1);
        }
        return ret;
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_ELEVATOR) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(4);
            }
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd(5, 0);
            taskSpawnFromTable(&D_shelter_b2_elevator_hall_8018379C, 0, 0x541B0001, 0);
        }
        return 0;
    }
    return 1;
}

/// Refuses every key-item use request (message 0x13F1), returning zero.
static s32 _shelterB2ElevatorHallRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores room commands and their argument words, returning zero.
static s32 _shelterB2ElevatorHallIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores the borrowed room-action request, returning zero without reading it.
static s32 _shelterB2ElevatorHallIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Starts the elevator sound script for cue 1; other cues do nothing, and all return zero.
static s32 _shelterB2ElevatorHallHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum { ELEVATOR_SOUND_CUE    = 1,
           ELEVATOR_SOUND_SCRIPT = 0x541B0001 };

    if (cueId == ELEVATOR_SOUND_CUE) {
        sndEvtRequestScriptStart(ELEVATOR_SOUND_SCRIPT, 0, 0);
    }
    return 0;
}

/// The room's three-entry task state table, dispatched by
/// `shelterB2ElevatorHallRoomTask` from a stack copy.
static const TaskFuncTable3 D_shelter_b2_elevator_hall_8017D5F0 = {
    {
        _shelterB2ElevatorHallInitRoomTask,
        _shelterB2ElevatorHallIdleRoomTask,
        taskKill,
    },
};

/// Installs the room message handlers and registers the task as the current room receiver.
///
/// Called in state 0; advances to the idle state without allocating work or a body.
static void _shelterB2ElevatorHallInitRoomTask(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_hall_801837A8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room receiver alive in state 1 while messages perform its work.
static void _shelterB2ElevatorHallIdleRoomTask(Task* task)
{
}

void shelterB2ElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b2_elevator_hall_8017D5F0;
    states.funcs[task->state](task);
}

/// Draws the south walkway door's blue unlocked or red locked status glow.
///
/// A nonzero unlock nibble selects the blue lamp's world point; zero selects
/// the red lamp's. The room glow task calls this for mapped views 5 and 6.
/// Requires composed view matrices, an initialized scratch stack and the
/// current frame's primitive arena and depth ordering table.
static inline void _shelterB2ElevatorHallDrawDoorIndicator(void)
{
    enum {
        DOOR_INDICATOR_RADIUS_SCALE  = 0x100,                                        // Outer pixels = scale * 64 / (camera Z / 4 + 1)
        DOOR_INDICATOR_FLICKER_SHIFT = 5,                                            // Odd animation frames add 32 to each RGB byte
        DOOR_UNLOCKED_COLOR          = (DOOR_INDICATOR_FLICKER_SHIFT << 12) | 0x04C, // Base RGB bytes 0, 64, 192
        DOOR_LOCKED_COLOR            = (DOOR_INDICATOR_FLICKER_SHIFT << 12) | 0xC40, // Base RGB bytes 192, 64, 0
    };

    if (gameFlagGetNibble(GAME_FLAG_B2_HALL_SOUTH_WALKWAY_DOOR_UNLOCKED) != 0) {
        _glowDrawTintedDisc(D_shelter_b2_elevator_hall_801838A8, DOOR_INDICATOR_RADIUS_SCALE, DOOR_UNLOCKED_COLOR);
    } else {
        _glowDrawTintedDisc(D_shelter_b2_elevator_hall_801838B0, DOOR_INDICATOR_RADIUS_SCALE, DOOR_LOCKED_COLOR);
    }
}

void shelterB2ElevatorHallDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           MAPPED_VIEW_MASK = 0xFF,
           LAMP_RADIUS      = 0x180,
           ACCENT_RADIUS    = 0x200,
           LAMP_COLOR       = 0x111, // RGB nibbles, each scaled by 16
           ACCENT_COLOR     = 0x412 };

    // Select this loaded room's effect implementations before any dependent spawn.
    if (task->state == GLOWS_INITIALIZE) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B2_ELEVATOR_HALL_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B2_ELEVATOR_HALL_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B2_ELEVATOR_HALL_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B2_ELEVATOR_HALL_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B2_ELEVATOR_HALL_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_ELEVATOR_HALL_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_ELEVATOR_HALL_SPARK_BURST;
        task->state               = GLOWS_DRAW;
    }

    // Draw only the world-space lamps visible from the mapped camera.
    // Views 2..4 span adjacent lamp-point arrays; their containing object is unproven.
    switch (viewGetMappedIndex() & MAPPED_VIEW_MASK) {
        case 2: {
            const SVECTOR* lampEndpoints;
            lampEndpoints = D_shelter_b2_elevator_hall_801837D8;
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[2], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[16], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[24], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 3: {
            const SVECTOR* lampEndpoints;
            lampEndpoints = D_shelter_b2_elevator_hall_801837F8;
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[8], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[12], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[20], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 4: {
            const SVECTOR* lampEndpoints;
            lampEndpoints = D_shelter_b2_elevator_hall_80183808;
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[8], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[12], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[16], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 5: {
            const SVECTOR* lampEndpoints;
            _shelterB2ElevatorHallDrawDoorIndicator();
            lampEndpoints = D_shelter_b2_elevator_hall_80183868;
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[2], ACCENT_RADIUS, ACCENT_COLOR);
            _glowDrawCapsule(&lampEndpoints[4], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 6: {
            const SVECTOR* lampEndpoints;
            _shelterB2ElevatorHallDrawDoorIndicator();
            lampEndpoints = D_shelter_b2_elevator_hall_80183868;
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[2], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_tinted_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB2ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b2_elevator_hall_801816C8(Task* arg0)
{
    _roomVisualEffectsSparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_elevator_hall_80182B48(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
