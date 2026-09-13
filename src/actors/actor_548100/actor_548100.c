#include "common.h"

#include "actors/actor_548100.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

void func_actor_548100_801330EC(void);
s32  func_actor_548100_80134CB8(u8 nodeA, u8 nodeB);

INCLUDE_RODATA("actors/nonmatchings/actor_548100/actor_548100", D_actor_548100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", ActorsShared8013845cSub1);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132338);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132420);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132550);

INCLUDE_RODATA("actors/nonmatchings/actor_548100/actor_548100", D_actor_548100_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_548100/actor_548100", D_actor_548100_80131E6C);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80132684);

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

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_801330EC);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133200);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133684);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133BBC);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_80133F88);

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100", func_actor_548100_801342D8);

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
