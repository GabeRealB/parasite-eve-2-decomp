#include "rooms/dryfield_night_g_r_kitchen.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
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

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
/// Selects the nighttime sound bank for the shared G & R kitchen door handler.
///
/// Keep this binding through the door-handler implementation fragment.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/g_r_kitchen.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event the room's gate `roomEventGate`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `gRoomEventTaskDesc`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;
extern TaskDesc     gRoomEventTaskDesc;

/// The room's message table, `(msgId, handler)` pairs ending at `TASK_MESSAGE_TABLE_END`,
/// which the entry task installs as its own `Task::msgTable`.
extern TaskMessageEntry D_dryfield_night_g_r_kitchen_8017E254[];

/// The two pairs of world points the room's light shafts run between, one
/// pair per `SVECTOR[2]`: the first array holds the two shafts drawn in view
/// 2, the second the two drawn in view 3.
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E27C[];
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E29C[];

s32 func_dryfield_night_g_r_kitchen_8017D8BC(Task*, s32, s32, s32);
s32 func_dryfield_night_g_r_kitchen_8017D948(Task*, s32, s32, s32);
s32 func_dryfield_night_g_r_kitchen_8017D950(Task*, s32, s32, s32);

extern WorldCollisionGrid    D_dryfield_night_g_r_kitchen_8017E554[1];
extern WorldCollisionTrigger D_dryfield_night_g_r_kitchen_8017E864[2];
extern WorldCollisionTrigger D_dryfield_night_g_r_kitchen_8017E8FC[7];
extern WorldCoordRoomLights  D_dryfield_night_g_r_kitchen_8017E84C[1];

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_g_r_kitchen_8017E254[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, grKitchenDoorMsg },
    { 5105, func_dryfield_night_g_r_kitchen_8017D8BC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_g_r_kitchen_8017D950 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_g_r_kitchen_8017D948 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_dryfield_night_g_r_kitchen_8017E27C[4] = {
    { -1473, -2181, 2023, 0 },
    { -1473, -2181, 2416, 0 },
    { -197, -2161, 2973, 0 },
    { 197, -2161, 2973, 0 },
};

SVECTOR D_dryfield_night_g_r_kitchen_8017E29C[4] = {
    { -70, -2790, -370, 0 },
    { -70, -2790, 370, 0 },
    { 70, -2790, -370, 0 },
    { 70, -2790, 370, 0 },
};

WorldCoordRoomLighting D_dryfield_night_g_r_kitchen_8017E2BC[1] = {
    { D_dryfield_night_g_r_kitchen_8017E84C, NULL },
};

WorldCollisionRoomResources D_dryfield_night_g_r_kitchen_8017E2C4[1] = {
    { D_dryfield_night_g_r_kitchen_8017E554, D_dryfield_night_g_r_kitchen_8017E864, D_dryfield_night_g_r_kitchen_8017E8FC, NULL },
};

u8* D_dryfield_night_g_r_kitchen_8017E2D4[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_g_r_kitchen_8017E2D8[1] = { 3 };

DirectionWarpEntry D_dryfield_night_g_r_kitchen_8017E2DC[2] = {
    { { { .word = 1024 }, -1168, 0, 2135 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -1168, 0, 2135 }, { 0, 0, 0, 0 }, 0x53130002, 0x53130001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 478 },
    { { { .word = 2048 }, 0, 0, 2512 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 0, 0, 2512 }, { 0, 0, 0, 0 }, 0x53130002, 0x53130001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 477 },
};

static SVECTOR _gDryfieldNightGRKitchenCollision00F94Normals[6] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_normals.inc"
};

static SVECTOR _gDryfieldNightGRKitchenCollision00F94Verts[26] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGRKitchenCollision00F94Faces[16] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_faces.inc"
};

static s16 _gDryfieldNightGRKitchenCollision00F94Cells[32] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightGRKitchenCollision00F94Cells[i])
static s16* _gDryfieldNightGRKitchenCollision00F94Table[2] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_g_r_kitchen_8017E554[1] = {
    { NULL, _gDryfieldNightGRKitchenCollision00F94Normals, _gDryfieldNightGRKitchenCollision00F94Verts, _gDryfieldNightGRKitchenCollision00F94Faces, _gDryfieldNightGRKitchenCollision00F94Table, 1800, 3000, 1, 2, 4000, 16 },
};

ViewCamera D_dryfield_night_g_r_kitchen_8017E578[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 1053 },
    { { { { 3955, 0, 1064 }, { 591, 3405, -2198 }, { -885, 2276, 3288 } }, { -400, 2700, 1000 } }, 230 },
    { { { { -3988, 0, 933 }, { -276, 3911, -1183 }, { -891, -1215, -3808 } }, { -500, 400, -2700 } }, 230 },
};

SpriteBatch D_dryfield_night_g_r_kitchen_8017E5E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_g_r_kitchen_8017E5F4[7] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -88, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, -40, 675, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -112, -16, 725, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -104, 0, 750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, 32, 700, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, 40, 675, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -96, 16, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_g_r_kitchen_8017E680[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_g_r_kitchen_8017E698[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_g_r_kitchen_8017E6A8[3] = {
    { { .empty = D_dryfield_night_g_r_kitchen_8017E5E4 }, D_dryfield_night_g_r_kitchen_8017E5E4, NULL },
    { { .elements = D_dryfield_night_g_r_kitchen_8017E5F4 }, D_dryfield_night_g_r_kitchen_8017E680, NULL },
    { { .empty = D_dryfield_night_g_r_kitchen_8017E698 }, D_dryfield_night_g_r_kitchen_8017E698, NULL },
};

WorldCoordPointLight D_dryfield_night_g_r_kitchen_8017E6CC[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2400, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 3200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1800, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1300, -1800, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 0x186A0, 0x186A0 },
};

WorldCoordRoomLights D_dryfield_night_g_r_kitchen_8017E84C[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_g_r_kitchen_8017E6CC), D_dryfield_night_g_r_kitchen_8017E6CC, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_g_r_kitchen_8017E864[2] = {
    { NULL, NULL, NULL, { -2, -1167, 381, 0 }, { { 1974, -1520, 391, 0 }, { -1973, -1520, -390, 0 }, { 1974, 1520, 391, 0 }, { -1973, 1520, -390, 0 } }, { -796, 0, 4017, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1, -1152, 479, 0 }, { { -1973, -1520, -390, 0 }, { 1974, -1520, 391, 0 }, { -1973, 1520, -390, 0 }, { 1974, 1520, 391, 0 } }, { 794, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_g_r_kitchen_8017E8FC[7] = {
    { NULL, NULL, NULL, { -1248, -48, 2336, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -48, 2784, 0 }, { { 736, 0, -320, 0 }, { 736, 0, 320, 0 }, { -736, 0, -320, 0 }, { -736, 0, 320, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -576, -64, -352, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 336, -64, -2400, 0 }, { { -688, 0, -576, 0 }, { 688, 0, -576, 0 }, { -688, 0, 576, 0 }, { 688, 0, 576, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 896, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -544, -64, 960, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -608, -64, -1600, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 784, -64, -912, 0 }, { { -464, 0, -1296, 0 }, { 464, 0, -1296, 0 }, { -464, 0, 1296, 0 }, { 464, 0, 1296, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_g_r_kitchen_8017EB10[3] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801445DC },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_80150C80 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_g_r_kitchen_8017EB34[4] = {
    { 40, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_8013E500 },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_80150C80 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_g_r_kitchen_8017EB64[3] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801445DC },
    { 40, 40, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_204000_80156500 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_g_r_kitchen_8017EB88[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017BF08, D_dryfield_night_g_r_kitchen_8017EB10 },
    { NULL, NULL },
    { D_map_dryfield_full_8017BF88, D_dryfield_night_g_r_kitchen_8017EB34 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017C038, D_dryfield_night_g_r_kitchen_8017EB64 },
};

WorldCollisionFootstepSounds D_dryfield_night_g_r_kitchen_8017EBE8 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionSurfaceProperties D_dryfield_night_g_r_kitchen_8017EBF4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_g_r_kitchen_8017EBFC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_g_r_kitchen_8017EBE8 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_g_r_kitchen_8017EC04[8] = {
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBFC,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 0, 222, 254 } };

RoomEventReq gRoomEventReq;

static void func_dryfield_night_g_r_kitchen_8017D958(Task* task);
static void func_dryfield_night_g_r_kitchen_8017D99C(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D8BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/g_r_kitchen_door_msg.inc.c"

/// The room's handler for message 0x13F0: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D948(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D950(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
static void func_dryfield_night_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_night_g_r_kitchen_8017E254;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room entry task: idles.
static void func_dryfield_night_g_r_kitchen_8017D99C(Task* task)
{
}

/// The room entry task's three states: install the room's message table,
/// idle, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_g_r_kitchen_8017D5DC = {
    { func_dryfield_night_g_r_kitchen_8017D958, func_dryfield_night_g_r_kitchen_8017D99C, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_g_r_kitchen_8017D9A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_g_r_kitchen_8017D5DC;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_shaft.inc.c"

/// Picks the pair of light shafts `glowDrawShaft`
/// draws from the current view index (`gGameSession->location.loc.view`, 2 or 3);
/// any other view draws nothing.
void func_dryfield_night_g_r_kitchen_8017E1E4(Task* unused)
{
    u8 view;

    view = gGameSession->location.loc.view;
    if (view == 2) {
        glowDrawShaft(&D_dryfield_night_g_r_kitchen_8017E27C[0], 0x100);
        glowDrawShaft(&D_dryfield_night_g_r_kitchen_8017E27C[2], 0x100);
    } else if (view == 3) {
        glowDrawShaft(&D_dryfield_night_g_r_kitchen_8017E29C[0], 0x100);
        glowDrawShaft(&D_dryfield_night_g_r_kitchen_8017E29C[2], 0x100);
    }
}
