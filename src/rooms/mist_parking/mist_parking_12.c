#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"

extern s16      D_80071076;
extern s32      D_mist_parking_80195320;
extern TaskDesc RoomsShared8017e5b8Desc;
extern Task*    RoomsShared8017ea2cTask;
extern void     Stage_RequestMidiFromMap(s32 arg0);

void func_mist_parking_8018357C(Task* arg0)
{
    func_800BC4E4();
    Mc_SaveData.field_7 = 2;
    Mc_SaveData.field_6 = 1;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    D_80071076          = 1;
    Fs_BeginBootLoad(&Mc_SaveData.field_4, 1);
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    Task_Kill(arg0);
}
