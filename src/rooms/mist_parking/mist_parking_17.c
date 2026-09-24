#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/mist_parking.h"

extern SVECTOR D_mist_parking_80191484[];
extern SVECTOR D_mist_parking_801914A4[];
extern SVECTOR D_mist_parking_801914B4[];
extern SVECTOR D_mist_parking_801914C4[];
extern SVECTOR D_mist_parking_801914D4[];
extern SVECTOR D_mist_parking_801914E4[];
extern SVECTOR D_mist_parking_80191554[];

void func_mist_parking_80184728(void)
{
    u8       view;
    SVECTOR* p;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_mist_parking_80185814(&D_mist_parking_80191484[0], 0x200, 0x444);
            break;
        case 18:
            p = D_mist_parking_801914A4;
            SOFT_TOUCH_REG(p);
            func_mist_parking_80185814(p, 0x200, 0x444);
            goto shared_8;
        case 7:
            func_mist_parking_80185814(&D_mist_parking_801914E4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914E4[2], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914E4[6], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914E4[8], 0x200, 0x444);
            func_mist_parking_80184A18(&D_mist_parking_801914E4[14], 0x60, 0xE0);
            break;
        case 20:
            func_mist_parking_80185814(&D_mist_parking_801914B4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914B4[2], 0x200, 0x444);
            break;
        case 9:
            func_mist_parking_80184E8C(&D_mist_parking_80191554[0], 0x60, 0x40);
            break;
        case 10:
            func_mist_parking_80185814(&D_mist_parking_801914A4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914A4[2], 0x200, 0x444);
            break;
        case 11:
            func_mist_parking_80185814(&D_mist_parking_801914A4[0], 0x200, 0x444);
            break;
        case 14:
            func_mist_parking_80185814(&D_mist_parking_801914B4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914B4[2], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914B4[8], 0x200, 0x444);
            break;
        case 15:
            func_mist_parking_80185814(&D_mist_parking_80191484[0], 0x200, 0x444);
            break;
        case 16:
            func_mist_parking_80185814(&D_mist_parking_801914D4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[2], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[4], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[6], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[8], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[12], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[14], 0x200, 0x444);
            break;
        case 5:
            func_mist_parking_80185814(&D_mist_parking_801914A4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914A4[2], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914A4[4], 0x200, 0x444);
            break;
        case 6:
        case 19:
            func_mist_parking_80185814(&D_mist_parking_801914D4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[2], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[4], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[6], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[8], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[10], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[12], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914D4[14], 0x200, 0x444);
            func_mist_parking_80184A18(&D_mist_parking_801914D4[16], 0x60, 0x100);
            break;
        case 8:
            p = D_mist_parking_801914B4;
            SOFT_TOUCH_REG(p);
        shared_8:
            TOUCH_REG(p);
            func_mist_parking_80185814(&D_mist_parking_801914B4[0], 0x200, 0x444);
            func_mist_parking_80185814(&D_mist_parking_801914C4[0], 0x200, 0x444);
            break;
    }
}
