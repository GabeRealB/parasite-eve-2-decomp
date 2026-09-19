#include "common.h"

#include "actors/actor_317000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>

void func_actor_317000_80162760(Task* task);
void func_actor_317000_80162768(Task* task);
void func_actor_317000_801621F4();

/// Per-frame tick. Runs the state body `Actor317000Work::field_4C0` selects
/// from a two-entry stack table, then integrates the 16.16 position: `step` is
/// added to `pos`, `step.vy` gains 0x120000 while `field_4C4` is raised, the
/// integer halves move the root coordinate and only the fractions are kept.
/// The animation slots tick, the second coordinate is refreshed while
/// `gGameSession->viewReady` is set, `field_4C6` ramps up by 0x40 to 0x1000 or
/// down by 0x80 to 0 on `field_4C5`, and the aim body runs against slot 3.
/// A non-negative `field_4C8` counts down and frees the model buffers at 0.
void func_actor_317000_80161E68(Task* task)
{
    TmdObject*       ext                 = task->extra;
    Actor317000Work* work                = (Actor317000Work*)task->work;
    void             (*states[2])(Task*) = { func_actor_317000_80162760, func_actor_317000_80162768 };
    GsCOORDINATE2*   coord;
    s32              i;

    states[(s16)work->field_4C0](task);

    coord           = ((TmdObject*)task->extra)->coords;
    work->pos[0].v += work->step.vx;
    work->pos[1].v += work->step.vy;
    work->pos[2].v += work->step.vz;
    if (work->field_4C4 != 0) {
        work->step.vy += 0x120000;
    }
    coord->coord.t[0] += work->pos[0].p.hi;
    coord->coord.t[1] += work->pos[1].p.hi;
    coord->coord.t[2] += work->pos[2].p.hi;
    coord->flg         = 0;
    work->pos[0].v     = work->pos[0].p.lo;
    work->pos[1].v     = work->pos[1].p.lo;
    work->pos[2].v     = work->pos[2].p.lo;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)&((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_4C5 != 0) {
        work->field_4C6 += 0x40;
        if (work->field_4C6 > 0x1000) {
            work->field_4C6 = 0x1000;
        }
    } else {
        work->field_4C6 -= 0x80;
        if (work->field_4C6 < 0) {
            work->field_4C6 = 0;
        }
    }
    func_actor_317000_801621F4(task, Game_GetPtrSlot(3), 0x400, 0x200, work->field_4C6);
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C8--;
    }
}

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
/// `arg0->work` is the work block, not a `TaskIdMap`.
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

    coord  = (Actor317000Coord*)((TmdObject*)task->extra)->coords;
    target = (Actor317000Coord*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
    work   = (Actor317000Work*)task->work;

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

    coord  = ((TmdObject*)task->extra)->coords;
    target = ((TmdObject*)targetTask->extra)->coords;
    head   = &coord[5];
    aim    = &target[4];

    delta.vx = aim->workm.t[0] - head->workm.t[0];
    delta.vy = aim->workm.t[1] - head->workm.t[1];
    delta.vz = aim->workm.t[2] - head->workm.t[2];
    VectorNormal(&delta, &delta);
    ApplyTransposeMatrixLV(&((TmdObject*)task->extra)->coords[2].workm, &delta, &delta);
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
    arm = &((TmdObject*)task->extra)->coords[3].coord;
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

/// `func_800B4114` is deliberately not declared in `gameplay/1BC.h`: the
/// callee's own definition takes its animation id as `u16`, which would add a
/// zero-extension at every call site. The callers declare it with a signed
/// second argument instead, as the sibling actor libs do.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Message 0x7DD handler of the table `func_actor_317000_8016267C` installs,
/// and the actor's spawn body. The placement's position and rotation are
/// copied into the work's `target` and `field_4B8..field_4BC`, `field_4C0` --
/// the index `func_actor_317000_80161E68` dispatches on -- is latched to 1, and
/// the animation preset is filled: `field_0` 0, `field_4` the optional start
/// animation (2 when absent) with `field_43F` taking its companion byte (1 when
/// absent), `field_8` 1, `field_C` 5 and `field_10` 1.
///
/// The preset is then installed the way `func_actor_317000_80162A10` installs
/// one, written out in-line: a changed `field_0` resets the bank in
/// `D_actor_317000_8016CF40` through `func_800B3F84` (`field_43E` latches it,
/// `field_43D` goes back to -1), and a changed `field_4` -- or a preset asking
/// for slots when `field_43C` says the slots are already ticking -- is pushed
/// onto `func_800B4114`'s per-slot loop instead of the `Gp_AnimResetSlot`
/// one, followed by a `Gp_AnimTickIndex` pass over the same 0x12 slots and
/// `field_43C` raised. Returns 0 either way.
s32 func_actor_317000_80162458(Task* task, s32 arg1, Actor317000Placement* place, Actor317000SpawnAnim* anim)
{
    Actor317000Work*       work;
    Actor317000Work*       w;
    Actor317000AnimPreset  preset;
    Actor317000AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor317000Work*)task->work;
    w->field_4C0   = 1;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4B8   = place->rot.vx;
    w->field_4BA   = place->rot.vy;
    w->field_4BC   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_43F   = anim->field_4;
    } else {
        preset.field_4 = 2;
        w->field_43F   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor317000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84((GpAnimCtx*)work, D_actor_317000_8016CF40[work->field_43E], ext, work->poses,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114((GpAnimCtx*)work, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000", D_actor_317000_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000", ActorsShared80138404Table);
