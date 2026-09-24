#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc   D_shelter_b1_north_maintenance_walkway_80184AAC[];

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway_2", func_shelter_b1_north_maintenance_walkway_8017DBC8);
