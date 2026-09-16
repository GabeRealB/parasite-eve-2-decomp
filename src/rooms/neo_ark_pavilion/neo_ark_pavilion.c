#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern GpMsgEntry D_neo_ark_pavilion_80183870[];

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion", D_neo_ark_pavilion_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion", func_neo_ark_pavilion_8017D660);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion", func_neo_ark_pavilion_8017E2B4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion", func_neo_ark_pavilion_8017E854);

s32 func_neo_ark_pavilion_8017E9EC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion", func_neo_ark_pavilion_8017E9F4);

/// Room message handler: on message `1`, spawns the pavilion's cap entity —
/// id `5` once flag `0x141` is set, `1` while it is clear.
s32 func_neo_ark_pavilion_8017EB3C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x141) != 0 ? 5 : 1, 1);
    }
    return 0;
}

s32 func_neo_ark_pavilion_8017EB78(void)
{
    return 0;
}

/// Room entry task tick: installs the room's message table (ids `0x13EE`-`0x13F1`),
/// hands the task to pointer slot 7, queues sound events `0x550D0005` and
/// `0x550D0006`, then advances state.
void func_neo_ark_pavilion_8017EB80(Task* arg0)
{
    arg0->field_24 = D_neo_ark_pavilion_80183870;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550D0005, 0, 0);
    SndEvt_EnqueueType6(0x550D0006, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_pavilion_8017EBEC(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion", RoomsShared8017d878Table);
