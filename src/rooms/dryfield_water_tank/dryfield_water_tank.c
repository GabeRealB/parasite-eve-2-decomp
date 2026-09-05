#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern u8 D_8007216C;
extern u8 D_801153F4;

extern TaskDesc D_dryfield_water_tank_8017FF88;
extern s32      D_dryfield_water_tank_80188D48;

void func_dryfield_water_tank_8017DB48(void);

void func_dryfield_water_tank_8017D618(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x55) == 3) {
                Gp_StartCapSlot(0xE, 1, 1);
                break;
            }
            Game_Session->field_1          = 1;
            D_dryfield_water_tank_80188D48 = D_8007216C;
            Gp_MsgPlayer3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(0xE, 0, 0);
            arg0->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                D_801153F4 = 2;
                SOFT_BARRIER();
                task->state = task->state + 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xA) {
                D_801153F4 = 0;
                GameFlag_SetNibble(0x55, 3);
                SndEvt_EnqueueType6(0x52150004, 0, 0);
                Task_SpawnFromTable(&D_dryfield_water_tank_8017FF88, 0, 0, 0);
                func_dryfield_water_tank_8017DB48();
            } else {
                Game_Session->field_1  = 0;
                Game_Session->field_68 = 0;
                D_801153F4             = 0;
                D_8007216C             = (u8)D_dryfield_water_tank_80188D48;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
            }
            break;
        default:
            return;
    }
    Task_Kill(task);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", D_dryfield_water_tank_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank", RoomsShared8017d878Table);
