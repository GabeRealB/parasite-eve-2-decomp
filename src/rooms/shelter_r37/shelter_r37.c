#include "rooms/shelter_r37.h"

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
extern TaskMessageEntry D_shelter_r37_8017D6D0[];

s32 func_shelter_r37_8017D5D0(Task*, s32, s32, s32);
s32 func_shelter_r37_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_r37_8017D61C(Task*, s32, s32, s32);
s32 func_shelter_r37_8017D624(Task*, s32, s32, s32);

TaskMessageEntry D_shelter_r37_8017D6D0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_r37_8017D5D8 },
    { 5105, func_shelter_r37_8017D5D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_r37_8017D624 },
    { ROOM_MESSAGE_COMMAND, func_shelter_r37_8017D61C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* D_shelter_r37_8017D6F8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_r37_8017D6FC[1] = { 3 };

DirectionWarpEntry D_shelter_r37_8017D700[1] = {
    { { { .word = 1024 }, 3756, 0, -197 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 3756, 0, -197 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterR37Collision00360Normals[6] = {
#include "assets/shelter_r37_collision_00360_normals.inc"
};

static SVECTOR _gShelterR37Collision00360Verts[22] = {
#include "assets/shelter_r37_collision_00360_verts.inc"
};

static WorldCollisionGridFace _gShelterR37Collision00360Faces[12] = {
#include "assets/shelter_r37_collision_00360_faces.inc"
};

static s16 _gShelterR37Collision00360Cells[48] = {
#include "assets/shelter_r37_collision_00360_cells.inc"
};

#define GRID_CELL(i) (&_gShelterR37Collision00360Cells[i])
static s16* _gShelterR37Collision00360Table[6] = {
#include "assets/shelter_r37_collision_00360_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_r37_8017D920 = { NULL, _gShelterR37Collision00360Normals, _gShelterR37Collision00360Verts, _gShelterR37Collision00360Faces, _gShelterR37Collision00360Table, 4250, 3500, 3, 2, 4000, 12 };

ViewCamera D_shelter_r37_8017D944[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x38D6, 0 } }, 257 },
    { { { { 888, 0, -3998 }, { -122, 4094, -27 }, { 3996, 125, 888 } }, { 2810, 910, 870 } }, 257 },
    { { { { 902, 0, 3995 }, { 3827, 1174, -864 }, { -1145, 3924, 258 } }, { 1110, 5890, 370 } }, 207 },
};

SpriteBatch D_shelter_r37_8017D9B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_r37_8017D9C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_r37_8017D9D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_r37_8017D9E0[3] = {
    { { .empty = D_shelter_r37_8017D9B0 }, D_shelter_r37_8017D9B0, NULL },
    { { .empty = D_shelter_r37_8017D9C0 }, D_shelter_r37_8017D9C0, NULL },
    { { .empty = D_shelter_r37_8017D9D0 }, D_shelter_r37_8017D9D0, NULL },
};

WorldCoordLight D_shelter_r37_8017DA04[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_r37_8017DB64[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4500, -2620, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3200, -2159, -922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -766, -2000, -4618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1240, -5082, 2801 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3966, -2581, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights D_shelter_r37_8017DD44 = { ARRAY_SIZE(D_shelter_r37_8017DA04), D_shelter_r37_8017DA04, ARRAY_SIZE(D_shelter_r37_8017DB64), D_shelter_r37_8017DB64, 0, NULL };

WorldCollisionTrigger D_shelter_r37_8017DD5C[4] = {
    { NULL, NULL, NULL, { 192, -3504, 1359, 0 }, { { 1344, -4304, 3152, 0 }, { -1344, -4304, -3152, 0 }, { 1344, 4304, 3152, 0 }, { -1344, 4304, -3152, 0 } }, { -3775, 0, 1609, 0 }, { 0, 0, 4096, 0 }, 5490, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -176, -3488, 846, 0 }, { { -1072, -4304, -2544, 0 }, { 1072, -4304, 2544, 0 }, { -1072, 4304, -2544, 0 }, { 1072, 4304, 2544, 0 } }, { 3779, 0, -1593, 0 }, { 0, 0, 4096, 0 }, 5094, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4560, -3552, -5312, 0 }, { { -480, -4304, 6912, 0 }, { 480, -4304, -6912, 0 }, { -480, 4304, 6912, 0 }, { 480, 4304, -6912, 0 } }, { -4088, 0, -284, 0 }, { 0, 0, 4096, 0 }, 8143, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4520, -3520, -5568, 0 }, { { 432, -4304, -6784, 0 }, { -432, -4304, 6784, 0 }, { 432, 4304, -6784, 0 }, { -432, 4304, 6784, 0 } }, { 4093, 0, 260, 0 }, { 0, 0, 4096, 0 }, 8030, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_r37_8017DE8C = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_shelter_r37_8017DE98 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_shelter_r37_8017DEA4 = {
    0x10000051,
    0x10000053,
    0x10000051,
};

WorldCollisionSurfaceProperties D_shelter_r37_8017DEB0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_r37_8017DEB8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_r37_8017DE8C },
};

WorldCollisionSurfaceProperties D_shelter_r37_8017DEC0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_r37_8017DE98 },
};

WorldCollisionSurfaceProperties D_shelter_r37_8017DEC8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_r37_8017DEA4 },
};

WorldCollisionSurfaceProperties D_shelter_r37_8017DED0[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_shelter_r37_8017DED8[8] = {
    D_shelter_r37_8017DEB0,
    D_shelter_r37_8017DED0,
    D_shelter_r37_8017DEC0,
    D_shelter_r37_8017DEC8,
    D_shelter_r37_8017DEB0,
    D_shelter_r37_8017DEB8,
    D_shelter_r37_8017DEB0,
    D_shelter_r37_8017DEB0,
};

static void func_shelter_r37_8017D62C(Task* task);
static void func_shelter_r37_8017D670(Task* task);

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_r37_8017D5D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `mapShelterRoomVariantResolve` and returns 1.
s32 func_shelter_r37_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_shelter_r37_8017D61C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_r37_8017D624(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
static void func_shelter_r37_8017D62C(Task* task)
{
    task->msgTable = D_shelter_r37_8017D6D0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_shelter_r37_8017D670(Task* task)
{
}

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_shelter_r37_8017D5C4 = {
    {
        func_shelter_r37_8017D62C,
        func_shelter_r37_8017D670,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_r37_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r37_8017D5C4;
    sp.funcs[task->state](task);
}
