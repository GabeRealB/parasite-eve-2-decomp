#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_motel_balcony.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 Gp_LcgState;

extern void Room_Draw02(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3);

void func_dryfield_motel_balcony_80181628(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    flag  = Gp_State1C->field_4;
    mem   = arg0->spawnArg2;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->field_22++;
        switch (arg0->state) {
            case 0:
                Gp_SpawnEff(0x60076, coord, 0x400, NULL);
                if (arg0->spawnArg1 != 0) {
                    Gp_SpawnEff(0x60070, coord, 0x80004600, NULL);
                    arg0->state = 1;
                    return;
                }
                Gp_SpawnEff(0x6007C, coord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, coord, 0x100, NULL);
                mem->field_24 = 0x100;
                mem->field_26 = 0xC0;
                arg0->state   = 2;
                return;
            case 1:
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60070, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                            (SVECTOR*)&mem->field_10);
                break;
            case 2:
                mem->field_26 -= 0x20;
                mem->field_24 += 0x30;
                rgb[0]         = mem->field_26;
                rgb[1]         = (u16)mem->field_26 >> 1;
                rgb[2]         = (u16)mem->field_26 >> 2;
                Room_Draw02(coord, 0x100, 0x100, rgb);
                Room_Draw02(coord, mem->field_24, mem->field_24, rgb);
                break;
            case 3:
                goto kill;
            default:
                return;
        }
        if (mem->field_22 >= 7) {
            arg0->state = 3;
        }
        return;
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
void func_dryfield_motel_balcony_801818B0(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}