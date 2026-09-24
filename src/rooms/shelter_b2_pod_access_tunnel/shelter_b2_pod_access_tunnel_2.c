#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b2_pod_access_tunnel_80183C08[];
extern SVECTOR D_shelter_b2_pod_access_tunnel_80183C48[];
extern SVECTOR D_shelter_b2_pod_access_tunnel_80183CC8[];

/// On the task's first tick stores seven room-specific values into resident
/// gameplay globals, then draws the `Room_Draw11` placements the current camera
/// view shows.
void func_shelter_b2_pod_access_tunnel_8017DC6C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024C;
        D_80115744  = 0x60258;
        D_8011573C  = 0x60263;
        D_80115720  = 0x6026F;
        D_80115758  = 0x601D5;
        D_8011572C  = 0x601F1;
        D_80115750  = 0x6020D;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183C08;
            Room_Draw11(&p[0], 0x200, 0x400);
            Room_Draw11(&p[2], 0x200, 0x400);
            Room_Draw11(&p[4], 0x200, 0x400);
            Room_Draw11(&p[6], 0x200, 0x400);
            Room_Draw11(&p[12], 0x200, 0);
            Room_Draw11(&p[14], 0x200, 0);
            Room_Draw11(&p[16], 0x200, 0);
            Room_Draw11(&p[18], 0x200, 0);
            Room_Draw11(&p[28], 0x200, 0x400);
            Room_Draw11(&p[30], 0x200, 0x400);
            Room_Draw11(&p[32], 0x200, 0x400);
            Room_Draw11(&p[34], 0x200, 0x400);
            Room_Draw11(&p[40], 0x200, 0);
            Room_Draw11(&p[42], 0x200, 0);
            Room_Draw11(&p[44], 0x200, 0);
            Room_Draw11(&p[46], 0x200, 0);
            break;
        }
        case 3:
        case 6: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183C48;
            Room_Draw11(&p[0], 0x200, -0x400);
            Room_Draw11(&p[2], 0x200, -0x400);
            Room_Draw11(&p[10], 0x200, 0);
            Room_Draw11(&p[12], 0x200, 0);
            Room_Draw11(&p[14], 0x200, 0x800);
            Room_Draw11(&p[16], 0x200, 0x800);
            Room_Draw11(&p[28], 0x200, -0x400);
            Room_Draw11(&p[30], 0x200, -0x400);
            Room_Draw11(&p[38], 0x200, 0);
            Room_Draw11(&p[40], 0x200, 0);
            Room_Draw11(&p[42], 0x200, 0x800);
            Room_Draw11(&p[44], 0x200, 0x800);
            break;
        }
        case 4:
        case 7: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183CC8;
            Room_Draw11(&p[0], 0x200, -0x400);
            Room_Draw11(&p[2], 0x200, -0x400);
            Room_Draw11(&p[28], 0x200, -0x400);
            Room_Draw11(&p[30], 0x200, -0x400);
            break;
        }
    }
}
