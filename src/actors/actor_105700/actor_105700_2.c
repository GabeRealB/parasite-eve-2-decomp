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

/// The approach-cycle driver: state 0 picks the side from `field_6AA`, states
/// 1 and 2 cue sounds at fixed frames before rolling a dwell, and states 3 and
/// 4 alternate idles until it runs out. The same body as `Actor02000_Fn00E0C`.
void func_actor_105700_80132C64(Actor105700* arg0)
{
    s16              state;
    s16              nextAnim;
    s16              nextAnim2;
    s32              snd;
    s32              random3;
    s32              pan;
    s32              pan2;
    s32              pan3;
    u16              timer;
    u16              timer2;
    u32              random;
    u32              random2;
    Actor105700Work* work;
    GsCOORDINATE2*   self;

    work  = arg0->field_1C;
    self  = arg0->field_2C->field_8;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694          = 0x16;
                work->field_6A8          = 1;
                work->field_6B8          = 1;
                work->field_4CC.field_14 = -0xA7;
            } else {
                work->field_694          = 0x1A;
                work->field_6A8          = 2;
                work->field_6B8          = 2;
                work->field_4CC.field_14 = 0x109;
            }
            work->field_4CC.field_1C = 0x15E;
            work->field_69C          = 0;
            work->field_69E          = 0;
            work->field_6DE          = 1;
            work->field_4CC.flags    = (u16)(work->field_4CC.flags | 0x4000);
            work->field_564.flags    = (u16)(work->field_564.flags & 0xBFFF);
            arg0->field_20->field_4C = 0;
            work->field_6D4          = 1;
            break;
        case 1:
            if (work->field_698 == 0x14) {
                snd = D_actor_105700_80149004[work->field_6D6 + 0xC] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)Gp_GetObjDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = D_actor_105700_80149004[work->field_6D6 + 8] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan2, (s8)Gp_GetObjDepth(self));
            }
            if (work->field_698 >= 0x42) {
                work->field_694 = 0x19;
                work->field_6D4 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_6AE = (u16)((random >> 0x10) & 0x3F);
                Gp_LcgState     = (s32)random;
                if (arg0->field_20->field_40 > 0) {
                    work->field_6A8 = 3;
                } else {
                    arg0->field_30  = 2;
                    work->field_6A8 = 0;
                }
            }
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
                break;
            }
            break;
        case 2:
            if (work->field_698 == 0x19) {
                snd  = D_actor_105700_80149004[work->field_6D6 + 8] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan3 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan3, (s8)Gp_GetObjDepth(self));
            }
            if (work->field_698 >= 0x31) {
                work->field_694 = 0x1D;
                work->field_6D4 = 0;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_6AE = (u16)((random2 >> 0x10) & 0x3F);
                Gp_LcgState     = (s32)random2;
                if (arg0->field_20->field_40 > 0) {
                    work->field_6A8 = 3;
                } else {
                    arg0->field_30  = 2;
                    work->field_6A8 = 0;
                }
            }
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            break;
        case 3:
            timer           = work->field_6AE - 1;
            work->field_6AE = timer;
            if ((s16)timer <= 0) {
                nextAnim = 0x1C;
                if (work->field_6B8 == 1) {
                    nextAnim = 0x18;
                }
                work->field_6AE = 0xAU;
                work->field_694 = nextAnim;
                work->field_6A8 = 4;
                break;
            }
            break;
        case 4:
            timer2          = work->field_6AE - 1;
            work->field_6AE = timer2;
            if ((s16)timer2 <= 0) {
                nextAnim2 = 0x1D;
                if (work->field_6B8 == 1) {
                    nextAnim2 = 0x19;
                }
                work->field_694 = nextAnim2;
                work->field_6A8 = 3;
                random3         = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random3;
                work->field_6AE = (u16)(((u32)random3 >> 0x10) & 0x3F);
            }
            break;
    }
}

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
/// pose `field_4CC.field_14`, the flags `field_4CC.flags` / `field_564.flags`
/// and the step gate are all set before state 1 takes over. State 1 gates the
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
                work->field_694          = 0x16;
                work->field_6A8          = 1;
                work->field_6B8          = 1;
                work->field_6AE          = 0x42;
                work->field_4CC.field_14 = -0xA7;
            } else {
                work->field_694          = 0x1A;
                work->field_6A8          = 1;
                work->field_6B8          = 2;
                work->field_6AE          = 0x31;
                work->field_4CC.field_14 = 0x109;
            }
            work->field_4CC.field_1C = 0x15E;
            work->field_69C          = 0;
            work->field_69E          = 0;
            work->field_6DE          = 1;
            work->field_4CC.flags   |= 0x4000;
            work->field_564.flags   &= 0xBFFF;
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

/// Applies the work block's decaying tilt (`field_688`) to the root
/// coordinate: the tilt's rotation matrix is multiplied column by column into
/// the fourth coordinate's matrix, then X and Y each step 0x20 toward zero,
/// snapping once within 0x20. `field_6B4` is cleared when both have settled.
void func_actor_105700_801334F0(Actor105700* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                         = (MATRIX*)(((Actor105700ScratchStack*)G_SCRATCH_HEAD)->sp - 0x20);
    ((Actor105700ScratchStack*)G_SCRATCH_HEAD)->sp = (u32)matrix;
    active                                         = 0;
    work                                           = arg0->field_1C;
    coord                                          = arg0->field_2C->field_8;
    RotMatrix(&work->field_688, matrix);
    USE_REG(matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv((char*)matrix + 2);
    gte_rtir_real();
    gte_stclmv((char*)&coord[3].coord + 2);
    gte_ldclmv((char*)matrix + 4);
    gte_rtir_real();
    gte_stclmv((char*)&coord[3].coord + 4);
    angleX = work->field_688.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_688.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_688.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_688.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_688.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_688.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_6B4 = 0;
    }
    *(u32*)G_SCRATCH_HEAD += 0x20;
}

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

void               Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
struct _GpEffWork* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);
void               func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Teardown / effect tail of the approach cycle, the same body as
/// `Actor02000_Fn01A20` of `actor_102000`. `D_801153F4` overrides the state
/// machine: 0 clears the body position, 1 draws the ground quad and returns,
/// 2 parks the body behind the actor. Otherwise state 0 unlinks all five body
/// objects (the fifth only for the 0x38 / 0x39 variants), hands the variant
/// halfword to `Gp_ReleaseStateF0Add`, selects animation 0x1D (0x19 for
/// variant 1), saves the enemy pose and switches to state 1; state 1 spawns the
/// ground effect every fourth frame. The tail then reseeds or ticks the
/// nineteen animation slots and redraws the quad.
void func_actor_105700_80133878(GpEnemy* arg0, Actor105700* arg1)
{
    Actor105700Work* work;
    Actor105700Work* animWork;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    GsCOORDINATE2*   part;
    SVECTOR*         scratch;
    VECTOR3          pos;
    s16              anim;
    s16              duration;
    s32              i;
    u32              random;

    work    = arg1->field_1C;
    coord   = arg1->field_2C->field_8;
    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    switch (D_801153F4) {
        case 0:
            arg1->field_2C->field_C = 0;
            arg0->node.field_4      = 0;
            break;
        case 1:
            coord->flg                     = 0;
            arg1->field_2C->field_8[3].flg = 0;
            Gp_UpdateCoord(coord);
            root   = arg1->field_2C->field_8;
            pos.vx = root->workm.t[0];
            pos.vy = root->workm.t[1];
            pos.vz = root->workm.t[2];
            Gp_UpdateActorColor((GpEnemy*)arg1->field_20, (VECTOR*)&pos, 0, 0);
            root   = arg1->field_2C->field_8;
            part   = &root[3];
            pos.vx = part->workm.t[0];
            pos.vy = root->workm.t[1];
            pos.vz = part->workm.t[2];
            Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
            return;
        case 2:
            arg1->field_2C->field_C = 0x80;
            arg0->node.field_4      = 1;
            return;
    }
    switch (work->field_6A8) {
        case 0:
            arg0->field_54 = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_47C);
            Gp_UnlinkObj(&work->field_564);
            Gp_UnlinkObj(&work->field_4CC);
            Gp_UnlinkObj(&work->field_5E4);
            if ((u32)((u16)work->field_6CA - 0x38) < 2U) {
                Gp_UnlinkObj(&work->field_61C);
            }
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, work->field_6CA);
            anim = 0x1D;
            if (work->field_6B8 == 1) {
                anim = 0x19;
            }
            work->field_694 = anim;
            work->field_6A8 = 1;
            arg0->field_4B  = (u8)work->field_6B8;
            Gp_SaveEnemyPose(arg0);
            D_80115419 = 1;
            break;
        case 1:
            if (!(work->field_698 & 3)) {
                scratch->vx = 0;
                scratch->vz = 0;
                random      = (Gp_LcgState * 5) + 0x71357911;
                scratch->vy = -((random >> 0x10) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &arg1->field_2C->field_8[3], 0x400, scratch);
            }
            break;
    }
    animWork = arg1->field_1C;
    i        = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0U;
        duration            = D_actor_105700_801372EC[animWork->field_694];
        do {
            func_800B4114(&animWork->ctx, i, animWork->field_694, 0, (s32)duration);
            i += 1;
        } while (i < 0x13);
        coord->flg = 0;
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)(animWork->field_698 + i);
        do {
            Gp_AnimTickIndex(&animWork->ctx, i);
            i += 1;
        } while (i < 0x13);
        coord->flg = 0;
    }
    arg1->field_2C->field_8[3].flg = 0;
    Gp_UpdateCoord(coord);
    root   = arg1->field_2C->field_8;
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, (VECTOR*)&pos, 0, 0);
    root   = arg1->field_2C->field_8;
    part   = &root[3];
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700_2", func_actor_105700_80133C48);

INCLUDE_RODATA("actors/nonmatchings/actor_105700/actor_105700_2", D_actor_105700_80131EA0);

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
        work->field_5E4.field_18 = Gp_PackPair(&D_actor_105700_80148F14.pair, 4);
        work->field_5E4.flags   |= 0x8000;
    } else if (work->field_698 == anim + 0x28) {
        work->field_5E4.flags &= 0x7FFF;
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

/// Converts the root coordinate's world matrix into the frame of part 7 and
/// parks the (0, 100, -100) offset rotated through it, plus its translation,
/// in `field_644..648`; then stores the (0, -0x514, 10000) vector rotated by
/// the (-5, -5, 0) matrix in `field_63C..640` and raises the fifth body
/// object's 0xC000 flags. While `field_6AE` is non-zero, the parked point is
/// taken back to world space, the distance to the first `field_654` hit (10000
/// with none, plus 1000 for a kind-0x1 hit) replaces the vector's depth, and
/// the rotated result and the parked point go to `func_actor_105700_8013477C`.
void func_actor_105700_80134374(Actor105700* arg0)
{
    Actor105700AimScratch* scratch;
    Actor105700Work*       work;
    GsCOORDINATE2*         self;

    scratch     = (Actor105700AimScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x40);
    self        = arg0->field_2C->field_8;
    work        = arg0->field_1C;
    self[0].flg = 0;
    self[7].flg = 0;
    Gp_UpdateCoord(&self[7]);
    Gp_WorldToLocal(&self->workm, &self[7].workm, &scratch->mtx);
    scratch->vec.vy = 100;
    scratch->vec.vx = 0;
    scratch->vec.vz = -100;
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->vec);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    work->field_644 = scratch->mtx.t[0] + scratch->pos.vx;
    work->field_646 = scratch->mtx.t[1] + scratch->pos.vy;
    work->field_648 = scratch->mtx.t[2] + scratch->pos.vz;
    scratch->rot.vx = -5;
    scratch->rot.vy = -5;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    scratch->rot.vx = 0;
    scratch->rot.vy = -0x514;
    scratch->rot.vz = 10000;
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    work->field_63C        = scratch->pos.vx;
    work->field_63E        = scratch->pos.vy;
    work->field_640        = scratch->pos.vz;
    work->field_61C.flags |= 0xC000;
    if (work->field_6AE == 0) {
        *(u8**)G_SCRATCH_HEAD += 0x40;
        return;
    }
    gte_SetRotMatrix(&self->workm);
    scratch->vec.vx = work->field_644;
    scratch->vec.vy = work->field_646;
    scratch->vec.vz = work->field_648;
    gte_ldv0(&scratch->vec);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    scratch->vec.vx = scratch->pos.vx + self->workm.t[0];
    scratch->vec.vy = scratch->pos.vy + self->workm.t[1];
    scratch->vec.vz = scratch->pos.vz + self->workm.t[2];
    scratch->rot.vx = work->field_63C;
    scratch->rot.vy = work->field_63E;
    if (Gp_FindRec18(work->field_654, 0) != 0) {
        scratch->pos.vx = work->field_654[0].field_8 - scratch->vec.vx;
        scratch->pos.vy = work->field_654[0].field_A - scratch->vec.vy;
        scratch->pos.vz = work->field_654[0].field_C - scratch->vec.vz;
        scratch->rot.vz = SquareRoot0(scratch->pos.vx * scratch->pos.vx + scratch->pos.vy * scratch->pos.vy +
                                      scratch->pos.vz * scratch->pos.vz);
        if ((work->field_654[0].field_4 & 0xFFFF0000) == 0x10000) {
            scratch->rot.vz += 1000;
        }
    } else {
        scratch->rot.vz = 10000;
    }
    Gp_ClearRec18Occupied(work->field_654);
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    scratch->rot.vx = scratch->pos.vx;
    scratch->rot.vy = scratch->pos.vy;
    scratch->rot.vz = scratch->pos.vz;
    scratch->vec.vx = work->field_644;
    scratch->vec.vy = work->field_646;
    scratch->vec.vz = work->field_648;
    func_actor_105700_8013477C(arg0, &scratch->rot, &scratch->vec);
    *(u8**)G_SCRATCH_HEAD += 0x40;
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700_2", func_actor_105700_8013477C);

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

extern s32 D_80115750;
/// Sound id of the burst cue, with the spawn context's room/channel bits
/// packed in like `D_actor_105700_80149048`.
extern s32 D_actor_105700_8014904C;

/// Per-frame tick of the placed effect body from `func_actor_105700_80134FDC`.
/// Mode 0 of `D_801153F4` drifts the root coordinate along its Y axis, puffs
/// an effect every fourth frame and ends the cycle - burst, sound cue and
/// state 2 - on a hit, an empty room-parameter slot, or after 0x5A frames.

void func_actor_105700_8013541C(GpEnemy* arg0, Task* arg1)
{
    Actor105700FxWork* work;
    GsCOORDINATE2*     coord;
    TmdObject*         tmd;
    SVECTOR*           scratch;
    Actor105700Ctx*    ctx;
    s32                found;
    s32                idx;
    s32                sound;
    s32                pan;
    VECTOR             pos;

    tmd   = arg1->extra;
    coord = tmd->field_8;
    work  = (Actor105700FxWork*)arg1->idMap;
    found = 0;
    switch (D_801153F4) {
        case 0:
            tmd->field_C = 0;
            break;
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
            return;
        case 2:
            tmd->field_C = 0x80;
            return;
    }

    coord->flg         = 0;
    coord->coord.t[0] += (coord->coord.m[0][1] * 75) >> 11;
    coord->coord.t[1] += (coord->coord.m[1][1] * 75) >> 11;
    coord->coord.t[2] += (coord->coord.m[2][1] * 75) >> 11;

    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 0x28);
    if (++work->field_E8 >= 4) {
        scratch->vx = 0;
        scratch->vy = 0x64;
        scratch->vz = 0;
        Gp_SpawnEff(0x60070, coord, 0x01001600, scratch);
        work->field_E8 = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);

    if (work->recD0[0].field_4 != 0) {
        idx = func_800E1B24(work->recD0[0].field_4);
        if (Gp_RoomParamTables[Game_Session->field_7 - 1][Game_Session->field_6 - 1][idx]->field_1 == 0) {
            found = 1;
        }
        Gp_ClearRec18Occupied(work->recD0);
    }
    if (work->rec60[0].field_4 != 0 || found || ++work->field_EA >= 0x5A) {
        Gp_SpawnEff(D_80115750, coord, work->field_EE, NULL);
        ((TmdObject*)arg1->extra)->field_C = 0x80;
        ctx                                = arg1->spawnArg2;
        sound                              = D_actor_105700_8014904C | (((u16)ctx->field_8 >> 0xC) << 8);
        pan                                = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        arg1->state = 2;
        if ((work->rec60[0].field_4 & 0xFFFF0080) == 0x10000) {
            Gp_SpawnPadLerp(0xA, 0xFF, 8);
        }
    }
    *(u8**)G_SCRATCH_HEAD += 0x28;
}

extern s32 D_80115758;
extern s32 D_actor_105700_80149050;
extern s32 D_actor_105700_80149054;

/// Four-step burst sequence driven by `field_6A8`: 0 spawns the effect and cue
/// on entry, rumbles every tenth frame and either ends after `field_6B6` passes
/// 0x28 or times out at 0x96 frames; 1 counts `field_6AE` down into 2; 2
/// triggers the PE state and plays the second cue; 3 spawns random-offset
/// sparks every fourth frame until `field_6AE` runs out.
void func_actor_105700_80135750(Actor105700* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    SVECTOR*         scratch;
    s32              sound;
    u32              random;

    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    work    = arg0->field_1C;
    self    = arg0->field_2C->field_8;
    switch (work->field_6A8) {
        case 0:
            if (work->field_6AE == 0) {
                scratch->vx     = 0;
                scratch->vy     = 0;
                scratch->vz     = 0;
                work->field_690 = Gp_SpawnEff(D_80115758, &arg0->field_2C->field_8[4], 0x96, scratch);
                sound           = D_actor_105700_80149050 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)self), (s8)Gp_GetObjDepth((GpObj38*)self));
            }
            work->field_6CE = work->field_6D0 > 0;
            if ((s16)(work->field_6AE % 10) == 0) {
                Gp_SpawnPadLerp(5, 0x80, 8);
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6C2 = 0;
                if (--work->field_6C4 <= 0) {
                    work->field_6A6 = 9;
                    work->field_6A8 = 0;
                } else {
                    work->field_6A6 = 5;
                    work->field_6A8 = 3;
                    work->field_694 = 0x15;
                    work->field_6AE = 0x4F;
                }
                work->field_6CE = 0;
                if (work->field_690 != NULL) {
                    work->field_690->field_0->state = 3;
                }
                work->field_690 = NULL;
            } else if (++work->field_6AE >= 0x96) {
                work->field_6AE = 8;
                work->field_6A8 = 1;
                work->field_694 = 0xB;
                work->field_6C2 = 0;
                work->field_6CE = 0;
                work->field_690 = NULL;
                Gp_SpawnPadLerp(0xF, 0xFF, 8);
            }
            break;
        case 1:
            if (--work->field_6AE <= 0) {
                work->field_6A8 = 2;
                work->field_6AE = 0;
            }
            break;
        case 2:
            Gp_TriggerPeState(0, 0x10);
            work->field_6A6 = 2;
            work->field_6A8 = 0;
            work->field_694 = 2;
            sound           = D_actor_105700_80149054 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)self), (s8)Gp_GetObjDepth((GpObj38*)self));
            break;
        case 3:
            if (!(work->field_698 & 3)) {
                scratch->vx = 0;
                scratch->vz = 0;
                random      = Gp_LcgState * 5 + 0x71357911;
                scratch->vy = -((random >> 16) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &arg0->field_2C->field_8[3], 0x100, scratch);
            }
            if (--work->field_6AE <= 0) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700_2", func_actor_105700_80135AE4);

/// Per-state handlers of the approach cycle, indexed by `field_6A6`.
extern void (*D_actor_105700_801492A4[])(Actor105700*);

void func_actor_105700_80131ED0(Actor105700* arg0);

/// Every third frame while `field_6C4` is clear, kicks a dust effect off the
/// fourth body coordinate with a random upward velocity.
static __inline__ void Actor105700_SpawnDust(Actor105700* actor)
{
    Actor105700Work* work;
    SVECTOR*         head;
    SVECTOR*         rot;

    work                       = actor->field_1C;
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    rot                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = rot;
    if (++work->field_6B0 >= 3) {
        work->field_6B0 = 0;
        head[-1].vx     = 0;
        rot->vz         = 0;
        rot->vy         = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1FF);
        Gp_SpawnEff(0x600E0, &actor->field_2C->field_8[3], 0x100, rot);
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

/// Per-frame tick: runs the state handler, integrates the forward step,
/// advances or reseeds the animation slots, then draws. The same body as
/// `Actor02000_Fn02A34` plus the dust effect.
void func_actor_105700_80136158(GpEnemy* ctx, Actor105700* actor)
{
    VECTOR3          pos;
    Actor105700Ctx*  spawn;
    Actor105700Obj*  model;
    Actor105700Work* moveWork;
    Actor105700Work* animWork;
    Actor105700Work* work;
    Actor105700Work* flagWork;
    GsCOORDINATE2*   moveCoord;
    GsCOORDINATE2*   part;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    s16              duration;
    s32              i;
    u8               flags;

    work  = actor->field_1C;
    model = actor->field_2C;
    coord = model->field_8;
    switch (D_801153F4) {
        case 0:
            model->field_C    = 0;
            ctx->node.field_4 = 0;
            break;
        case 1:
            goto draw;
        case 2:
            model->field_C    = 0x80;
            ctx->node.field_4 = 1;
            return;
    }

    if (ctx->field_4C != 0) {
        spawn    = actor->field_20;
        flags    = spawn->field_4C;
        flagWork = actor->field_1C;
        if ((flags & 2) && (flagWork->field_6B8 == 0)) {
            spawn->field_4C     = flags & 0xFD;
            flagWork->field_6A6 = 0xA;
            flagWork->field_694 = 0x14;
            flagWork->field_6A8 = 0;
            flagWork->field_6E0 = 1;
        }
    }
    func_actor_105700_80131ED0(actor);
    D_actor_105700_801492A4[work->field_6A6](actor);
    if (work->field_69E != 0) {
        func_actor_105700_80133364(actor);
    }
    moveCoord              = actor->field_2C->field_8;
    moveWork               = actor->field_1C;
    moveWork->field_678    = moveCoord->coord.t[0];
    moveWork->field_67C    = moveCoord->coord.t[1];
    moveWork->field_680    = moveCoord->coord.t[2];
    moveCoord->coord.t[0] += (s32)(moveCoord->coord.m[0][2] * moveWork->field_69C) >> 0xC;
    if (moveWork->field_6DE < 2) {
        moveCoord->coord.t[1] += 0x80;
    }
    moveCoord->coord.t[2] += (s32)(moveCoord->coord.m[2][2] * moveWork->field_69C) >> 0xC;
    animWork               = actor->field_1C;
    i                      = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0;
        duration            = D_actor_105700_801372EC[animWork->field_694];
        do {
            func_800B4114(&animWork->ctx, i, animWork->field_694, 0, duration);
            i += 1;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)animWork->field_698 + i;
        do {
            Gp_AnimTickIndex(&animWork->ctx, i);
            i += 1;
        } while (i < 0x13);
    }
    if (work->field_6B4 != 0) {
        func_actor_105700_801334F0(actor);
    }
    func_actor_105700_801336FC(actor);
    coord->flg                      = 0;
    actor->field_2C->field_8[3].flg = 0;
    Gp_UpdateCoord(coord);
    if (work->field_6C4 == 0) {
        Actor105700_SpawnDust(actor);
    }
draw:
    USE_REG(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)actor->field_20, (VECTOR*)&pos, 0, 0);
    root   = actor->field_2C->field_8;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700_2", func_actor_105700_80136534);
