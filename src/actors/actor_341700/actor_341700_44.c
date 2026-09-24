#include "common.h"

#include "main/fs.h"
#include "main/session.h"

#include "actors/actor_341700.h"

extern s8 D_80115415; // absolute; set once CD command 0x21 is queued

/// Queues CD command 0x21 once, guarded by `D_80115415`: the first parameter
/// is 2 or 3 in place 1 or 2 of stage 4 areas 0x27/0x28 and 1 everywhere
/// else.
void func_actor_341700_80168004(void)
{
    u8 param1[8];
    u8 param2[8];

    if (D_80115415 == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (gGameSession->at4.loc.stage == 4 && (u32)(gGameSession->at4.loc.area - 0x27) < 2 && gGameSession->at4.loc.place == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else if (gGameSession->at4.loc.stage == 4 && (u32)(gGameSession->at4.loc.area - 0x27) < 2 && gGameSession->at4.loc.place == 2) {
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
