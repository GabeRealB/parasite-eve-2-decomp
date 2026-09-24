#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b4_lower_sewer_80181EA4[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F04[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F14[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_lower_sewer/shelter_b4_lower_sewer_3", func_shelter_b4_lower_sewer_8017E37C);

void func_shelter_b4_lower_sewer_8017E400(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758 = 0x600ED;
        D_8011572C = 0x600EE;
        D_80115750 = 0x600EF;
        if (GameFlag_GetNibble(0xB7) == 1) {
            D_8011574C = 0x6016E;
            D_80115738 = 0x6016F;
        }
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 6: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F14;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            Room_Draw01(&p[4], 0x200, 0x222);
            Room_Draw01(&p[6], 0x200, 0x222);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            Room_Draw01(&p[4], 0x200, 0x222);
            Room_Draw01(&p[6], 0x200, 0x222);
            Room_Draw01(&p[24], 0x200, 0x222);
            Room_Draw01(&p[26], 0x200, 0x222);
            Room_Draw01(&p[28], 0x200, 0x222);
            break;
        }
        case 5: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            Room_Draw01(&p[28], 0x200, 0x222);
            break;
        }
        case 7: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            Room_Draw01(&p[4], 0x200, 0x222);
            Room_Draw01(&p[6], 0x200, 0x222);
            Room_Draw01(&p[8], 0x200, 0x222);
            break;
        }
        case 8: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            Room_Draw01(&p[4], 0x200, 0x222);
            Room_Draw01(&p[6], 0x200, 0x222);
            Room_Draw01(&p[8], 0x200, 0x222);
            Room_Draw01(&p[22], 0x200, 0x222);
            Room_Draw01(&p[24], 0x200, 0x222);
            Room_Draw01(&p[26], 0x200, 0x222);
            Room_Draw01(&p[28], 0x200, 0x222);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x222);
            Room_Draw01(&p[4], 0x200, 0x222);
            Room_Draw01(&p[24], 0x200, 0x222);
            Room_Draw01(&p[26], 0x200, 0x222);
            Room_Draw01(&p[28], 0x200, 0x222);
            break;
        }
    }
}
