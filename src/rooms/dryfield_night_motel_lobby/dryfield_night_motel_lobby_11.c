#include "common.h"
#include "main/fs.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
extern GpAreaApplyRec D_dryfield_night_motel_lobby_801844AC;

void func_dryfield_night_motel_lobby_80181138(Task* arg0)
{
    Gp_ApplyAreaRecs(&D_dryfield_night_motel_lobby_801844AC);
    Game_Session->field_1 = 1;
    Task_Kill(arg0->spawnArg2);
    GameFlag_SetNibble(0x74, 1);
    arg0->state = (s32)(arg0->state + 1);
}

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
