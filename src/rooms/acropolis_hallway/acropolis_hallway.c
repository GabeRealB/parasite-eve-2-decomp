#include "rooms/acropolis_hallway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
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

extern GpMsgEntry D_acropolis_hallway_8017E238[];
extern SVECTOR    D_acropolis_hallway_8017FA4C;

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

extern GpGridParams   D_acropolis_hallway_8017E5D0[1];
extern GpObj4C        D_acropolis_hallway_8017E5F4[4];
extern GpObj4C        D_acropolis_hallway_8017E724[9];
extern GpRoomCoordSet D_acropolis_hallway_8017EBC4[1];
s32                   func_acropolis_hallway_8017D5D0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_acropolis_hallway_8017D72C(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_acropolis_hallway_8017D734(Task*, s32, s32, GpMessageArg);

GpMsgEntry D_acropolis_hallway_8017E238[4] = {
    { 5102, func_acropolis_hallway_8017D5D0 },
    { 5105, func_acropolis_hallway_8017D72C },
    { 5106, func_acropolis_hallway_8017D734 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_acropolis_hallway_8017E258[1] = {
    { D_acropolis_hallway_8017E5D0, D_acropolis_hallway_8017E5F4, D_acropolis_hallway_8017E724, NULL },
};

u8* D_acropolis_hallway_8017E268[1] = {
    D_8010CAF8,
};

GpViewCountRec D_acropolis_hallway_8017E26C[1] = {
    { { .bytes = { 5, 0 } } },
};

GpRoomCoordRec D_acropolis_hallway_8017E270[1] = {
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

GpGridFace D_acropolis_hallway_8017E4C0[17] = {
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

GpGridParams D_acropolis_hallway_8017E5D0[1] = {
    { NULL, D_acropolis_hallway_8017E358, D_acropolis_hallway_8017E3A8, D_acropolis_hallway_8017E4C0, D_acropolis_hallway_8017E5C8, 3250, 1250, 2, 1, 4000, 17 },
};

GpObj4C D_acropolis_hallway_8017E5F4[4] = {
    { NULL, NULL, NULL, { -1984, -2016, 0, 0 }, { { 0, -1792, -1760, 0 }, { 0, 1792, -1760, 0 }, { 0, -1792, 1760, 0 }, { 0, 1792, 1760, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 0, 0 }, 2508, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 320, -2016, 0, 0 }, { { -174, -1792, -1752, 0 }, { -174, 1792, -1752, 0 }, { 172, -1792, 1750, 0 }, { 172, 1792, 1750, 0 } }, { -4086, 0, 402, 0 }, { 0, 0, 0, 0 }, 2508, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 192, -1920, 0, 0 }, { { -174, 1792, -1752, 0 }, { -174, -1792, -1752, 0 }, { 172, 1792, 1750, 0 }, { 172, -1792, 1750, 0 } }, { 4084, 0, -404, 0 }, { 0, 0, 0, 0 }, 2508, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -2112, -1952, 0, 0 }, { { 0, 1792, -1760, 0 }, { 0, -1792, -1760, 0 }, { 0, 1792, 1760, 0 }, { 0, -1792, 1760, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 2508, 0, 3, 2, 129, 0 },
};

GpObj4C D_acropolis_hallway_8017E724[9] = {
    { NULL, NULL, NULL, { -2960, -1024, -384, 0 }, { { -272, 0, -736, 0 }, { 272, 0, -736, 0 }, { -272, 0, 736, 0 }, { 272, 0, 736, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 783, 0, 4, 67, 2, 0 },
    { NULL, NULL, NULL, { -768, -1016, 920, 0 }, { { -496, 0, -352, 0 }, { 496, 0, -352, 0 }, { -496, 0, 352, 0 }, { 496, 0, 352, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 607, 0, 4, 49, 2, 0 },
    { NULL, NULL, NULL, { 2304, -1024, 928, 0 }, { { -496, 0, -352, 0 }, { 496, 0, -352, 0 }, { -496, 0, 352, 0 }, { 496, 0, 352, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 607, 0, 6, 33, 2, 0 },
    { NULL, NULL, NULL, { 2944, -1024, -672, 0 }, { { 288, 0, -496, 0 }, { 288, 0, 496, 0 }, { -288, 0, -496, 0 }, { -288, 0, 496, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 572, 0, 8, 19, 2, 0 },
    { NULL, NULL, NULL, { 800, -1088, -928, 0 }, { { -560, 0, -288, 0 }, { 560, 0, -288, 0 }, { -560, 0, 288, 0 }, { 560, 0, 288, 0 } }, { 0, 4101, 0, 0 }, { -201, 0, 4091, 0 }, 627, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 3184, -1056, 368, 0 }, { { -1056, 0, -912, 0 }, { 128, 0, -912, 0 }, { -1056, 0, 208, 0 }, { 128, 0, 208, 0 } }, { 0, 4113, 0, 0 }, { -4076, 0, 401, 0 }, 1390, 2, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 736, -1056, 848, 0 }, { { -528, 0, -240, 0 }, { 528, 0, -240, 0 }, { -528, 0, 432, 0 }, { 528, 0, 432, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 680, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { -784, -1056, -864, 0 }, { { -832, 0, -368, 0 }, { 832, 0, -368, 0 }, { -832, 0, 240, 0 }, { 832, 0, 240, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, 4091, 0 }, 909, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -2128, -1042, -1024, 0 }, { { -400, 0, -304, 0 }, { 400, 0, -304, 0 }, { -400, 0, 304, 0 }, { 400, 0, 304, 0 } }, { 0, 4100, 0, 0 }, { 200, 0, 4090, 0 }, 501, 2, 6, 0, 130, 0 },
};

GpAreaTmdRec D_acropolis_hallway_8017E9D0[2] = {
    { 7, 7, 2, 0, { 0, 0 }, D_801693AC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_hallway_8017E9E8[2] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_hallway_8017EA00[2] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_hallway_8017EA18[3] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 7, 7, 2, 0, { 0, 0 }, D_801693AC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
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

GpPointLight D_acropolis_hallway_8017EAA4[3] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2374, -2432, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1360, 2384 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2384, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1360, 2384 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2297, -2384, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1360, 2384 },
};

GpRoomCoordSet D_acropolis_hallway_8017EBC4[1] = {
    { 0, NULL, 3, D_acropolis_hallway_8017EAA4, 0, NULL },
};

GpSprtCmd D_acropolis_hallway_8017EBDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_hallway_8017EBEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_hallway_8017EBFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_hallway_8017EC0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_hallway_8017EC1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_acropolis_hallway_8017EC2C[5] = {
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

s32 D_acropolis_hallway_8017ED1C[3] = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

GpRoomParamRec D_acropolis_hallway_8017ED28[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_acropolis_hallway_8017ED30[1] = {
    { 0, 1, 0, 0, D_acropolis_hallway_8017ED1C },
};

GpRoomParamRec D_acropolis_hallway_8017ED38[1] = {
    { 0, 0, 1, 0, D_acropolis_hallway_8017ED1C },
};

GpRoomParamRec* D_acropolis_hallway_8017ED40[8] = {
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

SVECTOR D_acropolis_hallway_8017FA4C = { 0 };

static s32  func_acropolis_hallway_8017D830(GfxCoord* coord, GpRec18* rec, s16 arg2);
static s32  func_acropolis_hallway_8017D9D4(GfxCoord* coord, GpRec18* recs, s16 count, s16 push);
static void func_acropolis_hallway_8017E1C0(Task* task);

/// Message gate for the hallway's first hotspot: copies the incoming record to
/// the outgoing one, then edits the copy's `field_3` (the answer the caller
/// acts on) according to the message id and the room's progress nibbles.
/// Returning 0 means the message was consumed.
s32 func_acropolis_hallway_8017D5D0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 msgId;

    *out = *in;
    if (in->prefix.packed == 8) {
        if ((GameFlag_GetNibble(9) & 2) && in->field_5 == 0) {
            out->field_3 = 2;
        }
    }
    if (in->prefix.packed == 4 && in->field_2 == 3 && GameFlag_GetNibble(0) < 3) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->prefix.packed == 8 && GameFlag_GetNibble(0) == 3) {
        return 1;
    }
    msgId = in->prefix.packed;
    if (msgId == 4 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0) >= 3) {
            out->field_3 = msgId;
        }
        if (GameFlag_GetNibble(0) == 2) {
            out->field_3 = 3;
        }
    }
    return 1;
}

/// Message handler that accepts the message and does nothing else.
s32 func_acropolis_hallway_8017D72C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_acropolis_hallway_8017D734(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
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

/// Gets a 16.16 X/Y/Z displacement for `rec` from `func_800E0C10` and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_hallway_8017FA4C`. Returns non-zero when the X or Z
/// displacement is non-zero.
static s32 func_acropolis_hallway_8017D830(GfxCoord* coord, GpRec18* rec, s16 arg2)
{
    OverlayDeltaFlag* s;
    s32               val;

    s        = SCRATCH_PUSH(OverlayDeltaFlag);
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]              += s->delta.vx.w >> 16;
        coord->coord.t[2]              += s->delta.vz.w >> 16;
        D_acropolis_hallway_8017FA4C.vx = s->delta.vx.w >> 16;
        D_acropolis_hallway_8017FA4C.vy = s->delta.vy.w >> 16;
        D_acropolis_hallway_8017FA4C.vz = s->delta.vz.w >> 16;
        val                             = s->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_hallway_8017FA4C.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_hallway_8017FA4C.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_hallway_8017FA4C.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_hallway_8017FA4C.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP(OverlayDeltaFlag);
    return s->moved;
}

/// Measures the bearing of each type-1 or type-3 record in `recs` (up to
/// `count`, or the first zero key) from the coordinate's world position,
/// relative to the direction it faces. For a record that has every other such
/// record within a quarter turn of it, moves the coordinate `push` units back
/// along that record's bearing, in X and Z. Returns non-zero if it moved the
/// coordinate; returns 0 at once while `gGameSession->viewReady` is 1.
static s32 func_acropolis_hallway_8017D9D4(GfxCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    OverlayBisectorScratch* st;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_PUSH(OverlayBisectorScratch);
    st         = SCRATCH_HEAD(OverlayBisectorScratch);
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    st->eye.vz = (u16)coord->coord.t[2];

    overlayToWorld(coord->parent, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->angle[st->i] = ratan2(st->delta.vx, st->delta.vz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)st->aim.vz - (u16)st->eye.vz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, st->delta.vz);

            st->angle[st->i] = overlayWrapAngle(st->angle[st->i]);
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                st->diff = overlayWrapAngle(st->diff);
                if (abs(st->diff) > 0x400) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    hit = st->hit;
    SCRATCH_POP(OverlayBisectorScratch);
    return hit;
}

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
        tmd->flags    = 8;
        tmd->otOffset = 0;
        task->state++;
    }
    if (Gp_GetViewIndex() == 5) {
        tmd->flags = 8;
    } else {
        tmd->flags = 0x80;
    }
    if (flag == 2) {
        tmd->flags = 0x80;
    }
}

/// Model task step for a pickup's mesh: when the pickup's 2-bit flag reads 2
/// it sets mesh flag 4, otherwise it resets the mesh flags and draw offset and
/// allocates the mesh's TMD buffers. The view index is fetched but unused.
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
        tmd->flags |= 4;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
