#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/D4.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/sound.h"

/// The overlay's event/controller task, whose `idMap` holds an
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

/// Animation-set table this overlay hands the player task as message 0x3F4's
/// `GpAnimArg::field_0`, the counterpart of `D_actor_403100_8015570C`. The first
/// entry is a `GpAnimSet` in the overlay's own data; the other three point at
/// its work areas.
extern GpAnimSet* D_actor_444000_8014430C[4];

/// Run one step of the event task: act on the pending action index in
/// `field_2C`, then clear it so the action fires once.
void func_actor_444000_80132054(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->idMap;
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
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
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
            other = (Actor444000EventWork*)D_actor_444000_80161860->idMap;
            if (other->field_2A == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
                other->field_2A = 1;
            }
            break;
        case 3:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            target               = (Actor444000EventWork*)task->idMap;
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

    work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;
    switch (arg0) {
        case 0:
            Game_Session->field_52 = 1;
            D_8007216C             = work->field_28;
            break;
        case 1:
        case 2:
            switch (Game_Session->field_132) {
                case 0:
                    Game_Session->field_5 = 4;
                    D_8007216D            = 4;
                    break;
                case 1:
                    Game_Session->field_5 = 5;
                    D_8007216D            = 5;
                    break;
                case 2:
                case 3:
                    Game_Session->field_5 = 6;
                    D_8007216D            = 6;
                    break;
            }
            Game_Session->unknown_133[1] = Game_Session->field_5 - 1;
            Game_Session->unknown_133[0] = 1;
            Game_Session->field_76       = 1;
            D_8007216C                   = work->field_28;
            Gp_ApplyAreaRecs(D_8018FB6C);
            if (arg0 == 1) {
                work->field_24 = Task_Spawn(1, 0x2D, 0x10, 0);
            }
            Game_Session->field_52 = 1;
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000", func_actor_444000_80132358);

/// Play the event's sound cue once, latching a flag so a repeat call is a no-op.
void func_actor_444000_80132608(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    if (work->field_2A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_2A = 1;
    }
}

/// Forward a message to the slot-3 task the event work block carries.
void func_actor_444000_8013265C(s32 arg0)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    Gp_DispatchMsg(work->field_20, 0x3F3, arg0, 0);
}

/// Kill the subordinate task the event work block carries, if it is still alive.
void func_actor_444000_80132694(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    if (work->field_24 != NULL) {
        Task_Kill(work->field_24);
        work->field_24 = NULL;
    }
}

void func_actor_444000_801326DC(void)
{
    Actor444000Msg7DA msg;

    msg.field_0 = 0;
    msg.field_1 = 0x2C;
    msg.field_2 = 3;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000", D_actor_444000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000", jtbl_actor_444000_80131E24);
