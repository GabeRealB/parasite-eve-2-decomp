#include "common.h"

#include "gameplay/D4.h"

#include "rooms/room_common.h"

#include "main/task.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_transfer_tunnel_801828E8[];
extern SVECTOR D_shelter_b1_transfer_tunnel_801828F8[];

void func_shelter_b1_transfer_tunnel_8017D6D0(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115728  = 0x60249;
        D_80115744  = 0x60255;
        D_8011573C  = 0x60260;
        D_80115720  = 0x6026C;
        D_80115758  = 0x601CE;
        D_8011572C  = 0x601EA;
        D_80115750  = 0x60206;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_transfer_tunnel_801828E8;
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw25(&p[8], 0x200);
        } break;
        case 3: {
            SVECTOR* p = D_shelter_b1_transfer_tunnel_801828E8;
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw11(&p[4], 0x200, 0x800);
            Room_Draw25(&p[8], 0x200);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b1_transfer_tunnel_801828F8;
            Room_Draw11(&p[0], 0x200, 0);
            Room_Draw11(&p[4], 0x200, 0x800);
        } break;
    }
}
