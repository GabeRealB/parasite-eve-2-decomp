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

/// Coordinate node of a room prop's model (`TmdObject::field_8`). Same 0x50
/// layout as `GsCOORDINATE2`, except +0x44 (libgs `param`) holds an inline
/// `SVECTOR` of Euler angles that `RotMatrixZYX` composes into `mtx`.
/// `GpDisp2dCoord` (gameplay) is the same shape for a different object.
typedef struct {
    /* 0x00 */ u32            flg;
    /* 0x04 */ MATRIX         mtx;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ SVECTOR        rot;
    /* 0x4C */ GsCOORDINATE2* sub;
} MistR18Coord;
extern Task*    D_mist_r18_80186E98;
extern s16      D_80071076;
extern s8       D_801156F9;
extern TaskDesc RoomsShared8017e5b8Desc;

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_5", func_mist_r18_8017EA2C);

void func_mist_r18_8017EA60(void)
{
    if (D_mist_r18_80186E98 != NULL) {
        Task_Kill(D_mist_r18_80186E98);
    }
    D_mist_r18_80186E98 = NULL;
}

void func_mist_r18_8017EA98(Task* task)
{
    MistR18Coord* coord;
    TmdObject*    obj;

    if (task->state == 0) {
        coord           = (MistR18Coord*)((TmdObject*)task->extra)->field_8;
        coord->mtx.t[0] = -0x1496;
        coord->mtx.t[1] = -0x2DA;
        coord->mtx.t[2] = 0xB90;
        coord->rot.vx   = 0x6AA;
        coord->rot.vy   = -0xF8E;
        coord->rot.vz   = -0x333;
        RotMatrixZYX(&coord->rot, &coord->mtx);
        coord->flg    = 0;
        obj           = (TmdObject*)task->extra;
        obj->field_E  = -8;
        obj->field_C &= 0xFF7F;
        task->state++;
    }
}

void func_mist_r18_8017EB48(void)
{
    Gp_InitStarterInv();
    Mc_SaveData.field_7 = 1;
    Mc_SaveData.field_6 = 0x13;
    Mc_SaveData.field_8 = 3;
    Mc_SaveData.field_5 = 3;
    D_80071076          = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
}

void func_mist_r18_8017EBB8(void)
{
    Game_Session->field_52 = 1;
    CdCmd_StartOverlay(1U, 0x1EU, 0xBU);
    CdCmd_EnqueueReplaceOverlay82();
}

void func_mist_r18_8017EBF8(void)
{
    if (Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 7, 0, 0) != NULL) {
        D_801156F9 = 1;
    }
}
