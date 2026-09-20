#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "actors/actor_421600.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132310);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013285C);

s32 func_actor_421600_80132A00(Actor421600* arg0, s32 arg1, Actor421600Msg* arg2)
{
    Actor421600Work* work;
    GpEnemy*         enemy;
    s32              angle;
    s16              mode;

    work  = arg0->field_1C;
    enemy = arg0->field_20;

    if (arg2->field_0.word == 0x109) {
        switch (arg2->field_2.word) {
            case 1:
                work->field_EAA = work->field_EA8;
                break;
            case 2:
                if (work->field_0 == 0x26) {
                    work->field_0 = 0x26;
                }
                break;
            case 3:
                if (work->field_0 == 1) {
                    work->field_0 = 2;
                }
                break;
        }
        return 1;
    }

    work->field_E90.bytes[0] = arg2->field_0.bytes[0];
    work->field_E90.bytes[1] = arg2->field_0.bytes[1];
    work->field_E90.bytes[2] = arg2->field_2.bytes[0];

    if (arg2->field_0.word != 0x1402) {
        return 0;
    }

    switch (arg2->field_2.word) {
        case 0:
            enemy->hp = D_actor_421600_8013EF3C;
            if ((enemy->placeKey >> 12) == 0) {
                work->field_0 = 2;
            }
            return 1;

        case 1:
            mode      = enemy->placeKey >> 12;
            enemy->hp = D_actor_421600_8013EF3C;
            switch (mode) {
                case 0:
                    if (D_actor_421600_80151268 < 4) {
                        goto negstate;
                    }
                    if (work->field_0 != 0) {
                        goto tail;
                    }
                    arg0->field_2C->coords->coord.t[0] = 0x1057;
                    arg0->field_2C->coords->coord.t[2] = -0x11A3;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x400, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    work->field_0 = 0x20;
                    work->field_2 = -1;
                    goto tail;
                case 1:
                    if (D_actor_421600_80151268 < 5) {
                        goto negstate;
                    }
                    if (work->field_0 != 0) {
                        goto tail;
                    }
                    arg0->field_2C->coords->coord.t[0] = 0x1467;
                    arg0->field_2C->coords->coord.t[2] = 0x4B9;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x7BC, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    work->field_0 = 0x20;
                    work->field_2 = -1;
                    goto tail;
            }
            goto tail;
        negstate:
            work->field_0 = 0;
            work->field_2 = -1;
        tail:
            Gp_SetLightMode(enemy, 0);
            enemy->reactionFlags = 0;
            enemy->hp            = D_actor_421600_8013EF3C;
            return 1;

        case 2:
            switch (enemy->placeKey >> 12) {
                case 0:
                    if (D_actor_421600_80151268 <= 0) {
                        goto blockDE0;
                    }
                    arg0->field_2C->coords->coord.t[0] = -0xD40;
                    arg0->field_2C->coords->coord.t[2] = 0x104F;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x76C, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    Gp_SetLightMode(enemy, 0);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF3C;
                    work->field_0        = 6;
                    goto blockDE0;
                case 1:
                    if (D_actor_421600_80151268 < 2) {
                        goto blockDE0;
                    }
                    arg0->field_2C->coords->coord.t[0] = 0x138C;
                    arg0->field_2C->coords->coord.t[2] = 0x4B2;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x7BC, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    Gp_SetLightMode(enemy, 0);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF3C;
                    work->field_0        = 6;
                    goto blockDE0;
                default:
                    goto blockDE0;
            }
        blockDE0:
            if (D_actor_421600_80151268 == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            }
            return 1;

        case 3:
            if (work->field_E9C == 1) {
                work->field_E9C = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            }
            if (work->field_0 != 0x14 && work->field_0 != 0x11 && work->field_0 != 0x15 &&
                work->field_0 != 0x16 && work->field_0 != 0 && work->field_0 != 8) {
                work->field_0 = 5;
                work->field_2 = -1;
            }
            return 1;

        case 9:
            work->field_0 = 0;
            work->field_2 = -1;
            return 1;

        default:
            return 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132EC0);

/// Moves an interior coordinate to the nearest padded X or Z edge.
/// Returns 1 when moved, or 0 when already outside the rectangle.
s32 func_actor_421600_80133334(GsCOORDINATE2* arg0)
{
    s16 dx;
    s16 dz;
    s32 adx;
    s32 adz;
    s32 x;
    s32 z;
    s32 z2;

    x = arg0->coord.t[0];
    if ((x >= -0xC4D) && (x < 0xD16)) {
        z = arg0->coord.t[2];
        if (z < 0xC4E) {
            if (z >= -0xC4D) {
                if ((0xD16 - x) > (x + 0xC4E)) {
                    dx = -(((GpCoordXZ*)arg0)->field_18 + 0xCE4);
                } else {
                    dx = 0xDAC - ((GpCoordXZ*)arg0)->field_18;
                }
                z2 = arg0->coord.t[2];
                if ((0xC4E - z2) > (z2 + 0xC4E)) {
                    dz = -(((GpCoordXZ*)arg0)->field_20 + 0xCE4);
                } else {
                    dz = 0xCE4 - ((GpCoordXZ*)arg0)->field_20;
                }
                adx = ABS(dx);
                adz = ABS(dz);
                if (adz < adx) {
                    arg0->coord.t[2] += dz;
                } else {
                    arg0->coord.t[0] += dx;
                }
                arg0->flg = 0;
                return 1;
            }
        }
    }
    return 0;
}

void func_actor_421600_80133444(GsCOORDINATE2* arg0)
{
    SVECTOR                  vec;
    SVECTOR*                 dir;
    Actor421600ArenaScratch* blk;
    u8*                      head;
    s32                      outside;
    u32                      spad_a;
    u32                      spad_b;

    if ((u32)(arg0->coord.t[0] - 0x1F5) < 0x3E7) {
        if (arg0->coord.t[2] < 0x1F4) {
            if (arg0->coord.t[2] < -0x1F4) {
                head                           = *(void**)G_SCRATCH_HEAD;
                blk                            = (Actor421600ArenaScratch*)(head - 0xC);
                spad_a                         = (u32)PSX_SCRATCH;
                *(void**)((u8*)spad_a + 0x3FC) = blk;
                vec.vx                         = (u16)arg0->coord.t[0] - 0x3E8;
                vec.vy                         = 0;
                vec.vz                         = (u16)arg0->coord.t[2] + 1;
                blk->field_0                   = vec.vx;
                dir                            = &vec;
                blk->field_4                   = dir->vz;
                blk->field_8                   = 0x2D0;
                blk->field_0                   = blk->field_0 * blk->field_0;
                blk->field_4                   = blk->field_4 * blk->field_4;
                blk->field_8                   = blk->field_8 * blk->field_8;
                spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
                *(void**)((u8*)spad_b + 0x4)   = head;
                outside                        = blk->field_0 + blk->field_4 >= blk->field_8;
                if (outside != 0) {
                    return;
                }
                VectorNormalSS(dir, dir);
                gte_lddp(0x2BC);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                arg0->coord.t[0] = vec.vx + 0x3E8;
                arg0->coord.t[2] = vec.vz;
                arg0->flg        = 0;
            }
        }
    }
}

static __inline__ s16 Actor421600_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor421600AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor421600AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

static __inline__ s16 Actor421600_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor421600AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor421600AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

s32 func_actor_421600_801335BC(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                      head;
    Actor421600AvoidScratch* s;
    s16                      diff;
    s16                      t;
    s32                      mag;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor421600AvoidScratch);
    s                     = (Actor421600AvoidScratch*)*(u8**)G_SCRATCH_HEAD;

    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x38));
    VectorNormalSS((SVECTOR*)(head - 0x38), (SVECTOR*)(head - 0x38));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind  = recs[s->i].key & 0xFFFF0000;
        s->flags = recs[s->i].key & 0x80;
        switch (s->kind) {
            case 0x10000:
                if (s->flags == 0) {
                    s->blocked = 1;
                }
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = Actor421600_BearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = Actor421600_BearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12_real();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor421600AvoidScratch);
    return s->blocked != 0;
}

void func_actor_421600_80133B30(Actor421600* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    Actor421600AnimWork* work;
    s32                  blend;
    s32                  invBlend;
    s16                  index;
    s16                  next;

    index = 1;
    work  = (Actor421600AnimWork*)arg0->field_1C;
    do {
        switch (index) {
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0x800;
                break;
            case 3:
            case 4:
            case 5:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            work->blendSlots[index].rate = (u8)work->field_83A;
            work->slots[index].rate      = (s8)(work->field_832 - 3);
            func_800B3448(&work->anim, index, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, index, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(&work->anim, index, &pose, &blendPose, blend, invBlend);
        } else {
            work->slots[index].rate = (s8)(work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, index);
        }
        next  = index + 1;
        index = next;
    } while (next < 0x12);
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133CAC);

void func_actor_421600_80134604(Actor421600* arg0)
{
    s32                  index;
    u32                  table;
    Actor421600AnimWork* seekWork;
    Actor421600AnimWork* resetWork;
    Actor421600AnimWork* turnWork;
    Actor421600AnimWork* secondaryWork;
    Actor421600AnimWork* tickWork;
    Actor421600AnimWork* work;
    s32                  targetAngle;
    s32                  animation;
    s32                  updatedTurn;
    s16                  currentTurn;
    s16                  thirdAngle;
    s16                  state;
    s32                  currentAngle;
    s16                  angle;
    s32                  seekSlotIndex;
    s32                  resetSlotIndex;
    s32                  secondarySlotIndex;
    s32                  tickSlotIndex;
    s32                  signedTurn;
    s32                  sound;
    s32                  resetIndex;
    s32                  secondaryIndex;
    s32                  tickIndex;
    s32                  seekIndex;
    s32                  delta;
    s8*                  tickSlot;
    s8*                  seekSlot;
    s8*                  resetSlot;
    s8*                  secondarySlot;
    s32                  pan;
    s32                  currentAngleBits;
    u16                  originalTurn;
    s32                  targetAngleBits;
    u16                  updatedTurnBits;
    s32                  clampedAngle;
    s32                  targetTurn;

    work  = (Actor421600AnimWork*)arg0->field_1C;
    state = (s16)work->field_828;
    if (state == 1) {
        if (work->field_82C != (s16)work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)&D_actor_421600_80150DB4;
            seekSlot  = (s8*)&work->anim.slots;
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_832;
                animation      = (s16)seekWork->field_82E;
                seekSlot      += 0x28;
                index          = seekWork->field_82C * 0x19;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = (s16)seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(work->field_848, 0U, 0x48U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = (s8*)&work->anim.slots;
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_832;
            resetSlot      += 0x28;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)(s16)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = (s16)resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork            = (Actor421600AnimWork*)arg0->field_1C;
        secondaryIndex           = 1;
        secondarySlot            = (s8*)&secondaryWork->anim.slots;
        secondaryWork->field_83A = 0x20;
        secondaryWork->field_83C = 0x800;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_83A;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if ((s16)work->field_82A == 0) {
        tickWork  = (Actor421600AnimWork*)arg0->field_1C;
        tickIndex = 1;
        tickSlot  = (s8*)&tickWork->anim.slots;
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        func_actor_421600_80133B30(arg0);
        if (work->blendSlots[1].flags & 0x100) {
            work->field_82A = 0;
        }
    }
    targetAngle      = (s16)work->field_840;
    currentAngle     = (s16)work->field_844;
    targetAngleBits  = work->field_840;
    currentAngleBits = (u16)work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_844 = targetAngleBits;
    }
    angle        = (s16)work->field_844;
    clampedAngle = (u16)work->field_844;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        ActorsShared80132808(&arg0->field_2C->coords[2], thirdAngle);
        arg0->field_2C->coords[2].flg = 0;
        ActorsShared80132808(&arg0->field_2C->coords[3], thirdAngle);
        arg0->field_2C->coords[3].flg = 0;
        ActorsShared80132808(&arg0->field_2C->coords[4], (s16)clampedAngle / 2);
        arg0->field_2C->coords[4].flg = 0;
    }
    if (((s16)work->field_82E == 0) && (work->field_0 == 0x26)) {
        Gfx_RotMatrixX(&arg0->field_2C->coords[4].coord, 0x280, 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
    }
    turnWork     = (Actor421600AnimWork*)arg0->field_1C;
    targetTurn   = turnWork->field_83E;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->field_842;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->field_842 = (s16)((u16)turnWork->field_842 + 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->field_842;
    updatedTurnBits = (u16)turnWork->field_842;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->field_842 = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    ActorsShared80132808(&arg0->field_2C->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    arg0->field_2C->coords[10].flg = 0;
    sound                          = func_actor_421600_80133CAC(arg0, (Actor421600Work*)work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
    }
}

static __inline__ void Actor421600_BindMatrices(Actor421600* actor)
{
    Actor421600Work* work;
    TmdObject*       obj;
    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->lightMtx = &work->field_E04;
    obj->colorMtx = &work->field_E24;
}

void func_actor_421600_80134AD4(GpEnemy* enemy, Actor421600* actor)
{
    SVECTOR          dir;
    s32              kind;
    SVECTOR*         v;
    VECTOR           pos;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor421600Work* mapped;
    Actor421600Work* work;
    GpObj*           body;
    GpObj*           head;
    s16              extent;
    s32              linkKind;
    GpObj*           linkObj;
    root            = actor->field_2C->coords;
    obj             = actor->field_2C;
    work            = memCalloc(0xEB0, 0);
    actor->field_1C = work;
    if (work == 0) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    actor->exitCallback = func_actor_421600_8013E668;
    Actor421600_BindMatrices(actor);
    enemy->field_4    = &actor->field_2C->coords[0].coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->field_2C->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->reactionFlags = 0;
    enemy->hp            = (s16)D_actor_421600_8013EF38.hpMax;
    enemy->param         = &D_actor_421600_8013EF38;
    enemy->recs          = &work->field_90C;
    func_800B3F84(&((Actor421600AnimWork*)work)->anim, D_actor_421600_80151028, obj, &((Actor421600AnimWork*)work)->slots[18], ((Actor421600AnimWork*)work)->slots);
    func_800B3F84(&((Actor421600AnimWork*)work)->blendAnim, D_actor_421600_80151028, obj, &((Actor421600AnimWork*)work)->blendSlots[18], ((Actor421600AnimWork*)work)->blendSlots);
    work->field_828 = 2;
    work->field_82A = 0;
    work->field_82E = 1;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_421600_80134604(actor);
    work->field_B6C.ctx.recs = &work->field_B8C;
    work->field_B6C.coord    = root;
    work->field_B6C.pos.vx   = 0;
    work->field_B6C.pos.vy   = -0x11C;
    work->field_B6C.pos.vz   = 0;
    work->field_B6C.key      = 0x30001;
    work->field_B6C.radius   = 0x12C;
    work->field_B6C.flags    = 1;
    Gp_LinkObj(2, &work->field_B6C);
    work->field_CCC.end0.vy = -0x180;
    work->field_CCC.end1.vy = -0x180;
    extent                  = 0x2BC;
    linkKind                = 2;
    linkObj                 = &work->field_CAC;
    __asm__("" : "+r"(linkKind), "+r"(linkObj) : "m"(work->field_CCC.end0.vy), "m"(work->field_CCC.end1.vy));
    __asm__("" : "+r"(extent) : "m"(work->field_CCC.end0.vy), "m"(work->field_CCC.end1.vy));
    work->field_CCC.end0Radius   = 0x12C;
    mapped                       = work;
    mapped->field_CCC.end1Radius = 0x12C;
    __asm__("" : "+r"(extent), "+m"(mapped->field_CCC.end1Radius) : "r"(work->field_CE4));
    mapped->field_CCC.end1.vz   = extent;
    mapped->field_CAC.ctx.d4rec = &mapped->field_CCC;
    mapped->field_CCC.end0.vx   = 0;
    mapped->field_CCC.end0.vz   = 0;
    mapped->field_CCC.end1.vx   = 0;
    mapped->field_CCC.recs      = work->field_CE4;
    (&mapped->field_CAC)->coord = root;
    mapped->field_CAC.pos.vx    = 0;
    mapped->field_CAC.pos.vy    = 0;
    mapped->field_CAC.pos.vz    = 0;
    mapped->field_CAC.key       = 0x30001;
    mapped->field_CAC.radius    = 0;
    mapped->field_CAC.flags     = 3;
    mapped->field_B6C.flags     = mapped->field_B6C.flags | 0x4000;
    Gp_LinkObj(linkKind, linkObj);
    mapped->field_CAC.flags = mapped->field_CAC.flags | 0x4000;
    Gp_InitRec18Table(mapped->field_CE4, 0xC, 0);
    Gp_InitRec18Table(mapped->field_B6C.ctx.recs, 0xC, 0);
    body           = &mapped->field_8EC;
    body->coord    = &actor->field_2C->coords[2];
    body->ctx.recs = &mapped->field_90C;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x30001;
    body->radius   = 0x19C;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);
    head           = &mapped->field_A2C;
    head->coord    = &actor->field_2C->coords[10];
    head->ctx.recs = &mapped->field_A4C;
    head->pos.vx   = 0;
    head->pos.vy   = 0;
    head->pos.vz   = 0;
    head->key      = 0x30001;
    head->radius   = 0x100;
    head->flags    = 1;
    Gp_LinkObj(2, head);
    head->flags |= 0x8000;
    Gp_InitRec18Table(head->ctx.recs, 0xC, 0);
    mapped->field_A2C.pos.vx = 0;
    mapped->field_A2C.pos.vy = 0;
    mapped->field_A2C.pos.vz = -0x100;
    mapped->field_14         = 0;
    mapped->field_C[0].x     = actor->field_2C->coords->coord.t[0];
    mapped->field_C[0].z     = actor->field_2C->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->field_2C->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(5000);
    gte_ldsv(v);
    gte_gpf12_real();
    gte_stsv(v);
    mapped->field_C[1].x = actor->field_2C->coords->coord.t[0] + dir.vx;
    mapped->field_C[1].z = actor->field_2C->coords->coord.t[2] + dir.vz;
    mapped->field_E88    = 3;
    mapped->field_E7C    = 0;
    mapped->field_E80    = 1;
    mapped->field_E84    = 0;
    mapped->field_E8C    = 1;
    actor->field_24      = &D_actor_421600_80151118;
    root->sub            = &gGfxViewCoord;
    root->flg            = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    kind = actor->field_36;
    switch (kind & 0xF) {
        case 1:
            mapped->field_2 = -1;
            mapped->field_0 = 0;
            break;

        case 2:
            mapped->field_2 = -1;
            mapped->field_0 = 0x21;
            break;

        case 0:

        default:
            mapped->field_2 = -1;
            mapped->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }

    switch (((Task*)actor)->spawnArg1 & 0xF) {
        case 2:
            mapped->field_EA2 = D_actor_421600_8013EF48[0].field_2;
            mapped->field_EA4 = D_actor_421600_8013EF48[0].field_0;
            mapped->field_EA6 = D_actor_421600_8013EF48[0].field_4;
            mapped->field_EA8 = D_actor_421600_8013EF48[0].field_6;
            break;

        case 1:
            mapped->field_EA2 = D_actor_421600_8013EF48[2].field_2;
            mapped->field_EA4 = D_actor_421600_8013EF48[2].field_0;
            mapped->field_EA6 = D_actor_421600_8013EF48[2].field_4;
            mapped->field_EA8 = D_actor_421600_8013EF48[2].field_6;
            break;

        case 0:

        default:
            mapped->field_EA2 = D_actor_421600_8013EF48[1].field_2;
            mapped->field_EA4 = D_actor_421600_8013EF48[1].field_0;
            mapped->field_EA6 = D_actor_421600_8013EF48[1].field_4;
            mapped->field_EA8 = D_actor_421600_8013EF48[1].field_6;
            break;
    }

    D_801153F4.field_2      = 8;
    D_actor_421600_80151268 = 8;
    actor->state++;
}

/// Picks one of twelve hit positions out of `D_actor_421600_801510B8` by
/// damage magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the
/// model part that entry names. Same body as the shared
/// `Actor00100_Fn03340` / `func_actor_401300_80134BA4` pair, but it reads this
/// overlay's own table, so it stays a per-overlay copy.
void func_actor_421600_801350BC(Actor421600* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor421600Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_421600_801510B8[0];
                break;
            case 1:
                *sc = D_actor_421600_801510B8[1];
                break;
            case 2:
                *sc = D_actor_421600_801510B8[2];
                break;
            case 3:
                *sc = D_actor_421600_801510B8[3];
                break;
            default:
                *sc = D_actor_421600_801510B8[4];
                break;
        }
    } else if (mag >= 0x601) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_421600_801510B8[5];
                break;
            case 1:
                *sc = D_actor_421600_801510B8[6];
                break;
            default:
                *sc = D_actor_421600_801510B8[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_421600_801510B8[8];
        } else {
            *sc = D_actor_421600_801510B8[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_421600_801510B8[10];
        } else {
            *sc = D_actor_421600_801510B8[11];
        }
    }
    work->field_890.coord      = &arg0->field_2C->coords[sc->pad];
    work->field_890.spawnArgLo = 0x100;
    work->field_890.spawnArgHi = 2;
    work->field_898            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->coords[sc->pad], &work->field_898, &work->field_890);
    *(u32*)G_SCRATCH_HEAD += 8;
}

static __inline__ s32 Actor421600_FindDamageHit(GpRec18* records,
                                                SVECTOR* pos)
{
    s16 i;
    for (i = 0; i < 12; i++) {
        if (!records[i].key)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key;
        }
    }
    return 0;
}

void func_actor_421600_801354D8(Actor421600* arg0)
{
    s32                       callAngle;
    s32                       debugMode;
    PlayerStatus*             config = &Player_Status;
    s16                       effect;
    s16                       delta;
    s16                       z;
    s32                       state5;
    s16                       damageState;
    s16                       deathState;
    s16                       hurtState;
    s16                       poisonState;
    s16                       state0;
    s16                       state1;
    s16                       state2;
    s16                       state3;
    s16                       state4;
    s16                       wrapped;
    s16                       hitState;
    s16                       nextDeathState;
    GsCOORDINATE2*            objectCoord;
    s32                       tickDamage;
    s32                       dxSquared;
    s32                       dySquared;
    s32                       yaw;
    s32                       deathSound;
    s32                       hurtSound;
    s32                       hitSound;
    s32                       doubleDamage;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       distance;
    SVECTOR*                  hitPos;
    s32                       soundBase;
    s32                       deathPan;
    s32                       hurtPan;
    s32                       hitPan;
    u16                       totalDamage;
    u32                       kind;
    Actor421600Work*          work;
    GpEnemy*                  enemy;
    Actor421600DamageScratch* scratch;
    void*                     head;
    enemy = arg0->field_20;
    work  = arg0->field_1C;
    if (enemy->hp > 0) {
        head              = *(void**)G_SCRATCH_HEAD;
        scratch           = (*(Actor421600DamageScratch**)G_SCRATCH_HEAD =
                       (Actor421600DamageScratch*)head - 1);
        scratch->field_20 = Actor421600_FindDamageHit(
            &work->field_90C, (SVECTOR*)&scratch->field_18);
        if (scratch->field_20 == 0) {
            hitPos            = (SVECTOR*)&scratch->field_18;
            scratch->field_20 = Actor421600_FindDamageHit(&work->field_A4C, hitPos);
        }
        if (scratch->field_20 != 0) {
            scratch->field_2E = -1;
            work->field_E64   = Gp_GetIdParam2(scratch->field_20);
            kind              = Gp_GetIdParam0(scratch->field_20) & 0xFFFF;
            switch (kind) {
                case 0:
                case 6:
                case 7:
                case 8:
                case 9:
                    state0 = work->field_0;
                    if (state0 == 24 || state0 == 38 || state0 == 39 || state0 == 1) {
                        if (work->field_0 == 0x20) {
                            work->field_0 = 3;
                        } else {
                            work->field_0 = 0x1C;
                        }
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    if (work->field_82A == 0) {
                        work->field_E66 = 0U;
                    }
                    state1 = work->field_0;
                    if ((state1 != 0x22) && (state1 != 0x14) && (state1 != 0x11) &&
                        (state1 != 0x15) && (state1 != 0x16) && (state1 != 4) &&
                        (state1 != 0xB) && (state1 != 0x24) && (state1 != 7)) {
                        work->field_82A = 1;
                        work->field_838 = 9;
                        work->field_836 = 2;
                    }
                    break;
                case 4:
                case 5:
                    state2 = work->field_0;
                    if (state2 == 4 || state2 == 11 || state2 == 20 || state2 == 17) {
                        work->field_0 = 11;
                        work->field_2 = -1;
                    } else if (state2 != 21 && state2 != 7) {
                        work->field_0 = 20;
                    }
                    break;
                case 2:
                    state3 = work->field_0;
                    if (state3 == 33 || state3 == 4 || state3 == 11 || state3 == 17) {
                        work->field_0 = 11;
                        work->field_2 = -1;
                    } else if (state3 != 21 && state3 != 7) {
                        work->field_0 = 20;
                    }
                    Gp_SetObjFlag2((GpObj5D*)enemy, scratch->field_20, 0);
                    break;
                case 3:
                    state4 = work->field_0;
                    if ((state4 == 0x18) || (state4 == 0x26) || (state4 == 1) ||
                        (state4 == 0x20)) {
                        work->field_0 = 0x1C;
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    Gp_SetObjFlag4((GpObj5C*)enemy, scratch->field_20, 0);
                    break;
                case 1:
                    state5 = work->field_0;
                    if (state5 != 7) {
                        if (state5 == 4 || state5 == 11 || state5 == 20 || state5 == 17 ||
                            (state5 == 36 && (s16)work->field_6 < 10)) {
                            hitState      = 11;
                            work->field_0 = hitState;
                        } else if (state5 != 21 && state5 != 0 && state5 != 22 &&
                                   state5 != 7) {
                            hitState      = 20;
                            work->field_0 = hitState;
                        }
                    }
                    break;
            }
            dx                          = config->coordMtx->t[0] - arg0->field_2C->coords->coord.t[0];
            dxSquared                   = dx * dx;
            scratch->field_0            = dx;
            dy                          = config->coordMtx->t[1] - arg0->field_2C->coords->coord.t[1];
            dySquared                   = dy * dy;
            scratch->field_4            = dy;
            dz                          = config->coordMtx->t[2] - arg0->field_2C->coords->coord.t[2];
            scratch->field_8            = dz;
            distance                    = SquareRoot0(dxSquared + dySquared + (dz * dz));
            scratch->field_28           = distance;
            scratch->field_24           = Gp_ComputeDamage(scratch->field_20, distance, 0, 0);
            arg0->field_2C->coords->flg = 0;
            Gp_UpdateCoord(arg0->field_2C->coords);
            scratch->field_10 = (u16)arg0->field_2C->coords->workm.t[0];
            scratch->field_12 = (u16)arg0->field_2C->coords->workm.t[1];
            scratch->field_14 = (u16)arg0->field_2C->coords->workm.t[2];
            scratch->field_10 =
                (u16)(scratch->field_18 - arg0->field_2C->coords->workm.t[0]);
            scratch->field_12 =
                (u16)(scratch->field_1A - arg0->field_2C->coords->workm.t[1]);
            z                 = scratch->field_1C - arg0->field_2C->coords->workm.t[2];
            scratch->field_14 = (u16)z;
            yaw               = ratan2((s16)scratch->field_10, z);
            objectCoord       = arg0->field_2C->coords;
            delta =
                yaw - ratan2(-objectCoord->workm.m[2][0], objectCoord->workm.m[2][2]);
            wrapped           = delta;
            scratch->field_2C = delta;
            if (delta < 0) {
                while (1) {
                    if (wrapped >= -0x800)
                        break;
                    wrapped += 0x1000;
                }
            } else {
                while (1) {
                    if (wrapped <= 0x800)
                        break;
                    wrapped -= 0x1000;
                }
            }
            callAngle         = wrapped;
            scratch->field_2C = callAngle;
            func_actor_421600_801350BC(arg0, callAngle, scratch->field_20);
            work->field_844 = 0;
            work->field_840 = 0;
            if (Gp_RollEnemyChance(enemy, scratch->field_20, 0) != 0) {
                scratch->field_2E = 0;
                scratch->field_24 = (s32)(scratch->field_24 * 4);
            }
            damageState = work->field_0;
            if ((damageState == 4) || (damageState == 0xB) || (damageState == 0x11) ||
                (damageState == 0x24)) {
                doubleDamage      = scratch->field_24 * 2;
                scratch->field_24 = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->field_2E = 3;
                }
            }
            func_800E2C78((GpObj40*)enemy, scratch->field_20, scratch->field_24, 0);
            effect = scratch->field_2E;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, arg0->field_2C->coords + 2, effect, 0);
            }
            scratch->field_24 = (s32)(scratch->field_24 * 2);
            enemy->hp         = (s16)((u16)enemy->hp - (u16)scratch->field_24);
            func_800DA6E8(&enemy->node, scratch->field_24, 0);
            totalDamage     = work->field_E66 + (u16)scratch->field_24;
            work->field_E66 = totalDamage;
            if (enemy->hp <= 0) {
                D_actor_421600_80151268 -= 1;
                if ((Gp_GetIdParam0(scratch->field_20) & 0xFFFF) == 4) {
                    nextDeathState = 8;
                    goto setDeathState;
                }
                deathState = work->field_0;
                if (deathState == 33 || deathState == 17 || deathState == 11 ||
                    deathState == 4) {
                    work->field_0 = 11;
                    work->field_2 = -1;
                } else if (deathState == 7) {
                    work->field_0 = 21;
                    deathSound    = (((u16)enemy->placeKey >> 12) << 8) | 0x40010008;
                    deathPan      = (s8)Gp_GetObjPan(arg0->field_2C->coords);
                    SndEvt_EnqueueType6(deathSound, deathPan,
                                        (s8)gpGetObjDepth(arg0->field_2C->coords));
                } else {
                    hurtSound = (((u16)enemy->placeKey >> 12) << 8) | 0x40010008;
                    hurtPan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
                    SndEvt_EnqueueType6(hurtSound, hurtPan,
                                        (s8)gpGetObjDepth(arg0->field_2C->coords));
                    nextDeathState = 20;
                setDeathState:
                    work->field_0 = nextDeathState;
                }
                work->field_8E8 = 9;
                work->field_8E9 = 1;
                work->field_8EA = 3;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&work->field_8E8, 0x7DB);
            } else {
                if ((s16)totalDamage >= 0x47) {
                    hurtState = work->field_0;
                    if ((hurtState != 0x21) && (hurtState != 0x14) &&
                        (hurtState != 0x11) && (hurtState != 7) &&
                        (work->field_E90.bytes[2] != 1)) {
                        soundBase     = 0x40010008;
                        work->field_0 = 0x14;
                    } else {
                        goto normalHitSound;
                    }
                } else {
                normalHitSound:
                    soundBase = 0x40010007;
                }
                hitSound = (((u16)enemy->placeKey >> 0xC) << 8) | soundBase;
                hitPan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
                SndEvt_EnqueueType6(hitSound, hitPan,
                                    (s8)gpGetObjDepth(arg0->field_2C->coords));
            }
            debugMode = D_80072729;
            if (debugMode == 1) {
                enemy->hp       = 0x64;
                work->field_838 = 9;
                work->field_82A = (s16)debugMode;
                work->field_836 = 2;
            }
        }
        if (enemy->reactionFlags & 0xC) {
            scratch->field_24 = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->reactionFlags = (u8)(enemy->reactionFlags & 0xF3);
            }
            enemy->hp  = (s16)((u16)enemy->hp - (u16)scratch->field_24);
            tickDamage = scratch->field_24;
            if (tickDamage != 0) {
                func_800DA6E8(&enemy->node, tickDamage, 0);
                if (enemy->hp <= 0) {
                    D_actor_421600_80151268 -= 1;
                    poisonState              = work->field_0;
                    if ((poisonState != 4) && (poisonState != 0xB) &&
                        (poisonState != 0x11)) {
                        work->field_0 = 0xC;
                    } else {
                        work->field_0 = 0x15;
                    }
                } else {
                    if (work->field_0 == 0x1C) {
                        work->field_0 = 0x26;
                    }
                    work->field_82A = 1;
                    work->field_838 = 0x12;
                    work->field_836 = 2;
                }
            }
        }
        *(u32*)G_SCRATCH_HEAD += 0x30;
    }
}

void func_actor_421600_80135F6C(Actor421600* arg0)
{
    SVECTOR          offset;
    Actor421600Work* work;
    TmdObject*       obj;
    s32              nextState;
    s32              state;
    u16              flags;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        flags = work->field_B6C.flags;
        TOUCH_REG(flags);
        work->field_832       = 0x10;
        work->field_B6C.flags = flags | 0x4000;
        state                 = work->field_82E;
        do {
            nextState = 0xD;
        } while (0);
        if (state == nextState) {
            nextState = 1;
        } else {
            nextState = 2;
        }
        work->field_828 = nextState;
        SOFT_BARRIER();
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_421600_80134604(arg0);
        return;
    }
    func_actor_421600_80134604(arg0);
    if ((work->field_68 & 2) && (work->field_82E == 0xD)) {
        work->field_82E = 1;
        work->field_828 = 1;
    }
    if (work->field_68 & 0x100) {
        if (work->field_82E == 0xF) {
            work->field_828 = 2;
            work->field_82E = 0x10;
        }
        func_actor_421600_80134604(arg0);
    }
    if (work->field_82E == 0xE) {
        if ((u32)((work->field_5A & 0x3FF) - 8) < 2U) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, arg0->field_2C->coords + 7, 0x80002300, &offset);
            }
        }
        if ((work->field_5A & 0x3FF) == 8) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, arg0->field_2C->coords + 7, 0x80003400, &offset);
            }
        }
    }
}

/// Takes an `SVECTOR` off `G_SCRATCH_HEAD`, runs `coord` through
/// `Gfx_MatrixCol2` and `VectorNormalSS`, scales the resulting direction by
/// `amount` with the gte `gpf 12` (`-0x55` / `-0xC8` are the two aims
/// `func_actor_421600_80138D24` picks) and folds the scaled X/Y/Z back into
/// the coordinate's translation before handing the slot back. Same body and
/// same gte op as `Actor00100_MoveForward`, sized for one `SVECTOR` of scratch.
///
/// Keeping the alloc/use/free block whole matters: `G_SCRATCH_HEAD` is the bare
/// constant 0x1F8003FC, so an access written straight into a caller that touches
/// it twice CSEs the address into a callee-saved register, while the inliner
/// re-expands this body and each access keeps the `lw`/`sw` against the constant
/// that the assembler turns into the `lui`/`lw` (and `lui $at`/`sw`) pair the
/// ROM has.
static __inline__ void Actor421600_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

static __inline__ s16 Actor421600_Zone(GsCOORDINATE2* coord)
{
    s32 x, z, ix, iz;
    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x >= 0xD49)
        ix = 3;
    else if (x > 0)
        ix = 2;
    else
        ix = x >= -0xC7F;
    iz = 0;
    if (z < 0xBB9) {
        iz = 1;
        if (z <= 0) {
            iz = 3;
            if (z >= -0xBB7)
                iz = 2;
        }
    }
    return D_actor_421600_801511C0[ix | (iz * 4)];
}

void func_actor_421600_80136138(Actor421600* arg0)
{
    Actor421600Work*        work;
    Actor421600SeekScratch *head, *blk;
    GpEnemy*                ctx;
    TmdObject*              obj;
    GsCOORDINATE2 *         coord2, *coord3, *coord4;
    s16                     playerZone, zone;
    s16                     nextZone;
    s16                     angle;
    s32                     wrapped;
    s32                     state;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 0;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    playerZone = Actor421600_Zone(Gp_ActorSlots[0]->extra->coords);
    zone       = Actor421600_Zone(arg0->field_2C->coords);
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    if (playerZone != zone) {
        switch ((s16)(playerZone - 1)) {
            case 0:
            case 1:
                if (zone >= 1 && zone <= 3) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 2:
                if (zone >= 1 && zone <= 6) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 3:
            case 4:
                if (zone >= 3 && zone <= 6) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 5:
                if (zone >= 3 && zone <= 9) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 6:
            case 7:
                if (zone >= 6 && zone <= 9) {
                    state = 0x26;
                    SOFT_TOUCH_REG(state);
                    work->field_0 = state;
                    return;
                }
                break;
            case 8:
                if (zone >= 6 && zone <= 11) {
                    state = 0x26;
                    SOFT_TOUCH_REG(state);
                    work->field_0 = state;
                    return;
                }
                break;
            case 9:
            case 10:
                if (zone >= 9 && zone <= 11) {
                    state = 0x26;
                    SOFT_TOUCH_REG(state);
                    work->field_0 = state;
                    return;
                }
                break;
            case 11:
                if (zone >= 0xB) {
                    work->field_0 = 0x26;
                    return;
                }
                if (zone >= 0xC) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
        }
    } else {
        work->field_0 = 0x26;
        return;
    }
    func_actor_421600_80134604(arg0);
    head                                       = *(Actor421600SeekScratch**)G_SCRATCH_HEAD;
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD -= 1;
    blk                                        = head - 1;
    if (zone > playerZone)
        nextZone = zone - 1;
    else
        nextZone = zone + 1;
    head[-1].vec.vx = D_actor_421600_80151158[nextZone].vx;
    blk->vec.vy     = D_actor_421600_80151158[nextZone].vy;
    blk->vec.vz     = D_actor_421600_80151158[nextZone].vz;
    blk->vec.vx     = blk->vec.vx - (u16)arg0->field_2C->coords->coord.t[0];
    blk->vec.vy     = 0;
    blk->vec.vz     = blk->vec.vz - (u16)arg0->field_2C->coords->coord.t[2];
    coord2          = arg0->field_2C->coords;
    angle           = ratan2(blk->vec.vx, blk->vec.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x21)
        blk->angle = 0x20;
    if (blk->angle < -0x20)
        blk->angle = -0x20;
    work->field_83E = blk->angle;
    coord3          = arg0->field_2C->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = arg0->field_2C->coords;
        Actor421600_MoveForward(coord4, 0x14);
    }
    func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &blk->vec);
    func_actor_421600_80133334(arg0->field_2C->coords);
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD += 1;
    arg0->field_2C->coords->flg                = 0;
}

/// Rebuild `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled by `y` on Y and left at 1.0 on X and Z, through
/// a 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
/// Same body as `Actor401300_RescaleYaw` / `ActorsShared80135a60`, per-axis
/// instead of uniform, which is why `y` arrives already narrowed to `s16`.
static __inline__ void Actor421600_ShrinkCoord(GsCOORDINATE2* coord, s16 y)
{
    void*                     head;
    Actor421600ShrinkScratch* blk;
    s16                       ang;
    u16                       m22;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor421600ShrinkScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = 0x1000;
    blk->scale.vy = y;
    blk->scale.vz = 0x1000;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] =
        *(u16*)&((Actor421600ShrinkScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    m22                     = *(u16*)&blk->m.m[2][2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
    coord->flg              = 0;
    coord->coord.m[2][2]    = m22;
}

/// Shrink tick: on the live-actor edge it drops the model's dirty flag, clears
/// the 0x4000 bit on the 0xB6C node, marks the enemy's list node and resets
/// `field_6` / `field_8A0`. Then it counts frames in `field_6` and, from frame
/// 0xB on, scales the model's coordinate Y by `0x1000 - (frame - 0xA) * 0x6B`
/// until that factor runs out at 0, through `Actor421600_ShrinkCoord`. The
/// frame counter also drives the light state: 1 sets modes 0 and 1, 20 (and
/// the fall-through from 1) sets mode 2, 38 sets `field_C` 0x80 and the
/// `field_0` state 0x16. Counting stops at 0x401.
void func_actor_421600_801366F4(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s32              t;
    u16              tick;

    work = arg0->field_1C;
    obj  = arg0->field_2C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj->flags             = 0;
        work->field_B6C.flags &= 0xBFFF;
        ctx->node.flags        = 1;
        work->field_6          = 0;
        work->field_8A0        = 0;
    }
    if ((s16)work->field_6 < 0x401) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
        switch ((s16)tick) {
            case 1:
                Gp_SetLightMode((GpObj4C*)ctx, 0);
                Gp_SetLightMode((GpObj4C*)ctx, 1);
                /* fallthrough */
            case 20:
                arg0->field_2C->flags = 2;
                Gp_SetLightMode((GpObj4C*)ctx, 2);
                break;
            case 22:
                break;
            case 38:
                arg0->field_2C->flags = 0x80;
                work->field_0         = 0x16;
                break;
        }
        if ((s16)work->field_6 >= 0xB) {
            t = ((s16)work->field_6 - 10) * 0x6B;
            if (t < 0x1000) {
                Actor421600_ShrinkCoord(arg0->field_2C->coords, 0x1000 - t);
            } else {
                Actor421600_ShrinkCoord(arg0->field_2C->coords, 0);
            }
        }
    }
}

void func_actor_421600_801369A0(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    GpEnemy*         found;
    s32              hi;
    s32              id;
    s32              stageAreaId;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        if (work->field_E9C == 1) {
            work->field_2 = -1;
            work->field_6 = 0;
            return;
        }
        work->field_6 = 0;
        do {
        } while (0);
        if (D_801153F4.field_2 >= 2U) {
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 1);
        }
        if (D_actor_421600_80151268 <= 0) {
            Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            work->field_8A0 = 1;
            work->field_0   = 0;
            return;
        }
    }
    if ((s16)work->field_6 < 0x80) {
        work->field_6 = work->field_6 + 1;
    }
    if (work->field_EAC > 0) {
        work->field_EAC = work->field_EAC - 1;
    }
    if (work->field_E90.bytes[2] != 2) {
        work->field_0 = 0;
        return;
    }
    found = NULL;
    switch (ctx->placeKey >> 0xC) {
        case 0:
            hi    = gGameSession->at4.loc.stage << 8;
            id    = gGameSession->at4.loc.area | 0x1000;
            found = (GpEnemy*)Gp_FindWorkById(id | hi);
            break;
        case 1:
            stageAreaId = (gGameSession->at4.loc.stage << 8) | gGameSession->at4.loc.area;
            found       = (GpEnemy*)Gp_FindWorkById(stageAreaId);
            break;
    }
    if (found != NULL) {
        if (found->hp > 0) {
            if (D_actor_421600_80151268 == 1) {
                work->field_0 = 0;
            }
        }
        if ((D_actor_421600_80151268 >= 2) || ((found->hp <= 0) && (D_actor_421600_80151268 == 1))) {
            switch (ctx->placeKey >> 0xC) {
                case 0:
                    arg0->field_2C->coords->coord.t[0] = -0xD40;
                    arg0->field_2C->coords->coord.t[2] = 0x104F;
                    arg0->field_2C->coords->flg        = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x76C, 1);
                    Gp_SetLightMode((GpObj4C*)ctx, 0);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF3C;
                    work->field_0      = 6;
                    break;
                case 1:
                    arg0->field_2C->coords->coord.t[0] = 0x138C;
                    arg0->field_2C->coords->coord.t[2] = 0x4B2;
                    arg0->field_2C->coords->flg        = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x7BC, 1);
                    Gp_SetLightMode((GpObj4C*)ctx, 0);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF3C;
                    work->field_0      = 6;
                    break;
            }
        }
    }
}

/// Scans the 12 0x18-byte records at 0xCE4 for one whose `field_4` carries the
/// 0x100000 kind, stopping at the first empty record. The 5-record twin of
/// this body is `Actor00100_HasRecord10`, which reads the same halves of
/// `field_0` instead; `func_actor_421600_80138D24` picks its aim scale with it.
static __inline__ s16 Actor421600_HasRecord10(Actor421600* arg0)
{
    Actor421600Work* work  = arg0->field_1C;
    s16              found = 0;
    s16              i;

    for (i = 0; i < 0xC; i++) {
        if (!work->field_CE4[i].key) {
            break;
        }
        if ((work->field_CE4[i].key & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

static __inline__ s16 Actor421600_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

static __inline__ s32 Actor421600_OutsideRadius(SVECTOR* pos, s16 radius)
{
    Actor421600RadiusScratch* head;
    Actor421600RadiusScratch* scratch;
    head                                         = *(Actor421600RadiusScratch**)G_SCRATCH_HEAD;
    scratch                                      = head - 1;
    *(Actor421600RadiusScratch**)G_SCRATCH_HEAD  = scratch;
    scratch->x                                   = pos->vx;
    scratch->z                                   = pos->vz;
    scratch->radius                              = radius;
    scratch->x                                  *= scratch->x;
    scratch->z                                  *= scratch->z;
    scratch->radius                             *= scratch->radius;
    *(Actor421600RadiusScratch**)G_SCRATCH_HEAD += 1;
    return scratch->x + scratch->z >= scratch->radius;
}

void func_actor_421600_80136C88(Actor421600* arg0)
{
    Actor421600Work*        work;
    GpEnemy*                ctx;
    Actor421600Work*        move;
    Actor421600SeekScratch* head;
    Actor421600SeekScratch* scratch;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          playerCoord;
    GsCOORDINATE2*          turnCoord;
    GpRec18*                records;
    u16                     angle;
    s16                     delta;
    s32                     value;
    s32                     magnitude;
    s16                     yaw;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        ctx             = arg0->field_20;
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        work->field_6           = 0;
        work->field_CCC.end1.vz = 0x26C;
        return;
    }
    head            = *(Actor421600SeekScratch**)G_SCRATCH_HEAD;
    scratch         = (*(Actor421600SeekScratch**)G_SCRATCH_HEAD = head - 1);
    move            = (Actor421600Work*)work;
    head[-1].vec.vx = move->field_C[move->field_14].x - arg0->field_2C->coords->coord.t[0];
    scratch->vec.vy = 0;
    scratch->vec.vz = move->field_C[move->field_14].z - arg0->field_2C->coords->coord.t[2];
    if (!Actor421600_OutsideRadius(&scratch->vec, 0xA0) || (s16)work->field_6 >= 0x15) {
        if (move->field_14 == 0)
            move->field_14 = 1;
        else
            move->field_14 = 0;
        work->field_6 = 0;
    }
    func_actor_421600_80134604(arg0);
    coord           = arg0->field_2C->coords;
    angle           = ratan2(scratch->vec.vx, scratch->vec.vz);
    delta           = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    value           = Actor421600_NormalizeYaw(delta);
    scratch->angle  = value;
    work->field_840 = value;
    if (scratch->angle >= 0x11)
        scratch->angle = 0x10;
    if (scratch->angle < -0x10)
        scratch->angle = -0x10;
    work->field_83E = scratch->angle;
    turnCoord       = arg0->field_2C->coords;
    yaw             = (u16)scratch->angle + ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]);
    scratch->angle  = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, yaw, 1);
    records = &work->field_90C;
    if ((s16)work->field_82A == 0) {
        if (Actor421600_HasRecord10(arg0)) {
            Actor421600_MoveForward(arg0->field_2C->coords, 20);
        } else {
            Actor421600_MoveForward(arg0->field_2C->coords, 20);
        }
        records = &work->field_90C;
    }
    func_actor_421600_80132310(arg0->field_2C->coords, records, 0xC, &scratch->vec);
    if (func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC) == 1) {
        magnitude = abs((s16)work->field_840);
        if (magnitude < 0x80)
            work->field_6 = (u16)work->field_6 + 1;
    }
    arg0->field_2C->coords->flg = 0;
    playerCoord                 = arg0->field_2C->coords;
    scratch->vec.vx             = Player_Status.coordMtx->t[0] - playerCoord->coord.t[0];
    scratch->vec.vy             = Player_Status.coordMtx->t[1] - playerCoord->coord.t[1];
    scratch->vec.vz             = Player_Status.coordMtx->t[2] - playerCoord->coord.t[2];
    if (!Actor421600_OutsideRadius(&scratch->vec, 2000)) {
        work->field_0 = 0x1C;
    } else if (!Actor421600_OutsideRadius(&scratch->vec, 4000)) {
        coord          = arg0->field_2C->coords;
        angle          = ratan2(scratch->vec.vx, scratch->vec.vz);
        delta          = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        value          = Actor421600_NormalizeYaw(delta);
        scratch->angle = value;
        value          = abs(value);
        if (value < 0x300) {
            work->field_0 = 0x1C;
        }
    }
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801373D4);

/// Spawn/aim tick: on the live-actor edge it resets the display objects, runs
/// the arena vector through the gte rotation, stores the world X (`field_8A4`)
/// and Z (`field_8AC`) it produces, plays the spawn sound, then bumps the
/// `field_6` frame counter. On frame 0xF of the 7 pose it enqueues the second
/// sound and, outside a live-game state, spawns the effect on the player's
/// coordinate. The `field_E90` id picks `field_0` on the 0x100 flag, 5 for
/// 0x11402 and 0x1F otherwise -- the same pair `func_actor_421600_8013E9D8`
/// writes.
void func_actor_421600_8013848C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    SVECTOR*         head;
    SVECTOR*         vec;
    SVECTOR*         gteVec;
    TmdObject*       obj;
    Task*            player;
    s32              x;
    s32              z;
    s32              sound;
    s32              pan;
    s32              eventPan;
    s32              state;
    u16              tick;

    work                       = arg0->field_1C;
    player                     = gameGetPtrSlot(3);
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 2;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    ctx                        = arg0->field_20;
    gteVec                     = vec;
    if (work->field_4 != 0) {
        TOUCH_REG(gteVec);
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_82E        = 5;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_832        = work->field_834;
        func_actor_421600_80134604(arg0);
        Gfx_MatrixCol2(&arg0->field_2C->coords->coord, vec);
        work->field_6 = 0;
        VectorNormalSS(vec, vec);
        if (work->field_E9E >= 0xFA1) {
            work->field_E9E = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        gte_gpf12_real();
        gte_stsv(gteVec);
        x               = head[-2].vx;
        work->field_8A8 = 0;
        work->field_8A4 = x;
        z               = vec->vz;
        work->field_8B4 = 7;
        work->field_8B6 = 1;
        work->field_8AC = z;
        pan             = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(7, (s32)pan, (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
    }
    tick          = work->field_6 + 1;
    work->field_6 = tick;
    if (((s16)tick == 0xF) && (work->field_8B4 == 7)) {
        sound    = (((u16)ctx->placeKey >> 0xC) << 8) | 0x4001000A;
        eventPan = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, (s32)eventPan, (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
        if (Gp_State1C->roomEffectMode == 2) {
            Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->coords + 1, 0x80003A00, NULL);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        state = work->field_E90.word & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 0x1F;
        }
        work->field_0 = state;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

static __inline__ void Actor421600_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

static __inline__ s16 Actor421600_PositionYaw(Actor421600* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor421600_ConfigPositionDelta(config, actor->field_2C->coords, pos);
    coord = actor->field_2C->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor421600_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

void func_actor_421600_80138750(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR*         head;
    SVECTOR*         vec;
    s32              x, z;
    s16              yaw;
    s32              outside;
    s32              state;

    head = *(SVECTOR**)G_SCRATCH_HEAD;
    vec  = (*(SVECTOR**)G_SCRATCH_HEAD = head - 2);
    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_82E        = 5;
        work->field_83E        = 0;
        work->field_6          = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_832        = work->field_834;
        Actor421600_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(0x20);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        x                       = head[-2].vx;
        work->field_8A8         = 0;
        work->field_8A4         = x;
        z                       = vec->vz;
        work->field_8B4         = 7;
        work->field_8B6         = 1;
        work->field_CCC.end1.vz = 0x320;
        work->field_8AC         = z;
        if ((work->field_E90.word & 0xFFFFFF) == 0x11402) {
            work->field_0 = 5;
        }
    }
    work->field_6 += 1;
    func_actor_421600_80134604(arg0);
    state = (s16)work->field_82E;
    switch (state) {
        case 5:
            if (work->field_68 & 0x100) {
                Actor421600_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, vec);
                outside = Actor421600_OutsideRadius(vec, 2000);
                if (outside) {
                    work->field_0 = 0x26;
                } else {
                    work->field_0 = 0x1F;
                }
            }
            break;
        case 3:
            yaw       = Actor421600_PositionYaw(arg0, vec, &Player_Status);
            vec[1].vz = yaw;
            if (Actor421600_HasRecord10(arg0)) {
                Actor421600_MoveForward(arg0->field_2C->coords, 85);
            } else {
                Actor421600_MoveForward(arg0->field_2C->coords, 200);
            }
            if (func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC)) {
                work->field_0 = 0x23;
            }
            if ((s16)work->field_6 >= 0x15) {
                work->field_828 = 1;
                work->field_82A = 0;
                work->field_82E = 5;
                work->field_832 = work->field_834;
            }
            break;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

/// Aim tick: on the live-actor edge it re-arms the model the way
/// `func_actor_421600_8013848C` does -- buffers reallocated, clip 0x10,
/// `field_82E` 6, the 0xB6C node's 0x4000 flag up -- with `field_8` and the
/// 0xCD8 offset it owns reseeded, then, while `field_6` is inside 9..0x18 and
/// `field_8` below 5, walks the 0xB8C table and counts a retry for every hit.
/// The 0xCE4 records decide which way the model is aimed: one carrying the
/// 0x100000 kind turns it by `-0x55`, none by `-0xC8`, through
/// `Actor421600_MoveForward`. Outside that frame window, and in both aim arms,
/// the 0xB8C walk is what runs.
void func_actor_421600_80138D24(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              found;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx             = arg0->field_20;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 6;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_CCC.end1.vz = -0x320;
    }
    work->field_6++;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 2;
    }
    if (((u32)(work->field_6 - 9) < 0x10) && ((s16)work->field_8 < 5)) {
        if (func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC) != 0) {
            work->field_8++;
        }
        found = Actor421600_HasRecord10(arg0);
        if (found != 0) {
            Actor421600_MoveForward(arg0->field_2C->coords, -0x55);
        } else {
            Actor421600_MoveForward(arg0->field_2C->coords, -0xC8);
        }
    } else {
        func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    }
    arg0->field_2C->coords->flg = 0;
}

/// Re-arms the model the way `func_actor_421600_8013848C` does -- buffers
/// reallocated, clip 0x10, `field_82E` 2, the 0xB6C node's 0x4000 flag up --
/// then walks the 0xB8C `GpRec18` table through `func_actor_421600_8013285C`.
/// Takes two `SVECTOR`s off `G_SCRATCH_HEAD` and fills the XZ offset of the
/// model coordinate from `Player_Status.coordMtx` (the camera target matrix),
/// forms the yaw difference against the model's own facing (row 2 of its
/// matrix), wraps it into `[-0x800, 0x800]` into `field_840` and re-aims the
/// coordinate with `Gfx_RotMatrixY`. Ends by writing the view index into
/// `field_0` on the two view transitions.
///
/// The coordinate is read twice into two locals: `coord` only feeds the offset
/// and dies before the first `ratan2`, while `coord2` is live across it, so GCC
/// 2.8.1 keeps them in a caller-saved and a callee-saved register respectively.
/// One local assigned twice is one pseudo with one live range and costs a sixth
/// saved register.
void func_actor_421600_8013903C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR*         head;
    SVECTOR*         vec;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    s16              angle;
    s32              view;

    head                        = *(SVECTOR**)G_SCRATCH_HEAD;
    *(SVECTOR**)G_SCRATCH_HEAD -= 2;
    vec                         = head - 2;
    work                        = arg0->field_1C;
    ctx                         = arg0->field_20;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 2;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6 = 0;
    }
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    arg0->field_2C->coords->flg = 0;
    coord                       = arg0->field_2C->coords;
    head[-2].vx                 = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    vec->vy                     = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    vec->vz                     = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    coord2                      = arg0->field_2C->coords;
    angle                       = ratan2(head[-2].vx, vec->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    work->field_840 = angle;
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, (s16)ratan2(vec->vx, vec->vz), 1);
    arg0->field_2C->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (((u16)ctx->placeKey >> 0xC) == 0) {
        view = Gp_GetViewIndex() & 0xFF;
        if (view == 3) {
            work->field_0 = view;
        }
    }
    if ((((u16)ctx->placeKey >> 0xC) == 1) && ((Gp_GetViewIndex() & 0xFF) == 8)) {
        work->field_0 = 3;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

/// Re-arms the model buffers and the 0x828 motion block the way
/// `func_actor_421600_8013848C` does, with clip 0x10 and pose 7, then walks the
/// two `GpRec18` movement tables 0x90C and 0xA4C through
/// `func_actor_421600_80132310`. `field_0` becomes 0x22 when either walk
/// reports a hit, and again when the squared XZ offset from `D_80073B8C` is
/// under the squared 0x5DC radius, so the actor only takes the state while the
/// camera target is close. Ends by clearing the model's `flg`.
void func_actor_421600_801392A8(Actor421600* arg0)
{
    Actor421600Work*         work;
    GpEnemy*                 ctx;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    MATRIX*                  target;
    void*                    head;
    Actor421600ArenaScratch* blk;
    SVECTOR                  vec;
    SVECTOR*                 dir;
    u32                      spad_a;
    u32                      spad_b;
    s32                      outside;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        ctx             = arg0->field_20;
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 2;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 7;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (((func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &vec) << 0x10) != 0) || ((func_actor_421600_80132310(arg0->field_2C->coords, &work->field_A4C, 0xC, &vec) << 0x10) != 0)) {
        work->field_0 = 0x22;
    }
    target                         = D_80073B8C;
    coord                          = arg0->field_2C->coords;
    vec.vx                         = (u16)target->t[0] - (u16)coord->coord.t[0];
    dir                            = &vec;
    dir->vy                        = (u16)target->t[1] - (u16)coord->coord.t[1];
    dir->vz                        = (u16)target->t[2] - (u16)coord->coord.t[2];
    head                           = *(void**)G_SCRATCH_HEAD;
    blk                            = (Actor421600ArenaScratch*)((u8*)head - 0xC);
    spad_a                         = (u32)PSX_SCRATCH;
    *(void**)((u8*)spad_a + 0x3FC) = blk;
    blk->field_0                   = vec.vx;
    blk->field_4                   = dir->vz;
    blk->field_8                   = 0x5DC;
    blk->field_0                   = blk->field_0 * blk->field_0;
    blk->field_4                   = blk->field_4 * blk->field_4;
    blk->field_8                   = blk->field_8 * blk->field_8;
    spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
    *(void**)((u8*)spad_b + 0x4)   = head;
    outside                        = blk->field_0 + blk->field_4 >= blk->field_8;
    if (outside == 0) {
        work->field_0 = 0x22;
    }
    arg0->field_2C->coords->flg = 0;
}

/// Death / respawn tick: re-arms the model buffers and the 0x828 motion block,
/// fires the 0x40010009 spawn sound and the 0x40010007 tick sound (draining
/// `field_40` by 0xF and flooring it at 1), then walks the two `GpRec18`
/// movement tables. While the id word's third byte reads 2 the actor is held
/// in the arena by clamping X -- and Z only when X was already inside -- and
/// otherwise `func_actor_421600_80133334` drags it back. Picks the state
/// `field_0` out of `field_40` / `field_4C`.
void func_actor_421600_8013947C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    s32              sound;
    s32              pan;
    s32              eventSound;
    s32              eventPan;
    s32              x;
    s32              z;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_832        = 0x10;
        work->field_82E        = 0xA;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_8EC.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        sound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010009;
        pan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
        ctx->hp -= 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        }
        eventSound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010007;
        eventPan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(eventSound, eventPan,
                            (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
    }
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_90C, 0xC);
    if (work->field_E90.bytes[2] == 2) {
        coord = arg0->field_2C->coords;
        x     = coord->coord.t[0];
        if (x > 0) {
            if (x >= 0xBEB) {
                coord->coord.t[0] = 0xB54;
            } else {
                goto block_10;
            }
        } else if (x < -0xB22) {
            coord->coord.t[0] = -0xA8C;
        } else {
        block_10:
            z = coord->coord.t[2];
            if (z > 0) {
                if (z >= 0xB23) {
                    coord->coord.t[2] = 0xA8C;
                }
            } else if (z < -0xB22) {
                coord->coord.t[2] = -0xA8C;
            }
        }
    } else {
        func_actor_421600_80133334(arg0->field_2C->coords);
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void func_actor_421600_80139718(Actor421600* arg0)
{
    s32                     radius = 0x5DC;
    Actor421600Work*        work;
    GpRec18*                record;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          coord3;
    GsCOORDINATE2*          facing3;
    GsCOORDINATE2*          facing4;
    GsCOORDINATE2*          facing5;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          facing2;
    GsCOORDINATE2*          turnCoord;
    MATRIX*                 matrix;
    Actor421600MoveScratch* scratch;
    SVECTOR*                target;
    SVECTOR*                target2;
    Actor421600MoveScratch* head;
    SVECTOR*                direction;
    Actor421600MoveScratch* head2;
    TmdObject*              obj;
    s16                     targetDelta;
    s16                     delta;
    s16                     yaw;
    s16                     delta3;
    s16                     delta4;
    s16                     delta5;
    s32                     playerX;
    s16                     delta1;
    s16                     delta2;
    s16                     targetYaw;
    s16                     z;
    s32                     magnitude;
    s32                     targetMagnitude;
    s16                     adjustedDelta;
    s32                     originalMagnitude;
    s16                     wrapped;
    s16                     wrapped2;
    s16                     wrapped3;
    s16                     wrapped4;
    s16                     wrapped5;
    s16                     wrappedYaw;
    s32                     angle3;
    s32                     angle4;
    s32                     angle5;
    s32                     angle;
    s32                     angle2;
    s32                     finalYaw;
    s32                     turnDelta;
    s32                     finalDelta;
    s32                     yawDifference;
    u16                     unsignedDelta;
    work = arg0->field_1C;
    if (work->field_4 != 0) {
        head                       = *(Actor421600MoveScratch**)G_SCRATCH_HEAD;
        obj                        = arg0->field_2C;
        scratch                    = (*(Actor421600MoveScratch**)G_SCRATCH_HEAD = head - 1);
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->field_B6C.flags |= 0x4000;

        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        coord           = arg0->field_2C->coords;
        head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
        scratch->vec.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        z               = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        scratch->vec.vz = z;
        facing          = arg0->field_2C->coords;
        angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
        delta1          = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped         = delta1;
        if (delta1 < 0) {
        wrapNegative:
            if (wrapped < -0x800) {
                wrapped += 0x1000;
                goto wrapNegative;
            }
        } else {
        wrapPositive:
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
                goto wrapPositive;
            }
        }
        work->field_840 = wrapped;
        matrix          = &scratch->matrix;
        Gfx_RotMatrixY(matrix, (s16)ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz) + 0x3E8, 1);
        Gfx_MatrixCol2(matrix, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(1000);
        gte_ldsv(&scratch->vec);
        gte_gpf12_real();
        gte_stsv(&scratch->vec);
        work->field_14                             = 0;
        work->field_C[0].x                         = (s16)((u16)scratch->vec.vx + arg0->field_2C->coords->coord.t[0]);
        *(Actor421600MoveScratch**)G_SCRATCH_HEAD += 1;
        work->field_C[0].z                         = (s16)((u16)scratch->vec.vz + arg0->field_2C->coords->coord.t[2]);
        work->field_CCC.end1.vz                    = 0x26C;
        return;
    }
    work->field_8      += 1;
    head2               = *(Actor421600MoveScratch**)G_SCRATCH_HEAD;
    scratch             = (*(Actor421600MoveScratch**)G_SCRATCH_HEAD = head2 - 1);
    head2[-1].vec.vx    = (s16)(work->field_C[work->field_14].x - arg0->field_2C->coords->coord.t[0]);
    scratch->vec.vy     = 0;
    scratch->vec.vz     = work->field_C[work->field_14].z - arg0->field_2C->coords->coord.t[2];
    coord2              = arg0->field_2C->coords;
    head2[-1].target.vx = (s16)(Player_Status.coordMtx->t[0] - coord2->coord.t[0]);
    target              = &head2[-1].target;
    target->vy          = Player_Status.coordMtx->t[1] - coord2->coord.t[1];
    target->vz          = Player_Status.coordMtx->t[2] - coord2->coord.t[2];
    if (!Actor421600_OutsideRadius(&scratch->vec, 0xA0) || (s16)work->field_6 >= 0x15) {
        facing2  = arg0->field_2C->coords;
        angle2   = ratan2((s32)head2[-1].target.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
        wrapNegative2:
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
                goto wrapNegative2;
            }
        } else {
        wrapPositive2:
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
                goto wrapPositive2;
            }
        }
        work->field_840 = wrapped2;
        if (work->field_14 == 0) {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) - 0x2EE, 1);
            work->field_14 = 1;
        } else {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x2EE, 1);
            work->field_14 = 0;
        }
        direction = &scratch->target;
        Gfx_MatrixCol2(&scratch->matrix, direction);
        VectorNormalSS(direction, direction);
        gte_lddp(2000);
        gte_ldsv(direction);
        gte_gpf12_real();
        gte_stsv(direction);
        work->field_C[work->field_14].x = (s16)((u16)scratch->target.vx + arg0->field_2C->coords->coord.t[0]);
        work->field_C[work->field_14].z = (s16)((u16)scratch->target.vz + arg0->field_2C->coords->coord.t[2]);
        work->field_6                   = 0;
    }
    func_actor_421600_80134604(arg0);
    facing3  = arg0->field_2C->coords;
    angle3   = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
    wrapNegative3:
        if (wrapped3 < -0x800) {
            wrapped3 += 0x1000;
            goto wrapNegative3;
        }
    } else {
    wrapPositive3:
        if (wrapped3 >= 0x801) {
            wrapped3 -= 0x1000;
            goto wrapPositive3;
        }
    }
    work->field_840 = wrapped3;
    facing4         = arg0->field_2C->coords;
    angle4          = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
    delta4          = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4        = delta4;
    if (delta4 < 0) {
    wrapNegative4:
        if (wrapped4 < -0x800) {
            wrapped4 += 0x1000;
            goto wrapNegative4;
        }
    } else {
    wrapPositive4:
        if (wrapped4 >= 0x801) {
            wrapped4 -= 0x1000;
            goto wrapPositive4;
        }
    }
    turnDelta         = wrapped4;
    scratch->original = (scratch->delta = (s16)turnDelta);
    delta             = scratch->delta;
    unsignedDelta     = (u16)scratch->delta;
    magnitude         = abs(scratch->delta);
    if (magnitude >= 0x601) {
        targetDelta     = (s16)work->field_840;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->delta = adjustedDelta;
        }
    }
    if (scratch->delta >= 0x21) {
        scratch->delta = 0x20;
    }
    if (scratch->delta < -0x20) {
        scratch->delta = -0x20;
    }
    work->field_83E = scratch->delta * 0x10;
    turnCoord       = arg0->field_2C->coords;
    yaw             = (u16)scratch->delta + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->delta  = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, (s32)yaw, 1);
    record = &work->field_90C;
    if ((s16)work->field_82A == 0) {
        if (Actor421600_HasRecord10(arg0)) {
            Actor421600_MoveForward(arg0->field_2C->coords, 20);
        } else {
            Actor421600_MoveForward(arg0->field_2C->coords, 20);
        }
        record = &work->field_90C;
    }
    func_actor_421600_80132310(arg0->field_2C->coords, record, 12, &scratch->vec);
    if (func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 12) == 1) {
        originalMagnitude = abs(scratch->original);
        if (originalMagnitude < 0x20) {
            work->field_6 += 1;
        }
    }
    arg0->field_2C->coords->flg = 0;
    coord3                      = arg0->field_2C->coords;
    scratch->target.vx          = (s16)(Player_Status.coordMtx->t[0] - coord3->coord.t[0]);
    target2                     = &scratch->target;
    target2->vy                 = Player_Status.coordMtx->t[1] - coord3->coord.t[1];
    target2->vz                 = Player_Status.coordMtx->t[2] - coord3->coord.t[2];
    if ((s16)work->field_8 > (s16)work->field_EA6) {
        if ((s16)work->field_EAA <= 0) {

            if (Actor421600_OutsideRadius(&scratch->target, radius)) {
                if (!Actor421600_OutsideRadius(&scratch->target, 0x1F40) && (s16)work->field_8 >= 0x1C3) {
                    facing5  = arg0->field_2C->coords;
                    angle5   = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
                    delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                    wrapped5 = delta5;
                    if (delta5 < 0) {
                    wrapNegative5:
                        if (wrapped5 < -0x800) {
                            wrapped5 += 0x1000;
                            goto wrapNegative5;
                        }
                    } else {
                    wrapPositive5:
                        if (wrapped5 >= 0x801) {
                            wrapped5 -= 0x1000;
                            goto wrapPositive5;
                        }
                    }
                    finalDelta     = wrapped5;
                    scratch->delta = (s16)finalDelta;
                    finalDelta     = abs(finalDelta);
                    if (finalDelta < 0x300) {
                        goto changeState;
                    }
                }
            } else {
            changeState:
                work->field_0 = 0x1C;
            }
            playerX            = -((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0];
            scratch->playerYaw = ratan2((s32)playerX, (s32)((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            targetYaw          = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x800;
            wrappedYaw         = targetYaw;
            scratch->yaw       = targetYaw;
            if (targetYaw < 0) {
            wrapYawNegative:
                if (wrappedYaw < -0x800) {
                    wrappedYaw += 0x1000;
                    goto wrapYawNegative;
                }
            } else {
            wrapYawPositive:
                if (wrappedYaw >= 0x801) {
                    wrappedYaw -= 0x1000;
                    goto wrapYawPositive;
                }
            }
            finalYaw      = wrappedYaw;
            scratch->yaw  = (s16)finalYaw;
            yawDifference = finalYaw - scratch->playerYaw;
            if (yawDifference < 0) {
                yawDifference = -yawDifference;
            }
            if (yawDifference >= 0x601) {
                work->field_0 = 0x1C;
            }
        } else {
            work->field_EAA -= 1;
        }
    }
    func_actor_421600_80133334(arg0->field_2C->coords);
    *(Actor421600MoveScratch**)G_SCRATCH_HEAD += 1;
    arg0->field_2C->coords->flg                = 0;
}

void func_actor_421600_8013A404(Actor421600* arg0)
{
    Actor421600Work* temp_s0;
    GsCOORDINATE2*   temp_v0_2;
    s32              temp_a0;
    s32              temp_a1;
    s32              var_a0;
    s32              var_v1;
    u32              temp_v0;
    u8               temp_v1;

    temp_s0 = arg0->field_1C;
    if (temp_s0->field_4 != 0) {
        temp_v0          = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = temp_v0;
        temp_s0->field_6 = temp_s0->field_EA4 + ((temp_v0 >> 0x10) & 0xF);
    }
    temp_s0->field_6 -= 1;
    func_actor_421600_80134604(arg0);
    if ((s16)temp_s0->field_6 < 0) {
        temp_v1 = temp_s0->field_E90.bytes[2];
        if ((temp_v1 == 1) || (temp_v1 == 3)) {
            temp_s0->field_0 = 5;
        } else if (temp_v1 == 2) {
            temp_v0_2 = arg0->field_2C->coords;
            temp_a0   = temp_v0_2->coord.t[0];
            temp_a1   = temp_v0_2->coord.t[2];
            if (temp_a0 >= 0xD49) {
                var_a0 = 3;
            } else if (temp_a0 > 0) {
                var_a0 = 2;
            } else {
                var_a0 = temp_a0 >= -0xC7F;
            }
            var_v1 = 0;
            if (temp_a1 < 0xBB9) {
                var_v1 = 1;
                if (temp_a1 <= 0) {
                    var_v1 = 3;
                    if (temp_a1 >= -0xBB7) {
                        var_v1 = 2;
                    }
                }
            }
            if (D_actor_421600_801511C0[var_a0 | (var_v1 * 4)] >= 0xB) {
                temp_s0->field_0 = 0x24;
            } else {
                temp_s0->field_0 = 6;
            }
        } else {
            temp_s0->field_0 = 0x24;
        }
    }
}

void func_actor_421600_8013A554(Actor421600* arg0)
{
    SVECTOR                   effect;
    s16                       aimZ;
    s16                       fallbackZ;
    s32                       fallbackAngle;
    s16                       fallbackDelta;
    s32                       moveAngle;
    s16                       moveDelta;
    s32                       playerX;
    s32                       facingAngle;
    s16                       facingDelta;
    s32                       aimAngle;
    s16                       aimDelta;
    s16                       targetZ;
    s16                       yaw;
    s16                       nextState;
    GsCOORDINATE2*            targetCoord;
    GsCOORDINATE2*            aimCoord;
    GsCOORDINATE2*            fallbackCoord;
    GsCOORDINATE2*            fallbackFacing;
    GsCOORDINATE2*            moveCoord;
    GsCOORDINATE2*            facingCoord;
    GsCOORDINATE2*            aimFacing;
    GsCOORDINATE2*            stepCoord;
    GsCOORDINATE2*            coord;
    s32                       sound;
    s32                       spawnEffect;
    s32                       effectFlags;
    s32                       part;
    s32                       fallbackYaw;
    s32                       distance;
    s32                       closeDistance;
    s32                       farDistance;
    s32                       pan;
    TmdObject*                obj;
    GpEnemy*                  ctx;
    Task*                     player;
    Actor421600Work*          work;
    GpEnemy*                  enemy;
    Actor421600AttackScratch* head;
    Actor421600AttackScratch* scratch;

    work   = arg0->field_1C;
    enemy  = arg0->field_20;
    player = gameGetPtrSlot(3);
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx             = arg0->field_20;
        ctx->node.flags = 0;
        Gp_ArmStateF0(1);
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 3;
        work->field_83E        = 0;
        work->field_B6C.flags  = (u16)(work->field_B6C.flags | 0x4000);
        func_actor_421600_80134604(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_E9E = 0;
        sound           = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010006;
        pan             = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(arg0->field_2C->coords));
        return;
    }
    head          = *(Actor421600AttackScratch**)G_SCRATCH_HEAD;
    scratch       = (*(Actor421600AttackScratch**)G_SCRATCH_HEAD = head - 1);
    coord         = arg0->field_2C->coords;
    scratch->zone = Actor421600_Zone(coord);
    if ((func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC) != 0) && ((s16)work->field_6 >= 0xB)) {
        work->field_0 = 5;
    }
    if ((func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &scratch->vec) << 0x10) != 0 && work->field_82E == 3) {
        work->field_8E4 = 8;
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&work->field_8D0, 0) == 0) {
            playerX            = -((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][0];
            scratch->playerYaw = ratan2(playerX, ((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][2]);
            targetCoord        = arg0->field_2C->coords;
            scratch->vec.vx    = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
            scratch->vec.vy    = (s16)(Player_Status.coordMtx->t[1] - targetCoord->coord.t[1]);
            targetZ            = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
            scratch->vec.vz    = targetZ;
            yaw                = ratan2(scratch->vec.vx, targetZ) + 0x800;
            scratch->yaw       = yaw;
            scratch->yaw       = Actor421600_NormalizeYaw(yaw);
            facingCoord        = arg0->field_2C->coords;
            facingAngle        = ratan2(scratch->vec.vx, scratch->vec.vz);
            facingDelta        = facingAngle - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
            scratch->delta     = Actor421600_NormalizeYaw(facingDelta);
            distance           = scratch->yaw - scratch->playerYaw;
            distance           = abs(distance);
            if (distance < 0x400) {
                work->field_E7C = (s32)&D_actor_421600_80151090;
            } else {
                work->field_E7C = (s32)&D_actor_421600_801510A4;
                scratch->yaw    = (s16)((u16)scratch->yaw + 0x800);
            }
            work->field_8C8.vx = 0;
            work->field_8C8.vy = (u16)scratch->yaw;
            work->field_8C8.vz = 0;
            work->field_8B8.vx = (s32)((TmdObject*)player->extra)->coords->coord.t[0];
            work->field_8B8.vy = (s32)((TmdObject*)player->extra)->coords->coord.t[1];
            work->field_8B8.vz = (s32)((TmdObject*)player->extra)->coords->coord.t[2];
            Gp_DispatchMsg(player, 0x3E9, (s32)&work->field_8B8, 0);
            if (work->field_E9E < 0x3E8) {
                if (enemy->hp > 0) {
                    closeDistance = scratch->yaw - scratch->playerYaw;
                    closeDistance = abs(closeDistance);
                    if (closeDistance < 0x400) {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 2), 0);
                    } else {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 3), 0);
                    }
                }
                if (scratch->reply != 1) {
                    work->field_E80          = 3;
                    work->field_E84          = 0;
                    work->field_E88          = 0;
                    work->field_8A4          = 0;
                    work->field_8A8          = 0;
                    work->field_8AC          = 0;
                    work->field_8B4          = 7;
                    work->field_8B6          = 1;
                    work->field_E9C          = 1;
                    work->field_E90.bytes[3] = 0;
                    Gp_DispatchMsg(player, 0x3FF, (s32)&work->field_E7C, 0);
                }
                nextState = 0x25;
            } else {
                if (enemy->hp > 0) {
                    farDistance = scratch->yaw - scratch->playerYaw;
                    farDistance = abs(farDistance);
                    if (farDistance < 0x400) {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
                    } else {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 1), 0);
                    }
                }
                if (scratch->reply == 1) {
                    ((GameActor*)player->work)->field_956 = 0xA;
                }
                work->field_E80          = 1;
                work->field_E84          = 0;
                work->field_E88          = 0;
                work->field_8A4          = 0;
                work->field_8A8          = 0;
                work->field_8AC          = 0;
                work->field_8B4          = 7;
                work->field_8B6          = 1;
                work->field_E9C          = 1;
                work->field_E90.bytes[3] = 0;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->field_E7C, 0);
                nextState = 0x1E;
            }
            work->field_0 = nextState;
        }
        aimCoord        = arg0->field_2C->coords;
        scratch->vec.vx = (s16)(Player_Status.coordMtx->t[0] - aimCoord->coord.t[0]);
        scratch->vec.vy = (s16)(Player_Status.coordMtx->t[1] - aimCoord->coord.t[1]);
        aimZ            = Player_Status.coordMtx->t[2] - aimCoord->coord.t[2];
        scratch->vec.vz = aimZ;
        aimFacing       = arg0->field_2C->coords;
        aimAngle        = ratan2(scratch->vec.vx, aimZ);
        aimDelta        = aimAngle - ratan2(-aimFacing->coord.m[2][0], aimFacing->coord.m[2][2]);
        scratch->aim    = Actor421600_NormalizeYaw(aimDelta);
    } else {
        fallbackCoord   = arg0->field_2C->coords;
        scratch->vec.vx = (s16)(Player_Status.coordMtx->t[0] - fallbackCoord->coord.t[0]);
        scratch->vec.vy = (s16)(Player_Status.coordMtx->t[1] - fallbackCoord->coord.t[1]);
        fallbackZ       = Player_Status.coordMtx->t[2] - fallbackCoord->coord.t[2];
        scratch->vec.vz = fallbackZ;
        fallbackFacing  = arg0->field_2C->coords;
        fallbackAngle   = ratan2(scratch->vec.vx, fallbackZ);
        fallbackDelta   = fallbackAngle - ratan2(-fallbackFacing->coord.m[2][0], fallbackFacing->coord.m[2][2]);
        fallbackYaw     = Actor421600_NormalizeYaw(fallbackDelta);
        scratch->aim    = (s16)fallbackYaw;
        fallbackYaw     = abs(fallbackYaw);
        if (fallbackYaw >= 0x601) {
            work->field_0 = 0x1D;
        }
    }
    arg0->field_2C->coords->flg = 0;
    moveCoord                   = arg0->field_2C->coords;
    moveAngle                   = ratan2(work->field_E70, work->field_E74);
    moveDelta                   = moveAngle - ratan2(-moveCoord->coord.m[2][0], moveCoord->coord.m[2][2]);
    scratch->delta              = Actor421600_NormalizeYaw(moveDelta);
    func_actor_421600_80134604(arg0);
    stepCoord = arg0->field_2C->coords;
    Actor421600_MoveForward(stepCoord, 200);
    work->field_E9E = (s16)((u16)work->field_E9E + 0xC8);
    if (work->field_82E == 3) {
        switch (work->field_5A & 0x3FF) {
            case 5:
                spawnEffect = 1;
                part        = 7;
                effectFlags = 0x4300;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 8:
                spawnEffect = 1;
                part        = 9;
                effectFlags = 0x3500;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 10:
                spawnEffect = 1;
                part        = 14;
                effectFlags = 0x5A00;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            case 13:
                spawnEffect = 1;
                part        = 17;
                effectFlags = 0x4800;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            default:
                effectFlags = 0;
                spawnEffect = 0;
                part        = 0;
                break;
        }
        if ((Gp_State1C->roomEffectMode == 2) && (spawnEffect == 1)) {
            Gp_SpawnEff(0x60054, arg0->field_2C->coords + part, effectFlags | 0x80000000, &effect);
        }
    }
    *(Actor421600AttackScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_421600_8013B00C(Actor421600* arg0)
{
    Actor421600Work*        work;
    Actor421600SeekScratch* head;
    Actor421600SeekScratch* blk;
    GpEnemy*                ctx;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          coord3;
    GsCOORDINATE2*          coord4;
    s32                     zone;
    s32                     x_entry;
    s32                     z_entry;
    s32                     var_a0_entry;
    s32                     var_v1_entry;
    Task*                   task;
    GsCOORDINATE2*          playerCoord;
    s32                     x;
    s32                     z;
    s16                     angle;
    s32                     wrapped;
    s32                     var_a0;
    s32                     var_v1;

    work = arg0->field_1C;
    task = gameGetPtrSlot(3);
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx             = arg0->field_20;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 3;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        playerCoord = ((TmdObject*)task->extra)->coords;
        x_entry     = playerCoord->coord.t[0];
        z_entry     = playerCoord->coord.t[2];
        if (x_entry >= 0xD49) {
            var_a0_entry = 3;
        } else if (x_entry > 0) {
            var_a0_entry = 2;
        } else {
            var_a0_entry = x_entry >= -0xC7F;
        }
        var_v1_entry = 0;
        if (z_entry < 0xBB9) {
            var_v1_entry = 1;
            if (z_entry <= 0) {
                var_v1_entry = 3;
                if (z_entry >= -0xBB7) {
                    var_v1_entry = 2;
                }
            }
        }
        if (D_actor_421600_801511C0[var_a0_entry | (var_v1_entry * 4)] >= 7) {
            work->field_E78 = 1;
            return;
        }
        work->field_E78 = 0xB;
        return;
    }
    coord = arg0->field_2C->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s16)zone == work->field_E78) {
        work->field_0 = 0;
        return;
    }
    head                                       = *(Actor421600SeekScratch**)G_SCRATCH_HEAD;
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD -= 1;
    blk                                        = head - 1;
    if ((s16)zone > work->field_E78) {
        head[-1].vec.vx = D_actor_421600_80151158[zone - 1].vx;
        blk->vec.vy     = D_actor_421600_80151158[zone - 1].vy;
        blk->vec.vz     = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].vec.vx = D_actor_421600_80151158[zone + 1].vx;
        blk->vec.vy     = D_actor_421600_80151158[zone + 1].vy;
        blk->vec.vz     = D_actor_421600_80151158[zone + 1].vz;
    }
    blk->vec.vx = blk->vec.vx - (u16)arg0->field_2C->coords->coord.t[0];
    blk->vec.vy = 0;
    blk->vec.vz = blk->vec.vz - (u16)arg0->field_2C->coords->coord.t[2];
    func_actor_421600_80134604(arg0);
    coord2 = arg0->field_2C->coords;
    angle  = ratan2(blk->vec.vx, blk->vec.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x81) {
        blk->angle = 0x80;
    }
    if (blk->angle < -0x80) {
        blk->angle = -0x80;
    }
    work->field_83E = blk->angle;
    coord3          = arg0->field_2C->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = arg0->field_2C->coords;
        Actor421600_MoveForward(coord4, 0xC8);
    }
    func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &blk->vec);
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD += 1;
    arg0->field_2C->coords->flg                = 0;
}

/// Zone-aim tick: the live-actor edge re-arms the model the way
/// `func_actor_421600_80138D24` does -- buffers reallocated, clip 0x10, pose 3,
/// motion 1, the 0xB6C node's 0x4000 flag up.
///
/// Otherwise the X and Z of the actor's coordinate are bucketed into the 4x4
/// zone table `D_actor_421600_801511C0` exactly as `func_actor_421600_8013A404`
/// does, and zone 5 abandons the tick into state 7. Any other zone picks the
/// neighbouring entry of the 8-byte pose table `D_actor_421600_80151158` --
/// `zone - 1` above the table's midpoint `mode`, `zone + 1` at or below it --
/// and copies all three halfwords into a 0xC block taken off `G_SCRATCH_HEAD`,
/// which becomes the XZ direction from the actor to that pose.
///
/// `mode` and the `(s8)` casts on `zone` are load-bearing, and so is the
/// `blk->vec.vy = 0` between the two coordinate subtractions. A plain `5`
/// literal lets expand fold `zone > 5` into `zone < 6`, which drops the two
/// register copies and the `slt` the ROM has; keeping the limit in a
/// declaration-initialised `s8` leaves it a register operand so the fold never
/// runs. The midpoint store then lands in the load-delay slot the subtractions
/// leave open.
void func_actor_421600_8013B4C4(Actor421600* arg0)
{
    Actor421600Work*        work;
    Actor421600SeekScratch* head;
    Actor421600SeekScratch* blk;
    GpEnemy*                ctx;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          coord3;
    GsCOORDINATE2*          coord4;
    s32                     zone;
    s8                      mode = 5;
    s32                     v;
    s32                     x;
    s32                     z;
    s16                     angle;
    s32                     wrapped;
    s32                     var_a0;
    s32                     var_v1;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx             = arg0->field_20;
        ctx->node.flags = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 3;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    coord = arg0->field_2C->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s8)zone == mode) {
        work->field_0 = 7;
        return;
    }
    head                                       = *(Actor421600SeekScratch**)G_SCRATCH_HEAD;
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD -= 1;
    blk                                        = head - 1;
    if ((s8)zone > mode) {
        head[-1].vec.vx = D_actor_421600_80151158[zone - 1].vx;
        blk->vec.vy     = D_actor_421600_80151158[zone - 1].vy;
        blk->vec.vz     = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].vec.vx = D_actor_421600_80151158[zone + 1].vx;
        blk->vec.vy     = D_actor_421600_80151158[zone + 1].vy;
        blk->vec.vz     = D_actor_421600_80151158[zone + 1].vz;
    }
    blk->vec.vx = blk->vec.vx - (u16)arg0->field_2C->coords->coord.t[0];
    blk->vec.vy = 0;
    blk->vec.vz = blk->vec.vz - (u16)arg0->field_2C->coords->coord.t[2];
    func_actor_421600_80134604(arg0);
    coord2 = arg0->field_2C->coords;
    angle  = ratan2(blk->vec.vx, blk->vec.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x81) {
        blk->angle = 0x80;
    }
    if (blk->angle < -0x80) {
        blk->angle = -0x80;
    }
    work->field_83E = blk->angle;
    coord3          = arg0->field_2C->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = arg0->field_2C->coords;
        Actor421600_MoveForward(coord4, 0xC8);
    }
    func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &blk->vec);
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD += 1;
    arg0->field_2C->coords->flg                = 0;
}

void func_actor_421600_8013B8E0(Actor421600* arg0)
{
    Actor421600Work* temp_s1;
    TmdObject*       temp_a0;

    temp_s1 = arg0->field_1C;
    if (temp_s1->field_4 != 0) {
        temp_a0                    = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        temp_a0->flags             = 0;
        Tmd_AllocBuffers(temp_a0);
        temp_s1->field_832                 = 0x10;
        temp_s1->field_82E                 = 0x11;
        temp_s1->field_828                 = 2;
        temp_s1->field_B6C.flags          |= 0x4000;
        arg0->field_2C->coords->coord.t[0] = 0;
        arg0->field_2C->coords->coord.t[1] = 0;
        arg0->field_2C->coords->coord.t[2] = 0;
        arg0->field_2C->coords->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0, 1);
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (temp_s1->field_68 & 0x100) {
        arg0->field_2C->coords->coord.t[0] = -0x334;
        arg0->field_2C->coords->coord.t[1] = 0;
        arg0->field_2C->coords->coord.t[2] = -0x4C4;
        arg0->field_2C->coords->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x400, 1);
        temp_s1->field_828 = 2;
        temp_s1->field_82E = 0;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        temp_s1->field_0 = 0x27;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013BA70);

/// Death tick: the live-actor edge arms the model (dirty 0x80, clip 0x19C, the
/// 0xB6C node's 0x4000 flag down, the enemy's list node marked, the 0x83E /
/// 0x840 / 0x844 triple and `field_6` cleared) and spawns the 0x60030 effect on
/// the second coordinate. Frames 2, 3, 5, 7 and 8 then free the model buffers
/// and spawn one effect each -- 0xA0005 on coordinate 9, 12, 1 and 3 -- whose
/// model is tinted from the enemy's area record (`field_24` / `field_25`) and
/// re-streamed. Frame 0xA writes the 0x16 state. The counter stops at 0x400.
void func_actor_421600_8013C8E0(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR          vec;
    GpAreaKey        key;
    GpAreaKey*       keyPtr;
    GpAreaKey*       sessionKey1;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       sessionKey3;
    GpAreaKey*       sessionKey4;
    u32              raw1;
    u32              raw2;
    u32              raw3;
    u32              raw4;
    u32              index1;
    u32              index2;
    u32              index3;
    u32              index4;
    GpEffWork*       effect1;
    GpEffWork*       effect2;
    GpEffWork*       effect3;
    GpEffWork*       effect4;
    TmdObject*       model1;
    TmdObject*       model2;
    TmdObject*       model3;
    TmdObject*       model4;
    GpAreaRec*       rec1;
    GpAreaRec*       rec2;
    GpAreaRec*       rec3;
    GpAreaRec*       rec4;
    GpAreaPlace*     entry1;
    GpAreaPlace*     entry2;
    GpAreaPlace*     entry3;
    GpAreaPlace*     entry4;
    u8               areaByte0;
    u16              tick;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    obj  = arg0->field_2C;
    if (work->field_4 != 0) {
        obj->flags             = 0x80;
        work->field_8EC.radius = 0x19C;
        work->field_B6C.flags  = (u16)(work->field_B6C.flags & 0xBFFF);
        ctx->node.flags        = 1;
        work->field_844        = 0;
        work->field_840        = 0;
        work->field_83E        = 0;
        work->field_6          = 0;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &vec);
    }
    if ((s16)work->field_6 == 2) {
        obj->flags |= 4;
        Tmd_FreeBuffers(obj);
    }
    if ((s16)work->field_6 == 3) {
        D_80114B78[0] = &D_actor_421600_80143EF4;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        effect1       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &vec);
        if (effect1 != NULL) {
            sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
            raw1        = ctx->placeKey;
            model1      = (TmdObject*)effect1->task->extra;
            key.stage   = sessionKey1->stage;
            key.area    = sessionKey1->area;
            key.room    = sessionKey1->room;
            areaByte0   = gGameSession->at4.loc.view;
            index1      = raw1 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec1          = Gp_GetNestedAreaRec(&key);
            entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)rec1->field_0);
            model1->tpage = entry1->tpage;
            model1->clut  = entry1->clut;
            if (model1->buffer != NULL) {
                tmdProcessStream(model1);
                tmdProcessStream(model1);
            }
        }
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0] = &D_actor_421600_801443C8;
        vec.vy        = 0;
        vec.vx        = 0;
        effect2       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 12, 0x200, &vec);
        if (effect2 != NULL) {
            sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
            raw2        = ctx->placeKey;
            model2      = (TmdObject*)effect2->task->extra;
            key.stage   = sessionKey2->stage;
            key.area    = sessionKey2->area;
            key.room    = sessionKey2->room;
            areaByte0   = gGameSession->at4.loc.view;
            index2      = raw2 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec2          = Gp_GetNestedAreaRec(&key);
            entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)rec2->field_0);
            model2->tpage = entry2->tpage;
            model2->clut  = entry2->clut;
            if (model2->buffer != NULL) {
                tmdProcessStream(model2);
                tmdProcessStream(model2);
            }
        }
    }
    if ((s16)work->field_6 == 7) {
        D_80114B78[0] = &D_actor_421600_80145604;
        effect3       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
            raw3        = ctx->placeKey;
            model3      = (TmdObject*)effect3->task->extra;
            key.stage   = sessionKey3->stage;
            key.area    = sessionKey3->area;
            key.room    = sessionKey3->room;
            areaByte0   = gGameSession->at4.loc.view;
            index3      = raw3 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec3          = Gp_GetNestedAreaRec(&key);
            entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)rec3->field_0);
            model3->tpage = entry3->tpage;
            model3->clut  = entry3->clut;
            if (model3->buffer != NULL) {
                tmdProcessStream(model3);
                tmdProcessStream(model3);
            }
        }
    }
    if ((s16)work->field_6 == 8) {
        D_80114B78[0] = &D_actor_421600_80145124;
        effect4       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL);
        if (effect4 != NULL) {
            sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
            raw4        = ctx->placeKey;
            model4      = (TmdObject*)effect4->task->extra;
            key.stage   = sessionKey4->stage;
            key.area    = sessionKey4->area;
            key.room    = sessionKey4->room;
            areaByte0   = gGameSession->at4.loc.view;
            index4      = raw4 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec4          = Gp_GetNestedAreaRec(&key);
            entry4        = (GpAreaPlace*)((index4 * 0x10) + (s32)rec4->field_0);
            model4->tpage = entry4->tpage;
            model4->clut  = entry4->clut;
            if (model4->buffer != NULL) {
                tmdProcessStream(model4);
                tmdProcessStream(model4);
            }
        }
    }
    if ((s16)work->field_6 == 0xA) {
        work->field_0 = 0x16;
    }
    if ((s16)work->field_6 < 0x400) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
    }
}

void func_actor_421600_8013CD3C(Actor421600* arg0)
{
    TmdObject*              obj;
    GpEnemy*                ctx;
    Actor421600Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          targetCoord;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          facing2;
    Actor421600TurnScratch* head;
    Actor421600TurnScratch* scratch;
    s16                     yaw;
    s16                     delta;
    s16                     z;
    s16                     steps;
    s16                     wrapped;
    s32                     angle;
    s32                     firstDelta;

    head    = *(Actor421600TurnScratch**)G_SCRATCH_HEAD;
    scratch = (*(Actor421600TurnScratch**)G_SCRATCH_HEAD = head - 1);
    work    = arg0->field_1C;
    ctx     = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        obj->flags             = 0;
        work->field_8EC.radius = 0x19C;
        work->field_B6C.flags |= 0x4000;
        ctx->node.flags        = 0;
        work->field_828        = 1;
        work->field_82E        = 0x14;
        work->field_832        = 0x10;
        work->field_6          = 0;
    }
    work->field_6 += 1;
    func_actor_421600_80134604(arg0);
    targetCoord     = arg0->field_2C->coords;
    head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = Player_Status.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = arg0->field_2C->coords;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta < 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta + 0x800;
            scratch->delta += 0x800;
        }
    }
    if (abs(scratch->delta) < 0x80) {
        work->field_0 = 0x1C;
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = arg0->field_2C->coords;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, (s32)yaw, 1);
    Gfx_MatrixCol2(&arg0->field_2C->coords->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(-0x1A);
    gte_ldsv(&scratch->vec);
    gte_gpf12_real();
    gte_stsv(&scratch->vec);
    coord               = arg0->field_2C->coords;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = arg0->field_2C->coords;
    coord2->coord.t[2] += scratch->vec.vz;
    Actor421600_MoveForward(arg0->field_2C->coords, -8);
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
    *(Actor421600TurnScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_421600_8013D1DC(Actor421600* arg0)
{
    TmdObject*              obj;
    GpEnemy*                ctx;
    Actor421600Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          targetCoord;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          facing2;
    Actor421600TurnScratch* head;
    Actor421600TurnScratch* scratch;
    s16                     yaw;
    s16                     delta;
    s16                     z;
    s16                     steps;
    s16                     wrapped;
    s32                     angle;
    s32                     firstDelta;

    head    = *(Actor421600TurnScratch**)G_SCRATCH_HEAD;
    scratch = (*(Actor421600TurnScratch**)G_SCRATCH_HEAD = head - 1);
    work    = arg0->field_1C;
    ctx     = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        obj->flags             = 0;
        work->field_8EC.radius = 0x19C;
        work->field_B6C.flags |= 0x4000;
        ctx->node.flags        = 0;
        work->field_828        = 1;
        work->field_82E        = 0x15;
        work->field_832        = 0x10;
        work->field_6          = 0;
    }
    work->field_6 += 1;
    func_actor_421600_80134604(arg0);
    targetCoord     = arg0->field_2C->coords;
    head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = Player_Status.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = arg0->field_2C->coords;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta > 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta - 0x800;
            scratch->delta -= 0x800;
        }
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = arg0->field_2C->coords;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, (s32)yaw, 1);
    Gfx_MatrixCol2(&arg0->field_2C->coords->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(0x1A);
    gte_ldsv(&scratch->vec);
    gte_gpf12_real();
    gte_stsv(&scratch->vec);
    coord               = arg0->field_2C->coords;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = arg0->field_2C->coords;
    coord2->coord.t[2] += scratch->vec.vz;
    Actor421600_MoveForward(arg0->field_2C->coords, -8);
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
    *(Actor421600TurnScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D658);

void func_actor_421600_8013E424(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_421600/actor_421600", ActorsShared80135df4Table);
