#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/D4.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/sound.h"
#include "main/mem.h"

/// The overlay's event/controller task, whose `work` holds an
/// `Actor444000EventWork`.
extern Task* D_actor_444000_80161860;

/// Weapon class (1 is the class whose animations sit at the low base) and the
/// equipped-weapon index within it; together they pick the player animation the
/// action-1 cue installs.
extern s8 D_8007218A;
extern u8 D_80073BA9;

/// The area-record id the loader replays, and the view index that goes with it;
/// `func_actor_444000_801321FC` republishes both whenever the room is entered.
extern u8 D_8007216C;
extern s8 D_8007216D;

/// 0xFF-terminated area-record list this overlay applies on entry.
extern GpAreaApplyRec D_8018FB6C[];

/// Main-executable globals with no module header yet: `D_80071075` gates the
/// event on the "everything is dead" state, and `D_8007272D` is the ending
/// selector the death sequence latches.
extern u8 D_80071075;
extern s8 D_8007272D;

/// Gameplay-resident globals the state-3 hand-off touches: `D_80187150` is the
/// task table the successor is spawned from, `D_8018FBC8` the view id copied
/// into `GameSession::sceneClock`, and `D_801855DE` a counter cleared with it.
extern s16      D_801855DE;
extern TaskDesc D_80187150;
extern u16      D_8018FBC8;

/// Spawn tables `func_800E8634` forwards to `Task_Spawn`, taken as raw
/// addresses: the first pair is used by the `spawnArg1` fast path in state 0
/// and the second by state 2.
extern u8 D_actor_444000_80144634;
extern u8 D_actor_444000_8014488C;
extern u8 D_actor_444000_8014431C;
extern u8 D_actor_444000_801444E4;

/// Animation-set table this overlay hands the player task as message 0x3F4's
/// `GpAnimArg::field_0`, the counterpart of `D_actor_403100_8015570C`. The first
/// entry is a `GpAnimSet` in the overlay's own data; the other three point at
/// its work areas.
extern GpAnimSet* D_actor_444000_8014430C[4];

/// Run one step of the event task: act on the pending action index in
/// `field_2C`, then clear it so the action fires once.
void func_actor_444000_80132054(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->work;
    Actor444000EventWork* other;
    Actor444000EventWork* target;
    GpAnimArg             msg;
    s32                   anim;

    switch (work->field_2C) {
        case 0:
            break;
        case 1:
            /* Install the weapon-specific player animation on the slot-3 task. */
            anim = D_80073BA9;
            if (D_8007218A == 1) {
                anim += 1;
            } else {
                anim += 0x22;
            }
            msg.field_0  = (void*)anim;
            msg.field_4  = 1;
            msg.field_8  = 1;
            msg.field_C  = 0xA;
            msg.field_10 = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            break;
        case 2:
            if (work->field_20 != NULL) {
                msg.field_0  = D_actor_444000_8014430C;
                msg.field_4  = 3;
                msg.field_8  = 0;
                msg.field_C  = 0;
                msg.field_10 = 0;
                Gp_DispatchMsg(work->field_20, 0x3F4, (s32)&msg, 0);
            }
            /* Same one-shot cue as func_actor_444000_80132608. */
            other = (Actor444000EventWork*)D_actor_444000_80161860->work;
            if (other->field_2A == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
                other->field_2A = 1;
            }
            break;
        case 3:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            target               = (Actor444000EventWork*)task->work;
            if (target->field_20 != NULL) {
                msg.field_0  = D_actor_444000_8014430C;
                msg.field_4  = 0;
                msg.field_8  = 1;
                msg.field_C  = 0xA;
                msg.field_10 = 0;
                Gp_DispatchMsg(target->field_20, 0x3F4, (s32)&msg, 0);
            }
            break;
    }
    work->field_2C = 0;
}

/// Bring the room's presentation up to date for an enter (0), a first entry
/// (1) or a re-entry (2): pick the view set from the current disc/scenario
/// stage in `GameSession::field_132`, republish the area-record id, and on a
/// first entry spawn the accompanying task. Any other `arg0` does nothing.
void func_actor_444000_801321FC(s32 arg0)
{
    Actor444000EventWork* work;

    work = (Actor444000EventWork*)D_actor_444000_80161860->work;
    switch (arg0) {
        case 0:
            gGameSession->viewDirty = 1;
            D_8007216C              = work->field_28.b;
            break;
        case 1:
        case 2:
            switch (gGameSession->field_132) {
                case 0:
                    gGameSession->at4.loc.room = 4;
                    D_8007216D                 = 4;
                    break;
                case 1:
                    gGameSession->at4.loc.room = 5;
                    D_8007216D                 = 5;
                    break;
                case 2:
                case 3:
                    gGameSession->at4.loc.room = 6;
                    D_8007216D                 = 6;
                    break;
            }
            gGameSession->eventRoomIndex = gGameSession->at4.loc.room - 1;
            gGameSession->field_133      = 1;
            gGameSession->roomObjsDirty  = 1;
            D_8007216C                   = work->field_28.b;
            Gp_ApplyAreaRecs(D_8018FB6C);
            if (arg0 == 1) {
                work->field_24 = Task_Spawn(1, 0x2D, 0x10, 0);
            }
            gGameSession->viewDirty = 1;
            break;
    }
}

/// Task body of the overlay's event/controller task, run once per frame while
/// the session is not paused (`GameSession::field_65`), no cutscene is active
/// (`Gp_StateC08.field_9`) and the battle state is not frozen
/// (`Gp_StateF0.field_4`).
///
/// State 0 allocates the `Actor444000EventWork` block and publishes the task in
/// `D_actor_444000_80161860`; a task spawned with `spawnArg1` set jumps
/// straight to state 3, otherwise it advances one state at a time. State 1
/// counts 0x2BD frames and then arms the death/ending sequence once. State 2
/// counts 0x15 frames and hands off to the follow-up task table. State 3 waits
/// for the room to settle, spawns the successor from `D_80187150` and kills
/// this task.
void func_actor_444000_80132358(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->work;
    Actor444000EventWork* alloc;
    Actor444000EventWork* other;
    s32                   state;
    s16                   timer;

    if (gGameSession->field_65 != 0) {
        return;
    }
    if ((s8)Gp_StateC08.field_9 != 0) {
        return;
    }
    if (Gp_StateF0.field_4 != 0) {
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.field_A == 1) {
                return;
            }
            if (D_80071075 != 0) {
                return;
            }
            alloc      = (Actor444000EventWork*)memCalloc(sizeof(Actor444000EventWork), false);
            task->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(task);
            } else {
                Mem_Set(alloc, 0, sizeof(Actor444000EventWork));
                alloc->field_20         = gameGetPtrSlot(3);
                D_actor_444000_80161860 = task;
            }
            if (task->spawnArg1 != 0) {
                work             = (Actor444000EventWork*)task->work;
                work->field_28.h = gGameSession->at4.loc.view;
                Gp_MsgPlayerWeapon(0);
                func_800E8634((s32)&D_actor_444000_80144634, 0, (s32)&D_actor_444000_8014488C);
                task->state = 3;
            } else {
                task->state += 1;
            }
            break;
        case 1:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x2BD) {
                Gp_MsgPlayerWeapon(0);
                other = (Actor444000EventWork*)D_actor_444000_80161860->work;
                if (other->field_30 == 0) {
                    Gp_StateF0.field_6       = 0;
                    Gp_StateF0.field_1       = 0xF;
                    Gp_StateF0.field_0       = 0;
                    Gp_StateF0.field_2       = 0;
                    Gp_StateF0.field_3       = 0;
                    gGameSession->flowFlags |= 0x80;
                    D_8007272D               = 0xD;
                    other->field_30          = state;
                }
                task->killCountdown = 0;
                task->state        += 1;
            }
            break;
        case 2:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x15) {
                work->field_28.h = gGameSession->at4.loc.view;
                func_800E8634((s32)&D_actor_444000_8014431C, 0, (s32)&D_actor_444000_801444E4);
                task->state += 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_801855DE               = 0;
                gGameSession->sceneClock = D_8018FBC8;
                Task_SpawnFromTable(&D_80187150, 0, 1, 0);
                taskKill(task);
                return;
            }
            break;
    }
    func_actor_444000_80132054(task);
}

/// Play the event's sound cue once, latching a flag so a repeat call is a no-op.
void func_actor_444000_80132608(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_2A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_2A = 1;
    }
}

/// Forward a message to the slot-3 task the event work block carries.
void func_actor_444000_8013265C(s32 arg0)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    Gp_DispatchMsg(work->field_20, 0x3F3, arg0, 0);
}

/// Kill the subordinate task the event work block carries, if it is still alive.
void func_actor_444000_80132694(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_24 != NULL) {
        taskKill(work->field_24);
        work->field_24 = NULL;
    }
}

void func_actor_444000_801326DC(void)
{
    Actor444000Msg7DA msg;

    msg.field_0 = 0;
    msg.field_1 = 0x2C;
    msg.field_2 = 3;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}
