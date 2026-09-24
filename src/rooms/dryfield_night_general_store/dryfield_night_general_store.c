#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern u8  D_801153F4;
extern u8  D_80115690;
extern u8  D_8007216C;
extern s16 D_80071076;

/// The room's two spawnable tasks: entry 0 the CAP-command task
/// `func_dryfield_night_general_store_8017DCA8`, entry 1 the cutscene task
/// `func_dryfield_night_general_store_8017DAF0`.
extern TaskDesc D_dryfield_night_general_store_8017E798[];

/// The save's stage byte as it was when the cutscene began, restored into
/// `D_8007216C` when the cutscene is cut short.
extern u8 D_dryfield_night_general_store_801858B4;

/// The record handed to helper task 0x31 when the cutscene asks for it.
extern GpStateBD8 D_dryfield_night_general_store_801858B8;

/// The warp point and room the cutscene task commits to, latched from the
/// message that spawned it.
extern u8 D_dryfield_night_general_store_801858C5;
extern u8 D_dryfield_night_general_store_801858C6;

s32 func_dryfield_night_general_store_8017D630(RoomEventReq* req, RoomEventMsg* msg);

/// Handler for the room's event ids 1 and 0x26. Both write a reply byte into
/// `out->field_3` unless `in->field_5` asks for a dry run.
///
/// For id 1 the reply is the id itself while flag nibble 0x63 is clear;
/// otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is below 4. It then
/// offers the event gate a request that plays stage sounds 0x5203000C and
/// 0x52030003 under flag nibble 0x3B, and returns the gate's answer.
///
/// For id 0x26 the reply is 1 (2 with nibble 0x53 set), plus 2 while nibble
/// 0x51 is clear, when nibble 0xC9 is set; otherwise 5, or 6 while nibble 0x51
/// is clear. Outside a dry run it spawns the cutscene task with `field_2` /
/// `field_3` latched as its destination, or runs CAP command 0xE once nibble
/// 0x62 is set, and answers 2. Any other id answers 1.
s32 func_dryfield_night_general_store_8017D904(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;
    s32          v;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 1 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->field_3 = msgId;
        } else {
            if (GameFlag_GetNibble(0x7A) < 4) {
                v = 4;
                TOUCH_REG(v);
                v = GameFlag_GetNibble(0x61) + 2;
            } else {
                v = 4;
            }
            out->field_3 = v;
        }
    }
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) == 0) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 = out->field_3 + 2;
            }
        } else if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 6;
        } else {
            out->field_3 = 5;
        }
    }
    if (in->msgId == 1) {
        req.field_0 = 0xD;
        req.field_4 = 0xD;
        req.field_8 = Gp_PackStageSndId(0x5203000C);
        req.field_C = Gp_PackStageSndId(0x52030003);
        req.flagId  = 0x3B;
        req.itemId  = 0;
        return func_dryfield_night_general_store_8017D630(&req, in);
    }
    if (in->msgId != 0x26) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(D_dryfield_night_general_store_8017E798, 1, 0, 0);
        D_dryfield_night_general_store_801858C5 = in->field_2;
        D_dryfield_night_general_store_801858C6 = in->field_3;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}

/// The room's cutscene task, a six-state script. State 0 silences the
/// player's weapon messages, saves the stage byte `Mc_SaveData.at4.loc.view`
/// and forces it to 0x10; states 1 and 3 each let one frame pass. State 2
/// queues stage sound 0x5203000D, runs CAP command 0xF and raises
/// `D_801153F4` / `D_80115690`.
///
/// State 4 checks the CAP event key: 0xB spawns helper task 0x31 with a
/// zeroed record whose `field_2` is 8 and moves on; any other key ends the
/// cutscene - `D_801153F4` cleared, stage sound 0x5203000E, the saved stage
/// byte written to `D_8007216C` and the weapon messages re-enabled. State 5
/// queues sound event 0x80000000, points the save's location at area 0x26
/// with the latched warp point and room, raises `D_80071076` and spawns
/// helper task 0x11. Both finishing arms kill the task.
void func_dryfield_night_general_store_8017DAF0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_night_general_store_801858B4 = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view                = 0x10;
            arg0->state                            += 1;
            return;
        case 1:
        case 3:
            arg0->state += 1;
            return;
        case 2:
            Gp_EnqueueStageSnd6(0x5203000D, 0, 0);
            D_801153F4 = 1;
            Gp_RunCapCmd1(0xF);
            D_80115690   = 1;
            arg0->state += 1;
            return;
        case 4:
            if (Gp_GetCapEventKey() == 0xB) {
                D_dryfield_night_general_store_801858B8.field_0 = 0;
                D_dryfield_night_general_store_801858B8.field_1 = 0;
                D_dryfield_night_general_store_801858B8.field_2 = 8;
                Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_night_general_store_801858B8);
                arg0->state += 1;
                return;
            }
            D_801153F4 = 0;
            Gp_EnqueueStageSnd6(0x5203000E, 0, 0);
            D_8007216C = D_dryfield_night_general_store_801858B4;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x26;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_general_store_801858C5;
            Mc_SaveData.at4.loc.room = D_dryfield_night_general_store_801858C6;
            D_80071076               = 1;
            Task_Spawn(0, 0x11, 0, 0);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_general_store/dryfield_night_general_store", D_dryfield_night_general_store_8017D5F4);
