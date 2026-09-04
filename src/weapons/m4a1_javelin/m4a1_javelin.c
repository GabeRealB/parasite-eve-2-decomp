#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/m4a1_javelin.h"

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern u32 Gp_LcgState;

void func_m4a1_javelin_8011F0AC(M4a1JavelinVecLo* arg0, s16 arg1, s16 arg2, s16 arg3);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011D1E4);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011DAB0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011E4A8);

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

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011F0AC);

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

INCLUDE_RODATA("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", D_m4a1_javelin_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011F5D4);
