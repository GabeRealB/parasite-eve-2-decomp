#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

/// Staging save location this room's warp handler latches: `field_2` /
/// `field_4` / `field_1` take the three bytes the outgoing location carries.
extern GpSaveLoc D_mine_secret_passage_80183448;

extern TaskDesc D_mine_secret_passage_80180EBC;

extern s16 D_80071076;
extern s8  D_801153F4;

/// 0x1E pair this room hands `Task_Spawn` for the helper it raises in state 4,
/// the same shape `D_mine_mesa_80189B38` has.
extern GpStateBD8 D_mine_secret_passage_80183440;

/// Runs the room's save sequence. State 0 asks for the caption, state 1 waits
/// for it and drops the periscope overlay, state 2 takes the confirm key or
/// backs out, state 3 counts the armed-shot window down before raising the PE
/// prompt, state 4 raises the helper task 0x31 and queues the sound event,
/// state 5 waits for that voice, and state 6 - the commit - copies the staged
/// location into `Mc_SaveData` and reloads. Every state but the commit advances
/// through the shared `advance` tail; a confirmed cancel stops without it.
void func_mine_secret_passage_8017D60C(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_RunCapCmd(2, 0);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            D_801153F4 = 0;
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                Task_Kill(arg0);
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                break;
            }
            D_801153F4          = 1;
            arg0->killCountdown = 3;
            arg0->state++;
            break;
        case 3:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            goto advance;
        case 4:
            D_mine_secret_passage_80183440.field_0 = 0;
            D_mine_secret_passage_80183440.field_1 = 0;
            D_mine_secret_passage_80183440.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_mine_secret_passage_80183440);
            SndEvt_EnqueueType6(0x54080003, 0, 0);
            goto advance;
        case 5:
            if (SndVoice_HasActiveId(0x54080003) != 0) {
                break;
            }
        advance:
            arg0->state++;
            break;
        case 6:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = (u8)D_mine_secret_passage_80183448.field_2;
            Mc_SaveData.at4.loc.warp = (u8)D_mine_secret_passage_80183448.field_4;
            Mc_SaveData.at4.loc.room = (u8)D_mine_secret_passage_80183448.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            Task_Kill(arg0);
            break;
    }
}

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
