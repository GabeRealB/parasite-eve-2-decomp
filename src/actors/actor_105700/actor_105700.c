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

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 D_actor_105700_80148F38[];
extern s16 D_actor_105700_80148F94[];

s32 Actor00300_Fn04B14(SVECTOR* arg0, SVECTOR* arg1);

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void func_actor_105700_80131ED0(Actor105700* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor105700Work*       work;
    GpDeltaScratch*        head;
    Actor105700HitScratch* scratch;
    GpEnemy*               enemy;
    GsCOORDINATE2*         self;
    GsCOORDINATE2*         other;
    GsCOORDINATE2*         part;
    s32                    i;
    s32                    x, y, z;
    s32                    damage;
    s32                    kind;
    s32                    dz;
    s32                    clamped;
    s32                    val;
    s32                    push;
    s16                    cooldown;
    u32                    rng;
    s32                    tilt;
    s32                    byte1;

    result                                    = 0;
    maxPush                                   = 0;
    hit                                       = 0;
    lastId                                    = 0;
    work                                      = arg0->field_1C;
    head                                      = *(GpDeltaScratch**)G_SCRATCH_HEAD;
    self                                      = arg0->field_2C->field_8;
    *(Actor105700HitScratch**)G_SCRATCH_HEAD -= 1;
    scratch                                   = *(Actor105700HitScratch**)G_SCRATCH_HEAD;
    enemy                                     = (GpEnemy*)arg0->field_20;

    switch (func_800E0C10(work->field_584, head - 4, 4, NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-4].vx.h.hi;
            self->coord.t[1] += scratch->delta.vy.h.hi;
            self->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            self->coord.t[0] = work->field_678;
            self->coord.t[1] = work->field_67C;
            self->coord.t[2] = work->field_680;
            break;
    }
    Gp_ClearRec18Occupied(work->field_584);

    if (work->field_4CC.flags & 0x4000) {
        switch (func_800E0C10(work->field_4EC, &scratch->delta, 5, NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.vx.h.hi;
                self->coord.t[2] += scratch->delta.vz.h.hi;
                break;
            case 2:
                self->coord.t[0] = work->field_678;
                self->coord.t[2] = work->field_680;
                break;
        }
    }

    if (work->field_69A != 0) {
        if (--work->field_69A <= 0) {
            work->field_69A = 0;
        }
    }

    for (i = 0; i < 5; i++) {
        switch ((u32)work->field_4EC[i].key >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_69A != 0) {
                    break;
                }
                other               = Gp_ActorSlots[((u32)work->field_4EC[i].key >> 7) & 1]->extra->coords;
                scratch->delta.vx.w = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vy.w = other->coord.t[1] - self->coord.t[1];
                dz                  = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vz.w = dz;
                val                 = (scratch->delta.vx.w * self->coord.m[0][2]) + (scratch->delta.vy.w * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->field_6AA     = val >= 0;
                damage              = Gp_ComputeDamage(work->field_4EC[i].key,
                                                       SquareRoot0((scratch->delta.vx.w * scratch->delta.vx.w) + (scratch->delta.vy.w * scratch->delta.vy.w) + (scratch->delta.vz.w * scratch->delta.vz.w)),
                                                       0, 0);
                kind                = Gp_GetIdParam0(work->field_4EC[i].key);
                if (work->field_6CE != 0 && work->field_6AA == 1 && work->field_6B8 == 0) {
                    if (work->field_4EC[i].key & 0x8000) {
                        if (D_actor_105700_80148F94[work->field_4EC[i].key & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (D_actor_105700_80148F38[work->field_4EC[i].key & 0x7F] != 0) {
                        hit              = 1;
                        work->field_6D0 -= damage;
                    }
                    if (hit == 1) {
                        if (work->field_6D0 <= 0) {
                            work->field_6A6        = 9;
                            work->field_6CE        = 0;
                            work->field_6D2        = 1;
                            work->field_6A8        = 0;
                            work->field_5E4.flags &= 0x7FFF;
                            if (work->field_690 != NULL) {
                                work->field_690->task->state = 3;
                                work->field_690              = NULL;
                            }
                        }
                        func_800DA6E8(&enemy->node, 0, 0);
                        cooldown = Gp_GetIdParam2(work->field_4EC[i].key);
                        if (cooldown > 0) {
                            work->field_69A = cooldown;
                        }
                        break;
                    }
                } else {
                    work->field_6CE = 0;
                    if ((kind & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[3], 2, NULL);
                    }
                }
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[3], 0, NULL);
                    }
                    if (work->field_6E0 == 0) {
                        result = 1;
                    }
                }
                if (work->field_6C4 != 0 && (work->field_4EC[i].key & 0x8000)) {
                    damage >>= 2;
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78((GpObj40*)enemy, work->field_4EC[i].key, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    if (work->field_6B8 == 0) {
                        result = 5;
                    } else {
                        result = 6;
                    }
                } else if (enemy->hp < enemy->param->hpMax * 15 / 100) {
                    if (work->field_6B8 == 0) {
                        result = 3;
                    } else {
                        result = 4;
                    }
                }
                if (work->field_6CC != 0 || work->field_6C2 != 0) {
                    work->field_6B6 += damage;
                }
                switch (kind & 0xFFFF) {
                    case 1:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3 && work->field_6E0 == 0) {
                            result = 2;
                        }
                        break;
                    case 2:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3) {
                            Gp_SetObjFlag2((GpObj5D*)enemy, work->field_4EC[i].key, 0);
                            result = 1;
                        }
                        break;
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                if (lastId != work->field_4EC[i].key) {
                    lastId             = work->field_4EC[i].key;
                    scratch->effOfs.vx = 0;
                    scratch->effOfs.vy = 0;
                    scratch->effOfs.vz = (work->field_6AA == 1) ? 0x12C : -0x96;
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key) & 0xFFFF, &arg0->field_2C->field_8[3],
                                  &scratch->effOfs, &work->field_670);
                }
                cooldown = Gp_GetIdParam2(work->field_4EC[i].key);
                if (cooldown > 0) {
                    work->field_69A = cooldown;
                }
                switch (result) {
                    case 0:
                        if (work->field_6A6 < 2) {
                            work->field_694 = 2;
                            work->field_6A6 = 2;
                            work->field_6A8 = 0;
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        rng         = Gp_LcgState >> 16;
                        tilt        = (rng & 0x7F) + 0x40;
                        if (!(rng & 1)) {
                            tilt = -tilt;
                        }
                        work->field_688.vx = tilt;
                        byte1              = (s16)rng >> 8;
                        val                = (byte1 & 0x7F) + 0x40;
                        if (!(byte1 & 1)) {
                            val = -val;
                        }
                        work->field_688.vy = val;
                        work->field_6B4    = 1;
                        break;
                    case 1:
                        work->field_6A6        = 8;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 2:
                        work->field_6A6        = 9;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 3:
                        work->field_6A6        = 0xB;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 4:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xC;
                            work->field_6A8 = 0;
                        }
                        break;
                    case 5:
                        work->field_6A6        = 0xD;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 6:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xE;
                            work->field_6A8 = 0;
                        }
                        break;
                }
                if (result != 0 && work->field_690 != NULL) {
                    work->field_690->task->state = 3;
                    work->field_690              = NULL;
                }
                break;
            case 3:
                part                = &arg0->field_2C->field_8[3];
                x                   = part->workm.t[0] - work->field_4EC[i].point.vx;
                scratch->delta.vx.w = x;
                y                   = part->workm.t[1] - work->field_4EC[i].point.vy;
                scratch->delta.vy.w = y;
                z                   = part->workm.t[2] - work->field_4EC[i].point.vz;
                scratch->delta.vz.w = z;
                push                = work->field_4EC[i].depth - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped             = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, &scratch->push);
                }
                break;
        }
    }

    if (maxPush > 0) {
        self->coord.t[0] += (maxPush * scratch->push.vx) >> 12;
        self->coord.t[2] += (maxPush * scratch->push.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_4EC);
    if (work->field_604[0].flags & 1) {
        work->field_5E4.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(work->field_604);
    }
    work->field_6B2 = 0;
    if (Gp_CountRec18Hi(work->field_4B4, 0x10000) != 0) {
        part               = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[4];
        scratch->effOfs.vx = part->workm.t[0];
        scratch->effOfs.vy = part->workm.t[1];
        scratch->effOfs.vz = part->workm.t[2];
        scratch->target.vx = self->workm.t[0];
        scratch->target.vy = self->workm.t[1];
        scratch->target.vz = self->workm.t[2];
        if (Actor00300_Fn04B14(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x40;
}

/// The `.align 3` pad after the function's 7-entry reaction jump table.
const s32 D_actor_105700_80131E68 = 0;

/// Per-frame tick of the approach cycle, sharing the `field_6A8` state with
/// `func_actor_105700_801341CC` and `func_actor_105700_80136AE0`; the same body
/// as `Actor02000_Fn00AEC` of `actor_102000` (see `overlay_dup_index.py find
/// func_actor_105700_80132944`). State 0 drains the `field_6DA` budget by
/// `field_69C` (0 while `field_698` is under the per-animation entry of
/// `D_actor_105700_801372EC`, 0x14 once past it) and runs the proximity cue
/// every frame; when the budget runs out it switches to animation 4 and state
/// 1. State 1 waits for `field_698` to reach 0x60, then either falls back to
/// animation 2 (budget left) or starts the lunge: animation 3, state 2, a fresh
/// budget of 1000 per unit of the placement record's `variant`, and `field_6A2` /
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
                    param           = spawn->field_3C->variant;
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
    head[-1].vx               = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
    delta->vy                 = 0;
    dz                        = Player_Status.coordMtx->t[2] - self->coord.t[2];
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
