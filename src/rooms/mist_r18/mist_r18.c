#include "common.h"

#include "gameplay/3CD8.h"

#include "main/display.h"

#include "main/session.h"

s32  func_8017A038(void);
void Room_Draw42(s32 tpage, s16 arg1);
void func_mist_r18_8017DBB8(s32 shade, s16 arg1);

extern u8 D_80071075;
extern s8 D_80114C12;

extern s32 D_mist_r18_8018576C;
extern s32 D_mist_r18_80185AE4;
extern s32 D_mist_r18_80185EBC;
extern s32 D_mist_r18_8018603C;
extern s32 D_mist_r18_801861BC;
extern s32 D_mist_r18_8018645C;
extern s32 D_mist_r18_8018651C;
extern s32 D_mist_r18_80186564;
extern s32 D_mist_r18_80186E9C;
extern s32 D_mist_r18_80186EA0;

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017D5F4);

void func_mist_r18_8017D960(void)
{
    s32 state;

    if ((Game_Session->field_1 == 0) && (D_80114C12 != 1) && (D_80071075 == 0)) {
        state = D_mist_r18_80186E9C;
        if (state == 1) {
            func_800E8634((s32)&D_mist_r18_80185EBC, 0, (s32)&D_mist_r18_80186564);
            D_mist_r18_80186E9C = 2;
        } else if (state == 2) {
            func_800E8634((s32)&D_mist_r18_8018576C, 0, (s32)&D_mist_r18_8018645C);
            D_mist_r18_80186EA0 = 0;
            D_mist_r18_80186E9C = 3;
        } else if (state == 3) {
            func_800E8614((s32)&D_mist_r18_8018603C, 0);
            D_mist_r18_80186E9C = 4;
        } else if (state == 4) {
            if (func_8017A038() != 1) {
                func_800E8614((s32)&D_mist_r18_801861BC, 0);
                D_mist_r18_80186EA0 = 1;
                return;
            }
            func_800E8634((s32)&D_mist_r18_80185AE4, 0, (s32)&D_mist_r18_8018651C);
            D_mist_r18_80186E9C = 5;
        }
    }
}

/// Fade task for the room's backdrop tint: state 0 arms the fade, states 1/3
/// ramp `killCountdown` up to 0x80 and back down to 0, state 2 holds until the
/// hold counter runs out (or the session's skip gate is set). Every state but
/// the last redraws through `func_mist_r18_8017DBB8`.
void func_mist_r18_8017DA8C(Task* task)
{
    s32 shade;

    shade = 1;
    switch (task->state) {
        case 0:
            task->killCountdown = 0;
            func_mist_r18_8017DBB8(1, 0);
            task->state++;
            break;
        case 1:
            task->killCountdown += 0x15;
            if (task->killCountdown >= 0x81) {
                shade = 0;
                task->state++;
            }
            break;
        case 2:
            shade = 0;
            task->spawnArg1--;
            if ((task->spawnArg1 <= 0) || (Game_Session->field_5F != 0)) {
                task->state++;
            }
            break;
        case 3:
            task->killCountdown -= 0x15;
            if (task->killCountdown < 0x16) {
                task->state++;
            }
            break;
        default:
            Task_Kill(task);
            return;
    }
    func_mist_r18_8017DBB8(shade, task->killCountdown);
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017DBB8);

/// Blit the room's backdrop out of the off-screen VRAM staging area into the
/// two framebuffer halves, bracketing both `MoveImage`s with STP writes so the
/// copied pixels keep their mask bit. The source row depends on which display
/// buffer is live, then the task advances a state.
void func_mist_r18_8017DD7C(Task* task)
{
    RECT     rect;
    DR_STP*  stp;
    DR_MOVE* mv;
    s16      x;
    s16      y;

    if (Display_State.field_1f == 0) {
        x = 0;
        y = 0;
    } else {
        x = 0;
        y = 0x110;
    }

    stp            = (DR_STP*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(Gpu_CurrentOt + 8, stp);

    mv             = (DR_MOVE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(mv + 1);
    rect.x         = x;
    rect.y         = y;
    rect.w         = 0xC0;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x340, 0);
    addPrim(Gpu_CurrentOt + 8, mv);

    mv             = (DR_MOVE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(mv + 1);
    rect.x         = x + 0xC0;
    rect.y         = y;
    rect.w         = 0x80;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x280, 0x100);
    addPrim(Gpu_CurrentOt + 8, mv);

    stp            = (DR_STP*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(Gpu_CurrentOt + 8, stp);

    task->killCountdown = 0;
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017DF80);

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void func_mist_r18_8017E144(s16 shade)
{
    SPRT* p;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(Gpu_CurrentOt + 8, p);
    Room_Draw42(0x340, 0);

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(Gpu_CurrentOt + 8, p);
    Room_Draw42(0x280, 0x100);
}

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017E2C8);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5DC);
