#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/libgpu.h>

extern s32 D_dryfield_night_driveway_8017F54C;
extern s32 D_dryfield_night_driveway_8017F6CC;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", D_dryfield_night_driveway_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", func_dryfield_night_driveway_8017D608);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", func_dryfield_night_driveway_8017D7A0);

/// Task callback: on its first tick it hides the display and hands control to
/// the captioned cutscene; on every later tick it kills the task and clears the
/// collected bit. Either way it advances its own state.
void func_dryfield_night_driveway_8017DAF4(Task* arg0)
{
    if (arg0->state == 0) {
        Game_Session->field_68 = 1;
        D_80115768             = 1;
        SetDispMask(0);
        func_800E3FAC(0xA2, 0x10);
        func_800E8634((s32)&D_dryfield_night_driveway_8017F54C, 0, (s32)&D_dryfield_night_driveway_8017F6CC);
    } else {
        Task_Kill(arg0);
        Gp_ClearCollectedBit(0x114);
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", func_dryfield_night_driveway_8017DB8C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", RoomsShared8017d878Table);
