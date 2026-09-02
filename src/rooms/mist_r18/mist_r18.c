#include "common.h"

#include "gameplay/3CD8.h"

#include "main/display.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/mist_r18.h"

s32  func_8017A038(void);
void func_mist_r18_8017E448(MistR18Sprite* sprite);
void func_mist_r18_8017E534(MistR18Sprite* sprite, u32 clutX, s32 clutY);
void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx);
void func_mist_r18_8017DBB8(s32 shade, s32 arg1);

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

/// Typewriter text task for the room's message box: state 0 measures the
/// script (or, for a negative per-glyph delay, reveals all of it at once) and
/// arms the countdown, state 1 draws the revealed glyphs each frame and
/// advances one glyph whenever the countdown runs out. Any other state, or a
/// session that has left the message, kills the task.
void func_mist_r18_8017D5F4(Task* task)
{
    MistR18Sprite     sprite;
    GameSession*      session;
    MistR18TextSpawn* spawn;
    s32               i;

    spawn = task->spawnArg2;
    if (session->field_1 == 0) {
        task->state = -1;
    }

    switch (task->state) {
        case 0:
            spawn->index = 0;
            if (spawn->delay < 0) {
                i = 0;
                if (spawn->script[0] != 0xFF) {
                    do {
                        i++;
                        spawn->index++;
                    } while (spawn->script[i] != 0xFF);
                }
                task->killCountdown = spawn->delayEnd;
            } else {
                task->killCountdown = spawn->delay;
            }
            break;

        case 1:
            sprite.x         = spawn->x;
            sprite.y         = spawn->y;
            sprite.u         = spawn->u;
            sprite.v         = spawn->v;
            sprite.r         = 0x80;
            sprite.g         = 0x80;
            sprite.b         = 0x80;
            sprite.semiTrans = 0;
            sprite.scale     = ONE;

            if (spawn->script[spawn->index - 1] == 0xFF) {
                break;
            }

            for (i = 0; i < spawn->index; i++) {
                if (spawn->script[i] == 0xFE) {
                    sprite.x  = spawn->x;
                    sprite.y += spawn->lineHeight;
                } else {
                    sprite.u = spawn->glyphs[spawn->script[i]].u + (spawn->u & 0x3F);
                    sprite.v = spawn->glyphs[spawn->script[i]].v + (u8)spawn->v;
                    sprite.w = spawn->glyphs[spawn->script[i]].w;
                    sprite.h = spawn->glyphs[spawn->script[i]].h;
                    if (sprite.h != 0) {
                        func_mist_r18_8017E534(&sprite, spawn->clutX, spawn->clutY);
                    }
                    sprite.x += spawn->glyphs[spawn->script[i]].w;
                }
            }

            func_mist_r18_8017E654(1, spawn->u, spawn->v, 4);

            if (--task->killCountdown < 0) {
                spawn->index++;
                if (spawn->script[spawn->index] == 0xFF) {
                    task->killCountdown = spawn->delayEnd;
                } else {
                    task->killCountdown = spawn->delay;
                }
            }

            sprite.x         = spawn->x - 3;
            sprite.y         = spawn->y - 3;
            sprite.w         = spawn->boxW;
            sprite.h         = spawn->boxH;
            sprite.b         = 0;
            sprite.g         = 0;
            sprite.r         = 0;
            sprite.semiTrans = 1;
            func_mist_r18_8017E448(&sprite);
            func_mist_r18_8017E654(0, 0, 0, 5);
            return;

        default:
            goto kill;
    }
    task->state++;
    return;

kill:
    Task_Kill(task);
}

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

/// Draw the room's two backdrop tint sprites (upper-left and lower-right
/// halves of the mist overlay) plus the trailing tpage packet. `shade` picks
/// the sprite code - shade-texture (0x65) while the fade is ramping in,
/// semi-transparent (0x66) otherwise - and `arg1` is the grey level written
/// into all three colour channels.
void func_mist_r18_8017DBB8(s32 shade, s32 arg1)
{
    SPRT*     sprt;
    DR_TPAGE* tp;
    s16       x;
    s16       y;

    x              = -0x96;
    y              = -0x5A;
    sprt           = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
    setSprt(sprt);
    if (shade == 0) {
        sprt->code = 0x65;
    } else {
        sprt->code = 0x66;
    }
    setXY0(sprt, x, y);
    sprt->clut = 0x43C0;
    setWH(sprt, 0xCF, 0x23);
    setRGB0(sprt, arg1, arg1, arg1);
    setUV0(sprt, 0, 0);
    addPrim(Gpu_CurrentOt + 4, sprt);

    x              = -0x22;
    y              = 0x36;
    sprt           = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
    setSprt(sprt);
    if (shade == 0) {
        sprt->code = 0x65;
    } else {
        sprt->code = 0x66;
    }
    setXY0(sprt, x, y);
    setRGB0(sprt, arg1, arg1, arg1);
    setUV0(sprt, 0, 0x24);
    sprt->clut = 0x43C1;
    setWH(sprt, 0xB7, 0x23);
    addPrim(Gpu_CurrentOt + 4, sprt);

    tp             = Gpu_PrimCursor;
    Gpu_PrimCursor = tp + 1;
    setDrawTPage(tp, 1, 0, 0x2B);
    addPrim(Gpu_CurrentOt + 4, tp);
}

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

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5DC);
