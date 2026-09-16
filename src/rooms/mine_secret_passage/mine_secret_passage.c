#include "common.h"
#include "main/sound.h"
#include "main/task.h"

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", D_mine_secret_passage_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", RoomsShared80181e70Table);

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D60C);

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", jtbl_mine_secret_passage_8017D5F0);

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

/// Handler id 0x13F2 of the room's `GpMsgEntry` table
/// `D_mine_secret_passage_80180E8C`: cues sound event 0x16 when the message's
/// `arg2` is 3. No `Task` is spawned, so the room owns this cue rather than a
/// child task.
s32 func_mine_secret_passage_8017D898(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x16, 0, 0);
    }
    return 0;
}
