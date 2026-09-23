#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_b4_reservoir_801848BC[];
extern TaskDesc   D_shelter_b4_reservoir_80184920[];
extern Task*      D_shelter_b4_reservoir_80184930;
extern s16        D_shelter_b4_reservoir_80184F80;
extern TaskDesc   D_shelter_b4_reservoir_80184F84[];

extern void func_shelter_b4_reservoir_8017E068(void);
void        func_shelter_b4_reservoir_8017E8E4(void);

void func_shelter_b4_reservoir_8017E7C8(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_reservoir_801848BC;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b4_reservoir_80184F84, 0, 0, 0);
    if (GameFlag_GetNibble(0xB7) != 0) {
        D_shelter_b4_reservoir_80184F80 = -0x1F4;
    } else {
        D_shelter_b4_reservoir_80184F80 = -0x7D0;
    }
    D_shelter_b4_reservoir_80184930 = Task_SpawnFromTable(D_shelter_b4_reservoir_80184920, 0, 0, 0);
    arg0->state                     = (s32)(arg0->state + 1);
}

void func_shelter_b4_reservoir_8017E864(void)
{
    func_shelter_b4_reservoir_8017E068();
    func_shelter_b4_reservoir_8017E8E4();
}
