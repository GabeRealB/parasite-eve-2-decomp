#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern GpMsgEntry D_neo_ark_bridge_80181F30[];

s32 func_neo_ark_bridge_8017E878(void)
{
    return 0;
}

s32 func_neo_ark_bridge_8017E880(void)
{
    return 0;
}

/// Room entry task tick: installs the room's message table (ids `0x13EE`-`0x13F1`),
/// hands the task to pointer slot 7, queues sound events `0x551B0003` and
/// `0x551B0004`, then advances state.
void func_neo_ark_bridge_8017E888(Task* arg0)
{
    arg0->field_24 = D_neo_ark_bridge_80181F30;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x551B0003, 0, 0);
    SndEvt_EnqueueType6(0x551B0004, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_bridge_8017E8F4(void)
{
}
