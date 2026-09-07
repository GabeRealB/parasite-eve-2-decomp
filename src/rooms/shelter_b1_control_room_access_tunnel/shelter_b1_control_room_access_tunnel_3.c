#include "common.h"

#include "gameplay/D4.h"

#include "rooms/room_common.h"

#include "main/task.h"

extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_control_room_access_tunnel_80181E9C[];
extern SVECTOR D_shelter_b1_control_room_access_tunnel_80181EAC[];

void func_shelter_b1_control_room_access_tunnel_8017E1BC(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601CF;
        D_8011572C  = 0x601EB;
        D_80115750  = 0x60207;
        D_80115734  = 0x60279;
        D_80115730  = 0x6027A;
        D_80115754  = 0x6027B;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_control_room_access_tunnel_80181E9C;
            Room_Draw11(&p[0], 0x200, 0x400);
            Room_Draw11(&p[4], 0x200, 0x400);
            Room_Draw25(&p[8], 0x200);
        } break;
        case 3: {
            SVECTOR* p = D_shelter_b1_control_room_access_tunnel_80181EAC;
            Room_Draw11(&p[0], 0x200, -0x400);
            Room_Draw11(&p[4], 0x200, -0x400);
        } break;
    }
}
