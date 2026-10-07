#include "rooms/dryfield_motel_room_3.h"

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

/// The room's message table, published at `Task::msgTable` by the room task.
extern TaskMessageEntry D_dryfield_motel_room_3_8017D6B4[];

static s32 _dryfieldMotelRoom3RefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom3AcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom3IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

extern WorldCollisionGrid    D_dryfield_motel_room_3_8017DDA0[1];
extern WorldCollisionTrigger D_dryfield_motel_room_3_8017DFC4[11];
extern WorldCollisionTrigger D_dryfield_motel_room_3_8017E308[1];
extern WorldCoordRoomLights  D_dryfield_motel_room_3_8017E4D4[1];

TaskMessageEntry D_dryfield_motel_room_3_8017D6B4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom3AcceptRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom3RefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom3IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom3IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_dryfield_motel_room_3_8017D6DC[1] = {
    { D_dryfield_motel_room_3_8017DDA0, D_dryfield_motel_room_3_8017DFC4, D_dryfield_motel_room_3_8017E308, NULL },
};

u8* D_dryfield_motel_room_3_8017D6EC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_room_3_8017D6F0[1] = { 6 };

WorldCoordRoomLighting D_dryfield_motel_room_3_8017D6F4[1] = {
    { D_dryfield_motel_room_3_8017E4D4, NULL },
};

DirectionWarpEntry D_dryfield_motel_room_3_8017D6FC[1] = {
    { { { .word = 3072 }, 4462, 0, 1067 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4462, 0, 1067 }, { 0, 0, 0, 0 }, 0x520D0002, 0x520D0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 484 },
};

static SVECTOR _gDryfieldMotelRoom3Collision007E0Normals[13] = {
#include "assets/dryfield_motel_room_3_collision_007E0_normals.inc"
};

static SVECTOR _gDryfieldMotelRoom3Collision007E0Verts[103] = {
#include "assets/dryfield_motel_room_3_collision_007E0_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelRoom3Collision007E0Faces[45] = {
#include "assets/dryfield_motel_room_3_collision_007E0_faces.inc"
};

static s16 _gDryfieldMotelRoom3Collision007E0Cells[80] = {
#include "assets/dryfield_motel_room_3_collision_007E0_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom3Collision007E0Cells[i])
static s16* _gDryfieldMotelRoom3Collision007E0Table[4] = {
#include "assets/dryfield_motel_room_3_collision_007E0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_room_3_8017DDA0[1] = {
    { NULL, _gDryfieldMotelRoom3Collision007E0Normals, _gDryfieldMotelRoom3Collision007E0Verts, _gDryfieldMotelRoom3Collision007E0Faces, _gDryfieldMotelRoom3Collision007E0Table, -200, -100, 2, 2, 4000, 45 },
};

ViewCamera D_dryfield_motel_room_3_8017DDC4[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x34BC, -3500 } }, 316 },
    { { { { 654, 0, -4043 }, { 0, 4096, 0 }, { 4043, 0, 654 } }, { -1650, 1050, -700 } }, 275 },
    { { { { 1425, 0, -3840 }, { -296, 4083, -109 }, { 3828, 315, 1420 } }, { -200, 1250, -900 } }, 235 },
    { { { { 3930, 0, -1152 }, { -94, 4082, -324 }, { 1148, 337, 3917 } }, { -1100, 1450, -200 } }, 230 },
    { { { { -3619, 0, 1916 }, { 173, 4079, 327 }, { -1908, 371, -3605 } }, { -3150, 1200, -3450 } }, 275 },
    { { { { 4066, 0, -488 }, { 0, 4096, 0 }, { 488, 0, 4066 } }, { -3800, 950, -2590 } }, 246 },
    { { { { -768, 0, -4023 }, { 313, 4083, -59 }, { 4011, -319, -765 } }, { -200, 750, -6400 } }, 235 },
    { { { { -419, 0, 4074 }, { 2571, 3176, 264 }, { -3160, 2585, -325 } }, { -3200, 2550, -6240 } }, 235 },
};

SpriteBatch D_dryfield_motel_room_3_8017DEE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DEF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DF04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DF14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DF24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DF34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DF44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_3_8017DF54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_room_3_8017DF64[8] = {
    { { .empty = D_dryfield_motel_room_3_8017DEE4 }, D_dryfield_motel_room_3_8017DEE4, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DEF4 }, D_dryfield_motel_room_3_8017DEF4, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF04 }, D_dryfield_motel_room_3_8017DF04, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF14 }, D_dryfield_motel_room_3_8017DF14, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF24 }, D_dryfield_motel_room_3_8017DF24, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF34 }, D_dryfield_motel_room_3_8017DF34, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF44 }, D_dryfield_motel_room_3_8017DF44, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF54 }, D_dryfield_motel_room_3_8017DF54, NULL },
};

WorldCollisionTrigger D_dryfield_motel_room_3_8017DFC4[11] = {
    { NULL, NULL, NULL, { 3519, -1136, 2943, 0 }, { { -989, -2160, -373, 0 }, { 990, -2160, 374, 0 }, { -989, 2160, -373, 0 }, { 990, 2160, 374, 0 } }, { 1453, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3455, -1104, 2815, 0 }, { { 990, -2128, 374, 0 }, { -989, -2128, -373, 0 }, { 990, 2128, 374, 0 }, { -989, 2128, -373, 0 } }, { -1455, 0, 3849, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2591, -1104, 862, 0 }, { { 246, -2128, -669, 0 }, { -245, -2128, 670, 0 }, { 246, 2128, -669, 0 }, { -245, 2128, 670, 0 } }, { 3859, 0, 1414, 0 }, { 0, 0, 4096, 0 }, 2231, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2719, -1120, 831, 0 }, { { -278, -2144, 657, 0 }, { 278, -2144, -657, 0 }, { -278, 2144, 657, 0 }, { 278, 2144, -657, 0 } }, { -3775, 0, -1600, 0 }, { 0, 0, 4096, 0 }, 2246, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4094, -1136, 5086, 0 }, { { -829, -2160, -245, 0 }, { 830, -2160, 246, 0 }, { -829, 2160, -245, 0 }, { 830, 2160, 246, 0 } }, { 1160, 0, -3929, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4063, -1136, 4991, 0 }, { { 830, -2160, 246, 0 }, { -829, -2160, -245, 0 }, { 830, 2160, 246, 0 }, { -829, 2160, -245, 0 } }, { -1163, 0, 3926, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3199, -1104, 5630, 0 }, { { -6, -2128, -865, 0 }, { 6, -2128, 865, 0 }, { -6, 2128, -865, 0 }, { 6, 2128, 865, 0 } }, { 4095, 0, -30, 0 }, { 0, 0, 4096, 0 }, 2289, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3328, -1152, 5631, 0 }, { { 6, -2176, 865, 0 }, { -6, -2176, -865, 0 }, { 6, 2176, 865, 0 }, { -6, 2176, -865, 0 } }, { -4101, 0, 26, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2112, -1120, 5840, 0 }, { { -6, -2144, -977, 0 }, { 6, -2144, 977, 0 }, { -6, 2144, -977, 0 }, { 6, 2144, 977, 0 } }, { 4097, 0, -27, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2272, -1152, 5888, 0 }, { { 6, -2176, 977, 0 }, { -6, -2176, -977, 0 }, { 6, 2176, 977, 0 }, { -6, 2176, -977, 0 } }, { -4122, 0, 23, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4383, -1120, 1056, 0 }, { { 3, -2160, -885, 0 }, { -2, -2160, 886, 0 }, { 3, 2160, -885, 0 }, { -2, 2160, 886, 0 } }, { 4102, 0, 10, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_room_3_8017E308[1] = {
    { NULL, NULL, NULL, { 4528, -48, 1040, 0 }, { { -368, 0, -816, 0 }, { 368, 0, -816, 0 }, { -368, 0, 816, 0 }, { 368, 0, 816, 0 } }, { 0, 4115, 0, 0 }, { -4096, 0, 0, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four white point lights for motel room 3, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength within `inner` and fades
/// to zero at `outer`. The loaded room overlay owns this mutable array:
/// coordinate updates parent and compose its transforms, and lighting queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldMotelRoom3PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 934, -1735, 6756 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2025, 2025, 2025 },
        },
        .inner = 1140,
        .outer = 5440,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 425, -1236, 2352 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2800, 2800, 2800 },
        },
        .inner = 2444,
        .outer = 7346,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4499, -1500, 5898 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2600, 2600, 2600 },
        },
        .inner = 880,
        .outer = 4011,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -1278, 6899 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2900, 2900, 2900 },
        },
        .inner = 987,
        .outer = 2342,
    },
};

WorldCoordRoomLights D_dryfield_motel_room_3_8017E4D4[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelRoom3PointLights), _gDryfieldMotelRoom3PointLights, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_motel_room_3_8017E4EC = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_motel_room_3_8017E4F8 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_3_8017E504[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_3_8017E50C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_3_8017E4EC },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_3_8017E514[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_3_8017E4F8 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_3_8017E51C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_room_3_8017E524[8] = {
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E50C,
    D_dryfield_motel_room_3_8017E514,
    D_dryfield_motel_room_3_8017E51C,
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E504,
};

static void func_dryfield_motel_room_3_8017D610(Task* task);
static void func_dryfield_motel_room_3_8017D654(Task* task);

/// Refuses every key-item-use request in the motel room 3.
///
/// Ignores the collected item ID and other arguments. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, leaving the room and inventory unchanged.
static s32 _dryfieldMotelRoom3RefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition from the motel room 3 with its destination unchanged.
///
/// Borrows a readable eight-byte request and writable reply for synchronous
/// dispatch; they may be the same record. Copies the complete request for both
/// query and execution calls, retains neither pointer, and returns 1.
static s32 _dryfieldMotelRoom3AcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_3_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_3_TRANSITION_ALLOWED;
}

/// Ignores room commands in the motel room 3 and returns zero.
///
/// The command word and other arguments are unused; no room state changes.
static s32 _dryfieldMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores directed room actions in the motel room 3 and returns zero.
///
/// The borrowed action request is neither read nor retained; all arguments
/// are unused and no room state changes.
static s32 _dryfieldMotelRoom3IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_motel_room_3_8017D610(Task* task)
{
    task->msgTable = D_dryfield_motel_room_3_8017D6B4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_motel_room_3_8017D654(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_room_3_8017D5C4 = {
    { func_dryfield_motel_room_3_8017D610, func_dryfield_motel_room_3_8017D654, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_room_3_8017D5C4`.
void func_dryfield_motel_room_3_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_3_8017D5C4;
    sp.funcs[task->state](task);
}
