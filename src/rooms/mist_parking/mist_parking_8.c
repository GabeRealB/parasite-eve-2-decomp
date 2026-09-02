#include "common.h"

#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16      D_80071076;
extern Task*    D_mist_parking_80195320;
extern TaskDesc RoomsShared8018397cDesc;
extern TaskDesc D_mist_parking_8018D75C;

void func_mist_parking_801830F8(void)
{
}

void func_mist_parking_80183100(s32 arg0)
{
    Gp_StartCapSlot(arg0 >> 16, 0, arg0);
}

void func_mist_parking_8018312C(s32 arg0)
{
    Task_SpawnFromTable(&RoomsShared8018397cDesc, 0, arg0, 0);
    Game_Session->field_64 = 1;
}

void func_mist_parking_8018316C(s32 arg0)
{
    Mc_SaveData.field_7 = 1;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    Mc_SaveData.field_6 = arg0;
    D_80071076          = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
    }
}

void func_mist_parking_801831F0(s32 arg0)
{
    Task**     slot;
    Task*      task;
    TmdObject* obj;

    if (arg0 == 0) {
        slot = &D_mist_parking_80195320;
    } else {
        slot = NULL;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = Task_SpawnFromTable(&D_mist_parking_8018D75C, arg0, 0, 0);
        *slot = task;
        if (task != NULL) {
            obj           = (TmdObject*)task->extra;
            obj->field_C &= ~0x80;
        }
    }
}

void func_mist_parking_8018326C(s32 arg0)
{
    if (arg0 == 0) {
        if (D_mist_parking_80195320 != NULL) {
            Task_Kill(D_mist_parking_80195320);
        }
        D_mist_parking_80195320 = NULL;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_801832AC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_80183304);
