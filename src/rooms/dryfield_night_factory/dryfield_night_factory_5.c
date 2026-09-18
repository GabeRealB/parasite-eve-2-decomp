#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

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
/// `gGameSession::loc.stage == 2` (the night factory) picks the larger spawn
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
/// `gGameSession` load quantity, and that load then wins `$v1` -- the target's
/// allocation. Split into two statements it takes `$v1` itself and the load
/// falls to `$a0`.
void func_dryfield_night_factory_80180438(Task* arg0)
{
    Task** slot;

    arg0->field_24 = D_dryfield_night_factory_80186E64;
    Game_SetPtrSlot(arg0, 7);
    slot       = (D_dryfield_night_factory_8018A7E8 = Mem_Calloc(4, 0));
    arg0->work = (TaskIdMap*)slot;
    if (gGameSession->loc.stage == 2) {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186E28;
    } else {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186DE0;
    }
    if (gGameSession->loc.stage == 2) {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186E94;
    } else {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186EA0;
    }
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 4, 0, (s32)D_dryfield_night_factory_8018A7E8);
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 5, 0, 0);
    if (gGameSession->loc.stage == 2) {
        Room_Util16(GameFlag_GetNibble(0x48) & 0xFF);
        SOFT_BARRIER();
    } else {
        Room_Util16(GameFlag_GetNibble(0x48) & 0xFF);
    }
    arg0->state++;
}

/// Handler for the room's `0x16` / `0x18` / `0x19` messages -- the factory's
/// progress gates. Like every room handler it copies the incoming record into
/// `out` and answers by editing the copy's `field_3`; `field_5` non-zero means
/// "just asking", so no prompt is started and the side effects are skipped.
///
/// `0x19` asks which visit to the factory this is: under the night session
/// (`gGameSession::loc.stage == 2`) it answers `2` once nibble `0x3A` has counted
/// two, and otherwise `1`; every other session variant answers nibble `0x61`
/// plus one. `0x19` also offers the gate a request that runs CAP command `0xE`
/// under flag nibble `-0x30` (fire while the nibble is *clear*, no item
/// prerequisite) and returns the gate's own answer. `0x18` answers `1` or `2`
/// by whether nibble `0x7A` is under four and, unless nibble `0x4A` is already
/// `2`, starts CAP slot 4 and sets nibble `field_6` to `2`. `0x16` does the
/// same nibble write and runs CAP command `0xD` while nibble `0x37` is clear.
/// A message it does not own answers `1`.
///
/// `dryfield_factory` carries this same body -- the body itself branches on the
/// night session, so one source covers both rooms -- and it wants promoting to
/// `src/rooms/lib/` once this overlay is not mid-sweep: the `shared` span
/// renumbers the units of both carrying overlays.
s32 func_dryfield_night_factory_80180574(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u8           variant;

    *out = *in;
    if (in->msgId == 0x19) {
        variant = gGameSession->loc.stage;
        if (variant == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = variant;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x18) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(0x7A) < 4) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        }
        if (in->msgId == 0x18) {
            if (GameFlag_GetNibble(0x4A) != 2) {
                if (in->field_5 != 0) {
                    return 0;
                }
                Gp_StartCapSlot(4, 1, 0);
                Gp_SetNibbleIf(in->field_6, 2);
                return 0;
            }
        }
    }
    if (in->msgId == 0x16) {
        if (GameFlag_GetNibble(0x37) == 0) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(0xD);
            return 0;
        }
    }
    if (in->msgId == 0x19) {
        req.field_0 = 0xE;
        req.field_4 = 0xE;
        req.field_8 = 0x52170013;
        req.field_C = 0x52170003;
        req.flagId  = -0x30;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, in);
    }
    return 1;
}

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
