#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

extern u8 D_8007216C;
extern u8 D_801153F4;

extern u32   D_dryfield_water_tower_8018768C;
extern Task* D_dryfield_water_tower_801876A0;

extern s32      D_dryfield_water_tower_801803A0[];
extern TaskDesc D_dryfield_water_tower_801803D8;
extern TaskDesc D_dryfield_water_tower_80182384[];

void func_dryfield_water_tower_8017DCB4(void);
void func_dryfield_water_tower_801802D8(u8 arg0);

void func_dryfield_water_tower_8017D948(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0x55) < 2) {
                func_dryfield_water_tower_8017DCB4();
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_RunCapCmd(7, 0);
                Game_Session->field_1           = 1;
                D_dryfield_water_tower_8018768C = D_8007216C;
                arg0->state                     = arg0->state + 1;
                return;
            }
            Gp_RunCapCmd1(7);
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                D_801153F4 = 2;
                /* keeps the `lw state` behind the `sb` instead of filling its load delay */
                SOFT_BARRIER();
                arg0->state = arg0->state + 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xA) {
                GameFlag_SetNibble(0x55, 2);
                func_dryfield_water_tower_8017DCB4();
                D_801153F4 = 0;
                Gp_DispatchMsg(D_dryfield_water_tower_801876A0, 0x13EC, 0, 0);
                SndEvt_EnqueueType6(0x52140009, 0, 0);
            } else {
                Game_Session->field_1  = 0;
                Game_Session->field_68 = 0;
                D_801153F4             = 0;
                D_8007216C             = D_dryfield_water_tower_8018768C;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
            }
            break;
        default:
            return;
    }
    Task_Kill(arg0);
}

/// The room's message handler, registered in the `(msgId, handler)` table the
/// script dispatches through. Message 0x13 (the player reaching the tower) is
/// the room's event: it builds the gate request for the tower's scene and
/// hands the gate the *incoming* message rather than the copy, so the answer
/// it returns is the gate's, with 0 (no prerequisite met) reported as 2.
/// Message 0x15 is the tower's own event and answers whether the scene it
/// guards is still due. Any other message just runs the 0x55 band's first-half
/// arming and reports 1.
s32 func_dryfield_water_tower_8017DAF8(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *msg;
    if (msg->msgId == 0x13) {
        req.field_0 = 0xA;
        req.field_4 = 6;
        req.field_8 = Gp_PackStageSndId(0x5214000E);
        req.field_C = Gp_PackStageSndId(0x52140003);
        req.flagId  = 0x34;
        req.itemId  = 0x10;
        ret         = RoomsShared8017d638(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (RoomsShared8017d638Flag != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
        return ret;
    }
    if (msg->field_5 == 0 && GameFlag_GetNibble(0x55) == 2) {
        GameFlag_SetNibble(0x55, 1);
    }
    if (msg->msgId == 0x15) {
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x4B) == 7) {
            GameFlag_SetNibble(0x4B, 0);
        }
        if (Game_Session->field_7 == 3) {
            return 1;
        }
        if (GameFlag_GetNibble(0x32) != 2) {
            return 0;
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", jtbl_dryfield_water_tower_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", jtbl_dryfield_water_tower_8017D618);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", jtbl_dryfield_water_tower_8017D630);
