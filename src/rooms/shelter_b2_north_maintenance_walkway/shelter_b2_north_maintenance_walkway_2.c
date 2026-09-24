#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/room_common.h"

/// The pair of cutscene blocks the walkway's scene hands to `func_800E8634`.
extern s32 D_80165354;
extern s32 D_80165834;

/// Area records applied once the walkway's scene has started.
extern GpAreaApplyRec D_shelter_b2_north_maintenance_walkway_80186380[];

/// The walkway's message table, installed as the room task's `msgTable`.
extern GpMsgEntry D_shelter_b2_north_maintenance_walkway_80183B60[];

extern void func_8016268C(void);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", func_shelter_b2_north_maintenance_walkway_8017DA88);

s32 func_shelter_b2_north_maintenance_walkway_8017DC44(void)
{
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DC4C(void)
{
    return 0;
}

/// Room message handler. On the visit whose sub-id (`field_2`) is 1, agrees with
/// the session's own sub-id and has not yet latched nibble 0x84, it starts the
/// cutscene pair, runs `func_800E3FAC(0xA2, 0x20)`, latches the nibble and
/// applies the room's area records. The outgoing record is never written.
s32 func_shelter_b2_north_maintenance_walkway_8017DC54(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->field_2;

    if (subId == 1 && GameFlag_GetNibble(0x84) == 0 && gGameSession->at4.loc.place == subId) {
        func_800E8634((s32)&D_80165354, 0, (s32)&D_80165834);
        func_800E3FAC(0xA2, 0x20);
        GameFlag_SetNibble(0x84, 1);
        Gp_ApplyAreaRecs(D_shelter_b2_north_maintenance_walkway_80186380);
    }
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DCE4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 7) {
        SndEvt_EnqueueType6(0x541E0000 | 7, 0, 0);
    }
    return 0;
}

void func_shelter_b2_north_maintenance_walkway_8017DD18(Task* task)
{
    task->msgTable = D_shelter_b2_north_maintenance_walkway_80183B60;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_8016268C();
    }
    task->state = task->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", RoomsShared8017d878Table);
