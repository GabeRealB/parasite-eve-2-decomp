#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/room_common.h"

extern SVECTOR D_shelter_1f_tent_80181D04[];
extern SVECTOR D_shelter_1f_tent_80181D0C[];
extern SVECTOR D_shelter_1f_tent_80181D1C[];
extern SVECTOR D_shelter_1f_tent_80181D2C[];
extern SVECTOR D_shelter_1f_tent_80181D3C[];

void func_shelter_1f_tent_8017FE10(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_tent_80181D2C;
            Room_Draw01(&p[0], 0x200, 0x111);
            Room_Draw13(&p[-5], 0x280, 0x40);
            Room_Draw13(&p[-4], 0x280, 0x444);
            Room_Draw13(&p[-3], 0x180, 0x22);
            Room_Draw13(&p[-2], 0x280, 0x444);
            Room_Draw13(&p[-1], 0x180, 0x22);
            Room_Draw32(&D_shelter_1f_tent_80181D3C[0], 0x60, 0x80);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_1f_tent_80181D04;
            Room_Draw13(&p[0], 0x280, 0x40);
            Room_Draw13(&p[1], 0x280, 0x444);
            Room_Draw13(&p[2], 0x180, 0x22);
            Room_Draw32(&p[7], 0x60, 0x80);
            break;
        }
        case 5: {
            SVECTOR* p = D_shelter_1f_tent_80181D0C;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw05(&p[6], 0x60, 0x80);
            break;
        }
        case 4:
        case 6: {
            SVECTOR* p = D_shelter_1f_tent_80181D1C;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x180, 0x22);
            break;
        }
        case 7: {
            SVECTOR* p = D_shelter_1f_tent_80181D04;
            Room_Draw13(&p[0], 0x280, 0x40);
            Room_Draw13(&p[1], 0x280, 0x444);
            Room_Draw13(&p[2], 0x180, 0x22);
            Room_Draw32(&D_shelter_1f_tent_80181D3C[0], 0x60, 0x80);
            break;
        }
    }
}
