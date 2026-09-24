#include "common.h"

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

extern SVECTOR D_shelter_b1_sleeping_quarters_8018054C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018055C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018056C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018058C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018059C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805BC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805EC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018060C[];

void func_shelter_b1_sleeping_quarters_8017D8E0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x60220;
        D_80115730  = 0x6022B;
        D_80115754  = 0x60236;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            Room_Draw12(D_shelter_b1_sleeping_quarters_8018058C, 0x200, 0, 0x111);
        case 2:
            Room_Draw12(D_shelter_b1_sleeping_quarters_8018054C, 0x200, 0, 0x10);
            break;
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018056C;
            Room_Draw12(&p[0], 0x200, 0x800, 0x111);
            Room_Draw12(&p[2], 0x200, 0x800, 0x111);
            Room_Draw12(&p[6], 0x200, -0x400, 0x111);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018059C;
            Room_Draw12(&p[0], 0x200, 0x800, 0x111);
            Room_Draw12(&p[6], 0x200, 0x800, 0x111);
            Room_Draw12(&p[8], 0x200, 0, 0x111);
            break;
        }
        case 6: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_801805EC;
            Room_Draw12(&p[0], 0x200, 0x400, 0x111);
            Room_Draw12(&p[2], 0x200, 0x400, 0x111);
            break;
        }
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018056C;
            Room_Draw12(&p[0], 0x200, -0x400, 0x111);
            Room_Draw12(&p[2], 0x200, 0, 0x111);
            Room_Draw12(&p[8], 0x200, 0x800, 0x111);
            Room_Draw12(&p[12], 0x200, 0x800, 0x111);
            Room_Draw12(&p[14], 0x200, 0, 0x111);
            break;
        }
        case 8:
            Room_Draw12(D_shelter_b1_sleeping_quarters_801805BC, 0x200, 0x800, 0x111);
        case 9:
            Room_Draw30(D_shelter_b1_sleeping_quarters_8018055C, 0x300, 0x100);
            break;
        case 10: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018060C;
            Room_Draw12(&p[0], 0x200, 0x800, 0x111);
            Room_Draw12(&p[2], 0x200, 0x800, 0x111);
            Room_Draw12(&p[4], 0x200, 0x800, 0x111);
            Room_Draw12(&p[6], 0x200, 0x800, 0x111);
            break;
        }
    }
}
