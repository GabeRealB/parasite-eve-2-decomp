#include "common.h"
#include "main/task.h"

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", D_mine_secret_passage_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", D_mine_secret_passage_8017D5C4);

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D60C);

s32 func_mine_secret_passage_8017D7C4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D7CC);

s32 func_mine_secret_passage_8017D888(void)
{
    return 0;
}

s32 func_mine_secret_passage_8017D890(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D898);

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D8C8);

void func_mine_secret_passage_8017D914(Task* arg0)
{
    if (GameFlag_GetNibble(0x172) == 0) {
        GameFlag_SetNibble(0x172, 1);
        Gp_SpawnIfCapIdle(3, 1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_mine_secret_passage_8017D968(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D970);

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D9D4);
