#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

extern s16 D_800691CA;

extern GpMsgEntry D_neo_ark_r31_8017D9F4[];
extern s32        D_80133F90;
extern s32        D_80134470;

s32 func_neo_ark_r31_8017D8FC(void)
{
    return 0;
}

s32 func_neo_ark_r31_8017D904(void)
{
    return 0;
}

void func_neo_ark_r31_8017D90C(Task* arg0)
{
    CdCmdQueue* queue;

    queue          = &CdCmd_Queue;
    arg0->msgTable = D_neo_ark_r31_8017D9F4;
    Game_SetPtrSlot(arg0, 7);
    queue->field_22A = 2;
    func_800E8634((s32)&D_80133F90, 0, (s32)&D_80134470);
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_r31_8017D980(void)
{
    D_800691CA = 2;
}
