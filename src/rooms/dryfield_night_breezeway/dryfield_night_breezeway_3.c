#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_breezeway.h"

/// The anchor points of the room's lights: one run of eight `SVECTOR`s. The
/// draw below reaches some points through their own address and others by
/// indexing from an earlier one, so the run carries three names.
extern SVECTOR D_dryfield_night_breezeway_8017E6A4[];
extern SVECTOR D_dryfield_night_breezeway_8017E6AC[];
extern SVECTOR D_dryfield_night_breezeway_8017E6C4;

/// The room's light draw: sets `Gp_State1C->roomEffectMode` to 2, then draws
/// the lights the current camera view (`gGameSession->at4.loc.view`) can see.
/// View 2 draws a sprite and a beam; view 3 draws a beam and then everything
/// view 4 draws, a pulsing star and a second beam. Other views draw nothing.
void func_dryfield_night_breezeway_8017E5BC(void)
{
    Gp_State1C->roomEffectMode = 2;
    switch (gGameSession->at4.loc.view) {
        case 2:
            func_dryfield_night_breezeway_8017E334(&D_dryfield_night_breezeway_8017E6AC[0], 2, 0x400);
            func_dryfield_night_breezeway_8017DB4C(&D_dryfield_night_breezeway_8017E6AC[5], 0x180);
            break;
        case 3:
            func_dryfield_night_breezeway_8017DB4C(&D_dryfield_night_breezeway_8017E6C4, 0x180);
            /* fallthrough */
        case 4:
            func_dryfield_night_breezeway_8017D6D8(&D_dryfield_night_breezeway_8017E6A4[0], 0x600, 0x80);
            func_dryfield_night_breezeway_8017DB4C(&D_dryfield_night_breezeway_8017E6A4[2], 0x180);
            break;
    }
}
