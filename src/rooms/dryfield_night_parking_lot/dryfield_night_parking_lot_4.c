#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// The room's per-view table: `Gp_State1C->field_A` latches the entry the
/// current camera index selects, and the room's effect tasks read it back.
extern u16 D_dryfield_night_parking_lot_8017EDBC[];

/// The parking lot's drawable points, one 8-byte `SVECTOR` per prop. The
/// phase each one belongs to is `Game_Session->field_4` (the room's stage).
extern SVECTOR D_dryfield_night_parking_lot_8017EDCC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDDC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDE4[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDEC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDFC[];

extern void func_dryfield_night_parking_lot_8017E08C(SVECTOR* arg0, SVECTOR* arg1, s32 arg2);

/// Parking-lot room draw: latches the view's entry of the room's per-view table
/// into `Gp_State1C->field_A`, then queues the props of the room phase
/// `Game_Session->field_4` selects - 2, 4 and 5 several points each, 3 and 6 a
/// single one. Every phase ends with the same semi-transparent `Room_Draw20`,
/// which `jump.c` cross-jumps into one tail block after the last case.
void func_dryfield_night_parking_lot_8017DC88(void)
{
    u8 view;

    view                = Gp_GetViewIndex();
    Gp_State1C->field_A = D_dryfield_night_parking_lot_8017EDBC[view - 1];
    switch (Game_Session->field_4) {
        case 2: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDCC;
            Room_Draw20(&p[0], 0, 0x300);
            Room_Draw20(&p[1], 0, 0x300);
            Room_Draw20(&p[2], 0, 0x330);
            Room_Draw20(&p[6], 1, 0x380);
            Room_Draw20(&p[7], 1, 0x380);
            break;
        }
        case 3: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDFC;
            Room_Draw20(&p[0], 1, 0x380);
            break;
        }
        case 4: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDEC;
            func_dryfield_night_parking_lot_8017E08C(&p[0], &p[1], 0x180);
            Room_Draw20(&p[2], 1, 0x380);
            Room_Draw20(&p[3], 1, 0x380);
            break;
        }
        case 5: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDDC;
            Room_Draw20(&p[0], 0, 0x300);
            Room_Draw20(&p[1], 0, 0x300);
            Room_Draw20(&p[5], 1, 0x380);
            Room_Draw20(&p[6], 1, 0x380);
            break;
        }
        case 6: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDE4;
            Room_Draw20(&p[0], 0, 0x300);
            break;
        }
    }
}
