#include "common.h"

#include "gameplay/D4.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_1f_guardroom_8017DA30[];

void func_shelter_1f_guardroom_8017D9CC(s32);

void func_shelter_1f_guardroom_8017D8D8(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    switch (arg0->state) {
        case 0:
            func_shelter_1f_guardroom_8017D9CC(0);
            queue->field_1EA = 1;
            slotParam[0]     = Stream_FindSlot(gGameSession->at4.raw.data, 0, 0);
            CdCmd_Enqueue(0x61, 0, slotParam);
            arg0->state++;
            break;
        case 1:
            if (queue->field_1FA != 0) {
                arg0->state = 2;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Task_RequestKill(arg0, 0);
            }
            break;
    }
}

void func_shelter_1f_guardroom_8017D9CC(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][2].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 1;
    } else {
        cmd[1].field_4 = 0;
    }
}

void func_shelter_1f_guardroom_8017DA28(void)
{
}
