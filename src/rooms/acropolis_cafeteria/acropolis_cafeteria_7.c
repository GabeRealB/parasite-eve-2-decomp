#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

extern s8  D_8007106B;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern GpMsgEntry D_acropolis_cafeteria_80184CEC[];

extern TaskDesc D_acropolis_cafeteria_80184178;
extern s32      D_acropolis_cafeteria_80184CFC;

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_cafeteria_80184178, 2, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

void func_acropolis_cafeteria_8017E708(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state != 0) {
        return;
    }
    task->field_24 = D_acropolis_cafeteria_80184CEC;
    Game_SetPtrSlot(task, 5);
    vec                            = (SVECTOR*)&work->field_10;
    D_acropolis_cafeteria_80184CFC = 0;
    work->field_10                 = 0x220;
    work->field_12                 = -0x12C;
    work->field_14                 = -0x6A0;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->field_10 = 0x400;
    work->field_12 = -0x12C;
    work->field_14 = -0x260;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->field_10 = 0x370;
    work->field_12 = -0x12C;
    work->field_14 = -0x860;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    task->state    = task->state + 1;
    work->field_10 = 0xBB8;
    work->field_12 = -0x834;
    work->field_14 = -0x7D0;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    work->field_10 = 0xB22;
    work->field_12 = -0x834;
    work->field_14 = -0x900;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    D_80115758 = 0x6028D;
    D_8011572C = 0x6028E;
    D_80115750 = 0x6028F;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", func_acropolis_cafeteria_8017E89C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", func_acropolis_cafeteria_8017EA90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", func_acropolis_cafeteria_8017F390);

s32 func_acropolis_cafeteria_8017F908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GsCOORDINATE2* coord;

    coord                          = ((TmdObject*)task->extra)->field_8;
    D_acropolis_cafeteria_80184CFC = arg2;
    if (arg2 != 0) {
        Gp_SpawnEff(0x6009D, coord, 0, NULL);
    }
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", RoomsShared80181e70Table);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", D_acropolis_cafeteria_8017D6AC);
