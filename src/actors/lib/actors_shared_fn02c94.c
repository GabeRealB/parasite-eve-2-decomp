#include "common.h"
#include "actors/actor_105500.h"
#include "main/display.h"
#include "psyq/inline_c.h"

void ActorsSharedFn02c94(Actor105500* actor)
{
    Actor105500LineScratch* s;
    Actor105500Work*        work;
    LINE_G2*                line;
    DR_TPAGE*               page;
    s32                     x;
    s32                     y;
    s32                     screen;
    s32                     screen1;
    s32                     shade;
    u8                      blue;

    s              = (Actor105500LineScratch*)(*(u8**)PSX_SCRATCH_ADDR(0x3FC) -= sizeof(Actor105500LineScratch));
    work           = actor->field_1C;
    s->position.vx = 0;
    s->position.vy = work->field_3A0 - 0x352;
    s->position.vz = 0;
    gte_SetRotMatrix(&work->field_370);
    gte_SetTransMatrix(&work->field_370);
    gte_ldv0(&s->position);
    __asm__ volatile("nop; nop; .word 0x4a180001");
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor105500LineScratch);
        return;
    }
    screen         = s->screen;
    x              = screen & 0xFFFF;
    y              = screen >> 16;
    s->position.vx = 0;
    s->position.vy = -0x352;
    s->position.vz = 0;
    gte_SetRotMatrix(&work->field_370);
    gte_SetTransMatrix(&work->field_370);
    gte_ldv0(&s->position);
    __asm__ volatile("nop; nop; .word 0x4a180001");
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor105500LineScratch);
        return;
    }
    line           = Gpu_PrimCursor;
    Gpu_PrimCursor = (u8*)line + sizeof(LINE_G2);
    screen1        = s->screen;
    setLineG2(line);
    setSemiTrans(line, 1);
    line->x0 = x;
    line->y0 = y;
    line->x1 = screen1;
    line->y1 = screen1 >> 16;
    if (work->field_3BC == 0) {
        line->b0 = line->g0 = line->r0 = 0x80;
        line->b1 = line->g1 = line->r1 = 0xC0;
    } else {
        if (--work->field_3BC <= 0) {
            work->field_3BC = 1;
        }
        shade    = (work->field_3BC * 0x80) / 45;
        line->r0 = shade;
        TOUCH_REG(shade);
        blue = shade;
        TOUCH_REG_USE(blue, shade);
        line->g0 = shade;
        line->b0 = blue;
        shade    = (work->field_3BC * 0xC0) / 45;
        line->r1 = shade;
        TOUCH_REG(shade);
        blue = shade;
        TOUCH_REG_USE(blue, shade);
        line->g1 = shade;
        line->b1 = blue;
    }
    addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), line);
    page           = Gpu_PrimCursor;
    Gpu_PrimCursor = (u8*)page + sizeof(DR_TPAGE);
    setlen(page, 1);
    page->code[0] = 0xE1000620;
    addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), page);
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor105500LineScratch);
}
