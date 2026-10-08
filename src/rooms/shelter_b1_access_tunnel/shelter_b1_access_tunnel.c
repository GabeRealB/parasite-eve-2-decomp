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
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
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
#include "../../shared/room_variants.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b1_access_tunnel_8017FF6C[4];

/// Task descriptor `_roomEventGate` spawns when a
/// gated event fires.
extern TaskDesc gRoomEventTaskDesc;

/// Task descriptor for the room's event task, spawned when the message handler
/// starts the latched event.
extern TaskDesc D_shelter_b1_access_tunnel_8017E710;

/// Message table `_shelterB1AccessTunnelInitializeRoom` installs on its task.
extern TaskMessageEntry D_shelter_b1_access_tunnel_8017E71C[];

extern SVECTOR D_shelter_b1_access_tunnel_8017E744[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7B4[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7D4[];

extern AreaApplyRec    D_shelter_b1_access_tunnel_8017FF44[];
extern RoomFadeStorage gRoomEventFade;

/// Copy of the message that fired a gated event, kept for the task
/// `roomEventTask` to warp from.
extern RoomEventMsg gRoomEventMsg;

/// Set by `_roomEventGate` when the event it gates has
/// just fired, clear otherwise.
extern RoomEventStartStorage gRoomEventActive;

extern RoomEventMsg gRoomEventStagedMsg;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `roomEventTask` plays.
extern RoomEventReqStorage gRoomEventReq;

extern RoomLatchedEvent gRoomEventLatched;

static void _shelterB1AccessTunnelInitializeRoom(Task* task);
static void _shelterB1AccessTunnelIdle(Task* task);

static s32 _shelterB1AccessTunnelResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1AccessTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB1AccessTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 command, s32 unusedArg);
static s32 _shelterB1AccessTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum { SHELTER_B1_ACCESS_TUNNEL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b1_access_tunnel_8017E710 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_access_tunnel_8017E71C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1AccessTunnelResolveRoomEvent },
    { SHELTER_B1_ACCESS_TUNNEL_MESSAGE_USE_KEY_ITEM, _shelterB1AccessTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1AccessTunnelIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1AccessTunnelIgnoreRoomCommand },
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

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static __inline__ s32 _shelterB1AccessTunnelStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    D_shelter_b1_access_tunnel_8017FF6C[0] = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *transition;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&D_shelter_b1_access_tunnel_8017E710, 0, 0, 0);
            D_shelter_b1_access_tunnel_8017FF6C[0] = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Resolves access-tunnel exits and gates the control-room and parking scenes.
///
/// Borrows complete eight-byte request/reply records, which may alias. Copies
/// the request before resolving its room; returns 0 for a locked freezer,
/// 1 for an ordinary exit, or 2 when a deferred door event is eligible.
/// Queries suppress scenes and meeting-progress updates. Executing departures
/// to the control room or transfer tunnel finish meeting progress 1..3.
static s32 _shelterB1AccessTunnelResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B1_ACCESS_TUNNEL_MEETING_FINISHED   = 4,
        SHELTER_B1_ACCESS_TUNNEL_CAP_DOOR_BLOCKED   = 1,
        SHELTER_B1_ACCESS_TUNNEL_CAP_CONTROL_ROOM   = 3,
        SHELTER_B1_ACCESS_TUNNEL_CAP_PARKING        = 4,
        SHELTER_B1_ACCESS_TUNNEL_MAP_MARK_BLOCKED   = 2,
        SHELTER_B1_ACCESS_TUNNEL_NO_FADE            = 0,
        SHELTER_B1_ACCESS_TUNNEL_SOUND_CONTROL_ROOM = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 9),
        SHELTER_B1_ACCESS_TUNNEL_SOUND_DOOR_OPEN    = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 1),
        SHELTER_B1_ACCESS_TUNNEL_SOUND_PARKING      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 5),
    };
    RoomEventReq     gateRequest;
    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_CONTROL_ROOM || request->areaId == GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) > 0 && gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) < SHELTER_B1_ACCESS_TUNNEL_MEETING_FINISHED) {
            areaApplySavedUpdates(D_shelter_b1_access_tunnel_8017FF44);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, SHELTER_B1_ACCESS_TUNNEL_MEETING_FINISHED);
        }
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 && gameFlagGetNibble(GAME_FLAG_GOLEM_FREEZER_UNLOCKED) == 0) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, SHELTER_B1_ACCESS_TUNNEL_MAP_MARK_BLOCKED);
            capRunCommandWithTransition(SHELTER_B1_ACCESS_TUNNEL_CAP_DOOR_BLOCKED);
        }
        return ROOM_VARIANT_TRANSITION_REFUSED;
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_CONTROL_ROOM) {
        gateRequest.capCmd        = SHELTER_B1_ACCESS_TUNNEL_CAP_CONTROL_ROOM;
        gateRequest.missingCapCmd = SHELTER_B1_ACCESS_TUNNEL_CAP_DOOR_BLOCKED;
        gateRequest.firstSnd      = SHELTER_B1_ACCESS_TUNNEL_SOUND_CONTROL_ROOM;
        gateRequest.secondSnd     = SHELTER_B1_ACCESS_TUNNEL_SOUND_DOOR_OPEN;
        gateRequest.flagId        = GAME_FLAG_B1_CONTROL_ROOM_TUNNEL_DOOR_UNLOCKED;
        gateRequest.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&gateRequest, reply);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING) {
        event.capCmd   = SHELTER_B1_ACCESS_TUNNEL_CAP_PARKING;
        event.stageSnd = SHELTER_B1_ACCESS_TUNNEL_SOUND_PARKING;
        event.flagId   = GAME_FLAG_B1_ACCESS_TUNNEL_TO_PARKING_SCENE;
        event.fade     = SHELTER_B1_ACCESS_TUNNEL_NO_FADE;
        return _shelterB1AccessTunnelStartEvent(reply, &event);
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Refuses key-item use in this room, selecting the inventory's cannot-use notice.
///
/// Ignores the collected `itemId` and every other argument; always returns zero.
static s32 _shelterB1AccessTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores room commands from scripts and triggers and always returns zero.
///
/// Neither the command word nor the other arguments are read or retained.
static s32 _shelterB1AccessTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 command, s32 unusedArg)
{
    return 0;
}

/// Ignores room-action requests from direction triggers and always returns zero.
///
/// The borrowed `request` and all other arguments are neither read nor retained.
static s32 _shelterB1AccessTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// The room's three-entry task state table, dispatched by
/// `shelterB1AccessTunnelRoomTask` from a stack copy.
static const TaskFuncTable3 D_shelter_b1_access_tunnel_8017D5F0 = {
    {
        _shelterB1AccessTunnelInitializeRoom,
        _shelterB1AccessTunnelIdle,
        taskKill,
    },
};

/// Publishes the initialized access-tunnel receiver for synchronous room messages.
///
/// Requires state 0 and the loaded room message table. Registers the live task
/// in `GAME_TASK_SLOT_ROOM` and advances to idle state 1, without allocating work.
static void _shelterB1AccessTunnelInitializeRoom(Task* task)
{
    task->msgTable = D_shelter_b1_access_tunnel_8017E71C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the room task available for messages without per-frame work or state changes.
static void _shelterB1AccessTunnelIdle(Task* task)
{
}

void shelterB1AccessTunnelRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b1_access_tunnel_8017D5F0;
    stateHandlers.funcs[task->state](task);
}

/// Draws two additive capsule glows with a shared radius scale and tint.
///
/// `worldPoints` supplies at least four consecutive, word-aligned world-space
/// endpoints, paired as [0, 1] and [2, 3]. The points are read only during the
/// call. Each capsule is independently rejected if either endpoint has negative
/// GTE flags; accepted endpoints must have nonzero depth (camera Z / 4).
/// The signed low halfword of `radiusScale` gives the pixel radius at each end
/// as `(s16)radiusScale * 64 / depth`. `packedColor` bits 8..11, 4..7 and 0..3
/// supply red, green and blue nibbles scaled by 16; odd animation frames set
/// bit 3 in each channel. Higher colour bits are ignored in this room's drawer.
///
/// Requires composed view matrices, an initialized scratch stack, and a current
/// ordering table and packet arena with space for up to twelve Gouraud quads
/// and their additive blend commands. Queued packets live until GPU completion;
/// the endpoint storage is not retained.
static inline void _shelterB1AccessTunnelDrawGlowPair(const SVECTOR worldPoints[4], s32 radiusScale, s32 packedColor)
{
    _glowDrawCapsule(&worldPoints[0], radiusScale, packedColor);
    _glowDrawCapsule(&worldPoints[2], radiusScale, packedColor);
}

void shelterB1AccessTunnelDrawGlowsTask(Task* unused)
{
    // Packed colours contain RGB nibbles, each scaled by 16 by the capsule drawer.
    // Radius scales produce pixel radii of scale * 64 / (camera Z / 4).
    enum {
        SHELTER_B1_ACCESS_TUNNEL_GLOW_WIDE_RADIUS_SCALE   = 0x200,
        SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE = 0x180,
        SHELTER_B1_ACCESS_TUNNEL_GLOW_BLUE_TINT           = 0x334,
        SHELTER_B1_ACCESS_TUNNEL_GLOW_GREEN_TINT          = 0x343,
        SHELTER_B1_ACCESS_TUNNEL_GLOW_CYAN_TINT           = 0x344,
        SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP           = 0x111,
    };
    u8 viewIndex;

    // Each active view draws only its selected pairs of world-space light endpoints.
    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_access_tunnel_8017E744;
            _shelterB1AccessTunnelDrawGlowPair(lightPoints, SHELTER_B1_ACCESS_TUNNEL_GLOW_WIDE_RADIUS_SCALE, SHELTER_B1_ACCESS_TUNNEL_GLOW_BLUE_TINT);
            _glowDrawCapsule(&lightPoints[4], SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, 4 * SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            break;
        }
        case 3: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_access_tunnel_8017E744;
            _shelterB1AccessTunnelDrawGlowPair(lightPoints, SHELTER_B1_ACCESS_TUNNEL_GLOW_WIDE_RADIUS_SCALE, SHELTER_B1_ACCESS_TUNNEL_GLOW_BLUE_TINT);
            _glowDrawCapsule(&lightPoints[4], SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            _glowDrawCapsule(&lightPoints[6], SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, 2 * SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            _glowDrawCapsule(&lightPoints[8], SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, 3 * SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            _glowDrawCapsule(&lightPoints[10], SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, 4 * SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            break;
        }
        case 4: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_access_tunnel_8017E7D4;
            _glowDrawCapsule(&lightPoints[0], SHELTER_B1_ACCESS_TUNNEL_GLOW_WIDE_RADIUS_SCALE, SHELTER_B1_ACCESS_TUNNEL_GLOW_GREEN_TINT);
            // Select the last two pairs of the fourteen-point array, across the
            // intervening four-point array, using PS1 integer byte addresses.
            _glowDrawCapsule((const SVECTOR*)((u32)lightPoints - 8 * sizeof(*lightPoints)), SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, 4 * SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            _glowDrawCapsule((const SVECTOR*)((u32)lightPoints - 6 * sizeof(*lightPoints)), SHELTER_B1_ACCESS_TUNNEL_GLOW_NARROW_RADIUS_SCALE, 3 * SHELTER_B1_ACCESS_TUNNEL_GLOW_GREY_STEP);
            break;
        }
        case 5: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_access_tunnel_8017E7B4;
            _shelterB1AccessTunnelDrawGlowPair(lightPoints, SHELTER_B1_ACCESS_TUNNEL_GLOW_WIDE_RADIUS_SCALE, SHELTER_B1_ACCESS_TUNNEL_GLOW_CYAN_TINT);
            break;
        }
    }
}

#include "../../shared/glow_draw_capsule.inc.c"
