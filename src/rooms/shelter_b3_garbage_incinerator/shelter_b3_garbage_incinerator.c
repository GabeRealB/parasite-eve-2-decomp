#include "types.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* D_shelter_b3_garbage_incinerator_801855D8;

u16 D_shelter_b3_garbage_incinerator_801855DC;

#include "rooms/shelter_b3_garbage_incinerator.h"

#include "shelter_b3_garbage_incinerator_private.h"

#include "gameplay/captions.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"

#include "mapui/map_shelter.h"

extern TaskDesc   D_8016BFE0;
extern TaskDesc   D_801449F4;
extern GpMsgEntry D_shelter_b3_garbage_incinerator_80185594[];

extern TaskDesc D_shelter_b3_garbage_incinerator_801855CC;

static void func_shelter_b3_garbage_incinerator_8017DB7C(Task* task);
static void func_shelter_b3_garbage_incinerator_8017DC54(Task* task);

/// State handlers of the room's controller task, run by
/// `func_shelter_b3_garbage_incinerator_8017DC7C`: set-up, a per-frame tick,
/// and the kill.
static const TaskFuncTable3 D_shelter_b3_garbage_incinerator_8017D5C4 = { {
    func_shelter_b3_garbage_incinerator_8017DB7C,
    func_shelter_b3_garbage_incinerator_8017DC54,
    taskKill,
} };

void func_shelter_b3_garbage_incinerator_8017D6EC(Task*);
s32  func_shelter_b3_garbage_incinerator_8017D838(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b3_garbage_incinerator_8017D840(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b3_garbage_incinerator_8017D9B4(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b3_garbage_incinerator_8017D9BC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b3_garbage_incinerator_8017DA74(Task*, s32, s32, TaskMessageArg);
s32  func_shelter_b3_garbage_incinerator_8017DB2C(Task*, s32, s32, TaskMessageArg);

GpMsgEntry D_shelter_b3_garbage_incinerator_80185594[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b3_garbage_incinerator_8017D840 },
    { 5105, func_shelter_b3_garbage_incinerator_8017D838 },
    { 5103, func_shelter_b3_garbage_incinerator_8017D9BC },
    { 5104, func_shelter_b3_garbage_incinerator_8017D9B4 },
    { 5108, func_shelter_b3_garbage_incinerator_8017DA74 },
    { 5106, func_shelter_b3_garbage_incinerator_8017DB2C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b3_garbage_incinerator_801855CC = { 0, 32, func_shelter_b3_garbage_incinerator_8017D6EC, { .model = NULL } };

u16 D_shelter_b3_garbage_incinerator_801855DE;

void func_shelter_b3_garbage_incinerator_8017D6EC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StateF0.field_4 = 1;
            Gp_RunCapCmd(0x12, 0);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0) {
                Gp_StateF0.field_4 = 0;
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
            } else {
                Gp_EnqueueStageSnd6(0x54280006, 0, 0);
                arg0->state++;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(0x54280006) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b3_garbage_incinerator_8018FC2C.areaId;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b3_garbage_incinerator_8018FC2C.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_shelter_b3_garbage_incinerator_8018FC2C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b3_garbage_incinerator_8017D838(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017D840(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == 0x29) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        Gp_StateF0.field_4 = 1;
        if (gGameSession->location.loc.room < 4) {
            Gp_SpawnIfCapIdle(3, 1);
            return 0;
        }
        if (GameFlag_GetNibble(0x73) != 0) {
            GameFlag_SetNibble(0x4C, 10);
        } else {
            GameFlag_SetNibble(0x4C, 5);
        }
        out->warp                                 = 4;
        D_shelter_b3_garbage_incinerator_8018FC2C = *out;
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
        Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_801855CC, 0, 0, 0);
        return 2;
    }
    if (in->areaId == 0x27 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = gGameSession->incineratorRoomGroup + 1;
    }
    return 1;
}

s32 func_shelter_b3_garbage_incinerator_8017D9B4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017D9BC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->warp == 2 && gGameSession->incineratorExitPhase == GAME_SESSION_INCINERATOR_EXIT_NONE) {
        if (gGameSession->incineratorDescentPhase == GAME_SESSION_INCINERATOR_DESCENT_COMPLETE) {
            Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_801855E0, 0, 0, 0);
            gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_WARP;
        } else if (D_shelter_b3_garbage_incinerator_801855DC >= 0x3D) {
            SndEvt_EnqueueType6(0x5428000D, 0, 0);
            func_shelter_b3_garbage_incinerator_80180FE4(0x16, 0, 0x3C);
            D_shelter_b3_garbage_incinerator_801855DC = 0;
        }
    }
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017DA74(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 0:
            Gp_DispatchMsg(D_shelter_b3_garbage_incinerator_801855D8, 0x13F4, 0, 0);
            break;
        case 1:
            gGameSession->skipEventIntro = 1;
            Task_SpawnFromTable(&D_801449F4, 0, 0, 0);
            break;
        case 2:
            gGameSession->skipEventIntro              = 1;
            D_shelter_b3_garbage_incinerator_801855DE = 1;
            Task_SpawnFromTable(&D_801449F4, 0, 1, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017DB2C(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 9:
            SndEvt_EnqueueType6(0x54280009, 0, 0);
            break;
        case 10:
            SndEvt_EnqueueType6(0x5428000A, 0, 0);
            break;
    }
    return 0;
}

static void func_shelter_b3_garbage_incinerator_8017DB7C(Task* task)
{
    task->msgTable = D_shelter_b3_garbage_incinerator_80185594;
    Game_SetPtrSlot(task, 7);
    func_shelter_b3_garbage_incinerator_8018108C(0x180, 0, 0);
    D_shelter_b3_garbage_incinerator_801855D8 = Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80185BA0, 0, 0, 0);
    if (gGameSession->location.loc.room >= 4) {
        Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 0, 0);
    }
    if (gGameSession->location.loc.variant == 2) {
        Task_SpawnFromTable(&D_8016BFE0, 0, 0, 0);
    }
    task->state = task->state + 1;
}

static void func_shelter_b3_garbage_incinerator_8017DC54(Task* task)
{
    char pad[0x10];

    if (D_shelter_b3_garbage_incinerator_801855DC < 0x3D) {
        D_shelter_b3_garbage_incinerator_801855DC++;
    }
}

/// Runs the room controller's current state through its three-entry state
/// table, copied onto the stack before the call.
void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_garbage_incinerator_8017D5C4;
    sp.funcs[task->state](task);
}
