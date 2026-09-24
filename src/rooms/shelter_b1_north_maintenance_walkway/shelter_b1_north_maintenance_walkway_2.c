#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_north_maintenance_walkway.h"

extern GpMsgEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc   D_shelter_b1_north_maintenance_walkway_80184AAC[];

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184AB8[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B08[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B18[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B48[];

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0);

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec;
    GpSprtCmd* cmd;
    s32        mode;

    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd            = rec[2].field_4;
        cmd[2].field_4 = 1;
    } else if (mode == 1) {
        cmd            = rec[2].field_4;
        cmd[2].field_4 = 0;
    }
}

void func_shelter_b1_north_maintenance_walkway_8017DBC8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60247;
        D_80115744  = 0x60253;
        D_8011573C  = 0x6025E;
        D_80115720  = 0x6026A;
        D_80115758  = 0x601CB;
        D_8011572C  = 0x601E7;
        D_80115750  = 0x60203;
        D_80115734  = 0x6021F;
        D_80115730  = 0x6022A;
        D_80115754  = 0x60235;
        arg0->state = 1;
    }

    switch (gGameSession->at4.loc.view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B18;
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[0], 0x200, 0x800);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[4], 0x200, -0x400);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B08;
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[0], 0x200, 0x800);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[2], 0x200, 0x800);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[4], 0x200, -0x400);
            break;
        }
        case 4:
        case 6: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B48;
            func_shelter_b1_north_maintenance_walkway_8017E55C(&p[0], 0x200);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-18], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-16], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-14], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-12], 0x200, -0x400);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-10], 0x200, -0x400);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-8], 0x200, -0x400);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-6], 0x200, 0x800);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184AB8;
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[0], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[6], 0x200, -0x400);
            break;
        }
    }
}
