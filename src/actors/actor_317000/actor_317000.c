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

/// Aim body of the facing state `func_actor_317000_80161E68` dispatches on,
/// which calls it with `Game_GetPtrSlot(3)` (the player) as the second
/// argument and passes three more it never reads (0x400, 0x200 and
/// `Actor317000Work::field_4C6`). The delta from the actor's sixth coordinate
/// (`coord[5]`) to the target's fifth is normalised, taken through the
/// transpose of the actor's third coordinate's `workm`, and normalised again
/// into `dir`. The three `ratan2`s reduce `dir` to an Euler triple -- the YZ,
/// XZ and XY plane angles, the middle one against the negated magnitude of
/// `dir.vz` -- which is added halfword-wise to the euler `Gp_ExtractEuler`
/// reads out of `coord[5]` before `RotMatrix` rebuilds it. An eighth of the
/// rebuilt euler then offsets `coord[3]`'s own euler, and `coord[5]` is
/// re-read and scaled by 5/8 before being rewritten as the identity rotated
/// by it.
///
/// `coord[5]` is kept as its own `head` pointer and `coord[3].coord` as its
/// own `arm` matrix rather than indexed off `coord`: the second and third
/// reads of `arg0->extra->field_8` are re-derived rather than sharing the
/// first, which is what keeps the task argument live in `$s1` across the
/// calls and puts `&target[4]` in its own register.
void func_actor_317000_801621F4(Task* task, Task* targetTask)
{
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       target;
    GsCOORDINATE2*       head;
    GsCOORDINATE2*       aim;
    MATRIX*              arm;
    Actor317000MatWords* words;
    VECTOR               delta;
    VECTOR               dir;
    SVECTOR              ang;
    SVECTOR              vec;
    SVECTOR              rot;

    coord  = ((TmdObject*)task->extra)->field_8;
    target = ((TmdObject*)targetTask->extra)->field_8;
    head   = &coord[5];
    aim    = &target[4];

    delta.vx = aim->workm.t[0] - head->workm.t[0];
    delta.vy = aim->workm.t[1] - head->workm.t[1];
    delta.vz = aim->workm.t[2] - head->workm.t[2];
    VectorNormal(&delta, &delta);
    ApplyTransposeMatrixLV(&((TmdObject*)task->extra)->field_8[2].workm, &delta, &delta);
    VectorNormal(&delta, &dir);

    rot.vx = ratan2(dir.vz, dir.vy);
    rot.vy = ratan2(dir.vx, -ABS(dir.vz));
    rot.vz = ratan2(dir.vx, dir.vy);

    Gp_ExtractEuler(&ang, &coord[5].coord);
    ang.vx = (u16)ang.vx + (u16)rot.vx;
    ang.vy = (u16)ang.vy + (u16)rot.vy;
    ang.vz = (u16)ang.vz + (u16)rot.vz;
    RotMatrix(&ang, &coord[5].coord);

    Gp_ExtractEuler(&rot, &coord[5].coord);
    arm = &((TmdObject*)task->extra)->field_8[3].coord;
    Gp_ExtractEuler(&vec, arm);
    vec.vx = (u16)vec.vx + rot.vx / 8;
    vec.vy = (u16)vec.vy + rot.vy / 8;
    vec.vz = (u16)vec.vz + rot.vz / 8;
    RotMatrix(&vec, arm);

    Gp_ExtractEuler(&vec, &coord[5].coord);
    vec.vx = vec.vx * 5 / 8;
    vec.vy = vec.vy * 5 / 8;
    vec.vz = vec.vz * 5 / 8;

    words          = (Actor317000MatWords*)&coord[5].coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord[5].coord);
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000", func_actor_317000_80162458);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000", D_actor_317000_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000", ActorsShared80138404Table);
