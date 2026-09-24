#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8       D_80115598;
extern s32      D_acropolis_promenade_80180E74;
extern TaskDesc D_acropolis_promenade_80180EA4;
extern Task*    D_acropolis_promenade_801862D8;

extern const TaskFuncTable3 D_acropolis_promenade_8017D5C4;
extern const TaskFuncTable3 D_acropolis_promenade_8017D5D0;

s32 func_acropolis_promenade_8017D8E0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 5) {
        if (Gp_GetCurBit2Flag(0x15) != 2) {
            Gp_StartCapSlot(5, 1, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_acropolis_promenade_8017D930(void)
{
}

s32 func_acropolis_promenade_8017D938(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 0xA:
            SndEvt_EnqueueType6(0x510B0009, 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(0x510B000A, 0, 0);
            break;
    }
    return 0;
}

/// Runs the prop task's current state (`func_acropolis_promenade_8017DAA4`,
/// `func_acropolis_promenade_8017DB48`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017D988(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5D0;
    sp.funcs[task->state](task);
}

void func_acropolis_promenade_8017D9E0(Task* arg0)
{
    arg0->msgTable = &D_acropolis_promenade_80180E74;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_promenade_801862D8 = Task_SpawnFromTable(&D_acropolis_promenade_80180EA4, 0, 0, 0);
    arg0->state                    = (s32)(arg0->state + 1);
    D_80115598                     = 1;
}

/// Runs the room task's current state (`func_acropolis_promenade_8017D9E0`,
/// `func_acropolis_promenade_8017D5E4`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017DA4C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5C4;
    sp.funcs[task->state](task);
}
