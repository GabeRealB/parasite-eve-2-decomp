#include "common.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

extern u8       D_80071075;
extern s8       D_8007218A;
extern s8       D_80114C12;
extern TaskDesc D_80164FF8;

void func_shelter_b3_garbage_incinerator_8017DCD4(Task* arg0)
{
    u8   param1[8];
    u8   param2[8];
    s32  msg[5];
    s32  out;
    s32  v;
    s32  w;
    s32* p;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                break;
            }
            SndEvt_EnqueueType6(0x5428000D, 0, 0);
            SndEvt_EnqueueType6(0x54280010, 0, 0);
            param1[2]       = 0x22;
            param1[3]       = 0;
            param1[0]       = 0;
            param2[0]       = 0x14;
            param2[1]       = 0;
            param2[2]       = 0;
            param2[3]       = 0;
            arg0->spawnArg1 = (u16)CdCmd_Enqueue(0x21, param1, param2);
            if ((u8)gGameSession->skipEventIntro == 0) {
                if (Player_Status.weapon == 0x17) {
                    p = msg;
                    w = Player_Status.weapon;
                    if (D_8007218A == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    msg[2] = 0;
                    msg[3] = 0;
                    msg[4] = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
                } else {
                    p = msg;
                    w = Player_Status.weapon;
                    if (D_8007218A == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    p[2]   = 1;
                    p[3]   = 10;
                    msg[4] = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
                }
                arg0->killCountdown = 0;
                arg0->state++;
            } else {
                arg0->state = 2;
            }
            break;
        case 1:
            if (++arg0->killCountdown >= 31) {
                arg0->state++;
            }
            break;
        case 2:
            if (CdCmd_IsSlotEmpty(arg0->spawnArg1)) {
                arg0->spawnArg2 = Task_SpawnFromTable(&D_80164FF8, 0, 0, 0);
                arg0->state++;
            }
            break;
        case 3:
            if (Task_PollKill(arg0->spawnArg2, &out) != 0) {
                taskKill(arg0);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017DF24);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017E158);
