#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105700_80131EA0;

/// Sound ids this actor's cues play, indexed by `Actor105700Work.field_6D6`
/// (row `field_6D6` starts at the second word, the `- 1` in the body).
extern s32 D_actor_105700_80149004[];

INCLUDE_RODATA("actors/nonmatchings/actor_105700/actor_105700", D_actor_105700_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80131ED0);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80132944);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80132B28);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80132C64);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80133040);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80133138);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80133364);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_801334F0);

/// Plays the actor's "appear"/"disappear" cue when the animation record's
/// flags gain bit 5 or bit 4, then mirrors those two bits back into the work
/// block's sound flags so each transition fires once. The pan and depth come
/// from the model's root coordinate.
void func_actor_105700_801336FC(Actor105700* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105700Work* work;
    GpObj38*         self;
    GpAnimRec*       rec;

    work = arg0->field_1C;
    self = arg0->field_2C->field_8;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(&work->ctx, (GpAnimSlot*)&work->slots[1]);
        if (rec != NULL) {
            if (!(rec->field_3 & 0x20) && (work->field_6A0 & 0x20)) {
                snd = D_actor_105700_80149004[work->field_6D6 * 2 - 1] |
                      (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
            }
            if (!(rec->field_3 & 0x10) && (work->field_6A0 & 0x10)) {
                snd = D_actor_105700_80149004[work->field_6D6 * 2] |
                      (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth(self));
            }
            work->field_6A0 = (u16)(rec->field_3 & 0x30);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80133878);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80133C48);

INCLUDE_RODATA("actors/nonmatchings/actor_105700/actor_105700", D_actor_105700_80131EA0);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_801341CC);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80134374);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_8013477C);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80134FDC);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_8013541C);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80135750);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80135AE4);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136158);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136534);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_801369D4);

extern s8 D_80115419;

void func_actor_105700_80132B28(Actor105700* arg0);

/// Per-frame tick, the same body as `Actor02000_Fn03268` of `actor_102000`.
/// State 0 counts `field_6AE` up to 0x5B frames and then hands over to state
/// 1 with animation 4, running `func_actor_105700_80132B28` every frame
/// meanwhile; state 1 waits for `field_698` to reach 0x5E and drops back to
/// state 0 with animation 1. Either way, once `field_6B2` or the global
/// `D_80115419` is set the actor switches to animation 2 and arms the shared
/// state-F0 slot.
void func_actor_105700_80136AE0(Actor105700* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            func_actor_105700_80132B28(arg0);
            break;
        case 1:
            if (work->field_698 >= 0x5E) {
                work->field_694 = 1;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136BC0);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136C4C);
