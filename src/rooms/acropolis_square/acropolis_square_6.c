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
extern s32      D_acropolis_square_80183B98;
extern s32      D_acropolis_square_80183B58;
extern s32      D_acropolis_square_80183830;
extern s32      D_acropolis_square_8018399C;
extern s32      D_acropolis_square_80183A5C;

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182260);

void func_acropolis_square_801822A4(void)
{
    char pad[0x10];

    if (Mc_SaveData.field_8 == 7 && D_acropolis_square_80183830 == 0) {
        D_acropolis_square_80183830 = 1;
        Mc_SaveData.field_5C5       = 2;
        func_800E8634((s32)&D_acropolis_square_8018399C, 0, (s32)&D_acropolis_square_80183A5C);
    }
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182308);

s32 func_acropolis_square_80182360(void)
{
    GpAreaKey key;

    if (GameFlag_GetNibble(0x1F) == 0) {
        GameFlag_SetNibble(0x1F, 1);
        key.field_3 = 1;
        key.field_2 = 1;
        Gp_SetAreaObjId(&key, 2, 1);
        Game_Session->field_1 = 1;
        Task_SpawnFromTable(&D_acropolis_square_80183808, 0, 0, 0);
        return 0;
    }
    return 1;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_801823DC);
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_801825DC);

s32 func_acropolis_square_8018344C(s32 arg0, s32 arg1, s32 arg2)
{
    D_acropolis_square_80183B98 = arg2;
    return 0;
}

void func_acropolis_square_8018345C(void)
{
}
