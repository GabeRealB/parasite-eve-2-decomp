#include "common.h"

#include "main/fs.h"
#include "main/session.h"

#include "actors/actors_shared_801692e8.h"

void ActorsShared801692e8(void)
{
    u8 param1[8];
    u8 param2[8];

    if (D_80115415 == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (Game_Session->field_7 == 4 && (u32)(Game_Session->field_6 - 0x27) < 2 && Game_Session->field_9 == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else if (Game_Session->field_7 == 4 && (u32)(Game_Session->field_6 - 0x27) < 2 && Game_Session->field_9 == 2) {
            param1[2] = 0xA;
            param1[0] = 3;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else {
            param1[2] = 0xA;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        }
        D_80115415 = 1;
    }
}
