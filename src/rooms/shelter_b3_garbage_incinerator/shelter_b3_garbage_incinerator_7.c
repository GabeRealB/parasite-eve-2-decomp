#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s8       D_8007218A;
extern u8       D_80073BA9;
extern TaskDesc D_80164190;

void func_shelter_b3_garbage_incinerator_80183E78(SVECTOR* v, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    u8                 lvl;
    u8                 r;
    u8                 g;
    u8                 b;

    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = head - 0x10;
    block                   = (RoomDraw13Scratch*)*(void**)G_SCRATCH_HEAD;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        lvl           = rsin(gDisplayState.animFrame * (s16)arg3) / 34 + 0x78;
        arg1          = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        r             = lvl * (((s16)arg2 >> 8) & 0xF) / 15;
        g             = lvl * (((s16)arg2 >> 4) & 0xF) / 15;
        b             = lvl * (arg2 & 0xF) / 15;
        ang           = 0;
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_7", func_shelter_b3_garbage_incinerator_801842A4);

void func_shelter_b3_garbage_incinerator_80184D7C(void)
{
}

void func_shelter_b3_garbage_incinerator_80184D84(Task* arg0)
{
    union {
        s32 msg[5];
        struct {
            u8 param1[8];
            u8 param2[8];
        } cd;
    } buf;
    s32 out;
    s32 v;

    switch (arg0->state) {
        case 0:
            v = D_80073BA9;
            if (D_8007218A == 1) {
                v = v + 1;
            } else {
                v = v + 0x22;
            }
            buf.msg[0] = v;
            buf.msg[1] = 1;
            buf.msg[2] = 0;
            buf.msg[3] = 0;
            buf.msg[4] = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)buf.msg, 0);
            arg0->spawnArg2 = Task_SpawnFromTable(&D_80164190, 0, 0, 0);
            arg0->state++;
            return;
        case 1:
            if (Task_PollKill(arg0->spawnArg2, &out) != 0) {
                arg0->state++;
            }
            return;
        case 2:
            arg0->state = 3;
            return;
        case 3:
            buf.cd.param1[2] = 0x22;
            buf.cd.param1[3] = 0;
            buf.cd.param1[0] = 0;
            buf.cd.param2[0] = 0x16;
            buf.cd.param2[1] = 0;
            buf.cd.param2[2] = 0;
            buf.cd.param2[3] = 0;
            CdCmd_Enqueue(0x21, buf.cd.param1, buf.cd.param2);
            taskKill(arg0);
            break;
    }
}
