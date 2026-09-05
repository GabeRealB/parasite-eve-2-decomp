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
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "weapons/m4a1_javelin.h"

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern u32 Gp_LcgState;

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011D1E4);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011DAB0);

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
