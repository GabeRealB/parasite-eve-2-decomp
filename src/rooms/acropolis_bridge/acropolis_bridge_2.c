#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_acropolis_bridge_80188E4C[];
extern TaskDesc   D_acropolis_bridge_80188E7C[];
extern Task*      D_acropolis_bridge_80191794;

void func_acropolis_bridge_8017F2D0(s32 arg0);

void func_acropolis_bridge_8017D98C(Task* arg0)
{
    arg0->field_24 = D_acropolis_bridge_80188E4C;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_bridge_80191794 = Task_SpawnFromTable(D_acropolis_bridge_80188E7C, 0, 0, 0);
    arg0->state                 = (s32)(arg0->state + 1);
    func_acropolis_bridge_8017F2D0(GameFlag_GetNibble(0x10) & 0xFF);
}
