#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/shelter_b1_access_tunnel.h"

extern SVECTOR D_shelter_b1_access_tunnel_8017E744[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7B4[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7D4[];

void func_shelter_b1_access_tunnel_8017DD60(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[2], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[4], 0x180, 0x444);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[2], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[4], 0x180, 0x111);
            func_shelter_b1_access_tunnel_8017DEC0(&p[6], 0x180, 0x222);
            func_shelter_b1_access_tunnel_8017DEC0(&p[8], 0x180, 0x333);
            func_shelter_b1_access_tunnel_8017DEC0(&p[10], 0x180, 0x444);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7D4;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x343);
            func_shelter_b1_access_tunnel_8017DEC0(&p[-8], 0x180, 0x444);
            func_shelter_b1_access_tunnel_8017DEC0(&p[-6], 0x180, 0x333);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7B4;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x344);
            func_shelter_b1_access_tunnel_8017DEC0(&p[2], 0x200, 0x344);
            break;
        }
    }
}
