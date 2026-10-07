#include "rooms/shelter_1f_heliport_s4.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

/// The room's message table, handed to its event task in state 0.
extern TaskMessageEntry D_shelter_1f_heliport_s4_8017D6D0[];

s32 func_shelter_1f_heliport_s4_8017D5D0(Task*, s32, s32, s32);
s32 func_shelter_1f_heliport_s4_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_1f_heliport_s4_8017D61C(Task*, s32, s32, s32);
s32 func_shelter_1f_heliport_s4_8017D624(Task*, s32, s32, s32);

TaskMessageEntry D_shelter_1f_heliport_s4_8017D6D0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_heliport_s4_8017D5D8 },
    { 5105, func_shelter_1f_heliport_s4_8017D5D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_heliport_s4_8017D624 },
    { ROOM_MESSAGE_COMMAND, func_shelter_1f_heliport_s4_8017D61C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* D_shelter_1f_heliport_s4_8017D6F8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_heliport_s4_8017D6FC[1] = { 5 };

DirectionWarpEntry D_shelter_1f_heliport_s4_8017D700[1] = {
    { { { .word = 0 }, 8000, 0, 1632 }, { 0, 0, 0, 0 }, { { .word = 0 }, 8000, 0, 1632 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelter1fHeliportS4Collision00408Normals[7] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_normals.inc"
};

static SVECTOR _gShelter1fHeliportS4Collision00408Verts[34] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_verts.inc"
};

static WorldCollisionGridFace _gShelter1fHeliportS4Collision00408Faces[15] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_faces.inc"
};

static s16 _gShelter1fHeliportS4Collision00408Cells[56] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fHeliportS4Collision00408Cells[i])
static s16* _gShelter1fHeliportS4Collision00408Table[9] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_1f_heliport_s4_8017D9C8 = { NULL, _gShelter1fHeliportS4Collision00408Normals, _gShelter1fHeliportS4Collision00408Verts, _gShelter1fHeliportS4Collision00408Faces, _gShelter1fHeliportS4Collision00408Table, 0, 2000, 3, 3, 4000, 15 };

ViewCamera D_shelter_1f_heliport_s4_8017D9EC[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x3CE8, -3400 } }, 235 },
    { { { { 600, 0, -4051 }, { -345, 4081, -51 }, { 4037, 348, 598 } }, { -1395, 1503, -3025 } }, 225 },
    { { { { 577, 0, 4055 }, { 345, 4081, -49 }, { -4040, 349, 575 } }, { -8585, 1503, -2595 } }, 225 },
    { { { { 3910, 0, 1218 }, { -32, 4094, 105 }, { -1217, -110, 3909 } }, { -2572, 1124, -1708 } }, 269 },
    { { { { -4049, 0, 618 }, { -54, 4079, -359 }, { -615, -363, -4033 } }, { -8235, 928, -4313 } }, 289 },
};

SpriteBatch D_shelter_1f_heliport_s4_8017DAA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_s4_8017DAB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_s4_8017DAC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_s4_8017DAD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_s4_8017DAE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_1f_heliport_s4_8017DAF0[5] = {
    { { .empty = D_shelter_1f_heliport_s4_8017DAA0 }, D_shelter_1f_heliport_s4_8017DAA0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAB0 }, D_shelter_1f_heliport_s4_8017DAB0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAC0 }, D_shelter_1f_heliport_s4_8017DAC0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAD0 }, D_shelter_1f_heliport_s4_8017DAD0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAE0 }, D_shelter_1f_heliport_s4_8017DAE0, NULL },
};

WorldCoordLight D_shelter_1f_heliport_s4_8017DB2C[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_1f_heliport_s4_8017DC8C[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4500, -2620, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3200, -2159, -922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -766, -2000, -4618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1240, -5082, 2801 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3966, -2581, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights D_shelter_1f_heliport_s4_8017DE6C = { ARRAY_SIZE(D_shelter_1f_heliport_s4_8017DB2C), D_shelter_1f_heliport_s4_8017DB2C, ARRAY_SIZE(D_shelter_1f_heliport_s4_8017DC8C), D_shelter_1f_heliport_s4_8017DC8C, 0, NULL };

WorldCollisionTrigger D_shelter_1f_heliport_s4_8017DE84[6] = {
    { NULL, NULL, NULL, { 5008, -1296, 3536, 0 }, { { 588, -2112, -2769, 0 }, { -611, -2112, 2745, 0 }, { 588, 2112, -2769, 0 }, { -611, 2112, 2745, 0 } }, { 4004, 0, 870, 0 }, { 0, 0, 4096, 0 }, 3519, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5153, -1360, 3536, 0 }, { { -624, -2112, 2765, 0 }, { 610, -2112, -2781, 0 }, { -624, 2112, 2765, 0 }, { 610, 2112, -2781, 0 } }, { -4013, 0, -893, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7968, -1312, 848, 0 }, { { 1504, -2112, 64, 0 }, { -1504, -2112, -64, 0 }, { 1504, 2112, 64, 0 }, { -1504, 2112, -64, 0 } }, { -175, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8080, -1280, 960, 0 }, { { -1632, -2112, -64, 0 }, { 1632, -2112, 64, 0 }, { -1632, 2112, -64, 0 }, { 1632, 2112, 64, 0 } }, { 160, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1455, -1377, 5314, 0 }, { { -2320, -2112, -880, 0 }, { 2320, -2112, 880, 0 }, { -2320, 2112, -880, 0 }, { 2320, 2112, 880, 0 } }, { 1459, 0, -3847, 0 }, { 0, 0, 4096, 0 }, 3258, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1537, -1440, 5217, 0 }, { { 2352, -2112, 928, 0 }, { -2352, -2112, -928, 0 }, { 2352, 2112, 928, 0 }, { -2352, 2112, -928, 0 } }, { -1509, 0, 3823, 0 }, { 0, 0, 4096, 0 }, 3288, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

s32 D_shelter_1f_heliport_s4_8017E04C[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_shelter_1f_heliport_s4_8017E058[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_shelter_1f_heliport_s4_8017E060[8] = {
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
};

static void func_shelter_1f_heliport_s4_8017D62C(Task* task);
static void func_shelter_1f_heliport_s4_8017D670(Task* task);

s32 func_shelter_1f_heliport_s4_8017D5D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, hands both to `mapShelterRoomVariantResolve` and returns 1.
s32 func_shelter_1f_heliport_s4_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    return 1;
}

s32 func_shelter_1f_heliport_s4_8017D61C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_1f_heliport_s4_8017D624(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
static void func_shelter_1f_heliport_s4_8017D62C(Task* task)
{
    task->msgTable = D_shelter_1f_heliport_s4_8017D6D0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_shelter_1f_heliport_s4_8017D670(Task* task)
{
}

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_heliport_s4_8017D5C4 = {
    {
        func_shelter_1f_heliport_s4_8017D62C,
        func_shelter_1f_heliport_s4_8017D670,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_1f_heliport_s4_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_heliport_s4_8017D5C4;
    sp.funcs[task->state](task);
}
