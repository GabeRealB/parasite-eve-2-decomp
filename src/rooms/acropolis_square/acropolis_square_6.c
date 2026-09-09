#include "common.h"

#include "decomp/common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_square.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 D_80070F70;
extern s16 D_acropolis_square_80183B68[];

extern TaskDesc D_acropolis_square_80183808;
extern s32      D_acropolis_square_80183B98;
extern s32      D_acropolis_square_80183B58;
extern s32      D_acropolis_square_80183830;
extern s32      D_acropolis_square_8018399C;
extern s32      D_acropolis_square_80183A5C;

s32 func_acropolis_square_80182360(void)
{
    GpAreaKey key;

    if (GameFlag_GetNibble(0x1F) == 0) {
        GameFlag_SetNibble(0x1F, 1);
        key.field_3 = 1;
        key.field_2 = 1;
        Gp_SetAreaObjId(&key, 2, 1);
        Game_Session->field_1 = 1;
        Task_SpawnFromTable(&D_acropolis_square_80183808, 0, 0, 0);
        return 0;
    }
    return 1;
}

void func_acropolis_square_801823DC(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    switch (task->state) { /* irregular */
        case 0:
            task->field_24 = &D_acropolis_square_80183B58;
            Game_SetPtrSlot(task, 5);
            D_acropolis_square_80183B98 = 0;
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            return;
        case 1:
            if ((0x268 >> ((u8)Game_Session->field_4 - 1)) & 1) {
                work->field_10.vx = 0x19AA;
                work->field_10.vy = -0xF96;
                work->field_10.vz = 0x8DE;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x10000218 + 0x10E08,
                            &work->field_10);
            }
            if ((u8)Game_Session->field_4 == 0xE) {
                work->field_10.vx = 0x18D2;
                work->field_10.vy = -0x100B;
                work->field_10.vz = 0x8AB;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x218 + 0x10010608,
                            &work->field_10);
            }
            if ((u8)Game_Session->field_4 == 9) {
                work->field_10.vx = 0x19AA;
                work->field_10.vy = -0xF96;
                work->field_10.vz = 0x8E8;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x118 + 0x80010308,
                            &work->field_10);
            }
            return;
    }
}

void func_acropolis_square_801825DC(Task* task)
{
    u8*                         head;
    u8*                         raw;
    AcropolisSquareBeamScratch* blk;
    POLY_G4*                    prim;
    LINE_G3*                    line;
    GsCOORDINATE2*              coord;
    void*                       mem;
    u16                         vz;
    s32                         i;
    s32                         pulse;
    s32                         level;
    s32                         height;
    s16                         amp;
    s16                         flip;
    s32                         ampSi;
    s32                         ampHalf;
    u8                          red;
    u8                          cyan;
    s32                         z;
    s32                         shift;
    u32                         depth;
    u32                         tag;
    u_long*                     ot;

    coord = ((TmdObject*)task->extra)->field_8;
    mem   = task->spawnArg2;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x18;
    SOFT_TOUCH_REG(raw);
    blk                     = (AcropolisSquareBeamScratch*)raw;
    blk->vec.vx             = *(u16*)&coord->workm.t[0];
    blk->vec.vy             = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = blk;
    blk->vec.vz             = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisSquareBeamScratch*)(head - 0x18))->vec);
    gte_rtps_real();
    gte_stsxy(&((AcropolisSquareBeamScratch*)(head - 0x18))->sx);
    gte_stszotz(&blk->otz);
    if (((AcropolisSquareBeamScratch*)(head - 0x18))->otz >= 0x11) {
        pulse  = D_80070F70;
        pulse *= task->spawnArg1 & 0xFF;
        flip   = (task->spawnArg1 >> 16) & 1;
        if (pulse & 0x80) {
            level = ~pulse & 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        amp   = level * 2;
        level = task->spawnArg1;
        if (level < 0) {
            height      = (level >> 8) & 0xFF;
            blk->rOuter = (height * 0x600) / blk->otz;
            blk->rInner = (height * 0xC0) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                ampSi          = amp;
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (ampSi * (flip ^ 1)) >> 1, (flip * ampSi) >> 1, (flip * ampSi) >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 12);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, ampSi * (flip ^ 1), flip * ampSi, flip * ampSi);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            ampHalf = amp >> 1;
            for (i = 2; i < 0x10; i += 8) {
                do {
                    prim           = (POLY_G4*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    red  = ampHalf * (flip ^ 1);
                    cyan = flip * ampHalf;
                    setRGB2(prim, red, cyan, cyan);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i]) >> 12);
                    prim->y0 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i - 4]) >> 12);
                    prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 11);
                    prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 11);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 8]) >> 12);
                    prim->y3 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i + 4]) >> 12);
                    shift    = Display_State.field_128;
                    depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                    __asm__("" : "+r"(depth) : "r"(shift), "m"(Display_State.field_128));
                    setaddr(prim, getaddr((u_long*)(depth + (s32)Gpu_CurrentOt)));
                    ot  = (u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt);
                    tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                    *ot = tag;
                    z   = blk->otz;
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG_USE(z, tag);
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);

                    prim           = (POLY_G4*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, cyan, cyan);
                } while (0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 8]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 0xC]) >> 13);
                prim->y3 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i + 8]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                z = blk->otz;
                __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_square_80183B68[i]));
                __asm__("" : "+r"(prim) : "r"(z), "r"(cyan));
                Gp_AddTpageShift((P_TAG*)prim, 1, z);
            }
        } else {
            blk->rOuter = (((level >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, amp * (flip ^ 1), flip * amp, flip * amp);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->rOuter;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->rOuter;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->rOuter) + blk->rOuter * (i + i);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (task->spawnArg1 & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)((u8*)line + sizeof(LINE_G3));
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, amp * (flip ^ 1), flip * amp, flip * amp);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->rOuter * (i * 3 - 1);
                    line->y0 = blk->sy - blk->rOuter * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->rOuter * (i * 3 - 1);
                    line->y2 = blk->sy + blk->rOuter * (i + 1);
                    addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) +
                                      (s32)Gpu_CurrentOt),
                            line);
                    Gp_AddTpageShift((P_TAG*)line, 1, blk->otz);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    Gp_ReleaseState1CMem(mem, task);
}

s32 func_acropolis_square_8018344C(s32 arg0, s32 arg1, s32 arg2)
{
    D_acropolis_square_80183B98 = arg2;
    return 0;
}

void func_acropolis_square_8018345C(void)
{
}
