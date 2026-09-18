#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// Four local-space points forming two light beams, laid end to end. splat
/// names this address because it is the one the code forms in a register; the
/// table itself starts one SVECTOR earlier, so the first beam here runs from
/// `[1]` back to `[0]` and the second from `[3]` to `[2]`.
extern SVECTOR D_dryfield_g_r_kitchen_8017EBF0[];
/// Same four-point layout, but both beams run forwards: `[0]` to `[1]` and
/// `[2]` to `[3]`.
extern SVECTOR D_dryfield_g_r_kitchen_8017EC08[];

void func_dryfield_g_r_kitchen_8017E27C(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

INCLUDE_ASM("rooms/nonmatchings/dryfield_g_r_kitchen/dryfield_g_r_kitchen_3", func_dryfield_g_r_kitchen_8017E27C);

/// Draws whichever pair of beams the room's current stage (`gGameSession`
/// byte 4) selects: 2 draws the two around the kitchen door through the shared
/// `Room_Draw24`, 3 draws the other two through this room's own beam routine.
/// Any other stage draws nothing.
void func_dryfield_g_r_kitchen_8017EB04(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->field_8;
    if (gGameSession->field_4 == 2) {
        Room_Draw24(coord, &D_dryfield_g_r_kitchen_8017EBF0[0], &D_dryfield_g_r_kitchen_8017EBF0[-1], 0x100);
        Room_Draw24(coord, &D_dryfield_g_r_kitchen_8017EBF0[2], &D_dryfield_g_r_kitchen_8017EBF0[1], 0x100);
    } else if (gGameSession->field_4 == 3) {
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[0], &D_dryfield_g_r_kitchen_8017EC08[1], 0x100);
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[2], &D_dryfield_g_r_kitchen_8017EC08[3], 0x100);
    }
}
