#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "pe/pepper_spray.h"

/// `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

INCLUDE_ASM("pe/nonmatchings/pepper_spray/pepper_spray", func_pepper_spray_8012EF34);

INCLUDE_ASM("pe/nonmatchings/pepper_spray/pepper_spray", func_pepper_spray_8012F21C);

/* Every scratch vector address is computed off `head`, not off `blk`, so the
   loads and stores keep spelling the block out from `head` rather than reusing
   the `blk` register the way CSE off `blk` would. */
void func_pepper_spray_8012F634(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**              scratch;
    u8*                 head;
    PepperSprayScratch* blk;
    PepperSprayScratch* copy;
    POLY_G4*            prim;
    MATRIX*             wm;
    s32                 ang;
    s32                 back;
    s32                 depth;
    s16                 color;

    depth    = -0x200;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (PepperSprayScratch*)(head - sizeof(PepperSprayScratch));
    *scratch = blk;
    copy     = blk;
    color    = arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    ang = arg1;

    back         = ang - 0xC0;
    blk->v[0].vx = (u32)rsin(back) >> 4;
    blk->v[0].vy = (u32)rcos(back) >> 4;
    blk->v[0].vz = 0;
    wm           = &arg0->workm;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((PepperSprayScratch*)(head - 0x28))->v[0]);
    gte_rtv0_real();
    gte_stsv(&((PepperSprayScratch*)(head - 0x28))->v[0]);
    *(u16*)&blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&arg0->workm.t[2];

    blk->v[1].vx = (u32)rsin(ang) >> 1;
    blk->v[1].vy = (u32)rcos(ang) >> 1;
    blk->v[1].vz = depth;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((PepperSprayScratch*)(head - 0x28))->v[1]);
    gte_rtv0_real();
    gte_stsv(&((PepperSprayScratch*)(head - 0x28))->v[1]);
    *(u16*)&blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&arg0->workm.t[2];

    blk->v[2].vx = (u32)rsin(ang) >> 4;
    blk->v[2].vy = (u32)rcos(ang) >> 4;
    blk->v[2].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((PepperSprayScratch*)(head - 0x28))->v[2]);
    gte_rtv0_real();
    gte_stsv(&((PepperSprayScratch*)(head - 0x28))->v[2]);
    *(u16*)&blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&arg0->workm.t[0];
    ang                  = ang + 0xC0;
    *(u16*)&blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&arg0->workm.t[2];

    blk->v[3].vx = (u32)rsin(ang) >> 4;
    blk->v[3].vy = (u32)rcos(ang) >> 4;
    blk->v[3].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((PepperSprayScratch*)(head - 0x28))->v[3]);
    gte_rtv0_real();
    gte_stsv(&((PepperSprayScratch*)(head - 0x28))->v[3]);
    *(u16*)&blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((PepperSprayScratch*)(head - 0x28))->v[0]);
    gte_rtps_real();
    prim           = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_stflg(&((PepperSprayScratch*)(head - 0x28))->flag);
    if (blk->flag >= 0) {
        gte_ldv3(&((PepperSprayScratch*)(head - 0x28))->v[1],
                 &((PepperSprayScratch*)(head - 0x28))->v[2],
                 &((PepperSprayScratch*)(head - 0x28))->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&((PepperSprayScratch*)(head - 0x28))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(copy);
            ((PepperSprayScratch*)(head - 0x28))->otz++;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2 >> 1, arg2 >> 1, color);
            setRGB3(prim, 0, 0, 0);
            addPrim((u_long*)(((((u32)((PepperSprayScratch*)(head - 0x28))->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, ((PepperSprayScratch*)(head - 0x28))->otz);
        }
    }
    /* Restore through the symbol so `lui 0x1F80` can fill the `bltz` delay
       slots. A live `scratch` pointer would keep the address in `$fp`. */
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(PepperSprayScratch);
}

INCLUDE_RODATA("pe/nonmatchings/pepper_spray/pepper_spray", D_pepper_spray_8012EF30);
