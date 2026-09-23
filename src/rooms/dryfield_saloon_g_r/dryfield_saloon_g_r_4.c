#include "common.h"

#include "gameplay/gameplay.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern SVECTOR D_dryfield_saloon_g_r_8017ECE4[];
extern SVECTOR D_dryfield_saloon_g_r_8017ED4C[];
extern s16     D_dryfield_saloon_g_r_8017ED84[];

void func_dryfield_saloon_g_r_8017DEC4(GsCOORDINATE2* coord);

void func_dryfield_saloon_g_r_8017DA70(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    for (i = 0; i < 6; i++) {
        if (mask & D_dryfield_saloon_g_r_8017ED84[i]) {
            Room_Draw35(coord, &D_dryfield_saloon_g_r_8017ECE4[i], 0, 0x200);
        }
    }
    for (i = 6; i < 11; i++) {
        if (mask & D_dryfield_saloon_g_r_8017ED84[i]) {
            Room_Draw35(coord, &D_dryfield_saloon_g_r_8017ECE4[i], 2, 0x200);
        }
    }
    if (mask & D_dryfield_saloon_g_r_8017ED84[12]) {
        func_dryfield_saloon_g_r_8017DEC4(coord);
    }
    if (mask & D_dryfield_saloon_g_r_8017ED84[11]) {
        Room_Draw24(coord, D_dryfield_saloon_g_r_8017ED4C, D_dryfield_saloon_g_r_8017ED4C - 1, 0x100);
    }
}
