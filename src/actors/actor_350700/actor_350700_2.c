#include "common.h"
#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors `ActorsShared80132920`: rotates the constant local-space offset
/// `D_actor_350700_80161E40` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances `field_4C2` so the dispatcher runs the next
/// handler. Where the shared body rotates the offset unchanged, this one
/// shrinks it to -0.4 of its length whenever `field_4C4` is clear.
void func_actor_350700_8016261C(Task* arg0)
{
    Actor350700Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor350700Work*)arg0->idMap;

    vec = D_actor_350700_80161E40;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4C2++;
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162764);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162860);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162998);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162A14);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162AF4);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162B30);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162D5C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_801630C0);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared80132920Offset);
