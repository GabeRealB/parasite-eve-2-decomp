#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_cellar_8017DBBC[2];
extern SVECTOR D_dryfield_cellar_8017DBCC[2];

/// Per-frame effect on the room's model task: draws the visit's pair of
/// sprites, one per entry of a two-entry SVECTOR table of local offsets.
/// The visit is `gGameSession->at4.loc.view`; visit 2 uses `D_...DBBC` and visit 3
/// `D_...DBCC`, and any other visit draws nothing. Gated on the room flag
/// (`GameFlag_GetNibble(0x52)`).
void func_dryfield_cellar_8017DAEC(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->coords;
    if (GameFlag_GetNibble(0x52) == 1) {
        if (gGameSession->at4.loc.view == 2) {
            Room_Draw35(coord, D_dryfield_cellar_8017DBBC, 1, 0x280);
            Room_Draw35(coord, D_dryfield_cellar_8017DBBC + 1, 1, 0x280);
        } else if (gGameSession->at4.loc.view == 3) {
            Room_Draw35(coord, D_dryfield_cellar_8017DBCC, 1, 0x280);
            Room_Draw35(coord, D_dryfield_cellar_8017DBCC + 1, 1, 0x280);
        }
    }
}
