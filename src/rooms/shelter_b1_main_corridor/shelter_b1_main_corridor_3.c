#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_main_corridor_801830D4[];
extern SVECTOR D_shelter_b1_main_corridor_80183114[];
extern SVECTOR D_shelter_b1_main_corridor_80183124[];
extern SVECTOR D_shelter_b1_main_corridor_80183134[];
extern SVECTOR D_shelter_b1_main_corridor_80183144[];

void func_shelter_b1_main_corridor_8017DDF0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60248;
        D_80115744  = 0x60254;
        D_8011573C  = 0x6025F;
        D_80115720  = 0x6026B;
        D_80115758  = 0x601CC;
        D_8011572C  = 0x601E8;
        D_80115750  = 0x60204;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_801830D4;
            Room_Draw12(&p[0], 0x200, 0x800, 0x111);
            Room_Draw12(&p[2], 0x200, 0x800, 0x111);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183114;
            Room_Draw12(&p[0], 0x200, 0x800, 0x10);
            Room_Draw12(&p[4], 0x200, 0, 0x10);
            Room_Draw20(&p[10], 1, 0x300);
            Room_Draw20(&p[11], 1, 0x300);
            Room_Draw20(&p[17], 1, 0x300);
            Room_Draw20(&p[18], 1, 0x300);
            break;
        }
        case 5:
            Room_Draw12(D_shelter_b1_main_corridor_80183134, 0x200, 0, 0x10);
            break;
        case 6:
            Room_Draw12(D_shelter_b1_main_corridor_80183114, 0x200, 0x800, 0x10);
            break;
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183124;
            Room_Draw12(&p[0], 0x200, 0x800, 0x10);
            Room_Draw12(&p[4], 0x200, 0, 0x10);
            Room_Draw12(&p[6], 0x200, 0x800, 0x100);
            Room_Draw20(&p[13], 1, 0x300);
            Room_Draw20(&p[14], 1, 0x300);
            Room_Draw20(&p[20], 1, 0x300);
            Room_Draw20(&p[21], 1, 0x300);
            break;
        }
        case 8: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183124;
            Room_Draw12(&p[0], 0x200, 0x800, 0x10);
            Room_Draw12(&p[6], 0x200, 0x800, 0x100);
            break;
        }
        case 9:
            Room_Draw12(D_shelter_b1_main_corridor_80183144, 0x200, 0, 0x10);
            break;
        case 10:
            Room_Draw12(D_shelter_b1_main_corridor_80183124, 0x200, 0x800, 0x10);
            break;
    }
}
