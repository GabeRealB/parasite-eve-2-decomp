#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "main/gfx.h"
#include "rooms/acropolis_helicopter_landing_pad.h"

extern s16        D_80071076;
extern s32        D_acropolis_helicopter_landing_pad_80184D9C;
extern TaskDesc   D_acropolis_helicopter_landing_pad_80184E68;
extern GpMsgEntry D_acropolis_helicopter_landing_pad_80182328[];
extern GsF_LIGHT  D_acropolis_helicopter_landing_pad_80182340[3];

void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task);

/// State-0 entry of the room's enemy task: allocates the 0x54-byte work block
/// into `Task::idMap`, marks the model (`field_E = 8`, clears bit 0x80 of
/// `field_C`), runs the placement setup and installs the message table.
void func_acropolis_helicopter_landing_pad_8017D658(Task* task)
{
    TmdObject* obj = task->extra;
    void*      mem;

    mem = Mem_Calloc(0x54, false);
    if (mem == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap   = mem;
    obj->field_E  = 8;
    obj->field_C &= 0xFF7F;
    func_acropolis_helicopter_landing_pad_8017D7B0(task);
    task->field_24      = D_acropolis_helicopter_landing_pad_80182328;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D6E0);

/// Points the model's light / colour matrices at the work block's own copies
/// and loads the room's three flat lights into them.
void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->idMap;
    TmdObject*     obj  = task->extra;
    GsF_LIGHT*     light;
    s32            i;

    obj->field_1C = &work->lightMtx;
    obj->field_20 = &work->colorMtx;
    for (i = 0, light = D_acropolis_helicopter_landing_pad_80182340; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D824);

/// Places the task's model at `placement`: copies the position onto the
/// coordinate's translation, the Euler angles onto its rotation, rebuilds
/// the rotation matrix and marks the coordinate dirty.
s32 func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 arg1, RoomPlacement* placement)
{
    RoomCoord* coord;

    coord             = (RoomCoord*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D964);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D9BC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DA9C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DE78);

void func_acropolis_helicopter_landing_pad_8017DFCC(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_FillPlayerHpMp();
            Gp_ClearCollectedBit(0x101);
            Gp_ClearCollectedBit(0x102);
            Gp_SetItemSeenBit(0x102, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 7);
            Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184E68, 0, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_5C5 = 1;
            Mc_SaveData.field_7   = 1;
            Mc_SaveData.field_6   = 0x12;
            Mc_SaveData.field_8   = 1;
            Mc_SaveData.field_5   = 1;
            D_80071076            = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Display_ReleaseRef();
            Task_Kill(arg0);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E0F8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E270);

/// Message 0x13EE handler: copies the requested `GpSaveLoc` to `dst`. For a
/// warp into stage 0xF it consults the room's phase
/// (`D_acropolis_helicopter_landing_pad_80184D9C`): phase 0 queues sound
/// event 0x1E and refuses the warp (returns 1); phase 2 starts cap slot 9
/// first. `field_5` set skips the side effect either way.
s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 0xF) {
        if (D_acropolis_helicopter_landing_pad_80184D9C == 0) {
            if (src->field_5 == 0) {
                SndEvt_EnqueueType7(-1, 0x1E);
            }
            return 1;
        }
        if (D_acropolis_helicopter_landing_pad_80184D9C == 2) {
            if (src->field_5 == 0) {
                Gp_StartCapSlot(9, 1, 0);
            }
            return 0;
        }
        return 0;
    }
    return 1;
}
