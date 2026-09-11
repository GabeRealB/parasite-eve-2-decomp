#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"
#include "actors/actors_shared_80139dcc.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `D_80115417` is one byte of the run
 * of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `Actor400600Work` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `D_80115417` matches with `SOFT_BARRIER()` after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work. */
extern void* D_800678F0[1];
extern s8    D_80115417;

extern s32 Gp_LcgState;

extern u8 D_actor_400600_80151B1C[];

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

/* Still `INCLUDE_ASM` in this overlay; `func_actor_400600_80139CAC` and
 * `func_actor_400600_8013A0B0` are called both with and without an argument,
 * so they keep an unprototyped declaration. */
void func_actor_400600_80135998(Task* arg0, s16 arg1);
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
void func_actor_400600_80137498(Task* arg0, s16 arg1);
void func_actor_400600_801387DC(Task* arg0, s32 arg1);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
s32  func_actor_400600_8013A0B0();
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_400600_8013AAD8(Task* arg0);
void func_actor_400600_8013AB44(Task* arg0);
void func_actor_400600_8013B830(Task* arg0);
void func_actor_400600_8013B8AC(Task* arg0);
void func_actor_400600_8013CB40(Task* arg0, u8 arg1);

void func_actor_400600_80139E68(Task* arg0, s16 arg1, Actor400600ViewPos* arg2)
{
    MATRIX         root;
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords            = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    Gfx_ViewCoord.flg = 0;
    coord             = &coords[arg1];
    Gp_UpdateCoord(&Gfx_ViewCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0] = arg2->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[2] = arg2->z - (local.t[2] - root.t[2]);
    coords[0].flg        = 0;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);
    Gp_UpdateCoord(coords);
}

void func_actor_400600_80139F4C(Task* arg0, s16 arg1, Actor400600ViewPos* arg2)
{
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    arg2->x    = local.t[0];
    arg2->y    = local.t[1];
    arg2->z    = coords[0].coord.t[2];
    coord->flg = 0;
}

void func_actor_400600_80139FE0(Task* arg0, s16 arg1, Actor400600ViewPos* arg2)
{
    MATRIX         root;
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[0].workm, &root);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    coords[0].coord.t[0] = arg2->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[1] = arg2->y - (local.t[1] - root.t[1]);
    coords[0].flg        = 0;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);
    Gp_UpdateCoord(coords);
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_8013A0B0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_8013A0F0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_8013A170);

void func_actor_400600_8013A26C(Task* arg0)
{
    Actor400600Work* work                = (Actor400600Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_400600_8013AAD8,
        func_actor_400600_8013AB44,
    };

    states[(s16)work->field_71C](arg0);
}
