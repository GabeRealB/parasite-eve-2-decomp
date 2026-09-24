#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

extern s8  D_8007216C;
extern s16 D_80114D08;

/// Exit state of the room's key-item event task, undoing its set-up
/// (`func_dryfield_breezeway_8017E464`): sends the two player messages with 1,
/// releases the display reference, clears the session's event, HUD and
/// cutscene holds, puts `D_8007216C` back from 6 to 4, kills the prompt task
/// the set-up spawned (`Task::spawnArg2`) and asks for its own removal.
void func_dryfield_breezeway_8017FE90(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_8007216C                 = 4;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    SOFT_BARRIER();
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}
