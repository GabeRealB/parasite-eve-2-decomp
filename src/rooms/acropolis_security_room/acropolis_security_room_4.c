#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/acropolis_security_room.h"
#include "rooms/room_common.h"

/// The security monitor's own hotspot table, hit-tested by
/// `func_acropolis_security_room_8017ECB4`.
extern RoomHotspot D_acropolis_security_room_80182648[];

/// The five camera ids the security monitor can display, in the order the
/// `GameFlag_GetNibble(0x2A)` nibble indexes them.
extern s16 D_acropolis_security_room_801826B4[];

extern s8  D_8007216C;
extern s16 D_80114D08;

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`. Defined in the previous unit,
/// which owns the `.rodata` it sits in.
extern const AsrMonitorStateTable D_acropolis_security_room_8017D5EC;

void func_acropolis_security_room_8017E0C4(s16 id);
void func_acropolis_security_room_8017E37C(Task* task);
void func_acropolis_security_room_8017E490(Task* task);
void func_acropolis_security_room_8017E8F0(s32 x, s32 y, s32 variant);
s32  func_acropolis_security_room_8017ECB4(RoomHotspot* table, s16 x, s16 y);
s32  func_acropolis_security_room_8017FCB0(RoomHotspot* table, s16 x, s16 y);
void func_acropolis_security_room_8017EDE4(Task* task);

/// Resets both action-prompt slots before the cursor driver's first frame and
/// steps the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1). The room carries a second copy of
/// this body at `func_acropolis_security_room_80180308`.
void func_acropolis_security_room_8017EDE4(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

/// Idle state of the security room's cap script: the same hotspot scan
/// `func_acropolis_security_room_8017EB9C` runs for the monitor, but against
/// the script's own table and with the hit recorded in the script's state
/// block instead of dispatched as a cap command. A confirmed
/// (`buttons[0].state == 2`) hit copies the hotspot's `id` and `promptKind` into the
/// state block and advances to state 3; with nothing under the cursor the
/// pending sub-step is cleared and the prompt merely highlights (`mode` 1).
/// `buttons[1].state == 2` leaves the scan by advancing to state 5.
void func_acropolis_security_room_8017EE44(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    RoomHotspot*                hs     = D_acropolis_security_room_801826DC;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017FCB0(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->variant      = hs->id;
                    st->promptKind   = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        st->field_0  = 0;
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}
