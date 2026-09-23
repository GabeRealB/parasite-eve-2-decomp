#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// Payload of the 0x7DB message this room sends to the slot-4 task when it is
/// entered on any place other than 1.
typedef struct {
    u8  field_0;
    u8  field_1;
    s16 field_2;
} _Msg7DB;

void func_80162B0C(s32);

extern u8         D_80165F48;
extern u8         D_80166848;
extern GpMsgEntry D_shelter_b2_pod_bottom_80181C6C[];

s32 func_shelter_b2_pod_bottom_8017D638(void)
{
    return 0;
}

s32 func_shelter_b2_pod_bottom_8017D640(void)
{
    return 0;
}

void func_shelter_b2_pod_bottom_8017D648(Task* arg0)
{
    _Msg7DB msg;

    arg0->msgTable = D_shelter_b2_pod_bottom_80181C6C;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_80162B0C(0);
        func_800E8634((s32)&D_80165F48, 0, (s32)&D_80166848);
    } else {
        msg.field_0 = 0;
        msg.field_1 = 0;
        msg.field_2 = 7;
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&msg, 0);
    }
    arg0->state++;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom", RoomsShared8017d878Table);
