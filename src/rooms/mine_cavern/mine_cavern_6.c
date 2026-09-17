#include "common.h"
#include "main/fs.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "rooms/mine_cavern.h"
extern s32 D_mine_cavern_8018EB54;
extern u8  D_8007216D;

void func_mine_cavern_8017E330(void)
{
    D_8007216D             = 2;
    Game_Session->field_5  = 2;
    Game_Session->field_76 = 1;
}

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

void func_mine_cavern_8017E3A0(s32 arg0)
{
    GameSessionFrom4*  sess;
    MineCavernSprtRec* rec;
    s32                v;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    rec  = (MineCavernSprtRec*)Gp_SprtTables[sess->field_3 - 1]->field_0[sess->field_2 - 1];
    v    = arg0 & 0xFF;

    if (v == 1) {
        rec->field_28->field_2C  = v;
        rec->field_34->field_34  = v;
        rec->field_100->field_2C = v;
        rec->field_10C->field_1C = v;
        rec->field_118->field_24 = v;
        return;
    }
    if (v == 0) {
        rec->field_28->field_2C  = 0;
        rec->field_34->field_34  = 0;
        rec->field_100->field_2C = 0;
        rec->field_10C->field_1C = 0;
        rec->field_118->field_24 = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_6", func_mine_cavern_8017E474);
