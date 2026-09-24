#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/dryfield_night_water_hole.h"

extern GpMsgEntry        D_dryfield_night_water_hole_801805F8[];
extern s32               D_dryfield_night_water_hole_8018065C;
extern s32               D_dryfield_night_water_hole_80180660;
extern TaskDesc          D_dryfield_night_water_hole_80180964[];
extern DnwhParamOverride D_dryfield_night_water_hole_801835D8[];
extern TaskDesc          D_801351FC[];
extern s16               D_80071076;

/// Answers the code in `in->field_0` in `out->field_3`, unless `in->field_5`
/// is set. Six codes have an answer, each from a progress nibble: 2 is 2 once
/// nibble 0x10F is set and 3 once nibble 0x11A reaches 2; 5, 41 and 45 are
/// nibbles 0xA4, 0xB6 and 0xB7 plus one; 16 is 3 once nibble 0x7A reaches 6;
/// and 20 maps nibble 0xF4's values 0-3 to 1, 6, 7 and 8 (1 otherwise). Every
/// other code leaves `out` untouched. Always returns 1.
s32 func_dryfield_night_water_hole_8017D6AC(DnwhUtilParam* in, DnwhUtilParam* out)
{
    if (in->field_5 == 0) {
        switch (in->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    out->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    out->field_3 = 3;
                }
                break;
            case 5:
                out->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    out->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        out->field_3 = 1;
                        break;
                    case 1:
                        out->field_3 = 6;
                        break;
                    case 2:
                        out->field_3 = 7;
                        break;
                    case 3:
                        out->field_3 = 8;
                        break;
                    default:
                        out->field_3 = 1;
                        break;
                }
                break;
            case 45:
                out->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                out->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// The room's event task, run on the descriptor staged in
/// `D_dryfield_night_water_hole_80183630`. State 0 sends the
/// descriptor's `field_4` to the slot-3 game pointer as message 0x3EE, skipping
/// to state 2 when it is 0xFFFF; state 1 waits until that pointer answers 0x3F0
/// with 0. States 2 and 3 play the sound event `field_8`, if any, and wait for
/// its voice to go quiet. State 4 queues type-7 sound event 0x80000000, commits
/// the save location in the descriptor's first four bytes (stage, area, warp,
/// room), re-spawns the player task as type 0x11 and kills itself.
void func_dryfield_night_water_hole_8017D7E8(Task* arg0)
{
    GpMsg3EE msg;
    void*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_dryfield_night_water_hole_80183630.field_4;
            if (msg.field_12 == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_dryfield_night_water_hole_80183630.field_8 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_dryfield_night_water_hole_80183630.field_8, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_dryfield_night_water_hole_80183630.field_8) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_dryfield_night_water_hole_80183630.field_0;
            Mc_SaveData.at4.loc.area  = D_dryfield_night_water_hole_80183630.field_1;
            Mc_SaveData.at4.loc.warp  = D_dryfield_night_water_hole_80183630.field_2;
            Mc_SaveData.at4.loc.room  = D_dryfield_night_water_hole_80183630.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room's message table, the `GpMsgEntry` list the room task publishes in
/// `Task::msgTable` for `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0, whose handler is `func_dryfield_night_water_hole_8017DC28`.
extern GpMsgEntry D_dryfield_night_water_hole_801805F8[];
/// The two four-byte records this room hands the slot-4 task as the message
/// 0x7DB payload, picked by `gGameSession::at4.loc.warp`. They are the last two of
/// the four-record run at 0x80180654, which differ only in the halfword at 0x2.
extern s32 D_dryfield_night_water_hole_8018065C;
extern s32 D_dryfield_night_water_hole_80180660;
/// Descriptor of the room's event task, spawned while progress nibble 0xB8 is
/// still clear. Its callback is `func_dryfield_night_water_hole_8017E630`.
extern TaskDesc D_dryfield_night_water_hole_80180964[];
/// Override list applied once nibble 0xB8 is set.
extern DnwhParamOverride D_dryfield_night_water_hole_801835D8[];
/// Resident task table the ending task is spawned from, descriptor 1.
extern TaskDesc D_801351FC[];

/// Room entry task tick: publish the message table above in `Task::msgTable`
/// and claim game pointer slot 7. Progress nibble 0xB8 then picks the opening
/// move: while it is clear the room's event task is spawned from
/// `D_dryfield_night_water_hole_80180964`, and once it is set the parameter
/// overrides are applied instead.
///
/// On the visit whose sub-id (`gGameSession::at4.loc.place`) is 1 and that has
/// already latched nibble 0x95, and with the slot-4 task present, the room
/// announces itself to it with message 0x7DB, carrying the payload record
/// `gGameSession::at4.loc.warp` selects. On sub-id 0xA, with pointer slot 0xA
/// filled and nibble 0xCF still clear, it latches 0xCF, arms
/// `func_800E3FAC(0xA2, 0x25)` and spawns the ending task. Then advances state.
void func_dryfield_night_water_hole_8017D958(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_water_hole_801805F8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0xB8) == 0) {
        Task_SpawnFromTable(D_dryfield_night_water_hole_80180964, 0, 0, 0);
    } else {
        func_dryfield_night_water_hole_8017DE88(D_dryfield_night_water_hole_801835D8);
    }
    if (gGameSession->at4.loc.place == 1 && Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x95) != 0) {
        if (gGameSession->at4.loc.warp == 2) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_water_hole_80180660, 0);
        } else {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_water_hole_8018065C, 0);
        }
    }
    if (gGameSession->at4.loc.place == 0xA && gameGetPtrSlot(0xA) != 0 && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0xCF, 2);
        func_800E3FAC(0xA2, 0x25);
        Task_SpawnFromTable(D_801351FC, 1, 0, 0);
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole", D_dryfield_night_water_hole_8017D688);
