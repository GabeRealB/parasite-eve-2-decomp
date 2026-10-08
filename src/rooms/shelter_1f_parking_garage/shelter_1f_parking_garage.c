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
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
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

static s32 _roomVariantResolveShelter(RoomEventMsg* request, RoomEventMsg* reply);

extern u8 D_shelter_1f_parking_garage_80181984;

extern TaskDesc         D_shelter_1f_parking_garage_80180BA0;
extern TaskDesc         D_shelter_1f_parking_garage_80180BAC;
extern TaskMessageEntry D_shelter_1f_parking_garage_80180BB8[];
extern TaskDesc         D_shelter_1f_parking_garage_80180BE0;
extern SVECTOR          D_shelter_1f_parking_garage_80180BFC[];
extern SVECTOR          D_shelter_1f_parking_garage_80180C4C[];

extern ScreenFade       gRoomEventFade;
extern ScreenFade       D_shelter_1f_parking_garage_80181978;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomDeparture    gRoomDeparture;
extern RoomLatchedEvent gRoomEventLatched;

static void _shelter1fParkingGarageInitializeRoom(Task* task);
static void _shelter1fParkingGarageIdle(Task* task);

extern WorldCollisionGrid         D_shelter_1f_parking_garage_80180FE8[1];
extern WorldCollisionTrigger      D_shelter_1f_parking_garage_801815F8[4];
extern WorldCollisionTrigger      D_shelter_1f_parking_garage_80181728[5];
extern WorldCoordRoomAmbientEntry D_shelter_1f_parking_garage_801818A4[5];
extern WorldCoordRoomLights       D_shelter_1f_parking_garage_801815E0[1];

static s32  _shelter1fParkingGarageRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32  _shelter1fParkingGarageResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelter1fParkingGarageIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg);
static s32  _shelter1fParkingGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _shelter1fParkingGarageUndergroundDepartureTask(Task* task);

/// The room's key-item request ID and the reply that displays "cannot use now".
enum {
    SHELTER_1F_PARKING_GARAGE_MESSAGE_USE_KEY_ITEM = 0x13F1,
    SHELTER_1F_PARKING_GARAGE_KEY_ITEM_UNUSABLE    = 0,
};

TaskDesc D_shelter_1f_parking_garage_80180BA0 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

TaskDesc D_shelter_1f_parking_garage_80180BAC = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_1f_parking_garage_80180BB8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fParkingGarageResolveRoomEvent },
    { SHELTER_1F_PARKING_GARAGE_MESSAGE_USE_KEY_ITEM, _shelter1fParkingGarageRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fParkingGarageHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fParkingGarageIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_1f_parking_garage_80180BE0 = { { { TASK_BODY_NONE, 32 } }, _shelter1fParkingGarageUndergroundDepartureTask, { .value = 0 } };

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

u8 D_shelter_1f_parking_garage_80181984 = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_shelter_1f_parking_garage_80181985 = 25;

u8 D_shelter_1f_parking_garage_80181986 = 36;

u8 D_shelter_1f_parking_garage_80181987 = 75;

RoomDeparture gRoomDeparture;

RoomLatchedEvent gRoomEventLatched;

static void _glowDrawAngledCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 when the room handles the departure, including an eligible query;
/// returns 1 for ordinary departure when a nonzero event flag is already set.
/// Every call clears the latest-start byte. Only `ROOM_EVENT_EXECUTE` copies
/// both records, writes 1 to a nonzero flag and raises that byte after spawning.
/// Borrows complete records for this call; flag IDs must be 0..503. The room's
/// singleton copies and CAP/sound resources must remain live until its task ends;
/// do not latch another event while that task still uses them.
static __inline__ s32 _shelter1fParkingGarageStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    D_shelter_1f_parking_garage_80181984 = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *message;
            gRoomEventLatched   = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&D_shelter_1f_parking_garage_80180BAC, 0, 0, 0);
            D_shelter_1f_parking_garage_80181984 = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

#include "../../shared/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room_event_departure_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

/// Resolves an initialized departure's selectors through a stage's room resolver.
///
/// Borrows both arguments during the call. Only area, warp, room and execute
/// mode are initialized in the temporary message; the resolver must read no
/// other fields and must accept aliased request/reply storage. Other departure
/// fields are preserved.
static inline void _roomVariantResolveDeparture(RoomDeparture* departure, RoomVariantResolver resolveVariant)
{
    RoomEventMsg request;

    request.areaId    = departure->area;
    request.warp      = departure->warp;
    request.room      = departure->room;
    request.queryOnly = ROOM_EVENT_EXECUTE;
    resolveVariant(&request, &request);
    departure->area = request.areaId;
    departure->warp = request.warp;
    departure->room = request.room;
}

/// Waits for the garage travel choice, fades out and stages the underground departure.
///
/// Starts bodyless in state 0. Choice 11 starts a 30-tick subtractive fade;
/// other choices resume actor/player control and end the task. On expiry,
/// advances companion schedule 9 to 10, resolves the Mine/Shelter destination
/// and starts the departure task. The final call still decrements the countdown
/// after requesting teardown. Room, fade and destination resources must remain live.
static void _shelter1fParkingGarageUndergroundDepartureTask(Task* task)
{
    enum {
        DEPARTURE_WAIT_CAP,
        DEPARTURE_CHECK_CHOICE,
        DEPARTURE_WAIT_FADE,
        DEPARTURE_CONFIRMED_CHOICE          = 11,
        DEPARTURE_FADE_FRAMES               = 30,
        DEPARTURE_FADE_TASK_BANK            = 1,
        DEPARTURE_FADE_TASK_TYPE            = 0x31,
        DEPARTURE_COMPANION_SCHEDULE_BEFORE = 9,
        DEPARTURE_COMPANION_SCHEDULE_AFTER  = 10,
        DEPARTURE_DEFAULT_ROOM              = 1,
        DEPARTURE_UNDERGROUND_ARRIVAL_WARP  = 2,
        DEPARTURE_SOUND                     = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 4)
    };
    RoomDeparture       departure;
    RoomVariantResolver resolveVariant;

    switch (task->state) {
        case DEPARTURE_WAIT_CAP:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case DEPARTURE_CHECK_CHOICE:
            if (capGetVariantKey() == DEPARTURE_CONFIRMED_CHOICE) {
                D_shelter_1f_parking_garage_80181978.blend      = SCREEN_FADE_SUBTRACT;
                D_shelter_1f_parking_garage_80181978.phase      = SCREEN_FADE_RUNNING;
                D_shelter_1f_parking_garage_80181978.rampFrames = DEPARTURE_FADE_FRAMES;
                taskSpawn(DEPARTURE_FADE_TASK_BANK, DEPARTURE_FADE_TASK_TYPE, 0, &D_shelter_1f_parking_garage_80181978);
                task->killCountdown = DEPARTURE_FADE_FRAMES;
                task->state++;
            } else {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
        case DEPARTURE_WAIT_FADE:
            if (task->killCountdown == 0) {
                if (gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == DEPARTURE_COMPANION_SCHEDULE_BEFORE) {
                    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, DEPARTURE_COMPANION_SCHEDULE_AFTER);
                }
                resolveVariant     = _roomVariantResolveShelter;
                departure.stage    = GAME_STAGE_MINE_SHELTER;
                departure.area     = GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING;
                departure.room     = DEPARTURE_DEFAULT_ROOM;
                departure.warp     = DEPARTURE_UNDERGROUND_ARRIVAL_WARP;
                departure.sndEvent = DEPARTURE_SOUND;
                departure.facing   = ROOM_DEPARTURE_SKIP_FACING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                // Resolve the destination before publishing the singleton departure.
                _roomVariantResolveDeparture(&departure, resolveVariant);
                gRoomDeparture = departure;
                taskSpawnFromTable(&D_shelter_1f_parking_garage_80180BA0, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Refuses every key-item use in the parking garage without changing the room.
///
/// Receives an integer item ID and an unused second argument. All parameters
/// are ignored; the zero reply selects the key-item menu's "cannot use now" notice.
static s32 _shelter1fParkingGarageRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return SHELTER_1F_PARKING_GARAGE_KEY_ITEM_UNUSABLE;
}

/// Resolves a destination variant and gates the garage-to-airlock departure scene.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; borrows complete eight-byte request
/// and writable reply records, which may alias. Returns 1 for ordinary passage
/// or an already-seen scene, otherwise 2 for a room-handled departure. Queries
/// never latch the scene. An executing first visit copies the records for the
/// staged controller; keep the room and map overlays loaded until it finishes.
static s32 _shelter1fParkingGarageResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        TRANSITION_DIRECT   = 1,
        AIRLOCK_CAP_COMMAND = 3,
        AIRLOCK_SOUND       = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 1),
        AIRLOCK_FADE_NONE   = 0
    };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_1F_AIRLOCK) {
        return TRANSITION_DIRECT;
    }
    event.capCmd   = AIRLOCK_CAP_COMMAND;
    event.stageSnd = AIRLOCK_SOUND;
    event.flagId   = GAME_FLAG_1F_GARAGE_TO_AIRLOCK_SCENE;
    event.fade     = AIRLOCK_FADE_NONE;
    return _shelter1fParkingGarageStartEvent(reply, &event);
}

/// Ignores CAP room commands and returns zero without changing the room.
///
/// Both integer argument words and the receiver/message ID are unused.
static s32 _shelter1fParkingGarageIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg)
{
    return 0;
}

/// Starts the underground-parking travel prompt for room action 10.
///
/// Borrows the four-byte `DirectionActionRequest` only during dispatch; its
/// argument and the zero second word are unused. Holds the player, starts CAP
/// command 2 and spawns the choice/departure task. Every action returns zero.
static s32 _shelter1fParkingGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { ACTION_UNDERGROUND_DEPARTURE = 10,
           CAP_UNDERGROUND_PROMPT       = 2 };

    if (request->actionId == ACTION_UNDERGROUND_DEPARTURE) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        capRunCommandWithTransition(CAP_UNDERGROUND_PROMPT);
        taskSpawnFromTable(&D_shelter_1f_parking_garage_80180BE0, 0, 0, 0);
    }
    return 0;
}

/// Installs the room message receiver and plays the warp-1 arrival CAP command.
///
/// State 0 publishes a borrowed live task in `GAME_TASK_SLOT_ROOM`, then enters
/// idle state 1. Requires initialized session/CAP resources and this room loaded.
static void _shelter1fParkingGarageInitializeRoom(Task* task)
{
    enum { ARRIVAL_CAP_WARP    = 1,
           ARRIVAL_CAP_COMMAND = 5 };
    task->msgTable = D_shelter_1f_parking_garage_80180BB8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.warp == ARRIVAL_CAP_WARP) {
        capRunCommandWithTransition(ARRIVAL_CAP_COMMAND);
    }
    task->state = task->state + 1;
}

/// State table of the room's controller task
/// `shelter1fParkingGarageRoomTask`: set up the room, then idle.
static const TaskFuncTable3 D_shelter_1f_parking_garage_8017D6A0 = { {
    _shelter1fParkingGarageInitializeRoom,
    _shelter1fParkingGarageIdle,
    taskKill,
} };

/// Keeps the room controller alive to receive messages without advancing its state.
static void _shelter1fParkingGarageIdle(Task* task)
{
    // Preserve the idle callback's stack reservation; the bytes are never read.
    char reservedFrame[0x10];
}

void shelter1fParkingGarageRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_1f_parking_garage_8017D6A0;
    stateHandlers.funcs[task->state](task);
}

/// Binds actor-spawned effects to the parking garage's room implementations.
///
/// Installs packed bank/type IDs for the Pawn/Rook GOLEMs' silence-scream flash,
/// sword trail and grenade-impact burst. Call after room-effect controller
/// initialization clears the selectors and before actors spawn these effects.
/// This room overlay must remain loaded while the selected effect tasks run.
static inline void _shelter1fParkingGarageBindActorEffects(void)
{
    gRoomEffectFlashId      = EFFECT_SHELTER_1F_PARKING_GARAGE_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_PARKING_GARAGE_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_PARKING_GARAGE_SPARK_BURST;
}

void shelter1fParkingGarageDrawViewGlowsTask(Task* task)
{
    enum {
        GLOWS_INITIALIZE            = 0,
        GLOWS_DRAW                  = 1,
        GLOWS_CAPSULE_RADIUS_SCALE  = 0x200,
        GLOWS_DISC_RADIUS_SCALE     = 0x300,
        GLOWS_CAPSULE_COLOR_FACTORS = (2 << 8) | (1 << 4),
        GLOWS_DISC_COLOR_FACTORS    = 2 << 8,
    };
    u8 mappedView;

    // Bind actor effect IDs once, after the room-effect controller resets them.
    if (task->state == GLOWS_INITIALIZE) {
        _shelter1fParkingGarageBindActorEffects();
        task->state = GLOWS_DRAW;
    }

    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2: {
            const SVECTOR* capsulePoints = D_shelter_1f_parking_garage_80180BFC;
            _glowDrawAngledCapsule(&capsulePoints[0], GLOWS_CAPSULE_RADIUS_SCALE, GLOW_HALF_TURN, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(&capsulePoints[2], GLOWS_CAPSULE_RADIUS_SCALE, GLOW_HALF_TURN, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(&capsulePoints[6], GLOWS_CAPSULE_RADIUS_SCALE, 0, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(&capsulePoints[8], GLOWS_CAPSULE_RADIUS_SCALE, 0, GLOWS_CAPSULE_COLOR_FACTORS);
            break;
        }
        case 4: {
            const SVECTOR* discPoint = D_shelter_1f_parking_garage_80180C4C;
            _glowDrawFactorDisc(discPoint, GLOWS_DISC_RADIUS_SCALE, GLOWS_DISC_COLOR_FACTORS);
            // The image addresses both preceding capsule pairs from this shared disc base.
            _glowDrawAngledCapsule(&discPoint[-12], GLOWS_CAPSULE_RADIUS_SCALE, GLOW_HALF_TURN, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(&discPoint[-6], GLOWS_CAPSULE_RADIUS_SCALE, 0, GLOWS_CAPSULE_COLOR_FACTORS);
            break;
        }
    }
}

#include "../../shared/glow_draw_angled_capsule.inc.c"

#include "../../shared/glow_draw_factor_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelter1fParkingGarageRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelter1fParkingGarageRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void shelter1fParkingGarageRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
