#include "common.h"

#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_night_saloon_g_r.h"

extern u8  D_8007216C;
extern s16 D_80114D08;
extern u8  D_801153F4;

/// Saved `Mc_SaveData.field_4` (area id), restored when the cutscene ends.
extern u8 D_dryfield_night_saloon_g_r_80188FA4;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r", RoomsShared8017d878Table);

/// Room cutscene task: case 0 saves the area id, forces `Mc_SaveData.field_4`
/// to 0xC, raises the script halt flags and starts cap command 0x13; the
/// following states wait for the cap to go idle, then build the room's display
/// mode object, and case 4 restores the area id and kills the task.
void func_dryfield_night_saloon_g_r_8017DB74(Task* task)
{
    McSaveData* save;
    u8          temp;

    switch (task->state) {
        case 0:
            gGameSession->field_1                = 1;
            gGameSession->field_68               = 1;
            D_801153F4                           = 2;
            save                                 = &Mc_SaveData;
            temp                                 = save->field_4;
            save->field_4                        = 0xC;
            D_dryfield_night_saloon_g_r_80188FA4 = temp;
            Gp_MsgPlayer3F3(0);
            Gp_RunCapCmd(0x13, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            func_dryfield_night_saloon_g_r_8017E698(0);
            task->state = task->state + 1;
            return;
        case 3:
            task->state = task->state + 1;
            return;
        case 4:
            gGameSession->field_1  = 0;
            gGameSession->field_68 = 0;
            D_80114D08             = 0xA;
            D_801153F4             = 0;
            D_8007216C             = D_dryfield_night_saloon_g_r_80188FA4;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        default:
            return;
    }
    Task_Kill(task);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r", D_dryfield_night_saloon_g_r_8017D600);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r", RoomsShared8018055cCourses);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r", RoomsShared8017e28cTitle);
