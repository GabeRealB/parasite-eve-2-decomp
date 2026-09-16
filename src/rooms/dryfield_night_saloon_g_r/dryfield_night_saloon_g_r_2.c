#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_night_saloon_g_r.h"

/// Cutscene script blob argument of `func_800E8614`.
extern void       func_800E8614(s32 arg0, s32 arg1);
extern GpMsgEntry D_dryfield_night_saloon_g_r_8017F918[];
extern TaskDesc   D_dryfield_night_saloon_g_r_8017F940[];
extern s32        D_dryfield_night_saloon_g_r_801848DC;
extern s32        D_dryfield_night_saloon_g_r_80184B34;
extern s32        D_dryfield_night_saloon_g_r_80184D2C;

s32 func_dryfield_night_saloon_g_r_8017DD84(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_dryfield_night_saloon_g_r_8017F940, 0, 0, 0);
            break;
        case 8:
            if (GameFlag_GetNibble(0x5A) == 0) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_801848DC, 0);
                GameFlag_SetNibble(0x5A, 1);
            } else if (GameFlag_GetNibble(0x5A) == 1) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_80184B34, 0);
            }
            break;
        case 10:
            if (GameFlag_GetNibble(0x5A) < 2) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_80184D2C, 0);
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_2", func_dryfield_night_saloon_g_r_8017DE68);

/// Room entry task tick: park the room's hotspot table in `Task::field_24` -
/// the table whose 0x13EE entry is the room's own script task - register the
/// task in pointer slot 7, then, on the phase-2 visit whose nibble 0xB0 is
/// still clear, announce the room to the slot-4 task with message 0x7DA
/// carrying the session's two id bytes and a zero halfword. Then advance state.
void func_dryfield_night_saloon_g_r_8017DF90(Task* task)
{
    DnsgrMsg7DA msg;

    task->field_24 = D_dryfield_night_saloon_g_r_8017F918;
    Game_SetPtrSlot(task, 7);
    if (Game_Session->field_9 == 2 && GameFlag_GetNibble(0xB0) == 0) {
        msg.field_0 = Game_Session->field_7;
        msg.field_1 = Game_Session->field_6;
        msg.field_2 = 0;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    }
    task->state = task->state + 1;
}
