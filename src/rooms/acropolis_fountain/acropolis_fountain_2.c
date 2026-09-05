#include "common.h"
#include "main/task.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/stream.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "gameplay/gameplay.h"
#include "rooms/acropolis_fountain.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s16 D_80071076;
extern s32 D_80070F70;

extern u8    D_acropolis_fountain_80183BB0;
extern Task* D_acropolis_fountain_80183BB4;

extern GpMsgEntry D_acropolis_fountain_8017E764[];

extern GpObj4A D_acropolis_fountain_8017E7A4;
extern GpObj4A D_acropolis_fountain_8017FB3C;

extern SVECTOR  D_acropolis_fountain_8017E7F0;
extern s16      D_acropolis_fountain_8017E7F8;
extern TaskDesc D_acropolis_fountain_8017E7FC;

void func_acropolis_fountain_8017DA1C(void);
void func_acropolis_fountain_8017E15C(Task* task, s32 view);

s32 func_acropolis_fountain_8017D77C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 args[2];

    if (arg2 == 3) {
        Gp_RunCapCmd1(((GameFlag_GetNibble(9) & 2) == 0) ? 3 : 6);
    }
    if (arg2 == 4) {
        Gp_StartCapSlot(4, 1, 0);
        func_acropolis_fountain_8017DA1C();
        GameFlag_SetNibble(0x12, 1);
    }
    return 0;
}

s32 func_acropolis_fountain_8017D7F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            SndEvt_EnqueueType6(0x51080003, 0, 0);
            break;
        case 4:
            SndEvt_EnqueueType6(0x51080004, 0, 0);
            break;
        case 9:
            SndEvt_EnqueueType6(0x51080009, 0, 0);
            break;
    }
    return 0;
}

void func_acropolis_fountain_8017D868(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(1);
            task->state = task->state + 1;
            break;

        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            /* fallthrough */

        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_6 = 3;
            Mc_SaveData.field_5 = 3;
            Mc_SaveData.field_8 = D_acropolis_fountain_80183BB0;
            D_80071076          = 1;
            Task_Spawn(0, 0x11, 0, 0);
            GameFlag_SetNibble(0, 5);
            Task_Kill(task);
            break;
    }
}

void func_acropolis_fountain_8017D960(Task* arg0)
{
    arg0->field_24 = D_acropolis_fountain_8017E764;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x12) != 0) {
        func_acropolis_fountain_8017DA1C();
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_fountain_8017D9BC(void)
{
}
