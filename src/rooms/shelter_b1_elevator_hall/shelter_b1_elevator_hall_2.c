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

extern SVECTOR D_shelter_b1_elevator_hall_80182CF4[];
extern SVECTOR D_shelter_b1_elevator_hall_80182D04[];
extern SVECTOR D_shelter_b1_elevator_hall_80182D14[];
extern SVECTOR D_shelter_b1_elevator_hall_80182D24[];
extern SVECTOR D_shelter_b1_elevator_hall_80182D34[];
extern SVECTOR D_shelter_b1_elevator_hall_80182D84[];

/// On the task's first tick stores seven room-specific values into resident
/// gameplay globals, then draws the `Room_Draw01` placements the current camera
/// view shows. Views 2 and 9 both end on `D_shelter_b1_elevator_hall_80182CF4[26]`;
/// writing that address off the array rather than through its own symbol is
/// what lets jump2 share view 9's call as view 2's tail.
void func_shelter_b1_elevator_hall_8017DC80(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60245;
        D_80115744  = 0x60251;
        D_8011573C  = 0x6025C;
        D_80115720  = 0x60268;
        D_80115758  = 0x601C8;
        D_8011572C  = 0x601E4;
        D_80115750  = 0x60200;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw01(&D_shelter_b1_elevator_hall_80182CF4[0], 0x180, 0x444);
            Room_Draw01(&D_shelter_b1_elevator_hall_80182CF4[26], 0x180, 0x44);
            break;
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D04;
            Room_Draw01(&p[0], 0x180, 0x444);
            Room_Draw01(&p[18], 0x200, 0x421);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D14;
            Room_Draw01(&p[0], 0x180, 0x444);
            Room_Draw01(&p[8], 0x180, 0x444);
            Room_Draw01(&p[16], 0x200, 0x421);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D24;
            Room_Draw01(&p[0], 0x180, 0x444);
            Room_Draw01(&p[6], 0x180, 0x444);
            Room_Draw01(&p[8], 0x180, 0x444);
            Room_Draw01(&p[12], 0x180, 0x444);
            Room_Draw01(&p[16], 0x200, 0x421);
            break;
        }
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D84;
            Room_Draw01(&p[0], 0x180, 0x444);
            Room_Draw01(&p[6], 0x200, 0x421);
            break;
        }
        case 8: {
            SVECTOR* p;
            p = D_shelter_b1_elevator_hall_80182D34;
            Room_Draw01(&p[0], 0x180, 0x444);
            Room_Draw01(&p[2], 0x180, 0x444);
            Room_Draw01(&p[14], 0x200, 0x421);
            break;
        }
        case 9:
            Room_Draw01(&D_shelter_b1_elevator_hall_80182CF4[26], 0x180, 0x44);
            break;
    }
}
