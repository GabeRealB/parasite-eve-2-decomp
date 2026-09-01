#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern Task* D_actor_461800_80133EB8;

extern Task*    D_actor_461800_80133EB4;
extern TaskDesc D_actor_461800_80133EBC;

INCLUDE_RODATA("actors/nonmatchings/actor_461800/actor_461800", D_actor_461800_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80131E38);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132048);

void func_actor_461800_801321DC(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = Task_SpawnFromTable(&D_actor_461800_80133EBC, 0, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = arg0;
    }
}

void func_actor_461800_8013223C(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = Task_SpawnFromTable(&D_actor_461800_80133EBC, 1, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = arg0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_8013229C);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132390);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132660);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_801329B0);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132A0C);
