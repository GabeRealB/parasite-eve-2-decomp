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

/// Moves the task to `state` with a fresh state machine.
static __inline__ void enter_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Colours `enemy` from `coord`'s world position through a 0x10-byte
/// `VECTOR` taken off `G_SCRATCH_HEAD`. Inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
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
            enter_state(arg0, 3);
            w2            = (Actor341700Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        enter_state(arg0, 7);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`, copying the 3x3 into the root coordinate.
static __inline__ void update_rotation(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    MATRIX*          m     = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
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

/// Moves the state machine to `state` at sub-state 0, reloading the work
/// block through the task as the original does.
static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Inlined copy of `func_actor_341700_80168234`: while `field_41E` is 1,
/// consumes the request in `field_448` (1..5 jump to states 6, 7, 8, 7, 9)
/// and returns 1; otherwise returns 0.
static __inline__ s32 take_request(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0:
                set_state(arg0, 6);
                break;
            case 1:
                set_state(arg0, 7);
                break;
            case 2:
                set_state(arg0, 8);
                break;
            case 3:
                set_state(arg0, 7);
                break;
            case 4:
                set_state(arg0, 9);
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

static __inline__ s32 is_hit(Task* arg0)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    if ((w->flags_EC.half & 1) || (w->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// State handler: with `field_44F` 1, a pending request 1 while `field_41E`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `field_44F` clear, a hit
/// calls `func_actor_341700_801681C4` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `field_44F`
/// register; writing `== work->field_44F` reloads the byte instead.
void func_actor_341700_801651E0(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_44F == 1) {
        if (work->field_41E != 0 && work->field_448 == 1) {
            work->field_41C = 0x20;
            work->field_418 = 0xB;
            work->field_414 = 2;
            return;
        }
        if (take_request(arg0) == 0 && is_hit(arg0)) {
            set_state(arg0, 3);
        }
    } else if (is_hit(arg0)) {
        func_actor_341700_801681C4(arg0, 1);
        set_state(arg0, 5);
    }
}

/// Five sub-state handlers `func_actor_341700_80165388` dispatches by
/// `field_422`, copied to the stack first.
const TaskFuncTable5 D_actor_341700_80161F48 = { {
    func_actor_341700_80169B40,
    func_actor_341700_80169BC8,
    func_actor_341700_80169C50,
    func_actor_341700_80169CC4,
    func_actor_341700_80169D54,
} };

/// Per-frame callback for the second enemy form, the five-state counterpart
/// of `func_actor_341700_80162DCC`: in mode 0 it aims (`func_actor_341700_80165008`),
/// lets a pending hit replace the state handler, rebuilds the root rotation,
/// then picks the next state - 4 when dead, 8 / 9 for messages 4 / 5, and
/// state 3 after a consumed `field_448` request. Mode 1 only recolours; both
/// clear bit 0x80 of the model's `field_C`, which mode 2 sets.
void func_actor_341700_80165388(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    GpEnemy*         enemy = arg0->spawnArg2;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_341700_80161F48;

    switch (D_801153F4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            func_actor_341700_80165008(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            func_actor_341700_801649DC(arg0);
            func_actor_341700_801626C4(arg0);
            update_rotation(arg0);
            func_actor_341700_801640F8(arg0, 0);
            if (work->field_438 == 0 && enemy->hp <= 0) {
                enter_state(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                enter_state(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                enter_state(arg0, 9);
            } else if (take_request(arg0)) {
                work->field_438 = 0;
                enter_state(arg0, 3);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            func_actor_341700_80162070(arg0, 2, 6, 0xC8, 0, 0xFF);
            func_actor_341700_80162070(arg0, 1, 7, 0x80, 0, 0xFF);
            func_actor_341700_80162070(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161F70);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161F7C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161F88);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161F94);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161FA4);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161FCC);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161FE4);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_7", D_actor_341700_80161FF8);
