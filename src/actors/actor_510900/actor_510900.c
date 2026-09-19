#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/session.h"

#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/gameflag.h"
#include "main/wipsys.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"
#include "actors/actors_shared_8013bbe4.h"

#include <psyq/inline_c.h>

/* `gte_ApplyMatrix` / `gte_MulMatrix0` / `gte_RotTransPers` / `gte_LoadAverageShort12`
 * from `psyq/gtemac.h`, except with the real `rtv0` / `rtir` / `rtps` / `gpf`
 * encodings this toolchain assembles correctly. */
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtir_real()  __asm__ volatile("nop; nop; .word 0x4A49E012")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_510900_80134C90(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_actor_510900_80135744(Actor510900* arg0);
void func_actor_510900_8013864C(Actor510900* arg0);
void func_actor_510900_801387F4(Actor510900* arg0);
void func_actor_510900_80138978(Actor510900* arg0);
void func_actor_510900_80138A9C(Actor510900* arg0);
void func_actor_510900_80138BF0(Actor510900* arg0);
void func_actor_510900_80138D38(Actor510900* arg0);
void func_actor_510900_80138F44(Actor510900* arg0);
void func_actor_510900_8013B804(Actor510900* arg0);
void func_actor_510900_8013BB20(Actor510900* arg0);
void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1);
void func_actor_510900_8013BC80(Actor510900* arg0);

extern u8  D_801153F4;
extern u32 Gp_LcgState;
extern s16 D_80073BA0;
extern s32 D_80070F70;

/// The twelve muzzle-flash CLUTs `func_actor_510900_80134C90` indexes by frame.
extern Actor510900SprClut D_actor_510900_8013C48C[];

/// The pair source the context's `field_50` points at; its `field_4` seeds the
/// enemy's HP.
extern GpPairSrcE D_actor_510900_80167980;

/// The block the tick handler reaches through `Task::msgTable`.
extern u32 D_actor_510900_80167A6C;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_510900_80167AA4[];

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_80180A64(GsCOORDINATE2* arg0);

/// Gameplay-resident camera-shake slot: `field_0` is the frame countdown and
/// `coord` is set local to `Gfx_ViewWorldMtx` each frame it runs.
extern GpCoord64 D_80114FF8;

void func_actor_510900_8013B424(s32 arg0);
void func_actor_510900_8013B524(Actor510900* arg0);

void func_actor_510900_80131F24(Task* arg0)
{
    GpEffWork*              mem;
    GsCOORDINATE2*          coord;
    GpCoordTail*            slot;
    GpCoord64*              base;
    GpEffWork*              eff;
    Actor510900MatrixWords* mat;
    s32                     i;
    s32                     bits;
    s32                     z;

    mem   = arg0->spawnArg2;
    coord = &((Actor510900Obj2C*)arg0->extra)->field_8->field_0;
    base  = &D_80114FF8;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            base->field_0 = 0;
        }
        if (arg0->spawnArg1 == 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        mat               = (Actor510900MatrixWords*)&coord->coord;
        coord->sub        = mem->field_8;
        mat->m00_m01      = 0x1000;
        mat->m02_m10      = 0;
        mat->m11_m12      = 0x1000;
        mat->m20_m21      = 0;
        mat->m22          = 0x1000;
        coord->coord.t[0] = mem->field_18;
        coord->coord.t[1] = mem->field_1A;
        z                 = mem->field_1C;
        coord->flg        = 0;
        coord->coord.t[2] = z;
        arg0->state       = 1;
    }
    Gp_UpdateCoord(coord);
    if (base->field_0 != 0) {
        slot->field_50 = 0x1000;
        slot->field_52 = 0x800;
        slot->field_54 = 0x400;
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        base->field_0--;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &base->coord.coord);
        base->coord.flg = 0;
        if (base->field_0 == 0) {
            arg0->spawnArg1 = 0;
            mem->field_22   = 0;
            mem->field_24   = 0;
        }
    }
    switch (arg0->spawnArg1) {
        case 0:
            break;
        case 1:
            mem->field_24 = (mem->field_24 < 0x100) ? mem->field_24 + 0x10 : 0x100;
            for (i = 0; i < 2; i++) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x2C0) - 0x80;
                mem->field_12 = 0x40;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + mem->field_24, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            base->field_0  = 0x10;
            slot->field_58 = 0x1F40;
            slot->field_5C = 0x2710;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            bits           = Gp_LcgState >> 16;
            /* The `field_24 + 0x10000` sums below are evaluated as their own
             * operand. Written plainly, `fold` reassociates the constant onto
             * the draw; held in a local, sched1 moves the load ahead of it. */
            if (!(bits & 3)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0x40;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + ({ mem->field_24 + 0x10000; }),
                                            (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x6004C, coord, ((Gp_LcgState >> 16) & 0xF0) + ({ mem->field_24 + 0x10000; }),
                                            (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + mem->field_24, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            bits >>= 1;
            if (bits % 3 == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0x80;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = -0x80;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x180, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            break;
        case 2:
            base->field_0  = 0x10;
            slot->field_58 = 0x1F40;
            slot->field_5C = 0x2710;
            for (i = 0; i < 3; i++) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0x40;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x10100, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            bits        = Gp_LcgState >> 16;
            if (!(bits & 7)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x100, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = 0x80;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            bits >>= 1;
            if (!(bits & 7)) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12 = -0x80;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                eff           = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x180, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            break;
        case 3:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            bits        = Gp_LcgState >> 16;
            mem->field_22++;
            if (mem->field_22 < 0x1E) {
                if (!(bits & 7)) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = -((Gp_LcgState >> 16) % 0x2C0) - 0x80;
                    mem->field_12 = 0x40;
                    mem->field_14 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    eff           = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x80, (SVECTOR*)&mem->field_10);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                bits >>= 1;
                if (!(bits & 3)) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->field_12 = 0x40;
                    mem->field_14 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    eff           = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, (SVECTOR*)&mem->field_10);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                for (i = 0; i < 2; i++) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->field_12 = 0;
                    mem->field_14 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    eff           = Gp_SpawnEff(0x6004C, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x10100, (SVECTOR*)&mem->field_10);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                bits >>= 1;
                if (!(bits & 7)) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->field_12 = 0;
                    mem->field_14 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    eff           = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x100, (SVECTOR*)&mem->field_10);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                bits >>= 1;
                if (!(bits & 7)) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->field_12 = 0x80;
                    mem->field_14 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    eff           = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, (SVECTOR*)&mem->field_10);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                for (i = 0; i < 2; i++) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->field_12 = -0x80;
                    mem->field_14 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    eff           = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x180, (SVECTOR*)&mem->field_10);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                base->field_0  = 0x10;
                slot->field_58 = 0x1F40;
                slot->field_5C = 0x2710;
            } else if (mem->field_22 < 0x3C) {
                base->field_0  = 2;
                slot->field_58 = 0x190;
                slot->field_5C = 0x190;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                mem->field_10  = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->field_12  = 0x80;
                mem->field_14  = 0;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                eff            = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x100, (SVECTOR*)&mem->field_10);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
            }
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80132D4C);

void func_actor_510900_801332EC(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    POLY_FT4*         prim;
    s16               flag;
    s16               x;
    s32               amt;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
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
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1 & 0xFFF) {
                    amt = (u16)arg0->spawnArg1 & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->field_24 = amt;
                if (arg0->spawnArg1 & 0x10000) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_28 = ((u32)Gp_LcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            prim->tpage = 0x2B;
            prim->code |= 3;
            amt         = mem->field_22;
            prim->clut  = (amt & 0x3F) | 0x43C0;
            amt         = mem->field_22;
            prim->v0    = 0x70;
            prim->u0    = amt * 32;
            amt         = mem->field_22;
            prim->v1    = 0x70;
            prim->u1    = amt * 32 + 0x1F;
            amt         = mem->field_22;
            prim->v2    = 0x9F;
            prim->u2    = amt * 32;
            amt         = mem->field_22;
            prim->v3    = 0x9F;
            prim->u3    = amt * 32 + 0x1F;
            block->dx   = (mem->field_24 * 31) / block->otz;
            block->dy   = (mem->field_24 * 47) / block->otz;
            x           = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        x = mem->field_28;
        if (x != 0) {
            coord->flg         = 0;
            coord->coord.t[1] -= x;
        }
        mem->field_22++;
        if (mem->field_22 < 8) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_8013371C(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    POLY_FT4*         prim;
    s16               flag;
    s16               x;
    s32               amt;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
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
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1 & 0xFFF) {
                    amt = (u16)arg0->spawnArg1 & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->field_24 = amt;
                if (mem->field_28 = (u16)((u32)arg0->spawnArg1 >> 16) & 1) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_28 = ((u32)Gp_LcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            prim->tpage = 0x2B;
            prim->code |= 3;
            amt         = mem->field_22;
            prim->clut  = ((amt + 8) & 0x3F) | 0x43C0;
            x           = mem->field_22;
            prim->u0    = (s16)(x % 8) * 32;
            x           = mem->field_22;
            prim->v0    = (x / 8) * 48 - 0x60;
            x           = mem->field_22;
            prim->u1    = (s16)(x % 8) * 32 + 0x1F;
            x           = mem->field_22;
            prim->v1    = (x / 8) * 48 - 0x60;
            x           = mem->field_22;
            prim->u2    = (s16)(x % 8) * 32;
            x           = mem->field_22;
            prim->v2    = (x / 8) * 48 - 0x31;
            x           = mem->field_22;
            prim->u3    = (s16)(x % 8) * 32 + 0x1F;
            x           = mem->field_22;
            prim->v3    = (x / 8) * 48 - 0x31;
            block->dx   = (mem->field_24 * 31) / block->otz;
            block->dy   = (mem->field_24 * 47) / block->otz;
            block->dx >>= mem->field_28;
            x           = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        if (mem->field_28 != 0) {
            coord->flg         = 0;
            coord->coord.t[1] += 0x38;
        }
        mem->field_22++;
        if (mem->field_22 < 12) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_80133C84(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    POLY_FT4*         prim;
    s16               flag;
    s16               x;
    s32               amt;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
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
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1 & 0xFFF) {
                    amt = (u16)arg0->spawnArg1 & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->field_24 = amt;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_28 = ((u32)Gp_LcgState >> 16) % 0x30;
                arg0->state++;
            }
            prim->tpage = 0x4B;
            prim->code |= 3;
            prim->clut  = 0x4382;
            x           = mem->field_22;
            prim->v0    = 0xD0;
            prim->u0    = (x / 2 + 4) * 32;
            x           = mem->field_22;
            prim->v1    = 0xD0;
            prim->u1    = (x / 2 + 4) * 32 + 0x1F;
            x           = mem->field_22;
            prim->v2    = 0xEF;
            prim->u2    = (x / 2 + 4) * 32;
            x           = mem->field_22;
            prim->v3    = 0xEF;
            prim->u3    = (x / 2 + 4) * 32 + 0x1F;
            block->dx   = (mem->field_24 * 31) / block->otz;
            block->dy   = (mem->field_24 * 31) / block->otz;
            x           = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 != 0) {
            return;
        }
        x = mem->field_28;
        if (x != 0) {
            coord->flg         = 0;
            coord->coord.t[1] -= x;
        }
        mem->field_22++;
        if (mem->field_22 < 8) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_801340E8(Task* arg0)
{
    Actor510900Cam*         base;
    GsCOORDINATE2*          cam;
    Actor510900CamCoord*    ext;
    GpEffWork*              eff;
    Actor510900Coord*       coord;
    Actor510900MatrixWords* mat;
    s32                     i;

    base  = &D_8011505C;
    cam   = &base->cam.coord;
    eff   = arg0->spawnArg2;
    coord = ((Actor510900Obj2C*)arg0->extra)->field_8;
    ext   = (Actor510900CamCoord*)cam;
    if (Gp_State1C->field_4 != 0) {
        Gp_ReleaseState1CMem(eff, arg0);
        return;
    }
    mat                       = (Actor510900MatrixWords*)&coord->field_0.coord;
    coord->field_0.sub        = eff->field_8;
    mat->m00_m01              = 0x1000;
    mat->m02_m10              = 0;
    mat->m11_m12              = 0x1000;
    mat->m20_m21              = 0;
    mat->m22                  = 0x1000;
    coord->field_0.coord.t[0] = eff->field_18;
    coord->field_0.coord.t[1] = eff->field_1A;
    coord->field_0.coord.t[2] = eff->field_1C;
    coord->field_0.flg        = 0;
    Gp_UpdateCoord(&coord->field_0);
    eff->field_10 = -0x200;
    eff->field_12 = 0x40;
    eff->field_14 = 0;
    Gp_SpawnEff(0x6003B, &coord->field_0, 0x180, (SVECTOR*)&eff->field_10);
    for (i = 0; i < 6; i++) {
        Gp_SpawnEff(0x60065, &coord->field_0, 0, (SVECTOR*)&eff->field_10);
        Gp_SpawnEff(0x600A4, &coord->field_0, 1, NULL);
    }
    base->field_0 = 4;
    ext->field_58 = 0xFA0;
    ext->field_5C = 0x12C0;
    ext->rot.vx   = 0xC00;
    ext->rot.vy   = 0x800;
    ext->rot.vz   = 0x400;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->field_0.workm, &cam->coord);
    cam->flg = 0;
    Gp_ReleaseState1CMem(eff, arg0);
}

/// One frame of the trail effect: the coordinate drifts by a per-effect random
/// step, and the segment between last frame's position and this one is drawn as
/// a `LINE_F2` that fades out over `field_24 * 16` frames.
void func_actor_510900_80134284(Task* arg0)
{
    Actor510900TrailScratch* block;
    GpEffWork*               eff;
    GsCOORDINATE2*           coord;
    LINE_F2*                 prim;
    s16                      mode;
    s16                      step;
    s32                      rng;
    s16                      val;
    s16                      count;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor510900TrailScratch);
    block                   = (Actor510900TrailScratch*)*(void**)G_SCRATCH_HEAD;
    eff                     = arg0->spawnArg2;
    mode                    = Gp_State1C->field_4;
    coord                   = &((Actor510900Obj2C*)arg0->extra)->field_8->field_0;
    if (mode != 0) {
        if (mode >= 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        eff->field_10 = 0x20 - ((Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        eff->field_12 = -((Gp_LcgState >> 16) & 0x3F) - 0x10;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        eff->field_14 = 0x20 - ((Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        step          = 2;
        if (((Gp_LcgState >> 16) & 3) != 0) {
            step = 1;
        }
        rng           = Gp_LcgState * 5 + 0x71357911;
        eff->field_24 = step;
        eff->field_26 = (((u32)rng >> 16) & 1) + 1;
        Gp_LcgState   = rng;
        arg0->state++;
    }
    block->vec0.vx     = *(u16*)&coord->workm.t[0];
    block->vec0.vy     = *(u16*)&coord->workm.t[1];
    block->vec0.vz     = *(u16*)&coord->workm.t[2];
    coord->coord.t[0] += eff->field_10;
    coord->coord.t[1] += eff->field_12;
    coord->coord.t[2] += eff->field_14;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    block->vec1.vx = *(u16*)&coord->workm.t[0];
    block->vec1.vy = *(u16*)&coord->workm.t[1];
    block->vec1.vz = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec0);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->vec1);
        gte_rtps_real();
        gte_stsxy(&block->sxy1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            prim           = (LINE_F2*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 3);
            setcode(prim, 0x40);
            val      = 0xFF - (eff->field_22 << (5 - eff->field_24));
            prim->r0 = val;
            prim->g0 = val >> eff->field_26;
            prim->b0 = val >> 3;
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            addPrim((u_long*)(((((u32)((block->otz0 + block->otz1) >> 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz0 + block->otz1) >> 1);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor510900TrailScratch);
    eff->field_12          += 6;
    count                   = eff->field_22 + 1;
    eff->field_22           = count;
    if (count > eff->field_24 * 16 - 1) {
        Gp_ReleaseState1CMem(eff, arg0);
    }
}

void func_actor_510900_801346D4(Task* arg0)
{
    GpEffWork*     eff;
    GsCOORDINATE2* coord;
    s16            mode;

    eff   = arg0->spawnArg2;
    mode  = Gp_State1C->field_4;
    coord = &((Actor510900Obj2C*)arg0->extra)->field_8->field_0;
    if (mode != 0) {
        if (mode >= 4 || arg0->state == 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    eff->field_22++;
    switch (arg0->state) {
        case 0:
            eff = Gp_SpawnEff(0x60184, coord, 0x480, NULL);
            if (eff != NULL) {
                Task_Reparent(arg0, eff->field_0);
            }
            arg0->state++;
            break;
        case 1:
            if (eff->field_22 >= 9) {
                arg0->state++;
            }
            break;
        case 2:
            Gp_SpawnEff(0x60070, coord, 0x82004400, NULL);
            if (eff->field_22 >= 0x33) {
                arg0->state++;
            }
            break;
        case 3:
            Gp_SpawnEff(0x60070, coord, 0xD2004400, NULL);
            if (eff->field_22 >= 0x3D) {
                arg0->state++;
            }
            break;
        case 4:
            Gp_ReleaseState1CMem(eff, arg0);
            break;
    }
}

void func_actor_510900_8013482C(Task* arg0)
{
    GpEffWork*     eff;
    GpEffWork*     spawned;
    GsCOORDINATE2* coord;
    s16            mode;
    s16            scale;
    s16            step;
    s32            tmp;
    s32            i;
    s32            n;

    eff   = arg0->spawnArg2;
    mode  = Gp_State1C->field_4;
    coord = &((Actor510900Obj2C*)arg0->extra)->field_8->field_0;
    if (mode != 0) {
        if (mode >= 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    eff->field_22++;
    if (arg0->state == 0) {
        scale = 0x300;
        if (arg0->spawnArg1 & 0xFFF) {
            scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        }
        eff->field_24 = scale;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        eff->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1 & 0xF000) {
            step = (arg0->spawnArg1 >> 12) & 0xF;
        } else {
            step = 2;
        }
        eff->field_28 = step;
        eff->field_2A = (s32)(*(u16*)&eff->field_24 << 16) >> 23;
        tmp           = ((GpEffSpawnArgHi*)&arg0->spawnArg1)->field_3;
        eff->field_20 = tmp & 0xF;
        if (eff->field_20 != 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            eff->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            eff->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            eff->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            gte_lddp(eff->field_24 << 3);
            gte_ldsv((SVECTOR*)&eff->field_10);
            gte_gpf12_real();
            gte_stsv((SVECTOR*)&eff->field_10);
            gte_lddp(eff->field_20 << 12);
            gte_ldsv((SVECTOR*)&eff->field_10);
            gte_gpf12_real();
            gte_stsv((SVECTOR*)&eff->field_10);
            gte_SetRotMatrix(&eff->field_8->coord);
            gte_ldv0((SVECTOR*)&eff->field_10);
            gte_rtv0_real();
            gte_stsv((SVECTOR*)&eff->field_10);
        } else if (!(arg0->spawnArg1 & 0xF0000000)) {
            n = D_80070F70 & 3;
            i = 0;
            if (n != 0) {
                do {
                    spawned = Gp_SpawnEff(0x60184, coord, ((s32)(*(u16*)&eff->field_24 << 16) >> 17) | 0x02001000, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->field_0);
                    }
                    i += 1;
                } while (i < n);
            }
            n = D_80070F70 & 1;
            i = 0;
            if (i < n) {
                do {
                    spawned = Gp_SpawnEff(0x60184, coord, ((s32)(*(u16*)&eff->field_24 << 16) >> 17) | 0x01002000, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->field_0);
                    }
                    i += 1;
                } while (i < n);
            }
        }
        eff->field_22--;
        arg0->state = 1;
    }
    func_actor_510900_80134C90(coord, eff->field_22 / eff->field_28, eff->field_24, eff->field_26);
    coord->coord.t[0] += eff->field_10;
    coord->coord.t[1] += eff->field_12;
    coord->coord.t[2] += eff->field_14;
    coord->flg         = 0;
    eff->field_24     += eff->field_2A;
    if (eff->field_22 > eff->field_28 * 11 - 1) {
        Gp_ReleaseState1CMem(eff, arg0);
    }
}

/// Draws one frame of the muzzle flash: a single textured `POLY_FT4`
/// billboarded on the effect coordinate's world point. `arg1` walks the twelve
/// sprite frames, each with its own CLUT in `D_actor_510900_8013C48C` and its
/// own texture window in `D_80111E48`; `arg2` is the flare's half-width
/// (divided down by the projected depth) and `arg3` its spin, so the quad is a
/// square rotated by `arg3` rather than an axis-aligned sprite. `otz` is
/// biased by one before it is used as the divisor so a point on the near plane
/// cannot divide by zero.
void func_actor_510900_80134C90(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**                  scratch;
    u8*                     head;
    Actor510900QuadScratch* block;
    Actor510900QuadScratch* vecp;
    POLY_FT4*               prim;
    GpEffUv8*               rec;
    s32                     a;
    u16                     vz;

    scratch                                          = (void**)G_SCRATCH_HEAD;
    head                                             = *scratch;
    ((Actor510900QuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                            = (Actor510900QuadScratch*)(head - 0x1C);
    block->vec.vy                                    = *(u16*)&arg0->workm.t[1];
    vz                                               = *(u16*)&arg0->workm.t[2];
    *scratch                                         = block;
    block->vec.vz                                    = vz;
    vecp                                             = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((Actor510900QuadScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((Actor510900QuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((Actor510900QuadScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = (D_actor_510900_8013C48C[arg1].clutY << 6) |
                     ((D_actor_510900_8013C48C[arg1].clutX >> 4) & 0x3F);
        rec       = &D_80111E48[arg1];
        prim->u0  = rec->u;
        prim->v0  = rec->v;
        prim->u1  = rec->u + 0x27;
        prim->v1  = rec->v;
        prim->u2  = rec->u;
        prim->v2  = rec->v + 0x27;
        prim->u3  = rec->u + 0x27;
        prim->v3  = rec->v + 0x27;
        a         = arg3;
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        a         = a + 0x400;
        prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Spawn/setup handler. It allocates the 0x5C8-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate, pair
/// source and HP (`field_40`, seeded from the pair source's `field_4`).
///
/// The block's 0x14-prefix then becomes the `GpAnimCtx`: `func_800B3F84` loads
/// the animation data into it over the nineteen `GpAnimSlot`s, and slots 1..18
/// are reset. Six enemies are spawned from `D_actor_510900_80167A18`; entries 2
/// and 3 are the two whose models get the current room's texture page and CLUT
/// row (`Gp_GetNestedAreaRec`, indexed by the context id's top nibble) and whose
/// tasks are kept in `field_568` / `field_56C`. Entry 2 also gets an effect
/// reparented onto this task.
///
/// The three list nodes at 0x47C / 0x4E4 / 0x504 are linked into the global
/// object lists with their collision tables (`Gp_InitRec18Table`), which also
/// sets each node's 0x8000 "last element" flag -- kept for the first node and
/// cleared again for the other two.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
void func_actor_510900_801350F8(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Obj2C* obj;
    Actor510900Coord* coord;
    Actor510900Work*  work;
    GpEnemy*          spawned;
    GpEffWork*        eff;
    u32               raw1;
    u32               raw2;
    u32               index1;
    u32               index2;
    TmdObject*        model1;
    TmdObject*        model2;
    GpCdRec10*        entry1;
    GpCdRec10*        entry2;
    GpAreaKey         key;
    GpAreaKey*        sessionKey1;
    GpAreaKey*        sessionKey2;
    GpRec18*          records1;
    GpRec18*          records2;
    u8                areaByte0;
    s32               i;

    obj   = arg1->field_2C;
    coord = obj->field_8;
    work  = memCalloc(sizeof(Actor510900Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy((GpEnemy*)arg0, (Task*)arg1);
        return;
    }
    arg1->field_1C     = work;
    obj->field_C       = 0x80;
    coord->field_0.flg = 0;
    obj->field_1C      = &work->field_45C;
    obj->field_20      = &work->field_43C;
    arg0->field_4      = &coord->field_0.coord;
    arg0->field_48     = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18  = &((TmdObject*)arg1->field_2C)->coords[3];
    arg0->field_1C  = 0;
    arg0->field_20  = 0;
    arg0->field_24  = 0;
    arg0->field_50  = &D_actor_510900_80167980;
    arg0->field_54  = (s32)work->rec49C;
    arg0->field_40  = D_actor_510900_80167980.field_4;
    work->field_53C = &((TmdObject*)arg1->field_2C)->coords[3];
    work->field_540 = 0x400;
    work->field_542 = 2;
    func_800B3F84((GpAnimCtx*)work, D_actor_510900_80167AA4, (GpAnimObj*)obj,
                  ((Actor510900Anim*)work)->poses, ((Actor510900Anim*)work)->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    work->field_592 = 1;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 1, 0, (GpEnemy*)arg0);
    spawned     = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 2, 0, (GpEnemy*)arg0);
    raw1        = arg0->field_8;
    model1      = spawned->task->extra;
    sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage   = sessionKey1->stage;
    key.area    = sessionKey1->area;
    index1      = raw1 >> 12;
    key.room    = sessionKey1->room;
    areaByte0   = gGameSession->at4.loc.view;
    key.view    = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    entry1        = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model1->tpage = entry1->field_D;
    model1->clut  = entry1->field_E;
    if (model1->buffer != NULL) {
        Tmd_ProcessStream(model1);
        Tmd_ProcessStream(model1);
    }
    work->field_568 = spawned->task;
    eff             = Gp_SpawnEff(0x80060043, ((TmdObject*)spawned->task->extra)->coords, 0, NULL);
    if (eff != NULL) {
        work->field_564 = (s32*)eff->field_0;
        Task_Reparent((Task*)arg1, eff->field_0);
    }
    if (work->field_564 != NULL) {
        work->field_564[0xD] = 0;
    }
    spawned     = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 3, 0, (GpEnemy*)arg0);
    raw2        = arg0->field_8;
    model2      = spawned->task->extra;
    sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage   = sessionKey2->stage;
    key.area    = sessionKey2->area;
    index2      = raw2 >> 12;
    key.room    = sessionKey2->room;
    areaByte0   = gGameSession->at4.loc.view;
    key.view    = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    entry2        = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model2->tpage = entry2->field_D;
    model2->clut  = entry2->field_E;
    if (model2->buffer != NULL) {
        Tmd_ProcessStream(model2);
        Tmd_ProcessStream(model2);
    }
    work->field_56C = spawned->task;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 0, (GpEnemy*)arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 1, (GpEnemy*)arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 2, (GpEnemy*)arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 6, 0, (GpEnemy*)arg0);
    work->obj47C.coord    = &((TmdObject*)arg1->field_2C)->coords[3];
    records1              = work->rec49C;
    work->obj47C.ctx.recs = records1;
    work->obj47C.pos.vx   = 0;
    work->obj47C.pos.vy   = 0;
    work->obj47C.pos.vz   = 0;
    work->obj47C.key      = 0x3001B;
    work->obj47C.radius   = 0x1C2;
    work->obj47C.flags    = 1;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(records1, 3, 0);
    records2              = work->rec524;
    work->obj47C.flags   |= 0x8000;
    work->obj4E4.coord    = ((TmdObject*)work->field_568->extra)->coords;
    work->obj4E4.ctx.recs = records2;
    work->obj4E4.pos.vx   = -0x140;
    work->obj4E4.pos.vy   = 0x80;
    work->obj4E4.pos.vz   = 0;
    work->obj4E4.key      = 0;
    work->obj4E4.radius   = 0x190;
    work->obj4E4.flags    = 1;
    Gp_LinkObj(3, &work->obj4E4);
    Gp_InitRec18Table(records2, 1, 0);
    work->obj4E4.flags   &= 0x7FFF;
    work->obj504.coord    = &((TmdObject*)arg1->field_2C)->coords[7];
    work->obj504.ctx.recs = records2;
    work->obj504.pos.vx   = 0;
    work->obj504.pos.vy   = 0;
    work->obj504.pos.vz   = 0;
    work->obj504.key      = 0;
    work->obj504.radius   = 0x190;
    work->obj504.flags    = 1;
    Gp_LinkObj(3, &work->obj504);
    work->obj504.flags &= 0x7FFF;
    arg1->field_24      = &D_actor_510900_80167A6C;
    arg1->exitCallback  = (TaskFunc)func_actor_510900_8013B608;
    func_actor_510900_8013B524(arg1);
    func_actor_510900_8013B424(1);
    arg1->state = 1;
}

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Coord* coord;
    Actor510900Work*  work;
    s32               snd;
    s32               pan;
    s32               pan2;
    s32               i;

    work             = arg1->field_1C;
    coord            = arg1->field_2C->field_8;
    arg0->node.flags = 1;
    if (work->field_586 == 0x20 && work->field_58A == 0xD2) {
        work->field_594 = 1;
        work->field_598 = 0xFF;
        snd             = (((u16)arg0->field_8 >> 0xC) << 8) | 0x4078000E;
        pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        work->field_580 = (((u16)arg0->field_8 >> 0xC) << 8) | 0x40780011;
        pan2            = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(work->field_580, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    work->field_58A++;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
    coord->field_0.flg = 0;
    Gp_UpdateCoord(&coord->field_0);
    func_actor_510900_8013BC38(arg1, coord);
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}
