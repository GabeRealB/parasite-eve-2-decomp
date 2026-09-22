#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// 0xFF-terminated `GpAreaApplyRec` list the room applies when the scripted end
/// of the visit fires.
extern GpAreaApplyRec D_dryfield_night_water_tank_801808B0;

/// Main-executable halfword the second state waits on before it may advance.
extern u16 D_801153F6;

/// Script blobs handed to `func_800E8634` (which forwards them to `Task_Spawn`)
/// as raw addresses.
extern s32 D_80137C28;
extern s32 D_80138570;

/// Exit task of the night water-tank room, in the shape the other rooms' wait
/// tasks have: three states on `Task::state`. State 0 raises bit 0x80 of
/// `gGameSession::flowFlags` once `Gp_StateF0` has reached 1, then advances;
/// state 1 advances to 2 as soon as the halfword at `D_801153F6` clears; state
/// 2 runs the room's ending -- apply the area records, set flags 0x7B, 0x83,
/// 0x155 and 3, spawn the script `func_800E8634` is handed -- and kills the
/// task, or, while `gGameSession::field_126` is still clear, just ticks
/// `Task::killCountdown` down and waits for another frame.
void func_dryfield_night_water_tank_8017D5D0(Task* task)
{
    SVECTOR3 unused; // never referenced; only reserves the frame slot the ROM has

    switch (task->state) {
        case 0:
            if (Gp_StateF0.field_0 == 1) {
                gGameSession->flowFlags = gGameSession->flowFlags | 0x80;
                task->state             = task->state + 1;
                return;
            }
            return;
        case 1:
            if (D_801153F6 == 0) {
                task->state = 2;
                return;
            }
            break;
        case 2:
            if (gGameSession->field_126 != 0) {
                Gp_ApplyAreaRecs(&D_dryfield_night_water_tank_801808B0);
                GameFlag_SetNibble(0x7B, 2);
                GameFlag_SetNibble(0x83, 1);
                func_800E8634((s32)&D_80137C28, 0, (s32)&D_80138570);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0xE);
                taskKill(task);
                return;
            }
            task->killCountdown = task->killCountdown - 1;
            break;
    }
}
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank", RoomsShared8017d878Table);
