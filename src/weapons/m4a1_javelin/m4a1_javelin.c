#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "weapons/m4a1_javelin.h"

/// Fixed local offset the guide beam's coordinate hangs at.
SVECTOR D_m4a1_javelin_8011FA90 = { 0, 0x200, 0x20, 0 };

/// `(0, 0x800, 0)`: the probe offset `func_800DE7CC` traces each beam segment
/// against, rotated into world space by `Gfx_ViewWorldMtx` first.
SVECTOR D_m4a1_javelin_8011FA98 = { 0, 0x800, 0, 0 };

/// Per-segment `flags` for `func_m4a1_javelin_8011DAB0`, walked from the far
/// end (`[5]`, bit 1: retake the beam angle) to the muzzle (`[0]`, bit 0: cap
/// the near end).
u16 D_m4a1_javelin_8011FAA0[6] = { 1, 0, 0, 0, 0, 2 };

/// The four RGB444 beam colours `GpEffWork::field_2A` fades through.
u16 D_m4a1_javelin_8011FAAC[4] = { 0x12, 0x124, 0x248, 0x36C };

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern u32 Gp_LcgState;

/// Per-frame task for the javelin's guide beam. `Task::spawnArg2` is the
/// `Gp_State1C` work block and `Task::extra` reaches the coordinate the beam
/// hangs on. Any room fade of 4 or more tears the effect down; a fade of 1 to 3
/// freezes it.
///
/// - State 0 hangs the coordinate off `GpEffWork::field_8` at the fixed offset
///   `D_m4a1_javelin_8011FA90` with an identity rotation, seeds the beam
///   parameters and falls through to state 1.
/// - State 1 is the muzzle flare: it claims room-light slot 1 as a narrowing
///   (`0x100` / `0x1000`) light whose radius halves every frame, then draws
///   eight `func_m4a1_javelin_8011EE78` tracers around a ring that widens by
///   `0x20` a frame until `field_24` reaches `0xC0`, which moves it to state 2.
/// - State 2 is the beam itself. The far end is either the cached
///   `D_m4a1_javelin_8012EB68` impact point or `GpEffWork::field_10` rotated
///   into world space, and `field_18` / `field_1A` / `field_1C` are a sixth of
///   the way back towards the muzzle. Six segments are drawn with
///   `func_m4a1_javelin_8011DAB0`; while `Gp_State1C::field_6` is set each
///   segment also probes `D_m4a1_javelin_8011FA98` (0x800 along +Y) with
///   `func_800DE7CC` and skins the ground contact with
///   `func_m4a1_javelin_8011E4A8` as long as the probe keeps hitting. The beam
///   fades one `D_m4a1_javelin_8011FAAC` colour step every 0x20 of `field_22`
///   and releases the work block when the last step runs out.
void func_m4a1_javelin_8011D1E4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GameActor*     actor;
    GpCoord64*     base;
    GsCOORDINATE2* light;
    GpCoordTail*   slot;
    GpMtxWords*    dstm;
    SVECTOR        pa;
    SVECTOR        pb;
    SVECTOR        qa;
    SVECTOR        qb;
    s32            i;
    s32            lim;
    s32            t;
    u16            rnd;

    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    base  = &Gp_RoomCoords[1];
    slot  = (GpCoordTail*)&base->coord;
    light = &base->coord;
    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;

    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    work->field_22 = (u16)work->field_22 + 1;
    switch (task->state) {
        case 0:
            dstm              = (GpMtxWords*)&coord->coord;
            coord->sub        = work->field_8;
            dstm->w0          = 0x1000;
            dstm->w1          = 0;
            dstm->w2          = 0x1000;
            dstm->w3          = 0;
            dstm->h4          = 0x1000;
            coord->coord.t[0] = D_m4a1_javelin_8011FA90.vx;
            coord->coord.t[1] = D_m4a1_javelin_8011FA90.vy;
            coord->coord.t[2] = D_m4a1_javelin_8011FA90.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            task->state                = 1;
            work->field_12             = 0x1F40;
            work->field_10             = 0;
            work->field_14             = 0;
            D_m4a1_javelin_8012EB68.vx = 0;
            D_m4a1_javelin_8012EB68.vy = 0;
            D_m4a1_javelin_8012EB68.vz = 0;
            D_m4a1_javelin_8012EB70    = 0;
            work->field_28             = 0x600;
            work->field_2A             = 3;
            D_m4a1_javelin_8012EB62    = 0;
            D_m4a1_javelin_8012EB60    = 0;
            /* fallthrough */
        case 1:
            Gp_UpdateCoord(coord);
            base->field_0  = 4;
            slot->field_58 = 0x100;
            slot->field_5C = 0x1000;
            t              = (s16)(u16)slot->field_50 >> 1;
            slot->field_50 = t;
            slot->field_52 = t >> 2;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            slot->field_54 = ((Gp_LcgState >> 16) & 0x700) + 0x400;
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
            light->flg = 0;
            if (work->field_24 == 0xC0) {
                task->state = 2;
            } else {
                work->field_24 = (u16)work->field_24 + 0x20;
                work->field_26 = (u16)work->field_26 + 0xC0;
                work->field_28 = (u16)work->field_28 - 0xF0;
            }
            pa.vx = ((M4a1JavelinVecLo*)coord->workm.t)->vx;
            pa.vy = ((M4a1JavelinVecLo*)coord->workm.t)->vy;
            pa.vz = ((M4a1JavelinVecLo*)coord->workm.t)->vz;
            for (i = 0; i < 0x1000; i += 0x200) {
                pb.vx = (work->field_28 * rsin(i)) >> 12;
                pb.vy = (u16)work->field_26;
                pb.vz = (work->field_28 * rcos(i)) >> 12;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&pb);
                gte_rtv0_real();
                gte_stsv(&pb);
                pb.vx = (u16)pb.vx + (u16)pa.vx;
                pb.vy = (u16)pb.vy + (u16)pa.vy;
                pb.vz = (u16)pb.vz + (u16)pa.vz;
                func_m4a1_javelin_8011EE78(&pa, &pb, work->field_24);
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            base->field_0  = 4;
            slot->field_58 = 0x400;
            slot->field_5C = 0x4000;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            rnd            = ((Gp_LcgState >> 16) & 0x700) + 0x800;
            slot->field_54 = rnd;
            slot->field_50 = rnd >> 1;
            slot->field_52 = rnd >> 1;
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
            D_m4a1_javelin_8012EB64 = 0;
            light->flg              = 0;
            D_m4a1_javelin_8012EB66 = 0;
            if (D_m4a1_javelin_8012EB70 != 0) {
                pa.vx = ((M4a1JavelinVecLo*)coord->workm.t)->vx;
                pa.vy = ((M4a1JavelinVecLo*)coord->workm.t)->vy;
                pa.vz = ((M4a1JavelinVecLo*)coord->workm.t)->vz;
                pb.vx = D_m4a1_javelin_8012EB68.vx;
                pb.vy = D_m4a1_javelin_8012EB68.vy;
                pb.vz = D_m4a1_javelin_8012EB68.vz;
            } else {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&pb);
                pa.vx = ((M4a1JavelinVecLo*)coord->workm.t)->vx;
                pa.vy = ((M4a1JavelinVecLo*)coord->workm.t)->vy;
                pa.vz = ((M4a1JavelinVecLo*)coord->workm.t)->vz;
                pb.vx = (u16)pb.vx + (u16)pa.vx;
                pb.vy = (u16)pb.vy + (u16)pa.vy;
                pb.vz = (u16)pb.vz + (u16)pa.vz;
            }
            work->field_18 = (pa.vx - pb.vx) / 6;
            work->field_1A = (pa.vy - pb.vy) / 6;
            work->field_1C = (pa.vz - pb.vz) / 6;
            pa.vx          = (u16)pb.vx;
            pa.vy          = (u16)pb.vy;
            pa.vz          = (u16)pb.vz;
            if (Gp_State1C->field_6 != 0) {
                gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                gte_ldv0(&D_m4a1_javelin_8011FA98);
                gte_rtv0_real();
                gte_stsv(&qb);
                qb.vx = (u16)qb.vx + (u16)pb.vx;
                qb.vy = (u16)qb.vy + (u16)pb.vy;
                qb.vz = (u16)qb.vz + (u16)pb.vz;
                pb.vy = (u16)pb.vy - 0x100;
                if (func_800DE7CC(&qb, &pb, &qb, NULL) == 1) {
                    lim = 6;
                } else {
                    lim = 5;
                }
                pb.vy = (u16)pb.vy + 0x100;
                for (i = 5; i >= 0; i--) {
                    pa.vx = (u16)pa.vx + (u16)work->field_18;
                    pa.vy = (u16)pa.vy + (u16)work->field_1A;
                    pa.vz = (u16)pa.vz + (u16)work->field_1C;
                    func_m4a1_javelin_8011DAB0(&pa, &pb, D_m4a1_javelin_8011FAA0[i],
                                               D_m4a1_javelin_8011FAAC[work->field_2A]);
                    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                    gte_ldv0(&D_m4a1_javelin_8011FA98);
                    gte_rtv0_real();
                    gte_stsv(&qa);
                    qa.vx = (u16)qa.vx + (u16)pa.vx;
                    qa.vy = (u16)qa.vy + (u16)pa.vy;
                    qa.vz = (u16)qa.vz + (u16)pa.vz;
                    pa.vy = (u16)pa.vy - 0x100;
                    if (func_800DE7CC(&qa, &pa, &qa, NULL) == 1) {
                        if (i < lim) {
                            func_m4a1_javelin_8011E4A8(&qa, &qb, D_m4a1_javelin_8011FAA0[i],
                                                       D_m4a1_javelin_8011FAAC[(s16)(u16)work->field_2A >> 1]);
                        }
                        lim = i;
                    } else {
                        lim = i - 1;
                    }
                    pa.vy = (u16)pa.vy + 0x100;
                    qb.vx = (u16)qa.vx;
                    qb.vy = (u16)qa.vy;
                    qb.vz = (u16)qa.vz;
                    pb.vx = (u16)pa.vx;
                    pb.vy = (u16)pa.vy;
                    pb.vz = (u16)pa.vz;
                }
            } else {
                for (i = 5; i >= 0; i--) {
                    pa.vx = (u16)pa.vx + (u16)work->field_18;
                    pa.vy = (u16)pa.vy + (u16)work->field_1A;
                    pa.vz = (u16)pa.vz + (u16)work->field_1C;
                    func_m4a1_javelin_8011DAB0(&pa, &pb, D_m4a1_javelin_8011FAA0[i], 0x36C);
                    pb.vx = (u16)pa.vx;
                    pb.vy = (u16)pa.vy;
                    pb.vz = (u16)pa.vz;
                }
            }
            if (work->field_22 >= 0x21) {
                work->field_2A = (u16)work->field_2A - 1;
                if (work->field_2A < 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
            } else if (*(s32*)&actor->field_954 != 0x40000) {
                work->field_22 = (u16)work->field_22 + 0x20;
            }
            break;
    }
}

/// Draws the javelin's aiming guide: a flat `LINE_F2` from `p0` to `p1` plus
/// three fans of `POLY_G4` segments, all dropped if either endpoint fails its
/// `RTPS` `FLAG` check (which also arms `D_m4a1_javelin_8012EB64` so the beam
/// angle is recomputed on the next visible frame). `color` is an RGB444 word
/// widened a nibble at a time, brightened by `Display_State.field_8`'s low bit
/// so the beam flickers every other frame; the fans use two thirds of that.
/// Bit 1 of `flags` forces the `ratan2` of the on-screen beam direction to be
/// taken again, otherwise the cached `D_m4a1_javelin_8012EB60` is reused. The
/// first fan caps the far end, the second (bit 0 of `flags`) caps the near end
/// and the third sweeps at double rate to skin the beam between the two.
void func_m4a1_javelin_8011DAB0(SVECTOR* p0, SVECTOR* p1, u16 flags, u16 color)
{
    u8*                     head;
    M4a1JavelinRingScratch* sc;
    LINE_F2*                line;
    POLY_G4*                prim;
    s32                     i;
    s32                     tipAng;
    s32                     baseAng;
    s32                     bodyAng;
    s32                     tint;
    u32                     rgb;
    u8                      r;
    u8                      g;
    u8                      b;
    u16                     angle;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(M4a1JavelinRingScratch);
    sc                    = (M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch));

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(p0);
    gte_rtps_real();
    gte_stsxy(&((M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch)))->sx0);
    gte_stflg(&((M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch)))->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&((M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch)))->otz0);
        sc->otz0++;
        gte_ldv0(p1);
        gte_rtps_real();
        gte_stsxy(&((M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch)))->sx1);
        gte_stflg(&((M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch)))->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&((M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch)))->otz1);
            rgb = color;
            r   = (rgb >> 4) & 0xF0;
            g   = rgb & 0xF0;
            b   = (color & 0xF) * 0x10;
            sc->otz1++;
            line           = (LINE_F2*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
            setLineF2(line);
            tint = ((u8)Display_State.field_8 & 1) * 0x10;
            r    = r + tint;
            g    = g + tint;
            b    = b + tint;
            setRGB0(line, r, g, b);
            line->x0 = sc->sx0;
            line->y0 = sc->sy0;
            line->x1 = sc->sx1;
            line->y1 = sc->sy1;
            addPrim((u_long*)(((((u32)sc->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), line);
            Gp_AddTpageShift((P_TAG*)line, 1, sc->otz0);
            sc->r0 = 0x4000 / sc->otz0;
            sc->r1 = 0x4000 / sc->otz1;
            r      = r * 2 / 3;
            g      = g * 2 / 3;
            b      = b * 2 / 3;
            if ((flags & 2) || D_m4a1_javelin_8012EB64 != 0) {
                angle                   = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                D_m4a1_javelin_8012EB60 = angle;
                D_m4a1_javelin_8012EB64 = 0;
                for (i = (s16)angle; i < (s16)angle + 0x800; i += 0x400) {
                    prim           = (POLY_G4*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    tipAng   = i + 0x800;
                    prim->x0 = *(u16*)&line->x1 + ((sc->r1 * rsin(tipAng)) >> 12);
                    prim->y0 = *(u16*)&line->y1 + ((sc->r1 * rcos(tipAng)) >> 12);
                    tipAng   = i + 0xA00;
                    prim->x1 = *(u16*)&line->x1 + ((sc->r1 * rsin(tipAng)) >> 12);
                    prim->y1 = *(u16*)&line->y1 + ((sc->r1 * rcos(tipAng)) >> 12);
                    prim->x2 = *(u16*)&line->x1;
                    prim->y2 = *(u16*)&line->y1;
                    tipAng   = i + 0xC00;
                    prim->x3 = *(u16*)&line->x1 + ((sc->r1 * rsin(tipAng)) >> 12);
                    prim->y3 = *(u16*)&line->y1 + ((sc->r1 * rcos(tipAng)) >> 12);
                    addPrim((u_long*)(((((u32)sc->otz1 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, sc->otz1);
                }
            } else {
                angle = D_m4a1_javelin_8012EB60;
            }
            if (flags & 1) {
                for (i = (s16)angle; i < (s16)angle + 0x800; i += 0x400) {
                    prim           = (POLY_G4*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = *(u16*)&line->x0 + ((sc->r0 * rsin(i)) >> 12);
                    prim->y0 = *(u16*)&line->y0 + ((sc->r0 * rcos(i)) >> 12);
                    baseAng  = i + 0x200;
                    prim->x1 = *(u16*)&line->x0 + ((sc->r0 * rsin(baseAng)) >> 12);
                    prim->y1 = *(u16*)&line->y0 + ((sc->r0 * rcos(baseAng)) >> 12);
                    prim->x2 = *(u16*)&line->x0;
                    prim->y2 = *(u16*)&line->y0;
                    baseAng  = i + 0x400;
                    prim->x3 = *(u16*)&line->x0 + ((sc->r0 * rsin(baseAng)) >> 12);
                    prim->y3 = *(u16*)&line->y0 + ((sc->r0 * rcos(baseAng)) >> 12);
                    addPrim((u_long*)(((((u32)sc->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, sc->otz0);
                }
            }
            for (i = (s16)angle; i < (s16)angle + 0x800; i += 0x400) {
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                bodyAng        = (s16)angle + ((i - (s16)angle) * 2);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, r, g, b);
                setRGB3(prim, r, g, b);
                prim->x0 = *(u16*)&line->x0 + ((sc->r0 * rsin(bodyAng)) >> 12);
                prim->y0 = *(u16*)&line->y0 + ((sc->r0 * rcos(bodyAng)) >> 12);
                prim->x1 = *(u16*)&line->x1 + ((sc->r1 * rsin(bodyAng)) >> 12);
                prim->y1 = *(u16*)&line->y1 + ((sc->r1 * rcos(bodyAng)) >> 12);
                prim->x2 = *(u16*)&line->x0;
                prim->y2 = *(u16*)&line->y0;
                prim->x3 = *(u16*)&line->x1;
                prim->y3 = *(u16*)&line->y1;
                addPrim((u_long*)(((((u32)sc->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, sc->otz0);
            }
        } else {
            D_m4a1_javelin_8012EB64 = 1;
        }
    } else {
        D_m4a1_javelin_8012EB64 = 1;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(M4a1JavelinRingScratch);
}

/// Draws the javelin launcher's targeting reticle: a `LINE_F2` between the two
/// world-space points `p0` and `p1` plus three fans of `POLY_G4` wedges, all
/// dropped if either endpoint fails its `RTPS` `FLAG` check (which also arms
/// `D_m4a1_javelin_8012EB66` so the next frame re-measures the angle). Bit 1 of
/// `flags` forces that re-measurement: `ratan2` of the screen-space delta gives
/// the reticle's roll, which is cached in `D_m4a1_javelin_8012EB62` and reused
/// on the frames that do not. Bit 0 adds the near-end fan. `color` is a packed
/// `0x0RGB` nibble triple; each nibble is widened to a byte, biased by the
/// 8-unit dither of `Display_State.field_8` and halved. Each fan is four
/// quarter-turn wedges of radius `0x4000 / otz`, so the reticle keeps a
/// constant on-screen size as the target moves away.
void func_m4a1_javelin_8011E4A8(SVECTOR* p0, SVECTOR* p1, u16 flags, u16 color)
{
    u8*                     head;
    M4a1JavelinRingScratch* sc;
    LINE_F2*                line;
    POLY_G4*                poly;
    u32                     dither;
    u32                     c;
    u8                      r;
    u8                      g;
    u8                      b;
    u16                     ang;
    s32                     i;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(M4a1JavelinRingScratch);
    sc                    = (M4a1JavelinRingScratch*)(head - sizeof(M4a1JavelinRingScratch));

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(p0);
    gte_rtps_real();
    gte_stsxy(&((M4a1JavelinRingScratch*)head)[-1].sx0);
    gte_stflg(&((M4a1JavelinRingScratch*)head)[-1].flag);
    if (sc->flag < 0) {
        goto fail;
    }
    gte_stszotz(&((M4a1JavelinRingScratch*)head)[-1].otz0);
    ((M4a1JavelinRingScratch*)head)[-1].otz0++;
    gte_ldv0(p1);
    gte_rtps_real();
    gte_stsxy(&((M4a1JavelinRingScratch*)head)[-1].sx1);
    gte_stflg(&((M4a1JavelinRingScratch*)head)[-1].flag);
    if (sc->flag < 0) {
        goto fail;
    }
    gte_stszotz(&((M4a1JavelinRingScratch*)head)[-1].otz1);

    sc->otz1++;
    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    dither = (Display_State.field_8 & 1) * 8;
    c      = color & 0xFFFF;
    r      = (((c >> 4) & 0xF0) + dither) >> 1;
    g      = ((c & 0xF0) + dither) >> 1;
    b      = (((color & 0xF) << 4) + dither) >> 1;
    setRGB0(line, r, g, b);
    line->x0 = sc->sx0;
    line->y0 = sc->sy0;
    line->x1 = sc->sx1;
    line->y1 = sc->sy1;
    addPrim((u_long*)(((((u32)((M4a1JavelinRingScratch*)head)[-1].otz0 << Display_State.field_128) >> 2) & 0xFFC) +
                      (s32)Gpu_CurrentOt),
            line);
    Gp_AddTpageShift((P_TAG*)line, 1, ((M4a1JavelinRingScratch*)head)[-1].otz0);
    sc->r0 = 0x4000 / ((M4a1JavelinRingScratch*)head)[-1].otz0;
    sc->r1 = 0x4000 / sc->otz1;

    if ((flags & 2) || D_m4a1_javelin_8012EB66 != 0) {
        ang                     = ratan2(line->y1 - line->y0, line->x0 - line->x1);
        D_m4a1_javelin_8012EB62 = ang;
        D_m4a1_javelin_8012EB66 = 0;
        for (i = (s16)ang; i < (s16)ang + 0x800; i += 0x400) {
            poly           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
            setPolyG4(poly);
            setRGB0(poly, 0, 0, 0);
            setRGB1(poly, 0, 0, 0);
            setRGB2(poly, r, g, b);
            setRGB3(poly, 0, 0, 0);
            poly->x0 = *(u16*)&line->x1 + ((sc->r1 * rsin(i + 0x800)) >> 12);
            poly->y0 = *(u16*)&line->y1 + ((sc->r1 * rcos(i + 0x800)) >> 12);
            poly->x1 = *(u16*)&line->x1 + ((sc->r1 * rsin(i + 0xA00)) >> 12);
            poly->y1 = *(u16*)&line->y1 + ((sc->r1 * rcos(i + 0xA00)) >> 12);
            poly->x2 = line->x1;
            poly->y2 = line->y1;
            poly->x3 = *(u16*)&line->x1 + ((sc->r1 * rsin(i + 0xC00)) >> 12);
            poly->y3 = *(u16*)&line->y1 + ((sc->r1 * rcos(i + 0xC00)) >> 12);
            addPrim((u_long*)(((((u32)sc->otz1 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), poly);
            Gp_AddTpageShift((P_TAG*)poly, 1, sc->otz1);
        }
    } else {
        ang = D_m4a1_javelin_8012EB62;
    }

    if (flags & 1) {
        for (i = (s16)ang; i < (s16)ang + 0x800; i += 0x400) {
            poly           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
            setPolyG4(poly);
            setRGB0(poly, 0, 0, 0);
            setRGB1(poly, 0, 0, 0);
            setRGB2(poly, r, g, b);
            setRGB3(poly, 0, 0, 0);
            poly->x0 = *(u16*)&line->x0 + ((sc->r0 * rsin(i)) >> 12);
            poly->y0 = *(u16*)&line->y0 + ((sc->r0 * rcos(i)) >> 12);
            poly->x1 = *(u16*)&line->x0 + ((sc->r0 * rsin(i + 0x200)) >> 12);
            poly->y1 = *(u16*)&line->y0 + ((sc->r0 * rcos(i + 0x200)) >> 12);
            poly->x2 = line->x0;
            poly->y2 = line->y0;
            poly->x3 = *(u16*)&line->x0 + ((sc->r0 * rsin(i + 0x400)) >> 12);
            poly->y3 = *(u16*)&line->y0 + ((sc->r0 * rcos(i + 0x400)) >> 12);
            addPrim((u_long*)(((((u32)sc->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), poly);
            Gp_AddTpageShift((P_TAG*)poly, 1, sc->otz0);
        }
    }

    for (i = (s16)ang; i < (s16)ang + 0x800; i += 0x400) {
        poly           = (POLY_G4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
        setPolyG4(poly);
        setRGB0(poly, 0, 0, 0);
        setRGB1(poly, 0, 0, 0);
        setRGB2(poly, r, g, b);
        setRGB3(poly, r, g, b);
        poly->x0 = *(u16*)&line->x0 + ((sc->r0 * rsin((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->y0 = *(u16*)&line->y0 + ((sc->r0 * rcos((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->x1 = *(u16*)&line->x1 + ((sc->r1 * rsin((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->y1 = *(u16*)&line->y1 + ((sc->r1 * rcos((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->x2 = line->x0;
        poly->y2 = line->y0;
        poly->x3 = line->x1;
        poly->y3 = line->y1;
        addPrim((u_long*)(((((u32)sc->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), poly);
        Gp_AddTpageShift((P_TAG*)poly, 1, sc->otz0);
    }
    goto done;

fail:
    D_m4a1_javelin_8012EB66 = 1;
done:
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(M4a1JavelinRingScratch);
}

/* `otz0` is taken before the branch on purpose: the address is the same one
   already held for `sc`, so CSE turns it into the copy the ROM keeps, which a
   `&sc->otz0` inside the `if` would fold away. */
void func_m4a1_javelin_8011EE78(SVECTOR* p0, SVECTOR* p1, u16 brightness)
{
    u8*                     head;
    M4a1JavelinLineScratch* sc;
    LINE_G2*                line;
    s32*                    otz0;

    head                                      = *(u8**)G_SCRATCH_HEAD;
    sc                                        = (M4a1JavelinLineScratch*)(head - sizeof(M4a1JavelinLineScratch));
    *(M4a1JavelinLineScratch**)G_SCRATCH_HEAD = sc;
    otz0                                      = &sc->otz0;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(p0);
    gte_rtps_real();
    gte_stsxy(&sc->sx0);
    gte_stflg(&sc->flag);
    if (sc->flag >= 0) {
        gte_stszotz(otz0);
        sc->otz0++;
        gte_ldv0(p1);
        gte_rtps_real();
        gte_stsxy(&sc->sx1);
        gte_stflg(&sc->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&sc->otz1);
            sc->otz1++;
            line           = (LINE_G2*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
            setLineG2(line);
            setRGB0(line, brightness >> 2, brightness >> 1, brightness);
            setRGB1(line, 0, 0, 0);
            line->x0 = sc->sx0;
            line->y0 = sc->sy0;
            line->x1 = sc->sx1;
            line->y1 = sc->sy1;
            addPrim((u_long*)(((((u32)sc->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), line);
            Gp_AddTpageShift((P_TAG*)line, 1, sc->otz0);
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(M4a1JavelinLineScratch);
}

/// Links the billboarded muzzle-flare quad for one javelin launch frame into
/// `Gpu_CurrentOt`, dropped entirely if the source point fails its `RTPS`
/// `FLAG` check. `arg0` is the world-space point, `arg1` picks the 0x1F-wide
/// animation column of the flare texture, `arg2` is the half-extent in world
/// units and `arg3` the spin angle: the corners sit at `arg3` and
/// `arg3 + 0x400`, a quarter turn apart, so the quad stays square as it spins.
void func_m4a1_javelin_8011F0AC(M4a1JavelinVecLo* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**                  scratch;
    u8*                     head;
    M4a1JavelinQuadScratch* block;
    POLY_FT4*               prim;
    SVECTOR*                vec;
    s32                     u0;
    s32                     u1;
    s32                     ang2;
    u16                     vz;

    scratch                                                                    = (void**)G_SCRATCH_HEAD;
    head                                                                       = *scratch;
    ((M4a1JavelinQuadScratch*)(head - sizeof(M4a1JavelinQuadScratch)))->vec.vx = arg0->vx;
    block                                                                      = (M4a1JavelinQuadScratch*)(head - sizeof(M4a1JavelinQuadScratch));
    block->vec.vy                                                              = arg0->vy;
    vz                                                                         = arg0->vz;
    *scratch                                                                   = block;
    block->vec.vz                                                              = vz;
    vec                                                                        = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((M4a1JavelinQuadScratch*)(head - sizeof(M4a1JavelinQuadScratch)))->sx);
    gte_stflg(&((M4a1JavelinQuadScratch*)(head - sizeof(M4a1JavelinQuadScratch)))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1JavelinQuadScratch*)(head - sizeof(M4a1JavelinQuadScratch)))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        setClut(prim, 0x30, 0x10B);
        u0 = arg1 << 5;
        u1 = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *scratch = (u8*)*scratch + sizeof(M4a1JavelinQuadScratch);
}

void func_m4a1_javelin_8011F4A4(M4a1JavelinVecLo* arg0)
{
    if (arg0 == NULL) {
        D_m4a1_javelin_8012EB70 = 0;
        return;
    }
    D_m4a1_javelin_8012EB68.vx = arg0->vx;
    D_m4a1_javelin_8012EB68.vy = arg0->vy;
    D_m4a1_javelin_8012EB70    = 1;
    D_m4a1_javelin_8012EB68.vz = arg0->vz;
}

void func_m4a1_javelin_8011F4E8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0x200;
        Gp_LcgState   = (Gp_LcgState * 5) + 0x71357911;
        mem->field_26 = (Gp_LcgState >> 16) & 0xFFF;
        arg0->state   = 1;
    }
    func_m4a1_javelin_8011F0AC((M4a1JavelinVecLo*)&coord->workm.t, mem->field_22 - 1, mem->field_24, mem->field_26);
    if (mem->field_22 == 8) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// Per-frame firing state machine for the M4A1 javelin launcher, the sibling of
/// `func_m4a1_grenade_8011D1EC`. State 0 arms the shot and raises the weapon
/// (clip 8 instead of 1 when it was already up), state 1 waits for that clip.
/// State 2 branches on `field_97F`: a held trigger (bit 0) drops into the
/// three-round burst of state 3, a tap (bit 1) fires the single 0x101 javelin
/// of state 5, and anything else falls straight into the burst. State 3 counts
/// `field_934` down to each round, spending one javelin, playing `0x201D0004`
/// and spawning the muzzle flash; on the frame `field_934` reaches 2 it drops
/// the aim lock and spawns the 0x6003B impact marker, which state 4 also does
/// before parking in state 7. States 5 and 6 run the flight timer and feed the
/// tracked point to `func_m4a1_javelin_8011F4A4` (or clear it when nothing is
/// in range) so the guide line is drawn. State 7 runs the recoil timer down and
/// hands back to `func_80106550` once `func_80105894` is done or the timer has
/// run out.
///
/// `Wip_SysConfig.field_22` is the low byte `func_801061F0` packs into
/// `GameActor::field_124`. Reading it through the struct rather than as a bare
/// `extern u8` at 0x80073BAA is what keeps GCC from hoisting the `lbu` above
/// the `actor->` stores: a scalar global and a struct field do not alias, so
/// the scheduler is free to move the load, and the block comes out reordered.
void func_m4a1_javelin_8011F5D4(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    GpEffWork*     eff;
    s32            anim;
    s32            delay;
    s32            tick;
    u16            count;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 0x58;
    coord                   = arg0->extra->field_8;
    actor                   = arg0->actor;
    spot                    = (GsCOORDINATE2*)*(void**)G_SCRATCH_HEAD;
    spot->sub               = NULL;

    switch (actor->field_95E) {
        case 0:
            anim              = 1;
            actor->field_956  = 4;
            actor->field_954  = 0;
            actor->field_95C  = 0;
            actor->field_95E += anim;
            actor->field_12A |= 0x400;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            actor->field_981 = 0;
            if (actor->field_97F & 1) {
                actor->field_95E  = 3;
                actor->field_979  = 9;
                actor->field_95A  = 0;
                actor->field_934  = 0;
                actor->field_93E  = 3;
                actor->field_124  = Wip_SysConfig.field_22 | 0x21D00;
                actor->field_12A |= 0x800;
                func_80106238(arg0, 0, 1);
            } else if (actor->field_97F & 2) {
                actor->field_95E  = 5;
                actor->field_95A  = 2;
                actor->field_979  = 0x1C;
                actor->field_934  = 6;
                actor->field_124  = 0x21D1F;
                actor->field_12A &= 0xF7FF;
                func_80106238(arg0, 0, 0);
                Gp_ConsumeSlotQty(0x9C, 0x101);
                eff = Gp_SpawnEff(0x6002F,
                                  (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                  0x1D, NULL);
                if (eff != NULL) {
                    Task_Reparent(actor->field_91C, eff->field_0);
                }
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x201D0005, 1);
                Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 3);
                break;
            }
            /* fallthrough */
        case 3:
            count = actor->field_93E;
            if (actor->field_93E != 0) {
                delay = actor->field_934;
                if (delay == 0) {
                    actor->field_93E  = count - 1;
                    actor->field_934  = 3;
                    actor->field_981  = 0;
                    actor->field_12A |= 0xC000;
                    Gp_ConsumeSlotQty(0x9C, 1);
                    if (func_80106264(1) == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x201D0004, 1);
                    Gp_SpawnEff(0x6006B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                0x1D, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                } else {
                    delay--;
                    actor->field_934 = delay;
                    if (delay == 2) {
                        actor->field_12A &= 0x3FFF;
                        if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                            Gp_SpawnEff(0x6003B, spot, 0, NULL);
                            Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                        }
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->field_95E  = 7;
            actor->field_12A &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_SpawnEff(0x6003B, spot, 0, NULL);
                Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
            }
            break;
        case 5:
        case 6:
            tick             = actor->field_934 - 1;
            actor->field_934 = tick;
            if (tick == 0) {
                if (actor->field_95E == 5) {
                    actor->field_95E  = 6;
                    actor->field_934  = 0x1C;
                    actor->field_12A |= 0xC000;
                } else {
                    actor->field_95E  = 7;
                    actor->field_12A &= 0x3FFF;
                }
            }
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                func_m4a1_javelin_8011F4A4((M4a1JavelinVecLo*)spot->workm.t);
                eff = Gp_SpawnEff(0x60183, spot, 0, NULL);
                if (eff != NULL) {
                    Task_Reparent(actor->field_91C, eff->field_0);
                }
            } else {
                func_m4a1_javelin_8011F4A4(NULL);
            }
            break;
        case 7:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x58;
}
