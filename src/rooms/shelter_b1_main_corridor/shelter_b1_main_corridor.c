#include "rooms/shelter_b1_main_corridor.h"

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
extern u8 D_shelter_b1_main_corridor_80185D44[4];

/// The event the gate last accepted: the message that triggered it, whose
/// `msgId`, `field_2` and `field_3` name the area, warp and room the event
/// task finally loads, and the request whose CAP command and sounds it runs.
extern RoomEventMsg        gRoomEventMsg;
extern RoomEventReqStorage gRoomEventReq;
/// Set once the gate has latched an event and spawned its task.
extern RoomEventStartStorage gRoomEventActive;
/// Spawn descriptor of the event task, `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The event the message handler last started: the message that triggered it,
/// the flag saying one was latched and its task spawned, and the event record.
/// `D_shelter_b1_main_corridor_80183098` spawns that event's task,
/// `roomEventStagedTask`.
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;
extern TaskDesc         D_shelter_b1_main_corridor_80183098;
/// Spawn argument the event task hands to task 0x31.
extern RoomFadeStorage gRoomEventFade;

/// The room's message table, which its tasks answer from.
extern TaskMessageEntry D_shelter_b1_main_corridor_801830A4[];

/// Points the per-view drawing places its capsules and sprites at.
extern SVECTOR D_shelter_b1_main_corridor_801830D4[];
extern SVECTOR D_shelter_b1_main_corridor_80183114[];
extern SVECTOR D_shelter_b1_main_corridor_80183124[];
extern SVECTOR D_shelter_b1_main_corridor_80183134[];
extern SVECTOR D_shelter_b1_main_corridor_80183144[];

/// Per-colour shift amounts the flash effect's brightness is scaled down by,
/// one row per colour its spawner can pick.

/// The two points the beam runs between, relative to its parent coordinate;
/// the second is also declared on its own.

s32 func_shelter_b1_main_corridor_8017DA8C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_main_corridor_8017DCEC(Task*, s32, s32, s32);
s32 func_shelter_b1_main_corridor_8017DCF4(Task*, s32, s32, s32);
s32 func_shelter_b1_main_corridor_8017DCFC(Task*, s32, s32, s32);
s32 func_shelter_b1_main_corridor_8017DD04(Task*, s32, s32, s32);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b1_main_corridor_80183098 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_main_corridor_801830A4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_main_corridor_8017DA8C },
    { 5105, func_shelter_b1_main_corridor_8017DCEC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_main_corridor_8017DCFC },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_main_corridor_8017DCF4 },
    { ROOM_MESSAGE_SOUND, func_shelter_b1_main_corridor_8017DD04 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b1_main_corridor_801830D4[8] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183114[2] = {
    { -3246, -2265, -0x2988, 0 },
    { -3246, -2265, -9370, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183124[2] = {
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183134[2] = {
    { 3260, -2265, -0x2988, 0 },
    { 3260, -2265, -9370, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183144[18] = {
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -0x34A6, 0 },
    { -1293, -4768, -0x2CE0, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -0x34A6, 0 },
    { 1293, -4768, -0x2CE0, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x70C2 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static inline RoomFxShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b1_main_corridor_801831F8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_main_corridor_801831FC[1] = { 10 };

DirectionWarpEntry D_shelter_b1_main_corridor_80183200[6] = {
    { { { .word = 0 }, 0, 0, -0x4A38 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1CD },
    { { { .word = 1024 }, -3467, 0, -0x2745 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 3510, 0, -0x2729 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -3500, 0, -1940 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -3500, 0, -1940 }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0x540F000D, 10, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_R47_1C6 },
    { { { .word = 3072 }, 3500, 0, -1940 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 0, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB1MainCorridorCollision06B30Normals[34] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_normals.inc"
};

static SVECTOR _gShelterB1MainCorridorCollision06B30Verts[172] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_verts.inc"
};

static WorldCollisionGridFace _gShelterB1MainCorridorCollision06B30Faces[82] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_faces.inc"
};

static s16 _gShelterB1MainCorridorCollision06B30Cells[392] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1MainCorridorCollision06B30Cells[i])
static s16* _gShelterB1MainCorridorCollision06B30Table[18] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_main_corridor_801840F0 = { NULL, _gShelterB1MainCorridorCollision06B30Normals, _gShelterB1MainCorridorCollision06B30Verts, _gShelterB1MainCorridorCollision06B30Faces, _gShelterB1MainCorridorCollision06B30Table, 5398, 0x4D03, 3, 6, 4000, 82 };

ViewCamera D_shelter_b1_main_corridor_80184114[10] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -4024, 0, 762 }, { 150, 4015, 794 }, { -747, 808, -3945 } }, { -721, 1939, 0x2F2F } }, 275 },
    { { { { -3981, 0, 962 }, { 213, 3993, 882 }, { -938, 908, -3882 } }, { -784, 2080, 8975 } }, 275 },
    { { { { -4071, 0, 448 }, { 31, 4085, 288 }, { -447, 290, -4061 } }, { -1306, 2162, 2495 } }, 269 },
    { { { { -2233, 0, -3433 }, { -683, 4013, 444 }, { 3364, 815, -2188 } }, { -62, 1858, 8634 } }, 269 },
    { { { { -2474, 0, 3264 }, { 649, 4014, 492 }, { -3199, 814, -2424 } }, { -362, 1983, 8329 } }, 269 },
    { { { { 3975, 0, 986 }, { 54, 4089, -219 }, { -984, 225, 3969 } }, { -665, 2036, 0x2EF4 } }, 269 },
    { { { { 4034, 0, 705 }, { 123, 4032, -707 }, { -694, 718, 3972 } }, { -564, 1895, 6271 } }, 257 },
    { { { { 1839, 0, -3659 }, { -944, 3957, -474 }, { 3535, 1056, 1777 } }, { 945, 1694, 2927 } }, 282 },
    { { { { 2080, 0, 3528 }, { 535, 4048, -316 }, { -3487, 622, 2056 } }, { -392, 1529, 3409 } }, 282 },
};

SpriteBatch D_shelter_b1_main_corridor_8018427C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_main_corridor_8018428C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_main_corridor_8018429C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_main_corridor_801842AC[56] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 1642, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 64, 1482, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 56, 1484, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 32, 1375, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 32, 1446, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 24, 1691, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 48, 1665, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 48, 1625, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, -24, 1509, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, -16, 1510, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, -8, 1520, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 0, 1426, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 16, 1432, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 24, 1443, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 16, 1440, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 8, 1437, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 0, 1434, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 32, 1446, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 40, 1449, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 32, 1438, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 48, 1431, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 56, 1401, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 48, 1444, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 72, 1373, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 64, 1372, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 64, 1450, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 56, 1456, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 56, 1451, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 72, 1458, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, -24, 1690, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 1681, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 8, 1666, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1578, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 72, 1596, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 1562, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 72, 1597, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 64, 1546, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 56, 1644, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 64, 1668, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 56, 1655, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 64, 1651, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 0, 1479, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 1467, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 16, 1452, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 24, 1442, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -16, 1685, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 16, 1638, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 32, 1631, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, -24, 1427, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, -16, 1428, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, -8, 1431, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 72, 1574, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 72, 1584, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 56, 1674, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 56, 1668, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 56, 1646, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_8018470C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 56, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b1_main_corridor_80184724[2] = {
    { { 33, 0, 285, 239 }, 1373 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b1_main_corridor_80184738[13] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 96, 694, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 96, 712, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 96, 712, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 96, 685, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 96, 675, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 72, 854, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 72, 682, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 48, 679, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 32, 650, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 647, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 650, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -32, 651, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -48, 679, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_8018483C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_main_corridor_80184854[17] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 104, 0xFFF0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 732, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 104, 730, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 88, 759, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 104, 746, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 80, 802, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 104, 779, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 773, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 64, 971, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 72, 754, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 72, 728, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 769, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 40, 773, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 8, 752, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -8, 747, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -32, 748, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_801849A8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_main_corridor_801849C0[24] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 48, 1953, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 2152, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 2102, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 40, 2073, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 40, 2045, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 40, 2024, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 40, 1995, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 1928, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 1922, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 8, 1955, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -16, 1961, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 48, 0xFFF0, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 40, 2257, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 2293, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 40, 2267, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 32, 2308, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 32, 2335, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 2369, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 2362, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 32, 2311, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 2315, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 2351, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 8, 2324, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, -16, 2289, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80184BA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b1_main_corridor_80184BB8[2] = {
    { { 105, 0, 215, 239 }, 2289 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b1_main_corridor_80184BCC[19] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 0, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 0, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 752, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 88, 802, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 88, 783, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 88, 774, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 88, 767, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 88, 758, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 750, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 838, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 72, 881, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 881, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 72, 874, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 72, 886, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 72, 897, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 72, 909, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 64, 984, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 72, 919, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 72, 925, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80184D48[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_main_corridor_80184D60[26] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, 88, 0, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 40, 731, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 24, 744, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, 8, 734, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, -8, 731, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, -24, 729, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, -32, 725, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -48, 721, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, -56, 717, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -152, -72, 714, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -80, 726, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -96, 723, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -104, 717, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -120, 712, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -72, 805, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -56, 780, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -40, 843, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -24, 850, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -8, 858, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 8, 745, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 24, 881, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, 40, 889, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 56, 923, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 56, 906, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 56, 743, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, 72, 843, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80184F68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_main_corridor_80184F80[20] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 112, 610, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 104, 627, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 96, 663, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 777, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 88, 672, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 80, 718, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 771, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 72, 780, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 56, 937, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 56, 736, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 56, 677, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 735, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 40, 735, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 16, 743, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 24, 739, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 0, 697, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -16, 708, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 701, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -40, 685, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -72, 704, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80185110[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_main_corridor_80185128[10] = {
    { { .empty = D_shelter_b1_main_corridor_8018427C }, D_shelter_b1_main_corridor_8018427C, NULL },
    { { .empty = D_shelter_b1_main_corridor_8018428C }, D_shelter_b1_main_corridor_8018428C, NULL },
    { { .empty = D_shelter_b1_main_corridor_8018429C }, D_shelter_b1_main_corridor_8018429C, NULL },
    { { .elements = D_shelter_b1_main_corridor_801842AC }, D_shelter_b1_main_corridor_8018470C, D_shelter_b1_main_corridor_80184724 },
    { { .elements = D_shelter_b1_main_corridor_80184738 }, D_shelter_b1_main_corridor_8018483C, NULL },
    { { .elements = D_shelter_b1_main_corridor_80184854 }, D_shelter_b1_main_corridor_801849A8, NULL },
    { { .elements = D_shelter_b1_main_corridor_801849C0 }, D_shelter_b1_main_corridor_80184BA0, D_shelter_b1_main_corridor_80184BB8 },
    { { .elements = D_shelter_b1_main_corridor_80184BCC }, D_shelter_b1_main_corridor_80184D48, NULL },
    { { .elements = D_shelter_b1_main_corridor_80184D60 }, D_shelter_b1_main_corridor_80184F68, NULL },
    { { .elements = D_shelter_b1_main_corridor_80184F80 }, D_shelter_b1_main_corridor_80185110, NULL },
};

WorldCoordPointLight D_shelter_b1_main_corridor_801851A0[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2129, -1643, -681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 88, -1643, -6981 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2129, -1643, -0x2CF5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -73, -1489, -0x40FD } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2197, 2212, 2233 }, { 0, 0 } }, 2944, 8822 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2366, -1643, -0x2CF5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2366, -1643, -681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
};

WorldCoordRoomLights D_shelter_b1_main_corridor_801853E0 = { 0, NULL, ARRAY_SIZE(D_shelter_b1_main_corridor_801851A0), D_shelter_b1_main_corridor_801851A0, 0, NULL };

WorldCollisionTrigger D_shelter_b1_main_corridor_801853F8[16] = {
    { NULL, NULL, NULL, { 64, -1568, -0x2EE0, 0 }, { { -2316, -3712, 337, 0 }, { 2305, -3712, -348, 0 }, { -2316, 3712, 337, 0 }, { 2305, 3712, -348, 0 } }, { -603, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -32, -1696, -0x2F5D, 0 }, { { 2562, -3712, -382, 0 }, { -2565, -3712, 379, 0 }, { 2562, 3712, -382, 0 }, { -2565, 3712, 379, 0 } }, { 601, 0, 4055, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 128, -1568, -7200, 0 }, { { 2592, -3712, 128, 0 }, { -2592, -3712, -128, 0 }, { 2592, 3712, 128, 0 }, { -2592, 3712, -128, 0 } }, { -203, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 112, -1664, -7056, 0 }, { { -2640, -3712, -80, 0 }, { 2640, -3712, 80, 0 }, { -2640, 3712, -80, 0 }, { 2640, 3712, 80, 0 } }, { 124, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1664, -3456, 0 }, { { -2640, -3712, -80, 0 }, { 2640, -3712, 80, 0 }, { -2640, 3712, -80, 0 }, { 2640, 3712, 80, 0 } }, { 124, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1536, -3616, 0 }, { { 2592, -3712, 128, 0 }, { -2592, -3712, -128, 0 }, { 2592, 3712, 128, 0 }, { -2592, 3712, -128, 0 } }, { -203, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2432, -1632, -9952, 0 }, { { -432, -3712, -2106, 0 }, { 424, -3712, 2099, 0 }, { -432, 3712, -2106, 0 }, { 424, 3712, 2099, 0 } }, { 4018, 0, -819, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2576, -1632, -9936, 0 }, { { 317, -3712, 1745, 0 }, { -329, -3712, -1755, 0 }, { 317, 3712, 1745, 0 }, { -329, 3712, -1755, 0 } }, { -4037, 0, 744, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2208, -1568, -0x2740, 0 }, { { -544, -3712, 1691, 0 }, { 536, -3712, -1700, 0 }, { -544, 3712, 1691, 0 }, { 536, 3712, -1700, 0 } }, { -3911, 0, -1246, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2337, -1600, -0x27A1, 0 }, { { 466, -3712, -1686, 0 }, { -475, -3712, 1674, 0 }, { 466, 3712, -1686, 0 }, { -475, 3712, 1674, 0 } }, { 3950, 0, 1106, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1568, -1600, -1776, 0 }, { { 48, -3712, -1472, 0 }, { -48, -3712, 1472, 0 }, { 48, 3712, -1472, 0 }, { -48, 3712, 1472, 0 } }, { 4107, 0, 133, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1664, -1568, -1824, 0 }, { { -58, -3712, 1401, 0 }, { 44, -3712, -1413, 0 }, { -58, 3712, 1401, 0 }, { 44, 3712, -1413, 0 } }, { -4101, 0, -149, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2144, -1536, -1952, 0 }, { { 206, -3712, 1372, 0 }, { -244, -3712, -1407, 0 }, { 206, 3712, 1372, 0 }, { -244, 3712, -1407, 0 } }, { -4050, 0, 655, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 10, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2272, -1536, -1920, 0 }, { { -195, -3712, -1487, 0 }, { 156, -3712, 1434, 0 }, { -195, 3712, -1487, 0 }, { 156, 3712, 1434, 0 } }, { 4075, 0, -491, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 8, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1696, -0x3C61, 0 }, { { -2293, -3712, 454, 0 }, { 2289, -3712, -457, 0 }, { -2293, 3712, 454, 0 }, { 2289, 3712, -457, 0 } }, { -802, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1696, -0x3D21, 0 }, { { 2538, -3712, -511, 0 }, { -2546, -3712, 502, 0 }, { 2538, 3712, -511, 0 }, { -2546, 3712, 502, 0 } }, { 801, 0, 4021, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_main_corridor_801858B8[6] = {
    { NULL, NULL, NULL, { 0, -48, -0x4990, 0 }, { { -1024, 0, -560, 0 }, { 1024, 0, -560, 0 }, { -1024, 0, 560, 0 }, { 1024, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3488, -48, -9984, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, -48, -9952, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3456, -48, -2016, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 24, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, -48, -2048, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -48, 0, 0 }, { { -1024, 0, -560, 0 }, { 1024, 0, -560, 0 }, { -1024, 0, 560, 0 }, { 1024, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 16, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_main_corridor_80185A80[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_main_corridor_80185AA4[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_main_corridor_80185ABC[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_main_corridor_80185AD4[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_main_corridor_80185AEC[3] = {
    { 56, 56, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801482C0 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_main_corridor_80185B10[6] = {
    { 21, 34, 2048, -3600, -1750, -7800, 1024, 0, 0, 2, 2 },
    { 21, 4, 0, 1100, -2100, -0x3C8C, 0, 0, 0, 2, 3 },
    { 21, 4, 0, -1100, -2100, -0x3C8C, 0, 0, 0, 2, 3 },
    { 11, 1, 0, 0, 0, -2000, 2048, 0, 2, 4, 0 },
    { 11, 1, 0, 0, 0, -0x2710, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185B70[4] = {
    { 6, 0, 1, 0, 0, -4000, 0, 0, 0, 2, 0 },
    { 6, 0, 1, 0, 0, -8500, 2048, 0, 0, 2, 0 },
    { 6, 0, 1, 0, 0, -0x38A4, 2048, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185BB0[3] = {
    { 3, 0, 0, 0, 0, -3000, 2048, 0, 0, 2, 0 },
    { 3, 0, 1, 0, 0, -0x2710, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185BE0[2] = {
    { 22, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185C00[3] = {
    { 56, 6, 1, 0, 0, -2000, 2048, 0, 0, 2, 0 },
    { 57, 4, 1, -2000, 0, -0x2710, 1024, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_main_corridor_80185C30[22] = {
    { NULL, NULL },
    { D_shelter_b1_main_corridor_80185B10, D_shelter_b1_main_corridor_80185A80 },
    { D_shelter_b1_main_corridor_80185B70, D_shelter_b1_main_corridor_80185AA4 },
    { D_shelter_b1_main_corridor_80185BB0, D_shelter_b1_main_corridor_80185ABC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_main_corridor_80185BE0, D_shelter_b1_main_corridor_80185AD4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_main_corridor_80185C00, D_shelter_b1_main_corridor_80185AEC },
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

WorldCollisionFootstepSounds D_shelter_b1_main_corridor_80185CE0 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b1_main_corridor_80185CEC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_main_corridor_80185CF4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_main_corridor_80185CFC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_main_corridor_80185CE0 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_main_corridor_80185D04[8] = {
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CF4,
    D_shelter_b1_main_corridor_80185CFC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventStartStorage gRoomEventActive = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b1_main_corridor_80185D44[4] = {
    0,
    7,
    132,
    0,
};

RoomEventReqStorage gRoomEventReq;

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 _corridorStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_b1_main_corridor_8017DD4C(Task* task);
static void           func_shelter_b1_main_corridor_8017DD90(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->queryOnly` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _corridorStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_main_corridor_80185D44[0] = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_main_corridor_80183098, 0, 0, 0);
            D_shelter_b1_main_corridor_80185D44[0] = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Messages 0xD, 0xE, 0x10 and 0x19 start the room's events on
/// flags 0xEE, 0xEF, 0x12C and 0x12D; 0x18 does the same on flag 0x12E once
/// nibble 0xAC is set, and before that runs CAP command 1. Message 9 runs CAP
/// command 5 once nibble 0x7A reaches 6, and otherwise goes through the rooms'
/// event gate on flag 0xA5. Any other message answers 1.
s32 func_shelter_b1_main_corridor_8017DA8C(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B1_ARMORY) {
        event.capCmd   = 3;
        event.stageSnd = 0x540F0001;
        event.flagId   = GAME_FLAG_B1_CORRIDOR_TO_ARMORY_SCENE;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS) {
        event.capCmd   = 4;
        event.stageSnd = 0x540F0001;
        event.flagId   = GAME_FLAG_B1_CORRIDOR_TO_QUARTERS_SCENE;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_STERILIZATION_ROOM) {
        event.capCmd   = 6;
        event.stageSnd = 0x540F0001;
        event.flagId   = GAME_FLAG_B1_CORRIDOR_TO_STERILIZATION_SCENE;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_ELEVATOR_HALL) {
        if (GameFlag_GetNibble(GAME_FLAG_STORY_CHAPTER) >= 6) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd1(5);
            }
            return 0;
        }
        req.capCmd        = 2;
        req.missingCapCmd = 1;
        req.firstSnd      = 0;
        req.secondSnd     = 0x540F0001;
        req.flagId        = GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED;
        req.collectedBit  = 0;
        return roomEventGate(&req, out);
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL) {
        if (GameFlag_GetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(1);
            }
            return 2;
        }
        event.capCmd   = 8;
        event.stageSnd = 0x540F0001;
        event.flagId   = GAME_FLAG_B1_CORRIDOR_TO_TRANSFER_SCENE;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL) {
        event.capCmd   = 7;
        event.stageSnd = 0x540F0001;
        event.flagId   = GAME_FLAG_B1_CORRIDOR_TO_CONTROL_TUNNEL_SCENE;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_main_corridor_8017DCEC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCF4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCFC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DD04(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 sndId;

    if (arg2 != 0xD) {
        if (arg2 == 0xE) {
            sndId = 0x540F0000 | 0xE;
            goto play;
        }
    } else {
        sndId = 0x540F000D;
    play:
        SndEvt_EnqueueType6(sndId, 0, 0);
    }
    return 0;
}

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7 and steps it to its next state.
static void func_shelter_b1_main_corridor_8017DD4C(Task* task)
{
    task->msgTable = D_shelter_b1_main_corridor_801830A4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room's message task.
static void func_shelter_b1_main_corridor_8017DD90(Task* task)
{
}

/// States of the room's message task: install the message table, idle, die.
static const TaskFuncTable3 D_shelter_b1_main_corridor_8017D5F0 = {
    { func_shelter_b1_main_corridor_8017DD4C, func_shelter_b1_main_corridor_8017DD90, taskKill },
};

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b1_main_corridor_8017DD98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_main_corridor_8017D5F0;
    sp.funcs[task->state](task);
}

/// Per-frame drawing task: on its first tick it sets seven gameplay effect ids,
/// then every tick draws the capsules and sprites of whichever camera view is
/// active (views 2-10).
void func_shelter_b1_main_corridor_8017DDF0(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B1_MAIN_CORRIDOR_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B1_MAIN_CORRIDOR_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_MAIN_CORRIDOR_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_MAIN_CORRIDOR_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B1_MAIN_CORRIDOR_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_MAIN_CORRIDOR_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_MAIN_CORRIDOR_SPARK_BURST;
        arg0->state               = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_801830D4;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[2], 0x200, 0x800, 0x111);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183114;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x10);
            glowDrawBeam(&p[4], 0x200, 0, 0x10);
            glowDrawFlareClipped(&p[10], 1, 0x300);
            glowDrawFlareClipped(&p[11], 1, 0x300);
            glowDrawFlareClipped(&p[17], 1, 0x300);
            glowDrawFlareClipped(&p[18], 1, 0x300);
            break;
        }
        case 5:
            glowDrawBeam(D_shelter_b1_main_corridor_80183134, 0x200, 0, 0x10);
            break;
        case 6:
            glowDrawBeam(D_shelter_b1_main_corridor_80183114, 0x200, 0x800, 0x10);
            break;
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183124;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x10);
            glowDrawBeam(&p[4], 0x200, 0, 0x10);
            glowDrawBeam(&p[6], 0x200, 0x800, 0x100);
            glowDrawFlareClipped(&p[13], 1, 0x300);
            glowDrawFlareClipped(&p[14], 1, 0x300);
            glowDrawFlareClipped(&p[20], 1, 0x300);
            glowDrawFlareClipped(&p[21], 1, 0x300);
            break;
        }
        case 8: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183124;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x10);
            glowDrawBeam(&p[6], 0x200, 0x800, 0x100);
            break;
        }
        case 9:
            glowDrawBeam(D_shelter_b1_main_corridor_80183144, 0x200, 0, 0x10);
            break;
        case 10:
            glowDrawBeam(D_shelter_b1_main_corridor_80183124, 0x200, 0x800, 0x10);
            break;
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_flare_clipped.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b1_main_corridor_8017EAD4(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b1_main_corridor_8017F81C(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b1_main_corridor_8017FBB4(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b1_main_corridor_80180FC4(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_main_corridor_801810F8(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_main_corridor_80181B5C(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_main_corridor_80182444(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
