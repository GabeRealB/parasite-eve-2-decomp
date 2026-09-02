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

extern void func_dryfield_motel_balcony_8017EF44(GsCOORDINATE2* arg0, s16 arg1);
extern void Room_Draw09(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
extern void func_dryfield_motel_balcony_8017F7E8(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

void func_dryfield_motel_balcony_8017E66C(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomFanScratch* block asm("s2");
    register POLY_G4*        prim asm("s0");
    register s32             ang asm("s3");
    register void**          scratch asm("a1");
    register u8*             head asm("a2");
    s32                      otz;
    s32                      radius;
    s32                      t;
    s32                      t2;
    u16                      vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                       = *(u16*)&arg0->workm.t[0];
        ((RoomFanScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomFanScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomFanScratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomFanScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomFanScratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = (arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}
void func_dryfield_motel_balcony_8017EA00(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s16            flag;
    s32            shift;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->field_22++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->field_8;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[0] = mem->field_18;
                coord->coord.t[1] = mem->field_1A;
                coord->coord.t[2] = mem->field_1C;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_20   = shift;
                arg0->spawnArg1 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
                arg0->state     = 1;
                mem->field_2A   = 0x100 / arg0->spawnArg1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->field_24   += mem->field_2A;
                mem->field_26   += mem->field_2A;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->field_24 >> D_dryfield_motel_balcony_801822AC[mem->field_20].r;
                rgb[1]           = mem->field_24 >> D_dryfield_motel_balcony_801822AC[mem->field_20].g;
                rgb[2]           = mem->field_24 >> D_dryfield_motel_balcony_801822AC[mem->field_20].b;
                func_dryfield_motel_balcony_8017E66C(coord, mem->field_26, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->field_22 & 1) {
                    func_dryfield_motel_balcony_8017E66C(coord, mem->field_26 + 0x100, rgb);
                }
                Room_Draw09(coord, 0x300 - (u16)mem->field_26 * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->field_24 = 0xFF;
                    arg0->state   = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->field_24 >= 0x11) {
                    rgb[0] = mem->field_24 >> D_dryfield_motel_balcony_801822AC[mem->field_20].r;
                    rgb[1] = mem->field_24 >> D_dryfield_motel_balcony_801822AC[mem->field_20].g;
                    rgb[2] = mem->field_24 >> D_dryfield_motel_balcony_801822AC[mem->field_20].b;
                    func_dryfield_motel_balcony_8017F7E8(coord, (u16)mem->field_26 * 4, rgb);
                    mem->field_24 -= 0x10;
                    mem->field_26 += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
void func_dryfield_motel_balcony_8017ED98(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->field_22++;
        if (arg0->state == 0) {
            mem->field_22 = 1;
            mem->field_24 = 0xE0;
            mem->field_26 = 0x80;
            mem->field_28 = 0xE0;
            mem->field_2A = 0x80;
            arg0->state   = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]        = mem->field_24;
        rgb[1]        = (u16)mem->field_24 >> 1;
        rgb[2]        = (u16)mem->field_24 >> 2;
        step          = mem->field_26 + 0x10;
        mem->field_26 = step;
        func_dryfield_motel_balcony_8017E66C(coord, step * 2, rgb);
        func_dryfield_motel_balcony_8017EF44(coord, mem->field_26);
        if (mem->field_28 >= 0x19) {
            rgb[0] = mem->field_28;
            rgb[1] = (u16)mem->field_28 >> 1;
            rgb[2] = (u16)mem->field_28 >> 2;
            Room_Draw09(coord, mem->field_2A * 3 / 2, 0x60, rgb);
            mem->field_28 -= 0x18;
            mem->field_2A += 0x30;
            return;
        }
        mem->field_24 -= 0x18;
        if (mem->field_24 < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}
INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony_2", func_dryfield_motel_balcony_8017EF44);
