#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115738;
extern s32     D_8011574C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_shelter_b2_main_corridor_80182F7C[];
extern SVECTOR D_shelter_b2_main_corridor_80182F9C[];
extern SVECTOR D_shelter_b2_main_corridor_80182FAC[];
extern SVECTOR D_shelter_b2_main_corridor_80182FBC[];
extern SVECTOR D_shelter_b2_main_corridor_80182FCC[];
extern SVECTOR D_shelter_b2_main_corridor_8018305C[];
extern SVECTOR D_shelter_b2_main_corridor_8018306C[];

void func_shelter_b2_main_corridor_8017EC34(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601D3;
            D_8011572C  = 0x601EF;
            D_80115750  = 0x6020B;
            D_8011574C  = 0x6016A;
            D_80115738  = 0x6016B;
            arg0->state = 1;
        case 1:
            switch (Gp_GetViewIndex() & 0xFF) {
                case 2: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182F7C;
                    Room_Draw12(&p[0], 0x200, 0, 0x111);
                    Room_Draw12(&p[2], 0x200, 0, 0x111);
                    break;
                }
                case 3: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182FAC;
                    Room_Draw12(&p[0], 0x200, 0x800, 0x10);
                    Room_Draw12(&p[2], 0x200, 0, 0x10);
                    Room_Draw12(&p[4], 0x200, 0, 0x10);
                    Room_Draw12(&p[6], 0x200, 0x800, 0x100);
                    Room_Draw20(&p[14], 1, 0x300);
                    Room_Draw20(&p[21], 1, 0x300);
                    break;
                }
                case 4: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182F9C;
                    Room_Draw12(&p[0], 0x200, 0x800, 0x10);
                    Room_Draw12(&p[24], 0x200, 0x800, 0x111);
                    Room_Draw12(&p[28], 0x200, -0x400, 0x111);
                    Room_Draw12(&p[32], 0x200, 0x800, 0x111);
                    break;
                }
                case 5:
                    Room_Draw12(D_shelter_b2_main_corridor_80182FBC, 0x200, 0, 0x10);
                    break;
                case 6:
                    Room_Draw12(D_shelter_b2_main_corridor_80182FAC, 0x200, 0x800, 0x10);
                case 7: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182FCC;
                    Room_Draw12(&p[0], 0x200, 0, 0x10);
                    Room_Draw12(&p[2], 0x200, 0x800, 0x100);
                    break;
                }
                case 8: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182FAC;
                    Room_Draw12(&p[0], 0x200, 0x800, 0x10);
                    Room_Draw12(&p[24], 0x200, 0x800, 0x111);
                    Room_Draw12(&p[28], 0x200, -0x400, 0x111);
                    Room_Draw12(&p[32], 0x200, 0x800, 0x111);
                    break;
                }
                case 9:
                    Room_Draw12(D_shelter_b2_main_corridor_80182FCC, 0x200, 0, 0x10);
                    break;
                case 10: {
                    SVECTOR* p = D_shelter_b2_main_corridor_8018305C;
                    Room_Draw12(&p[0], 0x200, 0x400, 0x111);
                    Room_Draw12(&p[4], 0x200, 0x800, 0x111);
                    Room_Draw12(&p[8], 0x200, 0x400, 0x111);
                    break;
                }
                case 11: {
                    SVECTOR* p = D_shelter_b2_main_corridor_8018306C;
                    Room_Draw12(&p[0], 0x200, 0x400, 0x111);
                    Room_Draw12(&p[4], 0x200, 0x800, 0x111);
                    Room_Draw12(&p[8], 0x200, 0x400, 0x111);
                    break;
                }
            }
            break;
    }
}
