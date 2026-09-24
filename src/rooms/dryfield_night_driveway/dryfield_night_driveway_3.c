#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "rooms/dryfield_night_driveway.h"
#include "rooms/room_common.h"

/// Three pairs of beam end points, back to back: the first pair at `B0[0]`,
/// the second at `B0[2]` and the third at `D0`. `D0` is its own symbol because
/// the code reaches the third pair by name while it indexes the array for the
/// second.
extern SVECTOR D_dryfield_night_driveway_801805B0[];
extern SVECTOR D_dryfield_night_driveway_801805D0;

/// Room draw hook: sets the effect mode to 2, then draws the beams the current
/// view (`gGameSession->at4.loc.view`) shows - views 2 and 9 the first pair, 4
/// and 7 the second, 5 the third, and 3 and 10 both the first and second.
void func_dryfield_night_driveway_8017E5CC(void)
{
    Gp_State1C->roomEffectMode = 2;
    switch (gGameSession->at4.loc.view) {
        case 2:
        case 9:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[0], 0x180);
            break;
        case 4:
        case 7:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
        case 5:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805D0, 0x180);
            break;
        case 3:
        case 10:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[0], 0x180);
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
    }
}
