#include "rooms/dryfield_motel_room_5.h"

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
extern TaskMessageEntry D_dryfield_motel_room_5_8017D6B4[];

static s32 _dryfieldMotelRoom5RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom5ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom5IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldMotelRoom5IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

extern WorldCollisionGrid    D_dryfield_motel_room_5_8017DC9C[1];
extern WorldCollisionTrigger D_dryfield_motel_room_5_8017DE40[8];
extern WorldCollisionTrigger D_dryfield_motel_room_5_8017E0A0[1];
extern WorldCoordRoomLights  D_dryfield_motel_room_5_8017E56C[1];

TaskMessageEntry D_dryfield_motel_room_5_8017D6B4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom5ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom5RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom5IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom5IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_dryfield_motel_room_5_8017D6DC[1] = {
    { D_dryfield_motel_room_5_8017DC9C, D_dryfield_motel_room_5_8017DE40, D_dryfield_motel_room_5_8017E0A0, NULL },
};

u8* D_dryfield_motel_room_5_8017D6EC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_room_5_8017D6F0[1] = { 6 };

WorldCoordRoomLighting D_dryfield_motel_room_5_8017D6F4[1] = {
    { D_dryfield_motel_room_5_8017E56C, NULL },
};

DirectionWarpEntry D_dryfield_motel_room_5_8017D6FC[1] = {
    { { { .word = 3072 }, 4350, 0, 2426 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4350, 0, 2426 }, { 0, 0, 0, 0 }, 0x521C0002, 0x521C0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 464 },
};

static SVECTOR _gDryfieldMotelRoom5Collision006DCNormals[12] = {
#include "assets/dryfield_motel_room_5_collision_006DC_normals.inc"
};

static SVECTOR _gDryfieldMotelRoom5Collision006DCVerts[85] = {
#include "assets/dryfield_motel_room_5_collision_006DC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelRoom5Collision006DCFaces[38] = {
#include "assets/dryfield_motel_room_5_collision_006DC_faces.inc"
};

static s16 _gDryfieldMotelRoom5Collision006DCCells[68] = {
#include "assets/dryfield_motel_room_5_collision_006DC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom5Collision006DCCells[i])
static s16* _gDryfieldMotelRoom5Collision006DCTable[4] = {
#include "assets/dryfield_motel_room_5_collision_006DC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_room_5_8017DC9C[1] = {
    { NULL, _gDryfieldMotelRoom5Collision006DCNormals, _gDryfieldMotelRoom5Collision006DCVerts, _gDryfieldMotelRoom5Collision006DCFaces, _gDryfieldMotelRoom5Collision006DCTable, -150, 0, 2, 2, 4000, 38 },
};

ViewCamera D_dryfield_motel_room_5_8017DCC0[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x2C10, -3000 } }, 289 },
    { { { { 828, 0, -4011 }, { -323, 4082, -66 }, { 3998, 330, 825 } }, { -300, 1280, -900 } }, 235 },
    { { { { 725, 0, 4031 }, { 300, 4084, -54 }, { -4019, 305, 723 } }, { -4800, 1280, -900 } }, 235 },
    { { { { 4029, 0, 732 }, { 75, 4074, -415 }, { -728, 422, 4008 } }, { -4100, 1380, -500 } }, 246 },
    { { { { 4048, 0, -622 }, { -374, 3272, -2434 }, { 497, 2463, 3234 } }, { -800, 2580, -2900 } }, 246 },
    { { { { -4048, 0, 622 }, { 335, 3452, 2178 }, { -524, 2204, -3411 } }, { -1600, 2280, -5800 } }, 246 },
};

SpriteBatch D_dryfield_motel_room_5_8017DD98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_5_8017DDA8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_5_8017DDB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_5_8017DDC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_5_8017DDD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_5_8017DDE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_room_5_8017DDF8[6] = {
    { { .empty = D_dryfield_motel_room_5_8017DD98 }, D_dryfield_motel_room_5_8017DD98, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDA8 }, D_dryfield_motel_room_5_8017DDA8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDB8 }, D_dryfield_motel_room_5_8017DDB8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDC8 }, D_dryfield_motel_room_5_8017DDC8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDD8 }, D_dryfield_motel_room_5_8017DDD8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDE8 }, D_dryfield_motel_room_5_8017DDE8, NULL },
};

WorldCollisionTrigger D_dryfield_motel_room_5_8017DE40[8] = {
    { NULL, NULL, NULL, { 2384, -1296, 1392, 0 }, { { 112, -1936, -1360, 0 }, { -112, -1936, 1360, 0 }, { 112, 1936, -1360, 0 }, { -112, 1936, 1360, 0 } }, { 4092, 0, 335, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2496, -1120, 1408, 0 }, { { -112, -2144, 1360, 0 }, { 112, -2144, -1360, 0 }, { -112, 2144, 1360, 0 }, { 112, 2144, -1360, 0 } }, { -4092, 0, -338, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3663, -1120, 3199, 0 }, { { 1449, -2144, 275, 0 }, { -1451, -2144, -277, 0 }, { 1449, 2144, 275, 0 }, { -1451, 2144, -277, 0 } }, { -768, 0, 4032, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3711, -1104, 3295, 0 }, { { -1568, -2128, -307, 0 }, { 1566, -2128, 305, 0 }, { -1568, 2128, -307, 0 }, { 1566, 2128, 305, 0 } }, { 783, 0, -4021, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2206, -1104, 4927, 0 }, { { -43, -2128, -1006, 0 }, { 43, -2128, 1006, 0 }, { -43, 2128, -1006, 0 }, { 43, 2128, 1006, 0 } }, { 4115, 0, -178, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2302, -1072, 5023, 0 }, { { 43, -2096, 1006, 0 }, { -43, -2096, -1006, 0 }, { 43, 2096, 1006, 0 }, { -43, 2096, -1006, 0 } }, { -4120, 0, 176, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1183, -1072, 4446, 0 }, { { -1093, -2096, 109, 0 }, { 1093, -2096, -109, 0 }, { -1093, 2096, 109, 0 }, { 1093, 2096, -109, 0 } }, { -409, 0, -4081, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1184, -1072, 4320, 0 }, { { 1093, -2096, -109, 0 }, { -1093, -2096, 109, 0 }, { 1093, 2096, -109, 0 }, { -1093, 2096, 109, 0 } }, { 406, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_room_5_8017E0A0[1] = {
    { NULL, NULL, NULL, { 4368, 0, 2272, 0 }, { { -432, 0, -800, 0 }, { 432, 0, -800, 0 }, { -432, 0, 800, 0 }, { 432, 0, 800, 0 } }, { 0, 4112, 0, 0 }, { -4096, 0, 0, 0 }, 907, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Twelve point lights for motel room 5, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength through `inner` and fades
/// with squared distance to zero at `outer`. The loaded room overlay owns the
/// mutable array: coordinate updates parent and compose its transforms, and
/// lighting queries overwrite attenuation. Borrowed pointers must not outlive
/// the overlay.
static WorldCoordPointLight _gDryfieldMotelRoom5PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10247, -936, -1738 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3280, 3076, 2870 },
        },
        .inner = 1587,
        .outer = 2185,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8192, -2555, -1604 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2460, 2460, 2460 },
        },
        .inner = 1120,
        .outer = 1672,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6380, -2555, -1625 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2460, 2460, 2460 },
        },
        .inner = 1057,
        .outer = 1718,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10564, -1003, -1164 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 1839,
        .outer = 2505,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 490, -1975, -479 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, 3072 },
        },
        .inner = 1107,
        .outer = 1294,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 169, -2006, -1625 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, 3076 },
        },
        .inner = 1112,
        .outer = 1374,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -911, -2035, -2681 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, 3072 },
        },
        .inner = 1288,
        .outer = 1572,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1618, -1020, -130 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2870, 2870, 2870 },
        },
        .inner = 2591,
        .outer = 3272,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3750, -1440, -3 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1229, 1229, 1229 },
        },
        .inner = 2029,
        .outer = 2673,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6865, -1440, 10 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1229, 1229, 1229 },
        },
        .inner = 1935,
        .outer = 2601,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6885, -1440, -2420 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2666, 2666, 2663 },
        },
        .inner = 2646,
        .outer = 3283,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3665, -1440, -2318 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3280, 3280, 3280 },
        },
        .inner = 2164,
        .outer = 2832,
    },
};

WorldCoordRoomLights D_dryfield_motel_room_5_8017E56C[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelRoom5PointLights), _gDryfieldMotelRoom5PointLights, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_motel_room_5_8017E584 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_motel_room_5_8017E590 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_5_8017E59C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_5_8017E5A4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_5_8017E584 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_5_8017E5AC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_5_8017E590 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_5_8017E5B4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_room_5_8017E5BC[8] = {
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E5A4,
    D_dryfield_motel_room_5_8017E5AC,
    D_dryfield_motel_room_5_8017E5B4,
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E59C,
};

static void _dryfieldMotelRoom5InitRoomTask(Task* task);
static void _dryfieldMotelRoom5IdleRoomTask(Task* unusedTask);

/// Refuses every key-item use request in Dryfield motel room 5.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldMotelRoom5RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with its requested destination unchanged.
///
/// Copies the complete eight-byte record and returns 1 for query and execution
/// requests alike, with no transition side effects. The non-null records are
/// borrowed through synchronous dispatch; `reply` must be writable and may be
/// the same object as `request`. Neither pointer is retained.
static s32 _dryfieldMotelRoom5ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_5_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_5_TRANSITION_ALLOWED;
}

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldMotelRoom5IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores room actions requested by direction triggers.
///
/// The borrowed request is neither read nor retained. The sender supplies zero
/// for the second payload and ignores the result, which is always zero.
static s32 _dryfieldMotelRoom5IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers motel room 5's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldMotelRoom5InitRoomTask(Task* task)
{
    task->msgTable = D_dryfield_motel_room_5_8017D6B4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps motel room 5's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldMotelRoom5IdleRoomTask(Task* unusedTask)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_room_5_8017D5C4 = {
    { _dryfieldMotelRoom5InitRoomTask, _dryfieldMotelRoom5IdleRoomTask, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_room_5_8017D5C4`.
void func_dryfield_motel_room_5_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_5_8017D5C4;
    sp.funcs[task->state](task);
}
