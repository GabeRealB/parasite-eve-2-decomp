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
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.raised
#include "../../shared/room_events.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The message and request the event gate latched for the event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

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
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_dilapidated_house_80189F68[3] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 40, 40, 1, 0, { 0, 0 }, D_80156500 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_dilapidated_house_80189F8C[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
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

WorldCoordRoomAmbientEntry D_dryfield_night_dilapidated_house_8018A054[12] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_dilapidated_house_8018A054) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 250, 250, 500, 281 } },
    { .color = { 500, 500, 750, 531 } },
    { .color = { 250, 250, 500, 281 } },
    { .color = { 250, 250, 500, 281 } },
    { .color = { 250, 250, 500, 281 } },
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

GpRoomParamRec* D_dryfield_night_dilapidated_house_8018A0E4[8] = {
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0D4,
    D_dryfield_night_dilapidated_house_8018A0DC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
    D_dryfield_night_dilapidated_house_8018A0CC,
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 192, 47, 192 } };

RoomEventReq gRoomEventReq;

static void func_dryfield_night_dilapidated_house_8017D970(Task* arg0);
static void func_dryfield_night_dilapidated_house_8017DA08(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

s32 func_dryfield_night_dilapidated_house_8017D8D4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for the room's `0x13EE` message, the warp destination the gameplay
/// side posts as `Gp_WarpLoc`: copies the incoming payload through to `out` and,
/// when the destination id is 5, offers the event gate a request that plays
/// the room's pair of stage sounds under flag nibble 0x3F. Returns 1 for a
/// destination it does not own.
s32 func_dryfield_night_dilapidated_house_8017D8DC(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    if (in->areaId == 5) {
        req.capCmd        = 0xC;
        req.missingCapCmd = 0xC;
        req.firstSnd      = 0x53090005;
        req.secondSnd     = 0x53090001;
        req.flagId        = 0x3F;
        req.collectedBit  = 0;
        return roomEventGate(&req, in);
    }
    return 1;
}

s32 func_dryfield_night_dilapidated_house_8017D960(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_dilapidated_house_8017D968(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
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
