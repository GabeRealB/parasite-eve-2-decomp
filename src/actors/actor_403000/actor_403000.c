#include "common.h"

#include "actors/actor_403000.h"
#include "actors/actor_400100_motion.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "psyq/abs.h"
#include "psyq/inline_c.h"

/// `gpf 12`. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// `rtps`, spelled out for the same reason.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern GpPairSrcE D_actor_403000_8013DA00;
extern GpPairSrcE D_actor_403000_8013DA10;
extern u32        D_actor_403000_80158B50;
extern u32        D_actor_403000_80158C08;
extern u32        D_actor_403000_80158CA8;
extern u8         D_80071075;
extern s8         D_80114C12;
extern s8         D_actor_403000_80158364[];

void func_800B4114(GpAnimCtx* anim, s32 slot, s16 animation, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132348);

s32 func_actor_403000_801324EC(Actor403000* arg0, s32 arg1, Actor403000Event* arg2)
{
    Actor403000Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;

    work->field_FA4 = arg2->b[0];
    work->field_FA5 = arg2->b[1];
    work->field_FA6 = arg2->b[2];
    if (arg2->w[0] == 0x204) {
        switch (arg2->w[1]) {
            case 0:
                enemy->field_40 = 0;
                work->field_0   = 0;
                return 1;
            case 1:
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                enemy->field_4C = 0;
                enemy->field_50 = &D_actor_403000_8013DA00;
                enemy->field_40 = D_actor_403000_8013DA00.field_4;
                work->field_AC6 = 0x18;
                work->field_0   = 1;
                work->field_2   = -1;
                return 1;
            case 2:
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                enemy->field_4C = 0;
                enemy->field_50 = &D_actor_403000_8013DA00;
                enemy->field_40 = D_actor_403000_8013DA00.field_4;
                work->field_AC6 = 0x19;
                work->field_0   = 1;
                work->field_2   = -1;
                return 1;
            case 3:
                enemy->field_40 = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                enemy->field_4C = 0;
                work->field_AC6 = 0x1A;
                work->field_0   = 1;
                work->field_2   = -1;
                enemy->field_4C = 0;
                enemy->field_40 = D_actor_403000_8013DA10.field_4;
                enemy->field_50 = &D_actor_403000_8013DA10;
                return 1;
            case 4:
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                enemy->field_4C = 0;
                work->field_AC6 = 0x1B;
                work->field_0   = 1;
                work->field_2   = -1;
                return 1;
            case 5:
                work->field_0            = 0x15;
                work->field_2            = -1;
                arg0->field_2C->field_24 = 2;
                arg0->field_2C->field_25 = 4;
                return 1;
            case 6:
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                enemy->field_4C = 0;
                enemy->field_40 = D_actor_403000_8013DA10.field_4;
                enemy->field_50 = &D_actor_403000_8013DA10;
                work->field_AC6 = 0x19;
                work->field_0   = 1;
                work->field_2   = -1;
                return 1;
            case 7:
                arg0->field_2C->field_24 = 2;
                arg0->field_2C->field_25 = 4;
                work->field_0            = 0x14;
                work->field_2            = -1;
                return 1;
            case 10:
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                enemy->field_4C = 0;
                work->field_AC6 = 0x18;
                work->field_0   = 1;
                work->field_2   = -1;
                return 1;
            case 11:
                work->field_0   = 4;
                work->field_2   = -1;
                work->field_FD3 = -1;
                work->field_FD2 = 1;
                work->field_FD5 = 1;
                if ((s8)arg0->field_2C->field_24 == 2) {
                    enemy->field_4C = 0;
                    enemy->field_40 = D_actor_403000_8013DA10.field_4;
                    enemy->field_50 = &D_actor_403000_8013DA10;
                }
                return 1;
            case 12:
                arg0->field_2C->field_24 = 2;
                arg0->field_2C->field_25 = 4;
                work->field_0            = 0x16;
                work->field_2            = -1;
                return 1;
            case 13:
                work->field_0            = 0x17;
                work->field_2            = -1;
                arg0->field_2C->field_24 = 2;
                arg0->field_2C->field_25 = 4;
                return 1;
        }
    }
    return 0;
}

void func_actor_403000_801327B0(GsCOORDINATE2* coord, SVECTOR* pos)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    u16       x;
    u16       y;

    coord->flg = 0;
    Gp_UpdateCoord(coord);
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(pos);
    gte_rtps_real();
    gte_stsxy(&sxy);
    gte_stflg(&flag);
    gte_stszotz(&otz);
    if (flag >= 0) {
        x           = sxy;
        y           = sxy >> 16;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        radius      = (s32)(((Gp_LcgState >> 16) & 0xF) + 0x1E) * 0x160 / (otz * 4);
        for (i = 0; i < 8; i++) {
            prim           = (POLY_G3*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)((POLY_GT3*)prim + 1);
            setPolyG3(prim);
            setRGB0(prim, 0xFF, 0x60, 0x60);
            setRGB1(prim, 0xF, 8, 8);
            setRGB2(prim, 0x2F, 8, 8);
            prim->x0 = x;
            prim->y0 = y;
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> 12);
            prim->y1 = y + ((rcos(i << 9) * radius) >> 12);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> 12);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> 12);
            addPrim(&Gpu_CurrentOt[(otz - 6) >> 4], prim);
            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)((DR_MODE*)dr + 1);
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&Gpu_CurrentOt[(otz - 6) >> 4], dr);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132AE0);

void func_actor_403000_801330D4(GsCOORDINATE2* parent)
{
    Actor403000TrailScratch* scratch;
    MATRIX*                  m;
    GsCOORDINATE2*           walker;
    SVECTOR*                 pos;
    s16                      i;

    *(u8**)G_SCRATCH_HEAD -= sizeof(Actor403000TrailScratch);
    scratch                = *(Actor403000TrailScratch**)G_SCRATCH_HEAD;
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    /* Identity, written as three words and a short through a second pointer. */
    m                                    = &scratch->coord.coord;
    *(s32*)&scratch->coord.coord.m[0][0] = 0x1000;
    *(s32*)&m->m[0][2]                   = 0;
    *(s32*)&m->m[1][1]                   = 0x1000;
    *(s32*)&m->m[2][0]                   = 0;
    m->m[2][2]                           = 0x1000;
    scratch->coord.coord.t[0]            = -0x3C;
    scratch->coord.coord.t[1]            = -0x28;
    scratch->coord.sub                   = parent;
    scratch->coord.coord.t[2]            = 0x12C;
    scratch->coord.flg                   = 0;
    Gp_UpdateCoord(&scratch->coord);
    walker          = &scratch->coord;
    pos             = &scratch->pos;
    scratch->pos.vz = 0;
    scratch->pos.vy = 0;
    scratch->pos.vx = 0;
    {
        SVECTOR local;
        VECTOR  result;
        s32     flag;

        local.vx = 0;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->sub == NULL)
                break;
            if (walker != &Gfx_ViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(&local);
                __asm__ volatile("nop; nop; .word 0x4A480012");
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->sub;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    D_actor_403000_80158DF0[0].vx = scratch->pos.vx;
    D_actor_403000_80158DF0[0].vy = scratch->pos.vy;
    D_actor_403000_80158DF0[0].vz = scratch->pos.vz;
}

void func_actor_403000_801332E8(Actor403000* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;

    work   = arg0->field_1C;
    orig   = work->field_ADA;
    target = orig;
    if (orig > 700) {
        target = 700;
    }
    if (orig < -700) {
        target = -700;
    }
    if (work->field_AE2 < target) {
        if (target - work->field_AE2 > 64) {
            work->field_AE2 += 64;
        } else {
            work->field_AE2 = target;
        }
    }
    if (target < work->field_AE2) {
        diff = work->field_AE2 - target;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff > 64) {
            work->field_AE2 -= 64;
        } else {
            work->field_AE2 = target;
        }
    }
    Gfx_RotMatrixZ(&arg0->field_2C->field_8[22].coord, work->field_AE2 / 2, 1);
    arg0->field_2C->field_8[22].flg = 0;
    Gfx_RotMatrixZ(&arg0->field_2C->field_8[23].coord, work->field_AE2 * 3 / 4, 1);
    arg0->field_2C->field_8[23].flg = 0;
}

void func_actor_403000_80133444(Actor403000* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;
    s32              delta;

    work = arg0->field_1C;
    if (work->field_AE0 < work->field_AE6 && (diff = abs(work->field_AE0 - work->field_AE6)) >= 8 && work->field_ADC > -0x280) {
        if (diff >= 24) {
            if (work->field_ADC > 0) {
                work->field_ADC = -32;
            } else {
                work->field_ADC -= 32;
            }
        } else {
            if (work->field_ADC > 0) {
                work->field_ADC = -2;
            } else {
                work->field_ADC -= 2;
            }
        }
    } else if (work->field_AE0 > work->field_AE6 && (diff = abs(work->field_AE0 - work->field_AE6)) >= 8 && work->field_ADC < 0x280) {
        if (diff >= 24) {
            if (work->field_ADC < 0) {
                work->field_ADC = 32;
            } else {
                work->field_ADC += 32;
            }
        } else {
            if (work->field_ADC < 0) {
                work->field_ADC = 2;
            } else {
                work->field_ADC += 2;
            }
        }
    } else {
        work->field_ADC = 0;
    }
    target          = work->field_ADC;
    work->field_AE6 = work->field_AE0;
    orig            = target;
    if (orig > 640) {
        target = 640;
    }
    if (orig < -640) {
        target = -640;
    }
    if (work->field_AE4 < target) {
        if (target - work->field_AE4 > 48) {
            work->field_AE4 += 48;
        } else {
            work->field_AE4 = target;
        }
    }
    if (target < work->field_AE4) {
        delta = work->field_AE4 - target;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta > 48) {
            work->field_AE4 -= 48;
        } else {
            work->field_AE4 = target;
        }
    }
    Gfx_RotMatrixZ(&arg0->field_2C->field_8[6].coord, work->field_AE4 / 2, 0);
    arg0->field_2C->field_8[6].flg = 0;
    Gfx_RotMatrixZ(&arg0->field_2C->field_8[7].coord, work->field_AE4 * 3 / 4, 0);
    arg0->field_2C->field_8[7].flg = 0;
}

void func_actor_403000_801336B4(Actor403000* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor403000AnimWork* work;

    work   = (Actor403000AnimWork*)arg0->field_1C;
    weight = work->field_AD4;
    anim   = &work->anim;
    for (i = 1; i < 0x18; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_AD2;
            work->slots[i].field_9      = (u8)(work->field_ACA - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_ACA - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

s32 func_actor_403000_801337E0(s32 arg0, Actor403000AnimWork* work)
{
    s32 ret;

    ret = 0;
    if (work->field_AF0 == (work->slots[1].field_2 & 0x3FF)) {
        return ret;
    }
    switch ((s16)(work->field_AC6 - 1)) {
        case 0:
            if ((work->slots[1].field_2 & 0x3FF) >= 0x21 && work->field_AF0 < 0x21) {
                ret = 0x401E0002;
            }
            if ((work->slots[1].field_2 & 0x3FF) >= 0x2C && work->field_AF0 < 0x2C) {
                ret = 0x401E0001;
            }
            break;
        case 8:
            if ((work->slots[1].field_2 & 0x3FF) >= 5 && work->field_AF0 < 5) {
                ret = 0x401E0002;
            }
            if ((work->slots[1].field_2 & 0x3FF) >= 0xA && work->field_AF0 < 0xA) {
                ret = 0x401E0001;
            }
            break;
        case 13:
            if ((work->slots[1].field_2 & 0x3FF) >= 0xF && work->field_AF0 < 0xF) {
                ret = 0x401E000E;
            }
            break;
        case 1:
            if ((work->slots[1].field_2 & 0x3FF) >= 0x1D && work->field_AF0 < 0x1D) {
                ret = 0x401E0003;
            }
            if ((work->slots[1].field_2 & 0x3FF) >= 0x17 && work->field_AF0 < 0x17) {
                ret = 0x401E0004;
            }
            break;
        case 11:
            if ((work->slots[1].field_2 & 0x3FF) >= 0x13 && work->field_AF0 < 0x13) {
                ret = 0x401E0008;
            }
        case 10:
            if ((work->slots[1].field_2 & 0x3FF) >= 0x12 && work->field_AF0 < 0x12) {
                ret = 0x401E0007;
            }
            if ((work->slots[1].field_2 & 0x3FF) >= 0xF && work->field_AF0 < 0xF) {
                ret = 0x401E000C;
            }
            break;
        case 6:
            if ((work->slots[1].field_2 & 0x3FF) >= 0x15 && work->field_AF0 < 0x15) {
                ret = 0x401E0009;
            }
            break;
        case 16:
            if ((work->slots[1].field_2 & 0x3FF) >= 8 && work->field_AF0 < 8) {
                ret = 0x401E000B;
            }
            break;
        case 7:
            if ((work->slots[1].field_2 & 0x3FF) >= 0xD && work->field_AF0 < 0xD) {
                ret = 0x401E000D;
            }
            break;
    }
    work->field_AF0 = work->slots[1].field_2 & 0x3FF;
    return ret;
}

void func_actor_403000_80133AF8(Actor403000* arg0)
{
    Actor403000AnimWork* seekWork;
    Actor403000AnimWork* resetWork;
    Actor403000AnimWork* turnWork;
    Actor403000AnimWork* secondaryWork;
    Actor403000AnimWork* tickWork;
    Actor403000AnimWork* work;
    u32                  table;
    s32                  index;
    s32                  animation;
    s32                  updatedTurn;
    s16                  currentTurn;
    s16                  thirdAngle;
    s16                  state;
    s32                  currentAngle;
    s32                  targetAngle;
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
    GpAnimSlot*          tickSlot;
    GpAnimSlot*          seekSlot;
    GpAnimSlot*          resetSlot;
    GpAnimSlot*          secondarySlot;
    s32                  pan;
    s32                  currentAngleBits;
    u16                  originalTurn;
    s32                  targetAngleBits;
    u16                  updatedTurnBits;
    s16                  clampedAngle;
    s32                  targetTurn;

    work  = (Actor403000AnimWork*)arg0->field_1C;
    state = work->field_AC0;
    if (state == 1) {
        if (work->field_AC4 != work->field_AC6) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)D_actor_403000_80158364;
            seekSlot  = work->slots;
            do {
                seekSlotIndex       = seekIndex;
                seekSlot[1].field_9 = (u8)seekWork->field_ACA;
                animation           = seekWork->field_AC6;
                seekSlot           += 1;
                index               = seekWork->field_AC4 * 0x2D;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x18);
            seekWork->field_AC4 = seekWork->field_AC6;
        }
        work->field_AC0 = 3;
        work->field_AC8 = 0;
        Mem_Set(work->pad_AEC, 0U, 0x60U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = work->slots;
        do {
            resetSlotIndex       = resetIndex;
            resetSlot[1].field_9 = (u8)resetWork->field_ACA;
            resetSlot           += 1;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)resetWork->field_AC6);
            resetIndex += 1;
        } while (resetIndex < 0x18);
        resetWork->field_AC4 = resetWork->field_AC6;
        work->field_AC0      = 3;
        work->field_AC8      = 0U;
        Mem_Set(work->pad_AEC, 0U, 0x60U);
    }
    if (work->field_ACE == 2) {
        secondaryWork            = (Actor403000AnimWork*)arg0->field_1C;
        secondaryIndex           = 1;
        secondarySlot            = secondaryWork->slots;
        secondaryWork->field_AD2 = 0x20;
        secondaryWork->field_AD4 = 0x800;
        do {
            secondarySlotIndex       = secondaryIndex;
            secondarySlot[1].field_9 = (u8)secondaryWork->field_AD2;
            secondarySlot           += 1;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_AD0);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x18);
        work->field_ACE = 3;
    }
    work->field_AC8 = (u16)(work->field_AC8 + 1);
    if (work->field_AC2 == 0) {
        tickWork  = (Actor403000AnimWork*)arg0->field_1C;
        tickIndex = 1;
        tickSlot  = tickWork->slots;
        do {
            tickSlotIndex       = tickIndex;
            tickSlot[1].field_9 = (u8)tickWork->field_ACA;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickSlot  += 1;
            tickIndex += 1;
        } while (tickIndex < 0x18);
    } else {
        func_actor_403000_801336B4(arg0);
        if (work->blendSlots[1].field_10 & 0x100) {
            work->field_AC2 = 0;
        }
    }
    targetAngle      = (s16)work->field_AD8;
    currentAngle     = (s16)work->field_AE0;
    targetAngleBits  = work->field_AD8;
    currentAngleBits = work->field_AE0;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_AE0 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_AE0 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_AE0 = targetAngleBits;
    }
    if (work->field_AE8 == 1) {
        angle        = (s16)work->field_AE0;
        clampedAngle = angle;
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        ActorsShared80132808(&arg0->field_2C->field_8[2], thirdAngle);
        arg0->field_2C->field_8[2].flg = 0;
        ActorsShared80132808(&arg0->field_2C->field_8[3], thirdAngle);
        arg0->field_2C->field_8[3].flg = 0;
        ActorsShared80132808(&arg0->field_2C->field_8[4], (s16)clampedAngle / 2);
        arg0->field_2C->field_8[4].flg = 0;
    }
    if (work->field_AE9 == 1) {
        func_actor_403000_80133444(arg0);
    }
    if (work->field_AEA == 1) {
        turnWork     = (Actor403000AnimWork*)arg0->field_1C;
        targetTurn   = turnWork->field_AD6;
        originalTurn = targetTurn;
        if ((s16)targetTurn >= 0x201) {
            targetTurn = 0x200;
        }
        if ((s16)originalTurn < -0x200) {
            targetTurn = -0x200;
        }
        signedTurn  = (s16)targetTurn;
        currentTurn = turnWork->field_ADE;
        if (currentTurn < signedTurn) {
            if ((signedTurn - currentTurn) >= 0xD) {
                turnWork->field_ADE = (s16)((u16)turnWork->field_ADE + 0xC);
            } else {
                turnWork->field_ADE = (s16)targetTurn;
            }
        }
        updatedTurn     = turnWork->field_ADE;
        updatedTurnBits = (u16)turnWork->field_ADE;
        if ((s16)targetTurn < updatedTurn) {
            delta = updatedTurn - (s16)targetTurn;
            if (delta < 0) {
                delta = -delta;
            }
            if (delta >= 0xD) {
                turnWork->field_ADE = (s16)(updatedTurnBits - 0xC);
            } else {
                turnWork->field_ADE = (s16)targetTurn;
            }
        }
        ActorsShared80132808(&arg0->field_2C->field_8[10], (s16)((s32)(u16)turnWork->field_ADE * -1));
        arg0->field_2C->field_8[10].flg = 0;
    }
    if (work->field_AEB == 1) {
        func_actor_403000_801332E8(arg0);
    }
    sound = func_actor_403000_801337E0((s32)arg0, work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
}

s32 func_actor_403000_80133FC0(Task* arg0, s16 arg1, s16 arg2)
{
    GsCOORDINATE2*            coord;
    Actor403000FacingScratch* scratch;
    s16                       angle;
    s32                       mag;
    GsCOORDINATE2*            coord2;

    if (arg1 == arg2) {
        return 1;
    }
    switch (arg1) {
        case 0:
            if (arg2 == 9) {
                goto calc;
            }
            if (arg2 < 4) {
                goto calc;
            }
            return 0;
        case 1:
        case 2:
        case 3:
            if (arg2 < 5) {
                goto calc;
            }
            return 0;
        case 4:
            if (arg2 >= 6) {
                return 0;
            }
            if (arg2 != 0) {
                goto calc;
            }
            return 0;
        case 5:
            if (arg2 < 4) {
                return 0;
            }
            if (arg2 != 9) {
                goto calc;
            }
            return 0;
        case 6:
        case 7:
        case 8:
            if (arg2 >= 5) {
                goto calc;
            }
            return 0;
        case 9:
        default:
            if (arg2 >= 6) {
                goto calc;
            }
            if (arg2 != 0) {
                return 0;
            }
            break;
    }
calc:
    scratch            = --*(Actor403000FacingScratch**)G_SCRATCH_HEAD;
    coord              = ((TmdObject*)arg0->extra)->field_8;
    scratch->target.vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
    scratch->target.vy = Wip_SysConfig.field_4->t[1] - coord->coord.t[1];
    scratch->target.vz = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
    coord2             = ((TmdObject*)arg0->extra)->field_8;
    angle              = ratan2(scratch->target.vx, scratch->target.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
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
    scratch->angle = mag = angle;
    if ((mag < 0 ? -mag : mag) < 0x200) {
        *(Actor403000FacingScratch**)G_SCRATCH_HEAD += 1;
        return 1;
    }
    *(Actor403000FacingScratch**)G_SCRATCH_HEAD += 1;
    return 0;
}

s32 func_actor_403000_80134204(GsCOORDINATE2* arg0)
{
    GsCOORDINATE2*          coord;
    Actor403000TurnScratch* scratch;
    SVECTOR*                table;
    SVECTOR*                v;
    s32                     x;
    s32                     z;
    s8                      col;
    s8                      row;
    s16                     angle;

    scratch = --*(Actor403000TurnScratch**)G_SCRATCH_HEAD;
    coord   = arg0;
    x       = coord->coord.t[0];
    z       = coord->coord.t[2];
    col     = 4;
    if (x >= 0xD48) {
        col = 3;
        if (x >= 0x1A90) {
            col = 2;
            if (x >= 0x2AF8) {
                col = x < 0x3C8C;
            }
        }
    }
    row            = z >= 0x1068;
    scratch->index = D_actor_403000_80158D48[col + row * 5] + 1;
    if (scratch->index == 10) {
        scratch->index = 0;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->index];
    scratch->target.vx  = v->vx;
    scratch->target.vy  = v->vy;
    scratch->target.vz  = v->vz;
    scratch->target.vx -= coord->coord.t[0];
    scratch->target.vz -= coord->coord.t[2];
    angle               = ratan2(scratch->target.vx, scratch->target.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
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
    if (angle < 0x400) {
        scratch->turn = 1;
    } else {
        scratch->turn = -1;
    }
    *(Actor403000TurnScratch**)G_SCRATCH_HEAD += 1;
    return scratch->turn;
}

void func_actor_403000_801343B8(GpEnemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    VECTOR           pos;
    Actor403000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpObj*           node;
    GpObj*           node2;
    GpObj*           node3;
    u32*             animSrc;
    GpRec18*         records;
    GpRec18*         records2;
    SVECTOR*         dirp;
    GpRec18*         firstRec;
    Actor403000Work* idWork;
    TmdObject*       tmd;

    obj         = (TmdObject*)arg1->extra;
    coord       = obj->field_8;
    arg1->idMap = (TaskIdMap*)(work = Mem_Calloc(0xFDCU, false));
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->exitCallback = func_actor_403000_8013D4F4;
    idWork             = (Actor403000Work*)arg1->idMap;
    tmd                = (TmdObject*)arg1->extra;
    tmd->field_1C      = &idWork->field_F10;
    tmd->field_20      = &idWork->field_F30;
    arg0->field_4      = &((TmdObject*)arg1->extra)->field_8->coord;
    arg0->field_48     = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_18     = &((TmdObject*)arg1->extra)->field_8[2];
    Gp_LinkNode(&arg0->node);
    animSrc         = &D_actor_403000_80158B50;
    work->field_FCA = 1;
    arg0->field_4C  = 0;
    arg0->field_40  = D_actor_403000_8013DA00.field_4;
    arg0->field_50  = &D_actor_403000_8013DA00;
    arg0->field_54  = (s32)(firstRec = work->objB50.rec);
    func_800B3F84(&((Actor403000AnimWork*)work)->anim, animSrc, (GpAnimObj*)obj,
                  ((Actor403000AnimWork*)work)->pad_3E8, ((Actor403000AnimWork*)work)->slots);
    func_800B3F84(&((Actor403000AnimWork*)work)->blendAnim, animSrc, (GpAnimObj*)obj,
                  ((Actor403000AnimWork*)work)->pad_93C, ((Actor403000AnimWork*)work)->blendSlots);
    work->field_AC0 = 2;
    work->field_AC2 = 0;
    work->field_AC6 = 0;
    work->field_AE0 = 0;
    work->field_AD8 = 0;
    work->field_ACC = 0x10;
    work->field_ACA = 0x10;
    work->field_AEB = 1;
    work->field_AEA = 1;
    work->field_AE9 = 1;
    work->field_AE8 = 1;
    func_actor_403000_80133AF8((Actor403000*)arg1);
    work->objD18.obj.field_C  = work->objD18.rec;
    work->objD18.obj.field_8  = coord;
    work->objD18.obj.field_10 = 0;
    work->objD18.obj.field_12 = -0x11C;
    work->objD18.obj.field_14 = 0;
    work->objD18.obj.field_18 = 0x30001;
    work->objD18.obj.field_1C = 0x12C;
    work->objD18.obj.flags    = 1;
    Gp_LinkObj(2, &work->objD18.obj);
    work->recDD0.field_2  = -0x180;
    work->recDD0.field_A  = -0x180;
    work->recDD0.field_C  = 0x2BC;
    work->objDB0.field_C  = (GpRec18*)&work->recDD0;
    work->objDB0.field_18 = 0x30001;
    work->recDD0.field_0  = 0;
    work->recDD0.field_4  = 0;
    work->recDD0.field_8  = 0;
    work->recDD0.field_10 = 0x12C;
    work->recDD0.field_12 = 0x12C;
    work->recDD0.field_14 = records = work->records;
    work->objDB0.field_8            = coord;
    work->objDB0.field_10           = 0;
    work->objDB0.field_12           = 0;
    work->objDB0.field_14           = 0;
    work->objDB0.field_1C           = 0;
    work->objDB0.flags              = 3;
    work->objD18.obj.flags         |= 0x4000;
    Gp_LinkObj(2, &work->objDB0);
    work->recE80.field_4  = -0x3E8;
    work->recE80.field_C  = 0x190;
    work->recE80.field_10 = 0x200;
    work->recE80.field_12 = 0x200;
    work->recE80.field_0  = 0;
    work->recE80.field_2  = 0;
    work->recE80.field_8  = 0;
    work->recE80.field_A  = 0;
    work->recE80.field_14 = records2 = work->recordsE98;
    work->objDB0.flags              |= 0x4000;
    work->objE60.field_8             = &((TmdObject*)arg1->extra)->field_8[5];
    work->objE60.field_C             = (GpRec18*)&work->recE80;
    work->objE60.field_10            = 0;
    work->objE60.field_12            = 0;
    work->objE60.field_14            = 0;
    work->objE60.field_18            = 0x3001E;
    work->objE60.field_1C            = 0;
    work->objE60.flags               = 3;
    Gp_LinkObj(2, &work->objE60);
    work->objE60.flags |= 0x8000;
    Gp_InitRec18Table(records, 5, 0);
    Gp_InitRec18Table(records2, 5, 0);
    Gp_InitRec18Table(work->objD18.obj.field_C, 5, 0);
    node           = &work->objB50.obj;
    node->field_8  = &((TmdObject*)arg1->extra)->field_8[1];
    node->field_C  = firstRec;
    node->field_10 = 0;
    node->field_12 = 0;
    node->field_14 = 0;
    node->field_18 = 0x3001E;
    node->field_1C = 0x3E8;
    node->flags    = 1;
    Gp_LinkObj(2, &work->objB50.obj);
    node->flags |= 0x8000;
    Gp_InitRec18Table(node->field_C, 5, 0);
    node2           = &work->objBE8.obj;
    node2->field_8  = &((TmdObject*)arg1->extra)->field_8[15];
    node2->field_C  = work->objBE8.rec;
    node2->field_10 = 0;
    node2->field_12 = 0;
    node2->field_14 = 0;
    node2->field_18 = 0x3001E;
    node2->field_1C = 0x320;
    node2->flags    = 1;
    Gp_LinkObj(2, &work->objBE8.obj);
    node2->flags |= 0x8000;
    Gp_InitRec18Table(node2->field_C, 5, 0);
    node3           = &work->objC80.obj;
    node3->field_8  = &((TmdObject*)arg1->extra)->field_8[4];
    node3->field_C  = work->objC80.rec;
    node3->field_10 = 0;
    node3->field_12 = 0;
    node3->field_14 = 0;
    node3->field_18 = 0x3001E;
    node3->field_1C = 0x320;
    node3->flags    = 1;
    Gp_LinkObj(2, &work->objC80.obj);
    node3->flags |= 0x8000;
    Gp_InitRec18Table(node3->field_C, 5, 0);
    work->objBE8.obj.field_10 = 0;
    work->objBE8.obj.field_12 = 0;
    work->objBE8.obj.field_14 = -0x100;
    work->field_FC0           = 0;
    work->field_F88           = GameFlag_GetNibble(0xE2);
    Gfx_MatrixCol2(&((TmdObject*)arg1->extra)->field_8->coord, &dir);
    dir.vy = 0;
    dirp   = &dir;
    VectorNormalSS(dirp, dirp);
    gte_lddp(0x1388);
    gte_ldsv(dirp);
    gte_gpf12_real();
    gte_stsv(dirp);
    work->field_F90 = &D_actor_403000_80158C08;
    work->field_F94 = 1;
    work->field_F9C = 3;
    work->field_F98 = 0;
    work->field_FA0 = 1;
    work->field_F8C = 0;
    arg1->field_24  = &D_actor_403000_80158CA8;
    coord->sub      = &Gfx_ViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_FD2 = -1;
    work->field_FD3 = -1;
    work->field_0   = 0xB;
    arg1->state++;
}

void func_actor_403000_80134910(Actor403000* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         scratch;
    Actor403000Work* work;
    GpEffArg*        eff;
    s32              mag;

    scratch = (*(SVECTOR**)G_SCRATCH_HEAD -= 2);
    mag     = (arg1 >= 0) ? arg1 : -arg1;
    work    = arg0->field_1C;
    if (mag < 0x200) {
        switch ((s32)((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) {
            case 0:
                scratch[0] = D_actor_403000_80158C48[0];
                scratch[1] = D_actor_403000_80158C48[3];
                break;
            case 1:
                scratch[0] = D_actor_403000_80158C48[1];
                scratch[1] = D_actor_403000_80158C48[2];
                break;
            case 2:
                scratch[0] = D_actor_403000_80158C48[2];
                scratch[1] = D_actor_403000_80158C48[0];
                break;
            case 3:
                scratch[0] = D_actor_403000_80158C48[3];
                scratch[1] = D_actor_403000_80158C48[1];
                break;
            default:
                scratch[0] = D_actor_403000_80158C48[4];
                break;
        }
    } else if (mag > 0x600) {
        switch ((s32)((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 2) {
            case 0:
                scratch[0] = D_actor_403000_80158C48[5];
                scratch[1] = D_actor_403000_80158C48[6];
                break;
            case 1:
                scratch[0] = D_actor_403000_80158C48[6];
                scratch[1] = D_actor_403000_80158C48[7];
                break;
            default:
                scratch[0] = D_actor_403000_80158C48[7];
                scratch[1] = D_actor_403000_80158C48[5];
                break;
        }
    } else if (arg1 > 0) {
        scratch[0] = D_actor_403000_80158C48[8];
        scratch[1] = D_actor_403000_80158C48[9];
    } else {
        scratch[0] = D_actor_403000_80158C48[10];
        scratch[1] = D_actor_403000_80158C48[11];
    }
    work->field_FA8.field_0 = &arg0->field_2C->field_8[scratch[0].pad];
    work->field_FA8.field_4 = 0x500;
    work->field_FA8.field_6 = 3;
    eff                     = &work->field_FA8;
    func_800FDB18((u16)Gp_GetIdParam1(arg2), &arg0->field_2C->field_8[scratch[0].pad], &scratch[0], eff);
    work->field_FA8.field_0 = &arg0->field_2C->field_8[scratch[1].pad];
    work->field_FA8.field_4 = 0x400;
    work->field_FA8.field_6 = 2;
    func_800FDB18((u16)Gp_GetIdParam1(arg2), &arg0->field_2C->field_8[scratch[1].pad], &scratch[1], eff);
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

s32 func_actor_403000_80134E00(Actor403000* arg0)
{
    Actor403000Work* work;
    s16              flags;
    s16              i;
    VECTOR           d;

    work  = arg0->field_1C;
    flags = GameFlag_GetNibble(0xE2);
    if (flags == work->field_F88) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (((flags >> i) & 1) && !((work->field_F88 >> i) & 1)) {
            d.vx = arg0->field_2C->field_8->coord.t[0] - D_actor_403000_80158D64[i].vx;
            d.vz = arg0->field_2C->field_8->coord.t[2] - D_actor_403000_80158D64[i].vz;
            if (SquareRoot0(d.vx * d.vx + d.vz * d.vz) < 3000) {
                work->field_F88 = flags;
                return 1;
            }
        }
    }
    work->field_F88 = flags;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134F44);

void func_actor_403000_80135F08(Actor403000* arg0)
{
    Actor403000Work* work;
    GpObj5D*         obj;
    TmdObject*       tmd;
    u32              seed;

    work = arg0->field_1C;
    obj  = arg0->field_20;
    if (work->field_4 != 0) {
        tmd             = arg0->field_2C;
        work->field_FCA = 0;
        tmd->field_C    = 0;
        Tmd_AllocBuffers(tmd);
        work->field_ACA         = 0x10;
        work->field_AC6         = 0xF;
        work->field_AC0         = 2;
        work->field_6           = 0;
        work->objD18.obj.flags |= 0x4000;
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_403000_80133AF8(arg0);
    if (work->field_60.word & 0x102) {
        seed          = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState   = seed;
        work->field_6 = (seed >> 0x10) & 0x1F;
    }
    if ((s16)work->field_6 > 0) {
        work->field_6--;
        work->field_ACA = 0;
    } else {
        work->field_ACA = 0x10;
    }
    if ((Gp_TickObjFlag2(obj) == 1) || (obj->field_40 <= 0)) {
        obj->field_4C &= 0xFD;
        work->field_0  = 0x12;
    }
}

static __inline__ void Actor403000_FaceScale(GsCOORDINATE2* coord, s16 sy)
{
    Actor403000ScaleScratch* head;
    Actor403000ScaleScratch* scratch;

    head                                       = *(Actor403000ScaleScratch**)G_SCRATCH_HEAD;
    scratch                                    = head - 1;
    *(Actor403000ScaleScratch**)G_SCRATCH_HEAD = scratch;
    scratch->angle                             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&scratch->m, scratch->angle, 1);
    scratch->scale.vx = 0x1000;
    scratch->scale.vy = sy;
    scratch->scale.vz = 0x1000;
    ScaleMatrix(&scratch->m, &scratch->scale);
    coord->coord.m[0][0]                        = head[-1].m.m[0][0];
    coord->coord.m[0][1]                        = scratch->m.m[0][1];
    coord->coord.m[0][2]                        = scratch->m.m[0][2];
    coord->coord.m[1][0]                        = scratch->m.m[1][0];
    coord->coord.m[1][1]                        = scratch->m.m[1][1];
    coord->coord.m[1][2]                        = scratch->m.m[1][2];
    coord->coord.m[2][0]                        = scratch->m.m[2][0];
    coord->coord.m[2][1]                        = scratch->m.m[2][1];
    coord->coord.m[2][2]                        = scratch->m.m[2][2];
    coord->flg                                  = 0;
    *(Actor403000ScaleScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_403000_8013603C(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s16              t;

    work  = arg0->field_1C;
    tmd   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        tmd->field_C = 0;
        Tmd_AllocBuffers(tmd);
        work->field_FCA         = 1;
        work->objD18.obj.flags &= 0xBFFF;
        Gp_ClearNodeSlots(&enemy->node);
        work->field_6 = 0;
        Gp_SetLightMode((GpObj4C*)enemy, 0);
    }
    if (work->field_F8C == 1 && D_80114C12 != work->field_F8C && D_80071075 == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 0, 0);
        work->field_F8C = 0;
    }
    if ((s16)work->field_6 <= 0x1000) {
        work->field_6++;
        if ((s16)work->field_6 % 5 == 0 && (s16)work->field_6 < 130) {
            switch ((s16)((s16)((s16)work->field_6 / 5) % 4)) {
                case 0:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[1], 0x12600, NULL);
                    break;
                case 1:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[4], 0x22400, NULL);
                    break;
                case 2:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[1], 0x32600, NULL);
                    break;
                case 3:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[18], 0x12500, NULL);
                    break;
            }
        }
        switch ((s16)work->field_6) {
            case 1:
                arg0->field_2C->field_C = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                break;
            case 0x76:
                Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
                Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[4], 1, NULL);
                Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[18], 1, NULL);
                break;
            case 0x78:
                arg0->field_2C->field_C = 2;
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                arg0->field_2C->field_24 = 2;
                arg0->field_2C->field_25 = 4;
                break;
            case 0x88:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        t = work->field_6;
        if (t > 0x64) {
            if ((t - 0x64) * 0x3C < 0x1000) {
                Actor403000_FaceScale(arg0->field_2C->field_8, 0x1000 - (t - 0x64) * 0x6B);
            } else {
                Actor403000_FaceScale(arg0->field_2C->field_8, 0);
            }
        } else {
            Actor403000_FaceScale(arg0->field_2C->field_8, 0x1000);
        }
    }
}

void func_actor_403000_801365D0(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s16              t;

    work  = arg0->field_1C;
    tmd   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        tmd->field_C = 0;
        Tmd_AllocBuffers(tmd);
        work->field_FCA         = 1;
        work->objD18.obj.flags &= 0xBFFF;
        Gp_ClearNodeSlots(&enemy->node);
        work->field_6 = 0;
        Gp_SetLightMode((GpObj4C*)enemy, 0);
    }
    if ((s16)work->field_6 <= 0x1000) {
        work->field_6++;
        if ((s16)work->field_6 % 5 == 0 && (s16)work->field_6 < 100) {
            switch ((s16)((s16)((s16)work->field_6 / 5) % 4)) {
                case 0:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[1], 0x12600, NULL);
                    break;
                case 1:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[4], 0x22400, NULL);
                    break;
                case 2:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[1], 0x32600, NULL);
                    break;
                case 3:
                    Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[18], 0x12500, NULL);
                    break;
            }
        }
        switch ((s16)work->field_6) {
            case 1:
                arg0->field_2C->field_C = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                break;
            case 0x58:
                Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
                Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[4], 1, NULL);
                Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[18], 1, NULL);
                break;
            case 0x5A:
                arg0->field_2C->field_C = 2;
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                arg0->field_2C->field_24 = 2;
                arg0->field_2C->field_25 = 4;
                break;
            case 0x6A:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        t = work->field_6;
        if (t > 0x46) {
            if ((t - 0x46) * 0x3C < 0x1000) {
                Actor403000_FaceScale(arg0->field_2C->field_8, 0x1000 - (t - 0x46) * 0x6B);
            } else {
                Actor403000_FaceScale(arg0->field_2C->field_8, 0);
            }
        } else {
            Actor403000_FaceScale(arg0->field_2C->field_8, 0x1000);
        }
    }
}

void func_actor_403000_80136B14(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;

    work  = arg0->field_1C;
    tmd   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        tmd->field_C = 0;
        Tmd_AllocBuffers(tmd);
        work->objD18.obj.flags |= 0x4000;
        Gp_SetLightMode((GpObj4C*)enemy, 1);
        enemy->field_4C     = 0;
        work->field_ACA     = 0x10;
        work->field_AC6     = 0x1C;
        work->field_AC0     = 2;
        enemy->node.field_4 = 1;
        work->field_FCA     = 1;
        Gp_ClearNodeSlots(&enemy->node);
        arg0->field_2C->field_E = 8;
        work->field_6           = 0;
    }
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 < 0x28) {
        work->field_6++;
    }
    switch ((s16)work->field_6) {
        case 2:
            arg0->field_2C->field_8[1].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
            break;
        case 5:
            arg0->field_2C->field_8[12].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[12]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[12], 1, NULL);
            break;
        case 15:
            arg0->field_2C->field_8[16].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[16]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[16], 1, NULL);
            break;
        case 30:
            arg0->field_2C->field_8[1].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
            arg0->field_2C->field_E = 0;
            break;
    }
}

void func_actor_403000_80136D68(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;

    work  = arg0->field_1C;
    tmd   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        tmd->field_C = 0;
        Tmd_AllocBuffers(tmd);
        work->objD18.obj.flags |= 0x4000;
        Gp_SetLightMode((GpObj4C*)enemy, 1);
        enemy->field_4C     = 0;
        work->field_ACA     = 0x10;
        work->field_AC6     = 0x1C;
        work->field_AC0     = 2;
        enemy->node.field_4 = 1;
        work->field_FCA     = 1;
        Gp_ClearNodeSlots(&enemy->node);
        arg0->field_2C->field_E = 8;
        work->field_6           = 0;
    }
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 < 300) {
        work->field_6++;
        switch ((s16)work->field_6 % 24) {
            case 10:
            case 13:
            case 18:
            case 21:
                Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[1], 0x01032600, NULL);
                break;
            default:
                Gp_SpawnEff(0x60070, &arg0->field_2C->field_8[(s16)((s16)work->field_6 % 24)], 0x01032600, NULL);
                break;
        }
    }
    switch ((s16)work->field_6) {
        case 2:
            arg0->field_2C->field_8[1].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
            break;
        case 5:
            arg0->field_2C->field_8[12].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[12]);
            break;
        case 15:
            arg0->field_2C->field_8[16].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[16]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[16], 1, NULL);
            break;
        case 30:
            arg0->field_2C->field_8[1].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
            arg0->field_2C->field_E = 0;
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80137084);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801377C8);

/// Per-frame push: on the frame `field_4` is set, turn the display object's
/// first matrix column into a short push vector and play the enemy's sound,
/// then send it to the player as message 0x3FE for the first 0x28 frames.
/// Bit 0 of `field_60` moves the state machine to 4 and flips `field_FD3`.
void func_actor_403000_801384E8(Actor403000* arg0)
{
    Actor403000Work*        work;
    GpEnemy*                enemy;
    Task*                   player;
    Actor403000PushScratch* scratch;
    s32                     sound;
    s32                     pan;
    s32                     ret;

    work                                      = arg0->field_1C;
    player                                    = Game_GetPtrSlot(3);
    scratch                                   = *(Actor403000PushScratch**)G_SCRATCH_HEAD - 1;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD = scratch;
    if (work->field_4 != 0) {
        enemy         = arg0->field_20;
        work->field_6 = 0;
        Gfx_MatrixCol0(&arg0->field_2C->field_8->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(0x55);
        gte_ldsv(&scratch->dir);
        gte_gpf12_real();
        gte_stsv(&scratch->dir);
        D_actor_403000_80158DB0.x        = scratch->dir.vx;
        D_actor_403000_80158DB0.y        = 0;
        D_actor_403000_80158DB0.z        = scratch->dir.vz;
        D_actor_403000_80158DB0.field_10 = 7;
        D_actor_403000_80158DB0.field_12 = 1;
        sound                            = ((enemy->field_8 >> 0xC) << 8) | 7;
        pan                              = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    if ((s16)work->field_6 < 0x28) {
        ret = Gp_DispatchMsg(player, 0x3FE, (s32)&D_actor_403000_80158DB0, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.x        = 0;
            D_actor_403000_80158DB0.y        = 0;
            D_actor_403000_80158DB0.z        = 0;
            D_actor_403000_80158DB0.field_10 = 7;
            D_actor_403000_80158DB0.field_12 = ret;
        }
    }
    if (work->field_60.half & 1) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD5 = work->field_FD2 = -work->field_FD3;
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801386E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80138DB0);

void func_actor_403000_801399A0(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C   = 0;
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_AC6           = 0xE;
        work->field_ACA           = 0x10;
        work->field_FCA           = 0;
        work->field_AE0           = 0;
        work->field_AD8           = 0;
        work->field_AD6           = 0;
        work->objD18.obj.flags   |= 0x4000;
        work->objB50.obj.flags   |= 0x4000;
    }
    if (func_actor_403000_80132348(arg0->field_2C->field_8, work->objD18.rec, 5) == 0) {
        func_actor_403000_80132348(arg0->field_2C->field_8, work->objB50.rec, 5);
    }
    func_actor_403000_80133AF8(arg0);
    if ((work->field_60.half & 0x100) && work->field_AC6 == 0xE) {
        work->objB50.obj.flags &= 0xBFFF;
        if (enemy->field_40 > 0) {
            if (enemy->field_4C & 2) {
                work->field_0 = 0x10;
            } else {
                work->field_0 = 0x12;
            }
        } else {
            work->field_F8C = 1;
            work->field_0   = 0x14;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80139AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A08C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A678);

/// Waypoint-grid cell under `coord`: column by `coord.t[0]` band, row by
/// `coord.t[2]`, as `func_actor_403000_80134204` computes it inline.
static inline s8 Actor403000_Cell(GsCOORDINATE2* coord)
{
    s32 x;
    s32 z;
    s8  col;
    s8  row;

    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x < 0xD48) {
        col = 4;
    } else if (x < 0x1A90) {
        col = 3;
    } else if (x < 0x2AF8) {
        col = 2;
    } else {
        col = x < 0x3C8C;
    }
    row = z >= 0x1068;
    return D_actor_403000_80158D48[col + row * 5];
}

/// Walk the waypoint ring: on the entry frame snap the model onto its cell's
/// waypoint and face the neighbour in the `field_FD3` direction; every frame
/// finish (state 2) on reaching the player's cell, give up (14) after 300
/// frames, or switch to 5 once `func_actor_403000_80133FC0` allows it.
void func_actor_403000_8013ACBC(Actor403000* arg0)
{
    Actor403000Work*        work;
    Actor403000*            player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s16                     index;
    s8                      base;
    SVECTOR*                last;

    work    = arg0->field_1C;
    player  = Game_GetPtrSlot(3);
    scratch = (*(Actor403000SeekScratch**)G_SCRATCH_HEAD -= 1);
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        work->field_FCA = 0;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_ACA           = 0x10;
        work->field_AC2           = 0;
        work->field_AC6           = 4;
        work->field_AD6           = 0;
        work->objD18.obj.flags   |= 0x4000;
        func_actor_403000_80133AF8(arg0);
        base                                = Actor403000_Cell(arg0->field_2C->field_8);
        scratch->base                       = base;
        table                               = D_actor_403000_80158CE0;
        v                                   = &table[base];
        scratch->vec.vx                     = v->vx;
        scratch->vec.vy                     = v->vy;
        scratch->vec.vz                     = v->vz;
        arg0->field_2C->field_8->coord.t[0] = scratch->vec.vx;
        arg0->field_2C->field_8->coord.t[1] = scratch->vec.vy;
        arg0->field_2C->field_8->coord.t[2] = scratch->vec.vz;
        arg0->field_2C->field_8->flg        = 0;
        if (work->field_FD3 == 1) {
            if (scratch->base + 1 >= 10) {
                scratch->vec.vx = table[0].vx;
                scratch->vec.vy = table[0].vy;
                scratch->vec.vz = table[0].vz;
            } else {
                index           = scratch->base + 1;
                scratch->vec.vx = table[index].vx;
                scratch->vec.vy = table[index].vy;
                scratch->vec.vz = table[index].vz;
            }
        } else {
            if (scratch->base - 1 < 0) {
                last            = &table[9];
                scratch->vec.vx = last->vx;
                scratch->vec.vy = last->vy;
                scratch->vec.vz = last->vz;
            } else {
                index           = scratch->base - 1;
                scratch->vec.vx = table[index].vx;
                scratch->vec.vy = table[index].vy;
                scratch->vec.vz = table[index].vz;
            }
        }
        scratch->vec.vx -= arg0->field_2C->field_8->coord.t[0];
        scratch->vec.vy  = 0;
        scratch->vec.vz -= arg0->field_2C->field_8->coord.t[2];
        coord            = arg0->field_2C->field_8;
        angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
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
        scratch->angle  = angle;
        work->field_AD8 = 0;
        scratch->angle += ratan2(-arg0->field_2C->field_8->coord.m[2][0], arg0->field_2C->field_8->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, scratch->angle, 1);
        work->field_6        = 0;
        work->field_8        = 0;
        work->field_FC2      = 0;
        work->recDD0.field_C = 0x320;
        scratch->facing      = Actor403000_Cell(player->field_2C->field_8);
    }
    work->field_6++;
    scratch->facing = Actor403000_Cell(player->field_2C->field_8);
    scratch->base   = Actor403000_Cell(arg0->field_2C->field_8);
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 > 300) {
        work->field_0 = 14;
    } else if (scratch->facing == scratch->base) {
        work->field_0   = 2;
        work->field_FD2 = work->field_FD3;
        work->field_FD3 = -work->field_FD3;
    } else if (func_actor_403000_80133FC0((Task*)arg0, scratch->base, scratch->facing) << 16) {
        if ((s16)work->field_6 > 60) {
            work->field_0 = 5;
        }
    }
    *(Actor403000SeekScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn toward the next waypoint: on the entry frame pick it from the grid
/// offset by `field_FD5`, set state 2 if it is within a quarter turn, and fold
/// the display object's scaled first and third matrix columns into the drift
/// `field_FB0`; every frame re-aim by `field_FC8` and apply that drift.
void func_actor_403000_8013B238(Actor403000* arg0)
{
    Actor403000Work*       work;
    Actor403000AimScratch* scratch;
    GsCOORDINATE2*         coord;
    s32                    x;
    s32                    z;
    s8                     col;
    s32                    b;
    s8                     row;
    TmdObject*             obj;
    SVECTOR*               v;
    SVECTOR*               table;
    s32                    mag;
    Actor403000AimScratch* head;
    s16                    angle;

    head                                     = *(Actor403000AimScratch**)G_SCRATCH_HEAD;
    work                                     = arg0->field_1C;
    *(Actor403000AimScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                                  = head - 1;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        work->field_FCA = 0;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_ACA           = 0x10;
        work->field_AC2           = 0;
        work->field_AC6           = 9;
        work->field_AD6           = 0;
        work->objD18.obj.flags   |= 0x4000;
        x                         = arg0->field_2C->field_8->coord.t[0];
        z                         = arg0->field_2C->field_8->coord.t[2];
        if (x < 0xD48) {
            col = 4;
        } else if (x < 0x1A90) {
            col = 3;
        } else if (x < 0x2AF8) {
            col = 2;
        } else {
            col = x < 0x3C8C;
        }
        row            = z >= 0x1068;
        b              = (s8)D_actor_403000_80158D48[col + row * 5];
        scratch->base  = b;
        scratch->index = scratch->base + work->field_FD5;
        if (scratch->index >= 10) {
            scratch->index -= 10;
        } else if (scratch->index < 0) {
            scratch->index += 10;
        }
        table            = D_actor_403000_80158CE0;
        v                = &table[scratch->index];
        scratch->vec.vx  = v->vx;
        scratch->vec.vy  = v->vy;
        scratch->vec.vz  = v->vz;
        scratch->vec.vx -= arg0->field_2C->field_8->coord.t[0];
        scratch->vec.vy -= arg0->field_2C->field_8->coord.t[1];
        scratch->vec.vz -= arg0->field_2C->field_8->coord.t[2];
        coord            = arg0->field_2C->field_8;
        angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
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
        mag            = angle;
        scratch->angle = mag;
        if (ABS(mag) < 0x400) {
            work->field_0 = 2;
        }
        if (scratch->angle < 0) {
            scratch->angle += 0x1000;
        }
        Gfx_MatrixCol0(&arg0->field_2C->field_8->coord, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(0x1B);
        gte_ldsv(&scratch->vec);
        gte_gpf12_real();
        gte_stsv(&scratch->vec);
        work->field_FB0 = scratch->vec;
        Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(-0x29);
        gte_ldsv(&scratch->vec);
        gte_gpf12_real();
        gte_stsv(&scratch->vec);
        work->field_FC8     = scratch->angle / 36;
        work->field_FB0.vx += scratch->vec.vx;
        work->field_FB0.vy += scratch->vec.vy;
        work->field_FB0.vz += scratch->vec.vz;
        work->field_AEB     = 0;
        work->field_AEA     = 0;
        work->field_AE9     = 0;
        work->field_AE8     = 0;
        work->field_6       = 0;
    }
    work->field_6++;
    func_actor_403000_80132348(arg0->field_2C->field_8, work->objD18.rec, 5);
    scratch->angle = work->field_FC8 + ratan2(-arg0->field_2C->field_8->coord.m[2][0], arg0->field_2C->field_8->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, scratch->angle, 1);
    arg0->field_2C->field_8->coord.t[0] += work->field_FB0.vx;
    arg0->field_2C->field_8->coord.t[1] += work->field_FB0.vy;
    arg0->field_2C->field_8->coord.t[2] += work->field_FB0.vz;
    arg0->field_2C->field_8->flg         = 0;
    func_actor_403000_80133AF8(arg0);
    if (work->field_60.half & 1) {
        work->field_0 = 2;
    }
    *(Actor403000AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B74C);

/// Ease the display object up toward the player and across to
/// `field_F74`/`field_F78`, turn the player's third matrix column into the
/// push vector for the first 8 frames, and once bit 0 of `field_60` is set
/// after frame 0xB move the state machine to 4.
void func_actor_403000_8013BDE0(Actor403000* arg0)
{
    Actor403000Work*        work;
    Actor403000*            player;
    Actor403000PushScratch* scratch;
    GpEnemy*                enemy;
    s32                     sound;
    s32                     pan;

    work                                      = arg0->field_1C;
    player                                    = Game_GetPtrSlot(3);
    scratch                                   = *(Actor403000PushScratch**)G_SCRATCH_HEAD - 1;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD = scratch;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        work->field_FCA = 0;
        work->field_6   = 0;
        sound           = ((enemy->field_8 >> 0xC) << 8) | 7;
        pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    if (arg0->field_2C->field_8->coord.t[1] < player->field_2C->field_8->coord.t[1]) {
        arg0->field_2C->field_8->coord.t[1] += 0x12C;
        arg0->field_2C->field_8->coord.t[0] += (work->field_F74 - arg0->field_2C->field_8->coord.t[0]) >> 2;
        arg0->field_2C->field_8->coord.t[2] += (work->field_F78 - arg0->field_2C->field_8->coord.t[2]) >> 2;
    }
    if ((s16)work->field_6 < 8) {
        Gfx_MatrixCol2(&player->field_2C->field_8->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(-0x2A);
        gte_ldsv(&scratch->dir);
        gte_gpf12_real();
        gte_stsv(&scratch->dir);
    }
    D_actor_403000_80158DB0.x        = scratch->dir.vx;
    D_actor_403000_80158DB0.y        = 0;
    D_actor_403000_80158DB0.z        = scratch->dir.vz;
    D_actor_403000_80158DB0.field_10 = 7;
    D_actor_403000_80158DB0.field_12 = 1;
    if ((work->field_60.half & 1) && (s16)work->field_6 >= 0xB) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD2 = work->field_FD5 = -func_actor_403000_80134204(arg0->field_2C->field_8);
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_403000_8013C050(Actor403000* arg0)
{
    Actor403000Work* work;
    Actor403000*     player;
    GpEnemy*         enemy;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;

    work   = arg0->field_1C;
    player = Game_GetPtrSlot(3);
    enemy  = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_FD6                     = 0;
        arg0->field_2C->field_8->coord.t[0] = 0x2134;
        arg0->field_2C->field_8->coord.t[1] = player->field_2C->field_8->coord.t[1] - 0x1518;
        arg0->field_2C->field_8->coord.t[2] = 0x1194;
        work->field_FCA                     = 1;
        work->field_AC6                     = 8;
        work->field_AC0                     = 2;
        work->field_6                       = 0;
        work->field_8                       = 0;
        work->field_ACA                     = 0;
        func_actor_403000_80133AF8(arg0);
    }
    if ((s16)work->field_6 > 0x3C) {
        work->field_FD9 = 0x14;
        sound           = ((enemy->field_8 >> 0xC) << 8) | 0x401E0010;
        pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_0 = 0xC;
    }
    if ((s16)work->field_6 > 0x64) {
        if (work->field_FB8 == arg0->field_2C->field_8->coord.t[0] &&
            work->field_FBA == arg0->field_2C->field_8->coord.t[1] &&
            work->field_FBC == arg0->field_2C->field_8->coord.t[2]) {
            work->field_8++;
        } else {
            work->field_8 = 0;
        }
        if (work->field_8 > 0x1E) {
            work->field_FD9 = 0x14;
            sound2          = ((enemy->field_8 >> 0xC) << 8) | 0x401E0010;
            pan2            = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
            SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
            work->field_0 = 0xC;
        }
        work->field_FB8 = arg0->field_2C->field_8->coord.t[0];
        work->field_FBA = arg0->field_2C->field_8->coord.t[1];
        work->field_FBC = arg0->field_2C->field_8->coord.t[2];
    }
    work->field_6++;
}

/// Seek the next waypoint: on the entry frame restart the animation and latch
/// the steering from `func_actor_403000_80134204`; every frame compare the
/// player's and the actor's grid cells, turn at most 8 units toward the chosen
/// waypoint and step forward.
void func_actor_403000_8013C2D4(Actor403000* arg0)
{
    Actor403000Work*        work;
    Actor403000*            player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s32                     mag;
    s8                      base;

    work    = arg0->field_1C;
    player  = Game_GetPtrSlot(3);
    scratch = (*(Actor403000SeekScratch**)G_SCRATCH_HEAD -= 1);
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        work->field_FCA = 0;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_ACA           = 0x10;
        work->field_AC2           = 0;
        work->field_AC6           = 1;
        work->field_AD6           = 0;
        work->objD18.obj.flags   |= 0x4000;
        func_actor_403000_80133AF8(arg0);
        work->field_6        = 0;
        work->field_8        = 0;
        work->field_FC2      = 0;
        work->recDD0.field_C = 0x320;
        scratch->facing      = Actor403000_Cell(player->field_2C->field_8);
        scratch->base        = Actor403000_Cell(arg0->field_2C->field_8);
        work->field_FD1      = func_actor_403000_80134204(arg0->field_2C->field_8);
    }
    func_actor_403000_80132348(arg0->field_2C->field_8, work->objD18.rec, 5);
    scratch->facing = Actor403000_Cell(player->field_2C->field_8);
    base            = Actor403000_Cell(arg0->field_2C->field_8);
    scratch->base   = base;
    if (func_actor_403000_80133FC0((Task*)arg0, base, scratch->facing) << 16) {
        work->field_0 = 5;
    }
    scratch->index = scratch->base + work->field_FD1;
    if (scratch->index != -1) {
        if (scratch->index == 10) {
            scratch->index = 0;
        }
    } else {
        scratch->index = 9;
    }
    table            = D_actor_403000_80158CE0;
    v                = &table[scratch->index];
    scratch->vec.vx  = v->vx;
    scratch->vec.vy  = v->vy;
    scratch->vec.vz  = v->vz;
    scratch->vec.vx -= arg0->field_2C->field_8->coord.t[0];
    scratch->vec.vy  = 0;
    scratch->vec.vz -= arg0->field_2C->field_8->coord.t[2];
    coord            = arg0->field_2C->field_8;
    angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
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
    mag             = angle;
    scratch->angle  = mag;
    work->field_AD8 = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->angle > 8) {
        scratch->angle = 8;
    }
    if (scratch->angle < -8) {
        scratch->angle = -8;
    }
    scratch->angle += ratan2(-arg0->field_2C->field_8->coord.m[2][0], arg0->field_2C->field_8->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, scratch->angle, 1);
    arg0->field_2C->field_8->flg = 0;
    Actor00100_MoveForward(arg0->field_2C->field_8, 0x16);
    *(Actor403000SeekScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C864);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", ActorsShared80135df4Table);

void func_actor_403000_8013D260(void)
{
}
