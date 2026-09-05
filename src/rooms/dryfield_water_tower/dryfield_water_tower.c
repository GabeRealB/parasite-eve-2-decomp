#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern u8 D_8007216C;
extern u8 D_801153F4;

extern u32   D_dryfield_water_tower_8018768C;
extern Task* D_dryfield_water_tower_801876A0;

void func_dryfield_water_tower_8017DCB4(void);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", D_dryfield_water_tower_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017D7D8);

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017DAF8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017DC64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017DCB4);

s32 func_dryfield_water_tower_8017DCFC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017DD04);

s32 func_dryfield_water_tower_8017DD3C(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017DD44);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", func_dryfield_water_tower_8017DD6C);

void func_dryfield_water_tower_8017DDD0(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", jtbl_dryfield_water_tower_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", jtbl_dryfield_water_tower_8017D618);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower", jtbl_dryfield_water_tower_8017D630);
