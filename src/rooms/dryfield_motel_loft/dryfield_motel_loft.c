#include "rooms/dryfield_motel_loft.h"

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
extern TaskMessageEntry D_dryfield_motel_loft_8017D6B4[];

static s32 _dryfieldMotelLoftRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _dryfieldMotelLoftAcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelLoftIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);
static s32 _dryfieldMotelLoftIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

extern WorldCollisionGrid    D_dryfield_motel_loft_8017D9BC[1];
extern WorldCollisionTrigger D_dryfield_motel_loft_8017DE2C[12];
extern WorldCollisionTrigger D_dryfield_motel_loft_8017E1BC[2];
extern WorldCoordRoomLights  D_dryfield_motel_loft_8017E614[1];

TaskMessageEntry D_dryfield_motel_loft_8017D6B4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelLoftAcceptRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelLoftRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelLoftIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelLoftIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_dryfield_motel_loft_8017D6DC[1] = {
    { D_dryfield_motel_loft_8017D9BC, D_dryfield_motel_loft_8017DE2C, D_dryfield_motel_loft_8017E1BC, NULL },
};

u8* D_dryfield_motel_loft_8017D6EC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_loft_8017D6F0[1] = { 7 };

WorldCoordRoomLighting D_dryfield_motel_loft_8017D6F4[1] = {
    { D_dryfield_motel_loft_8017E614, NULL },
};

DirectionWarpEntry D_dryfield_motel_loft_8017D6FC[1] = {
    { { { .word = 0 }, 3953, 0, -2156 }, { 0, 0, 0, 0 }, { { .word = 0 }, 3953, 0, -2156 }, { 0, 0, 0, 0 }, 0x521F0002, 0x521F0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 466 },
};

static SVECTOR _gDryfieldMotelLoftCollision003FCNormals[8] = {
#include "assets/dryfield_motel_loft_collision_003FC_normals.inc"
};

static SVECTOR _gDryfieldMotelLoftCollision003FCVerts[30] = {
#include "assets/dryfield_motel_loft_collision_003FC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelLoftCollision003FCFaces[17] = {
#include "assets/dryfield_motel_loft_collision_003FC_faces.inc"
};

static s16 _gDryfieldMotelLoftCollision003FCCells[58] = {
#include "assets/dryfield_motel_loft_collision_003FC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelLoftCollision003FCCells[i])
static s16* _gDryfieldMotelLoftCollision003FCTable[6] = {
#include "assets/dryfield_motel_loft_collision_003FC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_loft_8017D9BC[1] = {
    { NULL, _gDryfieldMotelLoftCollision003FCNormals, _gDryfieldMotelLoftCollision003FCVerts, _gDryfieldMotelLoftCollision003FCFaces, _gDryfieldMotelLoftCollision003FCTable, 6000, 2500, 3, 2, 4000, 17 },
};

ViewCamera D_dryfield_motel_loft_8017D9E0[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6978, 0 } }, 541 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { 388, 0, 4077 }, { -370, 4079, 35 }, { -4060, -372, 386 } }, { -600, 740, 1510 } }, 257 },
    { { { { -3971, 0, -1003 }, { -512, 3522, 2027 }, { 863, 2090, -3414 } }, { 5710, 2360, -2340 } }, 257 },
    { { { { -800, 0, 4016 }, { 475, 4067, 94 }, { -3988, 484, -795 } }, { -960, 1500, -1900 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
};

SpriteBatch D_dryfield_motel_loft_8017DADC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_loft_8017DAEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_loft_8017DAFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_loft_8017DB0C[9] = {
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -96, 875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, 0, 875, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 72, -120, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, 72, 0, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -80, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 40, 0, 1050, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -56, 1150, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, 0, 1150, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 24, -48, 1200, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_loft_8017DBC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_loft_8017DBD8[8] = {
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -64, -120, 750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -64, -24, 750, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -96, -120, 650, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -96, -24, 650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -128, -120, 600, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -128, -24, 600, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, -120, 575, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, -24, 575, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_loft_8017DC78[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_loft_8017DC90[7] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -16, -56, 1325, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -64, 8, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -64, -72, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -40, -64, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -40, 0, 1250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -88, 16, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -88, -80, 1000, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_loft_8017DD1C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_loft_8017DD34[7] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 8, 1500, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 0, -64, 1425, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 0, 0, 1425, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 32, -72, 1250, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 32, 0, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 64, 16, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 64, -80, 1000, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_loft_8017DDC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_loft_8017DDD8[7] = {
    { { .empty = D_dryfield_motel_loft_8017DADC }, D_dryfield_motel_loft_8017DADC, NULL },
    { { .empty = D_dryfield_motel_loft_8017DAEC }, D_dryfield_motel_loft_8017DAEC, NULL },
    { { .empty = D_dryfield_motel_loft_8017DAFC }, D_dryfield_motel_loft_8017DAFC, NULL },
    { { .elements = D_dryfield_motel_loft_8017DB0C }, D_dryfield_motel_loft_8017DBC0, NULL },
    { { .elements = D_dryfield_motel_loft_8017DBD8 }, D_dryfield_motel_loft_8017DC78, NULL },
    { { .elements = D_dryfield_motel_loft_8017DC90 }, D_dryfield_motel_loft_8017DD1C, NULL },
    { { .elements = D_dryfield_motel_loft_8017DD34 }, D_dryfield_motel_loft_8017DDC0, NULL },
};

WorldCollisionTrigger D_dryfield_motel_loft_8017DE2C[12] = {
    { NULL, NULL, NULL, { 3007, -1040, -417, 0 }, { { -4, -1264, -2500, 0 }, { -4, 1264, -2500, 0 }, { -3, -1264, 2489, 0 }, { -3, 1264, 2489, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2782, -1088, -354, 0 }, { { -3, -1264, 2492, 0 }, { -3, 1264, 2492, 0 }, { -3, -1264, -2498, 0 }, { -3, 1264, -2498, 0 } }, { 4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 928, -1024, 1199, 0 }, { { 687, -1264, 1236, 0 }, { 687, 1264, 1236, 0 }, { -687, -1264, -1235, 0 }, { -687, 1264, -1235, 0 } }, { 3587, 0, -1998, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 991, -1056, 959, 0 }, { { -685, -1264, -1237, 0 }, { -685, 1264, -1237, 0 }, { 686, -1264, 1237, 0 }, { 686, 1264, 1237, 0 } }, { -3582, 0, 1984, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1217, -1120, -1473, 0 }, { { 37, -1264, -1420, 0 }, { 37, 1264, -1420, 0 }, { -59, -1264, 1406, 0 }, { -59, 1264, 1406, 0 } }, { -4094, 0, -141, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1506, -1088, -1665, 0 }, { { 16, -1264, 1410, 0 }, { 16, 1264, 1410, 0 }, { -28, -1264, -1417, 0 }, { -28, 1264, -1417, 0 } }, { 4102, 0, -66, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4929, -1056, -1248, 0 }, { { 275, -1264, 1373, 0 }, { 275, 1264, 1373, 0 }, { -311, -1264, -1393, 0 }, { -311, 1264, -1393, 0 } }, { 4015, 0, -852, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4577, -1056, -1312, 0 }, { { -243, -1264, -1408, 0 }, { -243, 1264, -1408, 0 }, { 213, -1264, 1383, 0 }, { 213, 1264, 1383, 0 } }, { -4042, 0, 659, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5187, -1056, 703, 0 }, { { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 }, { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 } }, { -1044, 0, -3962, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5282, -1024, 512, 0 }, { { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 }, { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 } }, { 1044, 0, 3971, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1761, -1088, 1472, 0 }, { { -17, -1264, -1414, 0 }, { -17, 1264, -1414, 0 }, { 17, -1264, 1414, 0 }, { 17, 1264, 1414, 0 } }, { -4096, 0, 46, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2177, -1056, 1471, 0 }, { { 15, -1264, 1415, 0 }, { 15, 1264, 1415, 0 }, { -14, -1264, -1414, 0 }, { -14, 1264, -1414, 0 } }, { 4095, 0, -43, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_loft_8017E1BC[2] = {
    { NULL, NULL, NULL, { 4480, -48, -512, 0 }, { { -448, 0, -1024, 0 }, { 448, 0, -1024, 0 }, { -448, 0, 1024, 0 }, { 448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1264, -64, -80, 0 }, { { -5040, 0, -2672, 0 }, { 5040, 0, -2672, 0 }, { -5040, 0, 2672, 0 }, { 5040, 0, 2672, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 5701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Ten white point lights for the motel loft, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength through 2000 units and
/// fades to zero at 3000 units. The loaded room overlay owns the mutable array:
/// coordinate updates parent and compose its transforms, and lighting queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldMotelLoftPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5800, -1810, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4496, -1799, 2067 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1884, 1884, 1884 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5758, -1799, -1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1944, -1799, -2150 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -66, -1799, -2150 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 508, -1799, 2058 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1884, 1884, 1884 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4505, -1799, -2154 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3201, -1799, -10 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2805, -1799, -10 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2716, -1799, 2056 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1884, 1884, 1884 },
        },
        .inner = 2000,
        .outer = 3000,
    },
};

WorldCoordRoomLights D_dryfield_motel_loft_8017E614[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelLoftPointLights), _gDryfieldMotelLoftPointLights, 0, NULL },
};

s32 D_dryfield_motel_loft_8017E62C[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionSurfaceProperties D_dryfield_motel_loft_8017E638[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_loft_8017E640[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_loft_8017E648[8] = {
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E640,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
};

static void _dryfieldMotelLoftInitRoomTask(Task* task);
static void _dryfieldMotelLoftIdleRoomTask(Task* unusedTask);

/// Refuses every key-item-use request in the motel loft.
///
/// Ignores the collected item ID and other arguments. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, leaving the room and inventory unchanged.
static s32 _dryfieldMotelLoftRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition from the motel loft with its destination unchanged.
///
/// Borrows a readable eight-byte request and writable reply for synchronous
/// dispatch; they may be the same record. Copies the complete request for both
/// query and execution calls, retains neither pointer, and returns 1.
static s32 _dryfieldMotelLoftAcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_LOFT_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_LOFT_TRANSITION_ALLOWED;
}

/// Ignores room commands in the motel loft and returns zero.
///
/// The command word and other arguments are unused; no room state changes.
static s32 _dryfieldMotelLoftIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores directed room actions in the motel loft and returns zero.
///
/// The borrowed action request is neither read nor retained; all arguments
/// are unused and no room state changes.
static s32 _dryfieldMotelLoftIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the motel loft's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldMotelLoftInitRoomTask(Task* task)
{
    task->msgTable = D_dryfield_motel_loft_8017D6B4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the motel loft's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldMotelLoftIdleRoomTask(Task* unusedTask)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_loft_8017D5C4 = {
    { _dryfieldMotelLoftInitRoomTask, _dryfieldMotelLoftIdleRoomTask, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_loft_8017D5C4`.
void func_dryfield_motel_loft_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_loft_8017D5C4;
    sp.funcs[task->state](task);
}
