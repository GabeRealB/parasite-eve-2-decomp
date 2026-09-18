#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// The water tower's effect chain, six 8-byte `SVECTOR` anchors laid out
/// back to back from `E71C`: the tower's two lower rungs (`E71C[0]`, `E71C[1]`),
/// the platform below the tank (`E71C[2]`), then `E734`, `E73C` and `E744`,
/// each pair of neighbours a segment a draw call sweeps. `E73C` and `E744` are
/// named separately because the phase-2 case reaches `E73C` by name where the
/// phase-4 case reaches the same address as `E71C[4]` - indexing emits the base
/// plus 0x20, naming it emits its own `lui`.
extern SVECTOR D_dryfield_night_water_tower_8017E71C[5];
extern SVECTOR D_dryfield_night_water_tower_8017E734;
extern SVECTOR D_dryfield_night_water_tower_8017E73C;
extern SVECTOR D_dryfield_night_water_tower_8017E744;

/// Water tower room draw: marks the effect state (`field_A` = 2, the value
/// `actor_400100_text` and `Gp_EffCtlTaskAC` test), then sweeps the segment the
/// current phase (`gGameSession->at4.loc.view`) selects. Each phase names its
/// segment as the last of its draws, which `jump.c` cross-jumps into one tail
/// block after the last case; phases 3 and 4 also draw the rungs below it, and
/// phase 4 the one above as well.
void func_dryfield_night_water_tower_8017DB80(void)
{
    Gp_State1C->field_A = 2;
    switch (gGameSession->at4.loc.view) {
        case 2:
            Room_Draw17(&D_dryfield_night_water_tower_8017E73C, 2, 0x400);
            break;
        case 3:
            Room_Draw08(&D_dryfield_night_water_tower_8017E71C[0], 0x100);
            Room_Draw17(&D_dryfield_night_water_tower_8017E71C[2], 2, 0x400);
            break;
        case 4:
            Room_Draw08(&D_dryfield_night_water_tower_8017E71C[0], 0x100);
            Room_Draw17(&D_dryfield_night_water_tower_8017E71C[2], 2, 0x400);
            Room_Draw17(&D_dryfield_night_water_tower_8017E71C[4], 2, 0x400);
            break;
        case 5:
            Room_Draw17(&D_dryfield_night_water_tower_8017E734, 2, 0x400);
            break;
        case 7:
        case 10:
            Room_Draw17(&D_dryfield_night_water_tower_8017E744, 2, 0x400);
            Room_Draw17(&D_dryfield_night_water_tower_8017E744, 2, 0x400);
            break;
    }
}
