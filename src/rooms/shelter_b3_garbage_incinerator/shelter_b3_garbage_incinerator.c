#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b3_garbage_incinerator.h"

extern u16          D_shelter_b3_garbage_incinerator_801855DC;
extern TaskDesc     D_shelter_b3_garbage_incinerator_801855E0;
extern TaskDesc     D_8016BFE0;
extern TaskDesc     D_801449F4;
extern s16          D_shelter_b3_garbage_incinerator_801855DE;
extern GpMsgEntry   D_shelter_b3_garbage_incinerator_80185594[];
extern Task*        D_shelter_b3_garbage_incinerator_801855D8;
extern TaskDesc     D_shelter_b3_garbage_incinerator_80185BA0;
extern TaskDesc     D_shelter_b3_garbage_incinerator_80187150[];
extern TaskDesc     D_shelter_b3_garbage_incinerator_801855CC;
extern RoomEventMsg D_shelter_b3_garbage_incinerator_8018FC2C;
extern s16          D_80071076;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

void func_shelter_b3_garbage_incinerator_8017DB7C(Task* task);
void func_shelter_b3_garbage_incinerator_8017DC54(Task* task);

/// State handlers of the room's controller task, run by
/// `func_shelter_b3_garbage_incinerator_8017DC7C`: set-up, a per-frame tick,
/// and the kill.
const TaskFuncTable3 D_shelter_b3_garbage_incinerator_8017D5C4 = { {
    func_shelter_b3_garbage_incinerator_8017DB7C,
    func_shelter_b3_garbage_incinerator_8017DC54,
    taskKill,
} };

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
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b3_garbage_incinerator_8018FC2C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b3_garbage_incinerator_8018FC2C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b3_garbage_incinerator_8018FC2C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b3_garbage_incinerator_8017D838(void)
{
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017D840(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x29) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_StateF0.field_4 = 1;
        if (gGameSession->at4.loc.room < 4) {
            Gp_SpawnIfCapIdle(3, 1);
            return 0;
        }
        if (GameFlag_GetNibble(0x73) != 0) {
            GameFlag_SetNibble(0x4C, 10);
        } else {
            GameFlag_SetNibble(0x4C, 5);
        }
        out->field_2                              = 4;
        D_shelter_b3_garbage_incinerator_8018FC2C = *out;
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
        Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_801855CC, 0, 0, 0);
        return 2;
    }
    if (in->msgId == 0x27 && in->field_5 == 0) {
        out->field_3 = gGameSession->field_133 + 1;
    }
    return 1;
}

s32 func_shelter_b3_garbage_incinerator_8017D9B4(void)
{
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017D9BC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 2 && gGameSession->field_135 == 0) {
        if (gGameSession->field_132 == 3) {
            Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_801855E0, 0, 0, 0);
            gGameSession->field_135 = 1;
        } else if (D_shelter_b3_garbage_incinerator_801855DC >= 0x3D) {
            SndEvt_EnqueueType6(0x5428000D, 0, 0);
            func_shelter_b3_garbage_incinerator_80180FE4(0x16, 0, 0x3C);
            D_shelter_b3_garbage_incinerator_801855DC = 0;
        }
    }
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017DA74(s32 arg0, s32 arg1, s32 arg2)
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

s32 func_shelter_b3_garbage_incinerator_8017DB2C(s32 arg0, s32 arg1, s32 arg2)
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

void func_shelter_b3_garbage_incinerator_8017DB7C(Task* task)
{
    task->msgTable = D_shelter_b3_garbage_incinerator_80185594;
    Game_SetPtrSlot(task, 7);
    func_shelter_b3_garbage_incinerator_8018108C(0x180, 0, 0);
    D_shelter_b3_garbage_incinerator_801855D8 = Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80185BA0, 0, 0, 0);
    if (gGameSession->at4.loc.room >= 4) {
        Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 0, 0);
    }
    if (gGameSession->at4.loc.place == 2) {
        Task_SpawnFromTable(&D_8016BFE0, 0, 0, 0);
    }
    task->state = task->state + 1;
}

void func_shelter_b3_garbage_incinerator_8017DC54(Task* task)
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
