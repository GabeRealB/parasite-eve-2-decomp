#include "common.h"

#include "decomp/common.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/tmd.h"
#include "main/session.h"
#include "main/fs.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"

#include <psyq/abs.h>

#include "actors/actor_102300.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102300_80131E80;

/// First frame of each animation, indexed by `Actor102300Work::field_694`;
/// the state handlers offset it to get the frames their cues fire on.
extern s16 D_actor_102300_80135D64[];
/// The `Gp_PackPair` entry the lunge parks in the work block's 0x5E4 node.
extern GpU16Pair D_actor_102300_801477E4;
/// Base sound id of the lunge cue, ORed with the enemy's id nibble.
extern s32 D_actor_102300_80147918;

/// The `GpPairSrcE` the enemy parks in its own `field_50` slot.
extern GpPairSrcE D_actor_102300_801477F8;
/// Per-room voice-stream sector tables, indexed by `GameSession::at4.loc.stage` then
/// `field_6`; a NULL row means this room has no cue.
extern u16* D_actor_102300_80147AA0[];
/// The overlay's own spawn table: entry 0 is this enemy, 1 and 2 the two
/// companions the setup state spawns.
extern TaskDesc D_actor_102300_80147AB8;
/// Animation bank `func_800B3F84` binds to the work block.
extern u8 D_actor_102300_80147ADC[];
/// The gameplay LCG state the setup state advances for its coin flip.
extern u32 Gp_LcgState;

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 D_actor_102300_80147808[];
extern s16 D_actor_102300_80147864[];

s32 Actor00300_Fn04B14(SVECTOR* arg0, SVECTOR* arg1);

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void func_actor_102300_80131EA4(Actor102300* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor102300Work*       work;
    GpDeltaScratch*        head;
    Actor102300HitScratch* scratch;
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
    s32                    max;

    result                                    = 0;
    maxPush                                   = 0;
    hit                                       = 0;
    lastId                                    = 0;
    work                                      = arg0->field_1C;
    head                                      = *(GpDeltaScratch**)G_SCRATCH_HEAD;
    self                                      = arg0->field_2C->coords;
    *(Actor102300HitScratch**)G_SCRATCH_HEAD -= 1;
    scratch                                   = *(Actor102300HitScratch**)G_SCRATCH_HEAD;
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
                        if (D_actor_102300_80147864[work->field_4EC[i].key & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (D_actor_102300_80147808[work->field_4EC[i].key & 0x7F] != 0) {
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
                                work->field_690->field_0->state = 3;
                                work->field_690                 = NULL;
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
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->coords[3], 2, NULL);
                    }
                }
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->coords[3], 0, NULL);
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
                } else if (max = enemy->param->hpMax, enemy->hp < max / 4) {
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
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key) & 0xFFFF, &arg0->field_2C->coords[3],
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
                    work->field_690->field_0->state = 3;
                    work->field_690                 = NULL;
                }
                break;
            case 3:
                part                = &arg0->field_2C->coords[3];
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
        part               = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords[4];
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

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013290C);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80132AF0);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80132C2C);

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300", D_actor_102300_80131E80);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133008);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133100);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013332C);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801334B8);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801336C4);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133840);

/// Per-frame tick for the enemy's charge, sharing `field_6A8` with the rest of
/// the overlay and measuring the offset to the player through a 0x10-byte
/// `G_SCRATCH_HEAD` block. State 0 waits out the wind-up: from frame 0x47 it
/// mirrors `field_6D0` into `field_6CE` and commits to the charge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, zeroes
/// the cycle counter `field_6B6` on frame 0x46, and hands over to state 1 on
/// animation 6 once the animation is past its start frame plus 0x52. State 1
/// drives the charge, aiming `field_6A4` at the player each frame, switching to
/// state 2 on animation 7 within 1500 units and on animation 4 once the heading
/// has drifted more than 0x100 from `field_6A2`. State 2 raises the 0x5E4
/// node's 0x8000 flag and queues the cue on frame 0xD, drops the flag on frame
/// 0x1E, and from frame 0x3B picks animation 8 (back to state 0) within 3000
/// units or animation 4 otherwise. A `field_6B6` of 0x4C at any point aborts
/// the whole cycle back to animation 8.
void func_actor_102300_80133C10(Actor102300* arg0)
{
    s16              state;
    s16              diff;
    s16              turn;
    s32*             scratch;
    s32              dx;
    s32              dz;
    s32              dxAim;
    s32              dzAim;
    s32              dxHold;
    s32              dzHold;
    s32              dist;
    s32              sound;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->field_1C;
    state                 = work->field_6A8;
    self                  = arg0->field_2C->coords;
    switch (state) {
        case 0:
            if ((work->field_698 >= 0x47) && (work->field_6CE              = (s16)(work->field_6D0 > 0),
                                              ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]),
                                              dz                           = Player_Status.coordMtx->t[2] - self->coord.t[2],
                                              delta->vz                    = dz,
                                              dx                           = ((VECTOR*)(head - 0x10))->vx,
                                              ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0;
            if (work->field_698 == 0x46) {
                work->field_6B6 = 0;
                work->field_6CC = 1;
            }
            if ((work->field_698 >= 0x47) && (work->field_6B6 >= 0x4C)) {
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
                break;
            }
            if (work->field_698 >= (D_actor_102300_80135D64[work->field_694] + 0x52)) {
                work->field_6A8 = 1;
                work->field_694 = 6;
            }
            break;
        case 1:
            work->field_69C              = 0x84;
            work->field_69E              = 0;
            ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(Player_Status.coordMtx->t[2] - self->coord.t[2]);
            work->field_6A4              = (s16)(ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF);
            dxAim                        = ((VECTOR*)(head - 0x10))->vx;
            dzAim                        = delta->vz;
            dist                         = SquareRoot0((dxAim * dxAim) + (dzAim * dzAim));
            if (work->field_6B6 >= 0x4C) {
                goto reset;
            }
            if (dist < 0x5DC) {
                work->field_6A8 = 2;
                work->field_694 = 7;
                work->field_69C = 0;
            } else {
                diff = (ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF) - work->field_6A2;
                turn = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (turn > 0x100) {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0;
            if ((work->field_698 < 0xD) && (work->field_6B6 >= 0x4C)) {
            reset:
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_6CE       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
                break;
            }
            if (work->field_698 == 0xD) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.key   = Gp_PackPair(&D_actor_102300_801477E4, 1);
                sound                 = D_actor_102300_80147918 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan                   = (s8)Gp_GetObjPan((GpObj38*)self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)self));
            }
            if (work->field_698 == 0x1E) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            if (work->field_698 >= 0x3B) {
                work->field_6CE = 0;
                dxHold          = Player_Status.coordMtx->t[0] - self->coord.t[0];
                delta->vx       = dxHold;
                dzHold          = Player_Status.coordMtx->t[2] - self->coord.t[2];
                delta->vz       = dzHold;
                if (SquareRoot0((dxHold * dxHold) + (dzHold * dzHold)) < 0xBB8) {
                    work->field_6A6 = 4;
                    work->field_6A8 = 0;
                    work->field_694 = 8;
                } else {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                }
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

/// Per-frame tick for the enemy's lunge cycle, sharing the `field_6A8` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte `G_SCRATCH_HEAD` block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, aims
/// `field_6A4` at them every frame, raises the 0x5E4 node's 0x8000 flag on
/// frame 0x20 and queues the cue on frame 0x21, then hands over to state 1 on
/// animation 9 once the animation is past frame 0x27. State 1 waits for frame
/// 0x5E and moves on to state 2 on animation 4.
void func_actor_102300_801340B0(Actor102300* arg0)
{
    s16              startFrame;
    s16              state;
    s16              frame;
    s32*             scratch;
    s32              dz;
    s32              sound;
    s32              dx;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->field_1C;
    state                 = work->field_6A8;
    self                  = arg0->field_2C->coords;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
            dz                           = Player_Status.coordMtx->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = D_actor_102300_80135D64[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (D_actor_102300_80135D64[work->field_694] + 0x20)) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.key   = Gp_PackPair(&D_actor_102300_801477E4, 0);
            }
            if (work->field_698 == (D_actor_102300_80135D64[work->field_694] + 0x21)) {
                sound = D_actor_102300_80147918 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan   = (s8)Gp_GetObjPan((GpObj38*)self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)self));
            }
            if (work->field_698 >= (D_actor_102300_80135D64[work->field_694] + 0x27)) {
                work->field_6A8       = 1;
                work->field_694       = 9;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x5E) {
                work->field_6A6 = 2;
                work->field_6A8 = 2;
                work->field_694 = 4;
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80134338);

/// Spawn/setup state for this enemy. Allocates the 0x6E4 work block, wires the
/// model object to the block's own light/colour matrices, primes the nineteen
/// animation slots, then spawns the two companion enemies from the overlay's
/// table (entries 2 and 1) and points each one's model at the texture page and
/// CLUT row its room's `GpAreaPlace` names.
///
/// `GpEnemy::spawnState` then picks how the enemy starts: 0 builds the full
/// object set -- the four `GpObj` nodes with their `GpRec18` tables, the voice
/// cue looked up per room in `D_actor_102300_80147AA0`, and the coin-flip in
/// `field_6C4` drawn from `Gp_LcgState` -- while 1 and 2 only prime the
/// animation state and hand straight on to the next task state.
void func_actor_102300_801346CC(GpEnemy* enemy, Actor102300* actor)
{
    Actor102300Work* work;
    TmdObject*       obj;
    TmdObject*       model;
    TmdObject*       model2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parts;
    GsCOORDINATE2*   partsA;
    GsCOORDINATE2*   partsB;
    GsCOORDINATE2*   partsC;
    GsCOORDINATE2*   effParts;
    GpAreaKey*       sessionKey;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       keyPtr;
    GpAreaKey*       keyPtr2;
    u8               areaByte0;
    u8               areaByte02;
    GpAreaRec*       rec;
    GpAreaRec*       rec2;
    GpAreaPlace*     entry;
    GpAreaPlace*     entry2;
    GpEnemy*         eff;
    GpEnemy*         eff2;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    GpAreaKey        key;
    s32              i;
    s32              one;
    s32              kind;
    s32              idx;
    s32              idx2;
    s32              param;
    u32              lcg;

    obj   = actor->field_2C;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    actor->field_1C            = work;
    obj->flags                 = 0;
    coord->flg                 = 0;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x17;
    work->field_66C            = &D_actor_102300_80147AB8;
    work->field_670.coord      = &actor->field_2C->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->anim, D_actor_102300_80147ADC, obj, work->poses, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }

    eff        = Gp_SpawnEnemyFromTable(&D_actor_102300_80147AB8, 2, 0, enemy);
    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    model      = eff->task->extra;
    idx        = enemy->placeKey >> 12;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    keyPtr   = &key;
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    SOFT_DEF_REG(keyPtr);
    keyPtr       = &key;
    rec          = Gp_GetNestedAreaRec(keyPtr);
    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    SOFT_DEF_REG(keyPtr);

    eff2        = Gp_SpawnEnemyFromTable(&D_actor_102300_80147AB8, 1, 0, enemy);
    sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
    model2      = eff2->task->extra;
    idx2        = enemy->placeKey >> 12;
    key.stage   = sessionKey2->stage;
    key.area    = sessionKey2->area;
    key.room    = sessionKey2->room;
    areaByte02  = sessionKey2->view;
    keyPtr2     = &key;
    key.view    = areaByte02;
    Gp_SyncAreaKeyIndex(keyPtr2);
    SOFT_DEF_REG(keyPtr2);
    keyPtr2       = &key;
    rec2          = Gp_GetNestedAreaRec(keyPtr2);
    entry2        = (GpAreaPlace*)((idx2 << 4) + (s32)rec2->field_0);
    model2->tpage = entry2->tpage;
    model2->clut  = entry2->clut;
    if (model2->buffer != NULL) {
        tmdProcessStream(model2);
        tmdProcessStream(model2);
    }

    one  = 1;
    kind = enemy->spawnState;
    if (kind == one) {
        goto case1;
    }
    if (kind >= 2) {
        goto ge2;
    }
    if (kind == 0) {
        goto case0;
    }
    return;
ge2:
    if (kind == 2) {
        goto case2;
    }
    return;

case0:
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    parts             = actor->field_2C->coords;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->param      = &D_actor_102300_801477F8;
    enemy->recs       = work->field_4EC;
    enemy->coord      = &parts[3];
    enemy->hp         = D_actor_102300_801477F8.hpMax;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_6AC = enemy->place->mode & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = enemy->place->variant;
        work->field_6DA = param * 1000;
    }

    tbl = D_actor_102300_80147AA0[gGameSession->at4.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->at4.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x17;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_6D0            = 0xFA;
    work->field_49C.end0.vz    = 0x1F40;
    work->field_49C.end0Radius = 0x3E8;
    work->field_49C.end1Radius = 0x5DC;
    work->field_49C.end0.vx    = 0;
    work->field_49C.end0.vy    = 0;
    work->field_49C.end1.vx    = 0;
    work->field_49C.end1.vy    = 0;
    work->field_49C.end1.vz    = 0;
    work->field_49C.recs       = work->field_4B4;
    lcg                        = (Gp_LcgState * 5) + 0x71357911;
    work->field_6C4            = ((lcg >> 16) & 1) + 1;
    Gp_LcgState                = lcg;
    partsA                     = actor->field_2C->coords;
    work->field_47C.ctx.d4rec  = &work->field_49C;
    work->field_47C.pos.vx     = 0;
    work->field_47C.pos.vy     = 0;
    work->field_47C.pos.vz     = 0;
    work->field_47C.key        = 0;
    work->field_47C.radius     = 0;
    work->field_47C.flags      = 3;
    work->field_47C.coord      = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= 0xCC00;

    partsB                   = actor->field_2C->coords;
    work->field_4CC.ctx.recs = work->field_4EC;
    work->field_4CC.pos.vx   = 0;
    work->field_4CC.pos.vy   = 0;
    work->field_4CC.pos.vz   = 0;
    work->field_4CC.key      = 0x30017;
    work->field_4CC.radius   = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.coord    = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = actor->field_2C->coords;
    work->field_564.pos.vy   = -0x226;
    work->field_564.ctx.recs = work->field_584;
    work->field_564.pos.vx   = 0;
    work->field_564.pos.vz   = 0;
    work->field_564.key      = 0;
    work->field_564.radius   = 0x226;
    work->field_564.flags    = 1;
    work->field_564.coord    = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= 0x4200;

    effParts                 = ((TmdObject*)eff2->task->extra)->coords;
    work->field_5E4.ctx.recs = work->field_604;
    work->field_5E4.pos.vx   = 0;
    work->field_5E4.pos.vy   = 0x1F4;
    work->field_5E4.pos.vz   = 0;
    work->field_5E4.key      = 0;
    work->field_5E4.radius   = 0x1F4;
    work->field_5E4.flags    = 1;
    work->field_5E4.coord    = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);
    work->field_5E4.flags &= 0x7FFF;
    actor->field_30        = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->field_30 = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->field_30 = kind;
}

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80134CC0);

/// The lunge's own tick, run out of a 0x10-byte `G_SCRATCH_HEAD` block. State
/// 0 is the wind-up: it holds `field_69C` at 0 until the animation reaches its
/// start frame, aims `field_6A4` at the player and compares it with the
/// enemy's own facing `field_6A2` - past 0x581 apart it gives up and turns
/// (animation 3, or animation 4 when `field_6DC` says it has already turned
/// once), within 0x80 it raises the body node's 0xC000 flags and commits as
/// soon as `field_6B2` reports contact. State 1 picks what to do next: inside
/// 0x8CA of the player it lunges (animation 8), otherwise it draws from
/// `Gp_LcgState` through a mask that widens by a bit each cycle and either
/// circles (animation 0xA) or walks in (animation 5). State 2 waits out the
/// recovery and state 3 the turn.
void func_actor_102300_8013509C(Actor102300* actor)
{
    s16              yaw;
    s16              yaw2;
    s16              state;
    s16              deltaYaw;
    s16              deltaYaw2;
    s16              speed;
    s32              magnitude;
    s32              magnitude2;
    s16              wrapped;
    s16              wrapped2;
    s16              angle;
    s32              dx;
    s32              dz;
    u32              random;
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = actor->field_1C;
    state                 = work->field_6A8;
    coord                 = actor->field_2C->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= D_actor_102300_80135D64[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = (u16)work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | 0xC000;
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & 0x3FFF);
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            dz              = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                random      = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = random;
                if (!((random >> 0x10) & ((1 << (work->field_6C0 + 1)) - 1)) && !(Player_Status.peStateFlags & 0x10) &&
                    work->field_6C4 != 0) {
                    work->field_6A6 = 5;
                    work->field_6A8 = 0;
                    work->field_694 = 0xA;
                    work->field_6AE = 0;
                    work->field_6C2 = 1;
                    work->field_6B6 = 0;
                    work->field_6C0++;
                } else {
                    work->field_6A6 = 3;
                    work->field_6A8 = 0;
                    work->field_694 = 5;
                    work->field_6AE = 0;
                }
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | 0xC000;
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & 0x3FFF);
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = (u16)work->field_6A4 - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    *(s32*)G_SCRATCH_HEAD += 0x10;
}
