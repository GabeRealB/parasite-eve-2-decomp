#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// Current screen id at 0x8007218B; 9 skips the room script.
extern s8 D_8007218B;

/// Cutscene script blob argument of `func_800E8634`.
extern s32 D_neo_ark_r26_8017DA74;

/// Cutscene script blob argument of `func_800E8634`.
extern s32 D_neo_ark_r26_8017DFCC;

/// Room message handler table installed into `Task::field_24`.
extern GpMsgEntry D_neo_ark_r26_8017E0A4[];

s32 func_neo_ark_r26_8017D694(void)
{
    return 0;
}

s32 func_neo_ark_r26_8017D69C(void)
{
    return 0;
}

/// Room task state 0: installs the message table, claims pointer slot 7, then
/// starts the room script unless the screen is 9. Advances to state 1.
void func_neo_ark_r26_8017D6A4(Task* arg0)
{
    arg0->field_24 = D_neo_ark_r26_8017E0A4;
    Game_SetPtrSlot(arg0, 7);
    if (D_8007218B != 9) {
        func_800E8634((s32)&D_neo_ark_r26_8017DA74, 0, (s32)&D_neo_ark_r26_8017DFCC);
    }
    arg0->state = arg0->state + 1;
}
