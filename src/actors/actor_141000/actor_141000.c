#include "common.h"

#include "actors/actor_141000.h"
#include "actors/actors_shared_80162850.h"

#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 D_80070F70;

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_141000_80131E30;

/// The controller's four animation states, dispatched through by the
/// controller work block's `state` halfword.
extern TaskFuncTable4 D_actor_141000_80131E3C;

/// The descriptor table the controller spawns from: index 1 is the task this
/// state spawns and index 2 the model actor `func_actor_141000_80132EF4`
/// spawns later, every eighth frame.
extern TaskDesc D_actor_141000_801348D8[];

/// Quad index table: sixteen quads, four point/colour indices each.
extern s8 D_actor_141000_801347C8[][4];

/// Base vertex colours, scaled by the controller's `field_0` each frame.
extern u8 D_actor_141000_80134808[][4];

/// Draws the sixteen gouraud quads, each followed by a semi-transparency
/// tpage change, at `arg2` depth minus 20.
void func_actor_141000_80131E94(Actor141000* arg0, Actor141000Point* arg1, s32 arg2)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad  = D_actor_141000_801347C8[0];
    scale = ((Actor141000CtrlWork*)((Task*)((Task*)arg0)->spawnArg2)->work)->field_0;
    if (((Task*)arg0)->killCountdown >= 0x800) {
        ((Task*)arg0)->killCountdown = 0;
    }
    rsin(((Task*)arg0)->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (D_actor_141000_80134808[i][0] * (s16)scale) >> 12;
        colors[i].g = (D_actor_141000_80134808[i][1] * (s16)scale) >> 12;
        colors[i].b = (D_actor_141000_80134808[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = arg1[a].field_0;
        poly->y0   = arg1[a].field_2;
        poly->x1   = arg1[b].field_0;
        poly->y1   = arg1[b].field_2;
        poly->x2   = arg1[c].field_0;
        poly->y2   = arg1[c].field_2;
        poly->x3   = arg1[d].field_0;
        poly->y3   = arg1[d].field_2;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, poly);
        tpage          = Gpu_PrimCursor;
        Gpu_PrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = arg1[a].field_0;
        poly->y0   = arg1[a].field_2;
        poly->x1   = arg1[b].field_0;
        poly->y1   = arg1[b].field_2;
        poly->x2   = arg1[c].field_0;
        poly->y2   = arg1[c].field_2;
        poly->x3   = arg1[d].field_0;
        poly->y3   = arg1[d].field_2;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, poly);
        tpage          = Gpu_PrimCursor;
        Gpu_PrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, tpage);
        quad += 4;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_801323F0);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E3C);

void func_actor_141000_80132C24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E30;
    sp.funcs[task->state](task);
}

/// Spawn state of the overlay's controller task: takes the display object's
/// root coordinate, allocates the work block the later states read through
/// `Task::work` and arms it at step 0xFFF, un-parks the model (`field_C` bit
/// 0x80 is the flag that keeps a `TmdObject` out of the coordinate update),
/// republishes that coordinate onto the two scale helpers, spawns the attach
/// task from `D_actor_141000_801348D8` and hands the controller the shared kill
/// callback before advancing to the per-frame state. A failed allocation kills
/// the task instead of leaving a half-built controller behind.
void func_actor_141000_80132C7C(Task* task)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    GsCOORDINATE2*       coord;

    obj   = task->extra;
    coord = obj->coords;
    work  = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_0 = 0xFFF;
    obj->flags   &= 0xFF7F;
    func_actor_141000_80132FD0(coord, 0);
    func_actor_141000_8013308C(coord, 0);
    Task_SpawnFromTable(&D_actor_141000_801348D8, 1, 0, (s32)task);
    task->exitCallback = ActorsShared80162850;
    task->state       += 1;
}

/// Per-frame state of the overlay's controller task: copies the four animation
/// handlers onto the stack and runs the one the controller work block's `state`
/// halfword selects, sign-extended. A pending effect bit spawns the controller's
/// effect through the model's root coordinate, and the session's teardown flag
/// kills the task instead of letting it tick again.
void func_actor_141000_80132D3C(Task* task)
{
    Actor141000CtrlWork* work;
    TaskFuncTable4       sp;

    work = (Actor141000CtrlWork*)task->work;
    sp   = D_actor_141000_80131E3C;
    sp.funcs[(s16)work->state](task);
    if (D_80070F70 & 1) {
        Gp_SpawnEff(0x60070, (GsCOORDINATE2*)((TmdObject*)task->extra)->coords, 0x24200, NULL);
    }
    if (gGameSession->viewReady != 0) {
        taskKill(task);
    }
}
