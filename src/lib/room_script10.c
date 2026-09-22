#include "common.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/gameflag.h"
#include "rooms/room_common.h"
#include "gameplay/D4.h"

extern s8  D_8007216C;
extern s16 D_80114D08;

extern s8 D_8007216C;

/// Work block parked at `Task::work` by the factory rooms that link this
/// body. `field_A` is the one-shot trigger `Room_Util34` raises; this script
/// consumes it, parks `field_8` at 0xA, and steps the caller to state 2.
typedef struct RoomScript12Work {
    /* 0x0 */ byte pad_0[8];
    /* 0x8 */ s16  field_8;
    /* 0xA */ s16  field_A;
} RoomScript12Work;

void Room_Script10(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgAlly3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_8007216C                 = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    SOFT_BARRIER();
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

void Room_Script12(Task* task)
{
    RoomActionPrompt* prompt;
    RoomScript12Work* work;

    prompt           = &D_80114D28;
    work             = (RoomScript12Work*)task->work;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            D_8007216C = 0xC;
        } else {
            D_8007216C = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}

/// Sets the skip-link byte on the second sprite command of view 11 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.loc.stage` is 2.
void Room_Util17(s32 arg0)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}
