#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

/// Per-view spawn table for the observatory's ambient effect. Entry `i` of
/// `D_acropolis_observatory_8017FEB8` is the bitmask of camera views that want
/// effect `i`, tested against `1 << Gp_GetViewIndex()`; the matching entry of
/// `D_acropolis_observatory_8017FE78` is the offset the effect is spawned at.
extern SVECTOR D_acropolis_observatory_8017FE78[8];
extern u16     D_acropolis_observatory_8017FEB8[8];

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_3", func_acropolis_observatory_8017E19C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_3", func_acropolis_observatory_8017E424);

/// Re-spawns the observatory's ambient effects for the current camera view,
/// one per entry whose view mask contains the active view. Skipped entirely
/// once `Gp_State1C::field_4` has reached 4, i.e. once the room has faded out.
void func_acropolis_observatory_8017E6F8(Task* task)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;
    SVECTOR*       vec;
    u16*           flags;

    coord = ((TmdObject*)task->extra)->field_8;
    mask  = 1 << Gp_GetViewIndex();
    if (Gp_State1C->field_4 < 4) {
        i     = 0;
        vec   = D_acropolis_observatory_8017FE78;
        flags = D_acropolis_observatory_8017FEB8;
        do {
            if (*flags & mask) {
                Gp_SpawnEff(0x60028, coord, 0, vec);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
