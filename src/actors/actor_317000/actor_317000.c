#include "common.h"

#include "actors/actor_317000.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000", func_actor_317000_80161E68);

/// Turn-toward-target body of the facing state `func_actor_317000_80161E68`
/// dispatches on. The actor's own coordinate and the `Game_GetPtrSlot(3)` task's
/// (the player) are normalised into `dir`, whose yaw `ratan2` takes over
/// `dir.vz`, and the result is written as the roll/pitch-free facing
/// `{ 0, yaw, 0 }` at `Actor317000Coord::rot`. The same yaw is then compared
/// against the yaw `Gp_ExtractEuler` reads back out of the node's own matrix:
/// when the two are within 0x40 (64 of 4096 units) the actor is facing its
/// target already, which clears the work's dispatch index and its companion
/// halfword; otherwise the matrix's yaw is stepped toward the target by that
/// same 0x40 and `RotMatrix` rebuilds the node from the adjusted angles.
/// `arg0->idMap` is the work block, not a `TaskIdMap`.
void func_actor_317000_801620BC(Task* task)
{
    Actor317000Work*  work;
    Actor317000Coord* coord;
    Actor317000Coord* target;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;
    SVECTOR           ang;
    s16               diff;
    s32               absDiff;
    s32               y;

    coord  = (Actor317000Coord*)((TmdObject*)task->extra)->field_8;
    target = (Actor317000Coord*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    work   = (Actor317000Work*)task->idMap;

    delta.vx = target->coord.t[0] - coord->coord.t[0];
    delta.vy = target->coord.t[1] - coord->coord.t[1];
    delta.vz = target->coord.t[2] - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;

    Gp_ExtractEuler(&ang, &coord->coord);
    diff    = ratan2(dir.vx, dir.vz) - ang.vy;
    absDiff = abs(diff);
    if (absDiff >= 0x41) {
        y = ang.vy;
        if (diff < 0) {
            ang.vy = y - 0x40;
        } else {
            ang.vy = y + 0x40;
        }
    } else {
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }
    RotMatrix(&ang, &coord->coord);
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000", func_actor_317000_801621F4);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000", func_actor_317000_80162458);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000", D_actor_317000_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000", ActorsShared80138404Table);
