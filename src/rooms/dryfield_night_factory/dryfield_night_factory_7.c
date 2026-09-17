#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ecb4.h"

void func_dryfield_night_factory_801809EC(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_7", func_dryfield_night_factory_801809F4);

/// Idle state of the night factory's cap script, run as state 2 of
/// `RoomsShared8017fc38Table`. It holds the prompt idle for the `field_8`
/// frames the prompt state armed -- decrementing that countdown first and
/// bailing out while it is still non-zero or while a cap is playing -- and
/// otherwise hit-tests the room's hotspot table, which is the same body
/// `func_acropolis_security_room_8017EE44` runs for the security monitor and
/// `func_dryfield_factory_80180A4C` for the same room in daylight.
///
/// A confirmed hit (`buttons[0].state == 2`) copies the hotspot's `id` and
/// `promptKind` into the work block and advances to state 3; with nothing under
/// the cursor the prompt merely highlights (`mode` 1). `buttons[1].state == 2`
/// leaves the scan by advancing to state 5.
void func_dryfield_night_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = &D_80114D28;
    RoomHotspot*            hs     = D_dryfield_night_factory_80186EBC;
    NightFactoryScriptWork* st     = (NightFactoryScriptWork*)task->idMap;

    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_C      = hs->id;
                    st->field_E      = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_7", jtbl_dryfield_night_factory_8017D698);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_7", jtbl_dryfield_night_factory_8017D6B0);
