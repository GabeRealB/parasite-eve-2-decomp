#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/room_common.h"

extern SVECTOR D_neo_ark_substation_8017E310[];
extern SVECTOR D_neo_ark_substation_8017E330[];
extern SVECTOR D_neo_ark_substation_8017E350[];
extern SVECTOR D_neo_ark_substation_8017E360[];
extern SVECTOR D_neo_ark_substation_8017E380[];

void func_neo_ark_substation_8017D874(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_neo_ark_substation_8017E310;
            Room_Draw01(&p[0], 0x200, 0x444);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_substation_8017E310;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw01(&p[2], 0x200, 0x444);
            Room_Draw01(&p[4], 0x200, 0x444);
            Room_Draw01(&p[6], 0x200, 0x444);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_substation_8017E310;
            Room_Draw01(&p[0], 0x200, 0x222);
            Room_Draw01(&p[2], 0x200, 0x333);
            Room_Draw01(&p[4], 0x200, 0x444);
            Room_Draw01(&p[6], 0x200, 0x444);
            Room_Draw01(&p[16], 0x200, 0x222);
            Room_Draw01(&p[18], 0x200, 0x333);
            break;
        }
        case 5: {
            SVECTOR* p = D_neo_ark_substation_8017E330;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw01(&p[2], 0x200, 0x333);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_substation_8017E350;
            Room_Draw01(&p[0], 0x200, 0x333);
            Room_Draw01(&p[2], 0x200, 0x444);
            Room_Draw01(&p[12], 0x200, 0x444);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_substation_8017E360;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw01(&p[2], 0x200, 0x333);
            Room_Draw01(&p[4], 0x200, 0x222);
            Room_Draw01(&p[12], 0x200, 0x444);
            Room_Draw01(&p[14], 0x200, 0x333);
            Room_Draw01(&p[16], 0x200, 0x222);
            break;
        }
        case 8: {
            SVECTOR* p = D_neo_ark_substation_8017E380;
            Room_Draw01(&p[0], 0x200, 0x444);
            break;
        }
    }
}
