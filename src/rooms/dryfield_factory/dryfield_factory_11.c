#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_factory_80186EF8;
extern SVECTOR D_dryfield_factory_80186F00;
extern SVECTOR D_dryfield_factory_80186F08;

/// Per-frame effect: draws up to three glowing discs at fixed points in the
/// room. The draw set is selected by the stage-visit byte
/// `gGameSession->loc.view` taken as a bit index, and each group also gates on a
/// story flag, so a disc only appears on the visits and after the event that
/// the flag records.
void func_dryfield_factory_801825F0(Task* task)
{
    s32 state;

    state = 1 << gGameSession->loc.view;
    if (GameFlag_GetNibble(0x48) != 0 && (state & 0x15068) != 0) {
        Room_Draw15(&D_dryfield_factory_80186EF8, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(0x4A) == 1) {
            Room_Draw15(&D_dryfield_factory_80186F00, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(0x4A) == 2) {
            Room_Draw15(&D_dryfield_factory_80186F08, 0x80, 0x50A0);
        }
    }
}
