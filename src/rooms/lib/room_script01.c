#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

void Room_Script01(Task* task)
{
    GpSaveLoc    src;
    GpSaveLoc    dst;
    GpSaveLoc*   s;
    GpSaveLoc*   d;
    GameSession* session;
    s32          flag;
    s32          state;
    s32          arg;
    u8           room;

    flag  = task->spawnArg1;
    state = task->state;
    arg   = (s32)task->spawnArg2;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
                if (flag == 0x51) {
                    d           = &dst;
                    s           = &src;
                    *(u16*)&src = 0x26;
                    src.field_5 = 0;
                    if (s->field_5 == 0) {
                        if (GameFlag_GetNibble(0xC9) != 0) {
                            if (GameFlag_GetNibble(0x53) != 0) {
                                d->field_3 = 2;
                            } else {
                                d->field_3 = 1;
                            }
                            if (GameFlag_GetNibble(0x51) == 0) {
                                dst.field_3 = dst.field_3 + 2;
                            }
                        } else {
                            if (GameFlag_GetNibble(0x51) != 0) {
                                d->field_3 = 5;
                            } else {
                                d->field_3 = 6;
                            }
                        }
                    }
                    session             = Game_Session;
                    room                = dst.field_3;
                    session->field_5    = room;
                    Mc_SaveData.field_5 = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (Game_Session->field_5 >= 5) {
                Game_Session->field_52 = 1;
            }
            Task_Kill(task);
            return;
    }
}
