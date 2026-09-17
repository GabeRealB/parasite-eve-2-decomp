#include "common.h"

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc* D_dryfield_night_factory_8018A7E0;
extern TaskDesc* D_dryfield_night_factory_8018A7E4;
extern Task**    D_dryfield_night_factory_8018A7E8;

extern TaskDesc   D_dryfield_night_factory_80186DE0[];
extern TaskDesc   D_dryfield_night_factory_80186E28[];
extern GpMsgEntry D_dryfield_night_factory_80186E64[];
extern TaskDesc   D_dryfield_night_factory_80186E94[];
extern TaskDesc   D_dryfield_night_factory_80186EA0[];

extern void Room_Util16(s32);

/// `D_..._A7E0` / `D_..._A7E4` hold the address of whichever spawn table
/// `func_dryfield_night_factory_80180438` selected for the current session
/// (it stores `&D_..._80186E94` / `&D_..._80186E28` / ... into them), and
/// `D_..._A7E8` points at the `Mem_Calloc(4, 0)` slot the spawned task is
/// parked in. Both are read as values here, which is why the target loads
/// them (`lw $a0, %lo(...)`) rather than forming `&symbol`.
///
/// Room entry task tick: publish the room's message table in `Task::field_24`
/// and claim game pointer slot 7, then park the `Mem_Calloc` slot the poller
/// `func_dryfield_night_factory_8018076C` watches in it. Session variant
/// `Game_Session::field_7 == 2` (the night factory) picks the larger spawn
/// tables and the second progress-nibble interpretation; every other variant
/// picks the day set. The entry's own task and callback are spawned from the
/// selected table at index 4 and 5, then nibble 0x48 is read -- under variant
/// 2 through a branch whose two arms are the same call, which is why the
/// target keeps both copies of it.
///
/// `slot` and the store to `D_..._A7E8` are one chained assignment on purpose:
/// that makes GCC materialise the global's address ahead of `Mem_Calloc`, so
/// the address quantity's live range spans the call. `local-alloc.c`'s
/// `QTY_CMP_PRI` divides by the range length, which drops it below the
/// `Game_Session` load quantity, and that load then wins `$v1` -- the target's
/// allocation. Split into two statements it takes `$v1` itself and the load
/// falls to `$a0`.
void func_dryfield_night_factory_80180438(Task* arg0)
{
    Task** slot;

    arg0->field_24 = D_dryfield_night_factory_80186E64;
    Game_SetPtrSlot(arg0, 7);
    slot        = (D_dryfield_night_factory_8018A7E8 = Mem_Calloc(4, 0));
    arg0->idMap = (TaskIdMap*)slot;
    if (Game_Session->field_7 == 2) {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186E28;
    } else {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186DE0;
    }
    if (Game_Session->field_7 == 2) {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186E94;
    } else {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186EA0;
    }
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 4, 0, (s32)D_dryfield_night_factory_8018A7E8);
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 5, 0, 0);
    if (Game_Session->field_7 == 2) {
        Room_Util16(GameFlag_GetNibble(0x48) & 0xFF);
        SOFT_BARRIER();
    } else {
        Room_Util16(GameFlag_GetNibble(0x48) & 0xFF);
    }
    arg0->state++;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_5", func_dryfield_night_factory_80180574);

void func_dryfield_night_factory_8018076C(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_night_factory_8018A7E8 = Task_SpawnFromTable(D_dryfield_night_factory_8018A7E0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_night_factory_8018A7E8, &poll) != 0) {
                Task_Kill(task);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_5", RoomsShared8017fc38Table);
