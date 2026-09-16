#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_dryfield_night_parking_lot_8017ECB4;

s32 func_dryfield_night_parking_lot_8017DB34(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (Game_Session->field_9 == 3) && (GameFlag_GetNibble(0x79) == 0)) {
        GameFlag_SetNibble(0x79, 1);
        Gp_MsgPlayerWeapon(0);
        func_800E8614((s32)&D_dryfield_night_parking_lot_8017ECB4, 1);
    }
    return 0;
}
