#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_310600.h"
#include "gameplay/1BC.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_310600_80161E3C;

void func_actor_310600_801629C4(void)
{
}

void func_actor_310600_801629CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E3C;
    sp.funcs[task->state](task);
}

void func_actor_310600_80162A24(Task* arg0)
{
    Gp_UnlinkObj(&((Actor310600Work*)arg0->idMap)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_310600_80162A58(Task* arg0)
{
    TmdObject*       ext;
    Actor310600Work* work;

    work          = (Actor310600Work*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_310600_80162A74(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600_2", func_actor_310600_80162A7C);

/// Turns the actor's root part to face the work block's stored point: normalises
/// the offset from the part's own translation, takes its yaw with `ratan2`, and
/// rebuilds the local matrix from that yaw alone. Clearing `flg` makes
/// `Gp_UpdateCoordTree` recompute the world matrix from it, and bumping
/// `field_47E` moves the actor on to the next handler of its state table.
void func_actor_310600_80162AD8(Task* task)
{
    Actor310600Work*  work;
    Actor310600Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor310600Work*)task->idMap;
    coord = (Actor310600Coord*)((TmdObject*)task->extra)->field_8;

    delta.vx = work->field_4F8 - coord->coord.t[0];
    delta.vy = work->field_4FC - coord->coord.t[1];
    delta.vz = work->field_500 - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_47E++;
}

/// State handler reached by the `field_47E` advance `func_actor_310600_80162AD8`
/// ends with: rotates the constant local-space offset
/// `D_actor_310600_80161E54` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the update
/// loop, and advances `field_47E` again so the dispatcher runs the next handler.
/// The same body as `func_actor_335800_80163CA0` / `ActorsShared80132920`, which
/// rotate their own overlay's copy of the offset.
void func_actor_310600_80162B98(Task* task)
{
    Actor310600Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = (Actor310600Work*)task->idMap;

    vec = D_actor_310600_80161E54;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_47E++;
}
