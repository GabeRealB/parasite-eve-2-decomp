#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern GpMsgEntry     D_shelter_b3_incinerator_control_room_80181838[];
extern GpAreaApplyRec D_shelter_b3_incinerator_control_room_80182A40[];
extern s32            D_801360E4;
extern s32            D_80136804;

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_incinerator_control_room/shelter_b3_incinerator_control_room_2", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_incinerator_control_room/shelter_b3_incinerator_control_room_2", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_incinerator_control_room/shelter_b3_incinerator_control_room_2", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_incinerator_control_room/shelter_b3_incinerator_control_room_2", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_incinerator_control_room/shelter_b3_incinerator_control_room_2", RoomsShared8017ea68Title);

s32 func_shelter_b3_incinerator_control_room_8017FA84(void)
{
    return 0;
}

s32 func_shelter_b3_incinerator_control_room_8017FA8C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0x2A) {
        return 1;
    }
    if (GameFlag_GetNibble(0xA7) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(3);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_incinerator_control_room/shelter_b3_incinerator_control_room_2", func_shelter_b3_incinerator_control_room_8017FB20);

s32 func_shelter_b3_incinerator_control_room_8017FBE0(void)
{
    return 0;
}

s32 func_shelter_b3_incinerator_control_room_8017FBE8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54290009, 0, 0);
    }
    return 0;
}

void func_shelter_b3_incinerator_control_room_8017FC1C(Task* task)
{
    task->field_24 = D_shelter_b3_incinerator_control_room_80181838;
    Game_SetPtrSlot(task, 7);
    task->state++;
    if (Game_Session->field_8 == 4) {
        func_800E3FAC(0xA2, 0x23);
        Gp_ApplyAreaRecs(D_shelter_b3_incinerator_control_room_80182A40);
        Gp_FillAllyHp();
        func_800E8634((s32)&D_801360E4, 0, (s32)&D_80136804);
    }
}
