#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_factory.h"
#include "rooms/room_common.h"

extern TaskDesc* D_dryfield_factory_8018A3C0;
extern TaskDesc* D_dryfield_factory_8018A3C4;
extern Task**    D_dryfield_factory_8018A3C8;

extern GpMsgEntry D_dryfield_factory_801826D4[];
extern TaskDesc   D_dryfield_factory_80186DE0[];
extern TaskDesc   D_dryfield_factory_80186E28[];
extern TaskDesc   D_dryfield_factory_80186E94[];
extern GpMsgEntry D_dryfield_factory_80186EA0[];

/// Room entry task: publishes the room's message table, claims game pointer
/// slot 7 and parks a fresh one-word slot at `Task::work` (also kept in
/// `D_..._8018A3C8`) for the poller to fill. It then picks the spawn tables for
/// the session variant (`stage == 2` or not), spawns entries 4 and 5 of the
/// first, and passes progress nibble 0x48 on.
///
/// Outside stage 2 the second pick is the address of the room's script message
/// table, stored where a spawn table is expected; the cast records that.
///
/// `slot` and the store to `D_..._8018A3C8` are one chained assignment on
/// purpose: GCC then materialises the global's address ahead of `memCalloc`,
/// which is the register allocation the target has.
void func_dryfield_factory_8017D9CC(Task* arg0)
{
    Task** slot;

    arg0->msgTable = D_dryfield_factory_801826D4;
    Game_SetPtrSlot(arg0, 7);
    slot       = (D_dryfield_factory_8018A3C8 = memCalloc(4, 0));
    arg0->work = (TaskIdMap*)slot;
    if (gGameSession->at4.loc.stage == 2) {
        D_dryfield_factory_8018A3C4 = D_dryfield_factory_80186E28;
    } else {
        D_dryfield_factory_8018A3C4 = D_dryfield_factory_80186DE0;
    }
    if (gGameSession->at4.loc.stage == 2) {
        D_dryfield_factory_8018A3C0 = D_dryfield_factory_80186E94;
    } else {
        D_dryfield_factory_8018A3C0 = (TaskDesc*)D_dryfield_factory_80186EA0;
    }
    Task_SpawnFromTable(D_dryfield_factory_8018A3C4, 4, 0, (s32)D_dryfield_factory_8018A3C8);
    Task_SpawnFromTable(D_dryfield_factory_8018A3C4, 5, 0, 0);
    if (gGameSession->at4.loc.stage == 2) {
        func_dryfield_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
        SOFT_BARRIER();
    } else {
        func_dryfield_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
    }
    arg0->state++;
}
