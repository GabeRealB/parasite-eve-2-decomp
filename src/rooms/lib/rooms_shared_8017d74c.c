#include "common.h"

#include "gameplay/3CD8.h"

#include "main/mc.h"
#include "main/sound.h"

#include "rooms/rooms_shared_8017d638.h"
#include "rooms/rooms_shared_8017d74c.h"

extern s16 D_80071076;
extern u8  D_801153F4;

void RoomsShared8017d74c(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(RoomsShared8017d638Req.field_0);
            if (RoomsShared8017d638Req.field_8 != 0) {
                SndEvt_EnqueueType6(RoomsShared8017d638Req.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(RoomsShared8017d638Req.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (RoomsShared8017d638Req.field_C != 0) {
                SndEvt_EnqueueType6(RoomsShared8017d638Req.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(RoomsShared8017d638Req.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076          = 1;
            Mc_SaveData.field_6 = RoomsShared8017d638Msg.msgId;
            Mc_SaveData.field_8 = RoomsShared8017d638Msg.field_2;
            Mc_SaveData.field_5 = (u8)RoomsShared8017d638Msg.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            Task_Kill(task);
            break;
    }
}
