#include "rooms/neo_ark_r31.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// Room message handler table installed into `Task::msgTable`.
extern TaskMessageEntry D_neo_ark_r31_8017D9F4[];
extern EvsCommand       D_actor_461800_80133F90[];
extern EvsCommand       D_actor_461800_80134470[];

s32  func_neo_ark_r31_8017D8B0(Task*, s32, s32, s32);
s32  func_neo_ark_r31_8017D8B8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_r31_8017D8FC(Task*, s32, s32, s32);
s32  func_neo_ark_r31_8017D904(Task*, s32, s32, s32);
void func_neo_ark_r31_8017D5D0(Task*);

TaskDesc D_neo_ark_r31_8017D9E8 = { { { TASK_BODY_NONE, 192 } }, func_neo_ark_r31_8017D5D0, { .value = 0 } };

TaskMessageEntry D_neo_ark_r31_8017D9F4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_r31_8017D8B8 },
    { 5105, func_neo_ark_r31_8017D8B0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_r31_8017D904 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_r31_8017D8FC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* D_neo_ark_r31_8017DA1C[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_r31_8017DA20[1] = { 3 };

DirectionWarpEntry D_neo_ark_r31_8017DA24[1] = {
    { { { .word = 2048 }, 0, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 0, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 1, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

ViewCamera D_neo_ark_r31_8017DA5C[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7510, 0x61A8, -6980 } }, 329 },
    { { { { 2889, 0, -2903 }, { 2898, 236, 2884 }, { 167, -4089, 167 } }, { -8000, -1000, -7000 } }, 289 },
    { { { { -3243, 0, 2501 }, { 1904, 2655, 2469 }, { -1621, 3118, -2102 } }, { -8340, 1050, -7830 } }, 289 },
};

SpriteBatch D_neo_ark_r31_8017DAC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_r31_8017DAD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_r31_8017DAE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_r31_8017DAF8[3] = {
    { { .empty = D_neo_ark_r31_8017DAC8 }, D_neo_ark_r31_8017DAC8, NULL },
    { { .empty = D_neo_ark_r31_8017DAD8 }, D_neo_ark_r31_8017DAD8, NULL },
    { { .empty = D_neo_ark_r31_8017DAE8 }, D_neo_ark_r31_8017DAE8, NULL },
};

WorldCoordPointLight D_neo_ark_r31_8017DB1C[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -0x2710, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 0x186A0, 0x186A0 },
};

WorldCoordRoomLights D_neo_ark_r31_8017DB7C = { 0, NULL, ARRAY_SIZE(D_neo_ark_r31_8017DB1C), D_neo_ark_r31_8017DB1C, 0, NULL };

AreaResource D_neo_ark_r31_8017DB94[3] = {
    { 101, 618, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_461800_80139F8C },
    { 132, 618, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_461800_801437EC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_r31_8017DBB8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C550, D_neo_ark_r31_8017DB94 },
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
    { NULL, NULL },
};

s32 D_neo_ark_r31_8017DC20[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_neo_ark_r31_8017DC2C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_neo_ark_r31_8017DC34[8] = {
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
};

s32 D_neo_ark_r31_8017DC54 = 0;

static void func_neo_ark_r31_8017D90C(Task* arg0);
static void func_neo_ark_r31_8017D980(Task* task);

void func_neo_ark_r31_8017D5D0(Task* task)
{
    POLY_FT4* poly;
    DR_STP*   stp;
    s32       buf;
    s32       otz;
    s32       x;
    s32       y;
    s32       sx;
    s32       sy;
    s32       px;

    otz = 6;
    buf = gDisplayState.otBuffer;
    if (task->state == 0) {
        D_neo_ark_r31_8017DC54 = 3;
        task->state++;
    }
    if (D_neo_ark_r31_8017DC54 < 0) {
        taskCallExit(task);
        return;
    }
    for (x = 0; x < 0x140; x += 0xA0) {
        sx = x - 0xA0;
        for (y = 0; y < 0xF0; y += 0xF0) {
            sy             = y - 0x78;
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(POLY_FT4);
            poly->tpage    = getTPage(2, 0, x & ~0x3F, buf << 8);
            poly->y0 = poly->y1 = sy;
            poly->v0 = poly->v1 = (y + (buf << 4)) + gDisplayState.vramYOffset;
            if (poly->v0 < 0x10) {
                poly->y2 = poly->y3 = y + 0x78;
                poly->v2 = poly->v3 = poly->v0 + 0xF0;
            } else {
                s32 d    = 0xFF - poly->v0;
                poly->y2 = poly->y3 = sy + d;
                poly->v2 = poly->v3 = poly->v0 + d;
            }
            px       = sx - D_neo_ark_r31_8017DC54;
            poly->x0 = poly->x2 = px;
            poly->u0 = poly->u2 = x & 0x3F;
            if (poly->u0 < 0x60) {
                poly->x1 = poly->x3 = poly->x0 + 0xA0;
                poly->u1 = poly->u3 = poly->u0 + 0xA0;
            } else {
                s32 d    = 0xFF - poly->u0;
                poly->x1 = poly->x3 = poly->x0 + d;
                poly->u1 = poly->u3 = poly->u0 + d;
            }
            setlen(poly, 9);
            setcode(poly, 0x2F);
            addPrim(gGpuCurrentOt + otz, poly);
        }
    }
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + otz, stp);
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 0x3FF, stp);
}

s32 func_neo_ark_r31_8017D8B0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler for the save location: copies the incoming `RoomEventMsg`
/// onto the outgoing one and passes both to `func_map_neo_ark_80179B14`. Returns 1.
s32 func_neo_ark_r31_8017D8B8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_r31_8017D8FC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_neo_ark_r31_8017D904(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Room task state 0: installs the message table, claims pointer slot 7,
/// sets `gCdCmdQueue.imageMdecMode` to 2 and starts the room script with
/// `func_800E8634`. Advances to state 1.
static void func_neo_ark_r31_8017D90C(Task* arg0)
{
    CdCmdQueue* queue;

    queue          = &gCdCmdQueue;
    arg0->msgTable = D_neo_ark_r31_8017D9F4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    func_800E8634(D_actor_461800_80133F90, 0, D_actor_461800_80134470);
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task state 1: stores 2 into `gCdCmdQueue.imageMdecMode` every tick.
static void func_neo_ark_r31_8017D980(Task* task)
{
    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
}

/// State handlers of the room task `func_neo_ark_r31_8017D990`, indexed by
/// `Task::state`: the set-up tick, the tick that stores 2 into `gCdCmdQueue.imageMdecMode`,
/// and `taskKill`.
static const TaskFuncTable3 D_neo_ark_r31_8017D5C4 = {
    {
        func_neo_ark_r31_8017D90C,
        func_neo_ark_r31_8017D980,
        taskKill,
    },
};

/// Room task: dispatches through a stack copy of its state table.
void func_neo_ark_r31_8017D990(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_r31_8017D5C4;
    sp.funcs[task->state](task);
}
