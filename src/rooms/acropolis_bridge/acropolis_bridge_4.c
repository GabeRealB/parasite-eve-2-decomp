#include "common.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_bridge.h"

#include <psyq/libgs.h>

extern s8 D_8007216C;

extern s32   D_acropolis_bridge_801917A0;
extern Task* D_acropolis_bridge_80191798;

/// Per-frame state of the bridge model task: raises bit 0x80 of the object's
/// flags on camera views 8..10 and clears them elsewhere, then clears the root
/// coordinate's `flg` so its world matrix is rebuilt this frame.
void func_acropolis_bridge_8017DB08(Task* task)
{
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    extra = (TmdObject*)task->extra;
    coord = extra->field_8;
    if ((u32)(Gp_GetViewIndex() - 8) < 3U) {
        extra->field_C = 0x80;
    } else {
        extra->field_C = 0;
    }
    coord->flg = 0;
}

void func_acropolis_bridge_8017DB60(Task* arg0)
{
    Gp_StartCapSlot(7, 1, 1);
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_bridge_8017DBA0(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA3, 0, 0) == 0) {
        D_8007216C             = 8;
        Game_Session->field_68 = 1;
        Gp_MsgPlayer3F3(0);
        Gp_MsgPlayerWeapon(0);
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_acropolis_bridge_8017DC1C(Task* arg0)
{
    Task* temp_v0;

    temp_v0                     = Task_Spawn(2, 8, 0, 0);
    arg0->state                 = (s32)(arg0->state + 1);
    D_acropolis_bridge_80191798 = temp_v0;
}

void func_acropolis_bridge_8017DC68(Task* arg0)
{
    AcropolisBridgeMsg7DA msg = { 1, 0xB, 1 };

    if (Task_PollKill(D_acropolis_bridge_80191798, &D_acropolis_bridge_801917A0) != 0) {
        if (D_acropolis_bridge_801917A0 == 0) {
            D_8007216C             = 6;
            Game_Session->field_68 = 0;
            arg0->state            = (s32)(arg0->state + 1);
        } else {
            D_8007216C = 9;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            arg0->state = (s32)(arg0->state + 1);
        }
    }
}

void func_acropolis_bridge_8017DD24(Task* arg0)
{
    if (D_acropolis_bridge_801917A0 == 0) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Task_Kill(arg0);
        return;
    }
    Gp_MsgPlayer3F3(1);
    arg0->state += 1;
}
