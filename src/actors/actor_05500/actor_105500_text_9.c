#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_105500.h"
#include "main/display.h"

/// Draws a semi-transparent gouraud line through the actor's frame
/// `field_370`, from local height `field_3A0 - 0x352` down to -0x352, followed
/// by a texture-page primitive at the same depth; nothing is drawn when either
/// end is nearer than depth 30. With `field_3BC` at zero the ends are grey
/// levels 0x80 and 0xC0; otherwise `field_3BC` is decremented, holding at 1,
/// and scales both levels by `field_3BC / 45`.
void Actor05500_Fn02C94(Actor105500* actor)
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
    gte_rtps();
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
    gte_rtps();
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor105500LineScratch);
        return;
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = (u8*)line + sizeof(LINE_G2);
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
    addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), line);
    page           = gGpuPrimCursor;
    gGpuPrimCursor = (u8*)page + sizeof(DR_TPAGE);
    setlen(page, 1);
    page->code[0] = 0xE1000620;
    addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), page);
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor105500LineScratch);
}
