#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/sound.h"
#include "main/task.h"

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

/// Staging save location this room's warp handler latches: `field_2` /
/// `field_4` / `field_1` take the three bytes the outgoing location carries.
extern GpSaveLoc D_mine_secret_passage_80183448;

extern TaskDesc D_mine_secret_passage_80180EBC;

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", D_mine_secret_passage_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", RoomsShared80181e70Table);

INCLUDE_ASM("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", func_mine_secret_passage_8017D60C);

INCLUDE_RODATA("rooms/nonmatchings/mine_secret_passage/mine_secret_passage", jtbl_mine_secret_passage_8017D5F0);

s32 func_mine_secret_passage_8017D7C4(void)
{
    return 0;
}

/// Handler id 0x13EE of this room's copy of the `GpMsgEntry` table
/// `RoomsShared8017d8c8Msgs` (`D_mine_secret_passage_80180E8C`): copies the
/// requested `GpSaveLoc` to `dst` and forwards both to `func_80179A04`. A
/// stage-9 request latches the outgoing location's three bytes into the room's
/// staging save location and starts the cutscene task; `field_5` set only
/// suppresses that side effect. Returns 2 for a stage-9 request and 1 for
/// every other one.
s32 func_mine_secret_passage_8017D7CC(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 9) {
        if (src->field_5 == 0) {
            D_mine_secret_passage_80183448.field_2 = dst->field_0;
            D_mine_secret_passage_80183448.field_4 = dst->field_2;
            D_mine_secret_passage_80183448.field_1 = dst->field_3;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_mine_secret_passage_80180EBC, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

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
