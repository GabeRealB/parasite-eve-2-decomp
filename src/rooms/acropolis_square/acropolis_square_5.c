#include "common.h"

#include "decomp/common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern s8       D_8007216C;
extern TaskDesc D_acropolis_square_80183808;
extern Task*    D_acropolis_square_8018889C;
extern s32      D_acropolis_square_80183B34[];

s32 func_acropolis_square_80182110(s32 arg0, s32 arg1, s32 arg2)
{
    SndEvt_EnqueueType6(D_acropolis_square_80183B34[arg2], 0, 0);
    return 0;
}

void func_acropolis_square_80182148(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(5);
            /* Keeps this arm from being cross-jumped into the identical
               `case 3` arm; emits nothing. */
            SOFT_BARRIER();
            goto advance;
        case 1:
            D_8007216C = 7;
            goto advance;
        case 3:
            Gp_RunCapCmd1(5);
            goto advance;
        case 6:
            Gp_RunCapCmd1(5);
            D_8007216C = 8;
            /* fallthrough */
        case 4:
        case 5:
        advance:
            task->state++;
            return;
        case 2:
        case 7:
            GameFlag_SetNibble(0x15, 1);
            Task_Kill(task);
            return;
    }
}

void func_acropolis_square_80182200(s32 arg0)
{
    switch (arg0) { /* irregular */
        case 0:
            D_acropolis_square_8018889C = Task_SpawnFromTable(&D_acropolis_square_80183808, 2, 0, 0);
            return;
        case 1:
            Task_Kill(D_acropolis_square_8018889C);
            return;
    }
}
