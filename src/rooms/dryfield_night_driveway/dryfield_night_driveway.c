#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/libgpu.h>

extern s32 D_dryfield_night_driveway_8017F3D4;
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

/// Task callback: a four-step script. State 0 queues the weapon message and the
/// captioned command, state 1 waits one tick, state 2 starts the cutscene at
/// `D_dryfield_night_driveway_8017F3D4`, and state 3 - reached by falling out of
/// state 2 - clears the session's `field_4` area flag and kills the task once the
/// session reports `field_1` is zero.
void func_dryfield_night_driveway_8017DB8C(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            arg0->state += 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            func_800E8614((s32)&D_dryfield_night_driveway_8017F3D4, 0);
            arg0->state += 1;
            /* fallthrough */
        case 3:
            if (Game_Session->field_1 == 0) {
                Gp_ClearAreaFlag4((GpAreaKey*)&Game_Session->field_4);
                Task_Kill(arg0);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", RoomsShared8017d878Table);
