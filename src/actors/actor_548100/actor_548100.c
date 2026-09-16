#include "common.h"

#include "actors/actor_548100.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

void func_actor_548100_801330EC(void);
s32  func_actor_548100_80134CB8(u8 nodeA, u8 nodeB);

extern Actor548100Hotspot D_actor_548100_801357E8[];
extern Actor548100Route   D_actor_548100_801356D8;
extern Actor548100Route   D_actor_548100_80135750;

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

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132A14);

void func_actor_548100_80132EA0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132EA8);

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

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80134400);

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
