#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ecb4.h"

extern RoomHotspot D_dryfield_factory_80186EB0[];

/// Idle state of the room's script task. It counts down the delay the prompt
/// state armed and, once that is spent and no cap is playing, hit-tests the
/// room's hotspots under the cursor: a confirmed hit copies the hotspot's `id`
/// and `promptKind` into the work block and advances to state 3, and the
/// cancel button leaves for state 5.
void func_dryfield_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = &D_80114D28;
    RoomHotspot*            hs     = D_dryfield_factory_80186EB0;
    NightFactoryScriptWork* st     = (NightFactoryScriptWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
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
