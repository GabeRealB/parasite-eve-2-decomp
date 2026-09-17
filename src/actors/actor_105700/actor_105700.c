#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/* `gte_MulMatrix0` from `psyq/gtemac.h`, except with the real `rtv0` / `rtir`
 * encodings this toolchain assembles correctly. */
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105700_80131EA0;

/// Sound ids this actor's cues play, indexed by `Actor105700Work.field_6D6`
/// (row `field_6D6` starts at the second word, the `- 1` in the body).
extern s32 D_actor_105700_80149004[];

/// Per-animation frame marks: row `field_694` holds the frame the 0x1C, 0x28
/// and 0x7A marks of `func_actor_105700_801341CC` are measured from.
extern s16 D_actor_105700_801372EC[];

/// The body objects' variant flag comes from `D_actor_105700_80148F14`.
extern Actor105700PlaceSrc D_actor_105700_80148F14;

/// Set while the player is being grabbed; forces this actor's approach cycle
/// into its handover animation.
extern s8 D_80115419;

/// Nonzero skips the controller's state handler; the approach cycle's teardown
/// switches on it to draw or park the body instead of running the states.
extern u8 D_801153F4;

/// LCG the approach-cycle ticks roll into the `field_6AE` frame budget.
extern u32 Gp_LcgState;

INCLUDE_RODATA("actors/nonmatchings/actor_105700/actor_105700", D_actor_105700_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80131ED0);

/// Per-frame tick of the approach cycle, sharing the `field_6A8` state with
/// `func_actor_105700_801341CC` and `func_actor_105700_80136AE0`; the same body
/// as `Actor02000_Fn00AEC` of `actor_102000` (see `overlay_dup_index.py find
/// func_actor_105700_80132944`). State 0 drains the `field_6DA` budget by
/// `field_69C` (0 while `field_698` is under the per-animation entry of
/// `D_actor_105700_801372EC`, 0x14 once past it) and runs the proximity cue
/// every frame; when the budget runs out it switches to animation 4 and state
/// 1. State 1 waits for `field_698` to reach 0x60, then either falls back to
/// animation 2 (budget left) or starts the lunge: animation 3, state 2, a fresh
/// budget of 1000 per unit of the spawn record's byte 1, and `field_6A2` /
/// `field_6A4` set to the actor's current yaw and its opposite. State 2 holds
/// `field_69E` at 0x3B until `field_698` reaches 0x23, then returns to animation
/// 2 and state 0. A set `field_6B2` or `D_80115419` overrides everything with
/// animation 2 and the shared state-F0 slot.
void func_actor_105700_80132944(Actor105700* arg0)
{
    Actor105700Ctx*  spawn;
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    u8*              head;
    s16              state;
    s16              delta;
    s32              ang;
    s32              param;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;

    self  = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    spawn = arg0->field_20;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= D_actor_105700_801372EC[work->field_694]) {
                delta = 0x14;
            }
            work->field_69C  = delta;
            work->field_69E  = 0;
            work->field_6DA -= work->field_69C;
            if (work->field_6DA <= 0) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
                work->field_69C = 0;
            }
            func_actor_105700_80132B28(arg0);
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x60) {
                if (work->field_6DA <= 0) {
                    param           = spawn->field_3C->field_1;
                    work->field_694 = 3;
                    work->field_6A8 = 2;
                    work->field_6DA = param * 1000;
                    ang             = ratan2(self->coord.m[0][2], self->coord.m[2][2]) & 0xFFF;
                    work->field_6A2 = ang;
                    work->field_6A4 = (ang + 0x800) & 0xFFF;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
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

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

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
