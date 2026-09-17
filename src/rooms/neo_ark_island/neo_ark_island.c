#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern u8         D_80115598;
extern GpMsgEntry D_neo_ark_island_80181B48[];
extern s16        D_80071076;
extern s8         D_801153F4;

/// Staging save location the island commits: `field_2` / `field_4` / `field_1`
/// hold what `func_neo_ark_island_8017E968` copies out of the incoming
/// location, and `func_neo_ark_island_8017E844` moves those same three bytes
/// into `Mc_SaveData.field_6` / `field_8` / `field_5`.
extern GpSaveLoc D_neo_ark_island_80184008;

extern TaskDesc D_neo_ark_island_80181B78;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017D650);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_island/neo_ark_island", func_neo_ark_island_8017E2A4);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_island/neo_ark_island", RoomsShared8017d878Table);

/// Island arrival sequence, advanced one step per call: step 0 asks for the
/// caption, step 1 waits for the CAP system to go idle, step 2 clears the mode
/// flag and waits for the event key it answers with - anything but 0xA kills
/// the task and messages the player weapon - step 3 is the shared advance, and
/// step 4 raises the outgoing sound, commits the staged save location to
/// `Mc_SaveData` and spawns the task's successor.
void func_neo_ark_island_8017E844(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_SpawnIfCapIdle(1, 0);
            goto L_advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            goto L_advance;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                Task_Kill(arg0);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            D_801153F4 = 0;
            goto L_advance;
        case 3:
        L_advance:
            arg0->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076          = 1;
            Mc_SaveData.field_6 = D_neo_ark_island_80184008.field_2;
            Mc_SaveData.field_8 = D_neo_ark_island_80184008.field_4;
            Mc_SaveData.field_5 = D_neo_ark_island_80184008.field_1;
            Task_Spawn(0, 0x11, 0, 0);
            Task_Kill(arg0);
            break;
    }
}

s32 func_neo_ark_island_8017E960(void)
{
    return 0;
}

/// Island message handler. Message 0x1E, while the incoming location still
/// reports no pending flag, latches the save location the outgoing message
/// carries and starts the cutscene that leads to the island's arrival. Returns
/// 1 for every other message and for a location that is already latched.
s32 func_neo_ark_island_8017E968(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179B14(src, dst);
    if (*(u16*)src == 0x1E) {
        if (src->field_5 == 0) {
            D_neo_ark_island_80184008.field_2 = dst->field_0;
            D_neo_ark_island_80184008.field_4 = dst->field_2;
            D_neo_ark_island_80184008.field_1 = dst->field_3;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_neo_ark_island_80181B78, 0, 0, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_neo_ark_island_8017EA24(void)
{
    return 0;
}

s32 func_neo_ark_island_8017EA2C(void)
{
    return 0;
}

/// Maps a cap (cutscene) script event key to the island cue it should play:
/// key 3 plays `0x550E0003` outright, key 0x65 plays `0x550E0004` only while
/// the running cap script reports no event key. Every other key, and key 0x65
/// with a script still parked on one, is ignored. Always returns 0.
s32 func_neo_ark_island_8017EA34(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 3:
            id = 0x550E0003;
            goto play;
        case 0x65:
            if (Gp_GetCapEventKey() != 0) {
                break;
            }
            id = 0x550E0004;
        play:
            SndEvt_EnqueueType6(id, 0, 0);
            break;
    }
    return 0;
}

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
