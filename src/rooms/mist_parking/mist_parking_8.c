#include "common.h"

#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/mist_parking.h"

extern s16   D_80071076;
extern Task* D_mist_parking_80195320;

void func_mist_parking_801830F8(void)
{
}

void func_mist_parking_80183100(s32 arg0)
{
    Gp_StartCapSlot(arg0 >> 16, 0, arg0);
}

void func_mist_parking_8018312C(s32 arg0)
{
    Task_SpawnFromTable(&D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_8018316C(s32 arg0)
{
    Mc_SaveData.at4.loc.stage = 1;
    Mc_SaveData.at4.loc.warp  = 1;
    Mc_SaveData.at4.loc.room  = 1;
    Mc_SaveData.at4.loc.area  = arg0;
    D_80071076                = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
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
            obj         = (TmdObject*)task->extra;
            obj->flags &= ~0x80;
        }
    }
}

void func_mist_parking_8018326C(s32 arg0)
{
    if (arg0 == 0) {
        if (D_mist_parking_80195320 != NULL) {
            taskKill(D_mist_parking_80195320);
        }
        D_mist_parking_80195320 = NULL;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_801832AC);
