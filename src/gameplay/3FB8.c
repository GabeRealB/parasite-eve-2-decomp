#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#define gte_rtv0_real()   __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()   __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()   __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")
#define gte_gpf12_real()  __asm__ volatile("nop; nop; .word 0x4B98003D")

extern TaskDesc       D_80113340[];
extern GpEffArg       D_80113358;
extern TaskFuncTable3 Gp_EffTask07States;
extern u16            Gp_WeaponIdBase[];
extern GpAnimBlk*     Gp_PlayerAnimBlkTbl[];
extern u16            D_80112DF4[];
extern u16            D_80113F9C[];
extern u16            Gp_AllyIdBase[];
extern void*          Gp_AnimBlkTbl[];
extern s32            Gp_LcgState;

s32  Gp_ApplyHpDamage(s32 arg0);
void Gp_TickActorAnimState(GpActorWork* arg0);
void Gp_StepPlayerMove(GpActorWork* arg0);
void Gp_TurnPlayer(GpActorWork* arg0);
void Gp_AimYawToLock(GpActorWork* arg0, s32 arg1);
void Gp_AimPitchToLock(GpActorWork* arg0);
void Gp_AimPitchToLockAlt(GpActorWork* arg0);
void Gp_AimPitchRec(GpActorWork* arg0, s32 arg1, s32 arg2);
void Gp_AnimResetChildSlots(GpActorWork* arg0, s32 arg1);
void Gp_AnimPlayChildSlots(GpActorWork* arg0, s32 arg1, s32 arg2);
void Gp_AnimPlayChildSlotsEx(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void Gp_AnimTickChildSlots(GpActorWork* arg0);
s16  func_80103E7C(s16 arg0, s16 arg1);
void Gp_TrackLockTarget(GpActorWork* arg0);
void Gp_PlaceCoordOffset(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, SVECTOR* arg2);
s32  func_80104B54(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2);
s32  func_80104E00(GpActorWork* arg0, s32 arg1, GpXformArg* arg2);
s32  Gp_SetActorDest(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3);
s32  Gp_MoveActorBy(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2);
s32  func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_80105690(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_8010583C(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_80105894(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_80105B0C(GpActorWork* arg0);
s32  Gp_PickNearestRec18(GpRec18* arg0, GsCOORDINATE2* arg1, GsCOORDINATE2* arg2);
s32  func_80105ED4(GpActorWork* arg0);
void func_8010615C(GpActorWork* arg0);
void func_801066DC(GpActorWork* arg0, s16 arg1);
void Gp_PlayerMode2State4(GpActorWork* arg0);
void func_80109210(GpActorWork* arg0);
void func_80109250(GpActorWork* arg0);
void func_80109FC4(GpActorWork* arg0);
void func_8010A670(GpActorWork* arg0);
void func_80108684(GpActorWork* arg0);
void func_8010870C(GpActorWork* arg0, s32 arg1);
void Gp_ResetActorAnimState(GpActorWork* arg0, s32 arg1);
void func_80108874(GpActorWork* arg0);
void func_80108E0C(GpActorWork* arg0, GpLinkNode* arg1);
void func_80109374(GpActorWork* arg0);
void Gp_UpdateLockTarget(GpActorWork* arg0);
void Gp_TickPlayerActor(GpActorWork* arg0);
void func_801095BC(s32* arg0);
void func_80109720(GpActorWork* arg0);
void func_80109844(GpActorWork* arg0);
void func_80109A1C(GpActorWork* arg0);
void func_80109BB4(GpActorWork* arg0, GpRec18* arg1);
void Gp_StopPlayerAnim(GpActorWork* arg0, s32 arg1);
void func_8010AAB4(GpActorWork* arg0);
void func_8010ABD4(GpActorWork* arg0);
void func_8010AC54(GpActorWork* arg0);
void func_8010AD64(GpActorWork* arg0);
void Gp_PlayerStepSfx(GpActorWork* arg0);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void Gp_DrawEffTri(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3);
void func_800FCD00(Task* arg0);

void Gp_EffSprTask55(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    POLY_FT4*         prim;
    s16               flag;
    s32               t;
    s32               amt;
    s32               rng;
    s32               temp;
    s32               pal;
    u16               vz;
    SVECTOR*          vec;
    s32               t2;
    u16               y;
    s16               next;
    s32               n32;
    s32               lo;
    s32               quot;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
    } else {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        USE_REG(head);
        {
            register u16 vx asm("v0");
            vx                                         = *(u16*)&coord->workm.t[0];
            ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
        }
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x1C;
            block = (GpEffBeamScratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            block->otz     = block->otz + 1;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                t   = (u16)arg0->spawnArg1 & 0xFFF;
                amt = 0x200;
                if (t != 0) {
                    amt = t;
                }
                rng           = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = amt;
                mem->field_26 = ((u32)rng >> 16) & 0xFFF;
                temp          = ((u16)arg0->spawnArg1 & 0xF000) << 16;
                Gp_LcgState   = rng;
                if (temp != 0) {
                    temp = temp >> 28;
                } else {
                    temp = 1;
                }
                mem->field_28 = temp;
                pal           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_2A = pal & 3;
                mem->field_20 = (arg0->spawnArg1 >> 28) & 1;
                lo            = ((u16)mem->field_26 & 0xF) * rsin(mem->field_26);
                mem->field_12 = (u16)mem->field_12 - 0x18;
                mem->field_10 = (u16)mem->field_10 + (lo >> 12);
                mem->field_14 = (u16)mem->field_14 + ((((u16)mem->field_26 & 0xF) * rcos(mem->field_26)) >> 12);
                if (arg0->spawnArg1 & 0x100000) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_12 = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_14 = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                }
                if (arg0->spawnArg1 & 0x01000000) {
                    gte_lddp(mem->field_24 << 2);
                    vec = (SVECTOR*)&mem->field_10;
                    gte_ldsv(vec);
                    gte_gpf12_real();
                    gte_stsv(vec);
                }
                arg0->state = 1;
            }
            prim->code |= 3;
            prim->tpage = ((mem->field_2A & 3) << 5) | 9;
            prim->clut  = ((mem->field_20 * 14) & 0x42BE) | 0x4281;
            quot        = mem->field_22 / mem->field_28;
            prim->v0    = 0x78;
            prim->u0    = quot << 5;
            quot        = mem->field_22 / mem->field_28;
            prim->v1    = 0x78;
            prim->u1    = (quot << 5) + 0x1F;
            quot        = mem->field_22 / mem->field_28;
            prim->v2    = 0x97;
            prim->u2    = quot << 5;
            quot        = mem->field_22 / mem->field_28;
            prim->v3    = 0x97;
            prim->u3    = (quot << 5) + 0x1F;
            block->dx   = (((mem->field_24 * 0x1F) / block->otz) * rsin(mem->field_26)) >> 12;
            block->dy   = (((mem->field_24 * 0x1F) / block->otz) * rcos(mem->field_26)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_24 * 0x1F) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            block->dy   = (((mem->field_24 * 0x1F) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        t2                 = coord->coord.t[2] + mem->field_14;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        y                  = (u16)mem->field_12 + 6;
        next               = (u16)mem->field_22 + 1;
        USE_REG2(y, next);
        mem->field_22 = next;
        n32           = next;
        mem->field_12 = y;
        if ((mem->field_28 * 8 - 1) < n32) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void Gp_EffSprTask42(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    POLY_FT4*         prim;
    s16               flag;
    s32               t;
    s32               amt;
    s32               rng;
    s32               temp;
    s32               pal;
    u16               vz;
    SVECTOR*          vec;
    s32               t2;
    u16               y;
    s16               next;
    s32               n32;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
    } else {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        USE_REG(head);
        {
            register u16 vx asm("v0");
            vx                                         = *(u16*)&coord->workm.t[0];
            ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
        }
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x1C;
            block = (GpEffBeamScratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            block->otz     = block->otz + 1;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                t   = (u16)arg0->spawnArg1 & 0xFFF;
                amt = 0x200;
                if (t != 0) {
                    amt = t;
                }
                rng           = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = amt;
                mem->field_26 = ((u32)rng >> 16) & 0xFFF;
                temp          = ((u16)arg0->spawnArg1 & 0xF000) << 16;
                Gp_LcgState   = rng;
                if (temp != 0) {
                    temp = temp >> 28;
                } else {
                    temp = 1;
                }
                mem->field_28 = temp;
                pal           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_2A = pal & 3;
                mem->field_12 = mem->field_12 - 0x18;
                if (arg0->spawnArg1 & 0x100000) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_12 = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_14 = (((u32)Gp_LcgState >> 16) & 0x1F) - 0x10;
                }
                if (arg0->spawnArg1 & 0x01000000) {
                    gte_lddp(mem->field_24 << 2);
                    vec = (SVECTOR*)&mem->field_10;
                    gte_ldsv(vec);
                    gte_gpf12_real();
                    gte_stsv(vec);
                }
                arg0->state = 1;
            }
            prim->code |= 3;
            COMPILER_BARRIER();
            {
                s32 tp;
                tp          = ((mem->field_2A & 3) << 5) | 9;
                prim->clut  = 0x4285;
                prim->tpage = tp;
            }
            prim->u0  = (mem->field_22 / mem->field_28) * 0x10 - 0x80;
            prim->v0  = 0xB8;
            prim->u1  = (mem->field_22 / mem->field_28) * 0x10 - 0x71;
            prim->v1  = 0xB8;
            prim->u2  = (mem->field_22 / mem->field_28) * 0x10 - 0x80;
            prim->v2  = 0xC7;
            prim->u3  = (mem->field_22 / mem->field_28) * 0x10 - 0x71;
            prim->v3  = 0xC7;
            block->dx = (((mem->field_24 * 15) / block->otz) * rsin(mem->field_26)) >> 12;
            block->dy = (((mem->field_24 * 15) / block->otz) * rcos(mem->field_26)) >> 12;
            prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx = (((mem->field_24 * 15) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            block->dy = (((mem->field_24 * 15) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        t2                 = coord->coord.t[2] + mem->field_14;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        y                  = (u16)mem->field_12 + 6;
        next               = (u16)mem->field_22 + 1;
        USE_REG2(y, next);
        mem->field_22 = next;
        n32           = next;
        mem->field_12 = y;
        if ((mem->field_28 * 8 - 1) < n32) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F91AC);

void Gp_EffCtlTask9B(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    GpEffWork*     spawned;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag < 4) {
        if (arg0->state == 0) {
            coord->sub        = mem->field_8;
            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;
            coord->flg        = 0;
            arg0->state       = 1;
            mem->field_24     = ((u16)arg0->spawnArg1 * 3u) >> 4;
            temp              = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_26     = temp;
            mem->field_28     = temp << 2;
            if (((u16)mem->field_18 | (u16)mem->field_1A | (u16)mem->field_1C) == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_18 = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1A = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1C = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
            }
            VectorNormalSS((SVECTOR*)&mem->field_18, (SVECTOR*)&mem->field_10);
        }
        Gp_UpdateCoord(coord);
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        if (mem->field_22 < mem->field_28) {
            goto spawn;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
    return;
spawn:
    spawned = Gp_SpawnEff(0x60055, coord, 0x12200, 0);
    if (spawned != NULL) {
        gte_lddp(mem->field_24 - mem->field_22 * (mem->field_26 + 5));
        gte_ldsv(&mem->field_10);
        gte_gpf12_real();
        gte_stsv(&spawned->field_10);
    }
    mem->field_22++;
}

void Gp_EffSprTask30(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s16            flag;
    s32            sub;
    s32            ret;
    s32            id;
    s32            base;
    SVECTOR        vec;
    SVECTOR        dir;
    SVECTOR        wpos;
    u8             color[3];

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    Gp_UpdateCoord(coord);
    mem->field_22++;
    switch (arg0->state) {
        case 0:
            rot           = (GpMtxWords*)&coord->coord;
            rot->w0       = 0x1000;
            rot->w2       = 0x1000;
            rot->h4       = 0x1000;
            rot->w1       = 0;
            rot->w3       = 0;
            mem->field_18 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            mem->field_24 = 0x100;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_1A = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_20 = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_1C = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_28 = 0x200 - (((u32)Gp_LcgState >> 16) & 0x3FF);
            if (((u16)mem->field_10 | (u16)mem->field_12 | (u16)mem->field_14) == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->field_10);
                gte_rtv0_real();
                gte_stsv(&mem->field_10);
            }
            VectorNormalSS((SVECTOR*)&mem->field_10, (SVECTOR*)&mem->field_10);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            sub             = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            arg0->state     = 1;
            arg0->spawnArg1 = sub & 3;
            return;
        case 1:
            if (mem->field_22 >= 0x51) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            mem->field_1C += mem->field_28;
            if (mem->field_1A != 0 && mem->field_22 % mem->field_1A == 0) {
                mem->field_20++;
            }
            gte_lddp(mem->field_24);
            gte_ldsv(&mem->field_10);
            gte_gpf12_real();
            gte_stsv(&vec);
            coord->coord.t[0] += vec.vx;
            coord->coord.t[1] += vec.vy;
            coord->coord.t[2] += vec.vz;
            coord->flg         = 0;
            gte_SetRotMatrix(&Gfx_ViewWorldMtx);
            gte_ldv0(&vec);
            gte_rtv0_real();
            gte_stsv(&dir);
            wpos.vx = *(u16*)&coord->workm.t[0];
            wpos.vy = *(u16*)&coord->workm.t[1];
            wpos.vz = *(u16*)&coord->workm.t[2];
            dir.vx += wpos.vx;
            dir.vy += wpos.vy;
            dir.vz += wpos.vz;
            ret     = func_800DE7CC(&dir, &wpos, &dir, &wpos);
            if (ret == 1) {
                coord->coord.t[0] -= vec.vx;
                coord->coord.t[1] -= vec.vy;
                coord->coord.t[2] -= vec.vz;
                mem->field_10      = ((s16)(u16)wpos.vx >> 1) + ((s16)(u16)mem->field_10 >> 1);
                mem->field_12      = wpos.vy + ((s16)(u16)mem->field_12 >> 1);
                mem->field_14      = ((s16)(u16)wpos.vz >> 1) + ((s16)(u16)mem->field_14 >> 1);
                VectorNormalSS((SVECTOR*)&mem->field_10, (SVECTOR*)&mem->field_10);
                mem->field_24 = (s16)(u16)mem->field_24 >> 1;
                mem->field_28 = (s16)(u16)mem->field_28 >> 1;
                gte_lddp(mem->field_24);
                gte_ldsv(&mem->field_10);
                gte_gpf12_real();
                gte_stsv(&vec);
                coord->coord.t[0] += vec.vx;
                coord->coord.t[1] += vec.vy;
                coord->coord.t[2] += vec.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 1) != 0) {
                    if (arg0->spawnArg1 == 1) {
                        base = 0xC0001100;
                        id   = 0x60070;
                    } else {
                        base = 0x12100;
                        id   = 0x60055;
                    }
                    Gp_SpawnEff(id, coord, mem->field_18 + base, NULL);
                }
                if (mem->field_22 - mem->field_2A < 8 && mem->field_24 < 0x20) {
                    arg0->state   = arg0->spawnArg1 + 2;
                    mem->field_24 = 0;
                    mem->field_28 = 0;
                    mem->field_26 = 0x80;
                } else {
                    mem->field_2A = mem->field_22;
                }
            } else if (mem->field_24 != 0) {
                Gp_UpdateCoord(coord);
                mem->field_12 += 0x10000 / mem->field_24;
            }
            if (arg0->spawnArg1 == 2) {
                color[0] = color[1] = color[2] = 0x80;
                Gp_DrawEffSpark(arg0, arg0->spawnArg1, color);
            } else {
                Gp_DrawEffSpark(arg0, arg0->spawnArg1, NULL);
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->field_22 >= 0x33) {
                if (mem->field_26 < 0x10) {
                    mem->field_24++;
                    if (mem->field_24 < 8) {
                        Gp_DrawEffQuadT29(coord, mem->field_28, mem->field_24, 0);
                    } else {
                        Gp_ReleaseState1CMem(mem, arg0);
                    }
                } else {
                    u16 rnd;

                    color[0] = color[1] = color[2] = mem->field_26;
                    Gp_DrawEffSpark(arg0, arg0->spawnArg1, color);
                    mem->field_28 += (s16)(u16)mem->field_18 >> 4;
                    Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                    rnd            = ((u32)Gp_LcgState >> 16) % 3;
                    if (rnd == 0) {
                        Gp_SpawnEff(0x600A7, coord, mem->field_18, NULL);
                    }
                    Gp_DrawEffQuadT29(coord, mem->field_28, 0, 0);
                    mem->field_26 -= 0x10;
                }
            } else {
                Gp_DrawEffSpark(arg0, arg0->spawnArg1, NULL);
            }
            return;
        case 3:
            Gp_UpdateCoord(coord);
            if (mem->field_22 >= 0x33) {
                if (mem->field_26 < 0x10) {
                    Gp_ReleaseState1CMem(mem, arg0);
                    return;
                }
                color[0] = color[1] = color[2] = mem->field_26;
                Gp_DrawEffSpark(arg0, arg0->spawnArg1, color);
                mem->field_26 -= 0x10;
            } else {
                Gp_DrawEffSpark(arg0, arg0->spawnArg1, NULL);
            }
            return;
        case 4:
            Gp_UpdateCoord(coord);
            if (mem->field_22 >= 0x33) {
                if (mem->field_26 < 0x10) {
                    mem->field_24++;
                    if (mem->field_24 < 8) {
                        Gp_DrawEffQuadT29(coord, mem->field_28, mem->field_24, 0);
                    } else {
                        Gp_ReleaseState1CMem(mem, arg0);
                    }
                } else {
                    u16 rnd;

                    color[0] = color[1] = color[2] = mem->field_26;
                    Gp_DrawEffSpark(arg0, arg0->spawnArg1, color);
                    mem->field_28 += (s16)(u16)mem->field_18 >> 4;
                    Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                    rnd            = ((u32)Gp_LcgState >> 16) % 3;
                    if (rnd == 0) {
                        Gp_SpawnEff(0x600A7, coord, mem->field_18, NULL);
                    }
                    Gp_DrawEffQuadT29(coord, mem->field_28, 0, 0);
                    mem->field_26 -= 0x10;
                }
            } else {
                color[0] = color[1] = color[2] = mem->field_26;
                Gp_DrawEffSpark(arg0, arg0->spawnArg1, color);
            }
            return;
    }
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl Gp_EffTask07States\n"
        "Gp_EffTask07States:\n"
        "\t.word Gp_EffTask07State0\n"
        "\t.word Gp_EffTask07State1\n"
        "\t.word Task_Kill\n"
        ".section .text\n");
#endif

void Gp_DrawEffSpark(Task* arg0, s32 arg1, u8* arg2)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffWork*        mem;
    TmdObject*        extra;
    GsCOORDINATE2*    coord;
    POLY_FT4*         prim;
    u16               abr;
    s32               uv;
    s32               uv2;
    u8                green;
    u8                blue;
    s32               ang;
    u16               size;
    u16               frame;
    u16               angle;
    u16               vz;

    extra                                      = (TmdObject*)arg0->extra;
    mem                                        = arg0->spawnArg2;
    scratch                                    = (void**)G_SCRATCH_HEAD;
    abr                                        = 1;
    head                                       = *scratch;
    coord                                      = (GsCOORDINATE2*)extra->field_8;
    size                                       = mem->field_18;
    frame                                      = mem->field_20;
    angle                                      = mem->field_1C;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (GpEffBeamScratch*)tmp;
    }
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (arg1 == 1) {
            if (arg2 != NULL) {
                prim->r0 = arg2[0];
                green    = arg2[1];
                COMPILER_BARRIER();
                abr = 2;
                goto rgb;
            }
            prim->r0 = 0x20;
            prim->g0 = 0x20;
            prim->b0 = 0x20;
        } else if (arg2 != NULL) {
            prim->r0 = arg2[0];
            green    = arg2[1];
        rgb:
            prim->g0 = green;
            COMPILER_BARRIER();
            blue = arg2[2];
            setSemiTrans(prim, 1);
            prim->b0 = blue;
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = (abr << 5) | 8;
        prim->clut  = 0x428E;
        uv          = (frame & 7) * 0x18;
        uv2         = uv + 0x17;
        setUV4(prim, uv, 0xB8, uv2, 0xB8, uv, 0xCF, uv2, 0xCF);
        block->dx = ((((s16)size * 23) / block->otz) * rsin((s16)angle)) >> 12;
        block->dy = ((((s16)size * 23) / block->otz) * rcos((s16)angle)) >> 12;
        prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang       = (s16)angle + 0x400;
        block->dx = ((((s16)size * 23) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)size * 23) / block->otz) * rcos(ang)) >> 12;
        prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void Gp_DrawEffQuadT29(GsCOORDINATE2* arg0, s32 arg1, u16 arg2, u16 arg3)
{
    GsCOORDINATE2*    coord;
    register void**   scratch asm("a0");
    GpQuadScratch*    block;
    register SVECTOR* v asm("t0");
    register s32      i asm("t3");
    GpQuadCorner*     tbl;
    register u8*      head asm("v0");
    MATRIX*           wm;
    POLY_FT4*         prim;
    register s32      u0 asm("a2");
    register s32      u1 asm("v1");
    s32               texV;
    s32               vTop;
    u16*              clutTbl;

    coord    = arg0;
    scratch  = (void**)G_SCRATCH_HEAD;
    i        = 0;
    wm       = &coord->workm;
    tbl      = D_80111E38;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
    do {
        v->vx = tbl->x * arg1;
        v->vy = 0;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&coord->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&coord->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            vTop = 0xB8;
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            COMPILER_BARRIER();
            setlen(prim, 9);
            setcode(prim, 0x2F);
            clutTbl     = D_80112964;
            prim->tpage = 0x29;
            prim->clut  = ((((GpEffClutOff*)&clutTbl[arg3])->field_8 >> 4) & 0x3F) | 0x4280;
            u0          = (arg2 & 7) * 0x10 - 0x80;
            u1          = (arg2 & 7) * 0x10 - 0x71;
            texV        = vTop;
            prim->v0    = texV;
            prim->v1    = texV;
            prim->u0    = u0;
            prim->u1    = u1;
            prim->u2    = u0;
            texV        = 0xC7;
            prim->v2    = texV;
            prim->u3    = u1;
            prim->v3    = texV;
            prim->x0    = block->sxy0.vx;
            prim->y0    = block->sxy0.vy;
            prim->x1    = block->sxy1.vx;
            prim->y1    = block->sxy1.vy;
            prim->x2    = block->sxy2.vx;
            prim->y2    = block->sxy2.vy;
            prim->x3    = block->sxy3.vx;
            prim->y3    = block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

void Gp_EffTask07State1(Task* arg0)
{
    Task* slot;
    s32   kind;
    s32   spawnId;
    s32   idx;

    slot = Game_GetPtrSlot(3);
    if (slot == NULL) {
        return;
    }
    if (Gp_State1C->field_E >= 4) {
        return;
    }
    kind = Gp_StateC08.field_3;
    if (kind == 2) {
        return;
    }
    if ((Gp_State1C->field_16 != 1) && ((Gp_StateC08.field_0 / 10U) != 0x20)) {
        return;
    }
    if (kind == -1) {
        spawnId = D_80112A50[((u16)(Gp_StateC08.field_0 / 100U) - 1) * 9 +
                             ((u16)((u16)(Gp_StateC08.field_0 / 10U) % 10U) - 1) * 3 + kind +
                             (u16)(Gp_StateC08.field_0 % 10U)];
        if (spawnId == 0) {
            return;
        }
        Gp_SpawnEff(spawnId, (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8,
                    Gp_StateC08.field_2, 0);
    } else if (kind == 1) {
        idx = ((u16)(Gp_StateC08.field_0 / 100U) - 1) * 9 +
              ((u16)((u16)(Gp_StateC08.field_0 / 10U) % 10U) - 1) * 3 - 1;
        idx    += (u16)(Gp_StateC08.field_0 % 10U);
        spawnId = D_80112978[idx];
        if (spawnId == 0) {
            return;
        }
        Gp_SpawnEff(spawnId,
                    (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8 + D_80112B28[idx], 0,
                    0);
    }
}

void func_800FAA14(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            pan;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (arg0->state == 0) {
        arg0->spawnArg1 = D_80112B94[((u16)(Gp_StateC08.field_0 / 100U) - 1) * 9 +
                                     ((u16)((u16)(Gp_StateC08.field_0 / 10U) % 10U) - 1) * 3 +
                                     ((u16)(Gp_StateC08.field_0 % 10U) - 1U)];
    }
    Gp_UpdateCoord(coord);
    if (Gp_State1C->field_E >= 4) {
        goto kill;
    }
    if (Gp_StateC08.field_2 == 0) {
        goto kill;
    }
    if (Gp_StateC08.field_3 == 2) {
        goto kill;
    }
    if (Gp_State1C->field_16 != 1 && (u16)(Gp_StateC08.field_0 / 10U) != 0x20) {
    kill:
        if (arg0->spawnArg1 != 0) {
            SndEvt_EnqueueType7(arg0->spawnArg1, 1);
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (Gp_StateC08.field_2 >= 9) {
        if (mem->field_24 < 0x20) {
            if (mem->field_24 == 0) {
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(arg0->spawnArg1, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            Gp_SpawnEff(0x60032, coord, 0, 0);
            mem->field_24++;
        }
    }
}

void Gp_EffCtlTask32(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    s16            angle;
    s16            temp;
    s32            state;
    s32            lcg;
    u32            lcg2;
    u16            step;
    s32            one;
    s32            newState;

    mem           = arg0->spawnArg2;
    coord         = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    step          = mem->field_22 + 1;
    mem->field_22 = step;
    state         = arg0->state;
    switch (state) {
        case 0:
            mem->field_26 = 0x180;
            mem->field_28 = 0x80;
            mem->field_2A = 0x400;
            lcg           = Gp_LcgState * 5 + 0x71357911;
            lcg2          = lcg * 5 + 0x71357911;
            temp          = ((lcg2 >> 0x10) & 0x3FF) - 0x200;
            Gp_LcgState   = lcg;
            mem->field_24 = ((u32)lcg >> 0x10) & 0xFFF;
            Gp_LcgState   = (s32)lcg2;
            mem->field_14 = temp;
            mem->field_10 = (rcos(temp) * mem->field_2A) >> 0xC;
            mem->field_12 = ((rsin(mem->field_14) * mem->field_2A) >> 0xC) - 0x400;
            parent =
                (GsCOORDINATE2*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            one                  = ONE;
            *(s32*)&coord->coord = one;
            coord->sub           = parent;
            m                    = &coord->coord;
            *(s32*)&m->m[0][2]   = 0;
            *(s32*)&m->m[1][1]   = one;
            *(s32*)&m->m[2][0]   = 0;
            m->m[2][2]           = one;
            angle                = ((s16)mem->field_22 + mem->field_24) * 0x18;
            coord->coord.t[0]    = (rcos(angle) * mem->field_10) >> 0xC;
            coord->coord.t[1] =
                mem->field_12 +
                ((rsin((Display_State.field_8 + mem->field_24) << 6) * 0x60) >> 0xC);
            coord->coord.t[2] = (rsin(angle) * mem->field_10) >> 0xC;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            if (Gp_State1C->field_E >= 4) {
                goto set_state_4;
            }
            if (Gp_StateC08.field_3 == 2) {
                newState = 4;
                goto set_state;
            }
            break;
        case 1:
            angle             = ((s16)step + mem->field_24) * 0x18;
            coord->coord.t[0] = (rcos(angle) * mem->field_10) >> 0xC;
            coord->coord.t[1] =
                mem->field_12 +
                ((rsin((Display_State.field_8 + mem->field_24) << 6) * 0x60) >> 0xC);
            coord->coord.t[2] = (rsin(angle) * mem->field_10) >> 0xC;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            if ((Gp_State1C->field_E >= 4) || (Gp_StateC08.field_3 == 2)) {
                newState = 3;
                goto set_state;
            }
            if (Gp_StateC08.field_2 < 8) {
                arg0->state = 2;
            }
            break;
        case 2:
            mem->field_2A     = (u16)mem->field_2A - 0x80;
            angle             = ((s16)step + mem->field_24) * 0x18;
            mem->field_10     = (rcos(mem->field_14) * mem->field_2A) >> 0xC;
            mem->field_12     = ((rsin(mem->field_14) * mem->field_2A) >> 0xC) - 0x400;
            coord->coord.t[0] = (rcos(angle) * mem->field_10) >> 0xC;
            coord->coord.t[1] =
                mem->field_12 +
                ((rsin((Display_State.field_8 + mem->field_24) << 6) * 0x60) >> 0xC);
            coord->coord.t[2] = (rsin(angle) * mem->field_10) >> 0xC;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            if (Gp_State1C->field_E >= 4) {
                newState = 4;
                goto set_state;
            }
            if (mem->field_2A < 0x80) {
                newState = 4;
                goto set_state;
            }
            if (Gp_StateC08.field_3 == state) {
                newState = 4;
                goto set_state;
            }
            break;
        case 3:
            coord->flg         = 0;
            coord->coord.t[1] += 0x40;
            Gp_UpdateCoord(coord);
            if (mem->field_28 < 0xB) {
                newState = 4;
                goto set_state;
            }
            mem->field_28 = (u16)mem->field_28 - 0xA;
            break;
    }
    goto draw;
set_state_4:
    newState = 4;
set_state:
    arg0->state = newState;
draw:
    func_800EB6E8(coord, (u16)mem->field_22, (u16)mem->field_26 | 0x1000,
                  (u16)mem->field_28 | 0x1000);
    if (arg0->state == 4) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffCtlTaskAE(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    s32            state;
    s32            one;
    s32            pan;
    s16            temp;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    state = arg0->state;
    switch (state) {
        case 0:
            parent =
                (GsCOORDINATE2*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            one                  = ONE;
            *(s32*)&coord->coord = one;
            coord->sub           = parent + 12;
            m                    = &coord->coord;
            *(s32*)&m->m[0][2]   = 0;
            *(s32*)&m->m[1][1]   = one;
            *(s32*)&m->m[2][0]   = 0;
            m->m[2][2]           = one;
            coord->coord.t[0]    = 0;
            coord->coord.t[1]    = 0;
            coord->coord.t[2]    = 0;
            coord->flg           = 0;
            Gp_UpdateCoord(coord);
            arg0->state   = 1;
            mem->field_24 = 0;
            mem->field_26 = 0x40;
            mem->field_2A = 0x100 / arg0->spawnArg1;
            if (Gp_State1C->field_E >= 4) {
                goto kill;
            }
            if (Gp_StateC08.field_3 == 2) {
                goto kill;
            }
            if (Gp_State1C->field_16 != 1) {
                goto kill;
            }
            arg0->spawnArg1 = D_80112B94[((u16)(Gp_StateC08.field_0 / 100U) - 1) * 9 +
                                         ((u16)((u16)(Gp_StateC08.field_0 / 10U) % 10U) - 1) * 3 +
                                         ((u16)(Gp_StateC08.field_0 % 10U) - 1U)];
            pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(arg0->spawnArg1, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            return;
        case 1:
            Gp_UpdateCoord(coord);
            temp          = (u16)mem->field_24 + (u16)mem->field_2A;
            mem->field_24 = temp;
            if (temp >= 0x100) {
                mem->field_24 = 0xFF;
            }
            temp          = (u16)mem->field_26 + 8;
            mem->field_26 = temp;
            if (temp >= 0x201) {
                mem->field_26 = 0x200;
            }
            rgb[0] = mem->field_24;
            rgb[1] = (u16)mem->field_24 >> 1;
            rgb[2] = (u16)mem->field_24 >> 2;
            Gp_DrawRing(coord, mem->field_26, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->field_26 << 1), rgb);
            if (mem->field_24 >= 0x81) {
                temp          = ((u16)mem->field_2A << 1) + (u16)mem->field_28;
                mem->field_28 = temp;
                if (temp >= 0x100) {
                    mem->field_28 = 0xFF;
                }
                rgb[0] = mem->field_28;
                rgb[1] = (u16)mem->field_28 >> 1;
                rgb[2] = (u16)mem->field_28 >> 2;
                Gp_DrawArc(coord, ((u8)Gp_StateC08.field_2 << 24) >> 17, 0x60, rgb);
            }
            if (Gp_State1C->field_E >= 4) {
                goto snd7;
            }
            if (Gp_StateC08.field_3 == 2) {
                goto snd7;
            }
            if (Gp_State1C->field_16 != 1) {
                goto snd7;
            }
            if (Gp_StateC08.field_2 != 0) {
                return;
            }
            mem->field_24 = 0xFF;
            arg0->state   = 2;
            return;
        case 2:
            Gp_UpdateCoord(coord);
            mem->field_22++;
            if (mem->field_26 <= 0) {
                goto kill;
            }
            rgb[0] = mem->field_24;
            rgb[1] = (u16)mem->field_24 >> 1;
            rgb[2] = (u16)mem->field_24 >> 2;
            Gp_DrawRing(coord, mem->field_26, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->field_26 << 1), rgb);
            if (Gp_State1C->field_E >= 4) {
                goto snd7;
            }
            if (Gp_StateC08.field_3 == state) {
                goto snd7;
            }
            if (Gp_State1C->field_16 == 1) {
                goto decay;
            }
        snd7:
            SndEvt_EnqueueType7(arg0->spawnArg1, 1);
            arg0->state = 3;
            return;
        decay:
            mem->field_24 = (u16)mem->field_24 - 0x10;
            mem->field_26 = (u16)mem->field_26 - 0x30;
            return;
        case 3:
            Gp_UpdateCoord(coord);
            if (mem->field_24 < 0x11) {
                goto kill;
            }
            rgb[0] = mem->field_24;
            rgb[1] = (u16)mem->field_24 >> 1;
            rgb[2] = (u16)mem->field_24 >> 2;
            Gp_DrawRing(coord, mem->field_26, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->field_26 << 1), rgb);
            mem->field_24 = (u16)mem->field_24 - 0x10;
            return;
    }
    return;
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffCtlTaskC1(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            idx;
    u8             rgb[3];
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        idx           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
        mem->field_28 = D_80112C6C[idx & 3];
        arg0->state   = 1;
    }

    Gp_UpdateCoord(coord);
    rgb[0] = (mem->field_24 * (((u16)mem->field_28 >> 8) & 0xF)) >> 3;
    rgb[1] = (mem->field_24 * ((u8)mem->field_28 >> 4)) >> 3;
    rgb[2] = (mem->field_24 * ((u16)mem->field_28 & 0xF)) >> 3;
    Gp_DrawBandEx(coord, mem->field_26, 0x100, rgb);

    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffCtlTaskF3(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    Task*          slot;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_E != 0 ||
        (((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C & 0x80)) {
        if (Gp_State1C->field_E < 4) {
            return;
        }
        goto kill;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        s32 x;
        s32 y;

        Gp_State1C->field_12 |= 0x400;
        slot                  = Game_GetPtrSlot(3);
        parent                = (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8;
        coord->coord.t[0]     = 0;
        coord->coord.t[1]     = 0;
        coord->coord.t[2]     = 0;
        coord->flg            = 0;
        coord->sub            = parent + 8;
        arg0->state           = 1;
        mem->field_20         = (Gp_StateC08.field_0 % 10U) - 1;
        __asm__ volatile("" : "+m"(mem->field_20));
        x             = mem->field_20;
        mem->field_26 = 0x20;
        y             = mem->field_20;
        mem->field_28 = (x << 7) + 0x180;
        mem->field_2A = (y << 8) + 0x400;
    }

    Gp_UpdateCoord(coord);
    if (Gp_State1C->field_14 != 0) {
        rgb[2] = 0xC0;
        rgb[0] = 0xC0;
        rgb[1] = 0x60;
        Gp_DrawEffTri(coord, (s16)((u16)mem->field_28 + 0x80), (s16)((u16)mem->field_20 + 6), rgb);
        Gp_DrawRing(coord, mem->field_28, rgb);
        Gp_DrawRing(coord, (s16)((u16)mem->field_28 << 1), rgb);
        Gp_State1C->field_14 = 0;
    }

    if (Gp_StateC08.field_12 == 0) {
        goto kill;
    }
    if (!(Gp_State1C->field_12 & 0x400)) {
        goto kill;
    }
    if (Gp_State1C->field_16 == 1) {
        goto lcg;
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
    return;
lcg:
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16) & 3) {
        return;
    }
    slot        = Game_GetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gp_SpawnEff(0x600F4,
                (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8 +
                    ((((u32)Gp_LcgState >> 16) & 1) * 3 + 15),
                mem->field_2A | 0x8000, 0);
}

void Gp_DrawEffTri(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3)
{
    register u8*     rgb asm("s7");
    void**           scratch;
    register u8*     head asm("t1");
    GpEffFt4Scratch* block;
    POLY_G3*         prim;
    s16              step;
    s32              i;
    register s32     lcg asm("a0");
    s32              ang;
    s16              scale;
    s16              count;
    u16              vz;

    rgb     = arg3;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                        = *(u16*)&arg0->workm.t[0];
        ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpEffFt4Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    count         = arg2;
    step          = 0x1000 / count;
    scale         = arg1;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        for (i = 0; i < step * count; i += step) {
            lcg            = Gp_LcgState * 5 + 0x71357911;
            prim           = (POLY_G3*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            Gp_LcgState    = lcg;
            setPolyG3(prim);
            setRGB0(prim, rgb[0], rgb[1], rgb[2]);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, 0, 0);
            block->size = (scale * 128) / block->otz;
            ang         = (s16)(i + (s32)((u32)lcg >> 16) % step);
            prim->x0    = *(u16*)&block->sx;
            prim->y0    = *(u16*)&block->sy;
            prim->x1    = *(u16*)&block->sx + ((block->size * rsin(ang - 0x28)) >> 12);
            prim->y1    = *(u16*)&block->sy + ((block->size * rcos(ang - 0x28)) >> 12);
            prim->x2    = *(u16*)&block->sx + ((block->size * rsin(ang + 0x28)) >> 12);
            prim->y2    = *(u16*)&block->sy + ((block->size * rcos(ang + 0x28)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void Gp_EffCtlTaskF4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    Task*          slot;
    s16            flag;
    s32            y;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            goto kill;
        }
        slot = Game_GetPtrSlot(3);
        if (((TmdObject*)slot->extra)->field_C & 0x80) {
            return;
        }
        Gp_UpdateCoord(coord);
        goto draw_lcg;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_10 = 0;
        mem->field_14 = 0;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_12 = 0xFFF0 - (((u32)Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
        mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        arg0->state   = 1;
        mem->field_28 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xF000;
    }

    y                 = coord->coord.t[1] + mem->field_12;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    Gp_UpdateCoord(coord);
    if ((mem->field_22 & 3) == 0) {
        mem->field_20++;
    }
    if (mem->field_20 >= 8) {
        goto kill;
    }
    if (mem->field_28 & 0x8000) {
    draw_lcg:
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, (u16)mem->field_20, mem->field_26,
                      (u16)mem->field_24 | (((u32)Gp_LcgState >> 16) & 0x1000));
    } else {
        Gp_DrawFxQuad(coord, (u16)mem->field_20, mem->field_26,
                      (u16)mem->field_24 | (u16)mem->field_28);
    }
    return;
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffCtlTaskAC(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    GpEffWork*     spawned;
    Task*          slot;
    u8             rgb[3];
    u8             col;
    s32            saved;
    s32            temp;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_E != 0 ||
        (((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C & 0x80)) {
        if (Gp_State1C->field_E < 4) {
            return;
        }
        goto kill;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        s32 x;

        Gp_State1C->field_12 |= 0x200;
        slot                  = Game_GetPtrSlot(3);
        parent                = (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8;
        coord->coord.t[0]     = 0;
        coord->coord.t[1]     = 0;
        coord->coord.t[2]     = 0;
        coord->flg            = 0;
        coord->sub            = parent + 1;
        arg0->state           = 1;
        mem->field_20         = (Gp_StateC08.field_0 % 10U) - 1;
        __asm__ volatile("" : "+m"(mem->field_20));
        x             = mem->field_20;
        mem->field_26 = 0x20;
        mem->field_28 = ((x + 1) * 3) << 7;
        mem->field_2A = Wip_SysConfig.field_18;
    }

    Gp_UpdateCoord(coord);
    mem->field_24 = (u16)mem->field_26 + (((u16)mem->field_22 & 1) << 4);
    col           = mem->field_24;
    rgb[1]        = col;
    rgb[0]        = col;
    rgb[2]        = (u16)mem->field_24 >> 1;
    Gp_DrawRing(coord, mem->field_28, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->field_28 << 1), rgb);

    if (Gp_StateC08.field_10 == 0) {
        goto kill;
    }
    if (!(Gp_State1C->field_12 & 0x200)) {
        goto kill;
    }
    if (Gp_State1C->field_16 == 1) {
        goto continue_fx;
    }
kill:
    SndEvt_EnqueueType7(0x23, 1);
    Gp_ReleaseState1CMem(mem, arg0);
    return;
continue_fx:
    saved = mem->field_2A;
    if (Wip_SysConfig.field_18 < saved) {
        if (!(Wip_SysConfig.field_25 & 0x84) && (mem->field_26 < 0xA0)) {
            s32 i;

            Gp_DrawEffTri(coord, 0x200, 6, rgb);
            mem->field_26 = 0xC0;
            for (i = 0; i < 0x555; i += 0x2AA) {
                spawned = Gp_SpawnEff(0x600C1, coord, i, 0);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
            temp = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0xE, temp, (s8)Gp_GetObjDepth((GpObj38*)coord));
        } else if (mem->field_26 < 0x80) {
            mem->field_26 = 0x80;
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            slot        = Game_GetPtrSlot(3);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0,
                        (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8 +
                            ((((u32)Gp_LcgState >> 16) & 0xF) + 3),
                        0x10080, 0);
        }
    }

    mem->field_2A = (u16)Wip_SysConfig.field_18;
    if (mem->field_26 < 0x21) {
        return;
    }
    mem->field_26 = (u16)mem->field_26 - 8;
    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16) & 1) {
        return;
    }
    slot        = Game_GetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gp_SpawnEff(0x600E0,
                (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8 +
                    ((((u32)Gp_LcgState >> 16) & 0xF) + 3),
                0x10200, 0);
}

void Gp_EffCtlTask0E(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    Task*          slot;
    s16            flag;
    s32            one;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        Gp_State1C->field_12 |= 0x800;
        slot                  = Game_GetPtrSlot(3);
        parent                = (GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8;
        one                   = ONE;
        *(s32*)&coord->coord  = one;
        coord->sub            = parent + 8;
        m                     = &coord->coord;
        *(s32*)&m->m[0][2]    = 0;
        *(s32*)&m->m[1][1]    = one;
        *(s32*)&m->m[2][0]    = 0;
        m->m[2][2]            = one;
        coord->coord.t[0]     = 0;
        coord->coord.t[1]     = 0;
        coord->coord.t[2]     = 0;
        coord->flg            = 0;
        arg0->state           = 1;
    }

    Gp_UpdateCoord(coord);
    if (Gp_State1C->field_14 != 0) {
        rgb[0] = 0xC0;
        rgb[1] = 0x30;
        rgb[2] = 0x60;
        Gp_DrawEffTri(coord, 0x200, 4, rgb);
        Gp_DrawRing(coord, 0x180, rgb);
        Gp_DrawRing(coord, 0x300, rgb);
        Gp_State1C->field_14 = 0;
    }

    if ((Wip_SysConfig.field_25 & 0x80) && (Gp_State1C->field_12 & 0x800) &&
        (Gp_State1C->field_16 == 1)) {
        return;
    }
    Gp_State1C->field_10 &= ~0x80;
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_PulseState1C80(void)
{
    Gp_State1C->field_1A |= 0x80;
}

void Gp_EffTask07State0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void Gp_EffCtlTask07(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_EffTask07States;
    sp.funcs[arg0->state](arg0);
}

void Gp_EffCtlTaskA5(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            i;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            SndEvt_EnqueueType7(0xFF0D, 1);
            Gp_State1C->field_2 = 0;
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    Gp_UpdateCoord(coord);
    switch (arg0->state) {
        case 0:
            if (Gp_State1C->field_2 == 0) {
                temp = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(0xD, temp, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            Gp_State1C->field_2++;
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (arg0->spawnArg1 == 0) {
                Gp_SpawnEff(0x600A6, coord, 1, 0);
                arg0->state = 2;
            } else if (mem->field_24 == 0) {
                for (i = 0; i < 3; i++) {
                    Gp_SpawnEff(0x600A6, coord, arg0->spawnArg1, 0);
                }
                mem->field_24++;
            } else {
                mem->field_26++;
                if (mem->field_26 >= 9) {
                    mem->field_24 = 0;
                    mem->field_26 = 0;
                    mem->field_20++;
                    if (mem->field_20 >= arg0->spawnArg1) {
                        arg0->state = 2;
                    }
                }
            }
            break;
        case 2:
            mem->field_22++;
            if (mem->field_22 >= 0x65) {
                Gp_State1C->field_2--;
                if (Gp_State1C->field_2 <= 0) {
                    SndEvt_EnqueueType7(0xFF0D, 1);
                    Gp_State1C->field_2 = 0;
                }
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

void Gp_EffCtlTaskA6(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    SVECTOR*       in;
    SVECTOR*       out;
    s16            flag;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag < 4) {
        switch (arg0->state) {
            case 0:
                mem->field_22++;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = (u32)Gp_LcgState >> 16;
                mem->field_26 = (mem->field_24 & 0xF) + 8;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                temp          = arg0->spawnArg1;
                mem->field_28 = -(temp << 4) - (((u32)Gp_LcgState >> 16) & 0x7F);
                mem->field_2A = arg0->spawnArg1 * 24 + 0xC0;
                Gfx_RotMatrixY(&coord->coord, (u16)mem->field_24 & 0xFF0, 1);
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                arg0->state = 1;
                mem->field_14 =
                    ((u16)mem->field_24 & 0x1F) % (arg0->spawnArg1 * 3) + 7;
                func_800FCD00(arg0);
                Gp_SpawnEff(0x600A7, coord, mem->field_2A * 3 + 0x3000, 0);
                return;
            case 1:
                if (Gp_StateF0.field_4 != 1) {
                    Gp_UpdateCoord(coord);
                    mem->field_22++;
                    if (mem->field_14 != 0) {
                        in             = (SVECTOR*)&mem->field_10;
                        out            = (SVECTOR*)&mem->field_18;
                        mem->field_14 -= (mem->field_22 & 3) / 3;
                        gte_SetRotMatrix(&coord->coord);
                        gte_ldv0(in);
                        gte_rtv0_real();
                        gte_stsv(out);
                        coord->coord.t[0] += mem->field_18;
                        coord->coord.t[1] += mem->field_1A;
                        coord->coord.t[2] += mem->field_1C;
                        coord->flg         = 0;
                    }
                    if (mem->field_22 >= 0x81) {
                        arg0->state = 2;
                    }
                }
                goto do_fcd00;
            case 2:
                if (Gp_StateF0.field_4 != 1) {
                    Gp_UpdateCoord(coord);
                    mem->field_22++;
                    mem->field_26 -= mem->field_22 & 1;
                    mem->field_28 += 2;
                    mem->field_2A += 2;
                    if (mem->field_26 <= 0) {
                        goto kill;
                    }
                    if (mem->field_28 < 0) {
                        goto do_fcd00;
                    }
                    goto kill;
                }
                goto do_fcd00;
        }
        return;
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
    return;
do_fcd00:
    func_800FCD00(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FCD00);

void Gp_EffSprTaskA7(Task* arg0)
{
    GpEffWork*         mem;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     parent;
    MATRIX*            m;
    void**             scratch;
    u8*                head;
    GpEffFlareScratch* block;
    GpEffFlareScratch* otzp;
    POLY_FT4*          prim;
    s16                flag;
    s32                rng;
    s32                one;
    s32                n;
    s16                step;
    u16                vz;
    s32                prod;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (arg0->state == 0) {
        rng                  = Gp_LcgState * 5 + 0x71357911;
        mem->field_24        = ((u32)rng >> 16) & 0xFFF;
        mem->field_26        = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        parent               = mem->field_8;
        mem->field_12        = -(mem->field_24 & 7);
        one                  = ONE;
        *(s32*)&coord->coord = one;
        coord->sub           = parent;
        m                    = &coord->coord;
        *(s32*)&m->m[0][2]   = 0;
        *(s32*)&m->m[1][1]   = one;
        *(s32*)&m->m[2][0]   = 0;
        m->m[2][2]           = one;
        coord->coord.t[2]    = 0;
        coord->coord.t[1]    = 0;
        coord->coord.t[0]    = 0;
        coord->flg           = 0;
        Gp_LcgState          = rng;
        arg0->state++;
    }
    Gp_UpdateCoord(coord);
    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (GpEffFlareScratch*)(head - 0x1C);
    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    otzp          = block;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((GpEffFlareScratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    n = (((s32)arg0->spawnArg1 >> 12) & 3) + 1;
    gte_stsxy(&((GpEffFlareScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffFlareScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&otzp->otz);
        prim                                     = (POLY_FT4*)Gpu_PrimCursor;
        ((GpEffFlareScratch*)(head - 0x1C))->otz = ((GpEffFlareScratch*)(head - 0x1C))->otz + 1;
        Gpu_PrimCursor                           = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        setRGB0(prim, 0x60, 0x60, 0x60);
        prim->tpage = 0x28;
        prim->clut  = 0x4253;
        prim->code |= 2;
        {
            /* the divisor must stay a live copy of `n` in $v1 */
            register s32 count asm("v1");
            count    = n;
            prim->u0 = (mem->field_22 / count) << 5;
            prim->v0 = 0x18;
            prim->u1 = ((mem->field_22 / count) << 5) + 0x1F;
            prim->v1 = 0x18;
            prim->u2 = (mem->field_22 / count) << 5;
            prim->v2 = 0x37;
            prim->u3 = ((mem->field_22 / count) << 5) + 0x1F;
            prim->v3 = 0x37;
        }
        {
            s32 prod;
            prod      = ((mem->field_26 * 31) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(mem->field_24);
            block->dx = prod >> 12;
            USE_REG(prod); /* keep prod live so the shift lands in $v0 */
        }
        block->dy = (((mem->field_26 * 31) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(mem->field_24)) >> 12;
        prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        prod      = ((mem->field_26 * 31) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(mem->field_24 + 0x400);
        block->dx = prod >> 12;
        USE_REG(prod);
        block->dy = (((mem->field_26 * 31) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(mem->field_24 + 0x400)) >> 12;
        prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)((GpEffFlareScratch*)(head - 0x1C))->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
    if (Gp_State1C->field_4 != 0) {
        return;
    }
    step               = mem->field_12 - (mem->field_22 & 1);
    mem->field_12      = step;
    coord->flg         = 0;
    coord->coord.t[1] += step;
    mem->field_22++;
    if (mem->field_22 > n * 8 - 1) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FDB18);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl Gp_PlayerWorkStates\n"
        "Gp_PlayerWorkStates:\n"
        "\t.word Gp_InitPlayerWork\n"
        "\t.word Gp_PlayerWorkState1\n"
        "\t.word Gp_PlayerWorkState2\n"
        "\t.word Gp_TeardownSlot0\n"
        ".section .text\n");
#endif

void Gp_EffCtlTask7F(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    s16            flag;
    s16            step;
    s32            temp;
    s32            one;
    s32            span;
    s16            divisor;
    s16            half;
    s32            count;
    s32            i;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
        goto release;
    }
    if (arg0->state == 0) {
        parent               = mem->field_8;
        one                  = ONE;
        *(s32*)&coord->coord = one;
        coord->sub           = parent;
        m                    = &coord->coord;
        *(s32*)&m->m[0][2]   = 0;
        *(s32*)&m->m[1][1]   = one;
        *(s32*)&m->m[2][0]   = 0;
        m->m[2][2]           = one;
        coord->coord.t[0]    = mem->field_18;
        coord->coord.t[1]    = mem->field_1A;
        coord->coord.t[2]    = mem->field_1C;
        coord->flg           = 0;
        arg0->state          = 1;
        mem->field_24        = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
        temp                 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
        step                 = temp;
        mem->field_20        = temp;
        if (step != 1) {
            step = step * 3;
        } else {
            step = 1;
        }
        mem->field_26 = step;
        mem->field_28 = step * 2;
        mem->field_2A = mem->field_24 / 1280;
    }
    Gp_UpdateCoord(coord);
    if (Gp_State1C->field_4 != 0) {
        return;
    }
    if (mem->field_22 >= mem->field_28) {
    release:
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    span        = mem->field_24 >> 1;
    half        = (u32)span >> 1;
    divisor     = span;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16) & 1) {
        count = mem->field_2A;
    } else {
        count = 1;
    }
    for (i = 0; i < count; i++) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_10 = ((s32)((u32)Gp_LcgState >> 16) % divisor) - half;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_12 = (s32)((u32)Gp_LcgState >> 16) % divisor;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_14 = ((s32)((u32)Gp_LcgState >> 16) % divisor) - half;
        if (mem->field_22 < mem->field_26) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (mem->field_22 < (s32)((u32)Gp_LcgState >> 16) % mem->field_28) {
                Gp_SpawnEff(0x60080, coord, (mem->field_10 & 0x10000) | 0x300,
                            (SVECTOR*)&mem->field_10);
            } else {
                Gp_SpawnEff(0x6008D, coord, 0x300, (SVECTOR*)&mem->field_10);
            }
        } else {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                Gp_SpawnEff(0x60070, coord, (mem->field_24 >> 2) + 0xC0013200,
                            (SVECTOR*)&mem->field_10);
            }
        }
    }
    mem->field_22++;
}

void Gp_EffCtlTaskE3(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
    } else {
        if (arg0->state == 0) {
            coord->sub        = mem->field_8;
            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;
            coord->flg        = 0;
            arg0->state       = 1;
            mem->field_24     = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
            temp              = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_26     = temp;
            mem->field_28     = temp << 2;
        }
        Gp_UpdateCoord(coord);
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 >= mem->field_28) {
            Gp_ReleaseState1CMem(mem, arg0);
        } else {
            Gp_SpawnEff(0x60070, coord, (mem->field_24 >> 2) + 0x80021400, (SVECTOR*)&mem->field_10);
        }
    }
}

void Gp_EffSprTask80(Task* arg0)
{
    GpEffFt4Scratch* block;
    GsCOORDINATE2*   coord;
    GpEffWork*       mem;
    POLY_FT4*        prim;
    s16              x;
    s16              y;
    s32              amt;
    s32              t;
    u16              uv;
    s32              scale;
    s32              c;
    u32              rnd;
    s32              flag2;
    u8*              head;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 < 4) {
        head                    = (u8*)*(void**)G_SCRATCH_HEAD - 0x18;
        *(void**)G_SCRATCH_HEAD = head;
        block                   = (GpEffFt4Scratch*)head;
        if (arg0->state == 0) {
            t   = (u16)arg0->spawnArg1 & 0xFFF;
            amt = 0x200;
            if (t != 0) {
                amt = t;
            }
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = amt;
            mem->field_26 = ((u32)Gp_LcgState >> 16) % 12 + 12;
            flag2         = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            if (flag2 & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                rnd         = (u32)Gp_LcgState >> 16;
                rnd         = rnd % 40;
            } else {
                rnd = 0;
            }
            mem->field_2A = rnd;
            arg0->state++;
            arg0->spawnArg1 &= 0x80000000;
        }
        Gp_UpdateCoord(coord);
        block->vec.vx = *(u16*)&coord->workm.t[0];
        block->vec.vy = *(u16*)&coord->workm.t[1];
        block->vec.vz = *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&block->sx);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (mem->field_22 < 0xC) {
                scale = mem->field_24 * mem->field_22 / 12;
            } else {
                scale = (u16)mem->field_24;
            }
            mem->field_28 = scale;
            if (mem->field_26 - 8 < mem->field_22) {
                c = (mem->field_26 - mem->field_22 + 1) * 0x10;
                setRGB0(prim, c, c, c);
            } else {
                prim->code |= 1;
            }
            prim->tpage = 0x29;
            prim->clut  = 0x4282;
            prim->code |= 2;
            uv          = mem->field_22;
            prim->v0    = 0x98;
            prim->u0    = (s16)((s16)uv % 6) * 0x20;
            uv          = mem->field_22;
            prim->v1    = 0x98;
            prim->u1    = ((s16)((s16)uv % 6) * 0x20) + 0x1F;
            uv          = mem->field_22;
            prim->v2    = 0xB7;
            prim->u2    = (s16)((s16)uv % 6) * 0x20;
            uv          = mem->field_22;
            prim->v3    = 0xB7;
            prim->u3    = ((s16)((s16)uv % 6) * 0x20) + 0x1F;
            block->size = (mem->field_28 * 0x1F) / block->otz;
            x           = *(u16*)&block->sx - *(u16*)&block->size;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sx + *(u16*)&block->size;
            prim->x3    = x;
            prim->x1    = x;
            y           = *(u16*)&block->sy - *(u16*)&block->size;
            prim->y1    = y;
            prim->y0    = y;
            y           = *(u16*)&block->sy + *(u16*)&block->size;
            prim->y3    = y;
            prim->y2    = y;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        if (mem->field_2A != 0) {
            coord->coord.t[1] -= mem->field_2A;
            coord->flg         = 0;
        }
        mem->field_22++;
        if (mem->field_26 >= mem->field_22) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTask8D(Task* arg0)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    GsCOORDINATE2*   coord;
    GpEffWork*       mem;
    POLY_FT4*        prim;
    s16              x;
    s16              y;
    s32              amt;
    s32              t;
    u16              uv;
    u16              vz;
    s32              scale;
    s32              c;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 < 4) {
        Gp_UpdateCoord(coord);
        scratch                                   = (void**)G_SCRATCH_HEAD;
        head                                      = *scratch;
        ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x18;
            block = (GpEffFt4Scratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
        gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
            USE_REG(head);
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                t   = (u16)arg0->spawnArg1 & 0xFFF;
                amt = 0x200;
                if (t != 0) {
                    amt = t;
                }
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = amt;
                mem->field_26 = (((u32)Gp_LcgState >> 16) & 7) + 0x10;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_2A = ((u32)Gp_LcgState >> 16) % 0x30;
                arg0->state++;
                arg0->spawnArg1 &= 0x80000000;
            }
            if (mem->field_22 < 0xC) {
                scale = mem->field_24 * mem->field_22 / 12;
            } else {
                scale = (u16)mem->field_24;
            }
            mem->field_28 = scale;
            if (mem->field_26 - 8 < mem->field_22) {
                c = (mem->field_26 - mem->field_22 + 1) * 0x10;
                setRGB0(prim, c, c, c);
            } else {
                prim->code |= 1;
            }
            prim->tpage = 0x28;
            prim->clut  = 0x430D;
            prim->code |= 2;
            uv          = mem->field_22;
            prim->v0    = 0xA0;
            prim->u0    = (uv & 7) * 0x18;
            uv          = mem->field_22;
            prim->v1    = 0xA0;
            prim->u1    = ((uv & 7) * 0x18) + 0x17;
            uv          = mem->field_22;
            prim->v2    = 0xB7;
            prim->u2    = (uv & 7) * 0x18;
            uv          = mem->field_22;
            prim->v3    = 0xB7;
            prim->u3    = ((uv & 7) * 0x18) + 0x17;
            block->size = (mem->field_28 * 0x17) / block->otz;
            x           = *(u16*)&block->sx - *(u16*)&block->size;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sx + *(u16*)&block->size;
            prim->x3    = x;
            prim->x1    = x;
            y           = *(u16*)&block->sy - *(u16*)&block->size;
            prim->y1    = y;
            prim->y0    = y;
            y           = *(u16*)&block->sy + *(u16*)&block->size;
            prim->y3    = y;
            prim->y2    = y;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        coord->coord.t[1] -= mem->field_2A;
        coord->flg         = 0;
        mem->field_22++;
        if (mem->field_26 >= mem->field_22) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTask3F(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    GpMtxWords*       rot;
    void**            scratch;
    register u8*      head asm("a0");
    GpEffBeamScratch* block;
    POLY_FT4*         prim;
    u16               vz;
    s32               sub;
    s32               temp;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 < 4) {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        {
            register u16 vx asm("v0");
            vx                                         = *(u16*)&coord->workm.t[0];
            ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
        }
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x1C;
            block = (GpEffBeamScratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            if (arg0->state == 0) {
                mem->field_28 = ((arg0->spawnArg1 >> 12) & 3) + 2;
                temp          = (u16)arg0->spawnArg1 & 0xFFF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                mem->field_24 = temp;
                sub           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_20 = sub & 1;
                if (mem->field_20 != 0) {
                    rot               = (GpMtxWords*)&coord->coord;
                    coord->sub        = mem->field_8;
                    rot->w0           = 0x1000;
                    rot->w1           = 0;
                    rot->w2           = 0x1000;
                    rot->w3           = 0;
                    rot->h4           = 0x1000;
                    coord->coord.t[2] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[0] = 0;
                    coord->flg        = 0;
                    Gp_UpdateCoord(coord);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_12 = -(((u32)Gp_LcgState >> 16) & 3);
                } else {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_12 = -(((u32)Gp_LcgState >> 16) & 0xF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_14 = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
                }
                arg0->state++;
            }
            gte_stszotz(&block->otz);
            block->otz     = block->otz + 1;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4253;
            setUV4(prim, (mem->field_22 / mem->field_28) << 5, 0x18,
                   ((mem->field_22 / mem->field_28) << 5) + 0x1F, 0x18,
                   (mem->field_22 / mem->field_28) << 5, 0x37,
                   ((mem->field_22 / mem->field_28) << 5) + 0x1F, 0x37);
            block->dx = (((mem->field_24 * 0x1F) / block->otz) * rsin(mem->field_26)) >> 12;
            block->dy = (((mem->field_24 * 0x1F) / block->otz) * rcos(mem->field_26)) >> 12;
            prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx = (((mem->field_24 * 0x1F) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            block->dy = (((mem->field_24 * 0x1F) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        if (mem->field_20 != 0) {
            coord->coord.t[1] += mem->field_12;
            coord->flg         = 0;
        } else {
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->coord.t[1] -= (s16)(mem->field_24 / 736);
            coord->flg         = 0;
        }
        mem->field_22++;
        if (mem->field_22 <= (mem->field_28 * 8) - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FF710);

void Gp_EffSprTaskE0(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               temp;
    s32               pal;
    s32               t;
    s32               uv;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        vecp                                       = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            block->otz -= 0x20;
            if (block->otz < 0x10) {
                block->otz = 0x10;
            }
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                temp          = (u16)arg0->spawnArg1 & 0xFFF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = temp + (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                pal           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_2A = pal;
                arg0->state   = 1;
            }
            prim->tpage = 0x29;
            prim->code |= 3;
            t           = (((u16)mem->field_2A + 0x10A) << 6) | ((mem->field_2A * 6) & 0x3F);
            prim->clut  = t;
            uv          = mem->field_22;
            t           = 0x50;
            prim->v0    = t;
            prim->u0    = uv * 0x28;
            uv          = mem->field_22;
            prim->v1    = t;
            prim->u1    = uv * 0x28 + 0x27;
            uv          = mem->field_22;
            t           = 0x77;
            prim->v2    = t;
            prim->u2    = uv * 0x28;
            uv          = mem->field_22;
            prim->v3    = t;
            prim->u3    = uv * 0x28 + 0x27;
            block->dx   = (((mem->field_24 * 0x27) / block->otz) * rsin(mem->field_26)) >> 12;
            block->dy   = (((mem->field_24 * 0x27) / block->otz) * rcos(mem->field_26)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_24 * 0x27) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            block->dy   = (((mem->field_24 * 0x27) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 6) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTaskE1(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               temp;
    s32               pal;
    s32               t;
    s32               uv;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        block    = (GpEffBeamScratch*)(head - 0x1C);
        *scratch = block;
        vecp     = block;
        if (arg0->state == 0) {
            temp          = (u16)arg0->spawnArg1 & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = temp + (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            pal           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_2A = pal;
            arg0->state++;
        }
        Gp_UpdateCoord(coord);
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        block->vec.vz                              = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            block->otz -= 0x20;
            if (block->otz < 0x10) {
                block->otz = 0x10;
            }
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = ((0x10C - (u16)mem->field_2A) << 6) | (((0xC0 - mem->field_2A * 80) >> 4) & 0x3F);
            uv          = mem->field_22;
            t           = 0x88;
            prim->v0    = t;
            prim->u0    = uv * 0x18;
            uv          = mem->field_22;
            prim->v1    = t;
            prim->u1    = uv * 0x18 + 0x17;
            uv          = mem->field_22;
            t           = 0x9F;
            prim->v2    = t;
            prim->u2    = uv * 0x18;
            uv          = mem->field_22;
            prim->v3    = t;
            prim->u3    = uv * 0x18 + 0x17;
            block->dx   = (((mem->field_24 * 23) / block->otz) * rsin(mem->field_26)) >> 12;
            block->dy   = (((mem->field_24 * 23) / block->otz) * rcos(mem->field_26)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->field_24 * 23) / block->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            block->dy   = (((mem->field_24 * 23) / block->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 8) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTaskE2(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    s16            flag;
    s32            one;
    s32            temp;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag < 2) {
        if (arg0->state == 0) {
            if (arg0->spawnArg1 < 0) {
                parent               = mem->field_8;
                one                  = ONE;
                *(s32*)&coord->coord = one;
                coord->sub           = parent;
                m                    = &coord->coord;
                *(s32*)&m->m[0][2]   = 0;
                *(s32*)&m->m[1][1]   = one;
                *(s32*)&m->m[2][0]   = 0;
                m->m[2][2]           = one;
                coord->coord.t[0]    = mem->field_18;
                coord->coord.t[1]    = mem->field_1A;
                coord->coord.t[2]    = mem->field_1C;
                coord->flg           = 0;
            }
            temp          = (u16)arg0->spawnArg1 & 0xFFF;
            mem->field_2A = 0;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = temp + (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            arg0->state++;
        }
        Gp_UpdateCoord(coord);
        if (!(mem->field_22 & 1)) {
            Gp_DrawEffSpriteE2(coord, (u16)((s16)mem->field_22 >> 1),
                               (s16)(mem->field_24 | mem->field_2A), mem->field_26);
        }
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        mem->field_22++;
        if (mem->field_22 < 0xC) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_DrawEffSpriteE2(GsCOORDINATE2* arg0, u16 arg1, u32 arg2, s16 arg3)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    u32               pal;
    s32               ang;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    pal   = arg2 >> 16;
    arg2 &= 0xFFF;
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz -= 0x20;
        if (block->otz < 0x10) {
            block->otz = 0x10;
        }
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = getClut(0x130 - (s16)pal * 0xA0, pal + 0x10A);
        setUV4(prim, arg1 * 0x28, 0x38, arg1 * 0x28 + 0x27, 0x38, arg1 * 0x28, 0x5F,
               arg1 * 0x28 + 0x27, 0x5F);
        block->dx = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang       = arg3 + 0x400;
        block->dx = (((arg2 * 39) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(ang)) >> 12;
        prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void Gp_InitPlayerWork(GpActorWork* arg0)
{
    GameActor*     actor;
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    GpObj*         obj;
    GpRec18*       recs;
    GpRec18*       link;
    McSaveData*    save;
    s32            packed;
    s32            size;
    s32            kind;
    s32            anim;
    s32            idx;
    GpAnimArg      sp;
    Task*          task;
    s32            zero;
    s32            temp;
    GsCOORDINATE2* next;

    actor = arg0->actor;
    extra = arg0->extra;
    coord = (GsCOORDINATE2*)extra->field_8;
    arg0->state++;
    arg0->field_24        = Gp_PlayerMsgTable;
    arg0->field_18        = Gp_TeardownSlot0;
    actor->field_938      = 0x13;
    Gp_ActorSlots[0]      = arg0;
    Wip_SysConfig.field_4 = &coord->coord;
    coord->sub            = &Gfx_ViewCoord;
    coord->flg            = 0;
    extra->field_C        = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    Gp_BindActorAnim(arg0);

    actor->field_985 = 0x10;
    actor->field_10  = coord->coord.t[0];
    actor->field_14  = coord->coord.t[1];
    actor->field_18  = coord->coord.t[2];

    recs            = actor->field_17C;
    obj             = (GpObj*)actor->field_AC;
    obj->field_C    = (GpRec18*)actor->field_88;
    obj->field_8    = coord;
    actor->field_90 = (s32)recs;
    obj->field_12   = -0x12C;
    save            = &Mc_SaveData;
    obj->field_10   = 0;
    obj->field_14   = 0;
    {
        s32 temp;
        temp          = save->field_22;
        obj->field_1C = 0x12C;
        obj->flags    = 4;
        packed        = 0x10000;
        obj->field_18 = temp | packed;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table((GpRec18*)actor->field_90, 0x12, 0);

    {
        s32            zero;
        s32            temp;
        GsCOORDINATE2* next;
        zero = 0;
        TOUCH_REG(zero);
        link            = (GpRec18*)actor->field_94;
        size            = 0xDC;
        obj->flags     |= 0xF200;
        obj             = (GpObj*)actor->field_CC;
        next            = (GsCOORDINATE2*)arg0->extra->field_8;
        obj->field_C    = link;
        obj->field_8    = next + 4;
        actor->field_9C = (s32)recs;
        obj->field_12   = 0x64;
        obj->field_10   = 0;
        obj->field_14   = 0x28;
        temp            = save->field_22;
        obj->field_1C   = size;
        obj->flags      = 0x14;
        obj->field_18   = temp | packed;
        Gp_LinkObj(zero, obj);
    }

    zero = 0;
    TOUCH_REG(zero);
    obj->flags     |= 0x8000;
    link            = (GpRec18*)actor->field_A0;
    obj             = (GpObj*)actor->field_EC;
    next            = (GsCOORDINATE2*)arg0->extra->field_8;
    obj->field_C    = link;
    obj->field_8    = next + 1;
    actor->field_A8 = (s32)recs;
    obj->field_10   = 0;
    obj->field_12   = 0x52;
    obj->field_14   = 0;
    temp            = save->field_22;
    obj->field_1C   = size;
    obj->flags      = 0x24;
    obj->field_18   = temp | packed;
    Gp_LinkObj(zero, obj);
    obj->flags |= 0xC000;

    kind             = actor->field_954;
    anim             = actor->field_93C;
    actor->field_920 = func_80104258(arg0, 0, 1, 1);
    task             = func_80104258(arg0, 1, 1, 1);
    actor->field_924 = task;
    if (task != NULL) {
        Gp_SyncHeldRelated();
        Gp_SpawnWeaponEff();
    }
    if (kind == 2) {
        sp.field_C  = 0;
        idx         = actor->field_93A;
        sp.field_8  = 0;
        sp.field_4  = anim;
        sp.field_10 = 0;
        sp.field_0  = (void*)idx;
        func_80104508(arg0, 0, &sp, 0);
        actor->field_984 = 0x38;
    }
    if ((*(u32*)&save->field_4 & 0xFFFF0000) == 0x1050000) {
        actor->field_991 = 1;
    }
}

void Gp_PlayerWorkState1(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GpObj*         objs[2];
    s32            dy;
    s32            i;
    s8             bits;

    actor = arg0->actor;
    coord = (GsCOORDINATE2*)arg0->extra->field_8;
    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x300)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_90, 0x12, &actor->field_930);
        } else {
            actor->field_992 = 0;
        }
    }

    objs[0] = (GpObj*)actor->field_AC;
    objs[1] = (GpObj*)actor->field_EC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= 0x4000;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~0x4000;
        }
    }
    actor->field_983 = 0;
    coord->flg       = 0;
    Gp_UpdateCoord(coord);
}

void Gp_AttachActorObj(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    register s32   id asm("s4");
    s32            kind;
    void**         scratch;
    register s32   hi asm("v1");
    u8*            head;
    register void* p asm("v0");
    GameActor*     actor;
    GpObj*         obj;
    GpActorD4Rec*  rec;
    VECTOR*        tmp;
    GsCOORDINATE2* src;
    Task*          task;
    register void* extra asm("v0");
    s16            vz;
    s32            scale;
    register s32   three asm("v0");
    s32            packed;
    register s32   flag asm("v0");

    id = arg1;
    SOFT_TOUCH_REG(id);
    kind = arg2;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(kind));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    head  = *scratch;
    actor = arg0->actor;
    p     = head - 0x10;
    obj   = (GpObj*)actor->field_10C;
    rec   = (GpActorD4Rec*)actor->field_14C;
    asm("" : "+r"(obj), "+r"(rec) : "r"(p));
    *scratch = p;
    task     = actor->field_91C;
    if (task != NULL) {
        tmp                               = p;
        extra                             = task->extra;
        src                               = (GsCOORDINATE2*)((TmdObject*)extra)->field_8;
        *(GsCOORDINATE2*)actor->field_3D4 = *src;
        Gfx_RotMatrixX(&((GsCOORDINATE2*)actor->field_3D4)->workm, 0x400, 0);
        obj->field_8                     = actor->field_3D4;
        ((GpActorSvec*)actor)->field_418 = 0;
        ((GpActorSvec*)actor)->field_41A = 0;
        ((GpActorSvec*)actor)->field_41C = 0;
        obj->field_C                     = (GpRec18*)actor->field_14C;
        COMPILER_BARRIER();
        three            = 3;
        packed           = id << 8;
        obj->flags       = three;
        flag             = 0x20000;
        flag             = kind | flag;
        packed          |= flag;
        obj->field_10    = 0;
        obj->field_12    = 0;
        obj->field_14    = 0;
        actor->field_124 = packed;
        *tmp             = D_80112FA4[id];
        rec->field_8     = tmp->vx;
        rec->field_A     = tmp->vy;
        vz               = tmp->vz;
        rec->field_0     = rec->field_8;
        rec->field_C     = vz;
        rec->field_2     = rec->field_A;
        rec->field_4     = rec->field_C + D_80112F60[id];
        USE_REG(id);
        scale = 0x100;
        if (Wip_SysConfig.field_21 == 0x13) {
            scale = 0x280;
        }
        rec->field_12 = scale;
        if (kind != 0xD) {
            rec->field_10 = scale;
        } else {
            rec->field_10 = 0x900;
        }
        rec->field_14 = actor->field_32C;
        Gp_LinkObj(1, obj);
        Gp_InitRec18Table(rec->field_14, 6, 0);
        USE_REG(actor);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

s32 func_801011D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32* arg3)
{
    void**          scratch;
    u8*             head;
    register void*  p asm("v0");
    GpDeltaScratch* s;
    s32             ret;
    s32             val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x10;
    s        = p;
    *scratch = p;
    ret      = func_800E0FEC((GpRec18*)arg1, s, arg2, arg3);
    if (ret != 0) {
        val = ((GpDeltaScratch*)(head - 0x10))->vx.w;
        if ((val & 0xFFFF) != 0) {
            ((volatile GpDeltaScratch*)s)->vx.w = val + ((val >= 0) ? 0x10000 : -0x10000);
        }
        val = s->vy.w;
        if ((val & 0xFFFF) != 0) {
            s->vy.w = val + ((val >= 0) ? 0x10000 : -0x10000);
        }
        val = s->vz.w;
        if ((val & 0xFFFF) != 0) {
            s->vz.w = val + ((val >= 0) ? 0x10000 : -0x10000);
        }
        arg0->coord.t[0] += s->vx.h.hi;
        arg0->coord.t[1] += s->vy.h.hi;
        arg0->coord.t[2] += s->vz.h.hi;
        if (arg3 != NULL) {
            *arg3 = func_800E1ACC((u8*)arg3);
        }
        if ((s->vx.w | s->vz.w) == 0) {
            ret = 0;
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

void func_8010133C(void)
{
    void**       scratch;
    u8*          head;
    GpScratch10* tmp;
    GpScratch10* s;
    s32          color;

    scratch    = (void**)G_SCRATCH_HEAD;
    color      = 0x808008;
    head       = *scratch;
    tmp        = (GpScratch10*)(head - 0x10);
    *scratch   = tmp;
    s          = tmp;
    s->field_8 = color;
    s->field_E = -0x58;
    for (s->field_0 = 0; s->field_0 < 2; s->field_0++) {
        s->field_4 = 0;
        s->field_C = -0x40;
        for (; s->field_4 < 3; s->field_4++) {
            s->field_C += 0x40;
            s->field_E -= 0x50;
        }
        s->field_8 = 0x37A78;
        s->field_E = 8;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void Gp_PlayerWorkState2(Task* arg0)
{
    arg0->state = 3;
}

void Gp_TeardownSlot0(GpActorWork* arg0)
{
    volatile GameActor* inner;
    Task*               task;

    inner            = arg0->actor;
    arg0->field_18   = NULL;
    Gp_ActorSlots[0] = NULL;
    task             = inner->field_914;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_918;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_91C;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_920;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_924;
    if (task != NULL) {
        Task_Kill(task);
    }
    Gp_UnlinkObj((GpObj*)inner->field_AC);
    Gp_UnlinkObj((GpObj*)inner->field_CC);
    Gp_UnlinkObj((GpObj*)inner->field_EC);
    Gp_UnlinkObj((GpObj*)inner->field_10C);
    Gp_UnlinkObj((GpObj*)inner->field_12C);
    Task_Kill((Task*)arg0);
}

void Gp_PlayerWorkTask(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = Gp_PlayerWorkStates;
    sp.funcs[arg0->state](arg0);
}

typedef struct {
    byte pad[0x973];
    u8   field_973;
} ActorDirByte;

void Gp_UpdatePlayerMove(void)
{
    void**              scratch;
    u8*                 head;
    u8*                 newhead;
    SVECTOR*            vec;
    GpActorWork*        work;
    GameActor*          actor;
    GameActor*          p;
    register TmdObject* extra asm("v0");
    GsCOORDINATE2*      coord;
    u16                 buttons;
    u16                 prev;
    s8                  f975;
    s8                  f977;
    GameSession*        session;
    Task*               task;
    MATRIX*             mat;
    register s8         f973 asm("a1");

    work     = Game_GetPtrSlot(3);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    newhead  = head - 8;
    *scratch = newhead;
    COMPILER_BARRIER();
    p     = work->actor;
    extra = work->extra;
    f973  = p->field_973;
    coord = (GsCOORDINATE2*)extra->field_8;
    f975  = p->field_975;
    TOUCH_REG3(coord, f975, p);
    prev = p->field_962;
    USE_REG(prev);
    actor = p;
    TOUCH_REG2(actor, p);
    p->field_976 = f975;
    p->field_974 = f973;
    session      = Game_Session;
    TOUCH_REG_USE(session, f973);
    f977 = ((volatile GameActor*)p)->field_977;
    USE_REG(f977);
    p->field_964       = prev;
    buttons            = session->field_58;
    p->field_978       = f977;
    p->field_962       = buttons;
    p->field_966       = p->field_962 & ~p->field_964;
    p->field_968       = p->field_964 & ~p->field_962;
    p->field_977       = (p->field_962 >> 6) & 1;
    Gp_StateF0.field_2 = 0;
    vec                = (SVECTOR*)newhead;
    if (D_80115768 == 0) {
        Gp_TickPlayerActor(work);
    }
    coord->coord.t[0] += actor->field_40;
    coord->coord.t[1] += actor->field_44;
    coord->coord.t[2] += actor->field_48;
    actor->field_40    = 0;
    actor->field_44    = 0;
    actor->field_48    = 0;
    Gp_ClearRec18Occupied(actor->field_17C);
    if (actor->field_91C != NULL) {
        Gp_ClearRec18Occupied(actor->field_32C);
    }
    if (actor->field_984 & 1) {
        coord->coord.t[1] += 0x80;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    if ((s8)actor->field_986 != 0) {
        ((SVECTOR*)(head - 8))->vx = (u16)actor->field_30.vx;
        vec->vy                    = (u16)actor->field_30.vy;
        vec->vz                    = (u16)actor->field_30.vz;
    } else {
        ((SVECTOR*)(head - 8))->vx =
            (u16)coord->workm.m[0][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
        vec->vy =
            (u16)coord->workm.m[1][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
        vec->vz =
            (u16)coord->workm.m[2][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
    }
    task                            = actor->field_91C;
    ((SVECTOR*)actor->field_88)->vx = vec->vx;
    ((SVECTOR*)actor->field_88)->vy = vec->vy;
    ((SVECTOR*)actor->field_88)->vz = vec->vz;
    ((SVECTOR*)actor->field_94)->vx = vec->vx;
    ((SVECTOR*)actor->field_94)->vy = vec->vy;
    ((SVECTOR*)actor->field_94)->vz = vec->vz;
    ((SVECTOR*)actor->field_A0)->vx = vec->vx;
    ((SVECTOR*)actor->field_A0)->vy = vec->vy;
    ((SVECTOR*)actor->field_A0)->vz = vec->vz;
    if (task != NULL) {
        *(GsCOORDINATE2*)actor->field_3D4 =
            *(GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
        mat = &((GsCOORDINATE2*)actor->field_3D4)->workm;
        if (Wip_SysConfig.field_21 != 0x17) {
            Gfx_RotMatrixX(mat, -0x400, 0);
            Gfx_RotMatrixY(mat, -0x20, 0);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void Gp_TickActorAnimState(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimRec* rec;
    s32        i;
    s32        anim;
    s32        extra;
    u16        flags;

    actor = arg0->actor;
    rec   = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
    switch (actor->field_95C) {
        case 0:
        case 1:
            break;
        case 2:
            if (actor->field_95E != 0) {
                break;
            }
            if (func_8010583C(arg0, 0, 0, 0) != 0) {
                break;
            }
            anim              = 9;
            extra             = 5;
            i                 = 1;
            actor->field_95E += i;
            actor             = arg0->actor;
            if (i < actor->field_938) {
                do {
                    Gp_AnimPlaySlot((GpAnimCtx*)actor->field_424, i, 0, anim, 0, 0, extra,
                                    actor->field_928);
                    actor->field_438[i].field_9 = actor->field_985;
                    i++;
                } while (i < actor->field_938);
            }
            break;
        case 3:
            break;
        case 5:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    Gp_ResetActorAnimState(arg0, 3);
                }
            }
            break;
        case 4:
        case 6:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 7:
        case 9:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->field_95E++;
                }
            }
            break;
        case 8:
            if (rec != NULL) {
                flags = actor->field_438[1].field_10;
                if ((flags & 1) || (flags & 2)) {
                    actor->field_95E++;
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 10:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->field_95E = 0x3E8;
                }
            }
            break;
    }
}

static const s32 s_jtbl_pad = 0;

void Gp_StepPlayerMove(GpActorWork* arg0)
{
    register GameActor*     actor asm("s3");
    register GsCOORDINATE2* coord asm("s5");
    register GpMoveScratch* s asm("s4");
    TmdObject*              extra;
    u16                     mode;
    SVECTOR*                vec;
    MATRIX*                 mat;
    s16*                    tbl;
    s32                     val;
    s32                     dz;
    register s32            lockz asm("a0");
    s32                     angle;
    s32                     flag;
    s32                     t2;

    {
        void**       scratch;
        register u8* tmp asm("v0");

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = *scratch;
        TOUCH_REG_MEM(tmp);
        tmp      = tmp - 0x40;
        s        = (GpMoveScratch*)tmp;
        *scratch = tmp;
    }

    actor = arg0->actor;
    extra = arg0->extra;
    mode  = actor->field_958;
    coord = (GsCOORDINATE2*)extra->field_8;
    switch (mode) {
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424,
                              (GpAnimSlot*)actor->field_438 + 1) == NULL) {
                case 0:
                    actor->field_0 = 0;
                    actor->field_4 = 0;
                    actor->field_8 = 0;
            } else {
                SVECTOR* vec0;

                s->scale = D_80112E10[(u16)actor->field_958];
                vec0     = &s->vec;
                Gfx_MatrixCol2(&coord->coord, vec0);
                VectorNormalSS(vec0, vec0);
                val            = s->vec.vx;
                val           *= actor->field_973;
                actor->field_0 = val / s->scale;
                actor->field_4 = 0;
                val            = s->vec.vz;
                val           *= actor->field_973;
                actor->field_8 = val / s->scale;
            }
            break;
        case 4:
            mat      = &coord->coord;
            tbl      = D_80112E10;
            vec      = &s->vec;
            s->scale = tbl[(u16)actor->field_958];
            Gfx_MatrixCol2(mat, vec);
            VectorNormalSS(vec, vec);
            val                = s->vec.vx;
            val               *= actor->field_973;
            actor->field_0     = val / s->scale;
            actor->field_4     = 0;
            val                = s->vec.vz;
            val               *= actor->field_973;
            actor->field_8     = val / s->scale;
            coord->coord.t[0] += actor->field_0;
            coord->coord.t[1] += actor->field_4;
            coord->coord.t[2] += actor->field_8;
            s->saved           = coord->coord;
            s->scale           = tbl[(u16)actor->field_958];
            Gp_GetLockPos((GpLockPos*)actor->field_90C, &s->lock);
            val   = coord->coord.t[0];
            lockz = s->lock.vz;
            val  -= s->lock.vx;
            if (val < 0) {
                val = -val;
            }
            s->vec.vx = val;
            dz        = coord->coord.t[2];
            dz       -= lockz;
            if (dz < 0) {
                dz = -dz;
            }
            val      += dz;
            s->vec.vx = val;
            s->angle  = 0x640000;
            angle     = 0x640000 / (s->vec.vx * 0x274);
            flag      = 0;
            TOUCH_REG_USE(flag, lockz);
            angle    = (0x800 - angle) >> 1;
            s->angle = angle;
            Gfx_RotMatrixY(mat, angle, flag);
            Gfx_MatrixCol2(mat, vec);
            val            = s->vec.vx;
            val           *= actor->field_975;
            actor->field_0 = val / s->scale;
            actor->field_4 = 0;
            val            = s->vec.vz;
            val           *= actor->field_975;
            actor->field_8 = val / s->scale;
            coord->coord   = s->saved;
            break;
    }
    coord->coord.t[0] += actor->field_0;
    COMPILER_BARRIER();
    coord->coord.t[1]      += actor->field_4;
    t2                      = coord->coord.t[2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x40;
    coord->coord.t[2]       = t2 + actor->field_8;
}

void Gp_TurnPlayer(GpActorWork* arg0)
{
    TmdObject*     extra;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            flag;
    SVECTOR*       rot;

    actor = arg0->actor;
    extra = arg0->extra;
    {
        register u16* tbl asm("a0");
        s32           idx;
        s32           yaw;

        idx   = actor->field_95A;
        coord = (GsCOORDINATE2*)extra->field_8;
        flag  = 0;
        if (idx != 0) {
            yaw             = *(volatile u8*)&actor->field_975;
            tbl             = D_80112E20;
            actor->field_52 = ((u16)actor->field_52 + tbl[idx] * (s8)yaw) & 0xFFF;
        }
    }
    rot = (SVECTOR*)&actor->field_50;
    TOUCH_REG2(rot, coord);
    coord = (GsCOORDINATE2*)&coord->coord;
    RotMatrix(rot, (MATRIX*)coord);
    MatrixNormal((MATRIX*)coord, (MATRIX*)coord);
    if ((s8)actor->field_97E == 1) {
        s32          temp;
        register s16 delta asm("v1");
        s32          val;

        if (actor->field_58 != 0) {
            val   = actor->field_58 >> 3;
            delta = val;
            temp  = val;
            flag  = 1;
            if (ABS(temp) < 0x20) {
                val = 0x20;
                if (temp < 0) {
                    val = -0x20;
                }
                delta = val;
            }
            actor->field_58 -= delta;
            if (ABS(actor->field_58) < 0x21) {
                actor->field_58 = 0;
            }
        }
        if (actor->field_5C != 0) {
            val   = actor->field_5C >> 3;
            delta = val;
            temp  = val;
            flag  = 1;
            if (ABS(temp) < 0x20) {
                val = 0x20;
                if (temp < 0) {
                    val = -0x20;
                }
                delta = val;
            }
            actor->field_5C -= delta;
            if (ABS(actor->field_5C) < 0x21) {
                actor->field_5C = 0;
            }
        }
        if (actor->field_60 != 0) {
            val   = actor->field_60 >> 3;
            delta = val;
            temp  = val;
            flag  = 1;
            if (ABS(temp) < 0x20) {
                val = 0x20;
                if (temp < 0) {
                    val = -0x20;
                }
                delta = val;
            }
            actor->field_60 -= delta;
            if (ABS(actor->field_60) < 0x21) {
                actor->field_60 = 0;
            }
        }
        if (actor->field_64 != 0) {
            val   = actor->field_64 >> 3;
            delta = val;
            temp  = val;
            flag  = 1;
            if (ABS(temp) < 0x20) {
                val = 0x20;
                if (temp < 0) {
                    val = -0x20;
                }
                delta = val;
            }
            actor->field_64 -= delta;
            if (ABS(actor->field_64) < 0x21) {
                actor->field_64 = 0;
            }
        }
        if (actor->field_70 != 0) {
            val   = actor->field_70 >> 3;
            delta = val;
            temp  = val;
            flag  = 1;
            if (ABS(temp) < 0x20) {
                val = 0x20;
                if (temp < 0) {
                    val = -0x20;
                }
                delta = val;
            }
            actor->field_70 -= delta;
            if (ABS(actor->field_70) < 0x21) {
                actor->field_70 = 0;
            }
        }
        if (flag == 0) {
            actor->field_97E = 0;
        }
    }
    coord        = (GsCOORDINATE2*)arg0->extra->field_8;
    coord[2].flg = 0;
    coord        = (GsCOORDINATE2*)&coord[2].coord;
    RotMatrixX(actor->field_58, (MATRIX*)coord);
    RotMatrixZ(actor->field_5C, (MATRIX*)coord);
    MatrixNormal((MATRIX*)coord, (MATRIX*)coord);
    coord        = (GsCOORDINATE2*)arg0->extra->field_8;
    coord[3].flg = 0;
    coord        = (GsCOORDINATE2*)&coord[3].coord;
    RotMatrixX(actor->field_60, (MATRIX*)coord);
    RotMatrixZ(actor->field_64, (MATRIX*)coord);
    MatrixNormal((MATRIX*)coord, (MATRIX*)coord);
    coord        = (GsCOORDINATE2*)arg0->extra->field_8;
    coord[4].flg = 0;
    coord        = (GsCOORDINATE2*)&coord[4].coord;
    Gfx_RotMatrixY((MATRIX*)coord, actor->field_6A, 0);
    MatrixNormal((MATRIX*)coord, (MATRIX*)coord);
    coord        = (GsCOORDINATE2*)arg0->extra->field_8;
    coord[6].flg = 0;
    coord        = (GsCOORDINATE2*)&coord[6].coord;
    Gfx_RotMatrixX((MATRIX*)coord, actor->field_70, 0);
    MatrixNormal((MATRIX*)coord, (MATRIX*)coord);
}

void Gp_AimYawToLock(GpActorWork* arg0, s32 arg1)
{
    void**       scratch;
    s32          hi;
    u8*          head;
    register u8* tmp asm("v1");
    GameActor*   actor;
    s32          thresh;

    thresh = arg1;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(thresh));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x6C;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        GpYawScratch*   block;
        GpAimRot*       rec;
        GsCOORDINATE2*  src;
        VECTOR3*        lock;
        register s32    val asm("v0");
        s32             dz;
        register s32    cmp asm("v1");
        GpAngleScratch* wrap;
        u8*             whead;
        s32             delta;
        s32             flag;
        register s32    packed asm("s2");
        s32             limit;
        register u16*   tbl asm("a1");

        block = (GpYawScratch*)tmp;
        TOUCH_REG(block);
        rec           = &D_801131B4[Wip_SysConfig.field_21];
        src           = (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8;
        block->rot.vx = rec->vx;
        block->rot.vy = rec->vy;
        block->rot.vz = rec->vz;
        Gp_PlaceCoordOffset(src, (GsCOORDINATE2*)block, (SVECTOR*)(head - 0xC));
        lock = (VECTOR3*)(head - 0x1C);
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x1C))->vx =
            ((VECTOR3*)(head - 0x1C))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        lock->vy = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        lock->vz = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val      = block->delta.vx;
        val      = ABS(val);
        val      = val * val;
        dz       = block->delta.vz;
        dz       = ABS(dz);
        dz       = dz * dz;
        val      = SquareRoot0(val + dz);
        cmp      = (s16)thresh;
        cmp      = cmp < val;
        if (cmp) {
            block->angle = ratan2(block->delta.vx, block->delta.vz);
            val          = *(u16*)&block->angle;
            cmp          = *(u16*)&actor->field_52;
            asm("lui %0, 0x1F80" : "=r"(whead) : "r"(val), "r"(cmp));
            whead = *(u8**)(whead + 0x3FC);
            val   = (s16)val - (s16)cmp;
            tmp   = (u8*)(whead - 0xC);
            wrap  = (GpAngleScratch*)tmp;
            TOUCH_REG_USE(wrap, tmp);
            ((GpAngleScratch*)(whead - 0xC))->field_0 = val;
            val                                      += 0x1000;
            wrap->field_4                             = val;
            val                                       = ((GpAngleScratch*)(whead - 0xC))->field_0;
            *(void**)G_SCRATCH_HEAD                   = wrap;
            delta                                     = val - 0x1000;
            wrap->field_8                             = delta;
            if (ABS(((GpAngleScratch*)(whead - 0xC))->field_0) < ABS(wrap->field_4) &&
                ABS(((GpAngleScratch*)(whead - 0xC))->field_0) < ABS(delta)) {
                cmp  = *(u16*)&((GpAngleScratch*)(whead - 0xC))->field_0;
                flag = 0x2000;
            } else if (ABS(wrap->field_4) < ABS(wrap->field_8)) {
                cmp  = *(u16*)&wrap->field_4;
                flag = 0x2000;
            } else {
                cmp  = *(u16*)&wrap->field_8;
                flag = 0x2000;
            }
            val = (s16)cmp;
            asm("lui %0, %%hi(D_80112E30)" : "=r"(tbl) : "r"(val));
            block->angle = val;
            asm("lui %0, %%hi(Wip_SysConfig)" : "=r"(val));
            asm("addiu %0, %1, %%lo(D_80112E30)" : "=r"(tbl) : "r"(tbl));
            asm("lbu %0, %%lo(Wip_SysConfig+0x21)(%1)" : "=r"(cmp) : "r"(val));
            asm("lui %0, 0x1F80" : "=r"(val) : "r"(cmp));
            val = (s32) * (void**)((u8*)val + 0x3FC);
            USE_REG(val);
            cmp                     = tbl[cmp];
            val                    += 0xC;
            *(void**)G_SCRATCH_HEAD = (void*)val;
            USE_REG(val);
            packed = cmp << 16;
            limit  = packed >> 16;
            if (func_800B9D80(flag) != 0) {
                val    = packed >> 17;
                limit += val;
            }
            {
                s32          angle;
                register s32 neg asm("v1");

                angle = block->angle;
                neg   = -limit;
                if (limit < angle) {
                    block->angle = limit;
                } else {
                    val = angle < neg;
                    if (val) {
                        block->angle = neg;
                    }
                }
            }
            actor->field_52 = ((u16)actor->field_52 + (u16)block->angle) & 0xFFF;
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x6C;
}

#define SCALE_PITCH(dst, src)        \
    do {                             \
        s32 _q;                      \
        s32 _x;                      \
        _x = (src);                  \
        asm volatile(                \
            "lui %0, 0x9249\n\t"     \
            "ori %0, %0, 0x2493\n\t" \
            "mult %1, %0\n\t"        \
            "mfhi %0\n\t"            \
            "addu %0, %0, %1\n\t"    \
            "sra %0, %0, 2\n\t"      \
            "sra %1, %1, 31\n\t"     \
            "subu %0, %0, %1\n\t"    \
            "sll %0, %0, 2"          \
            : "=&r"(_q), "+r"(_x));  \
        (dst) = _q;                  \
    } while (0)

void Gp_AimPitchToLock(GpActorWork* arg0)
{
    register void**   scratch asm("v0");
    u8*               head;
    register u8*      tmp asm("v1");
    GameActor*        actor;
    GpPitchScratch*   block;
    register VECTOR3* lock asm("s0");
    register VECTOR3* dest asm("a0");
    GsCOORDINATE2*    src;
    GpAimRot*         tbl;
    s32               item;
    Task*             slot;
    s32               val;
    register s32      dz asm("a0");
    s32               angle;
    s32               dist;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x84;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        block = (GpPitchScratch*)tmp;
        TOUCH_REG(block);
        src           = (GsCOORDINATE2*)arg0->extra->field_8;
        block->rot.vx = 0;
        block->rot.vy = -0x400;
        block->rot.vz = 0;
        Gp_PlaceCoordOffset(&src[2], (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
        lock = (VECTOR3*)(head - 0x24);
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x34))->vx =
            ((VECTOR3*)(head - 0x24))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest            = (VECTOR3*)(head - 0x34);
        dest->vy        = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz        = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val             = block->delta.vx;
        val             = ABS(val);
        val             = val * val;
        dz              = block->delta.vz;
        dz              = ABS(dz);
        dz              = dz * dz;
        val             = SquareRoot0(val + dz);
        block->dist     = val;
        val             = block->delta.vy >> 1;
        block->delta.vy = val;
        val             = ratan2(-val, block->dist);
        SCALE_PITCH(block->angle, val);
        angle        = block->angle - actor->field_58;
        block->angle = angle;
        if (angle >= 0x31) {
            block->angle = 0x30;
        } else if (angle < -0x30) {
            block->angle = -0x30;
        }
        if (ABS(actor->field_58 + block->angle) < 0x121) {
            actor->field_58 += block->angle;
            actor->field_5C  = (s16)(actor->field_58 / 5) * 3;
        }
        item = Wip_SysConfig.field_21;
        TOUCH_REG(item);
        tbl = D_801131B4;
        TOUCH_REG2(item, tbl);
        slot = actor->field_91C;
        TOUCH_REG_USE(item, slot);
        item = (item << 3) + (s32)tbl;
        {
            register TmdObject* extra asm("a0");
            u16                 vx;
            extra = (TmdObject*)slot->extra;
            TOUCH_REG(extra);
            vx            = ((GpAimRot*)item)->vx;
            src           = (GsCOORDINATE2*)extra->field_8;
            block->rot.vx = vx;
            block->rot.vy = ((GpAimRot*)item)->vy;
            block->rot.vz = ((GpAimRot*)item)->vz;
            Gp_PlaceCoordOffset(src, (GsCOORDINATE2*)block, (SVECTOR*)&block->rot);
        }
        lock = &block->lock;
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        block->delta.vx = block->lock.vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest            = &block->delta;
        dest->vy        = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz        = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val             = block->delta.vx;
        val             = ABS(val);
        val             = val * val;
        dz              = block->delta.vz;
        dz              = ABS(dz);
        dz              = dz * dz;
        val             = SquareRoot0(val + dz);
        dist            = val;
        block->dist     = dist;
        val             = ratan2(-block->delta.vy, dist);
        SCALE_PITCH(block->angle, val);
        angle        = block->angle - actor->field_60;
        block->angle = angle;
        if (angle >= 0x31) {
            block->angle = 0x30;
        } else if (angle < -0x30) {
            block->angle = -0x30;
        }
        if (ABS(actor->field_60 + block->angle) < 0x101) {
            actor->field_60 += block->angle;
            actor->field_64  = (s16)(actor->field_60 / 5) * 2;
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x84;
}

void Gp_AimPitchToLockAlt(GpActorWork* arg0)
{
    register void**   scratch asm("v0");
    u8*               head;
    register u8*      tmp asm("v1");
    GameActor*        actor;
    GpPitchScratch*   block;
    register VECTOR3* lock asm("s0");
    register VECTOR3* dest asm("a0");
    GsCOORDINATE2*    src;
    GpAimRot*         tbl;
    s32               item;
    Task*             slot;
    s32               val;
    register s32      dz asm("a0");
    s32               angle;
    s32               dist;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x84;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        block = (GpPitchScratch*)tmp;
        TOUCH_REG(block);
        src           = (GsCOORDINATE2*)arg0->extra->field_8;
        block->rot.vx = 0;
        block->rot.vy = -0x400;
        block->rot.vz = 0;
        Gp_PlaceCoordOffset(&src[2], (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
        lock = (VECTOR3*)(head - 0x24);
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x34))->vx =
            ((VECTOR3*)(head - 0x24))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest            = (VECTOR3*)(head - 0x34);
        dest->vy        = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz        = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val             = block->delta.vx;
        val             = ABS(val);
        val             = val * val;
        dz              = block->delta.vz;
        dz              = ABS(dz);
        dz              = dz * dz;
        val             = SquareRoot0(val + dz);
        block->dist     = val;
        val             = block->delta.vy >> 1;
        block->delta.vy = val;
        val             = ratan2(-val, block->dist);
        SCALE_PITCH(block->angle, val);
        angle        = block->angle - actor->field_5C;
        block->angle = angle;
        if (angle >= 0x31) {
            block->angle = 0x30;
        } else if (angle < -0x30) {
            block->angle = -0x30;
        }
        if (ABS(actor->field_5C + block->angle) < 0x121) {
            actor->field_5C += block->angle;
        }
        item = Wip_SysConfig.field_21;
        TOUCH_REG(item);
        tbl = D_801131B4;
        TOUCH_REG2(item, tbl);
        slot = actor->field_91C;
        TOUCH_REG_USE(item, slot);
        item = (item << 3) + (s32)tbl;
        {
            register TmdObject* extra asm("a0");
            u16                 vx;
            extra = (TmdObject*)slot->extra;
            TOUCH_REG(extra);
            vx            = ((GpAimRot*)item)->vx;
            src           = (GsCOORDINATE2*)extra->field_8;
            block->rot.vx = vx;
            block->rot.vy = ((GpAimRot*)item)->vy;
            block->rot.vz = ((GpAimRot*)item)->vz;
            Gp_PlaceCoordOffset(src, (GsCOORDINATE2*)block, (SVECTOR*)&block->rot);
        }
        lock = &block->lock;
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        block->delta.vx = block->lock.vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest            = &block->delta;
        dest->vy        = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz        = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val             = block->delta.vx;
        val             = ABS(val);
        val             = val * val;
        dz              = block->delta.vz;
        dz              = ABS(dz);
        dz              = dz * dz;
        val             = SquareRoot0(val + dz);
        dist            = val;
        block->dist     = dist;
        val             = ratan2(-block->delta.vy, dist);
        SCALE_PITCH(block->angle, val);
        angle        = block->angle - actor->field_64;
        block->angle = angle;
        if (angle >= 0x31) {
            block->angle = 0x30;
        } else if (angle < -0x30) {
            block->angle = -0x30;
        }
        if (ABS(actor->field_64 + block->angle) < 0x101) {
            actor->field_64 += block->angle;
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x84;
}

#undef SCALE_PITCH

void Gp_AimPitchRec(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    void**          scratch;
    s32             hi;
    u8*             head;
    register u8*    tmp asm("v1");
    GameActor*      actor;
    GpPitchScratch* block;
    VECTOR3*        lock;
    VECTOR3*        dest;
    GsCOORDINATE2*  src;
    GpAimRot*       rec;
    s32             val;
    register s32    dist asm("a1");
    register s32    cmp asm("v0");
    s32             dz;
    s32             angle;
    s32             thresh;

    thresh = arg2;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(thresh));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x84;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        block = (GpPitchScratch*)tmp;
        TOUCH_REG(block);
        rec           = &D_801131B4[arg1];
        src           = (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8;
        block->rot.vx = rec->vx;
        block->rot.vy = rec->vy;
        block->rot.vz = rec->vz;
        Gp_PlaceCoordOffset(src, (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
        lock = (VECTOR3*)(head - 0x24);
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x34))->vx = ((VECTOR3*)(head - 0x24))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest                          = (VECTOR3*)(head - 0x34);
        dest->vy                      = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz                      = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val                           = block->delta.vx;
        val                           = ABS(val);
        val                           = val * val;
        dz                            = block->delta.vz;
        dz                            = ABS(dz);
        dz                            = dz * dz;
        val                           = SquareRoot0(val + dz);
        dist                          = val;
        block->dist                   = dist;
        cmp                           = (s16)thresh;
        if (cmp < dist) {
            block->angle = ratan2(-block->delta.vy, dist);
            angle        = block->angle - actor->field_70;
            block->angle = angle;
            if (ABS(angle) >= 0x20) {
                if (angle >= 0x31) {
                    block->angle = 0x30;
                } else if (angle < -0x30) {
                    block->angle = -0x30;
                }
                if (ABS(actor->field_70 + block->angle) < 0x281) {
                    actor->field_70 += block->angle;
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x84;
}

void Gp_AimPitchDirect(GpActorWork* arg0)
{
    register void** scratch asm("v0");
    u8*             head;
    register u8*    tmp asm("v1");
    GameActor*      actor;
    GpPitchScratch* block;
    VECTOR3*        lock;
    VECTOR3*        dest;
    GsCOORDINATE2*  src;
    s32             val;
    s32             dz;
    s32             angle;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    tmp      = head - 0x84;
    *scratch = tmp;
    if (actor->field_90C != NULL) {
        block = (GpPitchScratch*)tmp;
        TOUCH_REG(block);
        src           = (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8;
        block->rot.vx = 0;
        block->rot.vy = 0;
        block->rot.vz = 0;
        Gp_PlaceCoordOffset(src, (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
        lock = (VECTOR3*)(head - 0x24);
        Gp_GetLockPos((GpLockPos*)actor->field_90C, lock);
        ((VECTOR3*)(head - 0x34))->vx = ((VECTOR3*)(head - 0x24))->vx - ((GsCOORDINATE2*)block)->coord.t[0];
        dest                          = (VECTOR3*)(head - 0x34);
        dest->vy                      = lock->vy - ((GsCOORDINATE2*)block)->coord.t[1];
        dest->vz                      = lock->vz - ((GsCOORDINATE2*)block)->coord.t[2];
        val                           = block->delta.vx;
        val                           = ABS(val);
        val                           = val * val;
        dz                            = block->delta.vz;
        dz                            = ABS(dz);
        dz                            = dz * dz;
        val                           = SquareRoot0(val + dz);
        block->dist                   = val;
        block->angle                  = ratan2(-block->delta.vy, val);
        angle                         = block->angle - actor->field_78;
        block->angle                  = angle;
        if (ABS(angle) >= 0x20) {
            if (angle >= 0x31) {
                block->angle = 0x30;
            } else if (angle < -0x30) {
                block->angle = -0x30;
            }
            if (ABS(actor->field_78 + block->angle) < 0x281) {
                actor->field_78 += block->angle;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x84;
}

void func_801030CC(GpActorWork* arg0)
{
    void**               scratch;
    u8*                  head;
    s32                  temp;
    RECT*                rect;
    GameActor*           actor;
    register GpImgRec*** table asm("a0");
    GpImgRec*            img;
    register s32         idx asm("v0");

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    temp     = (s32)(head - 8);
    *scratch = (void*)temp;
    rect     = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_80112E74;
            idx   = (s8)actor->field_987;
            temp  = Wip_SysConfig.field_26;
            idx   = idx * 4 - 5;
            idx   = idx + temp;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x4E;
                rect->w             = 0x19;
                rect->h             = 0x10;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_80112EB4;
            idx   = (s8)actor->field_98A;
            temp  = Wip_SysConfig.field_26;
            idx   = idx * 4 - 5;
            idx   = idx + temp;
            img   = table[idx][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 0xC;
                rect->y = 0x68;
                rect->w = 0xE;
                rect->h = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

inline static Task* spawn_tmd_attach(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*          task;
    GameActor*     actor;
    TmdObject*     extra;
    GpCoordExt*    coord;
    TmdObject*     obj;
    GsCOORDINATE2* saved;
    u8*            table;
    s32            type;

    extra = arg0->extra;
    actor = arg0->actor;
    saved = &extra->field_8[D_80112E04[arg2][arg1]];
    table = D_80112DFC;
    type  = Wip_SysConfig.field_26 - 2;
    task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)((TmdObject*)task->extra)->field_8;
    coord->sub      = saved;
    coord->field_44 = 0;
    obj             = (TmdObject*)task->extra;
    if (actor->field_910 != NULL) {
        obj->field_24 = 4;
        obj->field_25 = 6;
    } else {
        obj->field_24 = 6;
        obj->field_25 = 0;
    }
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    return task;
}

Task* func_80103294(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor = arg0->actor;
    if (actor->field_920 != NULL) {
        Task_Kill(actor->field_920);
    }
    actor->field_920 = spawn_tmd_attach(arg0, 0, arg1, arg2);
    if (actor->field_924 != NULL) {
        Task_Kill(actor->field_924);
    }
    actor->field_924 = spawn_tmd_attach(arg0, 1, arg1, arg2);
    return actor->field_924;
}

inline static Task* spawn_attach(Task* parent, s32 row, s32 item)
{
    GsCOORDINATE2* saved;
    Task*          task;
    TmdObject*     extra;
    GpCoordExt*    coord;
    s32            type;

    saved = ((TmdObject*)parent->extra)->field_8;
    if (item == 0) {
        return NULL;
    }
    type = D_80112DF4[row] - 1;
    task = Task_Spawn(7, type + item, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    extra           = (TmdObject*)task->extra;
    task->parent    = parent;
    coord           = (GpCoordExt*)extra->field_8;
    coord->sub      = saved;
    coord->field_44 = 1;
    return task;
}

s32 Gp_SpawnWeaponEff(void)
{
    GpActorWork*  work;
    GameActor*    actor;
    Task*         parent;
    Task*         task;
    WipSysConfig* cfg;
    s32           kind;
    s32           id;
    s32           arg2;
    TmdObject*    extra;
    GpCoordExt*   coord;
    GpEffWork*    eff;
    GameActor*    inner;
    GpAnimObj*    anim;
    register s32  ret asm("v0");

    work  = Game_GetPtrSlot(3);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    parent = actor->field_924;
    if (parent == NULL) {
        goto join_4C;
    }

    task             = spawn_attach(parent, Mc_SaveData.field_22, Wip_SysConfig.field_21);
    actor->field_91C = task;
    if (task == NULL) {
        goto join_4C;
    }

    cfg = &Wip_SysConfig;
    Gp_AttachActorObj(work, cfg->field_21, cfg->field_22);
    if (actor->field_914 != NULL) {
        goto join_50;
    }

    kind  = 0x16;
    extra = (TmdObject*)actor->field_91C->extra;
    id    = cfg->field_21;
    coord = (GpCoordExt*)extra->field_8;
    if (id != kind) {
        goto check_19;
    }
    id   = 0x80060024;
    arg2 = 0;
    goto do_call;

do_success:
    actor->field_914 = eff->field_0;
    Task_Reparent(work, eff->field_0);
    func_80106350(work, Wip_SysConfig.field_21, 0);
    goto join_50;

check_19:
    if (id != 0x19) {
        goto check_1C;
    }
    id = 0x80060029;
    goto do_call_item;

check_1C:
    if (id != 0x1C) {
        goto join_50;
    }
    id = 0x8006002A;
do_call_item:
    arg2 = cfg->field_21;
do_call:
    eff = Gp_SpawnEff(id, (GsCOORDINATE2*)coord, arg2, 0);
    if (eff != NULL) {
        goto do_success;
    }

join_4C:
join_50:
    actor->field_98F = 0;
    inner            = work->actor;
    anim             = (GpAnimObj*)work->extra;
    inner->field_93A = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    inner->field_928 = Gp_PlayerAnimBlkTbl[inner->field_93A];
    func_800B3F84((GpAnimCtx*)inner->field_424, inner->field_928, anim, &inner->field_7A8,
                  (GpAnimSlot*)inner->field_438);
    func_801066DC(work, 1);
    ret                               = (s32)actor->field_91C;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    return ret;
}

Task* Gp_SpawnPlayer(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;

    task = Task_Spawn(7, Wip_SysConfig.field_26 + 3, arg2, (s32)arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = Mem_Calloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
    Task_Kill(task);
    return NULL;

have_actor:
    Game_SetPtrSlot(task, 3);
    task->idMap = (TaskIdMap*)actor;
    Mem_Set(actor, 0, 0x998);
    actor->field_93C  = arg3->field_0;
    actor->field_52   = arg0->field_0;
    coord             = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    D_80115768        = 0;
    if (arg3->field_2 != 0) {
        actor->field_954 = 2;
    }
    return task;
}

void Gp_CaptureActorPad(GpActorWork* arg0)
{
    GameActor* actor;
    u16        buttons;
    s32        flag;

    actor            = arg0->actor;
    actor->field_974 = actor->field_973;
    actor->field_976 = actor->field_975;
    actor->field_964 = actor->field_962;
    buttons          = Game_Session->field_58;
    actor->field_978 = actor->field_977;
    actor->field_962 = buttons;
    actor->field_966 = actor->field_962 & ~actor->field_964;
    actor->field_968 = actor->field_964 & ~actor->field_962;
    flag             = 1;
    actor->field_977 = (actor->field_962 >> 6) & flag;
}

void Gp_BindActorAnim(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_93A = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    actor->field_928 = Gp_PlayerAnimBlkTbl[actor->field_93A];
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->field_438);
}

void Gp_AnimResetChildSlots(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            Gp_AnimResetSlot((GpAnimCtx*)actor->field_424, i, arg1);
            actor->field_438[i].field_9 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void Gp_AnimPlayChildSlots(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            Gp_AnimPlaySlot((GpAnimCtx*)actor->field_424, i, 0, arg1, 0, 0, 0, actor->field_928);
            actor->field_438[i].field_9 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void Gp_AnimPlayChildSlotsEx(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            Gp_AnimPlaySlot((GpAnimCtx*)actor->field_424, i, 0, arg1, 0, 0, arg3, actor->field_928);
            actor->field_438[i].field_9 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void Gp_AnimTickChildSlots(GpActorWork* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->actor;
    i     = 1;
    if (i < inner->field_938) {
        do {
            Gp_AnimTickIndex((GpAnimCtx*)inner->field_424, i);
            i++;
        } while (i < inner->field_938);
    }
}

s32 Gp_HpBand(void)
{
    WipSysConfig* p;
    s32           temp;
    s32           ret;

    p    = &Wip_SysConfig;
    temp = (u16)p->field_1a << 16;
    if ((temp >> 17) < p->field_18) {
        ret = 0;
    } else {
        ret = 1;
        if ((temp >> 18) >= p->field_18) {
            ret = 2;
        }
    }
    return ret;
}

void Gp_DetachLinkNode(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;

    inner = arg0->actor;
    node  = inner->field_90C;
    if (node != NULL) {
        node->field_5    = 0;
        inner->field_90C = NULL;
    }
    inner->field_97E = 1;
}

s32 Gp_ApplyDirArg(GpActorWork* arg0, GpDirArg* arg1)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            delta;
    register s32   temp asm("v1");
    s32            val;

    actor = arg0->actor;
    if (arg1->field_10 == 7) {
        if ((arg1->field_0 != 0) || (arg1->field_8 != 0)) {
            coord = (GsCOORDINATE2*)arg0->extra->field_8;
            delta = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            delta = delta - ratan2(arg1->field_0, arg1->field_8);
            temp  = delta;
            if ((s16)delta >= 0x802) {
                temp = delta - 0x1000;
            }
            if ((s16)temp < -0x800) {
                temp += 0x1000;
            }
            val = temp << 16;
            val = val >> 16;
            if (val < 0) {
                val = -val;
            }
            if (val < 0x400) {
                actor->field_973 = 1;
            } else {
                actor->field_973 = -1;
            }
        }
    }
    return actor->field_973;
}

void func_80103C74(GsCOORDINATE2* arg0, VECTOR3* arg1, VECTOR3* arg2)
{
    arg2->vx = arg1->vx - arg0->coord.t[0];
    arg2->vy = arg1->vy - arg0->coord.t[1];
    arg2->vz = arg1->vz - arg0->coord.t[2];
}

void func_80103CB4(GsCOORDINATE2* arg0, s32 arg1, VECTOR3* arg2, VECTOR3* arg3)
{
    void**  scratch;
    u8*     head;
    VECTOR* vec;

    scratch                      = (void**)G_SCRATCH_HEAD;
    head                         = *scratch;
    vec                          = (VECTOR*)(head - 0x10);
    *scratch                     = vec;
    ((VECTOR*)(head - 0x10))->vx = 0;
    vec->vy                      = -0x600;
    vec->vz                      = 0;
    ApplyMatrixLV(&arg0->coord, vec, vec);
    arg3->vx = arg2->vx - (arg0->coord.t[0] + ((VECTOR*)(head - 0x10))->vx);
    *scratch = (u8*)*scratch + 0x10;
    arg3->vy = arg2->vy - (arg0->coord.t[1] + vec->vy);
    arg3->vz = arg2->vz - (arg0->coord.t[2] + vec->vz);
}

s32 func_80103D8C(s32 arg0, s32 arg1)
{
    arg0 = ABS(arg0);
    arg0 = arg0 * arg0;
    arg1 = ABS(arg1);
    arg1 = arg1 * arg1;
    return SquareRoot0(arg0 + arg1);
}

s32 func_80103DD4(VECTOR3* arg0, VECTOR3* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR3* vec;
    s32      vz;
    s32      absz;
    s32      vx;

    scratch                       = (void**)G_SCRATCH_HEAD;
    head                          = *scratch;
    ((VECTOR3*)(head - 0x10))->vx = arg0->vx - arg1->vx;
    vec                           = (VECTOR3*)(head - 0x10);
    vec->vy                       = arg0->vy - arg1->vy;
    vz                            = arg0->vz - arg1->vz;
    absz                          = ABS(vz);
    vec->vz                       = vz;
    absz                          = absz * absz;
    vx                            = ((VECTOR3*)(head - 0x10))->vx;
    vx                            = ABS(vx);
    vx                            = vx * vx;
    *scratch                      = vec;
    vx                            = SquareRoot0(vx + absz);
    *scratch                      = (u8*)*scratch + 0x10;
    return vx;
}

s16 func_80103E7C(s16 arg0, s16 arg1)
{
    void**          scratch;
    u8*             head;
    GpAngleScratch* block;
    register s32    tmp asm("v0");
    s32             delta;
    u16             ret;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    delta   = arg1 - arg0;
    tmp     = (s32)(head - 0xC);
    block   = (GpAngleScratch*)tmp;
    TOUCH_REG_USE(block, tmp);
    ((GpAngleScratch*)(head - 0xC))->field_0 = delta;
    delta                                   += 0x1000;
    block->field_4                           = delta;
    tmp                                      = ((GpAngleScratch*)(head - 0xC))->field_0;
    *scratch                                 = block;
    delta                                    = tmp - 0x1000;
    block->field_8                           = delta;
    if (ABS(((GpAngleScratch*)(head - 0xC))->field_0) < ABS(block->field_4) &&
        ABS(((GpAngleScratch*)(head - 0xC))->field_0) < ABS(delta)) {
        ret = ((GpAngleScratch*)(head - 0xC))->field_0;
    } else if (ABS(block->field_4) < ABS(block->field_8)) {
        ret = block->field_4;
    } else {
        ret = block->field_8;
    }
    tmp          = (s32)G_SCRATCH_HEAD;
    *(void**)tmp = (u8*)*(void**)tmp + 0xC;
    return ret;
}

void Gp_TrackLockTarget(GpActorWork* arg0)
{
    GameActor*    actor;
    GpLinkNode*   node;
    WipSysConfig* p;
    s32           val;

    actor = arg0->actor;
    node  = actor->field_90C;
    if (node == NULL) {
        actor->field_97E = 1;
        return;
    }
    if (node->field_4 & 1) {
        node->field_5    = 0;
        actor->field_90C = NULL;
        actor->field_97E = 1;
        return;
    }
    if ((s8)actor->field_97E == 2) {
        p = &Wip_SysConfig;
        if (p->field_21 == 0x17) {
            val = 0x200;
        } else {
            val = 0x180;
        }
        Gp_AimYawToLock(arg0, val);
        if (p->field_21 == 0x17) {
            Gp_AimPitchToLockAlt(arg0);
        } else {
            Gp_AimPitchToLock(arg0);
        }
    }
}

GsCOORDINATE2* func_8010403C(s32 arg0)
{
    Task* slot;
    u8    idx;

    slot = Game_GetPtrSlot(3);
    idx  = D_80112E2C[Mc_SaveData.field_22 - 1][arg0];
    return &((GsCOORDINATE2*)((TmdObject*)slot->extra)->field_8)[idx];
}

void Gp_PlaceCoordOffset(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, SVECTOR* arg2)
{
    MATRIX* world;

    arg0->flg = 0;
    Gp_UpdateCoord(arg0);
    arg1->workm = arg0->workm;
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0tr_real();
    gte_stlvnl(arg1->workm.t);
    world = &Gfx_ViewWorldMtx;
    Gp_WorldToLocal(world, &arg1->workm, &arg1->coord);
    arg1->sub = (GsCOORDINATE2*)((u8*)world - OFFSET_OF(GsCOORDINATE2, workm));
    arg1->flg = 0;
    Gp_UpdateCoord(arg1);
}

s32 func_801041B4(GpActorWork* arg0)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    for (i = 0; i < 0x12; i++) {
        if ((actor->field_17C[i].field_4 & 0x100100) == 0x100000) {
            return 1;
        }
    }
    return 0;
}

void func_801041FC(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;
    GpPadEvt*  entry;
    u8         count;
    s32        idx;

    actor = arg0->actor;
    count = actor->field_981;
    if ((s8)actor->field_981 == 0) {
        idx = arg1 & 0xFFFF;
        SOFT_BARRIER();
        actor->field_981 = count + 1;
        entry            = &D_80112E28[idx];
        Pad_PostEvent(0, 1, entry->field_0, entry->field_2);
    }
}

Task* func_80104258(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*          task;
    GameActor*     actor;
    TmdObject*     extra;
    GpCoordExt*    coord;
    TmdObject*     obj;
    GsCOORDINATE2* saved;
    u8*            table;
    s32            type;

    extra = arg0->extra;
    actor = arg0->actor;
    saved = &extra->field_8[D_80112E04[arg2][arg1]];
    table = D_80112DFC;
    type  = Wip_SysConfig.field_26 - 2;
    task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)((TmdObject*)task->extra)->field_8;
    coord->sub      = saved;
    coord->field_44 = 0;
    obj             = (TmdObject*)task->extra;
    if (actor->field_910 != NULL) {
        obj->field_24 = 4;
        obj->field_25 = 6;
    } else {
        obj->field_24 = 6;
        obj->field_25 = 0;
    }
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    return task;
}

Task* func_80104364(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*          task;
    GsCOORDINATE2* saved;
    TmdObject*     extra;
    GpCoordExt*    coord;
    s32            type;

    saved = arg0->extra->field_8;
    if (arg2 == 0) {
        return NULL;
    }
    type = D_80112DF4[arg1] - 1;
    task = Task_Spawn(7, type + arg2, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra           = (TmdObject*)task->extra;
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)extra->field_8;
    coord->sub      = saved;
    coord->field_44 = 1;
    return task;
}

s32 Gp_KillPlayerEffs(void)
{
    GpActorWork* work;
    GameActor*   actor;
    Task*        task;

    work  = Game_GetPtrSlot(3);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    task = actor->field_918;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_918 = NULL;
    }

    task = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_91C = NULL;
    }

    task = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_914 = NULL;
    }

    Gp_UnlinkObj((GpObj*)actor->field_10C);
    return 1;
}

Task* func_80104490(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*          task;
    GsCOORDINATE2* saved;
    TmdObject*     extra;

    saved  = (GsCOORDINATE2*)((TmdObject*)arg0->actor->field_91C->extra)->field_8;
    arg2 <<= 2;
    arg1  += 0x60;
    task   = Task_Spawn(7, arg2 + arg1, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra                                 = (TmdObject*)task->extra;
    task->parent                          = (Task*)arg0;
    ((GsCOORDINATE2*)extra->field_8)->sub = saved;
    return task;
}

s32 func_80104508(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2, s32 arg3)
{
    GameActor*    actor;
    GpAnimObj*    extra;
    WipSysConfig* p;

    actor             = arg0->actor;
    extra             = (GpAnimObj*)arg0->extra;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 1;
    if (actor->field_928 != Gp_PlayerAnimBlkTbl[(s32)arg2->field_0]) {
        actor->field_928 = Gp_PlayerAnimBlkTbl[(s32)arg2->field_0];
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->field_438);
        actor->field_93A = (u16)arg2->field_0;
    }
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        Gp_AnimResetChildSlots(arg0, arg2->field_4);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, arg2->field_4, 0, arg2->field_C);
    }
    if (arg2->field_10 == 0) {
        actor->field_983 = 0x38;
    } else {
        actor->field_983 = 7;
    }
    return 0;
}

s32 func_80104684(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor*     actor;
    TmdObject*     extra;
    void           (*func)(TmdObject*);
    Task*          child;
    Task*          cur;
    s32            hi;
    register Task* temp asm("v0");
    u16            flags;

    actor = arg0->actor;
    extra = (TmdObject*)arg0->extra;
    func  = NULL;
    switch (arg2) {
        case 0:
            asm("lui %0, %%hi(Tmd_AllocBuffers)" : "=r"(hi));
            flags = extra->field_C;
            asm("addiu %0, %1, %%lo(Tmd_AllocBuffers)" : "=r"(func) : "r"(hi));
            extra->field_C = (flags | 0x80) & 0xFFFB;
            break;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            break;
        case 2:
            asm("lui %0, %%hi(Tmd_FreeBuffers)" : "=r"(hi));
            flags = extra->field_C;
            asm("addiu %0, %1, %%lo(Tmd_FreeBuffers)" : "=r"(func) : "r"(hi));
            extra->field_C = flags | 0x84;
            break;
        case 3:
            extra->field_C = extra->field_C | 0x84;
            break;
        case 4:
            asm("lui %0, %%hi(Tmd_AllocBuffers)" : "=r"(hi));
            flags = extra->field_C;
            asm("addiu %0, %1, %%lo(Tmd_AllocBuffers)" : "=r"(func) : "r"(hi));
            extra->field_C = flags & 0xFF7B;
            break;
    }
    if (func != NULL) {
        func(extra);
    }
    if (actor->field_920 != NULL) {
        ((TmdObject*)actor->field_920->extra)->field_C = extra->field_C;
        if (func != NULL) {
            func(extra);
        }
    }
    if (actor->field_924 != NULL) {
        ((TmdObject*)actor->field_924->extra)->field_C = extra->field_C;
        if (func != NULL) {
            func(extra);
        }
    }
    if (actor->field_91C != NULL) {
        {
            register TmdObject* dest asm("v1");
            dest          = (TmdObject*)actor->field_91C->extra;
            dest->field_C = extra->field_C;
        }
        temp = actor->field_91C->firstChild;
        if (temp != NULL) {
            child                               = temp;
            ((TmdObject*)child->extra)->field_C = extra->field_C;
            cur                                 = child;
            if (func != NULL) {
                func(extra);
            }
            if (child->nextSibling != child) {
                do {
                    cur                               = cur->nextSibling;
                    ((TmdObject*)cur->extra)->field_C = extra->field_C;
                    if (func != NULL) {
                        func(extra);
                    }
                } while (cur->nextSibling != child);
            }
        }
    }
    return 0;
}

s32 Gp_EnterActorMode2(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    TmdObject*     extra;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* next;
    u16            mode;
    VECTOR         vec;

    extra = arg0->extra;
    actor = arg0->actor;
    coord = (GsCOORDINATE2*)extra->field_8;
    mode  = actor->field_954;
    next  = coord + 1;
    if (mode != 2) {
        return 1;
    }
    if (arg2 != mode) {
        vec.vx = next->coord.t[0];
        vec.vy = next->coord.t[1];
        vec.vz = next->coord.t[2];
        ApplyMatrixLV(&coord->coord, &vec, &vec);
        coord->coord.t[0] += vec.vx;
        coord->coord.t[2] += vec.vz;
        next->coord.t[0]   = 0;
        next->coord.t[2]   = 0;
    }
    actor->field_10                   = coord->coord.t[0];
    actor->field_14                   = coord->coord.t[1];
    actor->field_18                   = coord->coord.t[2];
    actor->field_93A                  = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    actor->field_928                  = Gp_PlayerAnimBlkTbl[actor->field_93A];
    actor->field_985                  = 0x10;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    if (Gp_StateF0.field_0 == 1) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, (GpAnimObj*)extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->field_438);
        if (arg2 == mode) {
            Gp_ResetActorAnimState(arg0, 0);
        } else {
            func_8010870C(arg0, 0);
        }
        return 0;
    }
    if (arg2 == mode) {
        func_80108874(arg0);
        return 0;
    }
    if (arg2 == 1) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, (GpAnimObj*)extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->field_438);
        func_801066DC(arg0, 1);
    } else {
        func_801066DC(arg0, 0);
    }
    return 0;
}

void func_80104A4C(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor       = arg0->actor;
    p           = &Wip_SysConfig;
    p->field_24 = 0;
    if (actor->field_954 != 2) {
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (actor->field_966 & 0x20) {
                    p->field_24 = 1;
                }
            }
        }
    }
}

void func_80104AAC(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
}

s32 func_80104B54(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor*    actor;
    GpAnimObj*    extra;
    WipSysConfig* p;

    actor             = arg0->actor;
    extra             = (GpAnimObj*)arg0->extra;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 1;
    actor->field_928 = arg2->field_0;
    actor->field_93A = 0x7FFF;
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->field_438);
        Gp_AnimResetChildSlots(arg0, arg2->field_4);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, arg2->field_4, 0, arg2->field_C);
    }
    if (arg2->field_10 == 0) {
        actor->field_983 = 0x38;
    } else {
        actor->field_983 = 7;
    }
    return 0;
}

s32 func_80104CAC(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor* actor;
    GpAnimObj* extra;
    s32        flag;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_928 = arg2->field_0;
    actor->field_93A = 0x7FFF;
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->field_438);
        Gp_AnimResetChildSlots(arg0, arg2->field_4);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, arg2->field_4, 0, arg2->field_C);
    }
    flag = arg2->field_10;
    if (flag == 0) {
        flag = 0x38;
    } else {
        flag = 7;
    }
    actor->field_983 = flag;
    return 0;
}

s32 func_80104D68(Task* arg0, s32 arg1, GpXformArg* arg2)
{
    TmdObject*     extra;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    extra             = (TmdObject*)arg0->extra;
    actor             = (GameActor*)arg0->idMap;
    coord             = (GsCOORDINATE2*)extra->field_8;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    coord->coord.t[2] = arg2->field_8;
    actor->field_50   = arg2->field_10;
    actor->field_52   = arg2->field_12;
    actor->field_54   = arg2->field_14;
    mtx               = &coord->coord;
    RotMatrix((SVECTOR*)&actor->field_50, mtx);
    MatrixNormal(mtx, mtx);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    return 0;
}

s32 func_80104E00(GpActorWork* arg0, s32 arg1, GpXformArg* arg2)
{
    GameActor*          actor;
    register GameActor* inner asm("s1");
    WipSysConfig*       p;
    void**              scratch;
    u8*                 head;
    s32                 val;
    s32                 mode;
    s32                 angle;
    s32                 flag;

    flag              = 2;
    scratch           = (void**)G_SCRATCH_HEAD;
    head              = *scratch;
    *scratch          = head - 0x10;
    inner             = arg0->actor;
    actor             = inner;
    p                 = &Wip_SysConfig;
    actor->field_954  = flag;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    inner->field_982     = 1;
    inner->field_956     = flag;
    inner->field_983     = 0x38;
    angle                = (u16)arg2->field_12;
    inner->field_82      = angle;
    val                  = func_80103E7C(inner->field_52, angle);
    *(s32*)(head - 0x10) = val;
    mode                 = 6;
    if (val < 0) {
        mode = 5;
    }
    Gp_AnimPlayChildSlots(arg0, mode, 0);
    *scratch = (u8*)*scratch + 0x10;
    return 0;
}

s32 func_80104F5C(GpActorWork* arg0, s32 arg1, GpFacingArg* arg2)
{
    GameActor*    actor;
    WipSysConfig* p;
    s32           mode;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 3;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_80  = arg2->field_0;
    actor->field_82  = arg2->field_4;
    mode             = 0x24;
    if (arg2->field_0 != 0) {
        mode = 0x25;
    }
    Gp_AnimPlayChildSlots(arg0, mode, 0);
    return 0;
}

s32 Gp_SetActorDest(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 4;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_20  = arg2->field_0;
    actor->field_24  = arg2->field_4;
    actor->field_28  = arg2->field_8;
    if (arg3 != NULL) {
        actor->field_93C = arg3->field_0;
        actor->field_93E = arg3->field_4;
    } else {
        actor->field_93C = 0;
        actor->field_93E = 0;
    }
    return 0;
}

s32 func_80105190(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 4;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_20  = arg2->field_0;
    actor->field_24  = arg2->field_4;
    actor->field_28  = arg2->field_8;
    if (arg3 != NULL) {
        actor->field_93C = arg3->field_0;
        actor->field_93E = arg3->field_4;
    } else {
        actor->field_93C = 0;
        actor->field_93E = 0;
    }
    actor->field_956 = 8;
    return 0;
}

s32 func_801052B8(GpActorWork* arg0, s32 arg1, GpCountArg* arg2)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 5;
    actor->field_982 = 1;
    actor->field_983 = 0x38;
    actor->field_93E = arg2->field_0;
    actor->field_934 = arg2->field_4;
    return 0;
}

s32 Gp_MoveActorBy(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    WipSysConfig*  p;

    actor = arg0->actor;
    coord = (GsCOORDINATE2*)arg0->extra->field_8;
    if (arg2->field_12 == 0) {
        p                 = &Wip_SysConfig;
        actor->field_954  = 2;
        actor->field_95E  = 0;
        actor->field_973  = 0;
        actor->field_975  = 0;
        p->field_24       = 0;
        actor->field_97E  = 0;
        actor->field_60   = 0;
        actor->field_58   = 0;
        actor->field_64   = 0;
        actor->field_5C   = 0;
        actor->field_6A   = 0;
        actor->field_68   = 0;
        actor->field_70   = 0;
        actor->field_96C  = 0;
        actor->field_12A &= 0x3FFF;
        func_80106350(arg0, p->field_21, 0);
        if (Game_Session->field_1 != 0) {
            ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
        }
        actor->field_956 = 1;
        actor->field_982 = 1;
    }
    actor->field_983   = arg2->field_10;
    coord->coord.t[0] += arg2->field_0;
    coord->coord.t[1] += arg2->field_4;
    coord->coord.t[2] += arg2->field_8;
    Gp_ApplyDirArg(arg0, (GpDirArg*)arg2);
    return func_801041B4(arg0);
}

s32 func_801054D8(GpActorWork* arg0, s32 arg1, GpDelayArg* arg2)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor = arg0->actor;
    if ((s8)actor->field_97A != 0) {
        return 1;
    }
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956     = 6;
    Gp_StateC08.field_6 |= 1;
    actor->field_934     = arg2->field_14;
    actor->field_93E     = 0;
    return 0;
}

s32 func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 0xA;
    actor->field_983 = 0x38;
    return 0;
}

s32 func_80105690(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 7;
    actor->field_934 = arg2;
    return 0;
}

s32 func_80105754(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;
    s32           ret;

    actor = arg0->actor;
    ret   = 0;
    if (actor->field_954 != 2) {
        p                 = &Wip_SysConfig;
        actor->field_954  = 2;
        actor->field_95E  = 0;
        actor->field_973  = 0;
        actor->field_975  = 0;
        p->field_24       = 0;
        actor->field_97E  = 0;
        actor->field_60   = 0;
        actor->field_58   = 0;
        actor->field_64   = 0;
        actor->field_5C   = 0;
        actor->field_6A   = 0;
        actor->field_68   = 0;
        actor->field_70   = 0;
        actor->field_96C  = 0;
        actor->field_12A &= 0x3FFF;
        func_80106350(arg0, p->field_21, ret);
        if (Game_Session->field_1 != 0) {
            ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
        }
        actor->field_956 = 0xB;
    } else {
        ret = 1;
    }
    return ret;
}

s32 func_80105828(GpActorWork* arg0)
{
    return arg0->actor->field_982;
}

s32 func_8010583C(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;
    s32        ret;

    actor = arg0->actor;
    ret   = 0;
    for (i = actor->field_938 - 1; i > 0; i--) {
        if ((actor->field_438[i].field_10 & 0x100) == 0) {
            ret = 1;
            break;
        }
    }
    return ret;
}

s32 func_80105894(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;

    actor = (GameActor*)((arg1 * sizeof(GameActorSlot)) + (s32)arg0->actor);
    return (actor->field_438[0].field_10 & 0x102) == 0;
}

s32 func_801058BC(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    if (arg2 <= 0) {
        arg2 = 1;
    } else if (arg2 >= 0x80) {
        arg2 = 0x7F;
    }
    i = 1;
    if (i < actor->field_938) {
        do {
            actor->field_438[i].field_9 = arg2;
            i++;
        } while (i < actor->field_938);
    }
    actor->field_985 = arg2;
    return 0;
}

s32 Gp_CopyPlayerAnim(GpActorWork* arg0, s32 arg1, GpCopyArg* arg2)
{
    s32* dest;
    s32* src;
    s32  i;
    s32  count;

    dest  = (s32*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21];
    src   = arg2->field_0;
    count = arg2->field_4;
    if (count >= 0x21) {
        return 1;
    }
    dest = ((GpAnimBlk*)dest)->field_BC;
    for (i = 0; i < arg2->field_4; i++) {
        dest[i] = src[i];
    }
    return 0;
}

s32 Gp_ApplyPlayerDamage(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        ret;
    s32        out;

    actor = arg0->actor;
    ret   = 0;
    if (Mc_SaveData.field_5C2 == 0) {
        ret = Gp_ApplyHpDamage((s16)Gp_ScaleDamage(arg2, 0, &out, 0));
        if (ret != 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
        } else if (actor->field_910 == 0) {
            func_8010A42C(arg0, (u8)out);
        }
    }
    return ret;
}

s32 func_80105A60(Task* arg0, s32 arg1, s32 arg2)
{
    Gp_ReparentCoord((GsCOORDINATE2*)arg2, (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8);
    return 0;
}

s32 func_80105A8C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;

    inner = arg0->actor;
    if (arg2 == 0) {
        inner->field_958 = 1;
    } else {
        inner->field_958 = 3;
    }
    return 0;
}

s32 func_80105AB0(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;

    inner = arg0->actor;
    if (arg2 == 0) {
        inner->field_987 = 1;
        inner->field_98A = 2;
        inner->field_988 = 0;
        inner->field_98B = 0;
        inner->field_989 = 0;
        inner->field_98C = 0;
    } else if (arg2 < 4) {
        inner->field_987 = arg2 + 1;
        inner->field_988 = 0;
        inner->field_989 = 0;
    } else {
        inner->field_98A = arg2 - 3;
        inner->field_98B = 0;
        inner->field_98C = 0;
    }
    return 0;
}

void func_80105B0C(GpActorWork* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->actor;
    i     = 1;
    if (i < inner->field_938) {
        do {
            Gp_AnimTickSlot2((GpAnimCtx*)inner->field_424, (GpAnimSlot*)inner->field_438 + i);
            i++;
        } while (i < inner->field_938);
    }
}

void func_80105B74(VECTOR3* arg0)
{
    GameActor* actor;

    actor           = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    actor->field_40 = arg0->vx;
    actor->field_44 = arg0->vy;
    actor->field_48 = arg0->vz;
}

s32 Gp_PickNearestRec18(GpRec18* arg0, GsCOORDINATE2* arg1, GsCOORDINATE2* arg2)
{
    s32 minDist;

    minDist = 0x7FFFFFFF;
    if (Gp_CountRec18Hi(arg0, 0x30000) == 0) {
        s32               idx;
        s32*              pidx;
        register void**   scratch asm("v1");
        GpPickScratch*    block;
        register GpRec18* rec asm("s1");
        s32               i;
        s32               bestIdx;
        s32               dist;
        GpRec18*          picked;

        scratch = (void**)G_SCRATCH_HEAD;
        i       = 0;
        bestIdx = i;
        pidx    = &idx;
        rec     = arg0;
        {
            register void* p asm("v0");
            p        = *scratch;
            p        = (u8*)p - 0x68;
            block    = p;
            *scratch = p;
        }
        do {
            if (rec->field_4 & 0x100000) {
                s32 fy;
                s32 dy;
                {
                    register s32 dx asm("v0");
                    dx   = arg1->workm.t[0] - rec->field_8;
                    fy   = rec->field_A;
                    dist = dx;
                    if (dx < 0) {
                        dist = -dist;
                    }
                }
                {
                    register s32 t2 asm("v0");
                    register s32 fz asm("a0");
                    dy = arg1->workm.t[1] - fy;
                    t2 = arg1->workm.t[2];
                    if (dy < 0) {
                        dy = -dy;
                    }
                    fz    = rec->field_C;
                    dist += dy;
                    t2    = t2 - fz;
                    TOUCH_REG2(t2, dist);
                    if (t2 < 0) {
                        t2 = -t2;
                    }
                    dist += t2;
                }
                if (dist < minDist) {
                    func_800E0FEC(rec, (GpDeltaScratch*)block, 1, pidx);
                    idx = func_800E1ACC((u8*)pidx);
                    {
                        GameSession* session = Game_Session;
                        if (Gp_RoomParamTables[session->field_7 - 1][session->field_6 - 1][idx]->field_2 != 0) {
                            minDist = dist;
                            bestIdx = i;
                        }
                    }
                }
            }
            i++;
            rec++;
        } while (i < 6);
        if (minDist != 0x7FFFFFFF) {
            i                = 1;
            picked           = (GpRec18*)(bestIdx * 0x18 + (s32)arg0);
            block->sub       = 0;
            block->flg       = i;
            block->t[0]      = picked->field_8;
            block->t[1]      = picked->field_A;
            block->t[2]      = picked->field_C;
            block->offset.vx = rand() & 7;
            block->offset.vy = rand() & 7;
            block->offset.vz = rand() & 7;
            if (arg2 != 0) {
                arg2->workm.t[0] = block->t[0] + block->offset.vx;
                arg2->workm.t[1] = block->t[1] + block->offset.vy;
                arg2->workm.t[2] = block->t[2] + block->offset.vz;
            }
            if (Wip_SysConfig.field_21 != 0x1D) {
                if (Wip_SysConfig.field_22 == 0xE) {
                    GsCOORDINATE2* coord;
                    SVECTOR*       vec;
                    coord = (GsCOORDINATE2*)&block->flg;
                    vec   = &block->offset;
                    Gp_SpawnEff(0x6008D, coord, 0x300, vec);
                    Gp_SpawnEff(0x60080, coord, 0x300, vec);
                    Gp_SpawnEff(0x60070, coord, 0xC0013300, vec);
                } else {
                    Gp_SpawnEff(0x6003B, (GsCOORDINATE2*)&block->flg, 0, &block->offset);
                }
            }
        } else {
            i = 0;
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x68;
        return i;
    }
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105ED4);

s32 func_801060E0(GpActorWork* arg0)
{
    GameActor* actor;
    u16        mode;
    s32        flags;
    s32        mask1;
    s32        mask2;

    actor = arg0->actor;
    mode  = actor->field_954;
    if (mode != 2) {
        flags = actor->field_962;
        mask1 = 8;
        mask2 = 2;
    } else {
        flags = ((PadState*)&Pad_States)->buttons;
        if (Mc_SaveData.field_1a8 == mode) {
            mask1 = 0x80;
            mask2 = 0x10;
        } else {
            mask1 = 8;
            mask2 = 2;
        }
    }
    actor->field_97F = 0;
    if (flags & mask1) {
        actor->field_97F = 1;
    } else if (flags & mask2) {
        actor->field_97F = 2;
    }
    return actor->field_97F;
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_800978BC\n"
        "D_800978BC:\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011D1D8\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1D8\n"
        "\t.word 0x8011DDA0\n"
        "\t.word func_801065A0\n"
        "\t.word func_801065A0\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011D1D8\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011D1D4\n"
        "\t.word 0x8011D1D4\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1DC\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011D1DC\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011DBFC\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011D1C4\n"
        "\t.word 0x8011F724\n"
        "\t.word 0x8011E040\n"
        "\t.word func_801065A0\n"
        "\t.word 0x8011E710\n"
        "\t.word 0x8011DA34\n"
        "\t.word 0x8011D1EC\n"
        "\t.word 0x8011E4F8\n"
        "\t.word 0x8011F5D4\n"
        "\t.word 0x8011DDA4\n"
        "\t.word 0x8011DDA4\n"
        "\t.word 0x8011DDA4\n"
        ".section .text\n");
#endif

void func_8010615C(GpActorWork* arg0)
{
    GameActor*         actor;
    GpActorFuncTable33 sp;

    sp               = D_800978BC;
    actor            = arg0->actor;
    actor->field_96A = 0xF89A;
    actor->field_973 = 0;
    sp.funcs[Wip_SysConfig.field_21](arg0);
}

void func_801061F0(void)
{
    WipSysConfig* p;
    GpActorWork*  work;
    s32           flag;
    register s32  f21 asm("a1");
    s32           f22;

    work                   = Game_GetPtrSlot(3);
    p                      = &Wip_SysConfig;
    flag                   = 0x20000;
    f21                    = p->field_21;
    f22                    = p->field_22;
    work->actor->field_124 = (f21 << 8) | (f22 | flag);
}

void func_80106238(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_124 = (actor->field_124 & 0xFFFF3FFF) | (((arg1 << 1) | arg2) << 14);
}

s32 func_80106264(s32 arg0)
{
    s32 item;
    s32 ret;

    item = Wip_SysConfig.field_21 + 0x7F;
    ret  = 0;
    if (arg0 & 1) {
        ret = Gp_ConsumeSlotQty(item, 0);
    }
    if (arg0 & 2) {
        ret |= Gp_ConsumeSlotQty(item, 0x100) << 16;
    }
    return ret;
}

s32 func_801062DC(GpActorWork* arg0, s32 arg1)
{
    s32 ret;
    s32 flag;
    s32 item;

    ret  = 0;
    item = Wip_SysConfig.field_21;
    flag = arg1 != 1;
    if (Gp_UnequipRelated(item + 0x7F, flag) == 1) {
        func_801088D4(arg0, flag, ret);
        ret = 1;
    }
    return ret;
}

void func_80106350(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        value;

    actor = arg0->actor;
    if (arg1 == 0x16) {
        if (actor->field_914 != NULL) {
            actor->field_914->spawnArg1 = -1;
        }
        SndEvt_EnqueueType7(0x20160003, 0);
        SndEvt_EnqueueType7(0x20160004, 0);
        SndEvt_EnqueueType7(0x20160005, 0);
    } else if (arg1 == 0x19) {
        if (actor->field_914 != NULL) {
            if (Gp_ConsumeSlotQty(0x98, 0x100) != 0) {
                actor->field_914->spawnArg1 = 1;
            } else {
                actor->field_914->spawnArg1 = 0;
            }
        }
    } else if (arg1 == 0x1C) {
        if (actor->field_914 != NULL) {
            if (Gp_ConsumeSlotQty(0x9B, 0x100) != 0) {
                value = 1;
                if (actor->field_914->spawnArg1 == 2) {
                    value = 3;
                }
                actor->field_914->spawnArg1 = value;
            } else {
                actor->field_914->spawnArg1 = (actor->field_914->spawnArg1 == 2) << 2;
            }
            if (actor->field_910 == NULL) {
                SndEvt_EnqueueType7(0x201C0005, 0);
            } else {
                SndEvt_EnqueueType7(0x40680002, 0);
            }
        }
    }
    actor->field_97E  = 1;
    actor->field_12A &= 0x3FFF;
}

void Gp_PlayObjSfx(GpObj38* arg0, s32 arg1, s32 arg2)
{
    s32 temp;

    temp = (s8)Gp_GetObjPan(arg0);
    SndEvt_EnqueueType6(arg1, temp, (s8)Gp_GetObjDepth(arg0));
    if (arg2 == 1) {
        Gp_SetStateF0Bit(1);
    }
}

void func_80106518(s32 arg0)
{
    register s32 cap asm("v0");
    s32          idx;

    cap = 0x1869E;
    idx = arg0 - 1;
    if (Mc_SaveData.field_888[idx] <= cap) {
        Mc_SaveData.field_888[idx]++;
    }
}

void func_80106550(GpActorWork* arg0)
{
    if (arg0->actor->field_97D & 4) {
        func_801055D4(arg0, 0, 0, 0);
    } else {
        Gp_ResetActorAnimState(arg0, 3);
    }
}

void func_801065A0(void)
{
}

void func_801065A8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        inner->field_97C = 1;
        func_8010870C(arg0, 5);
    } else if (inner->field_973 != inner->field_974 ||
               (inner->field_977 != inner->field_978 && inner->field_973 == 1)) {
        func_801066DC(arg0, 0);
    } else if (inner->field_973 == 0 && inner->field_975 != inner->field_976) {
        func_80108620(arg0);
    } else if ((inner->field_962 & 0xF000) == 0) {
        if (inner->field_942 < 0x7FFF) {
            inner->field_942++;
            if (inner->field_942 == 0x12C) {
                Gp_AnimPlayChildSlotsEx(arg0, Gp_HpBand() + 0x17, 0, 5);
            }
        }
    }
}

void func_801066DC(GpActorWork* arg0, s16 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    temp             = inner->field_973;
    inner->field_956 = 0;
    inner->field_95C = 0;
    if (temp == 0) {
        if (inner->field_975 != 0) {
            if (inner->field_975 == 1) {
                mode = 6;
            } else {
                mode = 5;
            }
        } else {
            mode = 1;
        }
        inner->field_958 = 0;
        inner->field_95A = 3;
    } else if ((inner->field_962 & 0x40) && (temp != -1)) {
        temp             = 1;
        inner->field_95A = temp;
        if (Mc_SaveData.field_25 == 0 && inner->field_991 == 0) {
            inner->field_958 = 3;
            mode             = 4;
        } else {
            mode             = 2;
            inner->field_958 = temp;
            if (inner->field_91C == NULL) {
                mode = 0x13;
            }
        }
    } else {
        inner->field_95A = 1;
        if (inner->field_973 == 1) {
            if (Mc_SaveData.field_25 != 0 && inner->field_991 == 0) {
                inner->field_958 = 3;
                mode             = 4;
            } else {
                inner->field_958 = 1;
                mode             = 2;
                if (inner->field_91C == NULL) {
                    mode = 0x13;
                }
            }
        } else {
            inner->field_958 = 2;
            mode             = 3;
        }
    }
    inner->field_954 = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    if (arg1 != 0) {
        Gp_AnimResetChildSlots(arg0, mode);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 4);
    }
}

INCLUDE_RODATA("gameplay/nonmatchings/3FB8", Gp_PlayerModeFns);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_8009794C\n"
        "D_8009794C:\n"
        "\t.word func_80108FA0\n"
        "\t.word Gp_PlayerNormalState1\n"
        "\t.word Gp_PlayerNormalState2\n"
        "\t.word func_801090E8\n"
        "\t.word func_80109138\n"
        "\t.word Gp_PlayerNormalState5\n"
        "\t.word Gp_PlayerNormalState6\n"
        "\t.word func_8010771C\n"
        "\t.word 0x00000000\n"
        ".section .text\n");
#endif

void Gp_TickPlayerNormal(GpActorWork* arg0)
{
    GameActor*        actor;
    GameActor*        inner;
    WipSysConfig*     p;
    u16               prev;
    GpActorFuncTable8 sp;

    sp    = D_8009794C;
    actor = arg0->actor;
    if (Wip_SysConfig.field_25 & 0x40) {
        func_8010A670(arg0);
    }
    if (actor->field_97B == 0) {
        func_80109250(arg0);
        func_80109210(arg0);
    } else {
        actor->field_973 = 0;
        actor->field_975 = 0;
    }
    p = &Wip_SysConfig;
    if (p->field_25 & 2) {
        if (actor->field_956 != 7) {
            actor->field_98E++;
            if ((s8)actor->field_98E >= 0x5A) {
                inner            = arg0->actor;
                prev             = inner->field_956;
                inner->field_954 = 0;
                inner->field_956 = 7;
                inner->field_958 = 0;
                inner->field_95A = 0;
                inner->field_95C = 0;
                inner->field_95E = 0;
                inner->field_981 = 0;
                inner->field_973 = 0;
                inner->field_975 = 0;
                inner->field_960 = prev;
                Gp_DetachLinkNode(arg0);
                inner->field_12A    &= 0x3FFF;
                Gp_StateC08.field_6 |= 1;
                func_80106350(arg0, p->field_21, 0);
                Gp_AnimPlayChildSlotsEx(arg0, 0x19, 3, 6);
            }
        }
    }
    sp.funcs[actor->field_956](arg0);
    func_80109FC4(arg0);
    Gp_PlayerStepSfx(arg0);
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (Wip_SysConfig.field_18 <= 0) {
        Gp_StopPlayerAnim(arg0, 4);
    }
}

void Gp_PlayerNormalState2(GpActorWork* arg0)
{
    GameActor* actor;
    s32        dir;
    s32        res;
    u8         item;
    u16        pad;
    s32        variant;
    s32        val;
    s32        base;

    actor = arg0->actor;
    if (actor->field_973 != actor->field_974) {
        Gp_ResetActorAnimState(arg0, 4);
    } else if (actor->field_973 == 0 && actor->field_975 != actor->field_976) {
        func_80108684(arg0);
    }
    if (func_80109290(arg0) == 0) {
        if ((actor->field_962 & 0xF000) == 0) {
            Gp_TrackLockTarget(arg0);
        }
        Gp_UpdateLockTarget(arg0);
        if ((s8)func_801060E0(arg0) != 0 &&
            Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) != NULL &&
            actor->field_940 == 0) {
            dir = D_80112EF8[Wip_SysConfig.field_21] != 0 ? actor->field_97F : 1;
            res = func_80106264(dir);
            if (res > 0 ||
                (item = actor->field_97F,
                 D_80112F1C[Wip_SysConfig.field_21][(u8)(item - 1)] != 0)) {
                if (Wip_SysConfig.field_25 & 0x80) {
                    Gp_ApplyHpDamage(2);
                }
                if (Wip_SysConfig.field_18 > 0) {
                    actor->field_97D = 1;
                    actor->field_95E = 0;
                    func_8010615C(arg0);
                }
            } else if (res == 0) {
                pad = actor->field_966;
                if ((s8)item == 1 ? (pad & 8) : (pad & 2)) {
                    actor->field_940 = 0xA;
                    if (func_801062DC(arg0, dir) == 0) {
                        func_801095BC(&variant);
                        base = Wip_SysConfig.field_21 << 16;
                        val  = variant | 0x20000001;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | val, 0);
                    }
                }
            }
        }
    }
    func_80105ED4(arg0);
}

void Gp_PlayerNormalState5(GpActorWork* arg0)
{
    GameActor*     actor;
    GameActor*     inner;
    GpAnimRec*     rec;
    GsCOORDINATE2* coord;
    s32            base;
    s32            done;
    s32            mode;
    s32            temp;
    s32            flags;
    s32            tick;
    s32            step;
    u8             item;
    u16            next;
    s32            variant;

    actor            = arg0->actor;
    done             = 0;
    coord            = (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8;
    base             = Wip_SysConfig.field_21 << 16;
    actor->field_973 = 0;
    func_801095BC(&variant);
    if (actor->field_93E != 2 && (actor->field_966 & 0x40) && actor->field_95E != 0x64) {
        actor->field_98F = 1;
        inner            = arg0->actor;
        inner->field_954 = 0;
        inner->field_956 = 2;
        inner->field_958 = 0;
        if (inner->field_973 != 0) {
            temp = 1;
        } else {
            temp = 3;
        }
        inner->field_95A = temp;
        inner->field_95C = 0;
        inner->field_95E = 0;
        if (Wip_SysConfig.field_25 & 1) {
            Gp_DetachLinkNode(arg0);
            inner->field_97E = 1;
        } else {
            inner->field_97E = 2;
        }
        temp = inner->field_973;
        if (temp == 0) {
            if (inner->field_975 != 0) {
                mode = 0xD;
            } else {
                mode = 9;
            }
        } else if (temp == 1) {
            mode             = 0xC;
            inner->field_958 = 3;
            inner->field_97E = temp;
        } else {
            inner->field_958 = 2;
            mode             = 0xD;
        }
        Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 6);
        return;
    }

    switch (Wip_SysConfig.field_21) {
        case 3:
        case 17:
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    if (actor->field_95E == 0) {
                        actor->field_95E = 1;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000002, 0);
                    } else {
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000003, 0);
                        done             = 1;
                        actor->field_95E = 0x64;
                    }
                }
            }
            break;
        case 9:
            switch (actor->field_95E) {
                case 0:
                    actor->field_95E = 1;
                    actor->field_934 = 0xA;
                    /* fallthrough */
                case 1:
                    tick             = actor->field_934 - 1;
                    actor->field_934 = tick;
                    if (tick == -1) {
                        actor->field_95E += 1;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000002, 0);
                        if (actor->field_98F == 0) {
                            Gp_SpawnEff(0x6006D, coord, 0, NULL);
                        }
                    }
                    break;
                case 2:
                case 0x64:
                    rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424,
                                        (GpAnimSlot*)actor->field_438 + 1);
                    if (rec != NULL && rec != actor->field_92C) {
                        actor->field_92C = rec;
                        if ((rec->field_3 & 0x30) == 0x30) {
                            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000003, 0);
                            done             = 1;
                            actor->field_98F = 0;
                            actor->field_95E = 0x64;
                        }
                    }
                    break;
            }
            break;
        case 11:
            if (actor->field_95E == 0) {
                actor->field_95E = 1;
                flags            = 0x20000002;
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
            } else {
                rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424,
                                    (GpAnimSlot*)actor->field_438 + 1);
                if (rec != NULL && rec != actor->field_92C) {
                    actor->field_92C = rec;
                    if ((rec->field_3 & 0x30) == 0x30) {
                        flags = 0x20000003;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                        done             = 1;
                        actor->field_95E = 0x64;
                    }
                }
            }
            break;
        case 12:
            if (actor->field_95E == 0) {
                temp             = actor->field_98F;
                actor->field_95E = 1;
                if (temp == 0) {
                    Gp_SpawnEff(0x6006E, coord, Wip_SysConfig.field_21, NULL);
                }
            }
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    flags = 0x20000003;
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                    done             = 1;
                    actor->field_98F = 0;
                    actor->field_95E = 0x64;
                }
            }
            break;
        case 13:
        case 14:
        case 23:
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    if (actor->field_95E == 0) {
                        flags = 0x20000003;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                        done             = 1;
                        actor->field_95E = 0x64;
                    } else {
                        flags = 0x20000002;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                    }
                }
            }
            break;
        case 15:
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    switch (actor->field_95E) {
                        case 0:
                            actor->field_95E = 1;
                            flags            = 0x20000003;
                            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                            break;
                        case 1:
                            flags = 0x20000003;
                            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                            done             = 1;
                            actor->field_95E = 0x64;
                            break;
                        case 0x64:
                            flags = 0x20000002;
                            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | (variant | flags), 0);
                            break;
                    }
                }
            }
            break;
        case 16:
        case 20:
        case 21:
        case 25:
        case 26:
        case 28:
        case 29:
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    if (actor->field_95E == 0) {
                        actor->field_95E = 1;
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000003, 0);
                    } else {
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000003, 0);
                        done             = 1;
                        actor->field_95E = 0x64;
                    }
                }
            }
            break;
        case 19:
            break;
        case 27:
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    item = Gp_GetItemSlot(Wip_SysConfig.field_21 + 0x7F)->field_2;
                    if (item - 0x9F > 0) {
                        variant = ((item - 0xA0) % 3) << 24;
                    }
                    variant = base | variant;
                    if (actor->field_960 != 0) {
                        step = actor->field_95E;
                        if (step == 0) {
                            variant |= 0x20000008;
                        } else if (step == 1) {
                            variant |= 0x20000005;
                        } else if (step == 0x64) {
                            variant |= 0x20000009;
                        }
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, variant, 0);
                    } else {
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, variant | 0x20000003, 0);
                    }
                    next             = actor->field_95E + 1;
                    actor->field_95E = next;
                    if (next == 2) {
                        done             = 1;
                        actor->field_95E = 0x64;
                    }
                }
            }
            break;
        default:
            if (actor->field_95E == 0) {
                actor->field_95E = 1;
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000002, 0);
            } else {
                rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424,
                                    (GpAnimSlot*)actor->field_438 + 1);
                if (rec != NULL && rec != actor->field_92C) {
                    actor->field_92C = rec;
                    if ((rec->field_3 & 0x30) == 0x30) {
                        Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, base | 0x20000003, 0);
                        done             = 1;
                        actor->field_95E = 0x64;
                    }
                }
            }
            break;
    }
    if (done != 0) {
        if (actor->field_93E != 0) {
            Gp_FlushPendingRelated(Wip_SysConfig.field_21 + 0x7F, actor->field_960);
        } else {
            Gp_FillRelated(Wip_SysConfig.field_21 + 0x7F, actor->field_960);
        }
    }
    Gp_UpdateLockTarget(arg0);
}

void Gp_PlayerNormalState6(GpActorWork* arg0)
{
    GpActorWork* work;
    s32          fade;
    register s32 a3 asm("a3");
    s32          next;
    s32          kind;
    GameActor*   actor;
    GameActor*   inner;
    s32          mode;
    s32          temp;
    s32          flag;
    s32          snd;
    s32          val;

    work             = arg0;
    actor            = work->actor;
    actor->field_973 = 0;
    if (Gp_StateC08.field_3 == 2) {
        actor->field_95E = 5;
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_95C     = 9;
            actor->field_95E    += 1;
            Gp_StateC08.field_6 |= 4;
            mode                 = 0x1A;
            if (actor->field_93C != 0) {
                mode = 0x2A;
                if (actor->field_93C == 1) {
                    mode = 0x1D;
                }
            }
            Gp_AnimPlayChildSlotsEx(work, mode, 0, 6);
            goto do_db500_2;
        case 2:
            next             = actor->field_95E;
            kind             = actor->field_93C;
            actor->field_95C = 0;
            actor->field_95E = next + 1;
            COMPILER_BARRIER();
            mode = 0x1B;
            if (kind != 0) {
                mode = 0x2B;
                if (kind == 1) {
                    mode = 0x1E;
                }
            }
            Gp_AnimResetChildSlots(work, mode);
        case 3:
            if (Gp_StateC08.field_2 != 0) {
                goto do_db500_2;
            }
            actor->field_95C  = 9;
            actor->field_95E += 1;
            snd               = 4;
            if ((u32)(Gp_StateC08.field_0 - 0x12C) >= 0x12DU) {
                snd = 3;
            }
            Gp_SetStateF0Bit(snd);
            mode = 0x1C;
            if (actor->field_93C != 0) {
                mode = 0x2C;
                if (actor->field_93C == 1) {
                    mode = 0x1F;
                }
            }
            Gp_AnimResetChildSlots(work, mode);
            break;
        case 1:
        do_db500_2:
            Gp_SetStateF0Bit(2);
            break;
        case 4:
            break;
        case 5:
            flag = 1;
            if (actor->field_960 == 0) {
                func_801066DC(work, 0);
                break;
            }
            a3 = 8;
            if (actor->field_960 == flag) {
                a3 = 6;
            }
            temp             = 3;
            inner            = work->actor;
            val              = 2;
            fade             = a3;
            inner->field_954 = 0;
            inner->field_956 = val;
            inner->field_958 = 0;
            if (inner->field_973 != 0) {
                temp = 1;
            }
            inner->field_95A = temp;
            inner->field_95C = 0;
            inner->field_95E = 0;
            if (Wip_SysConfig.field_25 & 1) {
                Gp_DetachLinkNode(work);
                inner->field_97E = flag;
            } else {
                inner->field_97E = val;
            }
            temp = inner->field_973;
            if (temp == 0) {
                if (inner->field_975 != 0) {
                    mode = 0xD;
                } else {
                    mode = 9;
                }
            } else if (temp == 1) {
                mode             = 0xC;
                inner->field_958 = 3;
                inner->field_97E = temp;
            } else {
                inner->field_958 = 2;
                mode             = 0xD;
            }
            if (fade == 0) {
                Gp_AnimResetChildSlots(work, mode);
            } else {
                Gp_AnimPlayChildSlotsEx(work, mode, 0, fade);
            }
            break;
    }
}

void func_8010771C(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor            = arg0->actor;
    actor->field_973 = 0;
    if (!(Wip_SysConfig.field_25 & 2)) {
        actor->field_95E = 1;
        actor->field_98E = 0;
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            actor->field_98E = 0xF;
        case 1:
            if (actor->field_966 & 0xF0F0) {
                actor->field_98E--;
            }
            if ((s8)actor->field_98E > 0) {
                break;
            }
            if (actor->field_960 == 0) {
                func_801066DC(arg0, 0);
                break;
            }
            inner            = arg0->actor;
            inner->field_954 = 0;
            inner->field_956 = 2;
            inner->field_958 = 0;
            if (inner->field_973 != 0) {
                temp = 1;
            } else {
                temp = 3;
            }
            inner->field_95A = temp;
            inner->field_95C = 0;
            inner->field_95E = 0;
            if (Wip_SysConfig.field_25 & 1) {
                Gp_DetachLinkNode(arg0);
                inner->field_97E = 1;
            } else {
                inner->field_97E = 2;
            }
            temp = inner->field_973;
            if (temp == 0) {
                if (inner->field_975 != 0) {
                    mode = 0xD;
                } else {
                    mode = 9;
                }
            } else if (temp == 1) {
                mode             = 0xC;
                inner->field_958 = 3;
                inner->field_97E = temp;
            } else {
                inner->field_958 = 2;
                mode             = 0xD;
            }
            Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 6);
            break;
    }
}

void Gp_PlayerMode2State3(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    GpDashScratch* blk;
    GpDashScratch* vel;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            delay;
    s32            mode;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (GpDashScratch*)(head - 0x2C);
    *scratch = blk;
    vel      = blk;
    actor    = arg0->actor;
    coord    = (GsCOORDINATE2*)arg0->extra->field_8;
    switch (actor->field_95E) {
        case 0:
            blk->mtx = coord->coord;
            angle    = -0x180;
            if (actor->field_80 == 0) {
                angle = 0x180;
            }
            Gfx_RotMatrixX(&blk->mtx, angle, 0);
            Gfx_MatrixCol2(&blk->mtx, (SVECTOR*)(head - 0xC));
            VectorNormalSS((SVECTOR*)(head - 0xC), (SVECTOR*)(head - 0xC));
            if (actor->field_80 == 0) {
                actor->field_95E = 1;
                actor->field_934 = 0;
                actor->field_93E = actor->field_82 & 1;
                blk->div         = 0x6E;
            } else {
                actor->field_95E = 3;
                actor->field_934 = 5;
                blk->div         = 0x64;
            }
            actor->field_0 = vel->dir.vx / vel->div;
            actor->field_4 = vel->dir.vy / vel->div;
            actor->field_8 = vel->dir.vz / vel->div;
            break;
        case 1:
            if (func_80105ED4(arg0) != 0) {
                delay = 0xA;
                if (actor->field_82 == 1) {
                    delay = 0xB;
                }
                actor->field_934 = delay;
            } else if (actor->field_934 > 0) {
                actor->field_934--;
                if (actor->field_934 == 0) {
                    actor->field_82--;
                    if (actor->field_82 <= 0) {
                        actor->field_934 = 8;
                        actor->field_95E++;
                        Gfx_MatrixCol2(&coord->coord, (SVECTOR*)(head - 0xC));
                        VectorNormalSS((SVECTOR*)(head - 0xC), (SVECTOR*)(head - 0xC));
                        actor->field_0 = (s16)(blk->dir.vx / 180);
                        actor->field_4 = (s16)(blk->dir.vy / 180);
                        mode           = 0x26;
                        actor->field_8 = (s16)(blk->dir.vz / 180);
                        if (actor->field_93E != 0) {
                            mode = 0x27;
                        }
                        Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 3);
                    }
                }
                coord->coord.t[0] += actor->field_0;
                coord->coord.t[1] += actor->field_4;
                coord->coord.t[2] += actor->field_8;
            }
            break;
        case 2:
            func_80105ED4(arg0);
            if (actor->field_934 > 0) {
                actor->field_934--;
                coord->coord.t[0] += actor->field_0;
                coord->coord.t[1] += actor->field_4;
                coord->coord.t[2] += actor->field_8;
            }
            if (func_8010583C(arg0, 0, 0, 0) == 0) {
            block_land:
                actor->field_982 = 0;
                actor->field_956 = 1;
                Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 5);
            }
            break;
        case 3:
            if (func_80105ED4(arg0) != 0) {
                if (actor->field_82 == 1) {
                    Gfx_MatrixCol2(&coord->coord, (SVECTOR*)(head - 0xC));
                    VectorNormalSS((SVECTOR*)(head - 0xC), (SVECTOR*)(head - 0xC));
                    actor->field_0 = (s16)(blk->dir.vx / 58);
                    actor->field_8 = (s16)(blk->dir.vz / 58);
                }
                delay = 9;
                if (actor->field_82 == 1) {
                    delay = 5;
                }
                actor->field_934 = delay;
                actor->field_82--;
            } else if (actor->field_934 != 0) {
                actor->field_934--;
                coord->coord.t[0] += actor->field_0;
                coord->coord.t[1] += actor->field_4;
                coord->coord.t[2] += actor->field_8;
            } else if (actor->field_82 == 0) {
                goto block_land;
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

void Gp_PlayerMode2State4(GpActorWork* arg0)
{
    register u8*       tmp asm("a0");
    register s32       dx asm("v0");
    void**             scratch;
    u8*                head;
    TmdObject*         extra;
    GpApproachScratch* block;
    GsCOORDINATE2*     coord;
    GameActor*         actor;
    s32                angle;
    s32                val;
    s32                mode;

    scratch                                      = (void**)G_SCRATCH_HEAD;
    head                                         = *scratch;
    extra                                        = arg0->extra;
    actor                                        = arg0->actor;
    tmp                                          = head - 0x14;
    coord                                        = (GsCOORDINATE2*)extra->field_8;
    block                                        = (GpApproachScratch*)tmp;
    block->vec.vx                                = actor->field_20 - coord->coord.t[0];
    *scratch                                     = block;
    block->vec.vy                                = actor->field_24 - coord->coord.t[1];
    block->vec.vz                                = actor->field_28 - coord->coord.t[2];
    angle                                        = ratan2(block->vec.vx, block->vec.vz);
    actor->field_82                              = angle;
    val                                          = func_80103E7C(actor->field_52, angle);
    ((GpApproachScratch*)(head - 0x14))->field_0 = val;
    if (val >= 0x41) {
        ((GpApproachScratch*)(head - 0x14))->field_0 = 0x40;
    } else if (val < -0x40) {
        ((GpApproachScratch*)(head - 0x14))->field_0 = -0x40;
    } else if (actor->field_95E == 0) {
        actor->field_95E = 1;
    }
    actor->field_52 = ((u16)actor->field_52 + (u16)block->field_0) & 0xFFF;
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            mode             = 6;
            if (block->field_0 < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->field_0 == 0) {
                actor->field_958 = 1;
                actor->field_95E++;
                if (actor->field_93C == 0) {
                    mode = 2;
                    if (actor->field_91C == NULL) {
                        mode = 0x13;
                    }
                } else {
                    mode = actor->field_93C;
                }
                Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
            }
            break;
        case 2:
            dx  = coord->coord.t[0];
            dx -= actor->field_20;
            if (dx < 0) {
                dx = -dx;
            }
            if (dx < 0x69) {
                dx  = coord->coord.t[2];
                dx -= actor->field_28;
                if (dx < 0) {
                    dx = -dx;
                }
                if (dx < 0x69) {
                    actor->field_982 = 0;
                    actor->field_956 = 1;
                    mode             = 1;
                    if (actor->field_93E != 0) {
                        mode = actor->field_93E;
                    }
                    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
                } else {
                    dx               = 1;
                    actor->field_973 = dx;
                    Gp_StepPlayerMove(arg0);
                    func_80105ED4(arg0);
                }
            } else {
                dx               = 1;
                actor->field_973 = dx;
                Gp_StepPlayerMove(arg0);
                func_80105ED4(arg0);
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", Gp_PlayerMode2StateA);

void Gp_PlayerMode2StateB(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor = arg0->actor;
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            Gp_AnimPlayChildSlotsEx(arg0, 0x28, 0, 6);
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) !=
                NULL) {
                if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) {
                    inner            = arg0->actor;
                    inner->field_954 = 0;
                    inner->field_956 = 2;
                    inner->field_958 = 0;
                    if (inner->field_973 != 0) {
                        temp = 1;
                    } else {
                        temp = 3;
                    }
                    inner->field_95A = temp;
                    inner->field_95C = 0;
                    inner->field_95E = 0;
                    if (Wip_SysConfig.field_25 & 1) {
                        Gp_DetachLinkNode(arg0);
                        inner->field_97E = 1;
                    } else {
                        inner->field_97E = 2;
                    }
                    temp = inner->field_973;
                    if (temp == 0) {
                        if (inner->field_975 != 0) {
                            mode = 0xD;
                        } else {
                            mode = 9;
                        }
                    } else if (temp == 1) {
                        mode             = 0xC;
                        inner->field_958 = 3;
                        inner->field_97E = temp;
                    } else {
                        inner->field_958 = 2;
                        mode             = 0xD;
                    }
                    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 4);
                }
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    Gp_PlayerStepSfx(arg0);
}

void Gp_TickPlayerActor(GpActorWork* arg0)
{
    GameActor*        inner;
    GpActorFuncTable3 sp;

    sp    = Gp_PlayerModeFns;
    inner = arg0->actor;
    func_80104A4C(arg0);
    if (inner->field_940 > 0) {
        inner->field_940--;
    }
    if ((s8)inner->field_97A > 0) {
        inner->field_97A--;
    }
    inner->field_986 = 0;
    sp.funcs[inner->field_954](arg0);
    func_80109720(arg0);
    func_801030CC(arg0);
}

void Gp_ArmLockOnState(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         flag;

    inner            = arg0->actor;
    node             = Gp_FindLockNode(arg0);
    inner->field_973 = 0;
    if ((node != NULL && Gp_StateF0.field_0 < 2) || (flag = 1, Gp_StateF0.field_0 == flag) ||
        Mc_SaveData.field_929 != 0) {
        if (inner->field_95E != 0) {
            Gp_ArmStateF0(1);
            if (inner->field_97C != 0) {
                inner->field_97C = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            Gp_ResetActorAnimState(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->field_97D & 2) {
            inner->field_97C = 0;
            inner->field_97E = flag;
            Gp_DetachLinkNode(arg0);
            func_80108874(arg0);
        }
    }
}

void func_80108568(GpActorWork* arg0)
{
    GameActor* actor;

    actor = arg0->actor;
    if (actor->field_973 != actor->field_974) {
        Gp_ResetActorAnimState(arg0, 4);
    } else if (actor->field_973 == 0) {
        if (actor->field_975 != actor->field_976) {
            func_80108684(arg0);
        }
    }
}

void func_801085D0(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_973 = 0;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        func_8010870C(arg0, 4);
    }
}

void func_80108620(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 3;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    if (inner->field_975 == 0) {
        mode = 1;
    } else if (inner->field_975 == 1) {
        mode = 6;
    } else {
        mode = 5;
    }
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
}

void func_80108684(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    if (inner->field_973 != 0) {
        if (inner->field_973 == 1) {
            temp = 3;
        } else {
            temp = 2;
        }
        mode             = 0xD;
        inner->field_958 = temp;
        inner->field_95A = 1;
        if (inner->field_973 == 1) {
            mode = 0xC;
        }
    } else {
        mode             = 0xD;
        inner->field_95A = 3;
        if (inner->field_975 == 0) {
            mode = 9;
        }
    }
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
}

void func_8010870C(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_956 = 1;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 2;
    inner->field_95E = 0;
    inner->field_940 = 0;
    if (arg1 == 0) {
        Gp_AnimResetChildSlots(arg0, 7);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, 7, 0, arg1);
    }
}

void Gp_ResetActorAnimState(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 2;
    inner->field_958 = 0;
    if (inner->field_973 != 0) {
        temp = 1;
    } else {
        temp = 3;
    }
    inner->field_95A = temp;
    inner->field_95C = 0;
    inner->field_95E = 0;
    if (Wip_SysConfig.field_25 & 1) {
        Gp_DetachLinkNode(arg0);
        inner->field_97E = 1;
    } else {
        inner->field_97E = 2;
    }
    temp = inner->field_973;
    if (temp == 0) {
        if (inner->field_975 != 0) {
            mode = 0xD;
        } else {
            mode = 9;
        }
    } else if (temp == 1) {
        mode             = 0xC;
        inner->field_958 = 3;
        inner->field_97E = temp;
    } else {
        inner->field_958 = 2;
        mode             = 0xD;
    }
    if (arg1 == 0) {
        Gp_AnimResetChildSlots(arg0, mode);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, mode, 0, arg1);
    }
}

void func_80108874(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_956 = 3;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 2;
    inner->field_95C = 4;
    inner->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 8, 0, 6);
    Gp_DetachLinkNode(arg0);
}

void func_801088D4(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    if (arg2 == 2) {
        if (func_80106264(arg1) != 0) {
            if (D_80112F1C[Wip_SysConfig.field_21][0] == 0) {
                inner->field_95E = 0x3E8;
                return;
            }
        }
        inner->field_95C = 0xA;
        mode             = 0x14;
        if (Mc_SaveData.field_13 == 1) {
            func_80166E94(Game_GetPtrSlot(0xA), 0);
        }
    } else {
        if (arg2 == 1) {
            if (inner->field_954 == 2) {
                return;
            }
        }
        inner->field_95C = 5;
        mode             = arg1 + 0xE;
    }
    inner->field_956 = 5;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95E = 0;
    inner->field_960 = arg1;
    inner->field_93E = arg2;
    func_80106350(arg0, Wip_SysConfig.field_21, 0);
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 3);
}

void func_80108A0C(GpActorWork* arg0)
{
    GameActor*   inner;
    u16          prev;
    u16          tens;
    register s32 temp asm("v0");

    inner            = arg0->actor;
    prev             = inner->field_956;
    inner->field_956 = 6;
    inner->field_97E = 1;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_973 = 0;
    inner->field_960 = prev;
    temp             = (Gp_StateC08.field_0 % 100U) / 10U;
    tens             = temp;
    if (Gp_StateC08.field_0 >= 0x259U) {
        if (tens == 1) {
            inner->field_93C = 0;
        } else {
            inner->field_93C = 1;
        }
    } else if (tens == 3) {
        inner->field_93C = 2;
    } else if (Gp_StateC08.field_0 < 0x12CU) {
        inner->field_93C = 1;
    } else {
        inner->field_93C = 0;
    }
}

void func_80108AD4(GpActorWork* arg0)
{
    GameActor* inner;
    u16        prev;

    inner            = arg0->actor;
    prev             = inner->field_956;
    inner->field_954 = 0;
    inner->field_956 = 7;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_981 = 0;
    inner->field_973 = 0;
    inner->field_975 = 0;
    inner->field_960 = prev;
    Gp_DetachLinkNode(arg0);
    inner->field_12A    &= 0x3FFF;
    Gp_StateC08.field_6 |= 1;
    func_80106350(arg0, Wip_SysConfig.field_21, 0);
    Gp_AnimPlayChildSlotsEx(arg0, 0x19, 3, 6);
}

void Gp_PlayerMode2State0(GpActorWork* arg0)
{
    func_80105B0C(arg0);
    func_80105ED4(arg0);
}

void Gp_PlayerMode2State1(GpActorWork* arg0)
{
    Gp_AnimTickChildSlots(arg0);
    func_80105ED4(arg0);
}

void Gp_PlayerMode2State2(GpActorWork* arg0)
{
    GameActor* inner;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    inner = arg0->actor;
    cur   = inner->field_52;
    tgt   = inner->field_82;
    raw   = inner->field_82;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x41 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x41)) {
        flag             = 1;
        inner->field_52  = raw;
        inner->field_982 = 0;
        inner->field_956 = flag;
        Gp_AnimPlayChildSlotsEx(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x40) {
            delta = 0x40;
        } else if (delta < -0x40) {
            delta = -0x40;
        }
        inner->field_52 = ((u16)inner->field_52 + delta) & 0xFFF;
    }
    Gp_AnimTickChildSlots(arg0);
}

void Gp_PlayerMode2State8(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
        case 1:
            Gp_PlayerMode2State4(arg0);
            if (inner->field_95E == 2) {
                inner->field_958 = 3;
                mode             = 4;
                if (inner->field_93C != 0) {
                    mode = inner->field_93C;
                }
                Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
            }
            break;
        case 2:
            Gp_PlayerMode2State4(arg0);
            break;
    }
}

void Gp_PlayerMode2State6(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (inner->field_93E >= inner->field_934) {
        inner->field_97A = 0x12;
        if (inner->field_95E == 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            inner->field_95E = 1;
        }
    } else if (inner->field_966 & 0xF0F0) {
        inner->field_93E++;
    }
    Gp_AnimTickChildSlots(arg0);
}

void func_80108E0C(GpActorWork* arg0, GpLinkNode* arg1)
{
    GameActor*  inner;
    GpLinkNode* node;

    inner = arg0->actor;
    node  = inner->field_90C;
    if (node != arg1) {
        if (node != NULL) {
            node->field_5 = 0;
        }
        inner->field_90C = arg1;
    }
    arg1->field_5 = 1;
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl Gp_PlayerMode1States\n"
        "Gp_PlayerMode1States:\n"
        "\t.word Gp_PlayerMode1State0\n"
        "\t.word Gp_PlayerMode1State0\n"
        "\t.word Gp_PlayerMode1State0\n"
        "\t.word Gp_PlayerMode1State3\n"
        ".section .text\n");
#endif

void Gp_TickPlayerMode1(GpActorWork* arg0)
{
    GpActorFuncTable4 sp;

    sp = Gp_PlayerMode1States;
    sp.funcs[(u16)arg0->actor->field_96C](arg0);
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl Gp_PlayerMode2States\n"
        "Gp_PlayerMode2States:\n"
        "\t.word Gp_PlayerMode2State0\n"
        "\t.word Gp_PlayerMode2State1\n"
        "\t.word Gp_PlayerMode2State2\n"
        "\t.word Gp_PlayerMode2State3\n"
        "\t.word Gp_PlayerMode2State4\n"
        "\t.word Gp_PlayerMode2State5\n"
        "\t.word Gp_PlayerMode2State6\n"
        "\t.word Gp_PlayerMode2State7\n"
        "\t.word Gp_PlayerMode2State8\n"
        "\t.word Gp_PlayerMode2State9\n"
        "\t.word Gp_PlayerMode2StateA\n"
        "\t.word Gp_PlayerMode2StateB\n"
        ".section .text\n");
#endif

void Gp_TickPlayerMode2(GpActorWork* arg0)
{
    GameActor*         inner;
    GpActorFuncTable12 sp;

    sp    = Gp_PlayerMode2States;
    inner = arg0->actor;
    sp.funcs[inner->field_956](arg0);
    Gp_TurnPlayer(arg0);
    if (Wip_SysConfig.field_18 <= 0 && inner->field_956 != 0xA) {
        Gp_BindActorAnim(arg0);
        Gp_StopPlayerAnim(arg0, 4);
    }
}

void func_80108FA0(GpActorWork* arg0)
{
    func_801065A8(arg0);
    func_80109290(arg0);
    func_80105ED4(arg0);
}

void Gp_PlayerNormalState1(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         flag;

    Gp_TrackLockTarget(arg0);
    inner            = arg0->actor;
    node             = Gp_FindLockNode(arg0);
    inner->field_973 = 0;
    if ((node != NULL && Gp_StateF0.field_0 < 2) || (flag = 1, Gp_StateF0.field_0 == flag) ||
        Mc_SaveData.field_929 != 0) {
        if (inner->field_95E != 0) {
            Gp_ArmStateF0(1);
            if (inner->field_97C != 0) {
                inner->field_97C = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            Gp_ResetActorAnimState(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->field_97D & 2) {
            inner->field_97C = 0;
            inner->field_97E = flag;
            Gp_DetachLinkNode(arg0);
            func_80108874(arg0);
        }
    }
}

void func_801090E8(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_973 = 0;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        func_8010870C(arg0, 4);
    }
}

void func_80109138(GpActorWork* arg0)
{
    func_8010615C(arg0);
    func_801041FC(arg0, 0);
    Gp_UpdateLockTarget(arg0);
}

void Gp_PlayerMode1State0(GpActorWork* arg0)
{
    GameActor* inner;
    u8         kind;

    inner = arg0->actor;
    kind  = inner->field_972;
    switch (kind) {
        case 0:
        case 1:
        case 2:
        case 8:
        case 9:
        case 10:
        case 11:
            func_8010ABD4(arg0);
            break;
        case 5:
            func_8010AC54(arg0);
            break;
        case 6:
            func_80109A1C(arg0);
            break;
        case 3:
            func_8010AD64(arg0);
            break;
        case 7:
            func_80109844(arg0);
            break;
    }
}

void Gp_PlayerMode1State3(void)
{
}

void func_80109210(GpActorWork* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->actor;
    flags = inner->field_962;
    if (flags & 0xA000) {
        if (flags & 0x8000) {
            inner->field_975 = -1;
        } else {
            inner->field_975 = 1;
        }
    } else {
        inner->field_975 = 0;
    }
}

void func_80109250(GpActorWork* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->actor;
    flags = inner->field_962;
    if (flags & 0x5000) {
        if (flags & 0x4000) {
            inner->field_973 = -1;
        } else {
            inner->field_973 = 1;
        }
    } else {
        inner->field_973 = 0;
    }
}

s32 func_80109290(GpActorWork* arg0)
{
    GameActor*   inner;
    u16          prev;
    u16          tens;
    s32          ret;
    register s32 temp asm("v0");

    ret = 0;
    if (Gp_StateC08.field_3 == -2) {
        inner            = arg0->actor;
        prev             = inner->field_956;
        inner->field_956 = 6;
        inner->field_954 = 0;
        inner->field_958 = 0;
        inner->field_95A = 0;
        inner->field_95C = 0;
        inner->field_95E = 0;
        inner->field_973 = 0;
        inner->field_97E = 1;
        inner->field_960 = prev;
        temp             = (Gp_StateC08.field_0 % 100U) / 10U;
        tens             = temp;
        if (Gp_StateC08.field_0 >= 0x259U) {
            if (tens == 1) {
                inner->field_93C = 0;
            } else {
                inner->field_93C = 1;
            }
        } else if (tens == 3) {
            inner->field_93C = 2;
        } else if (Gp_StateC08.field_0 < 0x12CU) {
            inner->field_93C = 1;
        } else {
            inner->field_93C = 0;
        }
        ret = 1;
    }
    return ret;
}

void func_80109374(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if ((inner->field_962 & 0x80) && (Gp_StateC08.field_3 == 0) && (Wip_SysConfig.field_21 != 0) &&
        (inner->field_991 == 0)) {
        inner->field_97D = 1;
    } else {
        inner->field_97D = 2;
    }
}

void Gp_UpdateLockTarget(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* next;
    u16         flags;

    inner = arg0->actor;
    if (inner->field_90C != NULL) {
        flags = inner->field_966;
        if (flags & 0x40) {
            Gp_DetachLinkNode(arg0);
            return;
        }
        if (((inner->field_962 & 0x80) && (flags & 0xA000)) || (flags & 0x80)) {
            next = Gp_FindLockNodePad(arg0);
            goto install;
        }
    } else if ((inner->field_966 & 0x80) && !(Wip_SysConfig.field_25 & 1)) {
        register GpActorWork* a asm("a0");

        a                = arg0;
        inner->field_97E = 2;
        next             = Gp_FindLockNode(a);
        goto install;
    }
    return;

install: {
    GpLinkNode*          arg1;
    register GpLinkNode* node asm("v1");
    GameActor*           actor;
    s32                  flag;

    actor = arg0->actor;
    node  = actor->field_90C;
    arg1  = next;
    flag  = 1;
    if (node != arg1) {
        if (node != NULL) {
            node->field_5 = 0;
        }
        actor->field_90C = arg1;
    }
    arg1->field_5 = flag;
}
}

void Gp_PlayerMode2State5(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;
    s32        arg2;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            mode             = 2;
            flag             = 1;
            inner->field_95E = flag;
            inner->field_958 = flag;
            if (inner->field_91C == NULL) {
                mode = 0x13;
            }
            arg2 = 1;
            if (inner->field_934 == 0) {
                arg2 = 6;
            }
            Gp_AnimPlayChildSlots(arg0, mode, arg2);
        case 1:
            if (func_80105ED4(arg0) != 0) {
                inner->field_93E--;
                if (inner->field_93E <= 0) {
                    inner->field_982 = 0;
                    inner->field_956 = 1;
                    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 5);
                }
            } else {
                inner->field_973 = 1;
                Gp_StepPlayerMove(arg0);
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
}

void func_801095BC(s32* arg0)
{
    WipSysConfig*          p;
    volatile WipSysConfig* vp;

    p = &Wip_SysConfig;
    if (p->field_21 == 0x1B) {
        *arg0 = Gp_GetItemSlot(p->field_21 + 0x7F)->field_2 - 0x9F;
        if (*arg0 < 0) {
            *arg0 = 0xA;
        }
        *arg0 = (*arg0 - 0xA) << 24;
    } else {
        vp = p;
        if ((u32)(vp->field_22 - 0xA) < 6U) {
            *arg0 = ((vp->field_22 - 1) % 3) << 24;
            if (*arg0 < 0) {
                *arg0 = 0;
            }
        } else {
            *arg0 = 0;
        }
    }
}

void Gp_PlayerMode2State7(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            mode             = 0x20;
            flag             = 1;
            inner->field_95E = flag;
            if (inner->field_934 != 0) {
                mode = 0x21;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            Gp_AnimTickChildSlots(arg0);
            break;
    }
    Gp_PlayerStepSfx(arg0);
}

void Gp_PlayerMode2State9(GpActorWork* arg0)
{
    Gp_AnimTickChildSlots(arg0);
}

void func_80109720(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    u16            flags;
    s16            delta;
    s32            val;
    s32            temp;

    coord        = (GsCOORDINATE2*)arg0->extra->field_8;
    actor        = arg0->actor;
    coord[4].flg = 0;
    flags        = actor->field_962;
    if ((flags & 0xA000) && (actor->field_954 == 0)) {
        if (flags & 0x8000) {
            delta = -0x20;
        } else {
            delta = 0x20;
        }
        if (ABS(actor->field_6A + delta) < 0x1A1) {
            actor->field_6A += delta;
        }
    } else if (actor->field_6A != 0) {
        val   = actor->field_6A >> 3;
        delta = val;
        temp  = val;
        if (ABS(temp) < 0x40) {
            val = 0x40;
            if (temp < 0) {
                val = -0x40;
            }
            delta = val;
        }
        actor->field_6A -= delta;
        if (ABS(actor->field_6A) < 0x41) {
            actor->field_6A = 0;
        }
    }
}

void func_80109818(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 4;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 5;
    inner->field_95E = 0;
    inner->field_981 = 0;
}

void func_80109844(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GameActor*     inner;
    GameActor*     inner2;
    GpEffArg*      params;
    GsCOORDINATE2* coord;
    s32            idx;
    s32            temp;
    s32            val;

    inner    = arg0->actor;
    temp     = (u16)((u16)inner->field_96E / 12);
    idx      = 2;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    params   = &D_80113358;
    head    -= 8;
    *scratch = head;
    vec      = (SVECTOR*)head;
    if (temp < 3) {
        idx = temp;
    }
    temp = idx;
    __asm__ volatile("" : "+r"(temp) : "r"(head), "r"(vec));
    switch (inner->field_95E) {
        case 0:
            inner->field_95E   = 1;
            coord              = (GsCOORDINATE2*)((GpObj*)inner->field_AC)[(s8)inner->field_993].field_8;
            params->field_4    = (temp * 0x20) + 0x120;
            params->field_6    = temp + 1;
            D_80113358.field_0 = coord;
            inner->field_934   = 0;
            inner->field_93E   = temp;
            /* fallthrough */
        case 1:
            if (inner->field_934 == 0) {
                idx = 5;
                if (temp < 3) {
                    idx = 6;
                }
                inner->field_93E--;
                if (inner->field_93E == 0) {
                    inner->field_95E++;
                } else {
                    inner->field_934 = 6;
                }
                vec->vx = 0;
                val     = 0;
                if ((s8)inner->field_993 == 0) {
                    val = -0x190;
                }
                vec->vy = val;
                vec->vz = 0;
                func_800FDB18(idx, params->field_0, vec, params);
            } else {
                inner->field_934--;
            }
            break;
        case 2:
            break;
        case 3:
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_80109A1C(GpActorWork* arg0)
{
    GameActor*     inner;
    GpEffArg*      params;
    GsCOORDINATE2* coords;
    s32            idx;
    s32            temp;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            inner->field_95E = 1;
            inner->field_934 = 0;
            inner->field_93E = 0;
            /* fallthrough */
        case 1:
            if (inner->field_934 == 0) {
                params = &D_80113358;
                inner->field_93E++;
                if (inner->field_93E == 3) {
                    inner->field_95E++;
                } else {
                    inner->field_934 = 6;
                }
                coords          = &((GsCOORDINATE2*)arg0->extra->field_8)[inner->field_93E + 1];
                params->field_0 = coords;
                temp            = (u16)((u16)inner->field_96E / 12);
                idx             = 2;
                if (temp < 3) {
                    idx = temp;
                }
                temp            = idx;
                params->field_4 = (temp * 0x60) + 0xC0;
                params->field_6 = temp + 1;
                func_800FDB18(3, coords, 0, params);
            } else {
                inner->field_934--;
            }
            break;
        case 2:
            break;
        case 3:
            func_8010B210(arg0);
            inner->field_97A = 0x12;
            if (inner->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
}

void func_80109BB4(GpActorWork* arg0, GpRec18* arg1)
{
    void**             scratch;
    u8*                head;
    GpPushBackScratch* s;
    GameActor*         actor;
    GsCOORDINATE2*     coord;
    GpHitRec*          rec;
    GpObj*             obj;
    VECTOR*            delta;
    s32                i;
    s32                best;
    s32                push;
    s32                id;
    s32                val;

    rec      = (GpHitRec*)arg1;
    best     = 0;
    i        = 0;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x40;
    s        = (GpPushBackScratch*)(head - 0x40);
    actor    = arg0->actor;
    coord    = (GsCOORDINATE2*)arg0->extra->field_8;

    for (i = 0; i < 0x12; rec++, i++) {
        delta = &s->delta;
        if (rec->flags & 1) {
            switch (rec->kind) {
                case 0:
                case 1:
                case 2:
                    break;
                case 3:
                    if ((s8)actor->field_992 != 0) {
                        break;
                    }
                    id = rec->id;
                    if (id < 0x46 && D_80113F9C[id] == 1) {
                        obj = &((GpObj*)actor->field_AC)[(u8)rec->flags >> 4];
                        gte_SetRotMatrix(&((GsCOORDINATE2*)obj->field_8)->workm);
                        gte_ldv0(&obj->field_10);
                        gte_rtv0_real();
                        gte_stlvnl(&s->delta);
                        s->pos.vx = ((GsCOORDINATE2*)obj->field_8)->workm.t[0] +
                                    s->delta.vx;
                        s->pos.vy =
                            ((GsCOORDINATE2*)obj->field_8)->workm.t[1] + s->delta.vy;
                        s->pos.vz =
                            ((GsCOORDINATE2*)obj->field_8)->workm.t[2] + s->delta.vz;
                        s->delta.vx = s->pos.vx - rec->x;
                        s->delta.vy = s->pos.vy - rec->y;
                        s->delta.vz = s->pos.vz - rec->z;
                        push        = rec->dist - SquareRoot0(s->delta.vx * s->delta.vx +
                                                              s->delta.vy * s->delta.vy +
                                                              s->delta.vz * s->delta.vz);
                        val         = push;
                        if (push < 0) {
                            val = 0;
                        }
                        push = val;
                        if (best < push) {
                            best = push;
                            VectorNormal(delta, &s->unit);
                            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm,
                                                   &s->unit, &s->local);
                        }
                    }
                    break;
                case 4:
                    func_8010B2D4(arg0, (GpIdRec*)rec, (u8)rec->flags >> 4);
                    break;
                case 5:
                    func_8010B348(arg0, (GpIdRec*)rec, (u8)rec->flags >> 4);
                    break;
            }
        }
    }

    if (best > 0) {
        actor->field_986   = 1;
        actor->field_30.vx = coord->workm.t[0];
        actor->field_30.vy = coord->workm.t[1];
        actor->field_30.vz = coord->workm.t[2];
        s->pos.vx          = coord->coord.t[0];
        s->pos.vz          = coord->coord.t[2];
        coord->coord.t[0] += (best * s->local.vx) >> 12;
        coord->coord.t[2] += (best * s->local.vz) >> 12;
        coord->flg         = 0;
        Gp_UpdateCoord(coord);
        actor->field_30.vx = coord->workm.t[0] - actor->field_30.vx;
        actor->field_30.vy = coord->workm.t[1] - actor->field_30.vy;
        actor->field_30.vz = coord->workm.t[2] - actor->field_30.vz;
        VectorNormal(&actor->field_30, &actor->field_30);
        coord->coord.t[0] = s->pos.vx + ((best * s->local.vx) >> 14);
        coord->coord.t[2] = s->pos.vz + ((best * s->local.vz) >> 14);
        coord->flg        = 0;
        Gp_UpdateCoord(coord);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x40;
}

void func_80109FC4(GpActorWork* arg0)
{
    s32        flags;
    GameActor* actor;
    s32        temp;
    s32        mode;

    flags = Wip_SysConfig.field_25;
    actor = arg0->actor;
    if (flags != 0) {
        if (flags & 1) {
            temp             = (u16)actor->field_944 - 1;
            actor->field_944 = temp;
            if ((s16)temp <= 0) {
                flags &= ~1;
            }
        }
        if (flags & 2) {
            temp             = (u16)actor->field_946 - 1;
            actor->field_946 = temp;
            if ((s16)temp <= 0) {
                flags &= ~2;
            }
        }
        if (flags & 4) {
            temp             = actor->field_98D - 1;
            actor->field_98D = temp;
            if ((s8)temp <= 0) {
                Gp_ApplyHpDamage(1);
                mode = (u16)actor->field_958;
                if (mode == 0) {
                    actor->field_98D = 0x78;
                } else if (mode == 3) {
                    actor->field_98D = 0x14;
                } else {
                    actor->field_98D = 0x3C;
                }
            }
            temp             = (u16)actor->field_948 - 1;
            actor->field_948 = temp;
            if ((s16)temp <= 0) {
                flags &= ~4;
            }
        }
        if (flags & 0x10) {
            temp             = (u16)actor->field_94A - 1;
            actor->field_94A = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x10;
            }
        }
        if (flags & 0x20) {
            temp             = (u16)actor->field_94C - 1;
            actor->field_94C = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x20;
            }
        }
        if (flags & 0x40) {
            temp             = (u16)actor->field_94E - 1;
            actor->field_94E = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x40;
            }
        }
        if (flags & 0x80) {
            if ((u32)((u8)Gp_StateC08.field_A - 2) >= 2U) {
                temp             = (u16)actor->field_950 - 1;
                actor->field_950 = temp;
                if ((s16)temp <= 0) {
                    flags &= ~0x80;
                }
            }
        }
        Wip_SysConfig.field_25 = flags;
    }
}

void Gp_TriggerPeState(s32 arg0, s32 arg1)
{
    GpActorWork* work;
    GameActor*   inner;
    s32          mask;

    mask = arg1;
    if (arg0 == 0) {
        work = Game_GetPtrSlot(3);
        if (arg1 & 1) {
            inner = work->actor;
            if (func_800B9D80(0x101) == 0) {
                Wip_SysConfig.field_25 |= 1;
                inner->field_944        = 0x258;
                func_800EC9C8();
                Gp_DetachLinkNode(work);
                Gp_SetState1CPe(1);
            }
        }
        if (mask & 2) {
            inner = work->actor;
            if (func_800B9D80(0x102) == 0) {
                Wip_SysConfig.field_25 |= 2;
                inner->field_946        = 0x258;
                inner->field_98E        = 0;
                func_8010B210(work);
                Gp_SetState1CPe(2);
            }
        }
        if (mask & 4) {
            inner = work->actor;
            if (func_800B9D80(0x104) == 0) {
                Wip_SysConfig.field_25 |= 4;
                inner->field_948        = 0x258;
                inner->field_98D        = 0;
                Gp_SetState1CPe(4);
            }
        }
        if (mask & 0x10) {
            inner = work->actor;
            if (func_800B9D80(0x108) == 0) {
                Wip_SysConfig.field_25 |= 0x10;
                inner->field_94A        = 0x258;
                Gp_SetState1CPe(0x10);
            }
        }
        if (mask & 0x20) {
            inner = work->actor;
            if (func_800B9D80(0x110) == 0) {
                Wip_SysConfig.field_25 |= 0x20;
                inner->field_94C        = 0x258;
                Gp_SetState1CPe(0x20);
            }
        }
        if (mask & 0x40) {
            inner = work->actor;
            if (func_800B9D80(0x120) == 0) {
                Wip_SysConfig.field_25 |= 0x40;
                inner->field_94E        = 0x258;
                inner->field_990        = (rand() & 0x1F) + 0xA;
                inner->field_970        = 0;
                Gp_SetState1CPe(0x40);
            }
        }
        if (mask & 0x80) {
            inner = work->actor;
            if (func_800B9D80(0x140) == 0) {
                Wip_SysConfig.field_25 |= 0x80;
                inner->field_950        = 0x258;
                Gp_SetState1CPe(0x80);
                func_800ECA54();
            }
        }
    } else {
        Wip_SysConfig.field_25 &= ~arg1;
    }
}

void func_8010A42C(GpActorWork* arg0, s32 arg1)
{
    u8 kind;

    kind = arg1;
    if (kind != 0) {
        switch (kind) {
            case 0:
                break;
            case 1: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x101) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 1;
                inner->field_944        = 0x258;
                func_800EC9C8();
                Gp_DetachLinkNode(arg0);
                Gp_SetState1CPe(1);
                break;
            }
            case 2: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x102) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 2;
                inner->field_946        = 0x258;
                inner->field_98E        = 0;
                func_8010B210(arg0);
                Gp_SetState1CPe(2);
                break;
            }
            case 3: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x104) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 4;
                inner->field_948        = 0x258;
                inner->field_98D        = 0;
                Gp_SetState1CPe(4);
                break;
            }
            case 4:
                Gp_SetState1CPe(8);
                break;
            case 8: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x108) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x10;
                inner->field_94A        = 0x258;
                Gp_SetState1CPe(0x10);
                break;
            }
            case 9: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x110) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x20;
                inner->field_94C        = 0x258;
                Gp_SetState1CPe(0x20);
                break;
            }
            case 10: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x120) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x40;
                inner->field_94E        = 0x258;
                inner->field_990        = (rand() & 0x1F) + 0xA;
                inner->field_970        = 0;
                Gp_SetState1CPe(0x40);
                break;
            }
            case 11: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x140) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x80;
                inner->field_950        = 0x258;
                Gp_SetState1CPe(0x80);
                func_800ECA54();
                break;
            }
        }
    }
}

void func_8010A670(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         left;
    s32         right;
    s32         pad;
    s32         bits;
    s32         timer;
    s32         next;
    s32         mode;
    s32         dir;

    inner            = arg0->actor;
    timer            = inner->field_990 - 1;
    inner->field_990 = timer;
    if ((s8)timer == 0) {
        left             = 0x8000;
        next             = (rand() & 0x1F) + 0xA;
        pad              = inner->field_962;
        inner->field_990 = next;
        bits             = pad & 0xF000;
        if (bits == left || bits == (right = 0x2000)) {
            if (!(inner->field_970 & 0x5000)) {
                if (rand() & 1) {
                    dir = 0x4000;
                } else {
                    dir = 0x1000;
                }
                inner->field_970 = dir;
            }
        } else {
            bits = pad & 0x5000;
            if (bits) {
                if (rand() & 4) {
                    inner->field_970 &= 0xAFFF;
                } else if (rand() & 1) {
                    inner->field_970 = left;
                } else {
                    inner->field_970 = right;
                }
            }
        }
        if (Gp_StateF0.field_0 == 1) {
            if (inner->field_90C != NULL) {
                if (rand() & 3) {
                    Gp_DetachLinkNode(arg0);
                }
            } else {
                mode = inner->field_956;
                if (mode == 2 && !(Wip_SysConfig.field_25 & 1) && (rand() & 3)) {
                    node = Gp_FindLockNode(arg0);
                    if (node != NULL) {
                        inner->field_97E = mode;
                        func_80108E0C(arg0, node);
                    }
                }
            }
        }
    }
    if (Game_Session->field_58 & 0xF000) {
        inner->field_962 |= inner->field_970;
    }
}

s32 Gp_ApplyHpDamage(s32 arg0)
{
    register s32   amount asm("s1");
    s32            ret;
    register s32   tmp asm("v0");
    WipSysConfig*  p;
    Task*          slot;
    GsCOORDINATE2* coords;

    amount = arg0;
    ret    = 0;
    if (func_800B9D80(0x40000) != 0) {
        tmp    = (s16)arg0 >> 2;
        amount = arg0 - tmp;
    }
    if (func_800B9D80(0x800) != 0) {
        Wip_SysConfig.field_1c += (s16)amount / 5;
        if (Wip_SysConfig.field_1e < Wip_SysConfig.field_1c) {
            Wip_SysConfig.field_1c = Wip_SysConfig.field_1e;
        }
    }
    if (func_800B9D80(0x200) != 0) {
        p = &Wip_SysConfig;
        if (p->field_18 >= 5 && (s16)amount >= p->field_18) {
            slot        = Game_GetPtrSlot(3);
            coords      = ((TmdObject*)slot->extra)->field_8;
            p->field_18 = 1;
            Gp_SpawnEff(0x6009C, coords + 1, 5, 0);
            return 0;
        }
    }
    Wip_SysConfig.field_18 -= amount;
    if (Wip_SysConfig.field_18 > 0) {
        return ret;
    }
    if (Game_Session->field_1 != 0) {
        Wip_SysConfig.field_18 = 1;
    } else {
        ret = 1;
        Display_AcquireRef();
    }
    return ret;
}

void func_8010A9D0(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    func_8010AAB4(arg0);
    if ((u16)inner->field_96C == 1) {
        mode = 0x10;
    } else {
        mode = 0x11;
    }
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 3);
}

void Gp_StopPlayerAnim(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 1;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_96C = 3;
    if (arg1 == 0) {
        Gp_AnimResetChildSlots(arg0, 0x12);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, 0x12, 0, arg1);
    }
    Gp_DetachLinkNode(arg0);
    inner->field_983 |= 0x18;
}

void func_8010AAB4(GpActorWork* arg0)
{
    GameActor*    inner;
    WipSysConfig* p;

    p                    = &Wip_SysConfig;
    inner                = arg0->actor;
    Gp_StateC08.field_6 |= 1;
    func_80106350(arg0, p->field_21, 0);
    if (p->field_18 > 0) {
        inner->field_954 = 1;
        inner->field_958 = 0;
        inner->field_95A = 0;
        inner->field_95C = 7;
        inner->field_95E = 0;
        inner->field_973 = 0;
        Gp_ApplyHpDamage(inner->field_96E);
        inner->field_12A &= 0x3FFF;
        if ((s8)inner->field_97E == 2) {
            inner->field_97E = 1;
        }
        func_8010A42C(arg0, inner->field_972);
    }
}

void func_8010AB70(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    if (inner->field_956 != 0) {
        func_8010870C(arg0, 0xC);
    } else {
        func_801066DC(arg0, 0);
    }
}

void func_8010ABD4(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (inner->field_95E != 0) {
        if (inner->field_95E == 1) {
            func_8010B210(arg0);
            inner->field_97A = 0x12;
            if (inner->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        }
    }
}

void func_8010AC54(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* inner2;

    inner = arg0->actor;
    if (inner->field_95E == 0) {
        inner->field_95E = 1;
        inner->field_934 = 0;
        inner->field_93E = 0;
    }
    if (inner->field_934 == 0) {
        inner->field_93E++;
        if (inner->field_93E == 3) {
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        } else {
            inner->field_934 = 5;
        }
        Gp_SpawnEff(
            0x600E0, &((GsCOORDINATE2*)arg0->extra->field_8)[4 - inner->field_93E], 0x320, 0);
    } else {
        inner->field_934--;
    }
}

void func_8010AD64(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GameActor*     inner;
    GameActor*     inner2;
    GpEffArg*      params;
    GsCOORDINATE2* coord;
    s32            val;
    s32            idx;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG(scratch);
    head     = *scratch;
    params   = &D_80113358;
    vec      = (SVECTOR*)(head - 8);
    *scratch = vec;
    inner    = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            idx                     = (s8)inner->field_993;
            inner->field_95E        = 1;
            coord                   = (GsCOORDINATE2*)((GpObj*)inner->field_AC)[idx].field_8;
            params->field_4         = 0xC0;
            params->field_6         = 2;
            D_80113358.field_0      = coord;
            ((SVECTOR*)head)[-1].vx = 0;
            val                     = 0;
            if ((s8)inner->field_993 == 0) {
                val = -0x190;
            }
            vec->vy = val;
            vec->vz = 0;
            func_800FDB18(2, D_80113358.field_0, vec, params);
            break;
        case 1:
            break;
        case 2:
            SOFT_COMPILER_BARRIER();
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_8010AE98(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x101) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 1;
    inner->field_944        = 0x258;
    func_800EC9C8();
    Gp_DetachLinkNode(arg0);
    Gp_SetState1CPe(1);
}

void func_8010AF04(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x102) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 2;
    inner->field_946        = 0x258;
    inner->field_98E        = 0;
    func_8010B210(arg0);
    Gp_SetState1CPe(2);
}

void func_8010AF6C(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x104) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 4;
    inner->field_948        = 0x258;
    inner->field_98D        = 0;
    Gp_SetState1CPe(4);
}

void func_8010AFC0(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x108) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x10;
    inner->field_94A        = 0x258;
    Gp_SetState1CPe(0x10);
}

void func_8010B010(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x110) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x20;
    inner->field_94C        = 0x258;
    Gp_SetState1CPe(0x20);
}

void func_8010B060(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x120) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x40;
    inner->field_94E        = 0x258;
    inner->field_990        = (rand() & 0x1F) + 0xA;
    inner->field_970        = 0;
    Gp_SetState1CPe(0x40);
}

void func_8010B0C8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x140) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x80;
    inner->field_950        = 0x258;
    Gp_SetState1CPe(0x80);
    func_800ECA54();
}

void Gp_PlayerStepSfx(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* inner2;
    GpObj38*   obj;
    s32        mode;
    s32        snd;
    s32        temp;
    s32        temp2;

    inner = arg0->actor;
    obj   = (GpObj38*)arg0->extra->field_8;
    if (Mc_SaveData.field_5C2 != 0) {
        return;
    }
    if ((s8)inner->field_97A != 0) {
        return;
    }
    func_80109BB4(arg0, inner->field_17C);
    if ((u16)inner->field_96C == 0) {
        return;
    }
    inner2 = arg0->actor;
    func_8010AAB4(arg0);
    mode = 0x11;
    if ((u16)inner2->field_96C == 1) {
        mode = 0x10;
    }
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 3);
    temp  = (s8)Gp_GetObjPan(obj);
    temp2 = (s8)Gp_GetObjDepth(obj);
    snd   = 7;
    if ((u16)inner->field_96C == 1) {
        snd = 6;
    }
    SndEvt_EnqueueType6(snd, temp, temp2);
}

void func_8010B210(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_96C = 0;
    inner->field_972 = 0;
    inner->field_96E = 0;
}

s32 Gp_TestHpDamage(s32 arg0)
{
    WipSysConfig* p;
    u16           saved18;
    u16           saved1c;
    s32           out;
    s32           ret;

    p           = &Wip_SysConfig;
    saved18     = p->field_18;
    saved1c     = p->field_1c;
    ret         = Gp_ApplyHpDamage((s16)Gp_ScaleDamage(arg0, 0, &out, 0));
    p->field_18 = saved18;
    p->field_1c = saved1c;
    if (ret != 0) {
        Display_ReleaseRef();
    }
    return ret;
}

void func_8010B2A0(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(D_80113340, arg0, arg1, 0);
}

void func_8010B2D4(GpActorWork* arg0, GpIdRec* arg1, s32 arg2)
{
    GameActor* inner;
    s32        out;
    s32        flag;

    inner = arg0->actor;
    flag  = inner->field_910 != 0;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        arg2             = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_96E = Gp_ScaleDamage(arg1->field_4, 0, &out, flag);
        inner->field_972 = out;
    }
}

void func_8010B348(GpActorWork* arg0, GpIdRec* arg1, s32 arg2)
{
    GameActor* inner;
    u32        kind;

    inner = arg0->actor;
    kind  = (u16)arg1->field_4;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        if (kind == 2) {
            goto case24;
        }
        if (kind < 3) {
            goto do_call;
        }
        if (kind == 3) {
            goto case3;
        }
        if (kind != 4) {
            goto do_call;
        }
    case24:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 5;
        goto do_call;
    case3:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 0;
    do_call:
        inner->field_96E = Gp_LookupIdField(arg1->field_4, 0);
    }
}

void func_8010B3F8(Task* arg0)
{
    Task*          slot;
    GpEffArg*      params;
    GsCOORDINATE2* coords;
    s32            idx;
    u16            count;
    s16            next;

    slot = Game_GetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0;
            /* fallthrough */
        case 1:
            count = arg0->killCountdown;
            if ((count & 0xF) == 0) {
                next                = count + 0x100;
                params              = &D_80113358;
                arg0->killCountdown = next;
                idx                 = arg0->spawnArg1 & 3;
                if (next >= 0x300) {
                    Task_Kill(arg0);
                } else {
                    arg0->killCountdown = next | 6;
                }
                coords          = (GsCOORDINATE2*)slot->extra;
                count           = arg0->killCountdown;
                coords          = (GsCOORDINATE2*)((TmdObject*)coords)->field_8;
                params->field_4 = (idx * 0x60) + 0xC0;
                params->field_6 = idx + 1;
                coords          = &coords[((count & 0xF00) >> 8) + 1];
                params->field_0 = coords;
                func_800FDB18(3, coords, 0, params);
            } else {
                arg0->killCountdown = count - 1;
            }
            break;
    }
}

void func_8010B520(Task* arg0)
{
    GsCOORDINATE2* raw;
    Task*          slot;
    TmdObject*     extra;
    GpEffArg*      params;
    GsCOORDINATE2* coords;

    params          = &D_80113358;
    slot            = Game_GetPtrSlot(3);
    extra           = slot->extra;
    raw             = extra->field_8;
    params->field_4 = 0xC0;
    coords          = &raw[3];
    params->field_0 = coords;
    params->field_6 = (u16)arg0->spawnArg1 + 1;
    func_800FDB18(2, coords, 0, params);
    Task_Kill(arg0);
}

void func_8010B590(Task* arg0)
{
    TmdObject*  extra;
    GpCoordExt* coord;

    extra = (TmdObject*)arg0->extra;
    coord = (GpCoordExt*)extra->field_8;
    arg0->state++;
    coord->flg = 0;
    if (coord->field_44 != 0) {
        extra->field_C = 0;
    }
}

void func_8010B5C0(Task* arg0)
{
    Task*      parent;
    TmdObject* extra;

    parent              = arg0->parent;
    extra               = (TmdObject*)arg0->extra;
    extra->field_C      = ((TmdObject*)parent->extra)->field_C;
    extra->field_8->flg = 0;
}

void func_8010B5E4(Task* arg0)
{
    arg0->state = 3;
}

void func_8010B5F0(Task* arg0)
{
    Task_Kill(arg0);
}

void func_8010B610(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80097AB0;
    sp.funcs[arg0->state](arg0);
}

void Gp_EndPlayerActorTask(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    GameActor* next;
    GpAnimObj* extra;
    Task*      task;

    actor = arg0->actor;
    task  = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_91C = NULL;
        extra            = (GpAnimObj*)arg0->extra;
        inner            = arg0->actor;
        inner->field_93A = Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
        inner->field_928 = Gp_AnimBlkTbl[inner->field_93A];
        func_800B3F84((GpAnimCtx*)inner->field_424, inner->field_928, extra, &inner->field_7A8,
                      (GpAnimSlot*)inner->field_438);
        Gp_AnimResetChildSlots(arg0, 1);
        next            = arg0->actor;
        next->field_954 = 0;
        next->field_956 = 0;
        next->field_958 = 0;
        next->field_95A = 0;
        next->field_95C = 0;
        next->field_95E = 0;
        next->field_942 = 0;
        next->field_93E = 0;
        next->field_973 = 0;
        next->field_975 = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 4);
    }
    task = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_914 = NULL;
    }
}

s32 Gp_SetupAllyWeapon(void)
{
    GpActorWork* work;
    GameActor*   actor;
    GameActor*   inner;
    GameActor*   next;
    Task*        task;
    McSaveData*  save;
    GpActorD4*   block;
    s16          val1;
    s16          val2;
    GpEffWork*   eff;
    GpAnimObj*   extra;
    s32          ret;

    work  = Game_GetPtrSlot(0xA);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    if (actor->field_924 != NULL) {
        save             = &Mc_SaveData;
        task             = func_80104364((GpActorWork*)actor->field_924, save->field_13 + 1, save->field_5C7, 0);
        actor->field_91C = task;
        if (task != NULL) {
            block = actor->field_910;
            val1  = D_80167218[save->field_5C7];
            val2  = D_80167224[save->field_5C7];
            Gp_AttachActorObj(work, val1, val2);
            actor->field_124 |= 0x80;
            block->field_CD   = D_80167230[save->field_5C7];
            if ((u8)save->field_5C7 == 4 && actor->field_914 == NULL) {
                eff = Gp_SpawnEff(
                    0x80060180, (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, val1, 0);
                if (eff != NULL) {
                    actor->field_914 = eff->field_0;
                    func_80106350(work, val1, 0);
                }
            }
        }
    }

    inner            = work->actor;
    extra            = (GpAnimObj*)work->extra;
    inner->field_93A = Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    inner->field_928 = Gp_AnimBlkTbl[inner->field_93A];
    func_800B3F84((GpAnimCtx*)inner->field_424, inner->field_928, extra, &inner->field_7A8,
                  (GpAnimSlot*)inner->field_438);
    next            = work->actor;
    next->field_954 = 0;
    next->field_956 = 0;
    next->field_958 = 0;
    next->field_95A = 0;
    next->field_95C = 0;
    next->field_95E = 0;
    next->field_942 = 0;
    next->field_93E = 0;
    next->field_973 = 0;
    next->field_975 = 0;
    Gp_AnimResetChildSlots(work, 1);
    ret              = (s32)actor->field_91C;
    actor->field_983 = 7;
    return ret;
}

void func_8010B9A4(GpActorWork* arg0)
{
    GameActor*  actor;
    McSaveData* save;
    s32         field13;
    u16         temp;
    u16         anim;

    actor            = arg0->actor;
    actor->field_954 = 1;
    actor->field_95C = 7;
    save             = &Mc_SaveData;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95E = 0;
    actor->field_960 = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    if (save->field_5C2 == 0 && (field13 = save->field_13) == 1) {
        temp            = save->field_6C8 - actor->field_96E;
        save->field_6C8 = temp;
        if ((s16)temp <= 0 && Game_Session->field_1 != 0) {
            save->field_6C8 = field13;
        }
    }
    actor->field_12A &= 0x3FFF;
    if ((s8)actor->field_97E == 2) {
        actor->field_97E = 1;
    }
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    anim = 0x11;
    if ((u16)actor->field_96C == 1) {
        anim = 0x10;
    }
    Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 3);
}

Task* Gp_SpawnAlly(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3)
{
    Task*          task;
    GameActor*     actor;
    GpActorD4*     block;
    GsCOORDINATE2* coord;
    s32            type;

    if (arg1 == 1) {
        type = Mc_SaveData.field_5C7 + 0x7F;
    } else {
        type = arg1 + 0x82;
    }
    task = Task_Spawn(7, type, arg2, (s32)arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = Mem_Calloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
fail:
    Task_Kill(task);
    return NULL;

have_actor:
    block = Mem_Calloc(0xD4, 0);
    if (block == NULL) {
        goto fail;
    }
    Game_SetPtrSlot(task, 0xA);
    Mem_Set(actor, 0, 0x998);
    Mem_Set(block, 0, 0xD4);
    task->idMap      = (TaskIdMap*)actor;
    actor->field_910 = block;
    Gp_PumpTmdStream(task);
    actor->field_93C  = *arg3;
    actor->field_52   = arg0->field_0;
    coord             = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    return task;
}

void Gp_ResetActorMove(GpActorWork* arg0, s16 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    inner->field_93E = 0;
    inner->field_973 = 0;
    inner->field_975 = 0;
    if (arg1 != 0) {
        Gp_AnimResetChildSlots(arg0, 1);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 4);
    }
}

s32 func_8010BC70(GsCOORDINATE2* arg0)
{
    void**     scratch;
    u8*        head;
    VECTOR3*   vec;
    TmdObject* extra;
    s32        ret;

    extra    = (TmdObject*)((Task*)Game_GetPtrSlot(3))->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x10);
    *scratch = vec;
    func_80103C74(arg0, (VECTOR3*)((GsCOORDINATE2*)extra->field_8)->coord.t, vec);
    ret      = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

s16 func_8010BCF4(Task* arg0, VECTOR3* arg1)
{
    void**              scratch;
    u8*                 head;
    VECTOR3*            vec;
    TmdObject*          extra;
    register GameActor* actor asm("s2");
    s16                 ret;

    extra    = (TmdObject*)arg0->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x10);
    *scratch = vec;
    actor    = (GameActor*)arg0->idMap;
    func_80103C74((GsCOORDINATE2*)extra->field_8, arg1, vec);
    ret      = func_80103E7C(actor->field_52, ratan2(((VECTOR3*)(head - 0x10))->vx, vec->vz));
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

void func_8010BD88(GpActorWork* arg0, VECTOR3* arg1)
{
    void**         scratch;
    u8*            head;
    GpTurnScratch* vec;
    TmdObject*     extra;
    GameActor*     actor;
    s32            val;

    extra   = arg0->extra;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vec = *scratch = (GpTurnScratch*)(head - 0x14);
    actor          = arg0->actor;
    func_80103C74((GsCOORDINATE2*)extra->field_8, arg1, (VECTOR3*)vec);
    vec->angle = ratan2(((GpTurnScratch*)(head - 0x14))->vx, vec->vz);
    val        = func_80103E7C(actor->field_52, vec->angle);
    vec->angle = val;
    if (val > 0x40) {
        vec->angle = 0x40;
    } else if (val < -0x40) {
        vec->angle = -0x40;
    }
    actor->field_52         = (actor->field_52 + vec->angle) & 0xFFF;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

void func_8010BE5C(GpActorWork* arg0, VECTOR3* arg1)
{
    void**         scratch;
    u8*            head;
    GpAimScratch*  block;
    GsCOORDINATE2* coord;
    SVECTOR*       rot;
    TmdObject*     extra;
    GameActor*     actor;
    s32            val;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    extra   = arg0->extra;
    actor   = arg0->actor;
    coord   = (GsCOORDINATE2*)(head - 0x50);
    rot     = (SVECTOR*)(head - 0x58);
    /* The coord array replaces the object in the same register (v0). */
    extra = (TmdObject*)extra->field_8;
    block = *scratch = (GpAimScratch*)(head - 0x68);
    block->rot.vx    = 0;
    block->rot.vy    = 0;
    block->rot.vz    = 0;
    Gp_PlaceCoordOffset((GsCOORDINATE2*)extra + 4, coord, rot);
    func_80103C74(coord, arg1, (VECTOR3*)block);
    val = ratan2(((VECTOR3*)(head - 0x68))->vx, block->vec.vz) - actor->field_52;
    val = func_80103E7C(actor->field_6A, val);
    if (val > 0x20) {
        val = 0x20;
    } else if (val < -0x20) {
        val = -0x20;
    }
    if (ABS(actor->field_6A + val) < 0x1A0) {
        actor->field_6A += val;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x68;
}

void func_8010BF7C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    arg0->actor->field_910->field_C4 = arg1 + (arg2 & rand());
}

void func_8010BFCC(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_93A = Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    actor->field_928 = Gp_AnimBlkTbl[actor->field_93A];
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->field_438);
}

s32 func_8010C058(void)
{
    s32 ret;

    if (((s16)Mc_SaveData.field_6CA >> 1) < (s16)Mc_SaveData.field_6C8) {
        ret = 0;
    } else if (((s16)Mc_SaveData.field_6CA >> 2) >= (s16)Mc_SaveData.field_6C8) {
        ret = 2;
    } else {
        ret = 1;
    }
    return ret;
}

void Gp_TrackAllyLockTarget(GpActorWork* arg0, s32 arg1)
{
    GameActor*  actor;
    GpLinkNode* node;
    s32         val;

    actor = arg0->actor;
    node  = actor->field_90C;
    if (node == NULL || (node->field_4 & 1)) {
        actor->field_90C = NULL;
        actor->field_97E = 1;
    } else if ((s8)actor->field_97E == 2) {
        if (arg1 & 1) {
            val = 0;
            if (arg1 != 1) {
                val = 0x380;
            }
            Gp_AimYawToLock(arg0, val);
        }
        if (arg1 & 2) {
            if (D_80113388[Mc_SaveData.field_5C7] != 0) {
                Gp_AimPitchToLock(arg0);
            } else {
                Gp_AimPitchRec(arg0, D_80167218[Mc_SaveData.field_5C7], 0x380);
            }
        }
    }
}

void func_8010C180(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* actor;

    inner = arg0->actor;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    actor            = arg0->actor;
    actor->field_954 = 0;
    actor->field_956 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 4);
}

void Gp_BindActorD4(GpActorWork* arg0, SVECTOR3* arg1, s32 arg2)
{
    GsCOORDINATE2* dest;
    GsCOORDINATE2* src;
    GpActorD4*     block;
    GpObj*         obj;
    GpActorD4Rec*  rec;
    s16            vz;

    block         = arg0->actor->field_910;
    src           = (GsCOORDINATE2*)arg0->extra->field_8;
    obj           = (GpObj*)block->field_68;
    rec           = &block->field_88;
    dest          = (GsCOORDINATE2*)block->field_18;
    *dest         = *src;
    obj->field_8  = block->field_18;
    obj->field_14 = -0xA0;
    obj->field_18 = 0x60000;
    obj->field_C  = (GpRec18*)rec;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->flags    = 3;
    rec->field_8  = arg1->vx;
    rec->field_A  = arg1->vy;
    vz            = arg1->vz;
    rec->field_4  = arg2;
    rec->field_0  = rec->field_8;
    rec->field_12 = 0x80;
    rec->field_10 = 0x80;
    rec->field_14 = &block->field_A0;
    rec->field_C  = vz;
    rec->field_2  = rec->field_A;
    Gp_LinkObj(1, obj);
    Gp_InitRec18Table(rec->field_14, 1, 0);
    obj->flags |= 0xC800;
}

s32 func_8010C30C(GpActorWork* arg0)
{
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* next;
    GameActor*     actor;
    VECTOR         vec;
    void*          prev;
    void*          anim;
    s32            changed;

    extra  = arg0->extra;
    coord  = (GsCOORDINATE2*)extra->field_8;
    actor  = arg0->actor;
    next   = coord + 1;
    vec.vx = next->coord.t[0];
    vec.vy = next->coord.t[1];
    vec.vz = next->coord.t[2];
    ApplyMatrixLV(&coord->coord, &vec, &vec);
    coord->coord.t[0] += vec.vx;
    coord->coord.t[2] += vec.vz;
    next->coord.t[0]   = 0;
    next->coord.t[2]   = 0;
    actor->field_10    = coord->coord.t[0];
    actor->field_14    = coord->coord.t[1];
    actor->field_18    = coord->coord.t[2];
    prev               = actor->field_928;
    actor->field_93A   = Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    anim               = Gp_AnimBlkTbl[actor->field_93A];
    changed            = prev != anim;
    actor->field_928   = anim;
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, (GpAnimObj*)extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->field_438);
    actor->field_985 = 0x10;
    actor->field_983 = 7;
    actor->field_95E = 0;
    actor->field_960 = 0;
    Gp_ResetActorMove(arg0, changed);
    return 0;
}

void func_8010C46C(GpActorWork* arg0)
{
    GameActor* actor;

    actor             = arg0->actor;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
}

s32 func_8010C4F0(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor             = arg0->actor;
    extra             = (GpAnimObj*)arg0->extra;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    actor->field_956 = 1;
    if (actor->field_928 != Gp_AnimBlkTbl[(s32)arg2->field_0]) {
        actor->field_928 = Gp_AnimBlkTbl[(s32)arg2->field_0];
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->field_438);
        actor->field_93A = (u16)arg2->field_0;
    }
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        Gp_AnimResetChildSlots(arg0, arg2->field_4);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, arg2->field_4, 1, arg2->field_C);
    }
    if (arg2->field_10 == 0) {
        actor->field_983 = 0x38;
    } else {
        actor->field_983 = 7;
    }
    return 0;
}

s32 func_8010C648(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80104B54(arg0, arg1, arg2);
    p->field_24 = saved;
    return 0;
}

s32 func_8010C688(GpActorWork* arg0, s32 arg1, GpXformArg* arg2)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80104E00(arg0, arg1, arg2);
    p->field_24 = saved;
    return 0;
}

s32 func_8010C6C8(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    Gp_SetActorDest(arg0, arg1, arg2, arg3);
    p->field_24 = saved;
    return 0;
}

s32 func_8010C708(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3)
{
    WipSysConfig* p;
    u8            saved;
    GameActor*    actor;

    p     = &Wip_SysConfig;
    actor = arg0->actor;
    saved = p->field_24;
    Gp_SetActorDest(arg0, arg1, arg2, arg3);
    p->field_24      = saved;
    actor->field_956 = 8;
    return 0;
}

s32 func_8010C75C(GpActorWork* arg0, s32 arg1, GpDelayArg* arg2)
{
    GameActor* actor;

    actor = arg0->actor;
    if ((s8)actor->field_97A != 0) {
        return 1;
    }
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    actor->field_956 = 6;
    actor->field_934 = arg2->field_14;
    actor->field_93E = 0;
    return 0;
}

void Gp_MoveActorByKeep(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    Gp_MoveActorBy(arg0, arg1, arg2);
    p->field_24 = saved;
}

s32 Gp_CopyAllyAnim(GpActorWork* arg0, s32 arg1, GpCopyArg* arg2)
{
    s32* dest;
    s32* src;
    s32  i;
    s32  count;

    dest  = (s32*)Gp_AnimBlkTbl[Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7];
    src   = arg2->field_0;
    count = arg2->field_4;
    if (count >= 0x21) {
        return 1;
    }
    dest = ((GpAnimBlk*)dest)->field_BC;
    for (i = 0; i < arg2->field_4; i++) {
        dest[i] = src[i];
    }
    return 0;
}

s32 Gp_HurtAlly(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    s32 ret;

    ret = 0;
    if (Mc_SaveData.field_5C2 == 0) {
        Mc_SaveData.field_6C8 -= Gp_ScaleDamage(arg2, 0, 0, 1);
        if ((s16)Mc_SaveData.field_6C8 <= 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            ret = 1;
        }
    }
    return ret;
}

void func_8010C980(void* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5)
{
    arg1->field_8  = arg0;
    arg1->field_C  = arg2;
    arg1->field_10 = 0;
    arg1->field_12 = 0;
    arg1->field_14 = 0;
    arg1->flags    = 1;
    arg1->field_18 = arg4 | 0x30000;
    arg1->field_1C = arg5;
    Gp_LinkObj(2, arg1);
    arg1->flags |= 0x8000;
    Gp_InitRec18Table(arg1->field_C, (s16)arg3, 0);
}
