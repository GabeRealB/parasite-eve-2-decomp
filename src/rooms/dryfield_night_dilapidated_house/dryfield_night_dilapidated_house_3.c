#include "common.h"

#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_dryfield_night_dilapidated_house_8017DD30(GsCOORDINATE2* coord, s32 arg1);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_dilapidated_house/dryfield_night_dilapidated_house_3", func_dryfield_night_dilapidated_house_8017DD30);

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix and then re-poses three of its parts. The current visit is the
/// stage-visit byte `Game_Session->field_4` taken as a bit index, and each pose
/// is gated on that bit being one of a fixed set of visits; the second set is
/// contained in the other two, so a visit in it runs all three poses.
void func_dryfield_night_dilapidated_house_8017E670(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    coord = ((TmdObject*)arg0->extra)->field_8;
    mask  = 1 << Game_Session->field_4;
    Gp_UpdateCoord(coord);
    if (mask & 0x99C) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0);
    }
    if (mask & 0x998) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 8);
    }
    if (mask & 0x9F8) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0x10);
    }
}
