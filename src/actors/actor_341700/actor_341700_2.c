#include "common.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "actors/actor_341700.h"
#include "actors/actors_shared_80163354.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_80165cc0.h"
#include "actors/actors_shared_801662ec.h"

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80162974);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80162B8C);

static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Colours `enemy` from `coord`'s world
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

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 take_hit(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    Actor341700Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            set_state(arg0, 3);
            w2            = (Actor341700Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        set_state(arg0, 7);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at
/// 0x78..0x7C to 12 bits and rebuilds the model root's rotation from them
/// (Z, then X, then the heading) in a matrix taken off `G_SCRATCH_HEAD`.
static __inline__ void update_rotation(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
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

/// Per-frame callback for the main enemy, the same body as
/// `func_actor_342400_801640B0`. In mode 0 it aims at the nearest actor
/// (`ActorsShared801662ec`), lets a pending hit (`take_hit`) replace the state
/// handler, eases `field_424` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `field_448` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `field_438` is clear.
void func_actor_341700_80162DCC(Task* arg0)
{
    GpEnemy*         enemy = arg0->spawnArg2;
    TmdObject*       obj   = arg0->extra;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable11  sp    = D_actor_341700_80161E64;
    s32              cur;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            ActorsShared801662ec(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            ActorsShared80165cc0(arg0);
            cur             = work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            ActorsShared801639a8(arg0);
            if (work->field_432 == 1) {
                func_actor_341700_80168370(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            update_rotation(arg0);
            func_actor_341700_801640F8(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->field_40 <= 0) {
                set_state(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->field_40 <= 0) {
                set_state(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                set_state(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                set_state(arg0, 9);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
            ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
            ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}
