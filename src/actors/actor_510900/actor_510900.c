#include "common.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"

/// 0x24-byte scratch `func_actor_510900_80134284` takes from `G_SCRATCH_HEAD`
/// to draw one frame of the debris trail. `vec0` is the effect coordinate's
/// `workm.t[]` before the per-frame drift is added and `vec1` the same after,
/// so the two `RTPS` projections give the ends of the trail `LINE_F2`.
/// `otz0` / `otz1` receive `gte_stszotz` for each end and their mean picks the
/// OT bucket; `flag` is the shared `gte_stflg` the projections are dropped on.
typedef struct Actor510900TrailScratch {
    /* 0x00 */ SVECTOR vec0;
    /* 0x08 */ SVECTOR vec1;
    /* 0x10 */ s32     otz0;
    /* 0x14 */ s32     otz1;
    /* 0x18 */ s32     flag;
    /* 0x1C */ DVECTOR sxy0;
    /* 0x20 */ DVECTOR sxy1;
} Actor510900TrailScratch;
STATIC_ASSERT_SIZEOF(Actor510900TrailScratch, 0x24);

/// One VRAM CLUT coordinate per frame of the muzzle-flash sprite, packed the
/// way `getClut` takes them. `D_actor_510900_8013C48C` holds twelve, one for
/// each frame `D_80111E48` supplies the texture window for.
typedef struct Actor510900SprClut {
    /* 0x0 */ u16 clutX;
    /* 0x2 */ u16 clutY;
} Actor510900SprClut;
STATIC_ASSERT_SIZEOF(Actor510900SprClut, 4);

void Gp_DrawEffSprite7C(GsCOORDINATE2* arg0, s32 arg1, u32 arg2);

void func_actor_510900_80134C90(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);

extern s32 D_80070F70;

/// The twelve muzzle-flash CLUTs `func_actor_510900_80134C90` indexes by frame.
extern Actor510900SprClut D_actor_510900_8013C48C[];

void func_actor_510900_80131F24(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpPointLight*  slot;
    GpCoord64*     base;
    GpEffWork*     eff;
    GpMtxWords*    mat;
    s32            i;
    s32            bits;
    s32            z;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    base  = &Gp_RoomCoords[2];
    slot  = &base->data.light;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            base->framesLeft = 0;
        }
        if (arg0->spawnArg1 == 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        mat               = (GpMtxWords*)&coord->coord;
        coord->sub        = mem->parent;
        mat->m00_m01      = 0x1000;
        mat->m02_m10      = 0;
        mat->m11_m12      = 0x1000;
        mat->m20_m21      = 0;
        mat->m22          = 0x1000;
        coord->coord.t[0] = mem->pos.vx;
        coord->coord.t[1] = mem->pos.vy;
        z                 = mem->pos.vz;
        coord->flg        = 0;
        coord->coord.t[2] = z;
        arg0->state       = 1;
    }
    Gp_UpdateCoord(coord);
    if (base->framesLeft != 0) {
        slot->head.r = 0x1000;
        slot->head.g = 0x800;
        slot->head.b = 0x400;
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        base->framesLeft--;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &base->data.coord.coord);
        base->data.coord.flg = 0;
        if (base->framesLeft == 0) {
            arg0->spawnArg1 = 0;
            mem->age        = 0;
            mem->scale      = 0;
        }
    }
    switch (arg0->spawnArg1) {
        case 0:
            break;
        case 1:
            mem->scale = (mem->scale < 0x100) ? mem->scale + 0x10 : 0x100;
            for (i = 0; i < 2; i++) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x2C0) - 0x80;
                mem->move.vy = 0x40;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + mem->scale, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            base->framesLeft = 0x10;
            slot->inner      = 0x1F40;
            slot->outer      = 0x2710;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            bits             = Gp_LcgState >> 16;
            /* The `field_24 + 0x10000` sums below are evaluated as their own
             * operand. Written plainly, `fold` reassociates the constant onto
             * the draw; held in a local, sched1 moves the load ahead of it. */
            if (!(bits & 3)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0x40;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + ({ mem->scale + 0x10000; }),
                                           &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x6004C, coord, ((Gp_LcgState >> 16) & 0xF0) + ({ mem->scale + 0x10000; }),
                                           &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + mem->scale, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (bits % 3 == 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0x80;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = -0x80;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x180, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            break;
        case 2:
            base->framesLeft = 0x10;
            slot->inner      = 0x1F40;
            slot->outer      = 0x2710;
            for (i = 0; i < 3; i++) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0x40;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x10100, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            bits        = Gp_LcgState >> 16;
            if (!(bits & 7)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x100, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = 0x80;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 7)) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy = -0x80;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                eff          = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x180, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            break;
        case 3:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            bits        = Gp_LcgState >> 16;
            mem->age++;
            if (mem->age < 0x1E) {
                if (!(bits & 7)) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = -((Gp_LcgState >> 16) % 0x2C0) - 0x80;
                    mem->move.vy = 0x40;
                    mem->move.vz = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    eff          = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x80, &mem->move);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                bits >>= 1;
                if (!(bits & 3)) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy = 0x40;
                    mem->move.vz = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    eff          = Gp_SpawnEff(0x60045, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                for (i = 0; i < 2; i++) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy = 0;
                    mem->move.vz = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    eff          = Gp_SpawnEff(0x6004C, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x10100, &mem->move);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                bits >>= 1;
                if (!(bits & 7)) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy = 0;
                    mem->move.vz = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    eff          = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x100, &mem->move);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                bits >>= 1;
                if (!(bits & 7)) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy = 0x80;
                    mem->move.vz = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    eff          = Gp_SpawnEff(0x60052, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                for (i = 0; i < 2; i++) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy = -0x80;
                    mem->move.vz = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    eff          = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) + 0x180, &mem->move);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                base->framesLeft = 0x10;
                slot->inner      = 0x1F40;
                slot->outer      = 0x2710;
            } else if (mem->age < 0x3C) {
                base->framesLeft = 2;
                slot->inner      = 0x190;
                slot->outer      = 0x190;
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx     = -((Gp_LcgState >> 16) % 0x280) - 0x80;
                mem->move.vy     = 0x80;
                mem->move.vz     = 0;
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                eff              = Gp_SpawnEff(0x60059, coord, ((Gp_LcgState >> 16) & 0xF0) | 0x100, &mem->move);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
            }
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

void func_actor_510900_80132D4C(Task* arg0)
{
    GsCOORDINATE2    hit;
    GpEffWork*       mem;
    GsCOORDINATE2*   coord;
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s16              flag;
    s16              x;
    u8               col;
    u16              vz;
    u32              vy;
    u8*              carve;
    s32              x2;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        scratch                                   = (void**)G_SCRATCH_HEAD;
        head                                      = *scratch;
        ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        carve                                     = head - 0x1C;
        vy                                        = *(u16*)&coord->workm.t[1];
        SOFT_TOUCH_REG_USE2(vy, carve, carve);
        block         = (GpFxQuadScratch*)carve;
        block->vec.vy = vy;
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
        gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                mem->scale  = (u16)arg0->spawnArg1 & 0xFFF;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->angle  = ((u32)Gp_LcgState >> 16) & 0xF;
                if (arg0->spawnArg1 & 0x10000) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    mem->period = ((u32)Gp_LcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            if (mem->angle - 8 < mem->age) {
                x2 = (mem->angle - mem->age + 1) * 16;
                __asm__ volatile("" : "=r"(col) : "0"(x2));
                prim->r0 = x2;
                prim->g0 = x2;
                prim->b0 = x2;
            } else {
                col         = 0x80;
                prim->code |= 1;
            }
            prim->tpage = 0x2B;
            prim->clut  = 0x4380;
            prim->code |= 2;
            prim->u0    = (mem->age % 6) * 32;
            prim->v0    = 0;
            prim->u1    = (mem->age % 6) * 32 + 0x1F;
            prim->v1    = 0;
            prim->u2    = (mem->age % 6) * 32;
            prim->v2    = 0x27;
            prim->u3    = (mem->age % 6) * 32 + 0x1F;
            prim->v3    = 0x27;
            block->dx   = (mem->scale * 31) / block->otz;
            block->dy   = (mem->scale * 39) / block->otz;
            x           = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            if (coord->coord.t[1] < 0 && (mem->age & 1)) {
                hit.sub        = coord->sub;
                hit.coord.t[0] = coord->coord.t[0];
                hit.coord.t[1] = 0;
                hit.coord.t[2] = coord->coord.t[2];
                hit.flg        = 0;
                Gp_UpdateCoord(&hit);
                Gp_DrawEffSprite7C(&hit, (s32)(*(u16*)&mem->scale << 16) >> 17, (u8)col);
            }
        }
        SCRATCH_POP_BYTES(0x1C);
        if (Gp_State1C->eventState != 0) {
            return;
        }
        x = mem->period;
        if (x != 0) {
            coord->flg         = 0;
            coord->coord.t[1] -= x;
        }
        mem->age++;
        if (mem->angle >= mem->age) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_801332EC(Task* arg0)
{
    GpEffWork*       mem;
    GsCOORDINATE2*   coord;
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s16              flag;
    s16              x;
    s32              amt;
    u16              vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                   = (void**)G_SCRATCH_HEAD;
        head                                      = *scratch;
        ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x1C;
            block = (GpFxQuadScratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
        gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1 & 0xFFF) {
                    amt = (u16)arg0->spawnArg1 & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->scale = amt;
                if (arg0->spawnArg1 & 0x10000) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    mem->period = ((u32)Gp_LcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            prim->tpage = 0x2B;
            prim->code |= 3;
            amt         = mem->age;
            prim->clut  = (amt & 0x3F) | 0x43C0;
            amt         = mem->age;
            prim->v0    = 0x70;
            prim->u0    = amt * 32;
            amt         = mem->age;
            prim->v1    = 0x70;
            prim->u1    = amt * 32 + 0x1F;
            amt         = mem->age;
            prim->v2    = 0x9F;
            prim->u2    = amt * 32;
            amt         = mem->age;
            prim->v3    = 0x9F;
            prim->u3    = amt * 32 + 0x1F;
            block->dx   = (mem->scale * 31) / block->otz;
            block->dy   = (mem->scale * 47) / block->otz;
            x           = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        SCRATCH_POP_BYTES(0x1C);
        if (Gp_State1C->eventState != 0) {
            return;
        }
        x = mem->period;
        if (x != 0) {
            coord->flg         = 0;
            coord->coord.t[1] -= x;
        }
        mem->age++;
        if (mem->age < 8) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_8013371C(Task* arg0)
{
    GpEffWork*       mem;
    GsCOORDINATE2*   coord;
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s16              flag;
    s16              x;
    s32              amt;
    u16              vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                   = (void**)G_SCRATCH_HEAD;
        head                                      = *scratch;
        ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x1C;
            block = (GpFxQuadScratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
        gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1 & 0xFFF) {
                    amt = (u16)arg0->spawnArg1 & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->scale = amt;
                if (mem->period = (u16)((u32)arg0->spawnArg1 >> 16) & 1) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    mem->period = ((u32)Gp_LcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            prim->tpage = 0x2B;
            prim->code |= 3;
            amt         = mem->age;
            prim->clut  = ((amt + 8) & 0x3F) | 0x43C0;
            x           = mem->age;
            prim->u0    = (s16)(x % 8) * 32;
            x           = mem->age;
            prim->v0    = (x / 8) * 48 - 0x60;
            x           = mem->age;
            prim->u1    = (s16)(x % 8) * 32 + 0x1F;
            x           = mem->age;
            prim->v1    = (x / 8) * 48 - 0x60;
            x           = mem->age;
            prim->u2    = (s16)(x % 8) * 32;
            x           = mem->age;
            prim->v2    = (x / 8) * 48 - 0x31;
            x           = mem->age;
            prim->u3    = (s16)(x % 8) * 32 + 0x1F;
            x           = mem->age;
            prim->v3    = (x / 8) * 48 - 0x31;
            block->dx   = (mem->scale * 31) / block->otz;
            block->dy   = (mem->scale * 47) / block->otz;
            block->dx >>= mem->period;
            x           = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        SCRATCH_POP_BYTES(0x1C);
        if (Gp_State1C->eventState != 0) {
            return;
        }
        if (mem->period != 0) {
            coord->flg         = 0;
            coord->coord.t[1] += 0x38;
        }
        mem->age++;
        if (mem->age < 12) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_80133C84(Task* arg0)
{
    GpEffWork*       mem;
    GsCOORDINATE2*   coord;
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s16              flag;
    s16              x;
    s32              amt;
    u16              vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        scratch                                   = (void**)G_SCRATCH_HEAD;
        head                                      = *scratch;
        ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        {
            register u8* tmp asm("v0");
            tmp   = head - 0x1C;
            block = (GpFxQuadScratch*)tmp;
        }
        block->vec.vy = *(u16*)&coord->workm.t[1];
        vz            = *(u16*)&coord->workm.t[2];
        *scratch      = block;
        block->vec.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
        gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1 & 0xFFF) {
                    amt = (u16)arg0->spawnArg1 & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->scale  = amt;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->period = ((u32)Gp_LcgState >> 16) % 0x30;
                arg0->state++;
            }
            prim->tpage = 0x4B;
            prim->code |= 3;
            prim->clut  = 0x4382;
            x           = mem->age;
            prim->v0    = 0xD0;
            prim->u0    = (x / 2 + 4) * 32;
            x           = mem->age;
            prim->v1    = 0xD0;
            prim->u1    = (x / 2 + 4) * 32 + 0x1F;
            x           = mem->age;
            prim->v2    = 0xEF;
            prim->u2    = (x / 2 + 4) * 32;
            x           = mem->age;
            prim->v3    = 0xEF;
            prim->u3    = (x / 2 + 4) * 32 + 0x1F;
            block->dx   = (mem->scale * 31) / block->otz;
            block->dy   = (mem->scale * 31) / block->otz;
            x           = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->x2    = x;
            prim->x0    = x;
            x           = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3    = x;
            prim->x1    = x;
            x           = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y1    = x;
            prim->y0    = x;
            x           = *(u16*)&block->sy + *(u16*)&block->dy;
            prim->y3    = x;
            prim->y2    = x;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        SCRATCH_POP_BYTES(0x1C);
        if (Gp_State1C->eventState != 0) {
            return;
        }
        x = mem->period;
        if (x != 0) {
            coord->flg         = 0;
            coord->coord.t[1] -= x;
        }
        mem->age++;
        if (mem->age < 8) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_actor_510900_801340E8(Task* arg0)
{
    GpCoord64*     base;
    GsCOORDINATE2* cam;
    GpPointLight*  ext;
    GpEffWork*     eff;
    GsCOORDINATE2* coord;
    GpMtxWords*    mat;
    s32            i;

    base  = &Gp_RoomCoords[3];
    cam   = &base->data.coord;
    eff   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    ext   = &base->data.light;
    if (Gp_State1C->eventState != 0) {
        Gp_ReleaseState1CMem(eff, arg0);
        return;
    }
    mat               = (GpMtxWords*)&coord->coord;
    coord->sub        = eff->parent;
    mat->m00_m01      = 0x1000;
    mat->m02_m10      = 0;
    mat->m11_m12      = 0x1000;
    mat->m20_m21      = 0;
    mat->m22          = 0x1000;
    coord->coord.t[0] = eff->pos.vx;
    coord->coord.t[1] = eff->pos.vy;
    coord->coord.t[2] = eff->pos.vz;
    coord->flg        = 0;
    Gp_UpdateCoord(coord);
    eff->move.vx = -0x200;
    eff->move.vy = 0x40;
    eff->move.vz = 0;
    Gp_SpawnEff(0x6003B, coord, 0x180, &eff->move);
    for (i = 0; i < 6; i++) {
        Gp_SpawnEff(0x60065, coord, 0, &eff->move);
        Gp_SpawnEff(0x600A4, coord, 1, NULL);
    }
    base->framesLeft = 4;
    ext->inner       = 0xFA0;
    ext->outer       = 0x12C0;
    ext->head.r      = 0xC00;
    ext->head.g      = 0x800;
    ext->head.b      = 0x400;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &cam->coord);
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

    SCRATCH_PUSH_BYTES(sizeof(Actor510900TrailScratch));
    block = (Actor510900TrailScratch*)SCRATCH_HEAD(void);
    eff   = arg0->spawnArg2;
    mode  = Gp_State1C->eventState;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (mode != 0) {
        if (mode >= 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        eff->move.vx = 0x20 - ((Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        eff->move.vy = -((Gp_LcgState >> 16) & 0x3F) - 0x10;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        eff->move.vz = 0x20 - ((Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        step         = 2;
        if (((Gp_LcgState >> 16) & 3) != 0) {
            step = 1;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        eff->scale  = step;
        eff->angle  = (((u32)rng >> 16) & 1) + 1;
        Gp_LcgState = rng;
        arg0->state++;
    }
    block->vec0.vx     = *(u16*)&coord->workm.t[0];
    block->vec0.vy     = *(u16*)&coord->workm.t[1];
    block->vec0.vz     = *(u16*)&coord->workm.t[2];
    coord->coord.t[0] += eff->move.vx;
    coord->coord.t[1] += eff->move.vy;
    coord->coord.t[2] += eff->move.vz;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    block->vec1.vx = *(u16*)&coord->workm.t[0];
    block->vec1.vy = *(u16*)&coord->workm.t[1];
    block->vec1.vz = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec0);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->vec1);
        gte_rtps();
        gte_stsxy(&block->sxy1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            prim           = (LINE_F2*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 3);
            setcode(prim, 0x40);
            val      = 0xFF - (eff->age << (5 - eff->scale));
            prim->r0 = val;
            prim->g0 = val >> eff->angle;
            prim->b0 = val >> 3;
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            addPrim((u_long*)(((((u32)((block->otz0 + block->otz1) >> 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz0 + block->otz1) >> 1);
        }
    }
    SCRATCH_POP_BYTES(sizeof(Actor510900TrailScratch));
    eff->move.vy += 6;
    count         = eff->age + 1;
    eff->age      = count;
    if (count > eff->scale * 16 - 1) {
        Gp_ReleaseState1CMem(eff, arg0);
    }
}

void func_actor_510900_801346D4(Task* arg0)
{
    GpEffWork*     eff;
    GsCOORDINATE2* coord;
    s16            mode;

    eff   = arg0->spawnArg2;
    mode  = Gp_State1C->eventState;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (mode != 0) {
        if (mode >= 4 || arg0->state == 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    eff->age++;
    switch (arg0->state) {
        case 0:
            eff = Gp_SpawnEff(0x60184, coord, 0x480, NULL);
            if (eff != NULL) {
                Task_Reparent(arg0, eff->task);
            }
            arg0->state++;
            break;
        case 1:
            if (eff->age >= 9) {
                arg0->state++;
            }
            break;
        case 2:
            Gp_SpawnEff(0x60070, coord, 0x82004400, NULL);
            if (eff->age >= 0x33) {
                arg0->state++;
            }
            break;
        case 3:
            Gp_SpawnEff(0x60070, coord, 0xD2004400, NULL);
            if (eff->age >= 0x3D) {
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
    mode  = Gp_State1C->eventState;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (mode != 0) {
        if (mode >= 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    eff->age++;
    if (arg0->state == 0) {
        scale = 0x300;
        if (arg0->spawnArg1 & 0xFFF) {
            scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        }
        eff->scale  = scale;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        eff->angle  = ((u32)Gp_LcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1 & 0xF000) {
            step = (arg0->spawnArg1 >> 12) & 0xF;
        } else {
            step = 2;
        }
        eff->period = step;
        eff->step   = (s32)(*(u16*)&eff->scale << 16) >> 23;
        tmp         = ((GpEffSpawnArgHi*)&arg0->spawnArg1)->field_3;
        eff->index  = tmp & 0xF;
        if (eff->index != 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            eff->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            eff->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            eff->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            gte_lddp(eff->scale << 3);
            gte_ldsv(&eff->move);
            gte_gpf12();
            gte_stsv(&eff->move);
            gte_lddp(eff->index << 12);
            gte_ldsv(&eff->move);
            gte_gpf12();
            gte_stsv(&eff->move);
            gte_SetRotMatrix(&eff->parent->coord);
            gte_ldv0(&eff->move);
            gte_rtv0();
            gte_stsv(&eff->move);
        } else if (!(arg0->spawnArg1 & 0xF0000000)) {
            n = D_80070F70 & 3;
            i = 0;
            if (n != 0) {
                do {
                    spawned = Gp_SpawnEff(0x60184, coord, ((s32)(*(u16*)&eff->scale << 16) >> 17) | 0x02001000, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->task);
                    }
                    i += 1;
                } while (i < n);
            }
            n = D_80070F70 & 1;
            i = 0;
            if (i < n) {
                do {
                    spawned = Gp_SpawnEff(0x60184, coord, ((s32)(*(u16*)&eff->scale << 16) >> 17) | 0x01002000, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->task);
                    }
                    i += 1;
                } while (i < n);
            }
        }
        eff->age--;
        arg0->state = 1;
    }
    func_actor_510900_80134C90(coord, eff->age / eff->period, eff->scale, eff->angle);
    coord->coord.t[0] += eff->move.vx;
    coord->coord.t[1] += eff->move.vy;
    coord->coord.t[2] += eff->move.vz;
    coord->flg         = 0;
    eff->scale        += eff->step;
    if (eff->age > eff->period * 11 - 1) {
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
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    GpEffUv8*        rec;
    s32              a;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        a         = a + 0x400;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Spawn/setup handler. It allocates the 0x5C8-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate, pair
/// source and HP (`field_40`, seeded from the record's `hpMax`).
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
void func_actor_510900_801350F8(GpEnemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor510900Work* work;
    GpEnemy*         spawned;
    GpEffWork*       eff;
    u32              raw1;
    u32              raw2;
    u32              index1;
    u32              index2;
    TmdObject*       model1;
    TmdObject*       model2;
    GpAreaPlace*     entry1;
    GpAreaPlace*     entry2;
    GpAreaKey        key;
    GpAreaKey*       sessionKey1;
    GpAreaKey*       sessionKey2;
    GpRec18*         records1;
    GpRec18*         records2;
    u8               areaByte0;
    s32              i;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor510900Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = work;
    obj->flags     = 0x80;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_45C;
    obj->colorMtx  = &work->field_43C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &((TmdObject*)arg1->extra)->coords[3];
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &D_actor_510900_80167980;
    arg0->recs                 = work->rec49C;
    arg0->hp                   = D_actor_510900_80167980.hpMax;
    work->field_53C.coord      = &((TmdObject*)arg1->extra)->coords[3];
    work->field_53C.spawnArgLo = 0x400;
    work->field_53C.spawnArgHi = 2;
    func_800B3F84((GpAnimCtx*)work, D_actor_510900_80167AA4, (TmdObject*)obj,
                  ((ActorAnimRig19*)work)->poses, ((ActorAnimRig19*)work)->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    work->field_592 = 1;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 1, 0, arg0);
    spawned     = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 2, 0, arg0);
    raw1        = arg0->placeKey;
    model1      = spawned->task->extra;
    sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage   = sessionKey1->stage;
    key.area    = sessionKey1->area;
    index1      = raw1 >> 12;
    key.room    = sessionKey1->room;
    areaByte0   = gGameSession->at4.loc.view;
    key.view    = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model1->tpage = entry1->tpage;
    model1->clut  = entry1->clut;
    if (model1->buffer != NULL) {
        tmdProcessStream(model1);
        tmdProcessStream(model1);
    }
    work->field_568 = spawned->task;
    eff             = Gp_SpawnEff(0x80060043, ((TmdObject*)spawned->task->extra)->coords, 0, NULL);
    if (eff != NULL) {
        work->field_564 = (s32*)eff->task;
        Task_Reparent(arg1, eff->task);
    }
    if (work->field_564 != NULL) {
        work->field_564[0xD] = 0;
    }
    spawned     = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 3, 0, arg0);
    raw2        = arg0->placeKey;
    model2      = spawned->task->extra;
    sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage   = sessionKey2->stage;
    key.area    = sessionKey2->area;
    index2      = raw2 >> 12;
    key.room    = sessionKey2->room;
    areaByte0   = gGameSession->at4.loc.view;
    key.view    = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model2->tpage = entry2->tpage;
    model2->clut  = entry2->clut;
    if (model2->buffer != NULL) {
        tmdProcessStream(model2);
        tmdProcessStream(model2);
    }
    work->field_56C = spawned->task;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 0, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 1, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 2, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 6, 0, arg0);
    work->obj47C.coord    = &((TmdObject*)arg1->extra)->coords[3];
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
    work->obj504.coord    = &((TmdObject*)arg1->extra)->coords[7];
    work->obj504.ctx.recs = records2;
    work->obj504.pos.vx   = 0;
    work->obj504.pos.vy   = 0;
    work->obj504.pos.vz   = 0;
    work->obj504.key      = 0;
    work->obj504.radius   = 0x190;
    work->obj504.flags    = 1;
    Gp_LinkObj(3, &work->obj504);
    work->obj504.flags &= 0x7FFF;
    arg1->msgTable      = &D_actor_510900_80167A6C;
    arg1->exitCallback  = func_actor_510900_8013B608;
    func_actor_510900_8013B524(arg1);
    func_actor_510900_8013B424(1);
    arg1->state = 1;
}

void func_actor_510900_801355B4(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   coord;
    Actor510900Work* work;
    s32              snd;
    s32              pan;
    s32              pan2;
    s32              i;

    work                     = arg1->work;
    coord                    = ((TmdObject*)arg1->extra)->coords;
    arg0->node.state.b.flags = 1;
    if (work->field_586 == 0x20 && work->field_58A == 0xD2) {
        work->field_594 = 1;
        work->field_598 = 0xFF;
        snd             = (((u16)arg0->placeKey >> 0xC) << 8) | 0x4078000E;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        work->field_580 = (((u16)arg0->placeKey >> 0xC) << 8) | 0x40780011;
        pan2            = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(work->field_580, pan2, (s8)gpGetObjDepth(coord));
    }
    work->field_58A++;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_510900_8013BC38(arg1, coord);
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}
