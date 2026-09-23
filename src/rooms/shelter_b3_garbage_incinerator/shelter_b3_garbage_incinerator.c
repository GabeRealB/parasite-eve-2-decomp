#include "common.h"
#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

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
extern u8           D_801153F4;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

void RoomsShared801830f0(s16 arg0, s16 arg1, s16 arg2);
void func_shelter_b3_garbage_incinerator_8018108C(s32 arg0, s32 arg1, s32 arg2);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", func_shelter_b3_garbage_incinerator_8017D6EC);

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
        D_801153F4 = 1;
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
            RoomsShared801830f0(0x16, 0, 0x3C);
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

void func_shelter_b3_garbage_incinerator_8017DC54(void)
{
    char pad[0x10];

    if (D_shelter_b3_garbage_incinerator_801855DC < 0x3D) {
        D_shelter_b3_garbage_incinerator_801855DC++;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", jtbl_shelter_b3_garbage_incinerator_8017D5E8);
