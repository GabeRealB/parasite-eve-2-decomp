#include "rooms/shelter_b2_operating_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
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

// The flag symbol carries seven unproven bytes after the flag.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
// The request symbol carries twelve unproven bytes after the request.
#define ROOM_EVENT_REQ gRoomEventReq.request
#include "../../shared/room_events.h"

#define D_shelter_b2_operating_room_80180ABC (D_shelter_b2_operating_room_801809BC + 32)
#define D_shelter_b2_operating_room_80180ADC (D_shelter_b2_operating_room_801809BC + 36)
#define D_shelter_b2_operating_room_80180B44 (D_shelter_b2_operating_room_801809BC + 49)
#define D_shelter_b2_operating_room_80180B5C (D_shelter_b2_operating_room_801809BC + 52)
#define D_shelter_b2_operating_room_80180B6C (D_shelter_b2_operating_room_801809BC + 54)

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b2_operating_room_80184234[4];

/// Spawn descriptor of the exit gate's transition task.
extern TaskDesc gRoomEventTaskDesc;

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_shelter_b2_operating_room_80180910;

/// The room's message table, which the cap scripts index.
extern TaskMessageEntry D_shelter_b2_operating_room_8018091C[];

/// Point pairs and points the view task draws its glows at, per view.

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and request of the exit the gate last accepted, latched for
/// the transition task, and the flag saying the gate spawned it.
extern RoomEventMsg          gRoomEventMsg;
extern RoomEventStartStorage gRoomEventActive;
extern RoomEventReqStorage   gRoomEventReq;

/// The message and event the message handler latched for the room's event
/// task, and the flag saying the handler spawned it.
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

// Indexed views below share one contiguous table.
extern TaskDesc D_actor_207000_801575F0;

s32        func_shelter_b2_operating_room_8017DA94(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB2OperatingRoomIgnoreKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_shelter_b2_operating_room_8017DCA4(Task*, s32, s32, s32);
static s32 _shelterB2OperatingRoomIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* action, s32 unusedArg);

/// Inventory key-item message received by this room's task.
enum { SHELTER_B2_OPERATING_ROOM_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b2_operating_room_80180910 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_operating_room_8018091C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_operating_room_8017DA94 },
    { SHELTER_B2_OPERATING_ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2OperatingRoomIgnoreKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2OperatingRoomIgnoreAction },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_operating_room_8017DCA4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b2_operating_room_80180944[15] = {
    { 4950, -1750, 2570, 0 },
    { 5170, -1750, 2570, 0 },
    { 5340, -1750, 2700, 0 },
    { 5410, -1750, 2900, 0 },
    { 5340, -1750, 3100, 0 },
    { 5170, -1750, 3230, 0 },
    { 4950, -1750, 3230, 0 },
    { 4770, -1750, 3100, 0 },
    { 4690, -1750, 2900, 0 },
    { 4770, -1750, 2700, 0 },
    { 4900, -1750, 2900, 0 },
    { 5200, -1750, 2900, 0 },
    { 3360, 200, 3525, 0 },
    { 3880, 200, 3525, 0 },
    { 4600, 200, 3525, 0 },
};

SVECTOR D_shelter_b2_operating_room_801809BC[64] = {
    { 5235, 200, 3525, 0 },
    { 5855, 200, 3525, 0 },
    { 6475, 200, 3525, 0 },
    { 7100, 200, 3525, 0 },
    { 3360, 200, 2925, 0 },
    { 3880, 200, 2925, 0 },
    { 4600, 200, 2925, 0 },
    { 5235, 200, 2925, 0 },
    { 5855, 200, 2925, 0 },
    { 6475, 200, 2925, 0 },
    { 7100, 200, 2925, 0 },
    { 3360, 200, 2310, 0 },
    { 3880, 200, 2310, 0 },
    { 4600, 200, 2310, 0 },
    { 5235, 200, 2310, 0 },
    { 5855, 200, 2310, 0 },
    { 6475, 200, 2310, 0 },
    { 7100, 200, 2310, 0 },
    { 3885, -2245, 4640, 0 },
    { 4865, -2245, 4640, 0 },
    { 5845, -2245, 4640, 0 },
    { 6825, -2245, 4640, 0 },
    { 3885, -2245, 960, 0 },
    { 4865, -2245, 960, 0 },
    { 5845, -2245, 960, 0 },
    { 6825, -2245, 960, 0 },
    { 2560, -2245, 3670, 0 },
    { 2560, -2245, 2800, 0 },
    { 2560, -2245, 1930, 0 },
    { 8140, -2245, 3670, 0 },
    { 8140, -2245, 2800, 0 },
    { 8140, -2245, 1930, 0 },
    { -150, -1820, 5300, 0 },
    { 460, -1820, 5300, 0 },
    { 360, -2245, 3825, 0 },
    { 720, -2245, 3825, 0 },
    { 360, -2245, 1345, 0 },
    { 720, -2245, 1345, 0 },
    { 390, -2025, 295, 0 },
    { 690, -2025, 295, 0 },
    { 1690, -2130, 1810, 0 },
    { 1690, -2130, 1425, 0 },
    { 2310, -1980, 1625, 0 },
    { 2310, -1980, 1390, 0 },
    { 1875, -1825, 1090, 0 },
    { 2115, -1825, 1090, 0 },
    { 2980, -1765, 335, 0 },
    { 4365, -1765, 335, 0 },
    { 2980, -995, 335, 0 },
    { 3672, -1380, 335, 0 },
    { 8560, -1980, 2310, 0 },
    { 8560, -1980, 1925, 0 },
    { 9990, -2300, 5940, 0 },
    { 10375, -2300, 5940, 0 },
    { 10145, -2300, 45, 0 },
    { 10525, -2300, 45, 0 },
    { 8915, -2720, 4090, 0 },
    { 8915, -2720, 3605, 0 },
    { 8915, -2720, 2125, 0 },
    { 8915, -2720, 1640, 0 },
    { 11600, -2720, 4090, 0 },
    { 11600, -2720, 3605, 0 },
    { 11600, -2720, 2125, 0 },
    { 11600, -2720, 1640, 0 },
};

#include "../../shared/room_visual_effects_disc_data.inc.c"

u8* D_shelter_b2_operating_room_80180BC8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_operating_room_80180BCC[1] = { 7 };

DirectionWarpEntry D_shelter_b2_operating_room_80180BD0[3] = {
    { { { .word = 0 }, 0x27D8, 0, 400 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x27D8, 0, 400 }, { 0, 0, 0, 0 }, 0x541D0002, 0x541D0001, 0x541D0008, 2, DIRECTION_WARP_FLAG_NONE, 456 },
    { { { .word = 2048 }, 0x27D8, 0, 5600 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 0x27D8, 0, 5600 }, { 0, 0, 0, 0 }, 0x541D0004, 0x541D0003, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, 458 },
    { { { .word = 0 }, 496, 0, 1017 }, { 0, 0, 0, 0 }, { { .word = 0 }, 496, 0, 1017 }, { 0, 0, 0, 0 }, 0x541D0006, 0x541D0005, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB2OperatingRoomCollision03DA4Normals[18] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_normals.inc"
};

static SVECTOR _gShelterB2OperatingRoomCollision03DA4Verts[99] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_verts.inc"
};

static WorldCollisionGridFace _gShelterB2OperatingRoomCollision03DA4Faces[42] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_faces.inc"
};

static s16 _gShelterB2OperatingRoomCollision03DA4Cells[150] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2OperatingRoomCollision03DA4Cells[i])
static s16* _gShelterB2OperatingRoomCollision03DA4Table[8] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_operating_room_80181364 = { NULL, _gShelterB2OperatingRoomCollision03DA4Normals, _gShelterB2OperatingRoomCollision03DA4Verts, _gShelterB2OperatingRoomCollision03DA4Faces, _gShelterB2OperatingRoomCollision03DA4Table, 1000, 0, 4, 2, 4000, 42 };

ViewCamera D_shelter_b2_operating_room_80181388[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5500, 0x5208, -3500 } }, 380 },
    { { { { -4023, 0, -767 }, { 132, 4034, -694 }, { 755, -707, -3963 } }, { -9754, 492, -5270 } }, 230 },
    { { { { 3968, 0, -1013 }, { 80, 4083, 313 }, { 1010, -324, 3956 } }, { -9594, 639, -681 } }, 230 },
    { { { { 535, 0, 4060 }, { 2019, 3553, -266 }, { -3523, 2037, 464 } }, { -8388, 2849, -2161 } }, 257 },
    { { { { 698, 0, -4036 }, { -1979, 3569, -342 }, { 3517, 2008, 608 } }, { -2193, 2885, -2139 } }, 257 },
    { { { { 3937, 0, -1129 }, { -81, 4085, -282 }, { 1126, 293, 3926 } }, { -109, 1083, -705 } }, 230 },
    { { { { -3937, 0, -1129 }, { -62, 4089, 219 }, { 1127, 228, -3931 } }, { -342, 1051, -4981 } }, 230 },
    { { { { 108, 0, 4094 }, { 3896, 1258, -103 }, { -1257, 3897, 33 } }, { -5020, 1757, -3014 } }, 230 },
    { { { { 4095, 0, 0 }, { 0, 4017, -796 }, { 0, 796, 4017 } }, { -4888, 1408, -1622 } }, 261 },
    { { { { 15, 0, -4095 }, { 3773, 1592, 14 }, { 1592, -3773, 5 } }, { -4755, 952, -2617 } }, 257 },
    { { { { 3327, 0, 2388 }, { 304, 4062, -423 }, { -2368, 521, 3300 } }, { -7528, 1726, -67 } }, 415 },
    { { { { 16, 0, 4095 }, { 3674, 1809, -14 }, { -1809, 3674, 7 } }, { -5788, 2908, -3077 } }, 264 },
};

SpriteBatch D_shelter_b2_operating_room_80181538[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_operating_room_80181548[25] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 429, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 88, 496, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 88, 470, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 72, 492, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 16, 503, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 136, -56, 523, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, -120, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -80, 713, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -80, 694, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 104, -40, 959, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, 32, 918, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 64, 881, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 64, 881, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 80, -120, 971, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -120, 1100, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, -40, 981, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, -40, 1228, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 1167, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -120, 674, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -72, 744, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, -64, 797, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, -56, 876, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -48, 960, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -120, 783, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 104, -120, 879, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_operating_room_8018173C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_operating_room_8018175C[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 88, 509, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -96, -120, 1054, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -112, -120, 1084, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 40, 1062, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -112, -16, 1115, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -128, -16, 831, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -120, -16, 920, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -120, -120, 923, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -128, -120, 831, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -136, -16, 771, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -144, -16, 704, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -152, -16, 629, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -160, -16, 556, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -136, -120, 771, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -144, -120, 704, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -152, -120, 629, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -160, -120, 556, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_operating_room_801818B0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_operating_room_801818C8[87] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 40, 1121, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, -24, 1117, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 0, -8, 1036, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 0, 8, 932, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 8, 24, 850, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 16, 40, 776, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 56, 769, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 48, 784, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 72, 809, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 88, 809, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 56, 776, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 56, 839, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 40, 1033, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 24, 1170, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 1137, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 96, 8, 800, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 32, 834, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 775, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 56, 917, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 72, 72, 896, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 64, 88, 878, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, 88, 866, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 24, 1197, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, -8, 1077, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, 0, 1036, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -24, 8, 945, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 975, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, 40, 1024, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 24, 1009, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 8, 791, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, 8, 775, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, 16, 758, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 24, 694, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, 56, 838, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, 88, 867, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 88, 845, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -72, 1592, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -88, 1592, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -88, 1542, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -96, 1542, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -32, 1527, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -8, 1527, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, -8, 1527, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -32, 1527, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 16, -32, 1199, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 48, -32, 1043, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, -16, 990, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -16, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 40, -16, 1077, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 0, 1020, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, 8, 1175, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 24, 1130, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 64, 804, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, 80, 814, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 80, 842, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 80, 850, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 477, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -72, 521, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 40, -32, 555, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -8, 592, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, -40, 625, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -48, -40, 746, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, -72, 793, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -64, 738, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 827, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, -80, 826, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 16, -56, 730, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -56, 741, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -16, -32, 1239, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -8, 1467, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 56, -120, 1616, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -8, -120, 1515, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, -120, 1490, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -120, -64, 1550, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -136, -120, 1462, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, -64, 1612, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, -40, 1611, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 56, -72, 1774, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 16, -72, 1707, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, -96, 1542, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -96, 1590, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, -32, 1689, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -72, 1595, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -48, -48, 1657, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -48, -96, 1565, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -48, -120, 1517, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 88, 48 } }, -16, -112, 1621, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_b2_operating_room_80181F94[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 7, 0 } },
    { 15, 7, 0, 0, { 2, 0 } },
    { 22, 7, 0, 0, { 6, 0 } },
    { 29, 7, 0, 0, { 1, 0 } },
    { 36, 8, 0, 0, { 9, 0 } },
    { 44, 1, 0, 0, { 0, 0 } },
    { 45, 7, 0, 0, { 10, 0 } },
    { 52, 4, 0, 0, { 5, 0 } },
    { 56, 12, 0, 0, { 8, 0 } },
    { 68, 1, 0, 0, { 4, 0 } },
    { 69, 17, 0, 0, { 11, 0 } },
    { 86, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_operating_room_80182004[124] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 952, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 56, 749, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -64, 48, 752, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -64, 40, 804, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 32, 857, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 24, 888, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -48, 16, 937, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 8, 984, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 0, 1037, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, -8, 1096, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -8, 1119, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 0, 24, 901, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, 48, 928, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 40, 1040, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 964, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -56, 64, 857, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 645, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 72, 674, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 64, 693, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 64, 707, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 64, 708, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 942, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -8, 64, 727, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 88, 730, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -24, 96, 852, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 758, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 80, 750, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -16, 56, 762, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 48, 797, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 40, 844, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 32, 871, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 24, 922, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, 16, 955, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 24, 24, 929, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 24, 40, 966, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 24, 56, 1007, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, 72, 811, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, 88, 846, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, 72, 812, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, 88, 881, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 920, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, 0, 881, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 8, 836, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -104, 16, 824, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 24, 856, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, 16, 911, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 929, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, 8, 875, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, 24, 874, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -104, 40, 873, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 64, 895, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 40, 882, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 64, 885, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 831, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 835, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 1086, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -24, 1105, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 1166, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -8, 1146, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 0, 1064, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 0, 1111, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 16, 1200, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 24, 1206, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, 8, 1134, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 8, 1138, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 32, 1178, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -32, 1079, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -32, 1041, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -24, 964, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 8, 1120, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 24, 1171, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 56, 742, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 48, 739, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 64, 687, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 798, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 40, 768, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 56, 739, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 781, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 56, 860, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 72, 707, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 72, 712, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 80, 690, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -96, 104, 827, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -104, 88, 753, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 96, -16, 1283, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, 0, 1324, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, -8, 1181, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 8, 1197, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 712, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -48, 675, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -48, -40, 669, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -40, 633, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 709, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -48, 749, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, -48, 851, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -32, 851, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, -64, 844, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -56, 775, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -56, 713, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -32, -56, 710, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -80, 968, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -56, 770, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, -64, 862, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, -64, 805, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, -80, 993, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -120, 1013, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 1639, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 24, -120, 1494, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, -72, 1548, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, -72, 1490, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, -48, 1591, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -48, 1557, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -96, -72, 1609, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -104, -96, 1572, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -104, -120, 1503, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -40, -96, 1540, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -40, -120, 1506, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -40, -72, 1596, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -40, -48, 1628, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -96, -48, 1688, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, -120, 1476, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -120, 1420, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, -96, 1502, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 120, -96, 1460, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_operating_room_801829B4[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 7, 0 } },
    { 16, 11, 0, 0, { 2, 0 } },
    { 27, 13, 0, 0, { 6, 0 } },
    { 40, 15, 0, 0, { 1, 0 } },
    { 55, 9, 0, 0, { 9, 0 } },
    { 64, 1, 0, 0, { 0, 0 } },
    { 65, 6, 0, 0, { 10, 0 } },
    { 71, 13, 0, 0, { 5, 0 } },
    { 84, 2, 0, 0, { 8, 0 } },
    { 86, 2, 0, 0, { 4, 0 } },
    { 88, 18, 0, 0, { 11, 0 } },
    { 106, 18, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_operating_room_80182A24[40] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -80, 1041, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -24, 664, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 72, -104, 725, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, -112, 675, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 56, -96, 766, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, -88, 837, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -80, 923, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -120, 619, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -120, 669, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -120, 740, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -120, 847, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -120, 983, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 16, -120, 1120, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 24, -16, 1056, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 40, -16, 915, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -16, 824, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 72, -16, 741, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, -120, 640, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 88, -16, 658, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 104, -120, 609, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 104, -8, 604, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 120, -120, 568, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 120, 0, 558, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, -120, 521, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, 0, 510, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, -40, 730, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -40, 733, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -32, 756, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 40, -72, 1056, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -72, 1025, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 24, 792, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 40, 781, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 40, 794, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 812, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 8, 810, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 8, 873, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 88, -96, 692, { .fields = { 16, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, 72, -96, 766, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, 56, -88, 858, { .fields = { 16, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 72 } }, 48, -80, 944, { .fields = { 120, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_b2_operating_room_80182D44[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 1, 0 } },
    { 30, 6, 0, 0, { 2, 0 } },
    { 36, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_operating_room_80182D6C[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -120, 607, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -16, 625, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 619, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -16, 632, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, -120, 604, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, -120, 655, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -120, -112, 641, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, -104, 654, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, -112, 673, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, -104, 681, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, -88, -96, 675, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -40, 506, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -112, -96, 657, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -160, -8, 428, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -136, -8, 494, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -112, -8, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -88, -8, 645, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -8, 713, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -64, -120, 766, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -8, 775, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -8, 816, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -16, 479, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, -16, 463, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -64, 820, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -72, 838, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 859, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -40, -120, 897, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, -72, 880, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, -64, 1003, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -120, 962, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 0, -40, 1114, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 0, -120, 1096, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, 8, -112, 1165, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -16, -120, 1092, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -40, 1114, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -40, -56, 1158, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 48, 400, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -112, 72, 387, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -96, 88, 404, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, 104, 395, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 395, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -152, 48, 362, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 72, 368, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 88, 375, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -112, 104, 380, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 470, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 112 } }, -160, -120, 487, { .fields = { 56, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 112 } }, -144, -120, 522, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 112 } }, -128, -120, 563, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 72 } }, -112, -104, 596, { .fields = { 120, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_b2_operating_room_80183154[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 3, 0 } },
    { 26, 10, 0, 0, { 0, 0 } },
    { 36, 10, 0, 0, { 2, 0 } },
    { 46, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_operating_room_80183184[7] = {
    { { .empty = D_shelter_b2_operating_room_80181538 }, D_shelter_b2_operating_room_80181538, NULL },
    { { .elements = D_shelter_b2_operating_room_80181548 }, D_shelter_b2_operating_room_8018173C, NULL },
    { { .elements = D_shelter_b2_operating_room_8018175C }, D_shelter_b2_operating_room_801818B0, NULL },
    { { .elements = D_shelter_b2_operating_room_801818C8 }, D_shelter_b2_operating_room_80181F94, NULL },
    { { .elements = D_shelter_b2_operating_room_80182004 }, D_shelter_b2_operating_room_801829B4, NULL },
    { { .elements = D_shelter_b2_operating_room_80182A24 }, D_shelter_b2_operating_room_80182D44, NULL },
    { { .elements = D_shelter_b2_operating_room_80182D6C }, D_shelter_b2_operating_room_80183154, NULL },
};

WorldCoordPointLight D_shelter_b2_operating_room_801831D8[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1560, -1747, 1797 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 4014, 3932 }, { 0, 0 } }, 601, 1260 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2800, -1615, 1827 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 3686 }, { 0, 0 } }, 0, 2161 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -1705, 4987 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3932, 3932 }, { 0, 0 } }, 500, 1300 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 540, -1731, 1487 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 500, 2321 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6454, 567, 2999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 1722, 2522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3997, 567, 2999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 1823, 2643 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2800, -1534, 4217 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 3686 }, { 0, 0 } }, 0, 2103 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9030, -1847, 2448 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 4014, 3932 }, { 0, 0 } }, 701, 1199 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x27C4, -1726, 5417 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3493, 3826, 4003 }, { 0, 0 } }, 681, 1481 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2878, -1847, 548 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2736, 3442, 3506 }, { 0, 0 } }, 781, 1459 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6730, -1579, 2920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 163 }, { 0, 0 } }, 0, 2582 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3860, -1258, 2980 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 163 }, { 0, 0 } }, 0, 3162 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4541, -1560, 575 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 901, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 600, -1731, 3937 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 500, 2021 },
};

WorldCoordRoomLights D_shelter_b2_operating_room_80183718 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_operating_room_801831D8), D_shelter_b2_operating_room_801831D8, 0, NULL };

WorldCollisionTrigger D_shelter_b2_operating_room_80183730[10] = {
    { NULL, NULL, NULL, { 2075, -1440, 1530, 0 }, { { 16, -1936, 1216, 0 }, { -16, -1936, -1215, 0 }, { 16, 1937, 1216, 0 }, { -16, 1937, -1215, 0 } }, { -4112, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1916, -1408, 1628, 0 }, { { -16, -1936, -1312, 0 }, { 16, -1936, 1312, 0 }, { -16, 1937, -1312, 0 }, { 16, 1937, 1312, 0 } }, { 4115, 0, -52, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8800, -1472, 2080, 0 }, { { 16, -1936, 1216, 0 }, { -16, -1936, -1215, 0 }, { 16, 1937, 1216, 0 }, { -16, 1937, -1215, 0 } }, { -4112, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8673, -1440, 2112, 0 }, { { -16, -1936, -1312, 0 }, { 16, -1936, 1312, 0 }, { -16, 1937, -1312, 0 }, { 16, 1937, 1312, 0 } }, { 4115, 0, -52, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5055, -1473, 3102, 0 }, { { 159, -1936, -3548, 0 }, { -158, -1936, 3548, 0 }, { 159, 1937, -3548, 0 }, { -158, 1937, 3548, 0 } }, { 4091, 0, 182, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5280, -1505, 2960, 0 }, { { -152, -1936, 3370, 0 }, { 147, -1936, -3374, 0 }, { -152, 1937, 3370, 0 }, { 147, 1937, -3374, 0 } }, { -4097, 0, -182, 0 }, { 0, 0, 4096, 0 }, 3890, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 464, -1473, 2736, 0 }, { { -1467, -1936, -130, 0 }, { 1465, -1936, 129, 0 }, { -1467, 1937, -130, 0 }, { 1465, 1937, 129, 0 } }, { 359, 0, -4084, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 512, -1441, 2625, 0 }, { { 1398, -1936, 152, 0 }, { -1402, -1936, -155, 0 }, { 1398, 1937, 152, 0 }, { -1402, 1937, -155, 0 } }, { -449, 0, 4075, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28E1, -1504, 3072, 0 }, { { 1398, -1936, -127, 0 }, { -1407, -1936, 119, 0 }, { 1398, 1937, -127, 0 }, { -1407, 1937, 119, 0 } }, { 357, 0, 4082, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28C0, -1408, 3200, 0 }, { { -1473, -1936, 84, 0 }, { 1465, -1936, -92, 0 }, { -1473, 1937, 84, 0 }, { 1465, 1937, -92, 0 } }, { -246, 0, -4093, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b2_operating_room_80183A28[3] = {
    { NULL, NULL, { 2144, -1648, 3968, 0 }, { { 0, 2672, 1920, 0 }, { 0, 2672, -1920, 0 }, { 0, -2672, 1920, 0 }, { 0, -2672, -1920, 0 } }, { 4110, 0, 0, 0 }, 3288, 1, 0 },
    { NULL, NULL, { 8576, -1504, 4640, 0 }, { { 0, 2528, 1840, 0 }, { 0, 2528, -1840, 0 }, { 0, -2528, 1840, 0 }, { 0, -2528, -1840, 0 } }, { 4113, 0, 0, 0 }, 3124, 1, 0 },
    { NULL, NULL, { 8576, 0, 400, 0 }, { { 0, 2672, 864, 0 }, { 0, 2672, -864, 0 }, { 0, -2672, 864, 0 }, { 0, -2672, -864, 0 } }, { 4109, 0, 0, 0 }, 2804, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_operating_room_80183ADC[13] = {
    { NULL, NULL, NULL, { 0x2810, -48, 464, 0 }, { { -720, 0, -432, 0 }, { 720, 0, -432, 0 }, { -720, 0, 432, 0 }, { 720, 0, 432, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 839, WORLD_COLLISION_TRIGGER_ACTION_WARP, 28, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2820, -48, 5568, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_WARP, 30, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 528, -48, 864, 0 }, { { -720, 0, -432, 0 }, { 720, 0, -432, 0 }, { -720, 0, 432, 0 }, { 720, 0, 432, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 839, WORLD_COLLISION_TRIGGER_ACTION_WARP, 31, 50, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7552, -64, 4288, 0 }, { { -416, 0, -688, 0 }, { 416, 0, -688, 0 }, { -416, 0, 688, 0 }, { 416, 0, 688, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1376, -64, 2544, 0 }, { { -416, 0, -448, 0 }, { 416, 0, -448, 0 }, { -416, 0, 448, 0 }, { 416, 0, 448, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 610, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4336, -64, 896, 0 }, { { -816, 0, -368, 0 }, { 816, 0, -368, 0 }, { -816, 0, 368, 0 }, { 816, 0, 368, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5216, -64, 3040, 0 }, { { -1936, 0, -1456, 0 }, { 1936, 0, -1456, 0 }, { -1936, 0, 1456, 0 }, { 1936, 0, 1456, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 2415, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2960, -64, 4592, 0 }, { { -784, 0, -416, 0 }, { 784, 0, -416, 0 }, { -784, 0, 416, 0 }, { 784, 0, 416, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 886, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -64, 2912, 0 }, { { -400, 0, -1296, 0 }, { 400, 0, -1296, 0 }, { -400, 0, 1296, 0 }, { 400, 0, 1296, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 320, -64, 4784, 0 }, { { -720, 0, -448, 0 }, { 720, 0, -448, 0 }, { -720, 0, 448, 0 }, { 720, 0, 448, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3103, -64, 1175, 0 }, { { -738, 0, -192, 0 }, { 521, 0, -871, 0 }, { -744, 0, 952, 0 }, { 963, 0, 113, 0 } }, { 0, 4105, 0, 0 }, { 2896, 0, 2896, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6928, -64, 4608, 0 }, { { -1024, 0, -448, 0 }, { 1024, 0, -448, 0 }, { -1024, 0, 448, 0 }, { 1024, 0, 448, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7847, -64, 1343, 0 }, { { -763, 0, -1005, 0 }, { 787, 0, -200, 0 }, { -802, 0, 233, 0 }, { 780, 0, 974, 0 } }, { 0, 4098, 0, 0 }, { -1931, 0, 3612, 0 }, 1260, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_operating_room_80183EB8[5] = {
    { 70, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_8013F5F0 },
    { 71, 71, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_80139E60 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_8014E7A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_operating_room_80183EF4[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_operating_room_80183F0C[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_operating_room_80183F24[4] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { 70, 70, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207000_801575F0 },
    { 71, 71, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207000_80151E60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_operating_room_80183F54[7] = {
    { 70, 0, 0, 5250, 0, 900, 800, 0, 0, 2, 0 },
    { 70, 0, 0, 0, 0, 2600, 2300, 0, 0, 2, 0 },
    { 72, 0, 0, 4900, 0, 4400, 1200, 0, 2, 4, 0 },
    { 72, 0, 0, 5500, 0, 1500, 1024, 0, 2, 4, 0 },
    { 72, 0, 0, 3700, 0, 3000, 2048, 0, 2, 4, 0 },
    { 73, 0, 1, 0x2710, 0, 3000, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_operating_room_80183FC4[5] = {
    { 24, 0, 0, 3100, 0, 4300, 1024, 0, 0, 2, 0 },
    { 24, 0, 0, 7200, 0, 4300, 2048, 0, 0, 2, 0 },
    { 24, 0, 0, 3100, 0, 1600, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 7200, 0, 1600, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_operating_room_80184014[8] = {
    { 26, 0, 1, 7800, 0, 1950, 1150, 0, 0, 2, 0 },
    { 26, 0, 1, 7900, 0, 2250, 950, 0, 0, 2, 0 },
    { 26, 0, 1, 500, 0, 4500, 1950, 0, 0, 2, 0 },
    { 26, 0, 0, 6700, 0, 4300, 1850, 0, 0, 2, 0 },
    { 26, 0, 0, 5400, 0, 1400, 500, 0, 0, 2, 0 },
    { 26, 0, 0, 3600, 0, 4100, 0, 0, 0, 2, 0 },
    { 26, 0, 0, 400, 0, 2900, 1700, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_operating_room_80184094[9] = {
    { 24, 0, 0, 5700, 0, 1900, 800, 0, 0, 2, 0 },
    { 24, 0, 0, 3650, 0, 1500, 400, 0, 0, 2, 0 },
    { 24, 0, 0, 4950, 0, 4700, 2650, 0, 0, 2, 0 },
    { 24, 0, 0, 800, 0, 3400, 1950, 0, 0, 2, 0 },
    { 70, 0, 0, 5250, 0, 900, 800, 0, 2, 4, 0 },
    { 70, 0, 0, 0, 0, 2450, 1800, 0, 2, 4, 0 },
    { 70, 0, 0, 0x2904, 0, 3300, 2800, 0, 2, 4, 0 },
    { 71, 0, 1, 9200, 0, 2200, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_operating_room_80184124[12] = {
    { NULL, NULL },
    { D_shelter_b2_operating_room_80183F54, D_shelter_b2_operating_room_80183EB8 },
    { D_shelter_b2_operating_room_80183FC4, D_shelter_b2_operating_room_80183EF4 },
    { D_shelter_b2_operating_room_80184014, D_shelter_b2_operating_room_80183F0C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_operating_room_80184094, D_shelter_b2_operating_room_80183F24 },
};

WorldCoordRoomAmbientEntry D_shelter_b2_operating_room_80184184[8] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b2_operating_room_80184184) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 490, 520, 560, 513 } },
    { .color = { 400, 438, 400, 419 } },
    { .color = { 380, 758, 617, 598 } },
    { .color = { 641, 701, 691, 677 } },
    { .color = { 277, 257, 260, 264 } },
    { .color = { 620, 630, 690, 633 } },
};

WorldCollisionFootstepSounds D_shelter_b2_operating_room_801841C4 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionFootstepSounds D_shelter_b2_operating_room_801841D0 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b2_operating_room_801841DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_operating_room_801841E4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_operating_room_801841C4 },
};

WorldCollisionSurfaceProperties D_shelter_b2_operating_room_801841EC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_operating_room_801841D0 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_operating_room_801841F4[8] = {
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841E4,
    D_shelter_b2_operating_room_801841EC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventStartStorage gRoomEventActive = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b2_operating_room_80184234[4] = {
    0,
    37,
    9,
    8,
};

RoomEventReqStorage gRoomEventReq;

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 _shelterB2OperatingRoomStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event);
static void           _shelterB2OperatingRoomInitTask(Task* task);
static void           _shelterB2OperatingRoomIdleTask(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Latches a first-use room departure event, or tests its eligibility for a query.
///
/// Borrows complete eight-byte `destination` and twelve-byte `event` records
/// for this call. A zero flag ID stays eligible; any other ID must name a
/// valid game-flag nibble. Returns 1 when that nibble is already nonzero,
/// allowing the ordinary departure, or 2 when the staged event is eligible.
/// Executing an eligible request copies both records into room-owned storage,
/// sets a nonzero flag's nibble to 1 and attempts to spawn the staged task.
/// Every call clears the room's event-started byte; execution raises it even
/// if spawning fails. The room overlay must remain loaded through the event.
static __inline__ s32 _shelterB2OperatingRoomStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event)
{
    enum {
        SHELTER_B2_OPERATING_ROOM_EVENT_NO_FLAG            = 0,
        SHELTER_B2_OPERATING_ROOM_EVENT_FLAG_LATCHED       = 1,
        SHELTER_B2_OPERATING_ROOM_EVENT_ORDINARY_DEPARTURE = 1,
        SHELTER_B2_OPERATING_ROOM_EVENT_STAGED_DEPARTURE   = 2,
    };

    D_shelter_b2_operating_room_80184234[0] = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == SHELTER_B2_OPERATING_ROOM_EVENT_NO_FLAG) {
        if (destination->queryOnly == ROOM_EVENT_EXECUTE) {
            // Own both records before the staged task can use the caller's stack data.
            gRoomEventStagedMsg = *destination;
            gRoomEventLatched   = *event;
            if (event->flagId != SHELTER_B2_OPERATING_ROOM_EVENT_NO_FLAG) {
                gameFlagSetNibble(event->flagId, SHELTER_B2_OPERATING_ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&D_shelter_b2_operating_room_80180910, 0, 0, 0);
            D_shelter_b2_operating_room_80184234[0] = 1;
        }
        return SHELTER_B2_OPERATING_ROOM_EVENT_STAGED_DEPARTURE;
    }
    return SHELTER_B2_OPERATING_ROOM_EVENT_ORDINARY_DEPARTURE;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `mapShelterRoomVariantResolve`. Message 0x1E goes through the exit gate
/// `roomEventGate` on flag 0xA8. Message 0x1C, while nibble 0xAA is clear, answers 0 and - unless
/// `in->queryOnly` asks for a dry run - passes `in->flagId` to `gameFlagSetNibbleIfPresent`
/// and runs cap command 3; once the nibble is set it starts the room event on
/// flag 0x13A instead. Message 0x1F starts the event on flag 0x13B; any other
/// message answers 1.
s32 func_shelter_b2_operating_room_8017DA94(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY) {
        req.capCmd        = 2;
        req.missingCapCmd = 1;
        req.firstSnd      = 0x541D0007;
        req.secondSnd     = 0x541D0003;
        req.flagId        = GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED;
        req.collectedBit  = 0;
        return roomEventGate(&req, out);
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY && gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_SOUTH_DOOR_UNLOCKED) == 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(in->flagId, 2);
            capRunCommandWithTransition(3);
        }
        return 0;
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY) {
        event.capCmd   = 0xE;
        event.stageSnd = 0x541D0001;
        event.flagId   = GAME_FLAG_B2_OPERATING_TO_SOUTH_WALKWAY_SCENE;
        event.fade     = 0;
        return _shelterB2OperatingRoomStartEvent(out, &event);
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_LABORATORY) {
        event.capCmd   = 0xD;
        event.stageSnd = 0x541D0005;
        event.flagId   = GAME_FLAG_B2_OPERATING_TO_LAB_SCENE;
        event.fade     = 0;
        return _shelterB2OperatingRoomStartEvent(out, &event);
    }
    return 1;
}

/// Refuses key-item use without consuming the item or changing room state.
///
/// `itemId` is the collected-item ID from the inventory; all inputs are ignored.
/// Returns zero, which the inventory presents as an unusable item.
static s32 _shelterB2OperatingRoomIgnoreKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { SHELTER_B2_OPERATING_ROOM_KEY_ITEM_UNUSED = 0 };

    return SHELTER_B2_OPERATING_ROOM_KEY_ITEM_UNUSED;
}

s32 func_shelter_b2_operating_room_8017DCA4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == 0 ? 4 : 0x10, CAP_EVENT_NO_FLAGS);
            break;
        case 5:
            capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0 ? 0xF : 5, CAP_EVENT_NO_FLAGS);
            break;
    }
    return 0;
}

/// Ignores a direction trigger's room-action request and returns zero.
///
/// The sender borrows a `DirectionActionRequest` for synchronous dispatch.
/// This handler does not read it or any other input and performs no action.
static s32 _shelterB2OperatingRoomIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* action, s32 unusedArg)
{
    return 0;
}

/// Publishes the room's task and message handlers, then advances it to idle.
///
/// Called with state zero; the borrowed message table remains live with the
/// room overlay.
static void _shelterB2OperatingRoomInitTask(Task* task)
{
    task->msgTable = D_shelter_b2_operating_room_8018091C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the room task available for messages while idle.
static void _shelterB2OperatingRoomIdleTask(Task* task)
{
}

/// The room task's three states, dispatched by
/// `shelterB2OperatingRoomTask`: install the message table, idle,
/// end.
static const TaskFuncTable3 D_shelter_b2_operating_room_8017D5F0 = {
    { _shelterB2OperatingRoomInitTask, _shelterB2OperatingRoomIdleTask, taskKill }
};

void shelterB2OperatingRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b2_operating_room_8017D5F0;
    states.funcs[task->state](task);
}

/// Binds this room's charge-disc, flying-spark and projectile-burst effect IDs.
///
/// Installs bank-6 task IDs in `gRoomEffectGlowDiscId`,
/// `gRoomEffectFlyingSparkId` and `gRoomEffectOrangeBurst2Id` for the fireball
/// attack. Call during room setup before spawning these effects. The bindings
/// persist until replaced or cleared by the room-effect controller;
/// keep this room overlay loaded while the selected effect tasks can run.
static __inline__ void _shelterB2OperatingRoomBindFireballEffects(void)
{
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B2_OPERATING_ROOM_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B2_OPERATING_ROOM_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B2_OPERATING_ROOM_ORANGE_BURST_2;
}

void shelterB2OperatingRoomDrawGlowsTask(Task* task)
{
    // Radius parameters are world units; colours pack RGB nibbles scaled by 16.
    enum {
        SHELTER_B2_OPERATING_ROOM_GLOW_INITIALIZE,
        SHELTER_B2_OPERATING_ROOM_GLOW_DRAW,
        SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS    = 0x100,
        SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS       = 0x200,
        SHELTER_B2_OPERATING_ROOM_GLOW_LARGE_DISC_RADIUS = 0x380,
        SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR     = 0x444,
        SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR        = 0x433,
        SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR         = 0x400,
    };

    if (task->state == SHELTER_B2_OPERATING_ROOM_GLOW_INITIALIZE) {
        _shelterB2OperatingRoomBindFireballEffects();
        task->state = SHELTER_B2_OPERATING_ROOM_GLOW_DRAW;
    }

    // Only views 2..7 have glows; capsules borrow consecutive endpoint pairs.
    switch ((u8)viewGetMappedIndex()) {
        case 2:
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[54], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[58], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[62], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            break;
        case 3:
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[52], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[60], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[49], SHELTER_B2_OPERATING_ROOM_GLOW_LARGE_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[1], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[5], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[8], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[11], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[12], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[13], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[14], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[15], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[18], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[19], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[22], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[23], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[26], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[27], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[28], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[42], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            break;
        case 5:
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[0], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[6], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[8], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[9], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[10], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[13], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[14], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[15], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[16], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[17], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_WARM_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[20], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[21], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[24], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[25], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[29], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[30], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            glowDrawDisc(&D_shelter_b2_operating_room_801809BC[31], SHELTER_B2_OPERATING_ROOM_GLOW_DISC_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_RED_COLOR);
            break;
        case 6:
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[32], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[34], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            break;
        case 7:
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[36], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[38], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[40], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            _glowDrawCapsule(&D_shelter_b2_operating_room_801809BC[44], SHELTER_B2_OPERATING_ROOM_GLOW_CAPSULE_RADIUS, SHELTER_B2_OPERATING_ROOM_GLOW_NEUTRAL_COLOR);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b2_operating_room_8017ECFC(Task* arg0)
{
    _roomVisualEffectsGlowDiscTask(arg0);
}

void shelterB2OperatingRoomRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void shelterB2OperatingRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
