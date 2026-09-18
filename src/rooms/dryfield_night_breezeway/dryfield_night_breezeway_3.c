#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// The room's effect chain: eight 8-byte `SVECTOR` anchors laid out back to
/// back from `E6A4`, each pair of neighbours a segment a draw call sweeps.
/// `E6AC` is named separately because the phase-2 case reaches it by name
/// (its own `lui`) where the phase-4 case reaches `E6B4` as `E6A4[2]` - indexing
/// emits the base plus 0x10. `E6C4` is only ever reached by name.
extern SVECTOR D_dryfield_night_breezeway_8017E6A4[];
extern SVECTOR D_dryfield_night_breezeway_8017E6AC[];
extern SVECTOR D_dryfield_night_breezeway_8017E6C4;

/// Breezeway room draw: marks the effect state (`field_A` = 2, the value
/// `actor_400100_text` tests), then sweeps the segment the current phase
/// (`gGameSession->loc.view`) selects. Phase 2 sweeps the chain's far end, phase
/// 3 the middle pair and phase 4 the near end plus the whole-chain
/// `Room_Draw38`; each phase names its segment as its last draw, which `jump.c`
/// cross-jumps into one tail block after the last case.
void func_dryfield_night_breezeway_8017E5BC(void)
{
    Gp_State1C->field_A = 2;
    switch (gGameSession->loc.view) {
        case 2:
            Room_Draw17(&D_dryfield_night_breezeway_8017E6AC[0], 2, 0x400);
            Room_Draw08(&D_dryfield_night_breezeway_8017E6AC[5], 0x180);
            break;
        case 3:
            Room_Draw08(&D_dryfield_night_breezeway_8017E6C4, 0x180);
            /* fallthrough */
        case 4:
            Room_Draw38(&D_dryfield_night_breezeway_8017E6A4[0], 0x600, 0x80);
            Room_Draw08(&D_dryfield_night_breezeway_8017E6A4[2], 0x180);
            break;
    }
}
