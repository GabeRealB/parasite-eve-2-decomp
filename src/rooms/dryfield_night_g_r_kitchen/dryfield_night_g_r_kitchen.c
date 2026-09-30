#include "rooms/dryfield_night_g_r_kitchen.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_dryfield_night_g_r_kitchen_8017EC2C[4];

/// The event the room's gate `func_dryfield_night_g_r_kitchen_8017D5E8`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `D_dryfield_night_g_r_kitchen_8017E248`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg D_dryfield_night_g_r_kitchen_8017EC24;
extern RoomEventReq D_dryfield_night_g_r_kitchen_8017EC30;
extern TaskDesc     D_dryfield_night_g_r_kitchen_8017E248;

/// The room's message table, `(msgId, handler)` pairs ending at 0x7FFFFFFF,
/// which the entry task installs as its own `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_g_r_kitchen_8017E254[];

/// The two pairs of world points the room's light shafts run between, one
/// pair per `SVECTOR[2]`: the first array holds the two shafts drawn in view
/// 2, the second the two drawn in view 3.
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E27C[];
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E29C[];

void func_dryfield_night_g_r_kitchen_8017D74C(Task*);
s32  func_dryfield_night_g_r_kitchen_8017D8BC(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_night_g_r_kitchen_8017D8C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_night_g_r_kitchen_8017D948(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_night_g_r_kitchen_8017D950(Task*, s32, TaskMessageArg, TaskMessageArg);

extern GpGridParams   D_dryfield_night_g_r_kitchen_8017E554[1];
extern GpObj4C        D_dryfield_night_g_r_kitchen_8017E864[2];
extern GpObj4C        D_dryfield_night_g_r_kitchen_8017E8FC[7];
extern GpRoomCoordSet D_dryfield_night_g_r_kitchen_8017E84C[1];

TaskDesc D_dryfield_night_g_r_kitchen_8017E248 = { 0, 32, func_dryfield_night_g_r_kitchen_8017D74C, { .model = NULL } };

GpMsgEntry D_dryfield_night_g_r_kitchen_8017E254[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_g_r_kitchen_8017D8C4 },
    { 5105, func_dryfield_night_g_r_kitchen_8017D8BC },
    { 5103, func_dryfield_night_g_r_kitchen_8017D950 },
    { 5104, func_dryfield_night_g_r_kitchen_8017D948 },
    { 0x7FFFFFFF, NULL },
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

GpRoomCoordRec D_dryfield_night_g_r_kitchen_8017E2BC[1] = {
    { D_dryfield_night_g_r_kitchen_8017E84C, NULL },
};

GpRoomObjRec D_dryfield_night_g_r_kitchen_8017E2C4[1] = {
    { D_dryfield_night_g_r_kitchen_8017E554, D_dryfield_night_g_r_kitchen_8017E864, D_dryfield_night_g_r_kitchen_8017E8FC, NULL },
};

u8* D_dryfield_night_g_r_kitchen_8017E2D4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_g_r_kitchen_8017E2D8[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_dryfield_night_g_r_kitchen_8017E2DC[2] = {
    { { .words = { 1024, -1168, 0, 2135 } }, { 0, 0, 0, 0 }, { .words = { 1024, -1168, 0, 2135 } }, { 0, 0, 0, 0 }, 0x53130002, 0x53130001, 0, 2, 0, 478 },
    { { .words = { 2048, 0, 0, 2512 } }, { 0, 0, 0, 0 }, { .words = { 2048, 0, 0, 2512 } }, { 0, 0, 0, 0 }, 0x53130002, 0x53130001, 0, 2, 0, 477 },
};

SVECTOR D_dryfield_night_g_r_kitchen_8017E34C[6] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_normals.inc"
};

SVECTOR D_dryfield_night_g_r_kitchen_8017E37C[26] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_verts.inc"
};

GpGridFace D_dryfield_night_g_r_kitchen_8017E44C[16] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_faces.inc"
};

s16 D_dryfield_night_g_r_kitchen_8017E50C[32] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_g_r_kitchen_8017E50C[i])
s16* D_dryfield_night_g_r_kitchen_8017E54C[2] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_g_r_kitchen_8017E554[1] = {
    { NULL, D_dryfield_night_g_r_kitchen_8017E34C, D_dryfield_night_g_r_kitchen_8017E37C, D_dryfield_night_g_r_kitchen_8017E44C, D_dryfield_night_g_r_kitchen_8017E54C, 1800, 3000, 1, 2, 4000, 16 },
};

GpViewRec D_dryfield_night_g_r_kitchen_8017E578[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 1053 },
    { { { { 3955, 0, 1064 }, { 591, 3405, -2198 }, { -885, 2276, 3288 } }, { -400, 2700, 1000 } }, 230 },
    { { { { -3988, 0, 933 }, { -276, 3911, -1183 }, { -891, -1215, -3808 } }, { -500, 400, -2700 } }, 230 },
};

SpriteBatch D_dryfield_night_g_r_kitchen_8017E5E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_g_r_kitchen_8017E5F4[7] = {
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

GpSprtRec D_dryfield_night_g_r_kitchen_8017E6A8[3] = {
    { { .empty = D_dryfield_night_g_r_kitchen_8017E5E4 }, D_dryfield_night_g_r_kitchen_8017E5E4, NULL },
    { { .elements = D_dryfield_night_g_r_kitchen_8017E5F4 }, D_dryfield_night_g_r_kitchen_8017E680, NULL },
    { { .empty = D_dryfield_night_g_r_kitchen_8017E698 }, D_dryfield_night_g_r_kitchen_8017E698, NULL },
};

GpPointLight D_dryfield_night_g_r_kitchen_8017E6CC[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2400, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 3200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1800, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1300, -1800, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 0x186A0, 0x186A0 },
};

GpRoomCoordSet D_dryfield_night_g_r_kitchen_8017E84C[1] = {
    { 0, NULL, 4, D_dryfield_night_g_r_kitchen_8017E6CC, 0, NULL },
};

GpObj4C D_dryfield_night_g_r_kitchen_8017E864[2] = {
    { NULL, NULL, NULL, { -2, -1167, 381, 0 }, { { 1974, -1520, 391, 0 }, { -1973, -1520, -390, 0 }, { 1974, 1520, 391, 0 }, { -1973, 1520, -390, 0 } }, { -796, 0, 4017, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -1, -1152, 479, 0 }, { { -1973, -1520, -390, 0 }, { 1974, -1520, 391, 0 }, { -1973, 1520, -390, 0 }, { 1974, 1520, 391, 0 } }, { 794, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 3, 2, 129, 0 },
};

GpObj4C D_dryfield_night_g_r_kitchen_8017E8FC[7] = {
    { NULL, NULL, NULL, { -1248, -48, 2336, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, 0, 18, 18, 2, 0 },
    { NULL, NULL, NULL, { 96, -48, 2784, 0 }, { { 736, 0, -320, 0 }, { 736, 0, 320, 0 }, { -736, 0, -320, 0 }, { -736, 0, 320, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 801, 0, 20, 33, 2, 0 },
    { NULL, NULL, NULL, { -576, -64, -352, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 336, -64, -2400, 0 }, { { -688, 0, -576, 0 }, { 688, 0, -576, 0 }, { -688, 0, 576, 0 }, { 688, 0, 576, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 896, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -544, -64, 960, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { -608, -64, -1600, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 784, -64, -912, 0 }, { { -464, 0, -1296, 0 }, { 464, 0, -1296, 0 }, { -464, 0, 1296, 0 }, { 464, 0, 1296, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1372, 2, 4, 0, 130, 0 },
};

GpAreaTmdRec D_dryfield_night_g_r_kitchen_8017EB10[3] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 7, 7, 1, 0, { 0, 0 }, D_80150C80 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_g_r_kitchen_8017EB34[4] = {
    { 40, 40, 0, 0, { 0, 0 }, D_8013E500 },
    { 7, 7, 1, 0, { 0, 0 }, D_80150C80 },
    { 8, 7, 1, 0, { 0, 0 }, D_801513C8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_g_r_kitchen_8017EB64[3] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 40, 40, 1, 0, { 0, 0 }, D_80156500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_g_r_kitchen_8017EB88[12] = {
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

s32 D_dryfield_night_g_r_kitchen_8017EBE8[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

GpRoomParamRec D_dryfield_night_g_r_kitchen_8017EBF4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_g_r_kitchen_8017EBFC[1] = {
    { 0, 0, 1, 0, D_dryfield_night_g_r_kitchen_8017EBE8 },
};

GpRoomParamRec* D_dryfield_night_g_r_kitchen_8017EC04[8] = {
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBFC,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
    D_dryfield_night_g_r_kitchen_8017EBF4,
};

RoomEventMsg D_dryfield_night_g_r_kitchen_8017EC24 = { 0 };

u8 D_dryfield_night_g_r_kitchen_8017EC2C[4] = {
    0,
    0,
    222,
    254,
};

RoomEventReq D_dryfield_night_g_r_kitchen_8017EC30;

static s32  func_dryfield_night_g_r_kitchen_8017D5E8(RoomEventReq* req, RoomEventMsg* msg);
static void func_dryfield_night_g_r_kitchen_8017D958(Task* task);
static void func_dryfield_night_g_r_kitchen_8017D99C(Task* task);
static void func_dryfield_night_g_r_kitchen_8017D9FC(SVECTOR* arg0, s32 arg1);

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `queryOnly` on the message only reports the
/// answer, with none of the side effects.
static s32 func_dryfield_night_g_r_kitchen_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                     = req->flagId;
    D_dryfield_night_g_r_kitchen_8017EC2C[0] = 0;
    neg                                      = flag < 0;
    got                                      = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
                D_dryfield_night_g_r_kitchen_8017EC24 = *msg;
                D_dryfield_night_g_r_kitchen_8017EC30 = *req;
                id                                    = req->flagId;
                mode                                  = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_g_r_kitchen_8017E248, 0, 0, 0);
                D_dryfield_night_g_r_kitchen_8017EC2C[0] = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->flagId, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's CAP command,
/// plays its two sound events in turn and waits for each to finish, then
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11 to load it.
void func_dryfield_night_g_r_kitchen_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_g_r_kitchen_8017EC30.field_0);
            if (D_dryfield_night_g_r_kitchen_8017EC30.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_g_r_kitchen_8017EC30.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_g_r_kitchen_8017EC30.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_g_r_kitchen_8017EC30.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_g_r_kitchen_8017EC30.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_g_r_kitchen_8017EC30.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_night_g_r_kitchen_8017EC24.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_night_g_r_kitchen_8017EC24.warp;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_dryfield_night_g_r_kitchen_8017EC24.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D8BC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE, the first entry of its message
/// table. It copies the incoming record to `out`; for a record whose first
/// halfword is 0x14 it builds the room's event request -- flag nibble 0x34, no
/// prerequisite item, CAP commands 3 and 3 and two sounds -- and answers what
/// the event gate answers. Everything else answers 1.
s32 func_dryfield_night_g_r_kitchen_8017D8C4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    if (in->areaId == 0x14) {
        req.field_0 = 3;
        req.field_4 = 3;
        req.field_8 = 0x53130001;
        req.field_C = 0x53130004;
        req.flagId  = 0x34;
        req.itemId  = 0;
        return func_dryfield_night_g_r_kitchen_8017D5E8(&req, in);
    }
    return 1;
}

/// The room's handler for message 0x13F0: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D948(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D950(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
static void func_dryfield_night_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_night_g_r_kitchen_8017E254;
    Game_SetPtrSlot(task, 7);
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

/// Draws a light shaft between the two world points `arg0[0]` and `arg0[1]`:
/// a fan of gouraud wedges around each projected point, joined by wedges
/// spanning the two, the sweep oriented along the screen-space line between
/// them. Each radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is
/// drawn unless both points project. The lit vertices take a brightness that
/// flickers with the frame counter.
static void func_dryfield_night_g_r_kitchen_8017D9FC(SVECTOR* arg0, s32 arg1)
{
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1 = arg0 + 1;
    SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);
    block = SCRATCH_STACK_CURSOR(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd    = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);

                    prim           = gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayPointPairScratch);
}

/// Picks the pair of light shafts `func_dryfield_night_g_r_kitchen_8017D9FC`
/// draws from the current view index (`gGameSession->location.loc.view`, 2 or 3);
/// any other view draws nothing.
void func_dryfield_night_g_r_kitchen_8017E1E4(Task* unused)
{
    u8 view;

    view = gGameSession->location.loc.view;
    if (view == 2) {
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E27C[0], 0x100);
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E27C[2], 0x100);
    } else if (view == 3) {
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E29C[0], 0x100);
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E29C[2], 0x100);
    }
}
