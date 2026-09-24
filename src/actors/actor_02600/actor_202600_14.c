#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_202600.h"
#include "main/display.h"

/// Scratchpad block the line draw takes below the scratchpad top: `position`
/// is the point handed to the GTE, `screen` and `depth` its projection. The
/// first 0x10 bytes are reserved but never touched.
typedef struct Actor202600LineScratch {
    /* 0x00 */ s32     unused[4];
    /* 0x10 */ SVECTOR position;
    /* 0x18 */ s32     screen;
    /* 0x1C */ s32     depth;
} Actor202600LineScratch;
STATIC_ASSERT_SIZEOF(Actor202600LineScratch, 0x20);

/// Draws a vertical semi-transparent gouraud line in the space of the matrix
/// `field_370`, from height `field_3A0 - 0x352` to height -0x352; nothing is
/// drawn when either end projects nearer than depth 30. The line runs from grey
/// 0x80 to 0xC0; while `field_3BC` counts down (never below 1) both ends are
/// scaled by `field_3BC / 45`.
void Actor02600_Fn02C94(Actor202600* actor)
{
    Actor202600LineScratch* s;
    Actor202600Work*        work;
    LINE_G2*                line;
    DR_TPAGE*               page;
    s32                     x;
    s32                     y;
    s32                     screen;
    s32                     screen1;
    s32                     shade;
    u8                      blue;

    s              = (Actor202600LineScratch*)(*(u8**)PSX_SCRATCH_ADDR(0x3FC) -= sizeof(Actor202600LineScratch));
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
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor202600LineScratch);
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
        *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor202600LineScratch);
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
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += sizeof(Actor202600LineScratch);
}
