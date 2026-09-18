#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s8       D_8007217B;
extern TaskDesc RoomsShared8017daf0Desc;

void func_mine_mesa_8017E948(void)
{
    Task_SpawnFromTable(&RoomsShared8017daf0Desc, 5, 0, 0);
}

void func_mine_mesa_8017E978(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    switch ((s16)temp_v0) {
        case 0x2F:
            SndEvt_EnqueueType6(0x10000039, 0, 0);
            break;
        case 0x39:
            SndEvt_EnqueueType6(0x1000003A, 0, 0);
            break;
    }
    if ((gGameSession->field_1 == 0) || ((s16)arg0->killCountdown >= 0x39)) {
        Task_Kill(arg0);
    }
}

void func_mine_mesa_8017EA24(void)
{
    if (GameFlag_GetNibble(0x4C) != 0) {
        GameFlag_SetNibble(0x4C, 0);
        D_8007217B = 0;
        Task_CallExit(Game_GetPtrSlot(0xA));
        Game_SetPtrSlot(NULL, 0xA);
    }
}
