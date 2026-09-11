#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80163354.h"
#include "actors/actors_shared_80165cc0.h"
#include "actors/actors_shared_8016a538.h"

/// Ten state handlers, indexed by `Actor342400Work::field_420`; copied to the
/// stack before dispatch.
extern TaskFuncTable10 D_actor_342400_80161FE8;

/// Same helper as in `actor_342400_7.c`: colours `enemy` from `coord`'s world
/// position through a 0x10-byte `VECTOR` taken off `G_SCRATCH_HEAD`.
static __inline__ void update_color(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Message 0x2C00 with low nibble 3 (see `field_44C`) consumes the message and
/// moves the task to state 7 with a fresh state machine; returns 1 when it did,
/// so the caller skips this frame's state handler. The `s16` result is what
/// keeps the `move` between the flag and its test, and the reload through a
/// second local is what puts it in `$v1`.
static __inline__ s16 take_hit(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;
    s16              hit  = 0;
    Actor342400Work* w2;

    if ((work->field_44C & 0xF) == 3) {
        hit             = 1;
        work->field_44C = 0;
        arg0->state     = 7;
        w2              = (Actor342400Work*)arg0->idMap;
        w2->field_420   = 0;
        w2->field_422   = 0;
    }
    return hit;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`, copying the 3x3 into the root coordinate.
static __inline__ void update_rotation(Task* arg0)
{
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    MATRIX*          m     = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          dst;

    work->field_78           &= 0xFFF;
    work->field_7A           &= 0xFFF;
    work->field_7C           &= 0xFFF;
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    func_8004BFF8(work->field_7A, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// Per-frame callback, the ten-state counterpart of
/// `func_actor_342400_80165FC0`: in mode 0 a pending hit (`take_hit`) replaces
/// the state handler, and the root rotation is rebuilt from 0x78..0x7C before
/// `func_actor_342400_801653DC`.
void func_actor_342400_801670C0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable10  sp    = D_actor_342400_80161FE8;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            ActorsShared80165cc0(arg0);
            update_rotation(arg0);
            func_actor_342400_801653DC(arg0, 0);
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
