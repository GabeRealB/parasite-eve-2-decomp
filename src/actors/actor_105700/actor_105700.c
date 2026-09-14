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

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80132C64);

/// The `field_6A8` state shared by the approach-cycle ticks; state 0 arms the
/// dwell the `field_6B8` selector picks, and states 1 and 2 wait for
/// `field_698` to reach 0x10 / 0x16 before parking animation 0x19 / 0x1D and
/// rolling `Gp_LcgState` into the `field_6AE` budget. The same body as
/// `Actor02000_Fn011E8` of `actor_102000`.
void func_actor_105700_80133040(Actor105700* arg0)
{
    Actor105700Work* work;
    s16              state;
    s32              next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                work->field_694 = 0x19;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                work->field_694 = 0x1D;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}

/// Per-frame tick of the actor's approach cycle, the verbatim counterpart of
/// `Actor02000_Fn012E0` of `actor_102000` (see `overlay_dup_index.py find
/// func_actor_105700_80133138`, which also lists five more actors carrying it).
/// State 0 arms the cycle: `field_6AA` picks the dwell and animation, and the
/// pose `field_4E0`, the flags `field_4EA` / `field_582`, the two counters and
/// the step gate are all set before state 1 takes over. State 1 gates the
/// handover once through `field_6DE`, plays the cue of the animation `field_6B8`
/// selects at its 0x14 / 0x2C frame mark, and drops back to state 0 when the
/// `field_6AE` frame budget runs out.
void func_actor_105700_80133138(Actor105700* arg0)
{
    Actor105700Work* work;
    GpObj38*         self;
    s32              snd;
    s16              state;

    work  = arg0->field_1C;
    self  = (GpObj38*)arg0->field_2C->field_8;
    state = work->field_6A8;

    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694 = 0x16;
                work->field_6A8 = 1;
                work->field_6B8 = 1;
                work->field_6AE = 0x42;
                work->field_4E0 = -0xA7;
            } else {
                work->field_694 = 0x1A;
                work->field_6A8 = 1;
                work->field_6B8 = 2;
                work->field_6AE = 0x31;
                work->field_4E0 = 0x109;
            }
            work->field_4E8          = 0x15E;
            work->field_69C          = 0;
            work->field_69E          = 0;
            work->field_6DE          = 1;
            work->field_4EA         |= 0x4000;
            work->field_582         &= 0xBFFF;
            arg0->field_20->field_4C = 0;
            work->field_6D4          = 1;
            break;
        case 1:
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            if (work->field_6B8 == 1) {
                if (work->field_698 == 0x14) {
                    s32 pan;

                    snd = D_actor_105700_80149004[work->field_6D6 + 0xC] |
                          (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = D_actor_105700_80149004[work->field_6D6 + 8] |
                          (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = D_actor_105700_80149004[work->field_6D6 + 8] |
                      (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);

                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
            }
            work->field_6AE--;
            if (work->field_6AE <= 0) {
                arg0->field_30  = 2;
                work->field_6A8 = 0;
                work->field_6D4 = 0;
            }
            break;
    }
}

/// Verbatim port of `Actor02000_Fn0150C` of `actor_102000` - the two bodies are
/// byte-identical (see `overlay_dup_index.py find func_actor_105700_80133364`,
/// which also lists five more actors carrying it). Takes the root coordinate's
/// own heading through `ratan2` and steps the yaw `field_6A2` toward the parked
/// `field_6A4` by the dwell counter `field_69E` per frame: within half a turn
/// of the target it closes on it directly (or, for `field_694 == 3`, spins past
/// it by the unsigned counter), past that it unwinds the long way, snapping
/// straight onto the target once `field_69E` would overshoot. The resulting yaw
/// rebuilds the coordinate's matrix.
void func_actor_105700_80133364(Actor105700* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              ustep;
    s32              wstep;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_6A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_6A2 = ang;
    if (adiff < 0x800) {
        step  = work->field_69E;
        ustep = (u16)work->field_69E;
        if (step >= adiff) {
            work->field_6A2 = want;
        } else {
            if (work->field_694 == 3) {
                next = ang - ustep;
            } else {
                next = work->field_6A2;
                if (diff <= 0) {
                    next -= step;
                } else {
                    next += step;
                }
            }
            work->field_6A2 = next;
        }
    } else {
        wstep = work->field_69E;
        if (diff > 0) {
            if (wstep >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (wstep >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_6A2 = work->field_6A4;
        goto done;
    turn:
        if (work->field_694 == 3) {
            work->field_6A2 = (u16)work->field_6A2 - (u16)work->field_69E;
        } else {
            wrapStep = work->field_69E;
            cur      = work->field_6A2;
            if (diff > 0) {
                work->field_6A2 = cur - wrapStep;
            } else {
                work->field_6A2 = cur + wrapStep;
            }
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_6A2;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

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

/// Runs the animation's mark events: measures `field_698` against the three
/// frames `D_actor_105700_801372EC[field_694]` marks out. At the 0x1C mark the
/// body object is packed from `D_actor_105700_80148F14` and bit 0x8000 raised,
/// at 0x28 dropped; inside the 0x1C..0x1E window the player's distance decides
/// whether `field_69C` parks at 0x64; and past 0x7A the actor hands over to
/// animation 4. The delta the distance is taken from is left in the scratch
/// vector it is accumulated in.
void func_actor_105700_801341CC(Actor105700* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;
    s16              anim;
    s32              dx;
    s32              dz;
    s32              distance;

    *(VECTOR**)G_SCRATCH_HEAD -= 1;
    delta                      = *(VECTOR**)G_SCRATCH_HEAD;
    work                       = arg0->field_1C;
    anim                       = D_actor_105700_801372EC[work->field_694];
    self                       = arg0->field_2C->field_8;
    if (work->field_698 == anim + 0x1C) {
        work->field_5FC  = Gp_PackPair(&D_actor_105700_80148F14.pair, 4);
        work->field_602 |= 0x8000;
    } else if (work->field_698 == anim + 0x28) {
        work->field_602 &= 0x7FFF;
    }
    anim = D_actor_105700_801372EC[work->field_694];
    if ((work->field_698 >= anim + 0x1C) && (anim + 0x1E >= work->field_698)) {
        dx        = Wip_SysConfig.field_4->t[0] - self->coord.t[0];
        delta->vx = dx;
        dz        = Wip_SysConfig.field_4->t[2] - self->coord.t[2];
        delta->vz = dz;
        distance  = SquareRoot0((delta->vx * delta->vx) + (delta->vz * delta->vz));
        if (distance < 0x3E8) {
            work->field_69C = 0;
        } else {
            work->field_69C = 0x64;
        }
    } else {
        work->field_69C = 0;
    }
    if (work->field_698 >= D_actor_105700_801372EC[work->field_694] + 0x7A) {
        work->field_6A6 = 2;
        work->field_6A8 = 2;
        work->field_694 = 4;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_80134374);

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700", func_actor_105700_8013477C);

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `GpRec18` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into `D_actor_105700_80149048`.
extern s32 D_actor_105700_80149048;

void func_actor_105700_80134FDC(GpEnemy* arg0, Task* arg1)
{
    Actor105700FxWork*       work;
    Actor105700PlaceScratch* scratch;
    Actor105700Ctx*          ctx;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           parentCoord;
    TmdObject*               tmd;
    Task*                    parent;
    s32                      sound;
    s32                      pan;

    tmd         = arg1->extra;
    coord       = tmd->field_8;
    parent      = arg1->parent;
    parentCoord = ((TmdObject*)parent->extra)->field_8;
    work        = Mem_Calloc(0xF0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap   = (TaskIdMap*)work;
    tmd->field_C  = 0;
    scratch       = (Actor105700PlaceScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x38);
    tmd->field_1C = &work->lightMtx;
    tmd->field_20 = &work->colorMtx;

    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = 0;
    scratch->rot.vy = 0x1F4;
    scratch->rot.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    coord->sub         = &Gfx_ViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = 0x80;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0x10;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir_real();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][2]);

    work->field_EE = (D_actor_105700_80148F14.field_E != 1);

    work->obj40.field_8  = coord;
    work->obj40.field_C  = work->rec60;
    work->obj40.field_10 = 0;
    work->obj40.field_12 = 0;
    work->obj40.field_14 = 0;
    work->obj40.field_18 = Gp_PackPair(&D_actor_105700_80148F14.pair, 3);
    work->obj40.field_1C = 0x64;
    work->obj40.flags    = 1;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(work->rec60, 1, 0);
    work->obj40.flags |= 0x8000;

    work->obj78.field_8  = coord;
    work->obj78.field_C  = work->rec60;
    work->obj78.field_10 = 0;
    work->obj78.field_12 = 0;
    work->obj78.field_14 = 0;
    work->obj78.field_18 = 0x22B2B;
    work->obj78.field_1C = 0x64;
    work->obj78.flags    = 1;
    Gp_LinkObj(1, &work->obj78);
    work->obj78.flags |= 0x8000;

    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_A  = -0x1F4;
    work->d4rec.field_C  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->d4rec.field_14 = work->recD0;
    work->obj98.field_C  = (GpRec18*)&work->d4rec;
    work->obj98.field_8  = coord;
    work->obj98.field_10 = 0;
    work->obj98.field_12 = 0;
    work->obj98.field_14 = 0;
    work->obj98.field_18 = 0;
    work->obj98.field_1C = 0;
    work->obj98.flags    = 3;
    Gp_LinkObj(3, &work->obj98);
    Gp_InitRec18Table(work->recD0, 1, 0);
    work->obj98.flags |= 0x4400;

    arg1->state = 1;
    Task_DetachFromParent(arg1);

    coord->flg = 0;
    Gp_UpdateCoord(coord);

    ctx   = arg1->spawnArg2;
    sound = D_actor_105700_80149048 | (((u16)ctx->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));

    *(u8**)G_SCRATCH_HEAD += 0x38;
}

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
