#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/apobiosis.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

void func_apobiosis_8013017C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_apobiosis_80130630(GsCOORDINATE2* arg0, s16* arg1, s16 arg2, s16 arg3);

/// The apobiosis cast. Six states drive one screen flash plus a growing ring
/// of shards, scaled by `D_apobiosis_80130B5C[Gp_StateC08.field_0 % 10 - 1]`
/// so a longer combo casts a wider burst. State 0 parents the effect
/// coordinate on `GpEffWork.field_8` at the origin, publishes the task in
/// `D_apobiosis_80130BA0` so every shard can reparent onto it, plays the row's
/// `SndEvt_EnqueueType6` id panned at the coordinate, and seeds
/// `D_apobiosis_80130B80` with `field_0 * 2` angles - the ring's two rows of
/// azimuths. State 1 flashes at `field_2A`, drags the coordinate down 0x400,
/// grows `field_24` by the row's `field_4` each frame and redraws both the
/// player's ring and the shard ring, jittering every angle by +-0x80 per frame.
/// States 2..4 fade the flash out at 0x10 / 0xC / 8 a frame while spawning
/// 0x600F7 sparks on random polar offsets - one in four frames in state 2, one
/// a frame in state 3, two a frame in state 4 - and state 5 fades the last of
/// the flash before releasing the work block. `Gp_SpawnPadLerp` rumbles at each
/// state change, hardest on the widest row.
void func_apobiosis_8012EF4C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s32            i;
    s32            n;
    s32            pan;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B != -2) && (Gp_State1C->field_E < 4)) {
        mem->field_22 = (u16)mem->field_22 + 1;
        switch (arg0->state) {
            case 0:
                D_apobiosis_80130BA0 = arg0;
                rot                  = (GpMtxWords*)&coord->coord;
                coord->sub           = mem->field_8;
                rot->w0              = 0x1000;
                rot->w1              = 0;
                rot->w2              = 0x1000;
                rot->w3              = 0;
                rot->h4              = 0x1000;
                coord->coord.t[0]    = 0;
                coord->coord.t[1]    = 0;
                coord->coord.t[2]    = 0;
                coord->flg           = 0;
                Gp_UpdateCoord(coord);
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(D_apobiosis_80130B74[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
                arg0->state   = 1;
                mem->field_20 = Gp_StateC08.field_0 % 10 - 1;
                mem->field_24 = 0x200;
                mem->field_28 = 0x80;
                mem->field_2A = 0xF0;
                for (i = 0; i < D_apobiosis_80130B5C[mem->field_20].field_0 * 2; i++) {
                    Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                    D_apobiosis_80130B80[i] = (i << 10) + (((u32)Gp_LcgState >> 16) & 0x3FF);
                }
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                /* fallthrough */
            case 1:
                Gp_UpdateCoord(coord);
                if (mem->field_22 == 4) {
                    Gp_StateC08.field_6 |= 8;
                }
                func_apobiosis_8012F808(mem->field_2A);
                rgb[0] = rgb[1]    = mem->field_2A >> 2;
                rgb[2]             = (u16)mem->field_2A >> 1;
                coord->workm.t[1] -= 0x400;
                mem->field_24      = (u16)mem->field_24 + D_apobiosis_80130B5C[mem->field_20].field_4;
                func_apobiosis_8013017C(
                    &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[1], mem->field_22,
                    D_apobiosis_80130B5C[mem->field_20].field_2, 0);
                func_apobiosis_8012F9D0(coord, mem->field_24, 0x80, rgb);
                if (mem->field_22 & 1) {
                    func_apobiosis_8012F9D0(coord, 0x80, mem->field_24, rgb);
                }
                for (i = 0; i < D_apobiosis_80130B5C[mem->field_20].field_0; i++) {
                    Gp_LcgState              = Gp_LcgState * 5 + 0x71357911;
                    D_apobiosis_80130B80[i] -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
                    n                        = i + D_apobiosis_80130B5C[mem->field_20].field_4;
                    Gp_LcgState              = Gp_LcgState * 5 + 0x71357911;
                    D_apobiosis_80130B80[n] -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
                    mem->field_18            = mem->field_24 * rsin(D_apobiosis_80130B80[i]) >> 12;
                    mem->field_1A            = mem->field_24 * rcos(D_apobiosis_80130B80[i]) >> 12;
                    mem->field_1C =
                        mem->field_18 *
                            rcos(D_apobiosis_80130B80
                                     [i + D_apobiosis_80130B5C[mem->field_20].field_4]) >>
                        12;
                    func_apobiosis_80130630(coord, &mem->field_18, mem->field_22,
                                            D_apobiosis_80130B5C[mem->field_20].field_6);
                }
                coord->workm.t[1] += 0x400;
                if (mem->field_2A >= 0x19) {
                    mem->field_2A = (u16)mem->field_2A - 0x18;
                    return;
                }
                arg0->state = 2;
                Gp_SpawnPadLerp(0x14, 0xFF, 8);
                return;
            case 2:
                Gp_UpdateCoord(coord);
                func_apobiosis_8012F808(mem->field_2A);
                if (mem->field_2A >= 0x41) {
                    mem->field_2A = (u16)mem->field_2A - 0x10;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_24 = ((u32)Gp_LcgState >> 16) & 0x3FF;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                    mem->field_10 = mem->field_24 * rsin(mem->field_26) >> 12;
                    mem->field_14 = mem->field_24 * rcos(mem->field_26) >> 12;
                    Gp_SpawnEff(0x600F7, coord, 0, (SVECTOR*)&mem->field_10);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_2A = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x60;
                }
                if (mem->field_22 == 0x14) {
                    mem->field_2A = 0xF0;
                    arg0->state   = 3;
                }
                return;
            case 3:
                func_apobiosis_8012F808(mem->field_2A);
                if (mem->field_2A >= 0x21) {
                    mem->field_2A = (u16)mem->field_2A - 0xC;
                }
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = ((u32)Gp_LcgState >> 16) & 0x7FF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                mem->field_10 = mem->field_24 * rsin(mem->field_26) >> 12;
                mem->field_14 = mem->field_24 * rcos(mem->field_26) >> 12;
                Gp_SpawnEff(0x600F7, coord, 0, (SVECTOR*)&mem->field_10);
                if (mem->field_22 == 0x1E) {
                    if (mem->field_20 <= 0) {
                        arg0->state = 5;
                        Gp_SpawnPadLerp(mem->field_20 * 8 + 0x12, 0xFF, 8);
                    } else {
                        mem->field_2A = 0xF0;
                        arg0->state   = 4;
                        Gp_SpawnPadLerp(0x22, 0xFF, 8);
                    }
                }
                return;
            case 4:
                func_apobiosis_8012F808(mem->field_2A);
                if (mem->field_2A >= 9) {
                    mem->field_2A = (u16)mem->field_2A - 8;
                }
                for (i = 0; i < 2; i++) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                    mem->field_10 = mem->field_24 * rsin(mem->field_26) >> 12;
                    mem->field_14 = mem->field_24 * rcos(mem->field_26) >> 12;
                    Gp_SpawnEff(0x600F7, coord, 0, (SVECTOR*)&mem->field_10);
                }
                if (mem->field_22 == 0x28) {
                    arg0->state = 5;
                }
                return;
            case 5:
                func_apobiosis_8012F808(mem->field_2A);
                if (mem->field_2A >= 9) {
                    mem->field_2A = (u16)mem->field_2A - 8;
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Flashes a screen-filling `POLY_F4` over the whole 320x240 frame, offset by
/// `Display_State.vramYOffset` so it tracks the active draw buffer. `bright`
/// is the flash level: normally the quad is blue-tinted (red and green
/// halved), but on stage `Gp_StateC08.field_0 % 10 == 3` one draw in four
/// comes out yellow instead (blue halved). The prim is linked at a fixed
/// `otz` of 0x30, in front of the scene.
void func_apobiosis_8012F808(u32 bright)
{
    POLY_F4* prim;
    s32      rng;
    s32      kind;
    u16      level;

    prim           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setPolyF4(prim);
    kind  = (u16)(Gp_StateC08.field_0 % 10U) - 1;
    level = bright;
    if (kind == 2) {
        SOFT_TOUCH_REG(level);
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if ((((u32)rng >> 16) & 3) == 0) {
            setRGB0(prim, level, level, bright >> 1);
        } else {
            setRGB0(prim, (s16)level >> 1, (s16)level >> 1, level);
        }
    } else {
        SOFT_TOUCH_REG(level);
        setRGB0(prim, (s16)level >> 1, (s16)level >> 1, level);
    }
    setXY4(prim, -0xA0, -0x78 - Display_State.vramYOffset, 0xA0,
           -0x78 - Display_State.vramYOffset, -0xA0, 0x78 - Display_State.vramYOffset,
           0xA0, 0x78 - Display_State.vramYOffset);
    addPrim((u_long*)((((u32)(0x30 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x30);
}

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when the GTE
/// flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that form a
/// ring around it. The projected depth is pulled 0x40 towards the eye (never
/// nearer than 0x10) and both on-screen radii divide by it: `inner` is
/// `(s16)arg1 * 64 / otz` and `outer` is `(s16)(arg1 + arg2) * 64 / otz`, so
/// the ring is an annulus `arg2` wide. `rgb` tints the outer rim of every
/// wedge while its inner rim stays black. Each wedge is linked into the OT
/// bucket its own depth names and then handed to `Gp_AddTpageShift`. Same
/// shape as `func_plasma_8012FB10`, which grows its ring from `otz + 1`
/// instead.
void func_apobiosis_8012F9D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    ApobiosisRingScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    register void**       scratch asm("a1");
    register s32          saved asm("t1");
    register u8*          head asm("t0");
    register s32          inner asm("a0");
    register s32          outer asm("v1");
    register u8*          color asm("s4");
    s32                   t;
    u16                   vz;
    u32                   maskLo;
    u32                   maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *(u8* volatile*)scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(volatile u16*)&arg0->workm.t[0];
        ((ApobiosisRingScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (ApobiosisRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    outer         = saved + arg2;
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((ApobiosisRingScratch*)(head - 0x1C))->sx);
    gte_stflg(&((ApobiosisRingScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((ApobiosisRingScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz -= 0x40;
        if (block->otz < 0x10) {
            block->otz = 0x10;
        }
        inner = (s16)saved * 64;
        inner = inner / block->otz;
        SOFT_COMPILER_BARRIER();
        outer        = (s16)outer * 64;
        outer        = outer / block->otz;
        ang          = 0;
        block->inner = inner;
        block->outer = outer;

        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->inner * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->inner * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->inner * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->outer * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->outer * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// One shard of the apobiosis burst. Every frame it ticks the shard's life
/// counter `GpEffWork.field_22` and bails out - handing the work block back -
/// once the player is dying (`D_80114C0B`), the room is fading (`Gp_State1C`)
/// or the shard has outlived its state. State 0 reparents the shard onto the
/// cast task and splits on `spawnArg1`: a non-zero arg pins the shard to the
/// cast's coordinate at the origin (state 1), a zero arg gives it a random
/// drift `field_10`..`field_14` and lets it fly (state 2). Either way the tail
/// seeds the shard's `field_18`..`field_1C` offset, its radius `field_26` and
/// the intensity `field_2A` that picks a `D_apobiosis_80130B5C` row. Both live
/// states redraw the shard every other frame, at twice the row's radius while
/// pinned and at the plain radius once free.
void func_apobiosis_8012FE10(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B != -2) && (Gp_State1C->field_E < 4)) {
        mem->field_22 = (u16)mem->field_22 + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_apobiosis_80130BA0, arg0);
                if (arg0->spawnArg1 != 0) {
                    coord->sub        = mem->field_8;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                    coord->flg        = 0;
                    Gp_UpdateCoord(coord);
                    arg0->state = 1;
                } else {
                    mem->field_12 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_14 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                    arg0->state   = 2;
                }
                mem->field_1A = -0x1000;
                mem->field_24 = 0x80;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_18 = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1C = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                mem->field_2A = Gp_StateC08.field_0 % 10 - 1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                if (mem->field_22 & 1) {
                    mem->field_20 = (u16)mem->field_20 + 1;
                    func_apobiosis_8013017C(coord, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6 * 2,
                                            mem->field_26);
                    func_apobiosis_80130630(coord, &mem->field_18, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6 * 2);
                }
                if (mem->field_22 < 0x19) {
                    return;
                }
                break;
            case 2:
                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_22 & 1) {
                    mem->field_20 = (u16)mem->field_20 + 1;
                    func_apobiosis_8013017C(coord, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6,
                                            mem->field_26);
                    func_apobiosis_80130630(coord, &mem->field_18, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6);
                }
                if (mem->field_22 < 0x11) {
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_8013017C);

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_80130630);
