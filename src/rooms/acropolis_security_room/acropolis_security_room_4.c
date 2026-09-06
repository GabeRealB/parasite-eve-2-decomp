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
extern const AsrMonitorStateTable RoomsShared8017fc38Table;

void func_acropolis_security_room_8017E0C4(s16 id);
void func_acropolis_security_room_8017E37C(Task* task);
void func_acropolis_security_room_8017E490(Task* task);
void func_acropolis_security_room_8017E8F0(s32 x, s32 y, s32 variant);
s32  func_acropolis_security_room_8017ECB4(RoomHotspot* table, s16 x, s16 y);
s32  func_acropolis_security_room_8017FCB0(RoomHotspot* table, s16 x, s16 y);
void func_acropolis_security_room_8017EDE4(Task* task);

/// Per-frame cursor driver of the security-room action prompt, run as state 1
/// of `func_acropolis_security_room_8017E9D8`. Byte-for-byte the same body as
/// `func_acropolis_security_room_8017F480` in the cap script.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `func_acropolis_security_room_8017E8F0`
/// to draw the cursor. `RoomActionPrompt::targetId` doubles as the cursor speed
/// here and `field_E` as the double-press window: a second press inside that
/// many frames without the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_4", func_acropolis_security_room_8017EDE4);

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
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->idMap;

    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
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
