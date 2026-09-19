#include "common.h"

#include "gameplay/268.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#include "rooms/room_common.h"

extern Task*    RoomsShared8017ea2cTask;
extern s16      D_80071076;
extern s8       D_801156F9;
extern TaskDesc RoomsShared8017e5b8Desc;

void func_mist_r18_8017EA60(void)
{
    if (RoomsShared8017ea2cTask != NULL) {
        taskKill(RoomsShared8017ea2cTask);
    }
    RoomsShared8017ea2cTask = NULL;
}

void func_mist_r18_8017EA98(Task* task)
{
    RoomCoord* coord;
    TmdObject* obj;

    if (task->state == 0) {
        coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
        coord->coord.t[0] = -0x1496;
        coord->coord.t[1] = -0x2DA;
        coord->coord.t[2] = 0xB90;
        coord->rot.vx     = 0x6AA;
        coord->rot.vy     = -0xF8E;
        coord->rot.vz     = -0x333;
        RotMatrixZYX(&coord->rot, &coord->coord);
        coord->flg    = 0;
        obj           = (TmdObject*)task->extra;
        obj->otOffset = -8;
        obj->flags   &= 0xFF7F;
        task->state++;
    }
}

void func_mist_r18_8017EB48(void)
{
    Gp_InitStarterInv();
    Mc_SaveData.at4.loc.stage = 1;
    Mc_SaveData.at4.loc.area  = 0x13;
    Mc_SaveData.at4.loc.warp  = 3;
    Mc_SaveData.at4.loc.room  = 3;
    D_80071076                = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
}

void func_mist_r18_8017EBB8(void)
{
    gGameSession->viewDirty = 1;
    CdCmd_StartOverlay(1U, 0x1EU, 0xBU);
    CdCmd_EnqueueReplaceOverlay82();
}

void func_mist_r18_8017EBF8(void)
{
    if (Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 7, 0, 0) != NULL) {
        D_801156F9 = 1;
    }
}
