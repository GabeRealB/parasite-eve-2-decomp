#include "rooms/acropolis_hallway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"
#include "../../shared/actor_contacts.h"

extern TaskMessageEntry D_acropolis_hallway_8017E238[];
extern SVECTOR          ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void func_acropolis_hallway_8017D784(Task* task);
static void func_acropolis_hallway_8017D7C8(Task* task);

/// State handlers of the room task: set-up, an idle tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_hallway_8017D5C4 = {
    { func_acropolis_hallway_8017D784, func_acropolis_hallway_8017D7C8, taskKill },
};

extern u32     D_acropolis_hallway_8017F8A4[1];
extern SVECTOR D_acropolis_hallway_8017F8A8[21];
extern SVECTOR D_acropolis_hallway_8017F950[6];
extern TmdBone D_acropolis_hallway_8017F880[1];
extern u32     D_acropolis_hallway_8017F980[42];

extern WorldCollisionGrid    D_acropolis_hallway_8017E5D0[1];
extern WorldCollisionTrigger D_acropolis_hallway_8017E5F4[4];
extern WorldCollisionTrigger D_acropolis_hallway_8017E724[9];
extern WorldCoordRoomLights  D_acropolis_hallway_8017EBC4[1];
s32                          func_acropolis_hallway_8017D5D0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                          func_acropolis_hallway_8017D72C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32                          func_acropolis_hallway_8017D734(Task*, s32, s32, TaskMessageArg);

TaskMessageEntry D_acropolis_hallway_8017E238[4] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_hallway_8017D5D0 },
    { 5105, func_acropolis_hallway_8017D72C },
    { 5106, func_acropolis_hallway_8017D734 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

GpRoomObjRec D_acropolis_hallway_8017E258[1] = {
    { D_acropolis_hallway_8017E5D0, D_acropolis_hallway_8017E5F4, D_acropolis_hallway_8017E724, NULL },
};

u8* D_acropolis_hallway_8017E268[1] = {
    gViewIdentityMap,
};

GpViewCountRec D_acropolis_hallway_8017E26C[1] = {
    { { .bytes = { 5, 0 } } },
};

WorldCoordRoomLighting D_acropolis_hallway_8017E270[1] = {
    { D_acropolis_hallway_8017EBC4, NULL },
};

GpWarpRec D_acropolis_hallway_8017E278[4] = {
    { { .words = { 3072, 3012, -937, -698 } }, { 0, 0, 0, 0 }, { .words = { 3072, 3012, -937, -698 } }, { 0, 0, 0, 0 }, 0x51070002, 0x51070001, 0, 4, 0, 497 },
    { { .words = { 2048, 2271, -937, 970 } }, { 0, 0, 0, 0 }, { .words = { 2048, 2271, -937, 970 } }, { 0, 0, 0, 0 }, 0x51070002, 0x51070001, 0, 4, 0, 498 },
    { { .words = { 2048, -748, -937, 970 } }, { 0, 0, 0, 0 }, { .words = { 2048, -748, -937, 970 } }, { 0, 0, 0, 0 }, 0x51070002, 0x51070001, 0, 3, 0, 499 },
    { { .words = { 1024, -2963, -937, -623 } }, { 0, 0, 0, 0 }, { .words = { 1024, -2963, -937, -623 } }, { 0, 0, 0, 0 }, 0x51070004, 0x51070003, 0, 2, 0, 500 },
};

SVECTOR D_acropolis_hallway_8017E358[10] = {
#include "assets/acropolis_hallway_collision_01010_normals.inc"
};

SVECTOR D_acropolis_hallway_8017E3A8[35] = {
#include "assets/acropolis_hallway_collision_01010_verts.inc"
};

WorldCollisionGridFace D_acropolis_hallway_8017E4C0[17] = {
#include "assets/acropolis_hallway_collision_01010_faces.inc"
};

s16 D_acropolis_hallway_8017E58C[30] = {
#include "assets/acropolis_hallway_collision_01010_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_hallway_8017E58C[i])
s16* D_acropolis_hallway_8017E5C8[2] = {
#include "assets/acropolis_hallway_collision_01010_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_hallway_8017E5D0[1] = {
    { NULL, D_acropolis_hallway_8017E358, D_acropolis_hallway_8017E3A8, D_acropolis_hallway_8017E4C0, D_acropolis_hallway_8017E5C8, 3250, 1250, 2, 1, 4000, 17 },
};

WorldCollisionTrigger D_acropolis_hallway_8017E5F4[4] = {
    { NULL, NULL, NULL, { -1984, -2016, 0, 0 }, { { 0, -1792, -1760, 0 }, { 0, 1792, -1760, 0 }, { 0, -1792, 1760, 0 }, { 0, 1792, 1760, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 0, 0 }, 2508, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 320, -2016, 0, 0 }, { { -174, -1792, -1752, 0 }, { -174, 1792, -1752, 0 }, { 172, -1792, 1750, 0 }, { 172, 1792, 1750, 0 } }, { -4086, 0, 402, 0 }, { 0, 0, 0, 0 }, 2508, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 192, -1920, 0, 0 }, { { -174, 1792, -1752, 0 }, { -174, -1792, -1752, 0 }, { 172, 1792, 1750, 0 }, { 172, -1792, 1750, 0 } }, { 4084, 0, -404, 0 }, { 0, 0, 0, 0 }, 2508, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2112, -1952, 0, 0 }, { { 0, 1792, -1760, 0 }, { 0, -1792, -1760, 0 }, { 0, 1792, 1760, 0 }, { 0, -1792, 1760, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 2508, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_hallway_8017E724[9] = {
    { NULL, NULL, NULL, { -2960, -1024, -384, 0 }, { { -272, 0, -736, 0 }, { 272, 0, -736, 0 }, { -272, 0, 736, 0 }, { 272, 0, 736, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 783, WORLD_COLLISION_TRIGGER_ACTION_WARP, 4, 67, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -768, -1016, 920, 0 }, { { -496, 0, -352, 0 }, { 496, 0, -352, 0 }, { -496, 0, 352, 0 }, { 496, 0, 352, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 607, WORLD_COLLISION_TRIGGER_ACTION_WARP, 4, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2304, -1024, 928, 0 }, { { -496, 0, -352, 0 }, { 496, 0, -352, 0 }, { -496, 0, 352, 0 }, { 496, 0, 352, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 607, WORLD_COLLISION_TRIGGER_ACTION_WARP, 6, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2944, -1024, -672, 0 }, { { 288, 0, -496, 0 }, { 288, 0, 496, 0 }, { -288, 0, -496, 0 }, { -288, 0, 496, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 572, WORLD_COLLISION_TRIGGER_ACTION_WARP, 8, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -1088, -928, 0 }, { { -560, 0, -288, 0 }, { 560, 0, -288, 0 }, { -560, 0, 288, 0 }, { 560, 0, 288, 0 } }, { 0, 4101, 0, 0 }, { -201, 0, 4091, 0 }, 627, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3184, -1056, 368, 0 }, { { -1056, 0, -912, 0 }, { 128, 0, -912, 0 }, { -1056, 0, 208, 0 }, { 128, 0, 208, 0 } }, { 0, 4113, 0, 0 }, { -4076, 0, 401, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -1056, 848, 0 }, { { -528, 0, -240, 0 }, { 528, 0, -240, 0 }, { -528, 0, 432, 0 }, { 528, 0, 432, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -784, -1056, -864, 0 }, { { -832, 0, -368, 0 }, { 832, 0, -368, 0 }, { -832, 0, 240, 0 }, { 832, 0, 240, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, 4091, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2128, -1042, -1024, 0 }, { { -400, 0, -304, 0 }, { 400, 0, -304, 0 }, { -400, 0, 304, 0 }, { 400, 0, 304, 0 } }, { 0, 4100, 0, 0 }, { 200, 0, 4090, 0 }, 501, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_hallway_8017E9D0[2] = {
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_801693AC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_hallway_8017E9E8[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_hallway_8017EA00[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_hallway_8017EA18[3] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_80155AC4 },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_801693AC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_acropolis_hallway_8017EA3C[13] = {
    { NULL, NULL },
    { D_map_akropolis_8017B04C, D_acropolis_hallway_8017E9D0 },
    { D_map_akropolis_8017B0DC, D_acropolis_hallway_8017E9E8 },
    { D_map_akropolis_8017B0FC, D_acropolis_hallway_8017EA00 },
    { D_map_akropolis_8017B11C, D_acropolis_hallway_8017EA18 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordPointLight D_acropolis_hallway_8017EAA4[3] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2374, -2432, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1360, 2384 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2384, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1360, 2384 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2297, -2384, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1360, 2384 },
};

WorldCoordRoomLights D_acropolis_hallway_8017EBC4[1] = {
    { 0, NULL, ARRAY_SIZE(D_acropolis_hallway_8017EAA4), D_acropolis_hallway_8017EAA4, 0, NULL },
};

SpriteBatch D_acropolis_hallway_8017EBDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_hallway_8017EBEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_hallway_8017EBFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_hallway_8017EC0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_hallway_8017EC1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_hallway_8017EC2C[5] = {
    { { .empty = D_acropolis_hallway_8017EBDC }, D_acropolis_hallway_8017EBDC, NULL },
    { { .empty = D_acropolis_hallway_8017EBEC }, D_acropolis_hallway_8017EBEC, NULL },
    { { .empty = D_acropolis_hallway_8017EBFC }, D_acropolis_hallway_8017EBFC, NULL },
    { { .empty = D_acropolis_hallway_8017EC0C }, D_acropolis_hallway_8017EC0C, NULL },
    { { .empty = D_acropolis_hallway_8017EC1C }, D_acropolis_hallway_8017EC1C, NULL },
};

GpViewRec D_acropolis_hallway_8017EC68[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6590, 0 } }, 911 },
    { { { { -908, 0, 3994 }, { -529, 4059, -120 }, { -3958, -542, -900 } }, { 240, 1610, -240 } }, 207 },
    { { { { -481, 0, 4067 }, { 891, 3996, 105 }, { -3968, 897, -469 } }, { -2520, 2700, -240 } }, 207 },
    { { { { -624, 0, -4048 }, { -1848, 3644, 285 }, { 3601, 1870, -555 } }, { 1430, 3260, -240 } }, 207 },
    { { { { -4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, -4096 } }, { -880, 2360, 640 } }, 207 },
};

WorldCollisionFootstepSounds D_acropolis_hallway_8017ED1C = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionSurfaceProperties D_acropolis_hallway_8017ED28[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_hallway_8017ED30[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_hallway_8017ED1C },
};

WorldCollisionSurfaceProperties D_acropolis_hallway_8017ED38[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_hallway_8017ED1C },
};

WorldCollisionSurfaceProperties* D_acropolis_hallway_8017ED40[8] = {
    D_acropolis_hallway_8017ED28,
    D_acropolis_hallway_8017ED30,
    D_acropolis_hallway_8017ED28,
    D_acropolis_hallway_8017ED28,
    D_acropolis_hallway_8017ED38,
    D_acropolis_hallway_8017ED28,
    D_acropolis_hallway_8017ED28,
    D_acropolis_hallway_8017ED28,
};

TmdBone D_acropolis_hallway_8017ED60[1] = {
#include "assets/acropolis_hallway_model_0229C_skeleton.inc"
};

u32 D_acropolis_hallway_8017ED84[1] = {
#include "assets/acropolis_hallway_model_0229C_partVerts.inc"
};

SVECTOR D_acropolis_hallway_8017ED88[99] = {
#include "assets/acropolis_hallway_model_0229C_verts.inc"
};

u32 D_acropolis_hallway_8017F0A0[495] = {
#include "assets/acropolis_hallway_model_0229C_stream.inc"
};

TmdSource D_acropolis_hallway_8017F85C = {
    0,
    3728,
    0,
    1,
    D_acropolis_hallway_8017ED84,
    D_acropolis_hallway_8017ED88,
    &D_acropolis_hallway_8017ED88[99],
    D_acropolis_hallway_8017ED60,
    D_acropolis_hallway_8017F0A0,
};

TmdBone D_acropolis_hallway_8017F880[1] = {
#include "assets/acropolis_hallway_model_02468_skeleton.inc"
};

u32 D_acropolis_hallway_8017F8A4[1] = {
#include "assets/acropolis_hallway_model_02468_partVerts.inc"
};

SVECTOR D_acropolis_hallway_8017F8A8[21] = {
#include "assets/acropolis_hallway_model_02468_verts.inc"
};

SVECTOR D_acropolis_hallway_8017F950[6] = {
#include "assets/acropolis_hallway_model_02468_normals.inc"
};

u32 D_acropolis_hallway_8017F980[42] = {
#include "assets/acropolis_hallway_model_02468_stream.inc"
};

TmdSource D_acropolis_hallway_8017FA28 = {
    0,
    312,
    0,
    1,
    D_acropolis_hallway_8017F8A4,
    D_acropolis_hallway_8017F8A8,
    D_acropolis_hallway_8017F950,
    D_acropolis_hallway_8017F880,
    D_acropolis_hallway_8017F980,
};

SVECTOR ActorContact_ScratchPosition = { 0 };

static void func_acropolis_hallway_8017E1C0(Task* task);

/// Message gate for the hallway's first hotspot: copies the incoming record to
/// the outgoing one, then edits the copy's `room` (the answer the caller
/// acts on) according to the message id and the room's progress nibbles.
/// Returning 0 means the message was consumed.
s32 func_acropolis_hallway_8017D5D0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 msgId;

    *out = *in;
    if (in->areaId == 8) {
        if ((GameFlag_GetNibble(9) & 2) && in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = 2;
        }
    }
    if (in->areaId == 4 && in->warp == 3 && GameFlag_GetNibble(0) < 3) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->areaId == 8 && GameFlag_GetNibble(0) == 3) {
        return 1;
    }
    msgId = in->areaId;
    if (msgId == 4 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0) >= 3) {
            out->room = msgId;
        }
        if (GameFlag_GetNibble(0) == 2) {
            out->room = 3;
        }
    }
    return 1;
}

/// Message handler that accepts the message and does nothing else.
s32 func_acropolis_hallway_8017D72C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_acropolis_hallway_8017D734(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) { /* irregular */
        case 6:
            SndEvt_EnqueueType6(0x51070006, 0, 0);
            break;
        case 7:
            SndEvt_EnqueueType6(0x51070007, 0, 0);
            break;
    }
    return 0;
}

/// State 0 of the room task: installs the room's message table, publishes the
/// task in pointer slot 7 and advances to the next state.
static void func_acropolis_hallway_8017D784(Task* task)
{
    task->msgTable = D_acropolis_hallway_8017E238;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room task: does nothing.
static void func_acropolis_hallway_8017D7C8(Task* task)
{
}

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_hallway_8017D7D0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_hallway_8017D5C4;
    sp.funcs[task->state](task);
}

void func_acropolis_hallway_8017D828(Task* unused)
{
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Item-pickup model task step: on the first run resets the mesh flags and
/// arms the task, then hides the mesh with flag 0x80 unless the room is being
/// drawn from view 5, and always hides it once the item's 2-bit flag reads 2
/// (already taken).
void func_acropolis_hallway_8017E120(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2.pointer;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if (task->state == 0) {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        task->state++;
    }
    if (Gp_GetViewIndex() == 5) {
        tmd->flags = TMD_OBJECT_FLAGGED_PASS;
    } else {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flag == 2) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Model task step for a pickup's mesh: when the pickup's 2-bit flag reads 2
/// it sets `TMD_OBJECT_SKIP_AUTO_BUFFER`, otherwise it selects the flagged draw
/// pass, clears the draw offset and allocates the mesh's buffers. The view
/// index is fetched but unused.
static void func_acropolis_hallway_8017E1C0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2.pointer;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
