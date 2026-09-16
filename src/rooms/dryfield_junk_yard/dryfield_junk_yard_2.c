#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_dryfield_junk_yard_8017DD48[];
extern s32      D_dryfield_junk_yard_8017E160;
extern s32      D_dryfield_junk_yard_8017E2B0;

INCLUDE_ASM("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard_2", func_dryfield_junk_yard_8017DA4C);

s32 func_dryfield_junk_yard_8017DB78(Task* task, s32 msgId, GpMsg13EF* msg)
{
    Task* player;

    if (msg->field_2 == 1) {
        if (GameFlag_GetNibble(0x38) == 0) {
            GameFlag_SetNibble(0x38, 1);
            Task_SpawnFromTable(D_dryfield_junk_yard_8017DD48, 0, 0, 0);
        }
    }
    if (msg->field_2 == 2) {
        player = Game_GetPtrSlot(0xA);
        if ((player != NULL) && (((TmdObject*)(player->extra))->field_8->coord.t[0] >= 0x5209) &&
            (GameFlag_GetNibble(0x38) == 1)) {
            GameFlag_SetNibble(0x38, 2);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E160, 0, (s32)&D_dryfield_junk_yard_8017E2B0);
        }
    }
    return 0;
}
