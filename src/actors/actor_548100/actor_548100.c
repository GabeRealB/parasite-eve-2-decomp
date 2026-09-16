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
void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos);
void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
void func_actor_548100_80134AE0(s32 id, u8 stop);
void func_actor_548100_80134BA8(void);
void func_actor_548100_80134BF0(void);

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
    Actor548100Work*    work   = (Actor548100Work*)task->idMap;

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
    Actor548100Work* work = (Actor548100Work*)task->idMap;
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
    Actor548100Work* work = (Actor548100Work*)arg0->idMap;
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
    work = (Actor548100Work*)task->idMap;
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

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133200);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133684);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133BBC);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133F88);

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
