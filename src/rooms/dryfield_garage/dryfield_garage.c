#include "rooms/dryfield_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/garage.h"

extern RoomEventActiveBytes gRoomEventActive;

extern TaskDesc D_actor_120300_80141B6C[];

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's message table, installed on the room task by its entry state.
extern TaskMessageEntry D_dryfield_garage_8017DC7C[];

/// Spawn table of the task `func_dryfield_garage_8017DAA0`, ended by a 0xFFFF
/// entry.
extern TaskDesc D_dryfield_garage_8017DCAC[];

extern ActorTransform        D_dryfield_garage_8017DCC4;
extern WorldCollisionTrigger D_dryfield_garage_8017FD1C[11];

/// Storage for the task of the scene that turns the player back from the
/// junk-yard door.
///
/// While the water-tank scene has not been seen, a warp request for the junk
/// yard is refused and a scene task of the Gary Douglas actor loaded with
/// the room is spawned in its place; `task` receives that spawn's result. The
/// room only stores it. The four bytes after it are zero in the image and
/// have no recovered access; whether they are a second member or a separate
/// unreferenced variable is unproven.
typedef struct {
    Task* task;       // Scene task last spawned for a refused junk-yard warp; NULL before the first
    u8    unknown[4]; // Role unproven; zero, with no recovered access
} _DryfieldGarageDoorSceneStorage;
STATIC_ASSERT_SIZEOF(_DryfieldGarageDoorSceneStorage, 8);

extern _DryfieldGarageDoorSceneStorage D_dryfield_garage_8018021C;

static void func_dryfield_garage_8017DB18(Task* arg0);
static void func_dryfield_garage_8017DC08(Task* task);

extern TaskDesc Actor00100_D1BA84;

extern WorldCollisionGrid         D_dryfield_garage_8017E64C[1];
extern WorldCollisionTrigger      D_dryfield_garage_8017F69C[14];
extern WorldCollisionTrigger      D_dryfield_garage_8017FD1C[11];
extern WorldCoordRoomAmbientEntry D_dryfield_garage_80180148[16];
extern WorldCoordRoomLights       D_dryfield_garage_8017FD04[1];
s32                               func_dryfield_garage_8017D914(Task*, s32, s32, s32);
s32                               func_dryfield_garage_8017D91C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                               func_dryfield_garage_8017DA18(Task*, s32, s32, s32);
s32                               func_dryfield_garage_8017DA54(Task*, s32, RoomEventMsg*, s32);
void                              func_dryfield_garage_8017DAA0(Task*);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_garage_8017DC7C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_garage_8017D91C },
    { 5105, func_dryfield_garage_8017D914 },
    { ROOM_MESSAGE_SOUND, garageSoundMsg },
    { ROOM_MESSAGE_COMMAND, func_dryfield_garage_8017DA18 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_garage_8017DA54 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_garage_8017DCAC[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_garage_8017DAA0, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

ActorTransform D_dryfield_garage_8017DCC4 = { { 1680, 0, 6170, 0 }, { 0, 2048, 0, 0 } };

WorldCollisionRoomResources D_dryfield_garage_8017DCDC[1] = {
    { D_dryfield_garage_8017E64C, D_dryfield_garage_8017F69C, D_dryfield_garage_8017FD1C, NULL },
};

u8* D_dryfield_garage_8017DCEC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_garage_8017DCF0[1] = { 15 };

WorldCoordRoomLighting D_dryfield_garage_8017DCF4[1] = {
    { D_dryfield_garage_8017FD04, D_dryfield_garage_80180148 },
};

DirectionWarpEntry D_dryfield_garage_8017DCFC[2] = {
    { { { .word = 1024 }, 853, 0, 2741 }, { 0, 0, 0, 0 }, { { .word = 3584 }, 940, 0, 2200 }, { 0, 0, 0, 0 }, 0x52180004, 0x52180003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, 473 },
    { { { .word = 2048 }, 950, 0, 7300 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 1100, 0, 7300 }, { 0, 0, 0, 0 }, 0x52180002, 0x52180001, 0x52180007, 6, DIRECTION_WARP_FLAG_NONE, 472 },
};

SVECTOR gDryfieldGarageCollision0108CNormals[39] = {
#include "assets/dryfield_garage_collision_0108C_normals.inc"
};

SVECTOR gDryfieldGarageCollision0108CVerts[106] = {
#include "assets/dryfield_garage_collision_0108C_verts.inc"
};

WorldCollisionGridFace gDryfieldGarageCollision0108CFaces[54] = {
#include "assets/dryfield_garage_collision_0108C_faces.inc"
};

static s16 _gDryfieldGarageCollision0108CCells[214] = {
#include "assets/dryfield_garage_collision_0108C_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldGarageCollision0108CCells[i])
static s16* _gDryfieldGarageCollision0108CTable[9] = {
#include "assets/dryfield_garage_collision_0108C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_garage_8017E64C[1] = {
    { NULL, gDryfieldGarageCollision0108CNormals, gDryfieldGarageCollision0108CVerts, gDryfieldGarageCollision0108CFaces, _gDryfieldGarageCollision0108CTable, 150, 0, 3, 3, 4000, 54 },
};

ViewCamera D_dryfield_garage_8017E670[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5508, 0x33C2, -3974 } }, 257 },
    { { { { 3382, 0, 2309 }, { 756, 3870, -1107 }, { -2182, 1340, 3196 } }, { -3863, 2161, -312 } }, 257 },
    { { { { 1352, 0, 3866 }, { 657, 4036, -230 }, { -3809, 696, 1332 } }, { -0x294D, 1921, -1936 } }, 257 },
    { { { { 4042, 0, 661 }, { 142, 3999, -870 }, { -646, 882, 3947 } }, { -9784, 1851, -1821 } }, 257 },
    { { { { 2349, 0, 3354 }, { 3299, 742, -2310 }, { -607, 4028, 425 } }, { -9916, 5826, -2247 } }, 257 },
    { { { { 3533, 0, -2071 }, { -1751, 2188, -2987 }, { 1106, 3462, 1887 } }, { -997, 4246, -3702 } }, 257 },
    { { { { 1536, 0, 3796 }, { 0, 4096, 0 }, { -3796, 0, 1536 } }, { -4981, 1166, -1302 } }, 348 },
    { { { { -525, 0, -4062 }, { -354, 4080, 45 }, { 4046, 357, -523 } }, { -2448, 1577, -2001 } }, 269 },
    { { { { 3882, 0, -1306 }, { -40, 4093, -121 }, { 1305, 127, 3880 } }, { -3150, 1402, -251 } }, 289 },
    { { { { 2526, 0, -3224 }, { -214, 4086, -167 }, { 3216, 272, 2520 } }, { -1837, 1404, -858 } }, 259 },
    { { { { 2108, 0, 3511 }, { -16, 4095, 9 }, { -3511, -19, 2108 } }, { -4394, 1394, -628 } }, 541 },
    { { { { 1810, 0, 3674 }, { 1435, 3770, -707 }, { -3382, 1600, 1666 } }, { -5513, 2036, -1107 } }, 312 },
    { { { { 4005, 0, -857 }, { 90, 4072, 425 }, { 852, -434, 3982 } }, { -2588, 997, -330 } }, 289 },
    { { { { 2567, 0, -3191 }, { -300, 4077, -241 }, { 3177, 385, 2556 } }, { -2689, 1639, -896 } }, 447 },
    { { { { 1971, 0, 3590 }, { 498, 4056, -274 }, { -3555, 569, 1952 } }, { -6690, 1477, -1070 } }, 257 },
};

SpriteBatch D_dryfield_garage_8017E88C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017E89C[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 16, 705, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 56, 965, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, -64, 1117, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -48, 1377, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 104, -24, 1037, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -32, 1109, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -32, 1019, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 921, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 8, 1021, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 16, 673, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 16, 687, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 16, 714, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 104, 517, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 96, 548, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 572, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 88, 572, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 120, 96, 540, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 40, 613, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 96, 80, 599, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 88, 567, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 597, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 72, 630, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 96, 668, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 104, 695, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, 64, 667, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 32, 661, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 0, 635, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 48, 56, 677, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 40, 602, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 32, 639, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 72, 628, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 24, 676, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -24, 858, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -16, 795, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 24, 716, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 56, -16, 760, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -8, 710, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, 40, 800, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 56, 800, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 32, 790, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 756, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017EBD0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 37, 0, 0, { 1, 0 } },
    { 37, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017EBF0[28] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 905, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 48, 903, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 64, 900, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 56, 861, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 64, 850, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 64, 871, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 128, 24, 837, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 24, 827, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 32, 800, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 800, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, 40, 762, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -40, 1975, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 1925, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, -32, 1822, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -32, 1786, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, -24, 2000, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, -24, 1875, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, -24, 1850, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -16, 1750, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 0, 1825, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 56 } }, -64, -16, 1650, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, -16, 1862, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 16, -16, 1769, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 8, 1600, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 16, 1562, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 16, 1575, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 16, 1600, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 32, 1525, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017EE20[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 3, 0, 0, { 3, 0 } },
    { 6, 5, 0, 0, { 2, 0 } },
    { 11, 12, 0, 0, { 4, 0 } },
    { 23, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017EE58[11] = {
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, 16, 798, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 16, 784, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -104, 0, 854, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -48, 0, 854, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, 16, 820, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -96, 16, 824, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, 16, 789, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 8, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 0, 897, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 8, 837, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 837, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017EF34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017EF4C[12] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -48, 1405, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, -40, 1407, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, -56, 1405, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -56, 1407, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -136, -48, 1392, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -80, 1417, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -80, 1415, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -80, 1416, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, -64, 1416, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -104, 1250, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -80, 1425, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 48, -112, 1175, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017F03C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 1, 0, 0, { 2, 0 } },
    { 5, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017F064[13] = {
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, -64, 1023, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 144, -48, 950, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 120, -40, 975, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, -24, 1036, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 80, -16, 1026, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 64, -64, 1150, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 72 } }, 72, 48, 950, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 136 } }, -32, -88, 1275, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 64, -80, 934, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -80, 1000, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -40, 1000, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, 64, -112, 1017, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 24 } }, -8, -112, 1226, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017F168[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017F180[25] = {
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 40, -16, 600, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 112, 399, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 96, 490, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 40, 80, 550, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 104, 429, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, -16, 653, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -16, 682, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 96, 16, 538, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 96, 457, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 96, 456, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 88, 96, 463, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 410, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 104, 429, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 104, 464, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, 72, 602, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 72, 605, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 88, 529, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 104, 441, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 64, -16, 626, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 48, 570, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 128, 16, 462, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 80, 510, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 80, 550, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 88, 509, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 88, 503, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017F374[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F38C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F39C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F3AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F3BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F3CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F3DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_garage_8017F3EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_garage_8017F3FC[23] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, -48, 1091, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, -48, 1231, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, -48, 1326, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -40, 1243, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -40, 1099, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -40, -24, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 16, 1026, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, -24, 1037, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -32, -16, 970, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -16, 0, 928, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, -32, 1276, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, -24, 1229, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 56 } }, 8, 0, 807, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 56 } }, 88, 0, 922, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 88, 56, 817, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, 8, 56, 975, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -16, -16, 932, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -16, 820, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -16, 966, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 72, -16, 857, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 64, 725, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 64, 712, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 64, 700, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_garage_8017F5C8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_garage_8017F5E8[15] = {
    { { .empty = D_dryfield_garage_8017E88C }, D_dryfield_garage_8017E88C, NULL },
    { { .elements = D_dryfield_garage_8017E89C }, D_dryfield_garage_8017EBD0, NULL },
    { { .elements = D_dryfield_garage_8017EBF0 }, D_dryfield_garage_8017EE20, NULL },
    { { .elements = D_dryfield_garage_8017EE58 }, D_dryfield_garage_8017EF34, NULL },
    { { .elements = D_dryfield_garage_8017EF4C }, D_dryfield_garage_8017F03C, NULL },
    { { .elements = D_dryfield_garage_8017F064 }, D_dryfield_garage_8017F168, NULL },
    { { .elements = D_dryfield_garage_8017F180 }, D_dryfield_garage_8017F374, NULL },
    { { .empty = D_dryfield_garage_8017F38C }, D_dryfield_garage_8017F38C, NULL },
    { { .empty = D_dryfield_garage_8017F39C }, D_dryfield_garage_8017F39C, NULL },
    { { .empty = D_dryfield_garage_8017F3AC }, D_dryfield_garage_8017F3AC, NULL },
    { { .empty = D_dryfield_garage_8017F3BC }, D_dryfield_garage_8017F3BC, NULL },
    { { .empty = D_dryfield_garage_8017F3CC }, D_dryfield_garage_8017F3CC, NULL },
    { { .empty = D_dryfield_garage_8017F3DC }, D_dryfield_garage_8017F3DC, NULL },
    { { .empty = D_dryfield_garage_8017F3EC }, D_dryfield_garage_8017F3EC, NULL },
    { { .elements = D_dryfield_garage_8017F3FC }, D_dryfield_garage_8017F5C8, NULL },
};

WorldCollisionTrigger D_dryfield_garage_8017F69C[14] = {
    { NULL, NULL, NULL, { 1455, -1648, 1775, 0 }, { { -1960, -2672, -1625, 0 }, { 1960, -2672, 1624, 0 }, { -1960, 2672, -1625, 0 }, { 1960, 2672, 1624, 0 } }, { 2615, 0, -3157, 0 }, { 0, 0, 4096, 0 }, 3683, 0, 15, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1566, -1584, 1614, 0 }, { { 1960, -2608, 1673, 0 }, { -1960, -2608, -1673, 0 }, { 1960, 2608, 1673, 0 }, { -1960, 2608, -1673, 0 } }, { -2662, 0, 3116, 0 }, { 0, 0, 4096, 0 }, 3665, 0, 2, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 991, -1664, 4894, 0 }, { { 2101, -2688, -1181, 0 }, { -2101, -2688, 1181, 0 }, { 2101, 2688, -1181, 0 }, { -2101, 2688, 1181, 0 } }, { 2010, 0, 3577, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1024, -1600, 5023, 0 }, { { -2101, -2624, 1181, 0 }, { 2101, -2624, -1181, 0 }, { -2101, 2624, 1181, 0 }, { 2101, 2624, -1181, 0 } }, { -2011, 0, -3577, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7827, -1552, 5066, 0 }, { { -435, -2576, 52, 0 }, { 436, -2576, -51, 0 }, { -435, 2576, 52, 0 }, { 436, 2576, -51, 0 } }, { -484, 0, -4077, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7817, -1760, 4954, 0 }, { { 491, -2784, -59, 0 }, { -490, -2784, 60, 0 }, { 491, 2784, -59, 0 }, { -490, 2784, 60, 0 } }, { 491, 0, 4066, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7326, -1536, 2718, 0 }, { { 1076, -2560, 2157, 0 }, { -1076, -2560, -2156, 0 }, { 1076, 2560, 2157, 0 }, { -1076, 2560, -2156, 0 } }, { -3672, 0, 1831, 0 }, { 0, 0, 4096, 0 }, 3510, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7199, -1728, 2782, 0 }, { { -1076, -2752, -2156, 0 }, { 1076, -2752, 2157, 0 }, { -1076, 2752, -2156, 0 }, { 1076, 2752, 2157, 0 } }, { 3664, 0, -1829, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9680, -1840, 4735, 0 }, { { -1379, -2864, 332, 0 }, { 1379, -2864, -332, 0 }, { -1379, 2864, 332, 0 }, { 1379, 2864, -332, 0 } }, { -960, 0, -3984, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9664, -1792, 4640, 0 }, { { 1379, -2816, -332, 0 }, { -1379, -2816, 332, 0 }, { 1379, 2816, -332, 0 }, { -1379, 2816, 332, 0 } }, { 958, 0, 3982, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6315, -1792, 6411, 0 }, { { 1136, -2784, -1556, 0 }, { -1136, -2784, 1556, 0 }, { 1136, 2784, -1556, 0 }, { -1136, 2784, 1556, 0 } }, { 3318, 0, 2422, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6427, -1760, 6490, 0 }, { { -1047, -2784, 1415, 0 }, { 1047, -2784, -1414, 0 }, { -1047, 2784, 1415, 0 }, { 1047, 2784, -1414, 0 } }, { -3299, 0, -2442, 0 }, { 0, 0, 4096, 0 }, 3288, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4465, -1664, 1695, 0 }, { { 148, -2672, -2050, 0 }, { -163, -2672, 2041, 0 }, { 148, 2672, -2050, 0 }, { -163, 2672, 2041, 0 } }, { 4088, 0, 310, 0 }, { 0, 0, 4096, 0 }, 3367, 0, 3, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4592, -1760, 1648, 0 }, { { -175, -2672, 1978, 0 }, { 162, -2672, -1987, 0 }, { -175, 2672, 1978, 0 }, { 162, 2672, -1987, 0 } }, { -4098, 0, -349, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 15, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_garage_8017FAC4[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3565, -1012, 1211 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 642, 2848 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7335, -1012, 1897 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 564, 2686 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2845, -1012, 4145 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 562, 1281 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5512, -5017, 2053 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2375, 2457, 2457 }, { 0, 0 } }, 0, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2367, -1012, 6804 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 441, 1201 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6622, -1012, 6746 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 522, 1542 },
};

WorldCoordRoomLights D_dryfield_garage_8017FD04[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_garage_8017FAC4), D_dryfield_garage_8017FAC4, 0, NULL },
};

WorldCollisionTrigger D_dryfield_garage_8017FD1C[11] = {
    { NULL, NULL, NULL, { 3552, -64, 2384, 0 }, { { -1728, 0, -1392, 0 }, { 2048, 0, -1392, 0 }, { -1792, 0, 944, 0 }, { 2016, 0, 944, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, 4096, 0 }, 2468, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, -48, 7376, 0 }, { { -976, 0, -256, 0 }, { 976, 0, -256, 0 }, { -976, 0, 256, 0 }, { 976, 0, 256, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1007, WORLD_COLLISION_TRIGGER_ACTION_WARP, 26, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 608, -48, 2960, 0 }, { { 272, 0, -976, 0 }, { 272, 0, 976, 0 }, { -272, 0, -976, 0 }, { -272, 0, 976, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1011, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2192, -64, 5344, 0 }, { { -512, 0, -2880, 0 }, { 512, 0, -2880, 0 }, { -512, 0, 2880, 0 }, { 512, 0, 2880, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 2918, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5024, -64, 5312, 0 }, { { -512, 0, -2880, 0 }, { 512, 0, -2880, 0 }, { -512, 0, 2880, 0 }, { 512, 0, 2880, 0 } }, { 0, 4104, 0, 0 }, { 4091, 0, -201, 0 }, 2918, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9600, -64, 6368, 0 }, { { 688, 0, -1552, 0 }, { 688, 0, 1552, 0 }, { -688, 0, -1552, 0 }, { -688, 0, 1552, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 1693, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 16, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9680, -64, 1696, 0 }, { { 1088, 0, -624, 0 }, { 1088, 0, 624, 0 }, { -1088, 0, -624, 0 }, { -1088, 0, 624, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8335, -64, 4895, 0 }, { { 23, 0, -1493, 0 }, { 1494, 0, -22, 0 }, { -1493, 0, 23, 0 }, { -22, 0, 1494, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 976, -64, 1792, 0 }, { { 784, 0, -720, 0 }, { 784, 0, 48, 0 }, { -784, 0, -48, 0 }, { -784, 0, 720, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6752, -64, 1312, 0 }, { { 1088, 0, -464, 0 }, { 1088, 0, 464, 0 }, { -1088, 0, -464, 0 }, { -1088, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3624, -64, 2944, 0 }, { { -1864, 0, -1552, 0 }, { 1912, 0, -1552, 0 }, { -1928, 0, 1008, 0 }, { 1880, 0, 1008, 0 } }, { 0, 4119, 0, 0 }, { 201, 0, -4091, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_garage_80180060[2] = {
    { 106, 203, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_120300_80141B6C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_garage_80180078[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_garage_80180090[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_garage_801800A8[2] = {
    { 75, 75, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_garage_801800C0[2] = {
    { 75, 0, 0, 4676, 0, 2038, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_garage_801800E0[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B724, D_dryfield_garage_80180060 },
    { D_map_dryfield_8017B744, D_dryfield_garage_80180078 },
    { D_map_dryfield_8017B7A4, D_dryfield_garage_80180090 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_garage_801800C0, D_dryfield_garage_801800A8 },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_garage_80180148[16] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_garage_80180148) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 200, 200, 200, 200 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 500, 500, 500, 500 } },
    { .color = { 350, 350, 350, 350 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 560, 560, 560, 560 } },
};

WorldCollisionFootstepSounds D_dryfield_garage_801801C8 = {
    0x10000051,
    0x10000053,
    0x10000051,
};

WorldCollisionSurfaceProperties D_dryfield_garage_801801D4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_garage_801801DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_garage_801801C8 },
};

WorldCollisionSurfaceProperties* D_dryfield_garage_801801E4[8] = {
    D_dryfield_garage_801801D4,
    D_dryfield_garage_801801DC,
    D_dryfield_garage_801801D4,
    D_dryfield_garage_801801D4,
    D_dryfield_garage_801801D4,
    D_dryfield_garage_801801D4,
    D_dryfield_garage_801801D4,
    D_dryfield_garage_801801D4,
};

AreaApplyRec D_dryfield_garage_80180204[6] = {
    { 2, 2, 2, 1 },
    { 2, 11, 3, 1 },
    { 2, 15, 2, 1 },
    { 2, 29, 2, 17 },
    { 2, 29, 7, 33 },
    { 255, 0, 0, 0 },
};

_DryfieldGarageDoorSceneStorage D_dryfield_garage_8018021C = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 222, 221, 253 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_garage_8017DC10`: the entry state
/// `func_dryfield_garage_8017DB18`, the idle state
/// `func_dryfield_garage_8017DC08`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_garage_8017D5DC = {
    { func_dryfield_garage_8017DB18, func_dryfield_garage_8017DC08, taskKill },
};

#include "../../shared/garage_sound_msg.inc.c"

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_garage_8017D914(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`. For area 0x1A it answers 2 while nibble
/// 0x33 is clear, spawning the task of `D_dryfield_garage_8017DCAC` outside a
/// dry run; once 0x33 is set it sets nibble 0x2F (and 0x4B to 3) the first
/// time. For area 0x17 it reports nibble 0x47 in `out->room`, 1 when clear
/// and 2 when set. Otherwise it answers 1.
s32 func_dryfield_garage_8017D91C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    // Never touched, but its stack slot is load-bearing: `expand_decl` gives
    // every BLKmode local a frame slot whether or not anything reads it, and
    // MIPS_STACK_ALIGN(0x14) is what puts the saves at 0x28 and the frame at
    // 0x38. Dropping it shrinks the frame to 0x20 and the overlay stops
    // matching. See DECOMPILATION_LEARNINGS.md, "A frame 24 bytes too small is
    // a dead aggregate local".
    RoomEventReq req;
    s32          nib;

    *out = *in;
    if (in->areaId == GAME_AREA_DRYFIELD_JUNK_YARD) {
        if (GameFlag_GetNibble(GAME_FLAG_WATER_TANK_SCENE_SEEN) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Task_SpawnFromTable(D_dryfield_garage_8017DCAC, 0, 0, 0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(GAME_FLAG_GARAGE_JUNK_YARD_DOOR_PASSED) == 0) {
            GameFlag_SetNibble(GAME_FLAG_GARAGE_JUNK_YARD_DOOR_PASSED, 1);
            GameFlag_SetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 3);
        }
    }
    if (in->areaId == GAME_AREA_DRYFIELD_FACTORY) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            nib = GameFlag_GetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED);
            if (nib == 0) {
                nib = 1;
            } else {
                nib = 2;
            }
            out->room = nib;
        }
    }
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: on event 0x10 it
/// runs cap command 0x16 if nibble 0xFD is set, else 0x10. Always answers 0.
s32 func_dryfield_garage_8017DA18(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x10) {
        Gp_RunCapCmd1(GameFlag_GetNibble(GAME_FLAG_0FD) != 0 ? 0x16 : 0x10);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table: for warp point 2
/// outside place 1 it calls `Gp_SpawnIfCapIdle(0x13, 0)`. It returns no
/// value.
s32 func_dryfield_garage_8017DA54(Task* arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    if ((msg->warp == 2) && (gGameSession->location.loc.variant != 1)) {
        Gp_SpawnIfCapIdle(0x13, 0);
    }
}

/// Task spawned from `D_dryfield_garage_8017DCAC`: spawns the second entry of
/// gameplay's `D_actor_120300_80141B6C`, keeping the task in `D_dryfield_garage_8018021C.task`,
/// then ends itself.
void func_dryfield_garage_8017DAA0(Task* arg0)
{
    Task* spawned;
    s32   state;
    switch (arg0->state) {
        case 0:
            spawned                         = Task_SpawnFromTable(D_actor_120300_80141B6C, 1, 0, 0);
            state                           = arg0->state;
            D_dryfield_garage_8018021C.task = spawned;
            arg0->state                     = state + 1;
            break;
        case 1:
            taskKill(arg0);
            break;
    }
}

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7, sends message 0x3E9 with
/// `D_dryfield_garage_8017DCC4` to the task in pointer slot 0xA when arriving
/// by warp 2, moves nibble 0x155 from 1 to 2 (clearing nibble 3), clears bit 6
/// of `D_dryfield_garage_8017FD1C[0].flags` outside place 1, and advances to
/// the idle state.
static void func_dryfield_garage_8017DB18(Task* arg0)
{
    arg0->msgTable = D_dryfield_garage_8017DC7C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) && (gGameSession->location.loc.warp == 2)) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), 0x3E9, &D_dryfield_garage_8017DCC4, 0);
    }
    if (GameFlag_GetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) == 1) {
        GameFlag_SetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        GameFlag_SetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 2);
    }
    if (gGameSession->location.loc.variant != 1) {
        D_dryfield_garage_8017FD1C[0].flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    arg0->state = arg0->state + 1;
}

/// Idle state of the room task.
static void func_dryfield_garage_8017DC08(Task* task)
{
}

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_garage_8017DC10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_garage_8017D5DC;
    sp.funcs[task->state](task);
}

/// Empty function; nothing in the room references it.
void func_dryfield_garage_8017DC68(Task* unused)
{
}
