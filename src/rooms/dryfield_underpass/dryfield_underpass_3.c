#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_underpass_8017EAD0[8];
extern s16     D_dryfield_underpass_8017EB10[8];

/// Per-frame effect on the room's model task: draws the visit's set of props,
/// one per offset in `D_...EAD0` whose `D_...EB10` bitmask contains the active
/// visit. The visit is `Game_Session->field_4`; the whole effect is skipped
/// unless the room flag (`GameFlag_GetNibble(0x53)`) is clear.
void func_dryfield_underpass_8017DE30(Task* task)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;
    SVECTOR*       vec;
    s16*           flags;

    coord = ((TmdObject*)task->extra)->field_8;
    mask  = 1 << Game_Session->field_4;
    if (GameFlag_GetNibble(0x53) == 0) {
        i     = 0;
        vec   = D_dryfield_underpass_8017EAD0;
        flags = D_dryfield_underpass_8017EB10;
        do {
            if (mask & *flags) {
                Room_Draw35(coord, vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
