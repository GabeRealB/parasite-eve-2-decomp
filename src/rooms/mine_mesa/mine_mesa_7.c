#include "common.h"

#include "main/task.h"

extern s8       D_8007217B;
extern TaskDesc RoomsShared8017daf0Desc;

void func_mine_mesa_8017E948(void)
{
    Task_SpawnFromTable(&RoomsShared8017daf0Desc, 5, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_7", func_mine_mesa_8017E978);

void func_mine_mesa_8017EA24(void)
{
    if (GameFlag_GetNibble(0x4C) != 0) {
        GameFlag_SetNibble(0x4C, 0);
        D_8007217B = 0;
        Task_CallExit(Game_GetPtrSlot(0xA));
        Game_SetPtrSlot(NULL, 0xA);
    }
}
