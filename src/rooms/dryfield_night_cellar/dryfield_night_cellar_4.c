#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/dryfield_night_cellar.h"

/// The cellar's glow points, one pair per camera view that shows them: view 2
/// draws the pair at the first address and view 3 the pair at the second.
extern SVECTOR D_dryfield_night_cellar_8017DAD0[];
extern SVECTOR D_dryfield_night_cellar_8017DAE0[];

/// Per-frame effect: once event nibble 0x52 is 1, draws a glow sprite on each
/// of the two points belonging to the current camera view
/// (`gGameSession->at4.loc.view`), 2 or 3. Every other view draws nothing.
void func_dryfield_night_cellar_8017DA28(void)
{
    u8 visit;

    if (GameFlag_GetNibble(0x52) == 1) {
        visit = gGameSession->at4.loc.view;
        if (visit == 2) {
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAD0[0], 1, 0x280);
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAD0[1], 1, 0x280);
        } else if (visit == 3) {
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAE0[0], 1, 0x280);
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAE0[1], 1, 0x280);
        }
    }
}
