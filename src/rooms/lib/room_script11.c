#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

extern s8  D_8007216C;
extern s16 D_80114D08;

void Room_Script11(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_8007216C             = 4;
    /* Without the barrier GCC fills Task_Kill's delay slot with the byte store. */
    SOFT_BARRIER();
    Task_Kill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}
