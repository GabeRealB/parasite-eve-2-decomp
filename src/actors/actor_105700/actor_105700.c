#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/wipsys.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105700_80131EA0;

/// Sound ids this actor's cues play, indexed by `Actor105700Work.field_6D6`
/// (row `field_6D6` starts at the second word, the `- 1` in the body).
extern s32 D_actor_105700_80149004[];

INCLUDE_RODATA("actors/nonmatchings/actor_105700/actor_105700", D_actor_105700_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80131ED0);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80132944);

/// Proximity cue; the same body as `Actor02000_Fn00CD0` of `actor_102000`
/// (see `overlay_dup_index.py find func_actor_105700_80132B28`). Carves a
/// 0x10-byte direction vector off the scratch head, aims it from the player
/// at the actor's root coordinate, and takes its length through
/// `SquareRoot0`: under 0x5DC one of `D_801153F2`'s bit groups raises
/// `field_6B2`; past it the other two (the second only within 0xBB8) put the
/// actor into animation 4 and state 1.
void func_actor_105700_80132B28(Actor105700* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    s32              dx;
    s32              distance;
    s32              dz;
    s32              trigger;
    VECTOR*          head;
    VECTOR*          delta;

    self                      = arg0->field_2C->field_8;
    work                      = arg0->field_1C;
    head                      = *(VECTOR**)G_SCRATCH_HEAD;
    delta                     = head - 1;
    head[-1].vx               = (s32)(Wip_SysConfig.field_4->t[0] - self->coord.t[0]);
    delta->vy                 = 0;
    dz                        = Wip_SysConfig.field_4->t[2] - self->coord.t[2];
    delta->vz                 = dz;
    dx                        = head[-1].vx;
    trigger                   = 0;
    *(VECTOR**)G_SCRATCH_HEAD = delta;
    distance                  = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x5DC) {
        if (D_801153F2 & 0x17) {
            work->field_6B2 = 1;
        }
    } else {
        if (D_801153F2 & 5) {
            trigger = 1;
        }
        if ((D_801153F2 & 0x12) && (distance < 0xBB8)) {
            trigger = 1;
        }
        if (trigger != 0) {
            work->field_694 = 4;
            work->field_69C = 0;
            work->field_69E = 0;
            work->field_6AE = 0;
            work->field_6A8 = 1;
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

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
    self = (GpObj38*)arg0->field_2C->field_8;
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

/// Segment/quad collision test against every face in the `D_80115550` list.
/// Carves a 0x10-byte direction vector off the scratch head, normalises it
/// from `arg0` to `arg1`, then returns the first `func_800DFCCC` result of 1
/// (0 when no face reports one). The same body as `func_800E0308` of
/// `gameplay` and five other actor overlays - see
/// `overlay_dup_index.py find func_actor_105700_801369D4`.
s32 func_actor_105700_801369D4(SVECTOR* arg0, SVECTOR* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret                          = 0;
    scratch                      = (void**)G_SCRATCH_HEAD;
    node                         = D_80115550;
    head                         = *scratch;
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    TOUCH_REG_USE(head, node);
    vec      = (VECTOR*)head;
    vec->vy  = arg1->vy - arg0->vy;
    *scratch = vec;
    vec->vz  = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

extern s8 D_80115419;

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

/// State advance for the "handover" clip, the same body as
/// `Actor02000_Fn03348` of `actor_102000`. State 0 arms animation 0x11 and
/// clears the pair of dwell counters; state 1 waits for `field_698` to reach
/// 0x37 and then picks animation 2 with a 2-frame park in `field_6A6`, or
/// animation 0x14 with a 10-frame park when `field_6E0` is set. Either path
/// drops back to state 0.
void func_actor_105700_80136BC0(Actor105700* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80136C4C);
