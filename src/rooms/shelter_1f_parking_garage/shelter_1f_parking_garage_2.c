#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_shelter_1f_parking_garage_80180BFC[];
extern SVECTOR D_shelter_1f_parking_garage_80180C4C[];

void func_shelter_1f_parking_garage_8017DF6C(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601D6;
        D_8011572C  = 0x601F2;
        D_80115750  = 0x6020E;
        arg0->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_parking_garage_80180BFC;
            Room_Draw33(&p[0], 0x200, 0x800, 0x210);
            Room_Draw33(&p[2], 0x200, 0x800, 0x210);
            Room_Draw33(&p[6], 0x200, 0, 0x210);
            Room_Draw33(&p[8], 0x200, 0, 0x210);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_1f_parking_garage_80180C4C;
            Room_Draw29(&p[0], 0x300, 0x200);
            Room_Draw33(&p[-12], 0x200, 0x800, 0x210);
            Room_Draw33(&p[-6], 0x200, 0, 0x210);
            break;
        }
    }
}
