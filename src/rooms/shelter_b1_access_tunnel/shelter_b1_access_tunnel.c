#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

extern SVECTOR D_shelter_b1_access_tunnel_8017E744[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7B4[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7D4[];

void Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017D5FC);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", D_shelter_b1_access_tunnel_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017D760);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017D8D0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017DA68);

s32 func_shelter_b1_access_tunnel_8017DCA4(void)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCAC(void)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCB4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017DCBC);

void func_shelter_b1_access_tunnel_8017DD00(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017DD08);

void func_shelter_b1_access_tunnel_8017DD60(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            Room_Draw01(&p[0], 0x200, 0x334);
            Room_Draw01(&p[2], 0x200, 0x334);
            Room_Draw01(&p[4], 0x180, 0x444);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            Room_Draw01(&p[0], 0x200, 0x334);
            Room_Draw01(&p[2], 0x200, 0x334);
            Room_Draw01(&p[4], 0x180, 0x111);
            Room_Draw01(&p[6], 0x180, 0x222);
            Room_Draw01(&p[8], 0x180, 0x333);
            Room_Draw01(&p[10], 0x180, 0x444);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7D4;
            Room_Draw01(&p[0], 0x200, 0x343);
            Room_Draw01(&p[-8], 0x180, 0x444);
            Room_Draw01(&p[-6], 0x180, 0x333);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7B4;
            Room_Draw01(&p[0], 0x200, 0x344);
            Room_Draw01(&p[2], 0x200, 0x344);
            break;
        }
    }
}
