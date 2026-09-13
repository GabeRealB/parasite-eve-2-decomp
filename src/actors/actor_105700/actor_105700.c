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

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136AE0);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136BC0);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136C4C);
