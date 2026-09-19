#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_night_saloon_g_r.h"

/// Cutscene script blob argument of `func_800E8614`.
extern void       func_800E8614(s32 arg0, s32 arg1);
extern GpMsgEntry D_dryfield_night_saloon_g_r_8017F918[];
extern TaskDesc   D_dryfield_night_saloon_g_r_8017F940[];
extern s32        D_dryfield_night_saloon_g_r_801848DC;
extern s32        D_dryfield_night_saloon_g_r_80184B34;
extern s32        D_dryfield_night_saloon_g_r_80184D2C;
extern s32        D_dryfield_night_saloon_g_r_80183C94;
extern s32        D_dryfield_night_saloon_g_r_801847A4;
extern GpObj4A    D_dryfield_night_saloon_g_r_80188BB8;

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

/// Handler for this room's script entry 0x13EF, whose `GpMsg13EF` payload
/// arrives as `arg2`. `field_2 == 7` plays the room's first-visit cutscene
/// once (nibble 0x59). Then, in session phase 2 with nibble 0xB0 still clear,
/// `field_2 == 1` unlinks the room's 4A object and queues sound 0x5312000C,
/// while the room's own phase (`field_2 == 2`) announces the visit to the
/// slot-4 task with message 0x7DA carrying the session's two id bytes and a
/// non-zero action halfword, and sets nibble 0xB0. Always returns 0.
s32 func_dryfield_night_saloon_g_r_8017DE68(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    DnsgrMsg7DA msg;
    u8          temp_s0;

    if (arg2->field_2 == 7 && GameFlag_GetNibble(0x59) == 0) {
        func_800E8634((s32)&D_dryfield_night_saloon_g_r_80183C94, 0, (s32)&D_dryfield_night_saloon_g_r_801847A4);
        GameFlag_SetNibble(0x59, 1);
    }
    temp_s0 = gGameSession->at4.loc.place;
    if (temp_s0 == 2 && GameFlag_GetNibble(0xB0) == 0) {
        if (arg2->field_2 == 1) {
            Gp_UnlinkObj4A(0, &D_dryfield_night_saloon_g_r_80188BB8);
            SndEvt_EnqueueType6(0x5312000C, 0, 0);
        } else if (arg2->field_2 == temp_s0) {
            msg.field_0 = gGameSession->at4.loc.stage;
            msg.field_1 = gGameSession->at4.loc.area;
            msg.field_2 = 1;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            GameFlag_SetNibble(0xB0, 1);
        }
    }
    return 0;
}

/// Room entry task tick: park the room's hotspot table in `Task::msgTable` -
/// the table whose 0x13EE entry is the room's own script task - register the
/// task in pointer slot 7, then, on the phase-2 visit whose nibble 0xB0 is
/// still clear, announce the room to the slot-4 task with message 0x7DA
/// carrying the session's two id bytes and a zero halfword. Then advance state.
void func_dryfield_night_saloon_g_r_8017DF90(Task* task)
{
    DnsgrMsg7DA msg;

    task->msgTable = D_dryfield_night_saloon_g_r_8017F918;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 2 && GameFlag_GetNibble(0xB0) == 0) {
        msg.field_0 = gGameSession->at4.loc.stage;
        msg.field_1 = gGameSession->at4.loc.area;
        msg.field_2 = 0;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    }
    task->state = task->state + 1;
}
