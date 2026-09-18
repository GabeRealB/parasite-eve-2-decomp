#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_dryfield_night_junk_yard_8018073C[];
extern SVECTOR D_dryfield_night_junk_yard_80180754[];

/// Applies game flag nibble 0x9F to the sixth sprite command of view 0 in the
/// current room's sprite record: a zero nibble draws the command, a nonzero one
/// hides it (`Gp_LinkViewSprts` skips OT-linking when `field_4` is set).
void func_dryfield_night_junk_yard_8017D9B8(u8 arg0)
{
    GameSessionFrom4* sess = (GameSessionFrom4*)&gGameSession->field_4;
    GpSprtCmd*        cmd;

    cmd = Gp_SprtTables[sess->field_3 - 1][0].field_0[sess->field_2 - 1][6].field_4;
    if (arg0 == 0) {
        cmd[5].field_4 = 0;
    } else {
        cmd[5].field_4 = 1;
    }
}

/// Junk yard room draw: on the task's first pass the room's three effect-id
/// slots are pointed at the junk yard's own ids, `Gp_State1C::field_A` is set
/// to 2, and the props of the phase `gGameSession->field_4` selects are queued -
/// phases 2/8 five points off the room's first prop table, 4/9 three off the
/// second, 5/10 and 7 four off the first. Every phase ends with the same
/// semi-transparent `Room_Draw20`, which `jump.c` cross-jumps into one tail
/// block after the last case.
void func_dryfield_night_junk_yard_8017DA14(Task* task)
{
    if (task->state == 0) {
        D_80115758 = 0x600E5;
        D_8011572C = 0x600E6;
        D_80115750 = 0x600E7;
    }
    Gp_State1C->field_A = 2;
    switch (gGameSession->field_4) {
        case 2:
        case 8: {
            SVECTOR* p = D_dryfield_night_junk_yard_8018073C;
            Room_Draw34(&p[0], 0x180, 0x400);
            Room_Draw20(&p[3], 0, 0x300);
            Room_Draw20(&p[4], 0, 0x300);
            Room_Draw20(&p[5], 2, 0x300);
            Room_Draw20(&p[6], 1, 0x200);
            break;
        }
        case 4:
        case 9: {
            SVECTOR* p = D_dryfield_night_junk_yard_80180754;
            Room_Draw20(&p[0], 0, 0x300);
            Room_Draw20(&p[1], 0, 0x300);
            Room_Draw20(&p[3], 1, 0x200);
            break;
        }
        case 5:
        case 10: {
            SVECTOR* p = D_dryfield_night_junk_yard_8018073C;
            Room_Draw34(&p[0], 0x180, 0);
            Room_Draw20(&p[2], 0, 0x300);
            Room_Draw20(&p[3], 0, 0x300);
            Room_Draw20(&p[6], 1, 0x200);
            break;
        }
        case 7: {
            SVECTOR* p = D_dryfield_night_junk_yard_8018073C;
            Room_Draw34(&p[0], 0x180, 0);
            Room_Draw20(&p[2], 0, 0x300);
            Room_Draw20(&p[3], 0, 0x300);
            break;
        }
    }
}
