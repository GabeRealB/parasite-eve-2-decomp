#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// The driveway's three anchor pairs, six 8-byte `SVECTOR`s laid out back to
/// back from `B0`: the road's near end (`B0[0]`, `B0[1]`), its middle (`B0[2]`,
/// `B0[3]`, i.e. `C0`) and its far end (`B0[4]`, `B0[5]`, i.e. `D0`). Each pair
/// is one `Room_Draw08` sweep. `D0` is named separately because the phase-5
/// case reaches it by name (its own `lui`); the phases that reach the middle
/// anchor index the base array (`lo(B0+0x10)`), which is what makes the
/// compiler materialise that base once, in `$s0`, for the phase-3/10 case.
extern SVECTOR D_dryfield_night_driveway_801805B0[];
extern SVECTOR D_dryfield_night_driveway_801805D0;

/// Driveway room draw: marks the effect state (`field_A` = 2, the value
/// `actor_400100_text` tests), then sweeps the anchor pair the current phase
/// (`Game_Session->field_4`) selects - phases 2 and 9 the near pair, 4 and 7
/// the middle one, 5 the far one. Phases 3 and 10 sweep the near pair and then
/// the middle one, whose draw call `jump.c` cross-jumps into the tail block the
/// other phases share.
void func_dryfield_night_driveway_8017E5CC(void)
{
    Gp_State1C->field_A = 2;
    switch (Game_Session->field_4) {
        case 2:
        case 9:
            Room_Draw08(&D_dryfield_night_driveway_801805B0[0], 0x180);
            break;
        case 4:
        case 7:
            Room_Draw08(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
        case 5:
            Room_Draw08(&D_dryfield_night_driveway_801805D0, 0x180);
            break;
        case 3:
        case 10:
            Room_Draw08(&D_dryfield_night_driveway_801805B0[0], 0x180);
            Room_Draw08(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
    }
}
