#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80062735;

/// The passage's message table, which the room task answers messages with.
extern GpMsgEntry D_mine_secret_passage_80180E8C[];

/// Set-up state of the room task `func_mine_secret_passage_8017D970` drives:
/// points the task at the passage's message table, publishes it in pointer
/// slot 7, sets the `D_80062735` mode byte and advances to the next state.
void func_mine_secret_passage_8017D8C8(Task* arg0)
{
    arg0->msgTable = &D_mine_secret_passage_80180E8C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80062735  = 1;
}
