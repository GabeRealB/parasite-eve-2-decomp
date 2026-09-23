#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/rooms_shared_8017ecb4.h"
#include "rooms/shelter_b1_underground_parking.h"

extern u8          D_shelter_b1_underground_parking_8018D788;
extern u8          D_shelter_b1_underground_parking_8018D789;
extern u8          D_8007216C;
extern s16         D_80114D08;
extern TaskDesc    D_shelter_b1_underground_parking_80187664[];
extern RoomHotspot D_shelter_b1_underground_parking_8018767C[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_10", func_shelter_b1_underground_parking_80184284);

void func_shelter_b1_underground_parking_80184304(Task* task)
{
    SbupExamineWork* st;
    RoomHotspot*     hs;

    st = memCalloc(0x10, 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_shelter_b1_underground_parking_80187664, 0, 1, 0);
    task->work      = (TaskIdMap*)st;
    D_8007216C      = 0x15;
    /* The once-loops fold away, but flow weights the references inside them
       by loop depth. The outer one keeps the state load below the mode store;
       the inner one lifts the work pointer's global-alloc priority back above
       the parameter's so the two keep their callee-saved homes. */
    do {
        task->state++;
        do {
            st->field_0 = 0;
        } while (0);
    } while (0);
    Display_AcquireRef();
    for (hs = D_shelter_b1_underground_parking_8018767C; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

void func_shelter_b1_underground_parking_801843F0(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    Gp_RunCapCmd(GameFlag_GetNibble(0xE7) == 0 ? 2 : 3, 0);
    task->state++;
}

void func_shelter_b1_underground_parking_80184468(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    RoomHotspot*      hs     = D_shelter_b1_underground_parking_8018767C;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    gGameSession->hideHud = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_C    = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

void func_shelter_b1_underground_parking_80184594(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

void func_shelter_b1_underground_parking_801845F8(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        if (work->field_C == 0x10) {
            SndEvt_EnqueueType6(0x54140004, 0, 0);
            if (D_shelter_b1_underground_parking_8018D788 != D_shelter_b1_underground_parking_8018D789) {
                if (gGameSession->at4.loc.room == 1) {
                    SndEvt_EnqueueType6(0x54140006, 0, 0);
                } else {
                    SndEvt_EnqueueType6(0x54140005, 0, 0);
                }
                task->state = 6;
                return;
            }
        } else {
            D_shelter_b1_underground_parking_8018D789 ^= work->field_C;
            SndEvt_EnqueueType6(0x54140004, 0, 0);
            task->state = 2;
            return;
        }
    }
    task->state = 2;
}

void func_shelter_b1_underground_parking_801846EC(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_8007216C                 = 2;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    SOFT_BARRIER();
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

/// Commits the pending destination selected in the parking-lot map task:
/// promotes the pending value into the committed one, tears down the prompt
/// display, applies the selection to the session, then kills the child task
/// spawned for the selection UI and advances to the next state.
void func_shelter_b1_underground_parking_80184778(Task* task)
{
    D_shelter_b1_underground_parking_8018D788 = D_shelter_b1_underground_parking_8018D789;
    func_shelter_b1_underground_parking_80183B9C();
    func_shelter_b1_underground_parking_8018491C();
    task->killCountdown = 0;
    taskKill((Task*)task->spawnArg2);
    task->state++;
}

void func_shelter_b1_underground_parking_801847D0(Task* task)
{
    SbupExamineWork* work = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    work->fadeLevel += 6;
    if (work->fadeLevel >= 0x100) {
        work->fadeLevel = 0xFF;
    }
    Fade_DrawOverlay((u8)work->fadeLevel, (u8)work->fadeLevel, (u8)work->fadeLevel, 2);
    if (work->fadeLevel == 0xFF) {
        D_80114D08 = 0xA;
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Display_ReleaseRef();
        gGameSession->eventState   = 0;
        gGameSession->hideHud      = 0;
        gGameSession->cutsceneHold = 0;
        D_8007216C                 = 2;
        Task_RequestKill(task, 0);
    }
}

void func_shelter_b1_underground_parking_801848A4(void)
{
    D_shelter_b1_underground_parking_8018D788 = 0xFF;
    D_shelter_b1_underground_parking_8018D789 = 0;
}
