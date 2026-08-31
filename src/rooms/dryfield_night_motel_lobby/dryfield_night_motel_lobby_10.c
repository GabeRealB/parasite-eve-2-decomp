#include "common.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_10", func_dryfield_night_motel_lobby_80181138);

void func_dryfield_night_motel_lobby_8018119C(Task* arg0)
{
    SndEvt_EnqueueType6(0x53110008, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_night_motel_lobby_801811E0(Task* arg0)
{
    Gp_RunCapCmd1(8);
    arg0->state = (s32)(arg0->state + 1);
}
