#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115730;
extern s32     D_80115734;
extern s32     D_80115750;
extern s32     D_80115754;
extern s32     D_80115758;
extern SVECTOR D_shelter_b1_south_maintenance_walkway_80182330[];
extern SVECTOR D_shelter_b1_south_maintenance_walkway_801823D0[];

void func_shelter_b1_south_maintenance_walkway_8017DA8C(Task* task)
{
    SVECTOR* p;

    if (task->state == 0) {
        D_80115758  = 0x601C9;
        D_8011572C  = 0x601E5;
        D_80115750  = 0x60201;
        D_80115734  = 0x6021C;
        D_80115730  = 0x6021B;
        D_80115754  = 0x6021D;
        task->state = 1;
    }

    switch (gGameSession->at4.loc.view) {
        case 2:
            p = D_shelter_b1_south_maintenance_walkway_80182330;
            SOFT_TOUCH_REG(p);
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw11(&p[6], 0x200, 0x400);
            break;
        case 3:
            p = D_shelter_b1_south_maintenance_walkway_80182330;
            SOFT_TOUCH_REG(p);
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw11(&p[2], 0x200, 0);
            Room_Draw11(&p[6], 0x200, 0x400);
            Room_Draw11(&p[8], 0x200, 0x400);
            break;
        case 4:
            p = D_shelter_b1_south_maintenance_walkway_801823D0;
            SOFT_TOUCH_REG(p);
            Room_Draw25(&p[0], 0x200);
            Room_Draw11(&p[-16], 0x200, 0);
            Room_Draw11(&p[-10], 0x200, -0x400);
            Room_Draw11(&p[-6], 0x200, 0x800);
            break;
        case 5:
            p = D_shelter_b1_south_maintenance_walkway_801823D0;
            SOFT_TOUCH_REG(p);
            Room_Draw25(&p[0], 0x200);
            Room_Draw11(&p[-16], 0x200, 0);
            Room_Draw11(&p[-8], 0x200, 0);
            Room_Draw11(&p[-6], 0x200, -0x400);
            Room_Draw11(&p[-4], 0x200, 0);
            Room_Draw11(&p[-2], 0x200, -0x400);
            break;
    }
}
