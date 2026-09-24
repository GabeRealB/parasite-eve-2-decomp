#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

extern SVECTOR D_shelter_b2_breeding_room_80180450[];
extern SVECTOR D_shelter_b2_breeding_room_80180470[];
extern SVECTOR D_shelter_b2_breeding_room_80180480[];
extern SVECTOR D_shelter_b2_breeding_room_801804C0[];

void func_shelter_b2_breeding_room_8017D898(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6027C;
        D_80115730  = 0x6027D;
        D_80115754  = 0x6027E;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw01(&D_shelter_b2_breeding_room_80180470[0], 0x100, 0x142);
            Room_Draw31(&D_shelter_b2_breeding_room_80180470[24], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180470[25], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180470[26], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180470[27], 0x200, 0x444);
            break;
        case 3:
            Room_Draw31(&D_shelter_b2_breeding_room_80180450[0], 0x200, 0x222);
            Room_Draw31(&D_shelter_b2_breeding_room_80180450[1], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_breeding_room_80180450[2], 0x200, 0x222);
            Room_Draw31(&D_shelter_b2_breeding_room_80180450[12], 0x200, 0x333);
            Room_Draw31(&D_shelter_b2_breeding_room_80180450[13], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180450[16], 0x200, 0x333);
            Room_Draw31(&D_shelter_b2_breeding_room_80180450[17], 0x200, 0x444);
            break;
        case 4:
            Room_Draw31(&D_shelter_b2_breeding_room_801804C0[0], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_801804C0[1], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_801804C0[4], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_801804C0[5], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_801804C0[7], 0x200, 0x444);
            break;
        case 5:
            Room_Draw01(&D_shelter_b2_breeding_room_80180480[0], 0x100, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[8], 0x200, 0x111);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[9], 0x200, 0x111);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[12], 0x200, 0x222);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[13], 0x200, 0x222);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[14], 0x200, 0x333);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[15], 0x200, 0x333);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[16], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[17], 0x200, 0x444);
            break;
        case 6:
            Room_Draw01(&D_shelter_b2_breeding_room_80180480[0], 0x100, 0x444);
            Room_Draw01(&D_shelter_b2_breeding_room_80180480[2], 0x100, 0x444);
            Room_Draw01(&D_shelter_b2_breeding_room_80180480[4], 0x100, 0x142);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[16], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[17], 0x200, 0x444);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[18], 0x200, 0x222);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[19], 0x200, 0x222);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[20], 0x200, 0x111);
            Room_Draw31(&D_shelter_b2_breeding_room_80180480[21], 0x200, 0x111);
            break;
    }
}
