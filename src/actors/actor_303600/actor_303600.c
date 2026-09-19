#include "common.h"

#include "actors/actor_303600.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

extern Task*    D_actor_303600_8016E4C0;
extern Task*    D_actor_303600_8016E4C4;
extern TaskDesc D_actor_303600_80162E98;

/// The cutscene's two script blocks, handed to `func_800E8634` together when the
/// controller below arms the cutscene.
extern u8 D_actor_303600_80162AF0[];
extern u8 D_actor_303600_80162DD8[];

/// Main-executable globals with no module header yet: a `D_80114C12` of 1 or a
/// live `D_80071075` both mean a cutscene is already up, and `D_80071076` is the
/// latch state 2 below sets alongside `Mc_SaveData`.
extern u8  D_80071075;
extern s16 D_80071076;
extern s8  D_80114C12;

void func_actor_303600_80161F40(Task* arg0);

/// Command dispatcher the cutscene controller steps while the cutscene is up.
/// Commands 1-5 send the slot-4 task message 0x7DA carrying the session's two id
/// bytes and the command as selector, latching it in the published work block's
/// `field_C`; 4 then kills the fade in `D_actor_303600_8016E4C4` and spawns
/// `D_actor_303600_80162E98` entry 1. 6 and 7 spawn entry 2, and 8 kills the fade
/// and spawns entries 3 and 1. The command is cleared on the way out.
void func_actor_303600_80161F40(Task* arg0)
{
    Actor303600Work*  work = (Actor303600Work*)arg0->work;
    Actor303600Work*  w;
    Actor303600Msg7DA msg;

    switch (work->command) {
        case 0:
            break;
        case 1:
            w           = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.field_0 = gGameSession->at4.loc.stage;
            msg.field_1 = gGameSession->at4.loc.area;
            msg.field_2 = 1;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 1;
            break;
        case 2:
            w           = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.field_0 = gGameSession->at4.loc.stage;
            msg.field_1 = gGameSession->at4.loc.area;
            msg.field_2 = 2;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 2;
            break;
        case 3:
            w           = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.field_0 = gGameSession->at4.loc.stage;
            msg.field_1 = gGameSession->at4.loc.area;
            msg.field_2 = 3;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 3;
            break;
        case 4:
            w           = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.field_0 = gGameSession->at4.loc.stage;
            msg.field_1 = gGameSession->at4.loc.area;
            msg.field_2 = 4;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 4;
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            Task_SpawnFromTable(&D_actor_303600_80162E98, 1, 4, 0);
            break;
        case 5:
            w           = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.field_0 = gGameSession->at4.loc.stage;
            msg.field_1 = gGameSession->at4.loc.area;
            msg.field_2 = 5;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 5;
            break;
        case 6:
            Task_SpawnFromTable(&D_actor_303600_80162E98, 2, 8, 0);
            break;
        case 7:
            Task_SpawnFromTable(&D_actor_303600_80162E98, 2, 4, 0);
            break;
        case 8:
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            Task_SpawnFromTable(&D_actor_303600_80162E98, 3, 0, 0);
            Task_SpawnFromTable(&D_actor_303600_80162E98, 1, 4, 0);
            break;
    }
    work->command = 0;
}

/// Cutscene controller for the overlay. State 0 arms it once: a `D_80114C12` of
/// 1 or a live `D_80071075` both mean a cutscene is already up, so the state is
/// left where it is and the task returns; otherwise it allocates the
/// `Actor303600Work` block, zeroes it, parks the `Game_GetPtrSlot(3)` task in
/// `field_0` and publishes itself in `D_actor_303600_8016E4C0` with
/// `D_actor_303600_8016E4C4` cleared, then falls into state 1, which hands the
/// overlay's two cutscene script blocks to `func_800E8634`. State 2 waits for
/// the session's `eventState` to clear -- the cutscene having finished -- and then
/// arms the four `Mc_SaveData` bytes and the `D_80071076` latch the way
/// `func_actor_150400_80131ECC` does, starts the stage-0 type-0x11 task and
/// kills itself; while the cutscene is still up it steps the state machine
/// instead.
void func_actor_303600_8016216C(Task* arg0)
{
    Actor303600Work* work;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            work       = (Actor303600Work*)Mem_Malloc(0x10, 0);
            arg0->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x10);
                work->field_0           = (Task*)Game_GetPtrSlot(3);
                D_actor_303600_8016E4C0 = arg0;
                D_actor_303600_8016E4C4 = NULL;
            }
            arg0->state += 1;
            /* fallthrough */
        case 1:
            func_800E8634((s32)D_actor_303600_80162AF0, 0, (s32)D_actor_303600_80162DD8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Mc_SaveData.at4.loc.stage = 5;
                Mc_SaveData.at4.loc.area  = 0x1F;
                Mc_SaveData.at4.loc.warp  = 1;
                Mc_SaveData.at4.loc.room  = 1;
                D_80071076                = 1;
                Task_Spawn(0, 0x11, 0x10, 0);
                taskKill(arg0);
                break;
            }
            func_actor_303600_80161F40(arg0);
            break;
    }
}

/// Fade-out driver: the same eight-byte channel block `func_actor_303600_801623CC`
/// walks up, walked the other way.  State 0 allocates it and fills all three
/// channels with 0xFF; a failed allocation kills the task outright.  State 1
/// draws the overlay tinted `r`/`g`/`r` in mode 1, steps all three channels down
/// by `Task::spawnArg1` -- the fade rate, not a colour -- and once `r` has gone
/// below zero clears `D_actor_303600_8016E4C4` before killing the task.
void func_actor_303600_801622E8(Task* arg0)
{
    Actor303600FadeWork* work;
    Actor303600FadeWork* alloc;

    work = (Actor303600FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor303600FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 1);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if ((s16)work->r < 0) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(arg0);
            }
            break;
    }
}

/// Fade-in driver: state 0 allocates the eight-byte channel block and clears
/// all three channels; a failed allocation kills the task outright.  State 1
/// runs every frame: it draws the overlay tinted `r`/`g`/`r` in mode 1, steps
/// all three channels by `Task::spawnArg1` -- the fade rate, not a colour -- and
/// once `r` has passed 0x100 clears `D_actor_303600_8016E4C4` before killing the
/// task.  The fade-out counterpart that walks the same block the other way, from
/// 0xFF down past zero, is `func_actor_303600_801622E8`.
void func_actor_303600_801623CC(Task* arg0)
{
    Actor303600FadeWork* work;
    Actor303600FadeWork* alloc;

    work = (Actor303600FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor303600FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 1);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if ((s16)work->r >= 0x100) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(arg0);
            }
            break;
    }
}

/// One-shot announcement of the cutscene: while the work block's "message
/// outstanding" flag is still clear, hand the slot-4 task the session's two id
/// bytes plus selector 9 as message 0x7DA, record 9 in the work block and raise
/// the flag so the message goes out only once.
void func_actor_303600_801624B0(void)
{
    Actor303600Work*  work = (Actor303600Work*)D_actor_303600_8016E4C0->work;
    Actor303600Msg7DA msg;

    if (work->field_E == 0) {
        msg.field_0 = gGameSession->at4.loc.stage;
        msg.field_1 = gGameSession->at4.loc.area;
        msg.field_2 = 9;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_C = 9;
        work->field_E = 1;
    }
}

/// Cutscene teardown: kill the task a previous cutscene left in
/// `D_actor_303600_8016E4C4`, then, while the work block's message flag is
/// still clear, send the same 0x7DA announcement
/// `func_actor_303600_801624B0` sends and latch selector 9.  Finishes by
/// spawning the overlay's own continuation task -- `D_actor_303600_80162E98`
/// entry 3 -- so this runs exactly once per cutscene.
void func_actor_303600_8016253C(void)
{
    Actor303600Work*  work;
    Actor303600Msg7DA msg;

    if (D_actor_303600_8016E4C4 != NULL) {
        taskKill(D_actor_303600_8016E4C4);
        D_actor_303600_8016E4C4 = NULL;
    }

    work = (Actor303600Work*)D_actor_303600_8016E4C0->work;
    if (work->field_E == 0) {
        msg.field_0 = gGameSession->at4.loc.stage;
        msg.field_1 = gGameSession->at4.loc.area;
        msg.field_2 = 9;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_C = 9;
        work->field_E = 1;
    }

    Task_SpawnFromTable(&D_actor_303600_80162E98, 3, 0, 0);
}

void func_actor_303600_80162600(s16 arg0)
{
    Actor303600Work* work = (Actor303600Work*)D_actor_303600_8016E4C0->work;

    work->command = arg0;
    work->field_6 = 0;
}

void func_actor_303600_80162620(void)
{
    Gp_PulseState1C80();
    Gp_StateC08.field_6 |= 1;
}
