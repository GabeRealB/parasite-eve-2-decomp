#include "common.h"
#include "gameplay/gameplay.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

/// Sprite description the overlay's two primitive emitters read from.
///
/// `func_mist_r18_8017E448` turns it into a flat-shaded `TILE` and
/// `func_mist_r18_8017E534` into a textured `SPRT`; both take the screen
/// rectangle from `x`/`y`/`w`/`h`, the colour from `r`/`g`/`b`, and pick the
/// `SetShadeTex` argument from `semiTrans`. Only the textured emitter reads
/// `u`/`v`, and only their low bytes: the text renderer builds them as
/// halfwords (glyph offset plus the page origin) before handing the sprite
/// over.
typedef struct MistR18Sprite {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 u;
    /* 0x06 */ s16 v;
    /* 0x08 */ s16 w;
    /* 0x0A */ s16 h;
    /* 0x0C */ u8  r;
    /* 0x0D */ u8  g;
    /* 0x0E */ u8  b;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 semiTrans;
    /* 0x12 */ s16 scale;
} MistR18Sprite;

STATIC_ASSERT_SIZEOF(MistR18Sprite, 0x14);

/// One glyph cell in the font table `MistR18TextSpawn::glyphs`: its position
/// within the texture page and the size of its screen rectangle. A cell whose
/// `h` is 0 (a space) draws nothing but still advances the pen by `w`.
typedef struct MistR18Glyph {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
} MistR18Glyph;

STATIC_ASSERT_SIZEOF(MistR18Glyph, 0x4);

/// Spawn descriptor for the typewriter text task `func_mist_r18_8017D5EC`
/// drives, hung off `Task::spawnArg2`.
///
/// `script` is a run of glyph indices terminated by 0xFF, with 0xFE as a line
/// break; `index` is how many of them are revealed so far and doubles as the
/// draw count. `delay` is the per-glyph frame count reloaded into
/// `Task::killCountdown`, `delayEnd` the one used once the terminator is
/// reached; a negative `delay` reveals the whole script at once. `x`/`y` is
/// the pen origin, `u`/`v` the font page origin, `clutX`/`clutY` the palette
/// position and `boxW`/`boxH` the backing rectangle drawn behind the text.
typedef struct MistR18TextSpawn {
    /* 0x00 */ s16           x;
    /* 0x02 */ s16           y;
    /* 0x04 */ s16           u;
    /* 0x06 */ s16           v;
    /* 0x08 */ s16           clutX;
    /* 0x0A */ s16           clutY;
    /* 0x0C */ s16           delay;
    /* 0x0E */ s16           index;
    /* 0x10 */ u8*           script;
    /* 0x14 */ MistR18Glyph* glyphs;
    /* 0x18 */ s16           lineHeight;
    /* 0x1A */ s16           delayEnd;
    /* 0x1C */ s16           boxW;
    /* 0x1E */ s16           boxH;
} MistR18TextSpawn;

STATIC_ASSERT_SIZEOF(MistR18TextSpawn, 0x20);

/// Spawn descriptor for the sprite task `func_mist_r18_8017E3A4` drives: the
/// screen rectangle it redraws every frame, hung off `Task::spawnArg2`.
typedef struct MistR18SpriteSpawn {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
} MistR18SpriteSpawn;

STATIC_ASSERT_SIZEOF(MistR18SpriteSpawn, 0x8);

s32  func_8017A038(void);
void func_80179FC8(s32 arg0, s32 arg1);

void func_mist_r18_8017D960(Task* task);
void func_mist_r18_8017DBB8(s32 shade, s32 arg1);
void func_mist_r18_8017DD7C(Task* task);
void func_mist_r18_8017E320(Task* task);
void func_mist_r18_8017E39C(Task* task);
void func_mist_r18_8017E448(MistR18Sprite* sprite);
void func_mist_r18_8017E534(MistR18Sprite* sprite, u32 clutX, s32 clutY);
void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx);
void func_mist_r18_8017E8B8(Task* task);
void func_mist_r18_8017E92C(Task* task);
void func_mist_r18_8017E994(s32 tpage, s16 arg1);
void func_mist_r18_8017ECF4(Task* arg0);

extern s8 D_801156F9;

/// The room's task-spawn table; its entries are started by index from the
/// room's callbacks.
extern TaskDesc D_mist_r18_80184F04;
/// Spawn descriptor handed to entry 5 of `D_mist_r18_80184F04`.
extern s32 D_mist_r18_80184EE4;
extern s32 D_mist_r18_8018522C;
extern s32 D_mist_r18_8018576C;
extern s32 D_mist_r18_80185AE4;
extern s32 D_mist_r18_80185EBC;
extern s32 D_mist_r18_8018603C;
extern s32 D_mist_r18_801861BC;
extern s32 D_mist_r18_8018639C;
extern s32 D_mist_r18_8018645C;
extern s32 D_mist_r18_8018651C;
extern s32 D_mist_r18_80186564;
/// The two prop tasks `func_mist_r18_8017E6D8` spawns and
/// `func_mist_r18_8017E784` tears down, by index.
extern Task* D_mist_r18_80186E90;
extern Task* D_mist_r18_80186E94;
/// Handle of the task `func_mist_r18_8017EA2C` spawns.
extern Task* D_mist_r18_80186E98;
/// Step of the cutscene sequence `func_mist_r18_8017D960` walks.
extern s32 D_mist_r18_80186E9C;
/// Set by `func_mist_r18_8017D960` when the alternate cutscene branch ran.
extern s32 D_mist_r18_80186EA0;

/// State handlers of the attached-model task `func_mist_r18_8017E2C8`
/// dispatches: attach to the parent's part, an empty idle state, then
/// `taskKill`.
const TaskFuncTable3 D_mist_r18_8017D5C4 = {
    { func_mist_r18_8017E320, func_mist_r18_8017E39C, taskKill },
};

/// State handlers of the room's cutscene task `func_mist_r18_8017ED64`
/// dispatches: set-up, the cutscene step, then `taskKill`.
const TaskFuncTable3 D_mist_r18_8017D5D0 = {
    { func_mist_r18_8017ECF4, func_mist_r18_8017D960, taskKill },
};

/// State handlers of the backdrop task `func_mist_r18_8017E854` dispatches:
/// blit the backdrop into the framebuffer, fade it in, fade it out, then
/// `taskKill`.
const TaskFuncTable4 D_mist_r18_8017D5DC = {
    { func_mist_r18_8017DD7C, func_mist_r18_8017E8B8, func_mist_r18_8017E92C, taskKill },
};

/// Typewriter text task for the room's message box: state 0 measures the
/// script (or, for a negative per-glyph delay, reveals all of it at once) and
/// arms the countdown, state 1 draws the revealed glyphs each frame and
/// advances one glyph whenever the countdown runs out. Any other state, or a
/// session that has left the message, kills the task.
void func_mist_r18_8017D5EC(Task* task)
{
    MistR18Sprite     sprite;
    MistR18TextSpawn* spawn;
    s32               i;

    spawn = task->spawnArg2;
    if (gGameSession->eventState == 0) {
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
    taskKill(task);
}

/// Cutscene step of the room's cutscene task: while no event is running and
/// neither gate is set, start the next script of the sequence whose step
/// `D_mist_r18_80186E9C` holds. At step 4 it branches on `func_8017A038`,
/// setting `D_mist_r18_80186EA0` and staying on that step when the alternate
/// script runs.
void func_mist_r18_8017D960(Task* task)
{
    s32 state;

    if ((gGameSession->eventState == 0) && (Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
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
            if ((task->spawnArg1 <= 0) || (gGameSession->evtSkipped != 0)) {
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
            taskKill(task);
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
    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
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
    addPrim(gGpuCurrentOt + 4, sprt);

    x              = -0x22;
    y              = 0x36;
    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
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
    addPrim(gGpuCurrentOt + 4, sprt);

    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setDrawTPage(tp, 1, 0, 0x2B);
    addPrim(gGpuCurrentOt + 4, tp);
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

    if (gDisplayState.drawBuffer == 0) {
        x = 0;
        y = 0;
    } else {
        x = 0;
        y = 0x110;
    }

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + 8, stp);

    mv             = (DR_MOVE*)gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x;
    rect.y         = y;
    rect.w         = 0xC0;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x340, 0);
    addPrim(gGpuCurrentOt + 8, mv);

    mv             = (DR_MOVE*)gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x + 0xC0;
    rect.y         = y;
    rect.w         = 0x80;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x280, 0x100);
    addPrim(gGpuCurrentOt + 8, mv);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 8, stp);

    task->killCountdown = 0;
    task->state++;
}

/// Redraw the room's two backdrop halves as opaque `SPRT`s in OT slot 8,
/// tinting both with `shade`. Which display buffer is live shifts the source
/// rows in the off-screen staging area, so both the sprites' `v` texcoord and
/// the tpage row handed to `func_mist_r18_8017E994` move with it.
void func_mist_r18_8017DF80(s32 shade)
{
    SPRT* p;
    s16   tpageY;
    u8    u;
    u8    v;

    if (gDisplayState.drawBuffer == 0) {
        tpageY = 0;
        u      = 0;
        v      = 0;
    } else {
        tpageY = 0x100;
        u      = 0;
        v      = 0x10;
    }

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = u;
    p->v0   = v;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_mist_r18_8017E994(0, tpageY);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->x0   = 0x20;
    p->u0   = u;
    p->v0   = v;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_mist_r18_8017E994(0xC0, tpageY);
}

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void func_mist_r18_8017E144(s16 shade)
{
    SPRT* p;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
    addPrim(gGpuCurrentOt + 8, p);
    func_mist_r18_8017E994(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
    addPrim(gGpuCurrentOt + 8, p);
    func_mist_r18_8017E994(0x280, 0x100);
}

/// Per-frame entry point of the attached-model task: run the handler its state
/// selects from `D_mist_r18_8017D5C4` (attach to the parent's part, an empty
/// idle state, then `taskKill`), copied onto the stack each frame.
void func_mist_r18_8017E2C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r18_8017D5C4;
    sp.funcs[task->state](task);
}

/// Attach this task's model to part `Task::spawnArg1` of the parent task in
/// `Task::spawnArg2`: parent the coordinate system to that part's, share the
/// parent's light and colour matrices, reparent the task under it and step
/// past the set-up state.
void func_mist_r18_8017E320(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GpCoord*   coord;
    GpCoord*   dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = task->extra.tmd;
    parentExtra     = parent->extra.tmd;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Idle state of the attached-model task: nothing to do until it is killed.
void func_mist_r18_8017E39C(Task* task)
{
}

/// Redraw the room's sprite rectangle each frame until the spawn countdown in
/// `Task::spawnArg1` runs out, then kill the task.
void func_mist_r18_8017E3A4(Task* task)
{
    MistR18Sprite       sprite;
    MistR18SpriteSpawn* spawn;

    spawn = task->spawnArg2;

    if (task->state == 0) {
        sprite.x         = spawn->x;
        sprite.y         = spawn->y;
        sprite.w         = spawn->w;
        sprite.h         = spawn->h;
        sprite.b         = 0;
        sprite.g         = 0;
        sprite.r         = 0;
        sprite.semiTrans = 1;
        func_mist_r18_8017E448(&sprite);
        func_mist_r18_8017E654(0, 0, 0, 5);

        if (--task->spawnArg1 > 0) {
            return;
        }
    }
    taskKill(task);
}

/// Emit the sprite's screen rectangle as a flat-shaded `TILE` into OT slot 5.
void func_mist_r18_8017E448(MistR18Sprite* sprite)
{
    TILE* tile;

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    if (sprite->semiTrans == 0) {
        SetShadeTex(tile, 1);
        SetSemiTrans(tile, 0);
    } else {
        SetShadeTex(tile, 0);
        SetSemiTrans(tile, 1);
    }
    tile->r0 = sprite->r;
    tile->g0 = sprite->g;
    tile->b0 = sprite->b;
    tile->x0 = sprite->x;
    tile->y0 = sprite->y;
    tile->w  = sprite->w - 1;
    tile->h  = sprite->h - 1;
    AddPrim(gGpuCurrentOt + 5, tile);
}

/// Emit the sprite's screen rectangle as a textured `SPRT` into OT slot 4,
/// with the CLUT taken from the framebuffer position `clutX`/`clutY`.
void func_mist_r18_8017E534(MistR18Sprite* sprite, u32 clutX, s32 clutY)
{
    SPRT* p;
    u8    v;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    SetSprt(p);
    if (sprite->semiTrans == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0   = sprite->r;
    p->g0   = sprite->g;
    p->b0   = sprite->b;
    p->x0   = sprite->x;
    p->y0   = sprite->y;
    p->u0   = sprite->u;
    v       = sprite->v;
    p->clut = getClut(clutX, clutY);
    p->v0   = v;
    p->w    = sprite->w - 1;
    p->h    = sprite->h - 1;
    AddPrim(gGpuCurrentOt + 4, p);
}

/// Append a `DR_TPAGE` for the given tpage to OT slot `otIdx`.
void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx)
{
    DR_TPAGE* dr;

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    SetDrawTPage(dr, 1, 0, GetTPage(0, abr, x, y));
    AddPrim(gGpuCurrentOt + otIdx, dr);
}

/// Spawn prop task `idx` (0 or 1) into its slot if it is not already running,
/// and clear bit 7 of its model's flags. Other indices do nothing.
void func_mist_r18_8017E6D8(s32 idx)
{
    Task** slot;
    Task*  task;

    switch (idx) {
        case 0:
            slot = &D_mist_r18_80186E90;
            break;
        case 1:
            slot = &D_mist_r18_80186E94;
            break;
        default:
            slot = NULL;
            break;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = Task_SpawnFromTable(&D_mist_r18_80184F04, idx, 8, (s32)gameGetPtrSlot(3));
        *slot = task;
        if (task != NULL) {
            task->extra.tmd->flags &= 0xFF7F;
        }
    }
}

/// Kill and clear prop task `idx` (0 or 1); other indices do nothing.
void func_mist_r18_8017E784(s32 idx)
{
    if (idx == 0) {
        if (D_mist_r18_80186E90 != NULL) {
            taskKill(D_mist_r18_80186E90);
        }
        D_mist_r18_80186E90 = NULL;
    } else if (idx == 1) {
        if (D_mist_r18_80186E94 != NULL) {
            taskKill(D_mist_r18_80186E94);
        }
        D_mist_r18_80186E94 = NULL;
    }
}

void func_mist_r18_8017E7F0(void)
{
    Task_SpawnFromTable(&D_mist_r18_80184F04, 5, 0, (s32)&D_mist_r18_80184EE4);
}

/// Spawn entry 3 of the room's task table.
void func_mist_r18_8017E824(void)
{
    Task_SpawnFromTable(&D_mist_r18_80184F04, 3, 0, 0);
}

/// Per-frame entry point of the backdrop task: run the handler its state
/// selects from `D_mist_r18_8017D5DC`, copied onto the stack each frame.
void func_mist_r18_8017E854(Task* task)
{
    TaskFuncTable4 states;

    states = D_mist_r18_8017D5DC;
    states.funcs[task->state](task);
}

/// Fade the room in. `Task::killCountdown` is reused as the 0..0x80 fade level.
void func_mist_r18_8017E8B8(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown + 8;
    task->killCountdown = fade;
    if ((s16)fade >= 0x40) {
        task->killCountdown = 0x40;
    }
    if ((gGameSession->viewReady != 0) || ((u8)gGameSession->at4.loc.view != 2)) {
        task->killCountdown = 0x80;
        task->state++;
    }
}

/// Fade the room back out eight levels a frame, driving both backdrop redraws
/// with complementary shades, and advance the task's state once the level
/// bottoms out. `Task::killCountdown` holds the level.
void func_mist_r18_8017E92C(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    func_mist_r18_8017E144(task->killCountdown);
    func_mist_r18_8017DF80(0x80 - task->killCountdown);
}

/// Append a 15-bit, ABR-1 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 8.
void func_mist_r18_8017E994(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 8, p);
}

/// Spawn entry 4 of the room's task table and keep its handle in
/// `D_mist_r18_80186E98`, which `func_mist_r18_8017EA60` kills.
void func_mist_r18_8017EA2C(void)
{
    D_mist_r18_80186E98 = Task_SpawnFromTable(&D_mist_r18_80184F04, 4, 0, 0);
}

void func_mist_r18_8017EA60(void)
{
    if (D_mist_r18_80186E98 != NULL) {
        taskKill(D_mist_r18_80186E98);
    }
    D_mist_r18_80186E98 = NULL;
}

void func_mist_r18_8017EA98(Task* task)
{
    GpCoord*   coord;
    TmdObject* obj;

    if (task->state == 0) {
        coord               = task->extra.tmd->coords;
        coord->coord.t[0]   = -0x1496;
        coord->coord.t[1]   = -0x2DA;
        coord->coord.t[2]   = 0xB90;
        coord->param.rot.vx = 0x6AA;
        coord->param.rot.vy = -0xF8E;
        coord->param.rot.vz = -0x333;
        RotMatrixZYX(&coord->param.rot, &coord->coord);
        coord->flg    = 0;
        obj           = task->extra.tmd;
        obj->otOffset = -8;
        obj->flags   &= 0xFF7F;
        task->state++;
    }
}

void func_mist_r18_8017EB48(void)
{
    Gp_InitStarterInv();
    Mc_SaveData.at4.loc.stage = 1;
    Mc_SaveData.at4.loc.area  = 0x13;
    Mc_SaveData.at4.loc.warp  = 3;
    Mc_SaveData.at4.loc.room  = 3;
    gDisplayState.roomVariant = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
}

void func_mist_r18_8017EBB8(void)
{
    gGameSession->viewDirty = 1;
    CdCmd_StartOverlay(1U, 0x1EU, 0xBU);
    CdCmd_EnqueueReplaceOverlay82();
}

void func_mist_r18_8017EBF8(void)
{
    if (Task_SpawnFromTable(&D_mist_r18_80184F04, 7, 0, 0) != NULL) {
        D_801156F9 = 1;
    }
}

void func_mist_r18_8017EC38(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_mist_r18_8017EC58(void)
{
    Gp_RestoreStreamRng();
}

/// Clear the queued CD command and restart the CD queue.
void func_mist_r18_8017EC78(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_mist_r18_8017EC98(void)
{
    if (gGameSession->viewReady != 1) {
        D_801156F9 = 0;
    }
}

void func_mist_r18_8017ECC0(s8 arg0)
{
    gDisplayState.otDepthShift = arg0;
}

void func_mist_r18_8017ECCC(void)
{
    func_80179FC8(0, D_mist_r18_80186EA0);
}

void func_mist_r18_8017ECF4(Task* arg0)
{
    D_mist_r18_80186E90 = 0;
    D_mist_r18_80186E94 = 0;
    D_mist_r18_80186E98 = 0;
    Game_SetPtrSlot(arg0, 7);
    func_800E8634((s32)&D_mist_r18_8018522C, 0, (s32)&D_mist_r18_8018639C);
    arg0->state         = (s32)(arg0->state + 1);
    D_mist_r18_80186E9C = 1;
}

/// Per-frame entry point of the room's cutscene task: run the handler its state
/// selects from `D_mist_r18_8017D5D0` (set-up, the cutscene step
/// `func_mist_r18_8017D960`, then `taskKill`), copied onto the stack each
/// frame.
void func_mist_r18_8017ED64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r18_8017D5D0;
    sp.funcs[task->state](task);
}
