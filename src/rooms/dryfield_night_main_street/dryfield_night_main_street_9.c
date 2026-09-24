#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

/// Descriptor of the task `func_dryfield_night_main_street_8017DE78` runs as.
extern TaskDesc D_dryfield_night_main_street_801820A4;

/// Waits for the CAP script to go idle, then records the play time when the
/// script's event key is 1, drops collected bit 0x11A when 0x119 is also held,
/// and ends.
void func_dryfield_night_main_street_8017DE78(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 1) {
            Gp_MarkPlayTime();
        }
        if (Gp_HasCollectedBit(0x119) != 0 && Gp_HasCollectedBit(0x11A) != 0) {
            Gp_ClearCollectedBit(0x11A);
        }
        taskKill(task);
    }
}

/// Plays the stage sound a CAP script cue asks for: cues 8, 9 and 0xC play
/// their own sound (9 also plays 0xC's), and cues 0x65 and 0x78 play sound
/// 0xD when the event key is 1 and 0 respectively.
s32 func_dryfield_night_main_street_8017DEF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x52020008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x52020009, 0, 0);
            /* fallthrough */
        case 0xC:
            Gp_EnqueueStageSnd6(0x5202000C, 0, 0);
            break;
        case 0x65:
            if (Gp_GetCapEventKey() == 1) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
        case 0x78:
            if (Gp_GetCapEventKey() == 0) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
    }
    return 0;
}

/// Acts only on `arg2 == 1`. Once nibble 0x7B has reached 2 it ages flag
/// 0x119, sets current-bit flag 0x1B unless collected bit 0x119 is held,
/// spawns CAP entry 1 and the task above; before that it spawns CAP entry
/// 0x14 instead.
s32 func_dryfield_night_main_street_8017DFC8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x7B) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            Task_SpawnFromTable(&D_dryfield_night_main_street_801820A4, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}
