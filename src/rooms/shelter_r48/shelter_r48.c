#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc D_8014B958;

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48", func_shelter_r48_8017D660);

s32 func_shelter_r48_8017DF50(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;
    s32      ret;

    ret = 0;
    if (arg2 == 0x121 || arg2 == 0x122) {
        if (GameFlag_GetNibble(0x100) == 1) {
            node  = Gp_PendingObj4C;
            found = 1;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                gGameSession->eventState = 1;
                D_80115768               = 1;
                Task_SpawnOnDefaultList(&D_8014B958, 0, 0, 0);
                GameFlag_SetNibble(0x100, 2);
                GameFlag_SetNibble(0x12A, 4);
                ret = 1;
            }
        }
    }
    return ret;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_r48/shelter_r48", RoomsShared8017d878Table);
