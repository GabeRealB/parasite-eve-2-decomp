#include "rooms/shelter_b1_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
// The flag symbol carries seven unproven bytes after the flag.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
// The request symbol carries twelve unproven bytes after the request.
#define ROOM_EVENT_REQ gRoomEventReq.request
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b1_access_tunnel_8017FF6C[4];

/// Task descriptor `roomEventGate` spawns when a
/// gated event fires.
extern TaskDesc gRoomEventTaskDesc;

/// Task descriptor for the room's event task, spawned when the message handler
/// starts the latched event.
extern TaskDesc D_shelter_b1_access_tunnel_8017E710;

/// Message table `func_shelter_b1_access_tunnel_8017DCBC` installs on its task.
extern TaskMessageEntry D_shelter_b1_access_tunnel_8017E71C[];

extern SVECTOR D_shelter_b1_access_tunnel_8017E744[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7B4[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7D4[];

extern AreaApplyRec    D_shelter_b1_access_tunnel_8017FF44[];
extern RoomFadeStorage gRoomEventFade;

/// Copy of the message that fired a gated event, kept for the task
/// `roomEventTask` to warp from.
extern RoomEventMsg gRoomEventMsg;

/// Set by `roomEventGate` when the event it gates has
/// just fired, clear otherwise.
extern RoomEventStartStorage gRoomEventActive;

extern RoomEventMsg gRoomEventStagedMsg;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `roomEventTask` plays.
extern RoomEventReqStorage gRoomEventReq;

extern RoomLatchedEvent gRoomEventLatched;

static void func_shelter_b1_access_tunnel_8017DCBC(Task* task);
static void func_shelter_b1_access_tunnel_8017DD00(Task* task);

s32 func_shelter_b1_access_tunnel_8017DA68(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_access_tunnel_8017DCA4(Task*, s32, s32, s32);
s32 func_shelter_b1_access_tunnel_8017DCAC(Task*, s32, s32, s32);
s32 func_shelter_b1_access_tunnel_8017DCB4(Task*, s32, s32, s32);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b1_access_tunnel_8017E710 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_access_tunnel_8017E71C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_access_tunnel_8017DA68 },
    { 5105, func_shelter_b1_access_tunnel_8017DCA4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_access_tunnel_8017DCB4 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_access_tunnel_8017DCAC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b1_access_tunnel_8017E744[14] = {
    { 0x2E22, -2650, 7090, 0 },
    { 0x2E22, -2240, 7090, 0 },
    { 0x2E22, -1150, 7090, 0 },
    { 0x2E22, -740, 7090, 0 },
    { 9920, -2610, 7660, 0 },
    { 9920, -2610, 8340, 0 },
    { 7920, -2610, 7660, 0 },
    { 7920, -2610, 8330, 0 },
    { 5920, -2610, 7660, 0 },
    { 5920, -2610, 8330, 0 },
    { 3920, -2610, 7660, 0 },
    { 3920, -2610, 8330, 0 },
    { 2060, -2610, 7660, 0 },
    { 2060, -2610, 8330, 0 },
};

SVECTOR D_shelter_b1_access_tunnel_8017E7B4[4] = {
    { 9210, -2050, -140, 0 },
    { 9210, -1500, -140, 0 },
    { 0x2A30, -2050, -140, 0 },
    { 0x2A30, -1500, -140, 0 },
};

SVECTOR D_shelter_b1_access_tunnel_8017E7D4[2] = {
    { 1240, -2510, 8940, 0 },
    { 750, -2510, 8940, 0 },
};

u8* D_shelter_b1_access_tunnel_8017E7E4[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_access_tunnel_8017E7E8[1] = { 5 };

DirectionWarpEntry D_shelter_b1_access_tunnel_8017E7EC[4] = {
    { { { .word = 0 }, 9953, 0, 1474 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x288C, 0, 6240 }, { 0, 0, 0, 0 }, 0x54130002, 0x54130001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, 453 },
    { { { .word = 1024 }, 9600, 0, 3180 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x288C, 0, 6240 }, { 0, 0, 0, 0 }, 0x54130008, 0x54130007, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, 433 },
    { { { .word = 2048 }, 549, 0, 8582 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 549, 0, 8582 }, { 0, 0, 0, 0 }, 0x54130006, 0x54130005, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x27F0, 0, 6140 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x288C, 0, 6240 }, { 0, 0, 0, 0 }, 0x54130004, 0x54130003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB1AccessTunnelCollision01564Normals[6] = {
#include "assets/shelter_b1_access_tunnel_collision_01564_normals.inc"
};

static SVECTOR _gShelterB1AccessTunnelCollision01564Verts[24] = {
#include "assets/shelter_b1_access_tunnel_collision_01564_verts.inc"
};

static WorldCollisionGridFace _gShelterB1AccessTunnelCollision01564Faces[15] = {
#include "assets/shelter_b1_access_tunnel_collision_01564_faces.inc"
};

static s16 _gShelterB1AccessTunnelCollision01564Cells[72] = {
#include "assets/shelter_b1_access_tunnel_collision_01564_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1AccessTunnelCollision01564Cells[i])
static s16* _gShelterB1AccessTunnelCollision01564Table[9] = {
#include "assets/shelter_b1_access_tunnel_collision_01564_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_access_tunnel_8017EB24 = { NULL, _gShelterB1AccessTunnelCollision01564Normals, _gShelterB1AccessTunnelCollision01564Verts, _gShelterB1AccessTunnelCollision01564Faces, _gShelterB1AccessTunnelCollision01564Table, -10, -670, 3, 3, 4000, 15 };

ViewCamera D_shelter_b1_access_tunnel_8017EB48[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5000, 0x61A8, -5000 } }, 541 },
    { { { { -1316, 0, -3878 }, { -647, 4038, 219 }, { 3824, 683, -1298 } }, { -5060, 1800, -8530 } }, 289 },
    { { { { -751, 0, -4026 }, { -312, 4083, 58 }, { 4014, 318, -749 } }, { -110, 1580, -8360 } }, 289 },
    { { { { -348, 0, 4081 }, { 275, 4086, 23 }, { -4071, 276, -347 } }, { -7110, 1580, -8400 } }, 289 },
    { { { { -3977, 0, 976 }, { -126, 4061, -515 }, { -968, -530, -3944 } }, { -0x2846, 830, -8490 } }, 289 },
};

SpriteBatch D_shelter_b1_access_tunnel_8017EBFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_access_tunnel_8017EC0C[22] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 700, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 700, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 800, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 64, 72, 800, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 64, 8, 800, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 24, 875, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, -8, 875, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, 72, 875, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, 72, 950, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, 24, 950, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 8, -16, 950, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -16, 1037, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 24, 1037, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 40, -16, 1100, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 32, 1100, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -16, 1050, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 24, 1000, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 48, 1000, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -24, 1000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -72, 1000, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -96, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -120, 1000, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_access_tunnel_8017EDC4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 3, 0, 0, { 2, 0 } },
    { 16, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_access_tunnel_8017EDEC[74] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, -16, 2050, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 1000, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1000, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 16, 1000, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, 72, 1000, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 1000, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 8, 1000, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 40, 24, 1125, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 40, 0, 1125, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 16, 24, 1500, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 24, 72, 1500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 16, -8, 1250, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 0, -16, 1625, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 0, 24, 1625, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -16, 2000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, 24, 2000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -24, -16, 2150, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -16, 2150, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -48, 1800, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -40, 1800, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 0, 48, 1800, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, 8, 8, 1800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 0, -40, 1800, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 40, -40, 1750, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -40, -72, 1800, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 1800, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -48, -104, 1062, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 1050, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -24, 1000, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, 72, 1062, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, 24, 1062, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 32 } }, 24, -56, 1062, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -24, 1062, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 72, -48, 1050, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 120, -48, 1000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 0, -56, 1062, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -24, -80, 1062, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -40, -104, 1062, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -40, -120, 1062, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -120, 1062, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, -80, 850, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -32, 850, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 850, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 112, 72, 850, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 80, 72, 850, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 48, 850, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 8, 850, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 112, 48, 850, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 112, 8, 850, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 112, -32, 850, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 112, -56, 850, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 152, -48, 850, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, 88, -32, 850, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, -32, 850, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 88, -72, 850, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -80, 850, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -96, 850, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 40, -80, 850, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 40, -120, 850, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 16, -120, 850, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 0, -120, 850, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, -32, 1525, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1375, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1375, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1450, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1450, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1450, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 40, -120, 1525, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 40, -72, 1525, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 40, -24, 1525, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 40, 24, 1525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_access_tunnel_8017F3B4[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 2, 0, 0, { 4, 0 } },
    { 18, 0, 0, 0, { 1, 0 } },
    { 18, 8, 0, 0, { 5, 0 } },
    { 26, 14, 0, 0, { 0, 0 } },
    { 40, 21, 0, 0, { 6, 0 } },
    { 61, 13, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_access_tunnel_8017F3FC[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -32, 550, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 550, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -112, -120, 550, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -72, 550, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -104, 1075, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -80, 1075, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -48, 1075, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -128, 72, 1075, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -128, 40, 1075, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 48, 1075, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, 72, 1075, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, 0, 1075, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -40, 1075, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -40, 1075, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -88, -40, 1075, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -88, -72, 1075, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -72, 1075, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -56, -104, 1075, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, -120, 1075, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, -120, 1075, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -72, 1489, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 64, 1550, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 24, 1550, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -16, 1550, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, -40, 1550, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -80, -40, 1550, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -40, 1550, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -64, 1550, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, -64, 1550, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -24, -80, 1550, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -24, -120, 1550, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_access_tunnel_8017F668[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 16, 0, 0, { 2, 0 } },
    { 20, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_access_tunnel_8017F690[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_access_tunnel_8017F6A0[5] = {
    { { .empty = D_shelter_b1_access_tunnel_8017EBFC }, D_shelter_b1_access_tunnel_8017EBFC, NULL },
    { { .elements = D_shelter_b1_access_tunnel_8017EC0C }, D_shelter_b1_access_tunnel_8017EDC4, NULL },
    { { .elements = D_shelter_b1_access_tunnel_8017EDEC }, D_shelter_b1_access_tunnel_8017F3B4, NULL },
    { { .elements = D_shelter_b1_access_tunnel_8017F3FC }, D_shelter_b1_access_tunnel_8017F668, NULL },
    { { .empty = D_shelter_b1_access_tunnel_8017F690 }, D_shelter_b1_access_tunnel_8017F690, NULL },
};

WorldCoordLight D_shelter_b1_access_tunnel_8017F6DC[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_b1_access_tunnel_8017F83C[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4500, -2620, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3200, -2159, -922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -766, -2000, -4618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1240, -5082, 2801 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3966, -2581, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights D_shelter_b1_access_tunnel_8017FA1C = { ARRAY_SIZE(D_shelter_b1_access_tunnel_8017F6DC), D_shelter_b1_access_tunnel_8017F6DC, ARRAY_SIZE(D_shelter_b1_access_tunnel_8017F83C), D_shelter_b1_access_tunnel_8017F83C, 0, NULL };

WorldCollisionTrigger D_shelter_b1_access_tunnel_8017FA34[6] = {
    { NULL, NULL, NULL, { 0x2711, -1536, 5121, 0 }, { { -1936, -2016, 672, 0 }, { 1936, -2016, -672, 0 }, { -1936, 2016, 672, 0 }, { 1936, 2016, -672, 0 } }, { -1344, 0, -3872, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2710, -1536, 4976, 0 }, { { 2080, -2016, -752, 0 }, { -2080, -2016, 752, 0 }, { 2080, 2016, -752, 0 }, { -2080, 2016, 752, 0 } }, { 1395, 0, 3860, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7963, -1536, 8124, 0 }, { { -306, -2016, -1587, 0 }, { 306, -2016, 1588, 0 }, { -306, 2016, -1587, 0 }, { 306, 2016, 1588, 0 } }, { 4028, 0, -778, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8154, -1504, 8075, 0 }, { { 315, -2016, 1501, 0 }, { -315, -2016, -1501, 0 }, { 315, 2016, 1501, 0 }, { -315, 2016, -1501, 0 } }, { -4012, 0, 841, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3328, -1472, 8000, 0 }, { { -155, -2016, -1616, 0 }, { 142, -2016, 1604, 0 }, { -155, 2016, -1616, 0 }, { 142, 2016, 1604, 0 } }, { 4085, 0, -378, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3457, -1504, 8032, 0 }, { { 164, -2016, 1521, 0 }, { -169, -2016, -1528, 0 }, { 164, 2016, 1521, 0 }, { -169, 2016, -1528, 0 } }, { -4074, 0, 443, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_access_tunnel_8017FBFC[4] = {
    { NULL, NULL, NULL, { 9952, -48, 1216, 0 }, { { -1024, 0, -576, 0 }, { 1024, 0, -576, 0 }, { -1024, 0, 576, 0 }, { 1024, 0, 576, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9248, -48, 3472, 0 }, { { 576, 0, -880, 0 }, { 576, 0, 880, 0 }, { -576, 0, -880, 0 }, { -576, 0, 880, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_WARP, 21, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 960, -48, 8624, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2970, -48, 6096, 0 }, { { 368, 0, -624, 0 }, { 368, 0, 624, 0 }, { -368, 0, -624, 0 }, { -368, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 724, WORLD_COLLISION_TRIGGER_ACTION_WARP, 24, 66, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b1_access_tunnel_8017FD2C[1] = {
    { NULL, NULL, { 5152, -1440, 4400, 0 }, { { -3488, 2464, -2192, 0 }, { 3488, 2464, 2192, 0 }, { -3488, -2464, -2192, 0 }, { 3488, -2464, 2192, 0 } }, { -2188, 0, 3480, 0 }, 4775, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_shelter_b1_access_tunnel_8017FD68[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_access_tunnel_8017FD80[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_access_tunnel_8017FD98[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403900_801540E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_access_tunnel_8017FDB0[6] = {
    { 21, 4, 0, 9300, -2200, 700, 0, 0, 0, 2, 7 },
    { 21, 4, 0, 0x2A30, -2200, 700, 0, 0, 0, 2, 7 },
    { 21, 4, 0, 0x2AF8, -2400, 8500, 3072, 0, 0, 2, 7 },
    { 21, 4, 0, 0x2AF8, -2400, 7600, 3072, 0, 0, 2, 7 },
    { 21, 4, 0, 0x2AF8, -2600, 5100, 3072, 0, 0, 2, 7 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_access_tunnel_8017FE10[2] = {
    { 22, 0, 0, 0x2710, 0, 8000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_access_tunnel_8017FE30[2] = {
    { 39, 0, 0, 0x2710, 0, 8000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_access_tunnel_8017FE50[22] = {
    { NULL, NULL },
    { D_shelter_b1_access_tunnel_8017FDB0, D_shelter_b1_access_tunnel_8017FD68 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_access_tunnel_8017FE10, D_shelter_b1_access_tunnel_8017FD80 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_access_tunnel_8017FE30, D_shelter_b1_access_tunnel_8017FD98 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_shelter_b1_access_tunnel_8017FF00 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b1_access_tunnel_8017FF0C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_access_tunnel_8017FF14[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_access_tunnel_8017FF00 },
};

WorldCollisionSurfaceProperties D_shelter_b1_access_tunnel_8017FF1C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_access_tunnel_8017FF00 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_access_tunnel_8017FF24[8] = {
    D_shelter_b1_access_tunnel_8017FF0C,
    D_shelter_b1_access_tunnel_8017FF14,
    D_shelter_b1_access_tunnel_8017FF1C,
    D_shelter_b1_access_tunnel_8017FF0C,
    D_shelter_b1_access_tunnel_8017FF0C,
    D_shelter_b1_access_tunnel_8017FF0C,
    D_shelter_b1_access_tunnel_8017FF0C,
    D_shelter_b1_access_tunnel_8017FF0C,
};

AreaApplyRec D_shelter_b1_access_tunnel_8017FF44[2] = {
    { 4, 21, 22, 0 },
    { 255, 0, 0, 0 },
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventStartStorage gRoomEventActive = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b1_access_tunnel_8017FF6C[4] = {
    0,
    2,
    47,
    210,
};

RoomEventReqStorage gRoomEventReq;

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 _accessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->queryOnly` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _accessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_access_tunnel_8017FF6C[0] = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_access_tunnel_8017E710, 0, 0, 0);
            D_shelter_b1_access_tunnel_8017FF6C[0] = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Messages 0x12 and 0x18, while nibble 0x113 is between 1
/// and 3 and this is not a dry run, apply the room's area records and set the
/// nibble to 4. Message 0x15, while nibble 0xE5 is clear, answers 0 and -
/// unless `in->queryOnly` asks for a dry run - passes `in->flagId` to
/// `Gp_SetNibbleIf` and runs cap command 1. Otherwise message 0x12 goes through
/// the rooms' event gate on flag 0xAD, message 0x14 starts the room event on
/// flag 0x13F, and any other message answers 1.
s32 func_shelter_b1_access_tunnel_8017DA68(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B1_CONTROL_ROOM || in->areaId == GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) > 0 && GameFlag_GetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) < 4) {
            Gp_ApplyAreaRecs(D_shelter_b1_access_tunnel_8017FF44);
            GameFlag_SetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, 4);
        }
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 && GameFlag_GetNibble(GAME_FLAG_GOLEM_FREEZER_UNLOCKED) == 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SetNibbleIf(in->flagId, 2);
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_CONTROL_ROOM) {
        req.capCmd        = 3;
        req.missingCapCmd = 1;
        req.firstSnd      = 0x54130009;
        req.secondSnd     = 0x54130001;
        req.flagId        = GAME_FLAG_B1_CONTROL_ROOM_TUNNEL_DOOR_UNLOCKED;
        req.collectedBit  = 0;
        return roomEventGate(&req, out);
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING) {
        event.capCmd   = 4;
        event.stageSnd = 0x54130005;
        event.flagId   = GAME_FLAG_B1_ACCESS_TUNNEL_TO_PARKING_SCENE;
        event.fade     = 0;
        return _accessTunnelStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_access_tunnel_8017DCA4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCAC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCB4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b1_access_tunnel_8017DD08` from a stack copy.
static const TaskFuncTable3 D_shelter_b1_access_tunnel_8017D5F0 = {
    {
        func_shelter_b1_access_tunnel_8017DCBC,
        func_shelter_b1_access_tunnel_8017DD00,
        taskKill,
    },
};

/// Installs the room's message table on `task` and advances it.
static void func_shelter_b1_access_tunnel_8017DCBC(Task* task)
{
    task->msgTable = D_shelter_b1_access_tunnel_8017E71C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

static void func_shelter_b1_access_tunnel_8017DD00(Task* task)
{
}

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b1_access_tunnel_8017DD08(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_access_tunnel_8017D5F0;
    sp.funcs[task->state](task);
}

void func_shelter_b1_access_tunnel_8017DD60(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            glowDrawCapsule(&p[0], 0x200, 0x334);
            glowDrawCapsule(&p[2], 0x200, 0x334);
            glowDrawCapsule(&p[4], 0x180, 0x444);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            glowDrawCapsule(&p[0], 0x200, 0x334);
            glowDrawCapsule(&p[2], 0x200, 0x334);
            glowDrawCapsule(&p[4], 0x180, 0x111);
            glowDrawCapsule(&p[6], 0x180, 0x222);
            glowDrawCapsule(&p[8], 0x180, 0x333);
            glowDrawCapsule(&p[10], 0x180, 0x444);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7D4;
            glowDrawCapsule(&p[0], 0x200, 0x343);
            glowDrawCapsule(&p[-8], 0x180, 0x444);
            glowDrawCapsule(&p[-6], 0x180, 0x333);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7B4;
            glowDrawCapsule(&p[0], 0x200, 0x344);
            glowDrawCapsule(&p[2], 0x200, 0x344);
            break;
        }
    }
}

#include "../../shared/glow_draw_capsule.inc.c"
