#include "rooms/dryfield_motel_room_4.h"

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
extern TaskMessageEntry D_dryfield_motel_room_4_8017D6B4[];

static s32 _dryfieldMotelRoom4RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom4ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom4IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldMotelRoom4IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

extern WorldCollisionGrid    D_dryfield_motel_room_4_8017DDF0[1];
extern WorldCollisionTrigger D_dryfield_motel_room_4_8017DF94[8];
extern WorldCollisionTrigger D_dryfield_motel_room_4_8017E1F4[1];
extern WorldCoordRoomLights  D_dryfield_motel_room_4_8017E420[1];

TaskMessageEntry D_dryfield_motel_room_4_8017D6B4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom4ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom4RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom4IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom4IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_dryfield_motel_room_4_8017D6DC[1] = {
    { D_dryfield_motel_room_4_8017DDF0, D_dryfield_motel_room_4_8017DF94, D_dryfield_motel_room_4_8017E1F4, NULL },
};

u8* D_dryfield_motel_room_4_8017D6EC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_room_4_8017D6F0[1] = { 6 };

WorldCoordRoomLighting D_dryfield_motel_room_4_8017D6F4[1] = {
    { D_dryfield_motel_room_4_8017E420, NULL },
};

DirectionWarpEntry D_dryfield_motel_room_4_8017D6FC[1] = {
    { { { .word = 0 }, 3400, 0, 675 }, { 0, 0, 0, 0 }, { { .word = 0 }, 3400, 0, 675 }, { 0, 0, 0, 0 }, 0x520E0002, 0x520E0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 483 },
};

static SVECTOR _gDryfieldMotelRoom4Collision00830Normals[14] = {
#include "assets/dryfield_motel_room_4_collision_00830_normals.inc"
};

static SVECTOR _gDryfieldMotelRoom4Collision00830Verts[103] = {
#include "assets/dryfield_motel_room_4_collision_00830_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelRoom4Collision00830Faces[47] = {
#include "assets/dryfield_motel_room_4_collision_00830_faces.inc"
};

static s16 _gDryfieldMotelRoom4Collision00830Cells[104] = {
#include "assets/dryfield_motel_room_4_collision_00830_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom4Collision00830Cells[i])
static s16* _gDryfieldMotelRoom4Collision00830Table[4] = {
#include "assets/dryfield_motel_room_4_collision_00830_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_room_4_8017DDF0[1] = {
    { NULL, _gDryfieldMotelRoom4Collision00830Normals, _gDryfieldMotelRoom4Collision00830Verts, _gDryfieldMotelRoom4Collision00830Faces, _gDryfieldMotelRoom4Collision00830Table, -200, -200, 2, 2, 4000, 47 },
};

ViewCamera D_dryfield_motel_room_4_8017DE14[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3500, 0x2904, -2500 } }, 289 },
    { { { { 652, 0, 4043 }, { 208, 4090, -33 }, { -4038, 211, 651 } }, { -6800, 1250, -950 } }, 275 },
    { { { { 3984, 0, -951 }, { -39, 4092, -166 }, { 950, 171, 3980 } }, { -1200, 1450, -200 } }, 240 },
    { { { { 660, 0, -4042 }, { -57, 4095, -9 }, { 4042, 58, 660 } }, { -1400, 1250, -1050 } }, 275 },
    { { { { 209, 0, 4090 }, { 1972, 3588, -100 }, { -3583, 1975, 183 } }, { -6800, 2170, -3550 } }, 263 },
    { { { { 866, 0, -4003 }, { -172, 4092, -37 }, { 3999, 176, 865 } }, { -2200, 1160, -3120 } }, 269 },
};

SpriteBatch D_dryfield_motel_room_4_8017DEEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DEFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF3C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_room_4_8017DF4C[6] = {
    { { .empty = D_dryfield_motel_room_4_8017DEEC }, D_dryfield_motel_room_4_8017DEEC, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DEFC }, D_dryfield_motel_room_4_8017DEFC, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF0C }, D_dryfield_motel_room_4_8017DF0C, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF1C }, D_dryfield_motel_room_4_8017DF1C, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF2C }, D_dryfield_motel_room_4_8017DF2C, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF3C }, D_dryfield_motel_room_4_8017DF3C, NULL },
};

WorldCollisionTrigger D_dryfield_motel_room_4_8017DF94[8] = {
    { NULL, NULL, NULL, { 1872, -1136, 2736, 0 }, { { -1808, -2160, 16, 0 }, { 1808, -2160, -16, 0 }, { -1808, 2160, 16, 0 }, { 1808, 2160, -16, 0 } }, { -37, 0, -4105, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1888, -1088, 2656, 0 }, { { 1808, -2112, -16, 0 }, { -1808, -2112, 16, 0 }, { 1808, 2112, -16, 0 }, { -1808, 2112, 16, 0 } }, { 36, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4511, -1152, 1519, 0 }, { { -17, -2176, -1316, 0 }, { 14, -2176, 1313, 0 }, { -17, 2176, -1316, 0 }, { 14, 2176, 1313, 0 } }, { 4095, 0, -49, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4639, -1120, 1471, 0 }, { { 15, -2144, 1314, 0 }, { -16, -2144, -1315, 0 }, { 15, 2144, 1314, 0 }, { -16, 2144, -1315, 0 } }, { -4107, 0, 47, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3647, -1120, 3502, 0 }, { { 0, -2144, -738, 0 }, { 0, -2144, 738, 0 }, { 0, 2144, -738, 0 }, { 0, 2144, 738, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2260, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3776, -1136, 3488, 0 }, { { 0, -2160, 738, 0 }, { 0, -2160, -738, 0 }, { 0, 2160, 738, 0 }, { 0, 2160, -738, 0 } }, { -4109, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5456, -1088, 3840, 0 }, { { -112, -2160, 1026, 0 }, { 112, -2160, -1026, 0 }, { -112, 2160, 1026, 0 }, { 112, 2160, -1026, 0 } }, { -4082, 0, -447, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5344, -1088, 3808, 0 }, { { 112, -2160, -1026, 0 }, { -112, -2160, 1026, 0 }, { 112, 2160, -1026, 0 }, { -112, 2160, 1026, 0 } }, { 4079, 0, 444, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_room_4_8017E1F4[1] = {
    { NULL, NULL, NULL, { 3584, -48, 512, 0 }, { { -832, 0, -320, 0 }, { 832, 0, -320, 0 }, { -832, 0, 320, 0 }, { 832, 0, 320, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 891, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 22, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Point lights contributing in every view of Dryfield motel room 4.
///
/// Positions and falloff radii are in integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is 1.0). The room-light collection borrows this
/// writable array for the lifetime of the loaded room overlay. Runtime updates
/// attach the transforms to the view coordinate and compose their matrices;
/// lighting queries overwrite each light's attenuation.
static WorldCoordPointLight _gDryfieldMotelRoom4PointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5369, -1319, 5201 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2700, 2700, 2700 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2100,
        .outer = 4442,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4280, -1466, 1405 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 4500, 4500, 4500 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1020,
        .outer = 2601,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1051, -1619, 2995 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2600, 2600, 2600 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2485,
        .outer = 3743,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1025, -1772, -171 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5440, 5440, 5440 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2140,
        .outer = 3071,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5111, -1523, 416 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3600, 3600, 3600 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 788,
        .outer = 1130,
    },
};

WorldCoordRoomLights D_dryfield_motel_room_4_8017E420[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelRoom4PointLights), _gDryfieldMotelRoom4PointLights, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_motel_room_4_8017E438 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_motel_room_4_8017E444 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_4_8017E450[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_4_8017E458[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_4_8017E438 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_4_8017E460[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_4_8017E444 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_4_8017E468[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_room_4_8017E470[8] = {
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E458,
    D_dryfield_motel_room_4_8017E460,
    D_dryfield_motel_room_4_8017E468,
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E450,
};

/// Refuses every key-item use request in Dryfield motel room 4.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldMotelRoom4RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with its requested destination unchanged.
///
/// Copies the complete eight-byte record and returns 1 for query and execution
/// requests alike, with no transition side effects. The non-null records are
/// borrowed through synchronous dispatch; `reply` must be writable and may be
/// the same object as `request`. Neither pointer is retained.
static s32 _dryfieldMotelRoom4ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_4_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_4_TRANSITION_ALLOWED;
}

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldMotelRoom4IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores room actions requested by direction triggers.
///
/// The borrowed request is neither read nor retained. The sender supplies zero
/// for the second payload and ignores the result, which is always zero.
static s32 _dryfieldMotelRoom4IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Runs at state 0 and advances to state 1. The room slot borrows `task`, and
/// the task borrows the overlay's message table; both must stay live for dispatch.
static void _dryfieldMotelRoom4InitTask(Task* task)
{
    task->msgTable = D_dryfield_motel_room_4_8017D6B4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room task available for messages without doing per-frame work.
///
/// State 1 leaves the live task and its state unchanged.
static void _dryfieldMotelRoom4IdleTask(Task* unusedTask)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_room_4_8017D5C4 = {
    { _dryfieldMotelRoom4InitTask, _dryfieldMotelRoom4IdleTask, taskKill },
};

void dryfieldMotelRoom4Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_motel_room_4_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
