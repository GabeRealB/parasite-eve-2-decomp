#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

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

/* func_dryfield_water_tower_8017DAF8 is the room's message handler; it is the
   shared body RoomsShared8017daf8 (src/rooms/lib/, see its header), linked
   into both water tower rooms from the `shared` span at 0x538. */

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", RoomsShared8017d878Table);
