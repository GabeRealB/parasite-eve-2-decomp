#include "common.h"

#include "gameplay/3A34.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern u8       D_8007216D;
extern TaskDesc D_dryfield_driveway_8017E2FC;

s32 func_dryfield_driveway_8017DCC0(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x114) {
        if (GameFlag_GetNibble(0x3A) == 1) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                GameFlag_SetNibble(0x3A, 2);
                Task_SpawnOnDefaultList(&D_dryfield_driveway_8017E2FC, 0, 0, 0);
                gGameSession->at4.loc.room = (D_8007216D = 2);
                gGameSession->hideHud      = 1;
                gGameSession->eventState   = 1;
                return 1;
            }
        }
    }
    return 0;
}

s32 func_dryfield_driveway_8017DDB0(void)
{
    return 0;
}

s32 func_dryfield_driveway_8017DDB8(void)
{
    return 0;
}
