#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115730;
extern s32     D_80115734;
extern s32     D_80115750;
extern s32     D_80115754;
extern s32     D_80115758;
extern SVECTOR D_shelter_b2_south_maintenance_walkway_80182578[];
extern SVECTOR D_shelter_b2_south_maintenance_walkway_801825C8[];
extern SVECTOR D_shelter_b2_south_maintenance_walkway_801825D8[];
extern SVECTOR D_shelter_b2_south_maintenance_walkway_80182618[];

void func_shelter_b2_south_maintenance_walkway_8017DCC4(Task* task)
{
    u8       view;
    SVECTOR* p;

    if (task->state == 0) {
        D_80115758  = 0x601D1;
        D_8011572C  = 0x601ED;
        D_80115750  = 0x60209;
        D_80115734  = 0x60221;
        D_80115730  = 0x6022C;
        D_80115754  = 0x60237;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            p = D_shelter_b2_south_maintenance_walkway_801825D8;
            SOFT_TOUCH_REG(p);
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw11(&p[2], 0x200, -0x400);
            Room_Draw11(&p[4], 0x200, 0);
            Room_Draw11(&p[6], 0x200, -0x400);
            break;
        case 3:
            p = D_shelter_b2_south_maintenance_walkway_801825C8;
            SOFT_TOUCH_REG(p);
            Room_Draw11(&p[0], 0x200, 0x800);
            Room_Draw11(&p[2], 0x200, 0);
            Room_Draw11(&p[4], 0x200, 0x400);
            break;
        case 4:
            p = D_shelter_b2_south_maintenance_walkway_80182618;
            SOFT_TOUCH_REG(p);
            Room_Draw25(&p[0], 0x200);
            Room_Draw11(&p[-20], 0x200, 0);
            Room_Draw11(&p[-18], 0x200, 0);
            Room_Draw11(&p[-16], 0x200, 0);
            Room_Draw11(&p[-14], 0x200, 0x400);
            Room_Draw11(&p[-12], 0x200, 0x400);
            Room_Draw11(&p[-10], 0x200, 0x400);
            Room_Draw11(&p[-8], 0x200, 0);
            break;
        case 5:
            p = D_shelter_b2_south_maintenance_walkway_80182578;
            SOFT_TOUCH_REG(p);
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw11(&p[6], 0x200, 0x400);
            break;
    }
}
