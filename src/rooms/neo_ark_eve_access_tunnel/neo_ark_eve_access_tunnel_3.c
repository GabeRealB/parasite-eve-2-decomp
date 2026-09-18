#include "common.h"

#include "gameplay/D4.h"

#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

/// The room's own `GpMsgEntry[]` - the message table its cap scripts index
/// (ids 0x13EE-0x13F2, the shared room-message block). Terminated by the
/// `0x7FFFFFFF` id, and followed immediately by `TaskDesc
/// D_neo_ark_eve_access_tunnel_8017EAC4`.
extern GpMsgEntry D_neo_ark_eve_access_tunnel_8017EA94[];

void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1);

/// State 0 of the tunnel's message task: park the room's message table in
/// `Task::field_24` and publish the task in pointer slot 7, as every room-entry
/// task does. Then, once the session has reached state 0xB, set bit 15 of every
/// 16-bit half of the 0x25800-byte image buffer and latch `GameSession::field_69`
/// bit 0 - the flag that suppresses the bank-load spawn when the task ends.
void func_neo_ark_eve_access_tunnel_8017DF24(Task* arg0)
{
    arg0->field_24 = D_neo_ark_eve_access_tunnel_8017EA94;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->field_9 == 0xB) {
        u16* ptr = (u16*)Fs_ImgBuffers;
        s32  i   = 0;

        do {
            *ptr = (u16)(*ptr | 0x8000);
            i   += 1;
            ptr += 1;
        } while (i <= 0x12BFF);
        gGameSession->field_69 = 1;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_eve_access_tunnel_8017DFC0(void)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (gGameSession->field_9 < 4U) {
        func_neo_ark_eve_access_tunnel_8017E090(0, 0);
        func_neo_ark_eve_access_tunnel_8017E090(1, 0);
    }
    if (gGameSession->field_9 == 0xB) {
        queue->field_22A = 2;
    }
}
