#include "rooms/dryfield_night_dilapidated_house.h"

#include "types.h"

#include "dryfield_night_dilapidated_house_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_dryfield_night_dilapidated_house_8018A10C[4];

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_dryfield_night_dilapidated_house_8018A104;
extern RoomEventReq D_dryfield_night_dilapidated_house_8018A110;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

GpRoomCoordSet D_dryfield_night_dilapidated_house_80189B60[1] = {
    { 0, NULL, 8, D_dryfield_night_dilapidated_house_80189500, 1, D_dryfield_night_dilapidated_house_80189800.active },
};

GpObj4C D_dryfield_night_dilapidated_house_80189B78[12] = {
    { NULL, NULL, NULL, { 1712, -48, -2768, 0 }, { { -624, 0, -304, 0 }, { 624, 0, -304, 0 }, { -624, 0, 304, 0 }, { 624, 0, 304, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 692, 0, 5, 20, 2, 0 },
    { NULL, NULL, NULL, { -5680, -63, 192, 0 }, { { 304, 0, -624, 0 }, { 304, 0, 624, 0 }, { -304, 0, -624, 0 }, { -304, 0, 624, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 692, 0, 7, 34, 2, 0 },
    { NULL, NULL, NULL, { -4688, -64, 352, 0 }, { { -512, 0, -608, 0 }, { 512, 0, -608, 0 }, { -512, 0, 608, 0 }, { 512, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 794, 0x4002, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { -4704, -64, -944, 0 }, { { -512, 0, -560, 0 }, { 512, 0, -560, 0 }, { -512, 0, 560, 0 }, { 512, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { -4091, 0, 201, 0 }, 757, 0x4002, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { -3472, -64, -496, 0 }, { { -336, 0, -928, 0 }, { 1456, 0, -928, 0 }, { -336, 0, 928, 0 }, { 1136, 0, 928, 0 } }, { 0, 4107, 0, 0 }, { -4091, 0, 201, 0 }, 1722, 0x4002, 5, 0, 4, 0 },
    { NULL, NULL, NULL, { -1104, -64, -2800, 0 }, { { -1264, 0, -288, 0 }, { 912, 0, -288, 0 }, { -1264, 0, 1024, 0 }, { 912, 0, 1024, 0 } }, { 0, 4105, 0, 0 }, { -4091, 0, 201, 0 }, 1624, 0x4002, 18, 0, 4, 0 },
    { NULL, NULL, NULL, { -5280, -64, 704, 0 }, { { -624, 0, -224, 0 }, { 624, 0, -224, 0 }, { -624, 0, 224, 0 }, { 624, 0, 224, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 662, 0x4002, 17, 0, 2, 0 },
    { NULL, NULL, NULL, { -5120, -64, -2720, 0 }, { { -720, 0, -288, 0 }, { 720, 0, -288, 0 }, { -720, 0, 288, 0 }, { 720, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 773, 0x4002, 23, 0, 2, 0 },
    { NULL, NULL, NULL, { -2448, -64, 2688, 0 }, { { -544, 0, -288, 0 }, { 544, 0, -288, 0 }, { -544, 0, 288, 0 }, { 544, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 613, 2, 21, 0, 2, 0 },
    { NULL, NULL, NULL, { 1072, -64, 2688, 0 }, { { -576, 0, -288, 0 }, { 576, 0, -288, 0 }, { -576, 0, 288, 0 }, { 576, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 643, 2, 21, 0, 2, 0 },
    { NULL, NULL, NULL, { -2400, -64, -2752, 0 }, { { -720, 0, -288, 0 }, { 720, 0, -288, 0 }, { -720, 0, 288, 0 }, { 720, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 773, 2, 22, 0, 2, 0 },
    { NULL, NULL, NULL, { -928, -64, 2688, 0 }, { { -544, 0, -288, 0 }, { 544, 0, -288, 0 }, { -544, 0, 288, 0 }, { 544, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 613, 2, 21, 0, 130, 0 },
};

GpObj3A D_dryfield_night_dilapidated_house_80189F08[1] = {
    { NULL, NULL, { -4096, -2000, 1232, 0 }, { { 0, 2576, -2544, 0 }, { 0, -2576, -2544, 0 }, { 0, 2576, 2544, 0 }, { 0, -2576, 2544, 0 } }, { 4097, 0, 0, 0 }, { 36, 14 }, 129, 0 },
};

GpAreaTmdRec D_dryfield_night_dilapidated_house_80189F44[3] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 40, 40, 1, 0, { 0, 0 }, D_80156500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_dilapidated_house_80189F68[3] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 40, 40, 1, 0, { 0, 0 }, D_80156500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_dilapidated_house_80189F8C[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_dilapidated_house_80189FA4[22] = {
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B558, D_dryfield_night_dilapidated_house_80189F44 },
    { D_map_dryfield_full_8017B5D8, D_dryfield_night_dilapidated_house_80189F68 },
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
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B648, D_dryfield_night_dilapidated_house_80189F8C },
};

GpRoomBoundVec D_dryfield_night_dilapidated_house_8018A054[12] = {
    { 11, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 250, 250, 500, 281 },
    { 500, 500, 750, 531 },
    { 250, 250, 500, 281 },
    { 250, 250, 500, 281 },
    { 250, 250, 500, 281 },
};

s32 D_dryfield_night_dilapidated_house_8018A0B4[3] = {
    0x10000045,
    0x10000047,
    0x10000045,
};

s32 D_dryfield_night_dilapidated_house_8018A0C0[3] = {
    0x1000004D,
    0x1000004F,
    0x1000004D,
};

GpRoomParamRec D_dryfield_night_dilapidated_house_8018A0CC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_dilapidated_house_8018A0D4[1] = {
    { 0, 0, 1, 0, D_dryfield_night_dilapidated_house_8018A0B4 },
};

GpRoomParamRec D_dryfield_night_dilapidated_house_8018A0DC[1] = {
    { 0, 0, 1, 0, D_dryfield_night_dilapidated_house_8018A0C0 },
};

GpRoomParamRec * D_dryfield_night_dilapidated_house_8018A0E4[8] = {
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0D4,
    D_dryfield_night_dilapidated_house_8018A0DC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
};

RoomEventMsg D_dryfield_night_dilapidated_house_8018A104 = { 0 };

u8 D_dryfield_night_dilapidated_house_8018A10C[4] = {
    0,
    192,
    47,
    192,
};

RoomEventReq D_dryfield_night_dilapidated_house_8018A110;

static s32  func_dryfield_night_dilapidated_house_8017D600(RoomEventReq* req, RoomEventMsg* msg);
static void func_dryfield_night_dilapidated_house_8017D970(Task* arg0);
static void func_dryfield_night_dilapidated_house_8017DA08(Task* task);

/// The room's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
static s32 func_dryfield_night_dilapidated_house_8017D600(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                        = req->flagId;
    D_dryfield_night_dilapidated_house_8018A10C[0] = 0;
    neg                                         = flag < 0;
    got                                         = (s16)flag;
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
            if (msg->field_5 == 0) {
                D_dryfield_night_dilapidated_house_8018A104 = *msg;
                D_dryfield_night_dilapidated_house_8018A110 = *req;
                id                                          = req->flagId;
                mode                                        = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_dilapidated_house_8017E6F4, 0, 0, 0);
                D_dryfield_night_dilapidated_house_8018A10C[0] = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns. It raises `Gp_StateF0.field_4`, runs the latched
/// request's CAP command, plays its two stage sounds in turn (either may be
/// absent) waiting for each voice to finish, then stores the latched
/// message's `msgId`, `field_2` and `field_3` as the save location's area,
/// warp and room, spawns task 0x11 and kills itself.
void func_dryfield_night_dilapidated_house_8017D764(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_dilapidated_house_8018A110.field_0);
            if (D_dryfield_night_dilapidated_house_8018A110.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_dilapidated_house_8018A110.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_dilapidated_house_8018A110.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_dilapidated_house_8018A110.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_dilapidated_house_8018A110.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_dilapidated_house_8018A110.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant   = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_night_dilapidated_house_8018A104.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_night_dilapidated_house_8018A104.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_dryfield_night_dilapidated_house_8018A104.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_dryfield_night_dilapidated_house_8017D8D4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for the room's `0x13EE` message, the warp destination the gameplay
/// side posts as `Gp_WarpLoc`: copies the incoming payload through to `out` and,
/// when the destination id is 5, offers the event gate a request that plays
/// the room's pair of stage sounds under flag nibble 0x3F. Returns 1 for a
/// destination it does not own.
s32 func_dryfield_night_dilapidated_house_8017D8DC(Task* task, s32 msgId, RoomEventMsg * in, RoomEventMsg * out)
{
    RoomEventReq req;

    *out = *in;
    if (in->prefix.packed == 5) {
        req.field_0 = 0xC;
        req.field_4 = 0xC;
        req.field_8 = 0x53090005;
        req.field_C = 0x53090001;
        req.flagId  = 0x3F;
        req.itemId  = 0;
        return func_dryfield_night_dilapidated_house_8017D600(&req, in);
    }
    return 1;
}

s32 func_dryfield_night_dilapidated_house_8017D960(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_dilapidated_house_8017D968(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and advances. On the first visit (flag nibble 0x92 still
/// clear) it starts the cutscene script pair when pointer slot 0xA is filled,
/// then sets nibble 0x92 to 1 and nibble 0x7A to 3 and calls
/// `func_800E3FAC(0xA2, 0x11)`.
static void func_dryfield_night_dilapidated_house_8017D970(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_dilapidated_house_8017E700;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    if (GameFlag_GetNibble(0x92) == 0) {
        if (gameGetPtrSlot(0xA) != 0) {
            func_800E8634(D_dryfield_night_dilapidated_house_801868F4, 0,
                          D_dryfield_night_dilapidated_house_80187134);
        }
        GameFlag_SetNibble(0x92, 1);
        GameFlag_SetNibble(0x7A, 3);
        func_800E3FAC(0xA2, 0x11);
    }
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
static void func_dryfield_night_dilapidated_house_8017DA08(Task* task)
{
    char pad[0x10];
}

/// The room task's three states: setup, idle, and exit.
static const TaskFuncTable3 D_dryfield_night_dilapidated_house_8017D5DC = {
    {
        func_dryfield_night_dilapidated_house_8017D970,
        func_dryfield_night_dilapidated_house_8017DA08,
        taskKill,
    },
};

/// Runs the room task's current state, through a copy of its state table
/// taken onto the stack.
void func_dryfield_night_dilapidated_house_8017DA18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_dilapidated_house_8017D5DC;
    sp.funcs[task->state](task);
}

/// Cutscene script callback: queues the replacement of overlay 0x82.
void func_dryfield_night_dilapidated_house_8017DA70(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Cutscene script callback: queues overlay 0x81.
void func_dryfield_night_dilapidated_house_8017DA90(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Cutscene script callback: restores the stream random state.
void func_dryfield_night_dilapidated_house_8017DAB0(void)
{
    Gp_RestoreStreamRng();
}

/// Cutscene script callback: clears the queued CD command and restarts the CD
/// queue.
void func_dryfield_night_dilapidated_house_8017DAD0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

/// Cutscene script callback: spawns the first task of the room's two-entry
/// descriptor table, the one that starts the streamed sequence.
void func_dryfield_night_dilapidated_house_8017DAF0(void)
{
    Task_SpawnFromTable(D_dryfield_night_dilapidated_house_801872B4, 0, 0, 0);
}
