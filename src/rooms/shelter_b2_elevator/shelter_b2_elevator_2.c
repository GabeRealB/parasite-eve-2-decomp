#include "common.h"

#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern s16 D_80071076;

void func_shelter_b2_elevator_8017D888(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (task->killCountdown >= 0x15) {
                task->state++;
            }
            task->killCountdown = task->killCountdown + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData.field_6 = 9;
                    Mc_SaveData.field_8 = 3;
                    break;
                case 0xC:
                    Mc_SaveData.field_6 = 0x1B;
                    Mc_SaveData.field_8 = 2;
                    break;
                case 0xD:
                    Mc_SaveData.field_6 = 0x2A;
                    Mc_SaveData.field_8 = 3;
                    break;
            }
            task->state++;
            break;
        case 3:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.field_5 = 0;
            msg.msgId   = Mc_SaveData.field_6;
            msg.field_2 = Mc_SaveData.field_8;
            msg.field_3 = Mc_SaveData.field_5;
            msg2        = msg;
            func_80179A04(&msg, &msg2);
            D_80071076          = 1;
            Mc_SaveData.field_8 = msg2.field_2;
            Mc_SaveData.field_5 = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            Task_Kill(task);
            break;
    }
}

s32 func_shelter_b2_elevator_8017DA5C(void)
{
    return 0;
}
