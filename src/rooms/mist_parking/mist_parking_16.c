#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

extern s16      D_80071076;
extern s8       D_80073BAE;
extern Task*    RoomsShared8018459cTask;
extern TaskDesc RoomsShared8017daf0Desc;

void func_mist_parking_8018451C(void)
{
    func_800BC4BC();
    D_80073BAE          = 1;
    Mc_SaveData.field_6 = 5;
    Mc_SaveData.field_7 = 1;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    D_80071076          = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
}
