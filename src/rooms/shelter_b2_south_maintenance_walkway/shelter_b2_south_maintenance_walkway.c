#include "rooms/shelter_b2_south_maintenance_walkway.h"

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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
// The flag symbol carries seven unproven bytes after the flag.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
// The request symbol carries twelve unproven bytes after the request.
#define ROOM_EVENT_REQ gRoomEventReq.request
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b2_south_maintenance_walkway_801838F4[4];

/// Descriptors of the two event tasks: the one the event gate spawns, and the
/// one the walkway's handler spawns for its own event.
extern TaskDesc gRoomEventTaskDesc;
extern TaskDesc D_shelter_b2_south_maintenance_walkway_80182544;

/// The walkway's message table, installed as the room task's `msgTable`.
extern TaskMessageEntry D_shelter_b2_south_maintenance_walkway_80182550[];

/// Anchor points of the glows the room's draw task picks by camera view: ten
/// point pairs, one per glow, followed by the single point of the red disc.
/// Views share pairs, so each view draws its own run of the table.
extern SVECTOR D_shelter_b2_south_maintenance_walkway_80182578[];

/// Offsets from the anchor of the two points the twin trail follows. The
/// second is also reached under its own name.

/// Spawn payload of the task 0x31 the handler's event task may start.
extern RoomFadeStorage gRoomEventFade;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg        gRoomEventMsg;
extern RoomEventReqStorage gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern RoomEventStartStorage gRoomEventActive;

/// The message and the event the walkway's handler latched for its event task,
/// and the flag saying its last call did so.
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

s32        func_shelter_b2_south_maintenance_walkway_8017DA7C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

extern TaskDesc Actor04400_D107E4;

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b2_south_maintenance_walkway_80182544 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_south_maintenance_walkway_80182550[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_south_maintenance_walkway_8017DA7C },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM, _shelterB2SouthMaintenanceWalkwayIgnoreKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2SouthMaintenanceWalkwayIgnoreAction },
    { ROOM_MESSAGE_COMMAND, _shelterB2SouthMaintenanceWalkwayIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b2_south_maintenance_walkway_80182578[21] = {
    { 894, -196, 3102, 0 },
    { 894, -196, 2264, 0 },
    { 894, -196, 404, 0 },
    { 894, -196, -385, 0 },
    { 894, -196, -2032, 0 },
    { 894, -196, -2645, 0 },
    { 3102, -196, 3102, 0 },
    { 3102, -196, 2264, 0 },
    { 3102, -196, 404, 0 },
    { 3102, -196, -385, 0 },
    { 3102, -196, -2032, 0 },
    { 3102, -196, -2645, 0 },
    { 650, -196, -2895, 0 },
    { -31, -196, -2895, 0 },
    { 600, -196, -5113, 0 },
    { -45, -196, -5113, 0 },
    { -1556, -196, -2895, 0 },
    { -2498, -196, -2895, 0 },
    { -1556, -196, -5113, 0 },
    { -2498, -196, -5113, 0 },
    { 763, -1283, -2117, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

#include "../../shared/room_visual_effects_disc_data.inc.c"

u8* D_shelter_b2_south_maintenance_walkway_8018263C[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_south_maintenance_walkway_80182640[1] = { 5 };

DirectionWarpEntry D_shelter_b2_south_maintenance_walkway_80182644[2] = {
    { { { .word = 1024 }, -2412, 0, -3946 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2412, 0, -3946 }, { 0, 0, 0, 0 }, 0x541C0002, 0x541C0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 457 },
    { { { .word = 2048 }, 2048, 0, 4600 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 2048, 0, 4600 }, { 0, 0, 0, 0 }, 0x541C0002, 0x541C0001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, 456 },
};

static SVECTOR _gShelterB2SouthMaintenanceWalkwayCollision05428Normals[14] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_normals.inc"
};

static SVECTOR _gShelterB2SouthMaintenanceWalkwayCollision05428Verts[38] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_verts.inc"
};

static WorldCollisionGridFace _gShelterB2SouthMaintenanceWalkwayCollision05428Faces[18] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_faces.inc"
};

static s16 _gShelterB2SouthMaintenanceWalkwayCollision05428Cells[82] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2SouthMaintenanceWalkwayCollision05428Cells[i])
static s16* _gShelterB2SouthMaintenanceWalkwayCollision05428Table[6] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_south_maintenance_walkway_801829E8 = { NULL, _gShelterB2SouthMaintenanceWalkwayCollision05428Normals, _gShelterB2SouthMaintenanceWalkwayCollision05428Verts, _gShelterB2SouthMaintenanceWalkwayCollision05428Faces, _gShelterB2SouthMaintenanceWalkwayCollision05428Table, 2872, 5300, 2, 3, 4000, 18 };

ViewCamera D_shelter_b2_south_maintenance_walkway_80182A0C[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x35D2, 0 } }, 235 },
    { { { { 813, 0, 4014 }, { 1626, 3744, -329 }, { -3670, 1659, 743 } }, { -2032, 2295, 4776 } }, 235 },
    { { { { 950, 0, -3984 }, { -1563, 3767, -372 }, { 3664, 1607, 874 } }, { 2128, 2217, 4776 } }, 246 },
    { { { { 3928, 0, 1158 }, { 443, 3784, -1503 }, { -1070, 1567, 3629 } }, { -2962, 2210, 4830 } }, 235 },
    { { { { 3846, 0, 1407 }, { 529, 3795, -1446 }, { -1304, 1539, 3564 } }, { -2962, 2210, 182 } }, 235 },
};

SpriteBatch D_shelter_b2_south_maintenance_walkway_80182AC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_south_maintenance_walkway_80182AD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_south_maintenance_walkway_80182AE0[64] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 72, 0, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 80, 0, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 88, 0, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 64, 0, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 0, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -80, 0, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -56, 0, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -16, 0, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -80, 0, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -48, 0, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -8, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -120, 0, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -80, 0, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -40, 0, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -8, 0, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -16, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -80, 0, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -56, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -80, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, -56, 0, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -8, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -64, 0, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -32, 0, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -48, 0, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 32, 1019, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 24, 974, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 24, 977, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 968, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 1010, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 992, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -120, 48, 937, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 48, 766, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 64, 750, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -120, 64, 966, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -120, 72, 928, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 72, 750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 80, 750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -120, 80, 839, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 88, 850, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 88, 750, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -120, 40, 934, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 750, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 24, 944, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 16, 914, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 0, 896, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 24, 966, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 24, 750, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -40, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -160, -96, 750, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, -120, 750, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -128, -120, 631, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -88, -120, 794, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, -96, 749, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, -96, 805, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, -80, 737, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -64, 753, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, -48, 805, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -24, 842, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -8, 863, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 8, 836, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 769, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, -8, 788, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -24, 765, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -48, 747, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_south_maintenance_walkway_80182FE0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 64, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_south_maintenance_walkway_80182FF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_south_maintenance_walkway_80183008[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_south_maintenance_walkway_80183018[5] = {
    { { .empty = D_shelter_b2_south_maintenance_walkway_80182AC0 }, D_shelter_b2_south_maintenance_walkway_80182AC0, NULL },
    { { .empty = D_shelter_b2_south_maintenance_walkway_80182AD0 }, D_shelter_b2_south_maintenance_walkway_80182AD0, NULL },
    { { .elements = D_shelter_b2_south_maintenance_walkway_80182AE0 }, D_shelter_b2_south_maintenance_walkway_80182FE0, NULL },
    { { .empty = D_shelter_b2_south_maintenance_walkway_80182FF8 }, D_shelter_b2_south_maintenance_walkway_80182FF8, NULL },
    { { .empty = D_shelter_b2_south_maintenance_walkway_80183008 }, D_shelter_b2_south_maintenance_walkway_80183008, NULL },
};

WorldCoordPointLight D_shelter_b2_south_maintenance_walkway_80183054[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2022, -223, -3929 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2348, 2757, 2798 }, { 0, 0 } }, 1852, 2632 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 367, -223, -4032 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2570, 3039, 3058 }, { 0, 0 } }, 1540, 3660 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2187, -303, -4148 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1365, 1697, 1877 }, { 0, 0 } }, 1821, 2131 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1972, -223, 2426 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2007, 2638, 2897 }, { 0, 0 } }, 1742, 2801 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1986, -223, -1184 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2509, 3039, 3079 }, { 0, 0 } }, 2121, 3602 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2025, -223, -2356 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1847, 2297, 2416 }, { 0, 0 } }, 1500, 1800 },
};

WorldCoordRoomLights D_shelter_b2_south_maintenance_walkway_80183294 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_south_maintenance_walkway_80183054), D_shelter_b2_south_maintenance_walkway_80183054, 0, NULL };

WorldCollisionTrigger D_shelter_b2_south_maintenance_walkway_801832AC[6] = {
    { NULL, NULL, NULL, { 2000, -1361, 1664, 0 }, { { -1321, -1936, -474, 0 }, { 1306, -1936, 456, 0 }, { -1321, 1937, -474, 0 }, { 1306, 1937, 456, 0 } }, { 1372, 0, -3878, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2000, -1408, 1537, 0 }, { { 1224, -1936, 369, 0 }, { -1229, -1936, -377, 0 }, { 1224, 1937, 369, 0 }, { -1229, 1937, -377, 0 } }, { -1198, 0, 3932, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2224, -1408, -2769, 0 }, { { 1360, -1936, 208, 0 }, { -1360, -1936, -208, 0 }, { 1360, 1937, 208, 0 }, { -1360, 1937, -208, 0 } }, { -622, 0, 4052, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2176, -1281, -2657, 0 }, { { -1392, -1936, -224, 0 }, { 1392, -1936, 224, 0 }, { -1392, 1937, -224, 0 }, { 1392, 1937, 224, 0 } }, { 651, 0, -4054, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 224, -1440, -4128, 0 }, { { 11, -1936, 1210, 0 }, { -21, -1936, -1221, 0 }, { 11, 1937, 1210, 0 }, { -21, 1937, -1221, 0 } }, { -4112, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 96, -1408, -4160, 0 }, { { -20, -1936, -1316, 0 }, { 12, -1936, 1308, 0 }, { -20, 1937, -1316, 0 }, { 12, 1937, 1308, 0 } }, { 4115, 0, -52, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_south_maintenance_walkway_80183474[2] = {
    { NULL, NULL, NULL, { -2528, -48, -3968, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, 27, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1952, -48, 4448, 0 }, { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_south_maintenance_walkway_8018350C[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 24, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202400_8014E47C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_south_maintenance_walkway_80183530[3] = {
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_south_maintenance_walkway_80183554[4] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04400_D107E4 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_8014E7A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_south_maintenance_walkway_80183584[3] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { 55, 55, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205500_801528DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_south_maintenance_walkway_801835A8[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202300_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_south_maintenance_walkway_801835CC[9] = {
    { 21, 3, 0, 950, -2000, 2500, 1024, 0, 0, 2, 1 },
    { 21, 3, 0, 950, -2000, -500, 1024, 0, 0, 2, 7 },
    { 21, 2, 0, 1400, -2000, 5000, 2048, 0, 0, 2, 5 },
    { 21, 2, 0, 2600, -2000, 5000, 2048, 0, 0, 2, 5 },
    { 24, 0, 0, 2000, 0, -4000, 3700, 0, 2, 4, 0 },
    { 24, 0, 0, 2400, 0, -2900, 3200, 0, 2, 4, 0 },
    { 24, 0, 0, 2400, 0, -850, 3600, 0, 2, 4, 0 },
    { 24, 0, 0, 1450, 0, -2500, 1700, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_south_maintenance_walkway_8018365C[4] = {
    { 72, 0, 0, 2150, 0, 2050, 3700, 0, 0, 2, 0 },
    { 49, 0, 0, 1600, 0, -3600, 3500, 0, 2, 4, 0 },
    { 49, 0, 0, 1650, 0, 0, 1000, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_south_maintenance_walkway_8018369C[5] = {
    { 44, 0, 0, 2050, 0, 720, 3584, 0, 0, 2, 0 },
    { 72, 0, 0, 2000, 0, -1400, 2048, 0, 2, 4, 0 },
    { 72, 0, 0, 850, 0, -4200, 3072, 0, 2, 4, 0 },
    { 73, 0, 1, 1500, 0, -230, 1000, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_south_maintenance_walkway_801836EC[6] = {
    { 24, 0, 1, 2150, 0, 1750, 2000, 0, 0, 2, 0 },
    { 55, 0, 3, 2550, -2700, 550, 3360, 0, 2, 4, 5 },
    { 55, 0, 3, 2050, -2700, -1050, 3900, 0, 2, 4, 3 },
    { 55, 0, 3, 1600, -2700, -2200, 200, 0, 2, 4, 4 },
    { 55, 0, 3, 1000, -2700, -4100, 200, 0, 2, 4, 5 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_south_maintenance_walkway_8018374C[6] = {
    { 21, 3, 0, 800, -2200, 2500, 1024, 0, 0, 2, 1 },
    { 21, 3, 0, 800, -2200, 0, 1024, 0, 0, 2, 5 },
    { 21, 2, 0, 1400, -2000, 5000, 2048, 0, 0, 2, 5 },
    { 21, 2, 0, 2600, -2000, 5000, 2048, 0, 0, 2, 5 },
    { 23, 7, 1, 2000, 0, -3000, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_south_maintenance_walkway_801837AC[22] = {
    { NULL, NULL },
    { D_shelter_b2_south_maintenance_walkway_801835CC, D_shelter_b2_south_maintenance_walkway_8018350C },
    { D_shelter_b2_south_maintenance_walkway_8018365C, D_shelter_b2_south_maintenance_walkway_80183530 },
    { D_shelter_b2_south_maintenance_walkway_8018369C, D_shelter_b2_south_maintenance_walkway_80183554 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_south_maintenance_walkway_801836EC, D_shelter_b2_south_maintenance_walkway_80183584 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_south_maintenance_walkway_8018374C, D_shelter_b2_south_maintenance_walkway_801835A8 },
};

WorldCollisionOccluder D_shelter_b2_south_maintenance_walkway_8018385C[1] = {
    { NULL, NULL, { -1616, -1280, -544, 0 }, { { -2384, 2304, 2240, 0 }, { 2384, 2304, -2240, 0 }, { -2384, -2304, 2240, 0 }, { 2384, -2304, -2240, 0 } }, { 2809, 0, 2989, 0 }, 3998, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b2_south_maintenance_walkway_80183898 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b2_south_maintenance_walkway_801838A4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_south_maintenance_walkway_801838AC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_south_maintenance_walkway_80183898 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_south_maintenance_walkway_801838B4[8] = {
    D_shelter_b2_south_maintenance_walkway_801838A4,
    D_shelter_b2_south_maintenance_walkway_801838AC,
    D_shelter_b2_south_maintenance_walkway_801838A4,
    D_shelter_b2_south_maintenance_walkway_801838A4,
    D_shelter_b2_south_maintenance_walkway_801838A4,
    D_shelter_b2_south_maintenance_walkway_801838A4,
    D_shelter_b2_south_maintenance_walkway_801838A4,
    D_shelter_b2_south_maintenance_walkway_801838A4,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventStartStorage gRoomEventActive = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b2_south_maintenance_walkway_801838F4[4] = {
    0,
    7,
    123,
    189,
};

RoomEventReqStorage gRoomEventReq;

RoomLatchedEvent gRoomEventLatched;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// Latches a first-use departure event, or tests its eligibility for a query.
///
/// Borrows complete eight-byte `destination` and twelve-byte `event` records
/// for this call. A zero flag ID stays eligible; other IDs must name valid
/// game-flag nibbles. Returns 1 for an already latched flag, allowing ordinary
/// departure, or 2 for an eligible staged departure. Execution copies both
/// records into room-owned storage, sets a nonzero flag's nibble to 1 and
/// attempts to spawn the staged task. Every call clears the event-started
/// byte; eligible execution raises it even if spawning fails. The room
/// overlay must remain loaded until the staged task finishes.
static __inline__ s32 _shelterB2SouthMaintenanceWalkwayStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event)
{
    enum { EVENT_NO_FLAG            = 0,
           EVENT_FLAG_LATCHED       = 1,
           EVENT_ORDINARY_DEPARTURE = 1,
           EVENT_STAGED_DEPARTURE   = 2 };

    D_shelter_b2_south_maintenance_walkway_801838F4[0] = false;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == EVENT_NO_FLAG) {
        if (destination->queryOnly == ROOM_EVENT_EXECUTE) {
            // Own the caller's records before the staged task can consume them.
            gRoomEventStagedMsg = *destination;
            gRoomEventLatched   = *event;
            if (event->flagId != EVENT_NO_FLAG) {
                gameFlagSetNibble(event->flagId, EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&D_shelter_b2_south_maintenance_walkway_80182544, 0, 0, 0);
            D_shelter_b2_south_maintenance_walkway_801838F4[0] = true;
        }
        return EVENT_STAGED_DEPARTURE;
    }
    return EVENT_ORDINARY_DEPARTURE;
}

#include "../../shared/room_event_staged_task.inc.c"

/// Message handler: copies the incoming message to `out` and forwards both to
/// `mapShelterRoomVariantResolve`. Message 0x1D goes through the room's event gate on flag
/// 0xAA with no prerequisite; message 0x1B starts the room's own event on flag
/// 0x13C; any other message answers 1.
s32 func_shelter_b2_south_maintenance_walkway_8017DA7C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B2_OPERATING_ROOM) {
        req.capCmd        = 1;
        req.missingCapCmd = 1;
        req.firstSnd      = 0x541C0005;
        req.secondSnd     = 0x541C0001;
        req.flagId        = GAME_FLAG_OPERATING_ROOM_SOUTH_DOOR_UNLOCKED;
        req.collectedBit  = 0;
        return _roomEventGate(&req, out);
    }
    if (in->areaId != GAME_AREA_SHELTER_B2_ELEVATOR_HALL) {
        return 1;
    }
    event.capCmd   = 2;
    event.stageSnd = 0x541C0001;
    event.flagId   = GAME_FLAG_B2_SOUTH_WALKWAY_TO_ELEVATOR_SCENE;
    event.fade     = 0;
    return _shelterB2SouthMaintenanceWalkwayStartEvent(out, &event);
}

/// Refuses every key-item use request, returning zero without consuming the item.
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores every room command and returns zero; neither payload word is read.
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores every trigger action and returns zero without reading the borrowed request.
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs the room's message handlers and publishes its task for room requests.
///
/// Runs at state 0 and advances to idle state 1. The task and this overlay's
/// message table must remain live while `GAME_TASK_SLOT_ROOM` can receive messages.
static void _shelterB2SouthMaintenanceWalkwayInitializeRoomTask(Task* task)
{
    task->msgTable = D_shelter_b2_south_maintenance_walkway_80182550;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the room task available for messages without per-frame work.
static void _shelterB2SouthMaintenanceWalkwayIdleRoomTask(Task* task)
{
}

/// The room task's three states: setup, idle and exit.
static const TaskFuncTable3 D_shelter_b2_south_maintenance_walkway_8017D5F0 = {
    { _shelterB2SouthMaintenanceWalkwayInitializeRoomTask, _shelterB2SouthMaintenanceWalkwayIdleRoomTask, taskKill },
};

void shelterB2SouthMaintenanceWalkwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b2_south_maintenance_walkway_8017D5F0;
    stateHandlers.funcs[task->state](task);
}

/// Installs this room's packed bank-6 task IDs for the six shared combat effects.
///
/// Call after room-effect controller initialization and before spawning these
/// effects. The selectors persist until cleared or replaced; this overlay must
/// remain loaded while the selected effect tasks run.
static inline void _shelterB2SouthMaintenanceWalkwaySelectEffectTasks(void)
{
    gRoomEffectFlashId        = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPARK_BURST;
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2;
}

void shelterB2SouthMaintenanceWalkwayDrawGlowsTask(Task* task)
{
    enum { GLOW_INITIALIZE,
           GLOW_DRAW,
           GLOW_RADIUS_SCALE = 0x200,
           GLOW_QUARTER_TURN = 0x400,
           GLOW_HALF_TURN    = 0x800,
           GLOW_DISC_ANCHOR  = 20 };

    u8 viewIndex;

    if (task->state == GLOW_INITIALIZE) {
        _shelterB2SouthMaintenanceWalkwaySelectEffectTasks();
        task->state = GLOW_DRAW;
    }

    // Each view selects visible pairs of world-space anchors; view 4 adds the disc.
    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2:
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[12], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[14], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[16], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[18], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        case 3:
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[10], GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[12], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[14], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
        case 4:
            glowDrawRedDisc(&D_shelter_b2_south_maintenance_walkway_80182578[GLOW_DISC_ANCHOR], GLOW_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[2], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[4], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[6], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[8], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[10], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[12], GLOW_RADIUS_SCALE, 0);
            break;
        case 5:
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&D_shelter_b2_south_maintenance_walkway_80182578[6], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
    }
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
