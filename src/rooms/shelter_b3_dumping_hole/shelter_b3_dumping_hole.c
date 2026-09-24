#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b3_dumping_hole.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern u8       D_shelter_b3_dumping_hole_80187574[];
extern TaskDesc D_shelter_b3_dumping_hole_80189ADC;
extern u8       D_shelter_b3_dumping_hole_8018B080[];
extern u8       D_shelter_b3_dumping_hole_8018B428[];
extern u8       D_shelter_b3_dumping_hole_8018F4A4;
extern TaskDesc D_80164B78;

/// Message-table handler that accepts every message without acting on it.
s32 func_shelter_b3_dumping_hole_8017D758(void)
{
    return 0;
}

s32 func_shelter_b3_dumping_hole_8017D760(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x28) {
        if (func_shelter_b3_dumping_hole_8017FB70() != 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x16);
            }
            return 0;
        }
        if (in->field_5 == 0) {
            out->field_3 = (u8)gGameSession->eventRoomIndex + 1;
        }
        return 1;
    }
    return 1;
}

s32 func_shelter_b3_dumping_hole_8017D82C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x12) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x11D) != 0 ? 0x12 : 0x17, 1);
    }
    return 0;
}

/// Message-table handler that accepts every message without acting on it.
s32 func_shelter_b3_dumping_hole_8017D868(void)
{
    return 0;
}

s32 func_shelter_b3_dumping_hole_8017D870(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80189ADC, 0, 0, 0);
    return 0;
}

void func_shelter_b3_dumping_hole_8017D8A0(Task* arg0)
{
    arg0->msgTable = D_shelter_b3_dumping_hole_80187574;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_b3_dumping_hole_80183198(0x180, 0, 0);
    if (GameFlag_GetNibble(0x78) == 0) {
        if (gGameSession->at4.loc.place == 1) {
            if (gGameSession->at4.loc.warp == 3) {
                func_800E8634((s32)D_shelter_b3_dumping_hole_8018B080, 0,
                              (s32)D_shelter_b3_dumping_hole_8018B428);
            }
            func_800E3FAC(0xA2, 0x21);
            GameFlag_SetNibble(0x78, 1);
        }
    }
    if (gGameSession->at4.loc.room >= 2) {
        Task_SpawnFromTable(&D_80164B78, 0, 0, 0);
    }
    arg0->state                       += 1;
    D_shelter_b3_dumping_hole_8018F4A4 = 0;
}

/// Empty function; the unused local reserves the 0x10-byte stack frame the
/// original carries.
void func_shelter_b3_dumping_hole_8017D998(void)
{
    char pad[0x10];
}

/// State handlers of the room's controller task, run by
/// `func_shelter_b3_dumping_hole_8017D9A8`: set-up, an idle state, and the
/// kill.
const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D5C4 = { {
    func_shelter_b3_dumping_hole_8017D8A0,
    (TaskFunc)func_shelter_b3_dumping_hole_8017D998,
    taskKill,
} };

void func_shelter_b3_dumping_hole_8017D9A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D5C4;
    sp.funcs[task->state](task);
}
