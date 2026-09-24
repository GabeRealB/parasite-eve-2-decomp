#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Task table spawned by `func_shelter_b2_laboratory_801801D0` the first time
/// the laboratory console is used.
extern TaskDesc D_shelter_b2_laboratory_80182A6C[];

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_3", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_3", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_3", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_3", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_3", RoomsShared8017ea68Title);

s32 func_shelter_b2_laboratory_801800FC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_80179A04(in, out);
    if (GameFlag_GetNibble(0xD0) == 2) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(5);
        }
        return 2;
    }
    if (in->msgId != 0x21) {
        return 1;
    }
    req.field_0 = 1;
    req.field_4 = 1;
    req.field_8 = 0x541F0014;
    req.field_C = 0x541F0003;
    req.flagId  = 0xB1;
    req.itemId  = 0;
    return RoomsShared8017d638(&req, out);
}

/// Handler for slot-7 msg `0x13EF` in `RoomsShared8017db84Msgs`: the
/// directed action on the laboratory console (`field_2` 1). Runs the scripted
/// scene once, then replays cap script `6` on later visits.
s32 func_shelter_b2_laboratory_801801D0(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x13D) != 0) {
            if (GameFlag_GetNibble(0xD0) < 2) {
                Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(6);
            }
        } else {
            Gp_RunCapCmd1(0x1E);
            GameFlag_SetNibble(0x13D, 1);
        }
    }
    return 0;
}

s32 func_shelter_b2_laboratory_8018025C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x541F0017, 0, 0);
    }
    return 0;
}

extern TaskDesc D_80134564;
extern Task*    D_shelter_b2_laboratory_80182A68;

void func_shelter_b2_laboratory_80180290(Task* task)
{
    s32 result;

    switch (task->state) {
        case 0:
            D_shelter_b2_laboratory_80182A68 = Task_SpawnFromTable(&D_80134564, 0, 0, 0);
            task->state                     += 1;
            return;
        case 1:
            if (Task_PollKill(D_shelter_b2_laboratory_80182A68, &result) != 0) {
                D_shelter_b2_laboratory_80182A68 = NULL;
                if (result != 0) {
                    GameFlag_SetNibble(0xD0, 1);
                } else {
                    Gp_MsgPlayerWeapon(1);
                }
                taskKill(task);
            }
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_3", func_shelter_b2_laboratory_80180350);
