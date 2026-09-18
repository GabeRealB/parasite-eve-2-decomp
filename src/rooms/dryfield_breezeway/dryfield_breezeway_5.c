#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "rooms/dryfield_breezeway.h"
#include "rooms/room_common.h"

/// The action-prompt arm `Room_Util14` performs, plus a reset of the caller's
/// kill countdown: highlights the prompt for the fixed target id 0x80, clears
/// the on-screen position `func_800D4E78` fills in again when the prompt is
/// spawned, and steps the caller's script on one state.
void func_dryfield_breezeway_8017FD68(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// The action-prompt arm `Room_Util21` performs, run once the room's hotspot
/// scan has landed on an entry: re-seeds the cursor scan
/// `func_dryfield_breezeway_8017EB8C` at its reset position, clears the
/// prompt's highlight state, then re-spawns the prompt at the coordinates the
/// gameplay side left in `D_80114D28` with the display mode the scan latched in
/// `DbwEventWork::promptKind`, and steps the caller's script on one state.
void func_dryfield_breezeway_8017FD9C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    DbwEventWork*     work   = (DbwEventWork*)task->work;

    func_dryfield_breezeway_8017EB8C(task, 0, 0x20);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Closes whatever the hotspot scan left up and picks the room's next state:
/// re-seeds the cursor scan `func_dryfield_breezeway_8017EB8C` and clears the
/// prompt's highlight state as the arm above does, then interrogates the
/// gameplay side. While `func_800D4EC0` still reports a prompt on screen there
/// is nothing to decide, so the arm tears one down with cap slot 7 and parks on
/// state 2; once it is gone the `DbwEventWork::field_40` answer latch the
/// message handler `func_dryfield_breezeway_8017FBC8` wrote decides between
/// state 6 (the key item was accepted here) and state 2.
void func_dryfield_breezeway_8017FE08(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    DbwEventWork*     work   = (DbwEventWork*)task->work;
    s32               state;

    func_dryfield_breezeway_8017EB8C(task, 0, 0x20);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        Gp_StartCapSlot(7, 0, 0);
        state = 2;
    } else if (work->field_40 == 1) {
        state = 6;
    } else {
        state = 2;
    }
    /* `*&state`: taking the address keeps `state` in a stack slot, so the arms
       above are memory stores rather than the register assignments jump.c's
       `if (c) x = a; else x = b;` fold needs to hoist the else arm over the
       `field_40` test. Keeping that arm in its own block is what puts the value
       in $v0. */
    task->state = *&state;
}
