#include "common.h"
#include "main/fs.h"
#include "gameplay/3A34.h"
extern s32 D_mine_cavern_8018EB54;

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_5", func_mine_cavern_8017E330);

void func_mine_cavern_8017E358(void)
{
}

void func_mine_cavern_8017E360(void)
{
    Game_Session->field_9 = 4;
    Gp_StateF0.field_0    = 0;
    Gp_StateF0.field_5    = 0;
    Gp_StateF0.field_6    = 0;
    Gp_StateF0.field_8    = 0;
    Gp_StateF0.field_C    = 0;
    Gp_StateF0.field_10   = 0;
}

void func_mine_cavern_8017E394(void)
{
    D_mine_cavern_8018EB54 = 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_5", func_mine_cavern_8017E3A0);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_5", func_mine_cavern_8017E474);
