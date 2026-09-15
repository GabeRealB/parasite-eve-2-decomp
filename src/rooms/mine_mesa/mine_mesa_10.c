#include "common.h"

#include "main/task.h"

extern s16 D_mine_mesa_80189B6C;
extern s32 D_mine_mesa_80189B74[2];
extern s32 D_mine_mesa_80189B1C;

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_10", func_mine_mesa_801811C4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_10", func_mine_mesa_80181358);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_10", func_mine_mesa_801817BC);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_10", func_mine_mesa_80181800);

/// Starts the room's slot countdown: seeds `D_mine_mesa_80189B6C` to 10, clears
/// the two slots at `D_mine_mesa_80189B74`, points the task at the room's state
/// descriptor and advances a state. `func_mine_mesa_80181800` later empties a
/// slot and decrements the counter once the thing in it is gone.
void func_mine_mesa_80181848(Task* arg0)
{
    D_mine_mesa_80189B6C    = 10;
    D_mine_mesa_80189B74[1] = 0;
    D_mine_mesa_80189B74[0] = 0;
    arg0->field_24          = &D_mine_mesa_80189B1C;
    arg0->state++;
}

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa_10", RoomsShared80181e70Table);
