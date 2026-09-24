#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_north_maintenance_walkway.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[];

void func_shelter_b2_north_maintenance_walkway_8017DDE8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024B;
        D_80115744  = 0x60257;
        D_8011573C  = 0x60262;
        D_80115720  = 0x6026E;
        D_80115758  = 0x601D2;
        D_8011572C  = 0x601EE;
        D_80115750  = 0x6020A;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            if (GameFlag_GetNibble(0xA8) != 0) {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C28, 0x100, 0x504C);
            } else {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C30, 0x100, 0x5C40);
            }
            p = D_shelter_b2_north_maintenance_walkway_80183B90;
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[0], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[6], 0x200, -0x400);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-18], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-16], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-12], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-10], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-8], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x800);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-8], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x800);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-6], 0x200, 0x800);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x400);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-2], 0x200, -0x400);
            break;
        }
        case 6: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            func_shelter_b2_north_maintenance_walkway_8017E858(p, 0x200);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-14], 0x200, 0);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-4], 0x200, 0x800);
            func_shelter_b2_north_maintenance_walkway_8017E0DC(&p[-2], 0x200, 0);
            break;
        }
        case 7:
            if (GameFlag_GetNibble(0xA8) != 0) {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C28, 0x100, 0x504C);
            } else {
                func_shelter_b2_north_maintenance_walkway_8017EBB4(D_shelter_b2_north_maintenance_walkway_80183C30, 0x100, 0x5C40);
            }
            break;
        case 8:
            func_shelter_b2_north_maintenance_walkway_8017E0DC(D_shelter_b2_north_maintenance_walkway_80183BB0, 0x200, 0x400);
            break;
    }
}
