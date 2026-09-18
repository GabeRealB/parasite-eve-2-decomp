#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern TaskDesc D_dryfield_general_store_8017E164;

/// The two caption flags the store's cutscene task raises: `D_801153F4` while
/// the script owns the screen and `D_80115690` when CAP command 0xF showed the
/// letterbox. `D_8007216C` is the area-record id the same task publishes, and
/// `D_80071076` the "chapter advanced" halfword it sets on the way out.
extern u8  D_801153F4;
extern u8  D_80115690;
extern u8  D_8007216C;
extern s16 D_80071076;

/// The stage byte `Mc_SaveData.at4.loc.view` held when the cutscene began, saved by
/// `func_dryfield_general_store_8017DAC0`'s first state and restored into
/// `D_8007216C` when the cutscene is cut short.
extern u8 D_dryfield_general_store_801856F8;

/// The two script arguments, latched from the message that armed the cutscene
/// task `D_dryfield_general_store_8017E164`; the task itself reads them back to
/// place its actors.
extern u8 D_dryfield_general_store_80185709;
extern u8 D_dryfield_general_store_8018570A;

/// The 4-byte record `func_dryfield_general_store_8017DAC0` hands the helper
/// task 0x31 when the script's CAP event key asks for it.
extern GpStateBD8 D_dryfield_general_store_801856FC;

/// Handler for the store's two event ids. Both answer with a furniture-style
/// "which variant" byte in `out->field_3`, and a non-zero `field_5` asks what
/// would happen without the side effects.
///
/// Message 1 is the grandfather clock: with nibble 0x63 clear the reply is the
/// id itself, otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is still below
/// 4. The final arm offers the gate a request that plays the two stage sounds
/// 0x5203000C / 0x52030003 under flag nibble 0x3B.
///
/// Message 0x26 is the shop till: with nibble 0xC9 set the reply is 2, or 1
/// while nibble 0x53 is clear, plus 2 more while nibble 0x51 is clear;
/// otherwise 5, or 6 while nibble 0x51 is clear. The arm that is not asking
/// latches `field_2` / `field_3` for the spawned task and answers 2, or runs
/// CAP command 0xE when nibble 0x62 is set. Anything else answers 1.
s32 func_dryfield_general_store_8017D8D4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
        return RoomsShared8017d638(&req, in);
    }
    if (in->msgId != 0x26) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(&D_dryfield_general_store_8017E164, 1, 0, 0);
        D_dryfield_general_store_80185709 = in->field_2;
        D_dryfield_general_store_8018570A = in->field_3;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}

/// The store's cutscene task, the one the shop-till arm of
/// `func_dryfield_general_store_8017D8D4` spawns. It runs as a state script:
/// states 0 and 2 arm the cutscene and states 1 / 3 are the idle steps that
/// wait for CAP command 0xF to finish.
///
/// State 0 silences the player's weapon messages and latches the save's stage
/// byte into `D_dryfield_general_store_801856F8` before forcing that byte to
/// 0x10, the stage the cutscene belongs to. State 2 queues stage sound
/// 0x5203000D, hands CAP command 0xF the screen and raises `D_801153F4` /
/// `D_80115690` with it.
///
/// State 4 is the exit test. CAP event key 0xB means the script asked for the
/// helper task 0x31, which it spawns with a zeroed `GpStateBD8` record whose
/// `field_2` selects variant 8; any other key cuts the cutscene short instead -
/// captions off, stage sound 0x5203000E, the latched stage byte back into
/// `D_8007216C` and the player's weapon messages re-enabled.
///
/// State 5 is the commit: it queues sound event 0x80000000, moves the save to
/// chapter 0x26, raises `D_80071076`, latches the two script arguments into
/// `Mc_SaveData.at4.loc.warp` / `field_5` and spawns helper task 0x11.
///
/// Every arm that is finished with the task, state 5's and the cut-short arm of
/// state 4's, leaves through the shared `Task_Kill` below the switch.
void func_dryfield_general_store_8017DAC0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_general_store_801856F8 = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view          = 0x10;
            arg0->state                      += 1;
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
                D_dryfield_general_store_801856FC.field_0 = 0;
                D_dryfield_general_store_801856FC.field_1 = 0;
                D_dryfield_general_store_801856FC.field_2 = 8;
                Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_general_store_801856FC);
                arg0->state += 1;
                return;
            }
            D_801153F4 = 0;
            Gp_EnqueueStageSnd6(0x5203000E, 0, 0);
            D_8007216C = D_dryfield_general_store_801856F8;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x26;
            Mc_SaveData.at4.loc.warp = D_dryfield_general_store_80185709;
            Mc_SaveData.at4.loc.room = D_dryfield_general_store_8018570A;
            D_80071076               = 1;
            Task_Spawn(0, 0x11, 0, 0);
            break;
        default:
            return;
    }
    Task_Kill(arg0);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_general_store/dryfield_general_store", RoomsShared8017d878Table);
