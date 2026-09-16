#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern u8         D_80115598;
extern GpMsgEntry D_neo_ark_island_80181B48[];

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_island/neo_ark_island", D_neo_ark_island_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017D650);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017E2A4);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_island/neo_ark_island", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017E844);

s32 func_neo_ark_island_8017E960(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017E968);

s32 func_neo_ark_island_8017EA24(void)
{
    return 0;
}

s32 func_neo_ark_island_8017EA2C(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017EA34);

/// Room entry task tick in the family that announces the island's arrival:
/// installs the room's message table, hands the task to pointer slot 7, plays
/// the two island cues, then advances state and raises the `D_80115598` flag.
void func_neo_ark_island_8017EA94(Task* arg0)
{
    arg0->field_24 = D_neo_ark_island_80181B48;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550E0005, 0, 0);
    SndEvt_EnqueueType6(0x550E0006, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_neo_ark_island_8017EB08(void)
{
}
