#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

extern s8 D_8007216C;

void func_dryfield_night_motel_lobby_8018122C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_8007216C             = 4;
    Task_RequestKill(arg0, 0);
}
