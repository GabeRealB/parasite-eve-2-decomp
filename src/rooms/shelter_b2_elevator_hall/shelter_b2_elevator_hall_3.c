#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b2_elevator_hall_801837D8[];
extern SVECTOR D_shelter_b2_elevator_hall_801837F8[];
extern SVECTOR D_shelter_b2_elevator_hall_80183808[];
extern SVECTOR D_shelter_b2_elevator_hall_80183868[];
extern SVECTOR D_shelter_b2_elevator_hall_801838A8[];
extern SVECTOR D_shelter_b2_elevator_hall_801838B0[];

void func_shelter_b2_elevator_hall_8017DD60(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024A;
        D_80115744  = 0x60256;
        D_8011573C  = 0x60261;
        D_80115720  = 0x6026D;
        D_80115758  = 0x601D0;
        D_8011572C  = 0x601EC;
        D_80115750  = 0x60208;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b2_elevator_hall_801837D8;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[2], 0x180, 0x111);
            Room_Draw01(&p[16], 0x180, 0x111);
            Room_Draw01(&p[24], 0x200, 0x412);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b2_elevator_hall_801837F8;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[8], 0x180, 0x111);
            Room_Draw01(&p[12], 0x180, 0x111);
            Room_Draw01(&p[20], 0x200, 0x412);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b2_elevator_hall_80183808;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[8], 0x180, 0x111);
            Room_Draw01(&p[12], 0x180, 0x111);
            Room_Draw01(&p[16], 0x200, 0x412);
            break;
        }
        case 5: {
            SVECTOR* p;
            if (GameFlag_GetNibble(0xA9) != 0) {
                Room_Draw15(D_shelter_b2_elevator_hall_801838A8, 0x100, 0x504C);
            } else {
                Room_Draw15(D_shelter_b2_elevator_hall_801838B0, 0x100, 0x5C40);
            }
            p = D_shelter_b2_elevator_hall_80183868;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[2], 0x200, 0x412);
            Room_Draw01(&p[4], 0x200, 0x412);
            break;
        }
        case 6: {
            SVECTOR* p;
            if (GameFlag_GetNibble(0xA9) != 0) {
                Room_Draw15(D_shelter_b2_elevator_hall_801838A8, 0x100, 0x504C);
            } else {
                Room_Draw15(D_shelter_b2_elevator_hall_801838B0, 0x100, 0x5C40);
            }
            p = D_shelter_b2_elevator_hall_80183868;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[2], 0x200, 0x412);
            break;
        }
    }
}
