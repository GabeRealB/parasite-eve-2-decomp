#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_shelter_1f_bulwark_80180378[];
extern SVECTOR D_shelter_1f_bulwark_80180398[];

void func_shelter_1f_bulwark_8017E2A4(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601C2;
        D_8011572C  = 0x601C3;
        D_80115750  = 0x601C4;
        arg0->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_bulwark_80180378;
            Room_Draw29(&p[0], 0x300, 0x210);
            Room_Draw29(&p[1], 0x300, 0x210);
            Room_Draw29(&p[2], 0x300, 0x111);
            Room_Draw29(&p[3], 0x300, 0x111);
            break;
        }
        case 3:
            Room_Draw29(&D_shelter_1f_bulwark_80180398[0], 0x200, 0x200);
            break;
    }
}
