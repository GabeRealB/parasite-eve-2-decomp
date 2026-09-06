#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/pyrokinesis.h"

/// The `SndEvt_EnqueueType6` id for each pyrokinesis stage, three per PE level.
s32 D_pyrokinesis_80131DD8[] = {
    0xE00B0002,
    0xE00B0002,
    0xE00B0002,
    0xE00E0002,
    0xE00E0002,
    0xE00E0002,
    0xE0110002,
    0xE0110003,
    0xE0110004,
};

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

void PeShared8012fb14(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_pyrokinesis_801312B4(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3);

/// Runs one frame of the pyrokinesis cast: a five-state machine driven by
/// `Task::state`. State 0 copies the player rotation onto the effect
/// coordinate, rotates the combo-scaled launch offset into that frame, rolls
/// the 16 per-flame jitters, plays the roar picked by combo level and cast
/// variant, and links a `PyroWork` collision pair (list 1 + list 7) whose
/// packed id is the combo digits plus `0x28000`. State 1 walks the coordinate
/// by that offset each frame, redraws the cone and ring, parks the room light
/// slot on it and burns until the `D_80113D40` budget for the combo level runs
/// out. A `0x30000` hit on `obj` bursts into three `0x600F6` flames and moves
/// to state 3 (or 4 for cast variant 2); a `0x100000` hit on `obj2` means a
/// wall, which drops to state 2 and fades the cone out. States 3 and 4 grow
/// the two rings until they pass the combo radius, state 4 first stepping the
/// brightness down by 8 a frame. Any state releases if the player is dying
/// (`Gp_StateC08.field_3` / `D_80114C0B`) or the room is fading (`Gp_State1C`).
/// Scratch for the flame column's vertex work.
/// lists an object in the linker script at its first subsegment, and this has
s16 D_pyrokinesis_80131DFC[16] = { 0 };

void func_pyrokinesis_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    PyroWork*      work;
    TmdObject*     tmdo;
    GsCOORDINATE2* player;
    GpCoord64*     base;
    GsCOORDINATE2* slotc;
    GpCoordTail*   slot;
    GpMtxWords*    dstm;
    GpMtxWords*    srcm;
    GpEffWork*     spawned;
    GsCOORDINATE2  ground;
    u8             rgb[3];
    s32            i;
    s32            pan;
    s16            fade;
    s32            tick;
    s32            radius;
    s32            next;
    s16            amp;
    s32            tz;

    work          = (PyroWork*)arg0->idMap;
    mem           = arg0->spawnArg2;
    tmdo          = arg0->extra;
    coord         = tmdo->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    base          = Gp_RoomCoords;
    slotc         = &base->coord;
    slot          = (GpCoordTail*)slotc;
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_3 == -2) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            fade = Gp_State1C->field_E;
            if (fade >= 4) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (fade != 0) {
                mem->field_22 = (u16)mem->field_22 - 1;
                return;
            }
            work = Mem_Calloc(0x58, 0);
            if (work == NULL) {
                mem->field_22 = 0;
                return;
            }
            player     = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            dstm       = (GpMtxWords*)&coord->coord;
            srcm       = (GpMtxWords*)&player->coord;
            dstm->w0   = srcm->w0;
            dstm->w1   = srcm->w1;
            dstm->w2   = srcm->w2;
            dstm->w3   = srcm->w3;
            dstm->h4   = srcm->h4;
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->field_10 = 0;
            mem->field_12 = 0;
            mem->field_14 = (Gp_StateC08.field_0 % 10) * 64 + 0x1C0;
            gte_SetRotMatrix((MATRIX*)srcm);
            gte_ldv0(&mem->field_10);
            gte_rtv0_real();
            gte_stsv(&mem->field_10);
            for (i = 0; i < 16; i++) {
                Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
                D_pyrokinesis_80131DFC[i] = ((u32)Gp_LcgState >> 16) & 0xFF;
            }
            mem->field_24 = 0xC0;
            mem->field_26 = 0x500;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_28 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            mem->field_20 = (Gp_StateC08.field_0 % 10) - 1;
            pan           = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_pyrokinesis_80131DD8[mem->field_20 * 3 + arg0->spawnArg1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            Gp_SpawnPadLerp((s16)(mem->field_20 * 2 + 8), 0xFF, 8);
            if (mem->field_20 == 1) {
                arg0->spawnArg1 = 1;
            } else if (arg0->spawnArg1 == 1) {
                arg0->spawnArg1 = 0;
            }
            arg0->idMap        = (TaskIdMap*)work;
            work->obj.field_8  = coord;
            work->obj.field_C  = &work->rec;
            work->obj.field_18 = ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 +
                                 ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 +
                                 (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
            work->obj.field_1C = mem->field_26;
            work->obj.flags    = 1;
            Gp_LinkObj(1, &work->obj);
            work->rec.field_0   = 2;
            work->obj2.field_8  = coord;
            work->obj2.field_C  = &work->rec;
            work->obj2.field_18 = 0;
            work->obj.flags    |= 0x8000;
            work->obj2.field_1C = (s16)((u16)mem->field_26 << 16 >> 19);
            work->obj2.flags    = 1;
            Gp_LinkObj(7, &work->obj2);
            work->obj2.flags = (work->obj2.flags & 0x7FFF) | 0x4400;
            Gp_SpawnEff(0x60011, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            arg0->state = 1;
            func_pyrokinesis_80130848(coord, mem->field_22, mem->field_26, mem->field_28);
            func_pyrokinesis_80130130(coord, mem->field_26, (s16)((u16)mem->field_24 << 16 >> 17));
            if (Gp_CountRec18Hi(work->obj.field_C, 0x30000) != 0) {
                Gp_UnlinkObj(&work->obj);
                radius        = (mem->field_20 << 9) + 0x380;
                mem->field_26 = radius;
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(0x600F6, coord, i, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->field_0);
                    }
                }
                next = 3;
                if (arg0->spawnArg1 == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (Gp_FindRec18(work->obj2.field_C, 0x100000) != 0) {
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 1:
            if ((D_80114C0B == -2) || ((fade = Gp_State1C->field_E), fade >= 4)) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (fade != 0) {
                mem->field_22 = (u16)mem->field_22 - 1;
                return;
            }
            radius             = (mem->field_20 << 9) + 0x380;
            mem->field_26      = radius;
            work->obj.field_1C = radius;
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            SOFT_USE_REG(coord);
            func_pyrokinesis_80130848(coord, mem->field_22, mem->field_26, mem->field_28);
            func_pyrokinesis_80130130(coord, mem->field_26, (s16)((u16)mem->field_24 << 16 >> 17));
            if (arg0->spawnArg1 != 0) {
                func_pyrokinesis_80131784(coord, mem->field_22, mem->field_26, 0);
                func_pyrokinesis_80131784(coord, mem->field_22, mem->field_26, 1);
            }
            if (mem->field_22 < 0x1E) {
                spawned = Gp_SpawnEff(0x60069, coord, 0, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
            if (Gp_State1C->field_6 != 0) {
                if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                    func_pyrokinesis_801304C4(&ground, mem->field_26);
                }
            }
            base->field_0     = 4;
            slot->field_58    = (mem->field_20 << 9) + 0x200;
            slot->field_5C    = slot->field_58 * 16;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            amp               = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            slot->field_50    = amp;
            slot->field_52    = (u16)slot->field_50 >> 1;
            slot->field_54    = slot->field_50 >> 2;
            slotc->coord.t[0] = coord->coord.t[0];
            slotc->coord.t[1] = coord->coord.t[1];
            tz                = coord->coord.t[2];
            slotc->flg        = 0;
            slotc->coord.t[2] = tz;
            if (Gp_CountRec18Hi(work->obj.field_C, 0x30000) != 0) {
                Gp_UnlinkObj(&work->obj);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(0x600F6, coord, i, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->field_0);
                    }
                }
                next = 3;
                if (arg0->spawnArg1 == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (Gp_FindRec18(work->obj2.field_C, 0x100000) != 0) {
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            tick = mem->field_22;
            if (tick * 6 > D_80113D40[mem->field_20].field_4) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            if (tick < 0x1F) {
                Gp_ClearRec18Occupied(&work->rec);
                return;
            }
            Gp_UnlinkObj(&work->obj);
            Gp_UnlinkObj(&work->obj2);
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        case 2:
            if ((D_80114C0B == -2) || ((fade = Gp_State1C->field_E), fade >= 4)) {
                Gp_UnlinkObj(&work->obj);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (fade != 0) {
                mem->field_22 = (u16)mem->field_22 - 1;
                return;
            }
            Gp_UpdateCoord(coord);
            radius             = (u16)mem->field_26 - 0x40;
            mem->field_26      = radius;
            work->obj.field_1C = radius;
            func_pyrokinesis_80130848(coord, mem->field_22, mem->field_26, mem->field_28);
            func_pyrokinesis_80130130(coord, mem->field_26, (s16)((u16)mem->field_24 << 16 >> 17));
            if (mem->field_26 >= 0x81) {
                spawned = Gp_SpawnEff(0x60069, coord, 0, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
            if (Gp_CountRec18Hi(work->obj.field_C, 0x30000) != 0) {
                Gp_UnlinkObj(&work->obj);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(0x600F6, coord, i, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->field_0);
                    }
                }
                next = 3;
                if (arg0->spawnArg1 == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (mem->field_26 < 0x80) {
                Gp_UnlinkObj(&work->obj);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 3:
            if (D_80114C0B == -2) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            fade = Gp_State1C->field_E;
            if (fade >= 4) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (fade != 0) {
                mem->field_22 = (u16)mem->field_22 - 1;
                return;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_80130848(coord, mem->field_22, mem->field_26, mem->field_28);
            func_pyrokinesis_80130130(coord, mem->field_26, (s16)((u16)mem->field_24 << 16 >> 17));
            func_pyrokinesis_80130130(coord, (s16)((u16)mem->field_26 * 2),
                                      (s16)((u16)mem->field_24 << 16 >> 17));
            mem->field_26 = (u16)mem->field_26 + 0x40;
            if (mem->field_26 > ((mem->field_20 << 9) + 0x580)) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            return;
        case 4:
            if (D_80114C0B == -2) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            fade = Gp_State1C->field_E;
            if (fade >= 4) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (fade != 0) {
                mem->field_22 = (u16)mem->field_22 - 1;
                return;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_80130848(coord, mem->field_22, mem->field_26, mem->field_28);
            func_pyrokinesis_80130130(coord, mem->field_26, (s16)((u16)mem->field_24 << 16 >> 17));
            func_pyrokinesis_80130130(coord, (s16)((u16)mem->field_26 * 2),
                                      (s16)((u16)mem->field_24 << 16 >> 17));
            mem->field_26 = (u16)mem->field_26 + 0x40;
            if (mem->field_26 > ((mem->field_20 << 9) + 0x580)) {
                if (mem->field_24 >= 9) {
                    mem->field_24 = (u16)mem->field_24 - 8;
                    return;
                }
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
            }
            return;
    }
}

void func_pyrokinesis_8012FAC8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            scene;
    s16            flag;
    s32            state;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        scene = Gp_State1C->field_16;
        if (scene == 1) {
            flag = Gp_State1C->field_E;
            if (flag < 4) {
                if (flag != 0) {
                    return;
                }
                mem->field_22 = (u16)mem->field_22 + 1;
                Gp_UpdateCoord(coord);
                state = arg0->state;
                if (state == scene) {
                    goto L_case1;
                }
                if (state < 2) {
                    if (state == 0) {
                        goto L_case0;
                    }
                    return;
                }
                if (state == 2) {
                    goto L_case2;
                }
                if (state == 3) {
                    goto L_release;
                }
                return;
            L_case0:
                Gp_SpawnEff(0x80060010, coord, 0, 0);
                arg0->state = scene;
                return;
            L_case1:
                if (mem->field_22 == 8) {
                    Gp_SpawnEff(0x80060010, coord, 1, 0);
                    arg0->state = 2;
                }
                return;
            L_case2:
                if (mem->field_22 == 0x10) {
                    Gp_SpawnEff(0x80060000 | 0x10, coord, 2, 0);
                    arg0->state = 3;
                }
                return;
            }
        }
    }
L_release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws the pyrokinesis flame cone: a 16-vertex inner ring of radius `arg1`
/// at depth `0x100` and an outer ring of radius `arg1 + 0x100` at depth 0 are
/// built in the XY plane by `rsin` / `rcos`, rotated by `arg0`'s `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one `POLY_G4`. The inner edge carries the `arg2`
/// ramp `(arg2, arg2 >> 1, arg2 >> 2)` and the outer edge fades to black; a
/// negative `gte_stflg` drops the segment.
void func_pyrokinesis_8012FC34(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u32            ramp;
    u8             red;
    u8             grn;
    u8             blu;

    /* The ramp halves are unsigned: writing them as `(u16)arg2 >> 1` folds the
     * widening into an `andi`, where the ROM shifts the value up and back. */
    ramp     = (u32)arg2 << 16;
    red      = arg2;
    grn      = ramp >> 17;
    blu      = ramp >> 18;
    r1       = arg1 + 0x100;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
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
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
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
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / otz`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring.
void func_pyrokinesis_80130130(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;
    u16            red;

    scratch = (void**)G_SCRATCH_HEAD;
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
    red           = arg2;
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
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, *(u8*)&red, arg2 >> 1, arg2 >> 2);
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
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            ang = ang2;
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Draws the scorch mark the cone leaves on the floor: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size, laid flat into view space with
/// `Gfx_ViewWorldMtx` (rotation only, translation from `GsWSMATRIX`) and
/// offset by `arg0->workm.t`, then its four corners are projected through
/// `GsWSMATRIX`. On a non-negative `gte_stflg` it queues one semi-transparent
/// `POLY_FT4` (tpage 0x28, clut 0x428C) tinted `(0x30, 0x20, 0x20)`; the frame
/// counter's low bit picks between two 0x1F-wide UV columns at v = 0x38..0x57.
/// Same 0x38 scratch block and body as `Gp_DrawEffSprite7C`.
void func_pyrokinesis_801304C4(GsCOORDINATE2* arg0, s32 arg1)
{
    void**            scratch;
    register u8*      head asm("v1");
    GpQuadScratch*    block;
    register SVECTOR* v asm("a2");
    s32               i;
    GpQuadCorner*     tbl;
    POLY_FT4*         prim;
    s32               u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        v->vx = tbl->x * arg1;
        v->vy = 0;
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
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
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
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

/// Draws one billboard flame quad: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and a single semi-transparent `POLY_FT4` (tpage 0x29, clut
/// 0x428C) is spun around it, its four corners offset by the rotated
/// half-extents `(arg2 * 55 / otz) * rsin|rcos` at `arg3` and `arg3 + 0x400`.
/// `arg1`'s low bit picks between two 0x37-wide UV columns at v = 0xC8..0xFF.
/// A negative `gte_stflg` drops the quad.
void func_pyrokinesis_80130848(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s32               ang;
    s32               u70;
    s32               t;
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
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        ang = (s16)arg3;
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x428C;
        t           = (arg1 & 1) * 56;
        u70         = t + 0x70;
        prim->u0    = u70;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->u1    = t - 0x59;
        prim->u2    = u70;
        prim->v2    = 0xFF;
        prim->u3    = t - 0x59;
        prim->v3    = 0xFF;
        block->dx   = ((((s16)arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang         = ang + 0x400;
        block->dx   = ((((s16)arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void func_pyrokinesis_80130C54(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            temp_a1;
    s32            y;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->field_E;
        if (flag < 4) {
            if (flag != 0) {
                return;
            }
            mem->field_22 = (u16)mem->field_22 + 1;
            if (arg0->state == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = -(((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                arg0->state   = 1;
            }
            y                 = coord->coord.t[1] + mem->field_12;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            temp_a1 = mem->field_20;
            if (temp_a1 < 8) {
                if ((u16)mem->field_22 & 1) {
                    PeShared8012fb14(coord, temp_a1, 0x300, mem->field_24);
                }
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}
