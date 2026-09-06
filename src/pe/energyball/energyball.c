#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/energyball.h"

extern s32 D_80115724;
extern s32 Gp_LcgState;

/// `rtps` / `rtpt` / `mvmva 1,0,0,3,0` / `gpf 12`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Fires the energy ball: on the first frame it picks the charge level from the
/// combo counter, plays the matching loop sound, refills the surface-jitter
/// table and spawns one ball per charge level, fanning them out by 0x555 of
/// yaw each while `D_80115724` (the number of balls already in flight) allows
/// it. Every later frame just releases the work block.
void func_energyball_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    s32            level;
    s32            rng;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_20 = Gp_StateC08.field_0 % 10 - 1;
            level         = mem->field_20;
            mem->field_26 = (level << 8) + 0x300;
            if (D_80115724 < 0) {
                D_80115724 = 0;
            }
            if (D_80115724 == 0) {
                SndEvt_EnqueueType6(D_energyball_8013117C[mem->field_20], 0, 0);
            }
            for (i = 0; i < 0x10; i++) {
                rng                      = Gp_LcgState * 5 + 0x71357911;
                D_energyball_801311A0[i] = ((u32)rng >> 16) & 0xFF;
                Gp_LcgState              = rng;
            }
            for (i = 0; i < mem->field_20 + 1; i++) {
                if (D_80115724 + i >= 3) {
                    break;
                }
                mem->field_24 = i * 0x555 - (s16)(u16)mem->field_20 * 0x2AA;
                mem->field_10 = (mem->field_26 * rsin(mem->field_24)) >> 12;
                mem->field_14 = (mem->field_26 * rcos(mem->field_24)) >> 12;
                Gp_SpawnEff(0x800600F8, coord, i, (SVECTOR*)&mem->field_10);
            }
            arg0->state = 1;
            return;
        case 1:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

/// One ball of the energy ball cast; `spawnArg1` picks the `Gp_RoomCoords`
/// slot it owns and `spawnArg2` the `GpEffWork` block. While the room is
/// fading (`Gp_State1C->field_E`) it only redraws, and drops the ball once the
/// fade passes 4. Otherwise it walks `Task::state`: 0 allocates the
/// `EnergyBallWork` collision block, picks the charge row of
/// `D_energyball_80131194` from the combo counter and seeds a random spin
/// `field_28`; 1 grows the ball by the row's `field_2` per frame until it
/// reaches `field_0`, then links it on list 1 with a random direction; 2
/// flies it, re-aiming at the player every eighth frame and nudging each
/// velocity component by 0x10 on odd frames, bursting into three 0x600F9
/// effects on a hit (`Gp_CountRec18Hi`) or unlinking when the room's
/// `field_16` drops; 3 and 4 fade the burst out, growing to twice the row's
/// size or shrinking below one step. Cancel (`Gp_StateC08.field_3 == -2` or
/// the fade at 4 or more) anywhere but combo 0x2B lets the ball go: the last
/// ball in flight (`D_80115724`) queues the row's stop sound.
void func_energyball_8012F180(Task* arg0)
{
    GpEffWork*      mem;
    GsCOORDINATE2*  coord;
    EnergyBallWork* work;
    GpCoord64*      slot;
    GsCOORDINATE2*  sc;
    GpCoordTail*    tail;
    GsCOORDINATE2   ground;
    VECTOR          vec;
    GsCOORDINATE2*  player;
    GpEffWork*      spawned;
    SVECTOR*        dir;
    u16             r;
    s32*            snd;
    s16             fade;
    s32             cur;

    slot  = &Gp_RoomCoords[arg0->spawnArg1 + 4];
    sc    = &slot->coord;
    tail  = (GpCoordTail*)sc;
    coord = ((TmdObject*)arg0->extra)->field_8;
    fade  = Gp_State1C->field_E;
    work  = (EnergyBallWork*)arg0->idMap;
    mem   = arg0->spawnArg2;
    if (fade != 0) {
        if (fade >= 4) {
            if (D_80115724 > 0) {
                D_80115724 -= 1;
                if (D_80115724 == 0) {
                    SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                }
            }
            if (arg0->state != 0) {
            unlink:
                Gp_UnlinkObj(&work->obj);
            }
            goto release;
        }
        Gp_UpdateCoord(coord);
        func_energyball_8013035C(coord, mem->field_22, mem->field_26, mem->field_28);
        func_energyball_8012FFD0(coord, mem->field_26, (s16)(u16)mem->field_24 >> 2);
        if ((arg0->state < 3) && (Gp_State1C->field_6 != 0) &&
            (Gp_TraceGroundCoord(coord, &ground) == 1)) {
            func_energyball_801307D4(&ground, mem->field_26);
        }
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            work = Mem_Calloc(0x38, 0);
            if (work == NULL) {
                mem->field_22 = 0;
                return;
            }
            arg0->idMap   = (TaskIdMap*)work;
            mem->field_20 = (Gp_StateC08.field_0 % 10) - 1;
            mem->field_10 = 0;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = -(u16)D_energyball_80131194[mem->field_20].field_2;
            mem->field_14 = 0;
            mem->field_26 = 0;
            mem->field_28 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            D_80115724   += 1;
            mem->field_24 = 0xC0;
            mem->field_2A = 0x20;
            arg0->state   = 1;
            /* fallthrough */
        case 1:
            if (mem->field_26 < D_energyball_80131194[mem->field_20].field_0) {
                mem->field_26      = (u16)mem->field_26 + (u16)D_energyball_80131194[mem->field_20].field_2;
                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
            } else {
                Gp_UpdateCoord(coord);
                arg0->idMap        = (TaskIdMap*)work;
                work->obj.field_C  = &work->rec;
                work->obj.field_8  = coord;
                work->obj.field_18 = ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 +
                                     ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 +
                                     (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
                work->obj.field_1C = (s16)(u16)mem->field_26 >> 1;
                work->obj.flags    = 1;
                Gp_LinkObj(1, &work->obj);
                dir               = (SVECTOR*)&mem->field_10;
                work->rec.field_0 = 2;
                work->obj.flags  |= 0x8000;
                arg0->state       = 2;
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                mem->field_10     = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                mem->field_12     = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                mem->field_14     = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                VectorNormalSS(dir, dir);
                gte_lddp((u16)mem->field_2A);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                mem->field_18 = 0;
                mem->field_1A = -(u16)D_energyball_80131194[mem->field_20].field_2;
                mem->field_1C = 0;
            }
            slot->field_0  = 2;
            tail->field_58 = 0x100;
            tail->field_5C = 0x1000;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            r              = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->field_52 = r;
            tail->field_50 = (u16)tail->field_52 >> 1;
            tail->field_54 = tail->field_52 >> 1;
            sc->coord.t[0] = coord->coord.t[0];
            sc->coord.t[1] = coord->coord.t[1];
            sc->coord.t[2] = coord->coord.t[2];
            sc->flg        = 0;
            func_energyball_8013035C(coord, mem->field_22, mem->field_26, mem->field_28);
            func_energyball_8012FFD0(coord, mem->field_26, (s16)(u16)mem->field_24 >> 2);
            if ((Gp_State1C->field_6 != 0) && (Gp_TraceGroundCoord(coord, &ground) == 1)) {
                func_energyball_801307D4(&ground, mem->field_26);
            }
            coord->workm.t[1] += D_energyball_80131194[mem->field_20].field_2 * mem->field_22;
            func_energyball_80130B54(coord, mem->field_26,
                                     (D_energyball_80131194[mem->field_20].field_0 - mem->field_26) / 5);
            coord->workm.t[1] -= D_energyball_80131194[mem->field_20].field_2 * mem->field_22;
            if ((u16)(Gp_StateC08.field_0 / 10) != 0x2B) {
                if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->field_E >= 4)) {
                    if (D_80115724 > 0) {
                        D_80115724 -= 1;
                        if (D_80115724 == 0) {
                            SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                        }
                    }
                    goto unlink;
                }
            }
            return;
        case 2:
            if (((u16)mem->field_22 & 7) == 0) {
                player = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[1];
                vec.vx = player->workm.t[0] - coord->workm.t[0];
                vec.vy = player->workm.t[1] - coord->workm.t[1];
                vec.vz = player->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                mem->field_18 = vec.vx;
                mem->field_1A = vec.vy;
                mem->field_1C = vec.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->field_18);
                gte_rtv0_real();
                gte_stsv(&mem->field_18);
                gte_lddp(mem->field_20 * 0x180 + 0xA00);
                gte_ldsv(&mem->field_10);
                gte_gpf12_real();
                gte_stsv(&mem->field_10);
            }
            if ((u16)mem->field_22 & 1) {
                cur           = mem->field_10;
                mem->field_10 = (cur < mem->field_18) ? cur + 0x10 : cur - 0x10;
                cur           = mem->field_12;
                mem->field_12 = (cur < mem->field_1A) ? cur + 0x10 : cur - 0x10;
                cur           = mem->field_14;
                mem->field_14 = (cur < mem->field_1C) ? cur + 0x10 : cur - 0x10;
            }
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            slot->field_0  = 2;
            tail->field_58 = 0x100;
            tail->field_5C = 0x1000;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            r              = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->field_52 = r;
            tail->field_50 = (u16)tail->field_52 >> 1;
            tail->field_54 = tail->field_52 >> 1;
            sc->coord.t[0] = coord->coord.t[0];
            sc->coord.t[1] = coord->coord.t[1];
            sc->coord.t[2] = coord->coord.t[2];
            sc->flg        = 0;
            func_energyball_8013035C(coord, mem->field_22, mem->field_26, mem->field_28);
            func_energyball_8012FFD0(coord, mem->field_26, (s16)(u16)mem->field_24 >> 2);
            if (Gp_State1C->field_6 != 0) {
                if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                    func_energyball_801307D4(&ground, mem->field_26);
                }
            }
            if ((u16)(Gp_StateC08.field_0 / 10) != 0x2B) {
                if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->field_E >= 4)) {
                    if (D_80115724 > 0) {
                        D_80115724 -= 1;
                        if (D_80115724 == 0) {
                            SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                            SCHED_BARRIER();
                        }
                    }
                    goto unlink;
                }
            }
            if (Gp_CountRec18Hi(work->obj.field_C, 0x30000) != 0) {
                spawned = Gp_SpawnEff(0x600F9, coord, 0, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
                spawned = Gp_SpawnEff(0x600F9, coord, 0x2AA, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
                spawned = Gp_SpawnEff(0x600F9, coord, 0x555, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
                snd = D_energyball_8013117C;
                SndEvt_EnqueueType6(snd[mem->field_20 + 3], 0, 0);
                Gp_UnlinkObj(&work->obj);
                mem->field_26 = (u16)D_energyball_80131194[mem->field_20].field_0;
                arg0->state   = 3;
                return;
            }
            if (Gp_State1C->field_16 != 1) {
                Gp_UnlinkObj(&work->obj);
                arg0->state = 4;
                return;
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 3:
            Gp_UpdateCoord(coord);
            func_energyball_8013035C(coord, mem->field_22, mem->field_26, mem->field_28);
            func_energyball_8012FFD0(coord, mem->field_26, (s16)(u16)mem->field_24 >> 2);
            func_energyball_8012FFD0(coord, (u16)mem->field_26 * 2, (s16)(u16)mem->field_24 >> 2);
            mem->field_26 = (u16)mem->field_26 + (u16)D_energyball_80131194[mem->field_20].field_2;
            if (((u16)(Gp_StateC08.field_0 / 10) != 0x2B) &&
                ((Gp_StateC08.field_3 == -2) || (Gp_State1C->field_E >= 4))) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (D_energyball_80131194[mem->field_20].field_0 * 2 < mem->field_26) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            return;
        case 4:
            Gp_UpdateCoord(coord);
            func_energyball_8013035C(coord, mem->field_22, mem->field_26, mem->field_28);
            func_energyball_8012FFD0(coord, mem->field_26, (s16)(u16)mem->field_24 >> 2);
            func_energyball_8012FFD0(coord, (u16)mem->field_26 * 2, (s16)(u16)mem->field_24 >> 2);
            mem->field_26 = (u16)mem->field_26 - (u16)D_energyball_80131194[mem->field_20].field_2;
            if (((u16)(Gp_StateC08.field_0 / 10) != 0x2B) &&
                ((Gp_StateC08.field_3 == -2) || (Gp_State1C->field_E >= 4))) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (mem->field_26 < D_energyball_80131194[mem->field_20].field_2) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->field_20], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            return;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Overlay copy of `Gp_DrawRing` with a flat tint: draws an eight-segment
/// gouraud ring centred on `arg0`'s world position. The position is projected
/// through `GsWSMATRIX` by one `RTPS` and the ring is dropped when that sets a
/// negative `gte_stflg`. `arg1` is the radius in world units (scaled by 64 and
/// divided by the projected OTZ) and `arg2` the brightness: only the inner
/// vertex of each `POLY_G4` is lit, `(arg2 / 2, arg2, arg2 / 2)`, so every
/// wedge fades from green at the centre to black at the rim. Each wedge gets
/// the semi-transparent tpage of `Gp_AddTpageShift` at its OTZ.
void func_energyball_8012FFD0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    s16            color;
    s32            half;
    u16            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    color   = arg2;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = (arg1 * 64) / block->otz;
        half        = arg2 >> 1;
        ang         = 0;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, half, color, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Links one frame of the energy ball's core sprite at `arg0`'s world
/// position. The position is projected through `GsWSMATRIX` by a single `RTPS`
/// and the quad is dropped when that sets a negative `gte_stflg`. `arg1` is the
/// effect's frame counter and its low bit alternates the two looks: odd frames
/// draw the raw, semi-transparent 0x428B cell, even frames the 0x428C cell
/// tinted `(0x40, 0xC0, 0x60)`. `arg3` spins the quad and `arg2` sizes it: the
/// corners sit `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
void func_energyball_8013035C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**                      scratch;
    u8*                         head;
    EnergyQuadScratch*          block;
    register EnergyQuadScratch* p asm("v0");
    register u16                vx asm("v0");
    POLY_FT4*                   prim;
    s32                         ang2;
    u16                         vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    vx                                          = *(u16*)&arg0->workm.t[0];
    ((EnergyQuadScratch*)(head - 0x1C))->vec.vx = vx;
    p                                           = (EnergyQuadScratch*)(head - 0x1C);
    block                                       = p;
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    block->vec.vz                               = vz;
    *scratch                                    = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((EnergyQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((EnergyQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((EnergyQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        if (arg1 & 1) {
            setSemiTrans(prim, 1);
            setShadeTex(prim, 1);
            prim->tpage = 0x29;
            prim->clut  = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            setRGB0(prim, 0x40, 0xC0, 0x60);
            prim->tpage = 0x29;
            prim->clut  = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            setSemiTrans(prim, 1);
        }
        block->dx = (((arg2 * 55) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 55) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    DEF_REG(head);
}

/// Draws a ground-plane quad at `arg0`'s `workm` translation: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size (Y stays 0), rotated flat by
/// `Gfx_ViewWorldMtx`, then projected through `GsWSMATRIX`. One `RTPS` plus
/// one `RTPT` project the four corners; a negative `gte_stflg` drops the
/// quad. The texture is the two-frame tpage-0x28 strip at rows 0x38..0x57,
/// the frame picked by the low bit of `Display_State.field_8`, tinted
/// `(0x20, 0x30, 0x20)`.
void func_energyball_801307D4(GsCOORDINATE2* arg0, s32 arg1)
{
    void**               scratch;
    u8*                  head;
    EnergyGroundScratch* sc;
    POLY_FT4*            prim;
    GpQuadCorner*        tbl;
    SVECTOR*             v;
    s32                  i;
    s32                  otz;
    s32                  flag;
    s32                  u;
    s32                  prod;
    s32                  rb;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch - sizeof(EnergyGroundScratch);
    /* Store the freshly computed head and keep a copy for the rest of the
       function; without the barrier GCC folds the two together. */
    SOFT_TOUCH_REG(head);
    *scratch = head;
    sc       = (EnergyGroundScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = sc->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps_real();
    gte_stsxy(&sc->sxy0);
    gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_stszotz(&otz);
        otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        /* Assign and store r0 first so 0x20 is loaded next to `setcode` and
           stays live across g0/tpage; the `sb` itself schedules after tpage. */
        rb          = 0x20;
        prim->r0    = rb;
        prim->g0    = 0x30;
        prim->tpage = 0x28;
        prim->b0    = rb;
        prim->clut  = 0x428C;
        u           = ((Display_State.field_8 & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((Display_State.field_8 & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((Display_State.field_8 & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((Display_State.field_8 & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = *(u16*)&sc->sxy0.vx;
        prim->y0    = *(u16*)&sc->sxy0.vy;
        prim->x1    = *(u16*)&sc->sxy1.vx;
        prim->y1    = *(u16*)&sc->sxy1.vy;
        prim->x2    = *(u16*)&sc->sxy2.vx;
        prim->y2    = *(u16*)&sc->sxy2.vy;
        prim->x3    = *(u16*)&sc->sxy3.vx;
        prim->y3    = *(u16*)&sc->sxy3.vy;
        addPrim((u_long*)(((((u32)otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(EnergyGroundScratch);
}

/// Draws the energy ball's surface: two 16-vertex rings of the same radius
/// sit `arg1 * 2` apart in `arg0`'s local Y, are rotated by its `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell
/// is one of six 0x28-wide frames picked per vertex by the jitter table
/// `D_energyball_801311A0` plus the frame counter, the quad is tinted
/// `(arg2 >> 1, arg2, arg2 >> 1)`, and a negative `gte_stflg` drops the
/// segment.
void func_energyball_80130B54(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    s32            i;
    s32            next;
    s32            ang;
    s32            u;
    s16            idx;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (u32)(rsin(ang) * 3) >> 5;
        block->inner[i].vy = -(arg1 * 2);
        block->inner[i].vz = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (u32)(rsin(ang) * 3) >> 5;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        idx = (u32)(D_energyball_801311A0[i] + Display_State.field_8) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setRGB0(prim, arg2 >> 1, arg2, arg2 >> 1);
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            setSemiTrans(prim, 1);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

void func_energyball_8013107C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    u8             rgb[3];
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    Gp_UpdateCoord(coord);
    rgb[0] = (u16)mem->field_24 >> 1;
    rgb[1] = *(u8*)&mem->field_24;
    rgb[2] = (u16)mem->field_24 >> 1;
    Gp_DrawBandEx(coord, mem->field_26, 0x180, rgb);

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
