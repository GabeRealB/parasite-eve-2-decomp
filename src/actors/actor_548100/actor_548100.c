#include "common.h"

#include "actors/actor_548100.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/abs.h"
#include "rooms/room_common.h"

void func_actor_548100_801330EC(void);
s32  func_actor_548100_80134CB8(u8 nodeA, u8 nodeB);
void func_actor_548100_8013461C(Actor548100TexRect* rect);
void func_actor_548100_80133BBC(s32 arg0);
void func_actor_548100_80133200(s32 nodeA, s32 nodeB, u8 r, u8 g, u8 b);
void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos);
void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
void func_actor_548100_80134AE0(s32 id, u8 stop);
void func_actor_548100_80134BA8(void);
void func_actor_548100_80134BF0(void);

extern u8                 D_80070F87;
extern Actor548100Hotspot D_actor_548100_801357E8[];
extern Actor548100Route   D_actor_548100_801356D8;
extern Actor548100Route   D_actor_548100_80135750;
extern Actor548100TexRect D_actor_548100_801357C0[];
extern Actor548100TexRect D_actor_548100_801357E0;
extern s16                D_actor_548100_80135B50;
extern u8                 D_actor_548100_80135B52;
extern s8                 D_actor_548100_80135B53;
extern s8                 D_actor_548100_80135B54;
extern s8                 D_actor_548100_80135B55;
extern s8                 D_actor_548100_80135B56;
extern s8                 D_actor_548100_80135B57;
extern s8                 D_actor_548100_80135B58;
extern s8                 D_actor_548100_80135B59;
extern s8                 D_actor_548100_80135B5A;
extern s8                 D_actor_548100_80135B5B;

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", ActorsShared8013845cSub1);

/// Queues one 16x24 textured quad -- the action prompt icon -- at (`x`, `y`)
/// into the head of the current OT. `variant` selects the palette, 0x3C87 when
/// it is 2 and 0x3C88 otherwise, and 0 draws nothing at all. Same body as the
/// rooms' `Room_Draw36`.
void func_actor_548100_80132338(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(Gpu_CurrentOt, prim);
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132420);

void func_actor_548100_80132550(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    Actor548100Hotspot* hs     = D_actor_548100_801357E8;
    Actor548100Work*    work   = (Actor548100Work*)task->work;

    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    work->step       = 0;
    if (func_actor_548100_801348A4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->step       = hs->id;
                    work->field_6    = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

void func_actor_548100_80132684(Task* task)
{
    Actor548100Work* work = (Actor548100Work*)task->work;
    s32              kind;
    s32              state;
    s32              cmd;

    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        switch (work->step) {
            case 1:
            case 2:
            case 3:
            case 4:
                if (GameFlag_GetNibble(work->step + 0xBE) == 0) {
                    Gp_StartCapSlot(6, 0, 0);
                    state = 2;
                } else if (GameFlag_GetNibble(0xC3) != 0) {
                    Gp_StartCapSlot(6, 1, 3);
                    state = 2;
                } else {
                    if (GameFlag_GetNibble(work->step + 0xBE) == 1) {
                        work->bit2Slot = 4;
                        kind           = 2;
                    } else {
                        work->bit2Slot = 5;
                        kind           = 5;
                    }
                    Gp_SetCurBit2Flag(work->bit2Slot, 1);
                    Gp_StartCapSlot(6, 0, kind);
                    state = 7;
                }
                break;
            case 5:
                Gp_RunCapCmd(5, 0);
                state = 8;
                break;
            case 6:
                cmd = 4;
                goto run;
            case 7:
                cmd = 7;
                goto run;
            case 8:
                cmd = 9;
                goto run;
            case 9:
                cmd = 8;
            run:
                Gp_RunCapCmd(cmd, 0);
                state = 2;
                break;
            default:
                goto def;
        }
    } else {
        state = work->collectBitId;
        if (state != 0) {
            state = 6;
        } else {
        def:
            state = 2;
        }
    }
    task->state = state;
}

/// Player pressed the action button on this actor's map marker with the marker
/// route done: record the route leg the ramp runs along and hand the actor on to
/// state 9.
void func_actor_548100_80132808(Task* arg0)
{
    Actor548100Work* work = (Actor548100Work*)arg0->work;
    s32              distA;
    s32              distB;

    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 0xB) {
            if (GameFlag_GetNibble(0x110) != 0) {
                Gp_SetItemSeenBit(0x120, 1);
                Gp_SetItemSeenBit(0x12C, 1);
            }
            SndEvt_EnqueueType6(0x54060009, 0, 0);
            SndEvt_EnqueueType6(0x5406000A, 0, 0);
            GameFlag_SetNibble(0xC3, 1);
            arg0->state = 9;
            func_actor_548100_801330EC();
            work->field_10 = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[2].nodeA, D_actor_548100_80135B4C->leg[2].nodeB);
            distA          = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[0].nodeA, D_actor_548100_80135B4C->leg[0].nodeB);
            distB          = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[1].nodeA, D_actor_548100_80135B4C->leg[1].nodeB);
            if (distB < distA) {
                work->field_8  = distA;
                work->field_C  = distB;
                work->farFrom  = D_actor_548100_80135B4C->leg[0].nodeA;
                work->nearFrom = D_actor_548100_80135B4C->leg[1].nodeA;
                work->farTo    = D_actor_548100_80135B4C->leg[0].nodeB;
                work->nearTo   = D_actor_548100_80135B4C->leg[1].nodeB;
            } else {
                work->field_8  = distB;
                work->field_C  = distA;
                work->farFrom  = D_actor_548100_80135B4C->leg[1].nodeA;
                work->nearFrom = D_actor_548100_80135B4C->leg[0].nodeA;
                work->farTo    = D_actor_548100_80135B4C->leg[1].nodeB;
                work->nearTo   = D_actor_548100_80135B4C->leg[0].nodeB;
            }
            work->field_A  = 0;
            work->field_E  = 0;
            work->field_12 = 0;
            return;
        }
        if (Gp_GetCapEventKey() == 0x15) {
            SndEvt_EnqueueType6(0x54060009, 0, 0);
            GameFlag_SetNibble(0xC3, 0);
        }
        arg0->state = 2;
    }
}

void func_actor_548100_80132A14(Task* task)
{
    Actor548100Work*    work;
    Actor548100TexRect* rect;
    DR_MODE*            prim;
    Actor548100Route*   route;
    s32                 i;
    s32                 flagA;
    s32                 flagB;

    i    = 0;
    rect = D_actor_548100_801357C0;
    work = (Actor548100Work*)task->work;
    for (; i < 4; i++, rect++) {
        if (GameFlag_GetNibble(i + 0xBF) != 0) {
            func_actor_548100_8013461C(rect);
        }
    }

    prim           = (DR_MODE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 1);
    prim->code[0] = 0xE100022A;
    addPrim(&Gpu_CurrentOt[0x3FD], prim);

    func_actor_548100_80134960(D_actor_548100_80135B50, &D_actor_548100_80135B53, &D_actor_548100_80135B54, &D_actor_548100_80135B55);
    func_actor_548100_801349E0(D_actor_548100_80135B50, &D_actor_548100_80135B56, &D_actor_548100_80135B57, &D_actor_548100_80135B58);
    func_actor_548100_80134A60(D_actor_548100_80135B50, &D_actor_548100_80135B59, &D_actor_548100_80135B5A, &D_actor_548100_80135B5B);
    func_actor_548100_80134BF0();
    GameFlag_SetNibble(0xBB, 0);
    GameFlag_SetNibble(0xB5, 0);

    if (GameFlag_GetNibble(0xC3) != 0) {
        if (task->state != 9) {
            route = D_actor_548100_80135B4C;
            if (route->leg[0].nodeA != 0) {
                func_actor_548100_80134AE0(route->leg[0].nodeA, route->leg[0].nodeB);
                route = D_actor_548100_80135B4C;
            }
            if (route->leg[1].nodeA != 0) {
                func_actor_548100_80134AE0(route->leg[1].nodeA, route->leg[1].nodeB);
            }
            route = D_actor_548100_80135B4C;
            if (route->leg[2].nodeA != 0) {
                func_actor_548100_80134AE0(route->leg[2].nodeA, route->leg[2].nodeB);
            }
            if (D_actor_548100_80135B4C->flag_8 != 0) {
                func_actor_548100_80133BBC(1);
                GameFlag_SetNibble(0xB5, 1);
            }
            if (D_actor_548100_80135B4C->flag_9 != 0) {
                func_actor_548100_80133BBC(2);
                if (GameFlag_GetNibble(0xBE) == 2) {
                    GameFlag_SetNibble(0xBB, 3);
                } else {
                    GameFlag_SetNibble(0xBB, 2);
                }
            }
        } else {
            route = D_actor_548100_80135B4C;
            flagA = 0;
            flagB = 0;
            if (route->leg[2].nodeA != 0) {
                func_actor_548100_801342D8(route->leg[2].nodeA, route->leg[2].nodeB, work->field_12);
                flagB = work->field_12 == work->field_10;
            }
            if (work->farFrom != 0) {
                if (work->field_A != work->field_8) {
                    func_actor_548100_801342D8(work->farFrom, work->farTo, work->field_A);
                } else {
                    func_actor_548100_80134AE0(work->farFrom, work->farTo);
                }
            }
            if (work->nearFrom != 0) {
                if (work->field_E != work->field_C) {
                    func_actor_548100_801342D8(work->nearFrom, work->nearTo, work->field_E);
                } else {
                    func_actor_548100_80134AE0(work->nearFrom, work->nearTo);
                }
            }
            if (work->field_A == work->field_8 && work->farTo == 0 && work->farFrom != 0) {
                flagA = D_actor_548100_80135B4C->flag_8;
                flagB = flagB || D_actor_548100_80135B4C->flag_9;
            }
            if (flagA != 0) {
                func_actor_548100_80133BBC(1);
            }
            if (flagB != 0) {
                func_actor_548100_80133BBC(2);
            }
        }
    }

    func_actor_548100_80134BA8();
    if (GameFlag_GetNibble(0xC3) == 0) {
        GameFlag_SetNibble(0xBB, 0);
        GameFlag_SetNibble(0xB5, 0);
    } else {
        func_actor_548100_8013461C(&D_actor_548100_801357E0);
    }

    if (D_actor_548100_80135B52 == 0) {
        if (++D_actor_548100_80135B50 >= 0x20) {
            D_actor_548100_80135B52 = 1;
        }
    } else if (D_actor_548100_80135B52 == 1) {
        if (--D_actor_548100_80135B50 <= 0x10) {
            D_actor_548100_80135B52 = 0;
        }
    } else if (D_actor_548100_80135B50 > 0) {
        D_actor_548100_80135B50--;
    }
}

void func_actor_548100_80132EA0(void)
{
}

/// Outlines `rect` in (`r`, `g`, `b`) with four flat `LINE_F2` edges linked
/// into `Gpu_CurrentOt[1]`. Same body as the rooms' `Room_Draw26`.
void func_actor_548100_80132EA8(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);
}

void func_actor_548100_801330EC(void)
{
    s32 want;
    s32 have;
    s32 i;

    if (GameFlag_GetNibble(0xBE) == 1) {
        D_actor_548100_80135B4C = &D_actor_548100_801356D8;
    } else {
        D_actor_548100_80135B4C = &D_actor_548100_80135750;
    }
    while (D_actor_548100_80135B4C->bitA != -1) {
        want = 0;
        if (D_actor_548100_80135B4C->bitA != 0) {
            want = 1 << (D_actor_548100_80135B4C->bitA - 1);
        }
        if (D_actor_548100_80135B4C->bitB != 0) {
            want |= 1 << (D_actor_548100_80135B4C->bitB - 1);
        }
        have = 0;
        for (i = 0; i < 4; i++) {
            if (GameFlag_GetNibble(i + 0xBF) != 0) {
                have |= 1 << i;
            }
        }
        if (want == have) {
            break;
        }
        D_actor_548100_80135B4C++;
    }
}

/// Draws a line between nodes `nodeA` and `nodeB` as a flat `r`/`g`/`b` quad
/// two pixels wide, framed on each side by `POLY_G4` edges fading from black
/// into that colour. The quad lies along whichever axis the line spans further,
/// with the nodes ordered so the lower coordinate comes first.
void func_actor_548100_80133200(s32 nodeA, s32 nodeB, u8 r, u8 g, u8 b)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s32      ax;
    s32      ay;
    s32      bx;
    s32      by;
    s32      dx;
    s32      tmp;
    s32      x0;
    s32      y0;
    s32      x1;
    s32      y1;
    s32      x2;
    s32      y2;
    s32      x3;
    s32      y3;

    quad           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(quad + 1);
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = r;
    quad->g0 = g;
    quad->b0 = b;
    ax       = D_actor_548100_801358E4[nodeA].vx - 0x9E;
    ay       = D_actor_548100_801358E4[nodeA].vy - 0x76;
    bx       = D_actor_548100_801358E4[nodeB].vx - 0x9E;
    by       = D_actor_548100_801358E4[nodeB].vy - 0x76;
    dx       = ax - bx;
    if (dx < 0) {
        dx = bx - ax;
    }
    if (ABS(ay - by) < dx) {
        if (bx < ax) {
            tmp = ax;
            ax  = bx;
            bx  = tmp;
            tmp = ay;
            ay  = by;
            by  = tmp;
        }
        x0 = ax + 1;
        y0 = ay - 1;
        x1 = bx - 1;
        y1 = by - 1;
        x2 = x0;
        y2 = ay + 1;
        x3 = x1;
        y3 = by + 1;
    } else {
        if (by < ay) {
            tmp = ax;
            ax  = bx;
            bx  = tmp;
            tmp = ay;
            ay  = by;
            by  = tmp;
        }
        x0 = ax - 1;
        y0 = ay + 1;
        x1 = ax + 1;
        y1 = y0;
        x2 = bx - 1;
        y2 = by - 1;
        x3 = bx + 1;
        y3 = y2;
    }
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y1;
    quad->x2 = x2;
    quad->y2 = y2;
    quad->x3 = x3;
    quad->y3 = y3;
    addPrim(&Gpu_CurrentOt[0x3FC], quad);

    top            = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(top + 1);
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r;
    top->g2 = g;
    top->b2 = b;
    top->r3 = r;
    top->g3 = g;
    top->b3 = b;
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y1 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y1;
    addPrim(&Gpu_CurrentOt[0x3FC], top);

    left           = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(left + 1);
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r;
    left->g2 = g;
    left->b2 = b;
    left->r3 = r;
    left->g3 = g;
    left->b3 = b;
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x2 - 3;
    left->y1 = y2 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x2;
    left->y3 = y2;
    addPrim(&Gpu_CurrentOt[0x3FC], left);

    right          = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(right + 1);
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r;
    right->g2 = g;
    right->b2 = b;
    right->r3 = r;
    right->g3 = g;
    right->b3 = b;
    right->x0 = x1 + 3;
    right->y0 = y1 - 3;
    right->x1 = x3 + 3;
    right->y1 = y3 + 3;
    right->x2 = x1;
    right->y2 = y1;
    right->x3 = x3;
    right->y3 = y3;
    addPrim(&Gpu_CurrentOt[0x3FC], right);

    bottom         = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(bottom + 1);
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r;
    bottom->g2 = g;
    bottom->b2 = b;
    bottom->r3 = r;
    bottom->g3 = g;
    bottom->b3 = b;
    bottom->x0 = x2 - 3;
    bottom->y0 = y2 + 3;
    bottom->x1 = x3 + 3;
    bottom->y1 = y3 + 3;
    bottom->x2 = x2;
    bottom->y2 = y2;
    bottom->x3 = x3;
    bottom->y3 = y3;
    addPrim(&Gpu_CurrentOt[0x3FC], bottom);
}

/// Draws `edge` between its two nodes in the colour its `state` selects (1-3).
/// States 4 and 5 (5 swaps the nodes) split the line at `field_C` along x, or
/// along y when `flag_3` is set, clipping each half with a `DR_AREA` linked into
/// `Gpu_CurrentOt[0x3FC]` and drawing one half per colour.
void func_actor_548100_80133684(Actor548100Edge* edge)
{
    RECT     rect;
    DR_AREA* area;
    s32      ax;
    s32      ay;
    s32      bx;
    s32      by;
    s32      pos;
    s32      sign;
    s32      a;
    s32      b;

    a = edge->nodeA;
    b = edge->nodeB;
    switch (edge->state) {
        case 3:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B59, D_actor_548100_80135B5A, D_actor_548100_80135B5B);
            break;
        case 2:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
            break;
        case 1:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
            break;
        case 5:
            a = edge->nodeB;
            b = edge->nodeA;
        case 4:
            area           = (DR_AREA*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(area + 1);
            ax             = D_actor_548100_801358E4[a].vx - 0x9E;
            ay             = D_actor_548100_801358E4[a].vy - 0x76;
            bx             = D_actor_548100_801358E4[b].vx - 0x9E;
            by             = D_actor_548100_801358E4[b].vy - 0x76;
            setRECT(&rect, 0, 0, 0x140, 0xF0);
            rect.y += D_80070F87 * 0x110;
            SetDrawArea(area, &rect);
            addPrim(&Gpu_CurrentOt[0x3FC], area);
            if (edge->flag_3 == 0) {
                sign = 1;
                if (bx < ax) {
                    sign = -1;
                }
                pos = ax + sign * edge->field_C;
                func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
                area           = (DR_AREA*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(area + 1);
                if (ax < bx) {
                    setRECT(&rect, 0, 0, pos + 0xA0, 0xF0);
                } else {
                    setRECT(&rect, pos + 0xA0, 0, 0xA0 - pos, 0xF0);
                }
                rect.y += D_80070F87 * 0x110;
                SetDrawArea(area, &rect);
                addPrim(&Gpu_CurrentOt[0x3FC], area);
                func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
                area           = (DR_AREA*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(area + 1);
                if (ax < bx) {
                    setRECT(&rect, pos + 0xA0, 0, 0xA0 - pos, 0xF0);
                } else {
                    setRECT(&rect, 0, 0, pos + 0xA0, 0xF0);
                }
            } else {
                sign = 1;
                if (by < ay) {
                    sign = -1;
                }
                pos = ay + sign * edge->field_C;
                func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
                area           = (DR_AREA*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(area + 1);
                if (ay < by) {
                    setRECT(&rect, 0, 0, 0x140, pos + 0x78);
                } else {
                    setRECT(&rect, 0, pos + 0x78, 0x140, 0x78 - pos);
                }
                rect.y += D_80070F87 * 0x110;
                SetDrawArea(area, &rect);
                addPrim(&Gpu_CurrentOt[0x3FC], area);
                func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
                area           = (DR_AREA*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(area + 1);
                if (ay < by) {
                    setRECT(&rect, 0, pos + 0x78, 0x140, 0x78 - pos);
                } else {
                    setRECT(&rect, 0, 0, 0x140, pos + 0x78);
                }
            }
            rect.y += D_80070F87 * 0x110;
            SetDrawArea(area, &rect);
            addPrim(&Gpu_CurrentOt[0x3FC], area);
            break;
    }
}

/// Draws a translucent flat quad in the (`D_..._80135B53`..`55`) colour at
/// x 0x43..0x68, on row 1 (`arg0 == 1`) or row 2, framed by four `POLY_G4`
/// edges fading from black into that colour, all linked into
/// `Gpu_CurrentOt[0x3FC]`.
void func_actor_548100_80133BBC(s32 arg0)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s16      x0;
    s16      x1;
    s16      y0;
    s16      y1;
    u8       r1;
    u8       g1;
    u8       b1;
    u8       r2;
    u8       g2;
    u8       b2;
    u8       r3;
    u8       g3;
    u8       b3;
    u8       r4;
    u8       g4;
    u8       b4;

    quad           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(quad + 1);
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = D_actor_548100_80135B53;
    quad->g0 = D_actor_548100_80135B54;
    quad->b0 = D_actor_548100_80135B55;
    x0       = 0x43;
    if (arg0 == 1) {
        y0 = 0x2E;
        x1 = 0x68;
        y1 = 0x39;
    } else {
        y0 = 0x42;
        x1 = 0x68;
        y1 = 0x4D;
    }
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y0;
    quad->x2 = x0;
    quad->y2 = y1;
    quad->x3 = x1;
    quad->y3 = y1;
    addPrim(&Gpu_CurrentOt[0x3FC], quad);

    r1             = D_actor_548100_80135B53;
    g1             = D_actor_548100_80135B54;
    b1             = D_actor_548100_80135B55;
    top            = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(top + 1);
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r1;
    top->g2 = g1;
    top->b2 = b1;
    top->r3 = r1;
    top->g3 = g1;
    top->b3 = b1;
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y0 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y0;
    addPrim(&Gpu_CurrentOt[0x3FC], top);

    r2             = D_actor_548100_80135B53;
    g2             = D_actor_548100_80135B54;
    b2             = D_actor_548100_80135B55;
    left           = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(left + 1);
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r2;
    left->g2 = g2;
    left->b2 = b2;
    left->r3 = r2;
    left->g3 = g2;
    left->b3 = b2;
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x0 - 3;
    left->y1 = y1 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x0;
    left->y3 = y1;
    addPrim(&Gpu_CurrentOt[0x3FC], left);

    r3             = D_actor_548100_80135B53;
    g3             = D_actor_548100_80135B54;
    b3             = D_actor_548100_80135B55;
    right          = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(right + 1);
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r3;
    right->g2 = g3;
    right->b2 = b3;
    right->r3 = r3;
    right->g3 = g3;
    right->b3 = b3;
    right->x0 = x1 + 3;
    right->y0 = y0 - 3;
    right->x1 = x1 + 3;
    right->y1 = y1 + 3;
    right->x2 = x1;
    right->y2 = y0;
    right->x3 = x1;
    right->y3 = y1;
    addPrim(&Gpu_CurrentOt[0x3FC], right);

    r4             = D_actor_548100_80135B53;
    g4             = D_actor_548100_80135B54;
    b4             = D_actor_548100_80135B55;
    bottom         = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(bottom + 1);
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r4;
    bottom->g2 = g4;
    bottom->b2 = b4;
    bottom->r3 = r4;
    bottom->g3 = g4;
    bottom->b3 = b4;
    bottom->x0 = x0 - 3;
    bottom->y0 = y1 + 3;
    bottom->x1 = x1 + 3;
    bottom->y1 = y1 + 3;
    bottom->x2 = x0;
    bottom->y2 = y1;
    bottom->x3 = x1;
    bottom->y3 = y1;
    addPrim(&Gpu_CurrentOt[0x3FC], bottom);
}

/// Draws a translucent flat quad in half the (`D_..._80135B53`..`55`) colour
/// and a gradient border of four `POLY_G4` edges fading from black into
/// that colour, linked into `Gpu_CurrentOt[0x3FC]`. The flat quad itself is
/// never linked.
void func_actor_548100_80133F88(void)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    u8       r;
    u8       g;
    u8       b;

    r = D_actor_548100_80135B53;
    g = D_actor_548100_80135B54;
    b = D_actor_548100_80135B55;

    quad           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(quad + 1);
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = r >> 1;
    quad->g0 = g >> 1;
    quad->b0 = b >> 1;
    quad->x0 = -0x86;
    quad->y0 = -0x62;
    quad->x1 = -0x5B;
    quad->y1 = -0x62;
    quad->x2 = -0x86;
    quad->y2 = 0x65;
    quad->x3 = -0x5B;
    quad->y3 = 0x65;

    top            = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(top + 1);
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r;
    top->g2 = g;
    top->b2 = b;
    top->r3 = r;
    top->g3 = g;
    top->b3 = b;
    top->x0 = -0x8e;
    top->y0 = -0x6a;
    top->x1 = -0x53;
    top->y1 = -0x6a;
    top->x2 = -0x86;
    top->y2 = -0x62;
    top->x3 = -0x5b;
    top->y3 = -0x62;
    addPrim(&Gpu_CurrentOt[0x3FC], top);

    left           = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(left + 1);
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r;
    left->g2 = g;
    left->b2 = b;
    left->r3 = r;
    left->g3 = g;
    left->b3 = b;
    left->x0 = -0x8e;
    left->y0 = -0x6a;
    left->x1 = -0x8e;
    left->y1 = 0x6d;
    left->x2 = -0x86;
    left->y2 = -0x62;
    left->x3 = -0x86;
    left->y3 = 0x65;
    addPrim(&Gpu_CurrentOt[0x3FC], left);

    right          = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(right + 1);
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r;
    right->g2 = g;
    right->b2 = b;
    right->r3 = r;
    right->g3 = g;
    right->b3 = b;
    right->x0 = -0x53;
    right->y0 = -0x6a;
    right->x1 = -0x53;
    right->y1 = 0x6d;
    right->x2 = -0x5b;
    right->y2 = -0x62;
    right->x3 = -0x5b;
    right->y3 = 0x65;
    addPrim(&Gpu_CurrentOt[0x3FC], right);

    bottom         = (POLY_G4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(bottom + 1);
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r;
    bottom->g2 = g;
    bottom->b2 = b;
    bottom->r3 = r;
    bottom->g3 = g;
    bottom->b3 = b;
    bottom->x0 = -0x8e;
    bottom->y0 = 0x6d;
    bottom->x1 = -0x53;
    bottom->y1 = 0x6d;
    bottom->x2 = -0x86;
    bottom->y2 = 0x65;
    bottom->x3 = -0x5b;
    bottom->y3 = 0x65;
    addPrim(&Gpu_CurrentOt[0x3FC], bottom);
}

void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos)
{
    u8* route;
    u8* head;
    u8  prev;
    s32 edge;
    s32 total;
    s32 start;

    total = 0;
    head  = D_actor_548100_80135B24[id];
    start = total;
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {

        if (*route != 0xFF) {
            edge   = D_actor_548100_80135B5C[*route + prev * 100];
            total += D_actor_548100_801351D0[edge].dist;
            if (pos >= total) {
                D_actor_548100_801351D0[edge].state = 2;
            } else if (start < pos) {
                if (D_actor_548100_801351D0[edge].nodeA == prev) {
                    D_actor_548100_801351D0[edge].state = 4;
                } else {
                    D_actor_548100_801351D0[edge].state = 5;
                }
                D_actor_548100_801351D0[edge].field_C = pos - start;
            } else {
                D_actor_548100_801351D0[edge].state = 1;
            }
            start = total;
            prev  = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route == (stop & 0xFF)) {
            break;
        }
        route++;
    }
}

/// Build the edge graph's derived state: record every edge's id in the
/// node-pair matrix both ways round, then store its dominant axis in `flag_3`
/// (0 horizontal, 1 vertical), that axis's two screen-centred endpoint
/// coordinates in `field_4` / `field_6` and their span less 2 in `dist`.
/// Finally reset each edge's `state` for the current stage, as
/// `func_actor_548100_80134BF0` does.
void func_actor_548100_80134400(void)
{
    Actor548100Edge* edge;
    Actor548100Edge* cell;
    DVECTOR*         a;
    DVECTOR*         b;
    s32              ax;
    s32              bx;
    s32              ay;
    s32              by;
    s32              dx;
    u8               i;

    i = 0;
    for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++, i++) {
        D_actor_548100_80135B5C[edge->nodeB + edge->nodeA * 100] = i;
        D_actor_548100_80135B5C[edge->nodeA + edge->nodeB * 100] = i;
        a                                                        = &D_actor_548100_801358E4[edge->nodeA];
        b                                                        = &D_actor_548100_801358E4[edge->nodeB];
        ax                                                       = a->vx - 158;
        bx                                                       = b->vx - 158;
        dx                                                       = ax - bx;
        by                                                       = b->vy - 118;
        ay                                                       = a->vy - 118;
        if (dx < 0) {
            dx = bx - ax;
        }
        if (ABS(ay - by) < dx) {
            edge->dist    = ABS(ax - bx) - 2;
            edge->flag_3  = 0;
            edge->field_4 = ax;
            edge->field_6 = bx;
        } else {
            edge->dist    = ABS(ay - by) - 2;
            edge->flag_3  = 1;
            edge->field_4 = ay;
            edge->field_6 = by;
        }
    }
    if (GameFlag_GetNibble(0xBE) == 2) {
        for (cell = D_actor_548100_801351D0; cell->nodeA != 0; cell++) {
            if (cell->field_2 == 2) {
                cell->state = 0;
            } else {
                cell->state = 1;
            }
        }
    } else {
        for (cell = D_actor_548100_801351D0; cell->nodeA != 0; cell++) {
            if (cell->field_2 == 1) {
                cell->state = 0;
            } else {
                cell->state = 1;
            }
        }
    }
}

/// Link `rect` into the ordering table as a textured quad, taking the
/// primitive off the `Gpu_PrimCursor` bump allocator. The same four numbers are
/// the texture window and, shifted by the screen centre, the quad's screen
/// rectangle -- `u`/`v` are the table's own values and `x`/`y` those values
/// minus 160 and 120, so a record drawn from the origin-centred screen space
/// `Actor548100TexRect` is authored in lands on the matching part of the
/// texture page. Corner 0 and 2 share the left edge, 1 and 3 the right; the
/// upper corners share the top, the lower pair the bottom.
///
/// `x1`/`x3` are read before `v0`/`v1` -- that order is what puts the four
/// loads in the register file the target uses, and reordering them changes
/// the code without changing the meaning.
void func_actor_548100_8013461C(Actor548100TexRect* rect)
{
    POLY_FT4* prim;
    s32       u0;
    s32       v0;
    s32       u1;
    s32       v1;

    u0             = rect->u0;
    u1             = rect->u1;
    v0             = rect->v0;
    v1             = rect->v1;
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    SetPolyFT4(prim);
    prim->x0    = u0 - 0xA0;
    prim->y0    = v0 - 0x78;
    prim->x1    = u1 - 0xA0;
    prim->y1    = v0 - 0x78;
    prim->x2    = u0 - 0xA0;
    prim->y2    = v1 - 0x78;
    prim->x3    = u1 - 0xA0;
    prim->y3    = v1 - 0x78;
    prim->u0    = u0;
    prim->v0    = v0;
    prim->u1    = u1;
    prim->v1    = v0;
    prim->u2    = u0;
    prim->v2    = v1;
    prim->u3    = u1;
    prim->v3    = v1;
    prim->tpage = 0x116;
    setShadeTex(prim, 1);
    addPrim(&Gpu_CurrentOt[0x3FE], prim);
}
