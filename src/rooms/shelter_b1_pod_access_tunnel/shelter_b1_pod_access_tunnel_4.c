#include "common.h"

#include "gameplay/D4.h"

#include "rooms/room_common.h"

#include "main/task.h"

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_pod_access_tunnel_801839A4[];
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839E4[];

void func_shelter_b1_pod_access_tunnel_8017E7D4(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601CD;
        D_8011572C  = 0x601E9;
        D_80115750  = 0x60205;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839E4;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[2], 0x180, 0x111);
        } break;
        case 3:
        case 7: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[2], 0x180, 0x111);
            Room_Draw01(&p[4], 0x180, 0x111);
            Room_Draw01(&p[6], 0x180, 0x111);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            Room_Draw01(&p[0], 0x180, 0x111);
            Room_Draw01(&p[2], 0x180, 0x111);
        } break;
    }
}
