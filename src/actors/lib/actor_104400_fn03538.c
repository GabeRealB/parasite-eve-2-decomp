#include "common.h"

#include "actors/actor_104400.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

void Actor04400_Fn00220(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4, u8 arg5);
void Actor04400_Fn00874(Task* arg0);
void Actor04400_Fn022A8(Task* arg0, s16 arg1);
void Actor04400_Fn031B8(Task* arg0);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
void func_8004BFF8(s32 angle, MATRIX* matrix);

extern TaskFuncTable5 Actor04400_D00128;

/// `ActorsShared8013a2c0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
/// This unit needs its own copy; the two are separate translation units.
static __inline__ void Actor04400_UpdateColor(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Moves the task to `state` with a fresh state machine.
static __inline__ void Actor04400_SetState(Task* arg0, s32 state)
{
    Actor104400Work* w = (Actor104400Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Moves the state machine to `state` at sub-state 0 without touching the
/// task's own state; `Actor04400_TakeRequest` uses this one.
static __inline__ void Actor04400_SetStateIndex(Task* arg0, s32 state)
{
    Actor104400Work* w = (Actor104400Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Inlined copy of `Actor04400_Fn063E4`: while `field_41E` is 1, consumes the
/// request in `field_448` (1..5 jump to states 6, 7, 8, 7, 9) and returns 1;
/// otherwise returns 0.
static __inline__ s32 Actor04400_TakeRequest(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0:
                Actor04400_SetStateIndex(arg0, 6);
                break;
            case 1:
                Actor04400_SetStateIndex(arg0, 7);
                break;
            case 2:
                Actor04400_SetStateIndex(arg0, 8);
                break;
            case 3:
                Actor04400_SetStateIndex(arg0, 7);
                break;
            case 4:
                Actor04400_SetStateIndex(arg0, 9);
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: a flag
/// set to 0 up front and to 1 in each arm cross-jumps the arms' stores.
static __inline__ s16 Actor04400_TakeHit(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            Actor04400_SetState(arg0, 3);
            w2            = (Actor104400Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        Actor04400_SetState(arg0, 7);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`.
static __inline__ void Actor04400_UpdateRotation(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
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

/// Same body as `func_actor_342400_8016666C`, which `actor_342400` carries at
/// the same offset in its own slot.
///
/// The five-state per-frame callback of the enemy's state machine, the
/// counterpart of `Actor04400_Fn05DE0`. Mode 0 counts `field_442` up, aims
/// (`Actor04400_Fn031B8`), lets `Actor04400_TakeHit` replace the handler
/// `field_420` selects from `Actor04400_D00128`, rebuilds the model root
/// rotation through part 0's coordinate, and picks the next state: 4 once the
/// `field_40` hold is empty, 8 / 9 for messages 4 / 5, and 3 after a consumed
/// `field_448` request. Mode 1 recolours from part 1's world position; both
/// clear bit 0x80 of the model flags, which mode 2 sets.
///
/// It has a unit to itself because of the jump table GCC emits for the inner
/// `field_448` switch: a unit's `.rodata` is one object placed at the offset
/// its subsegment names, and this table belongs at 0x13C - ahead of the
/// `actor_104400_header_1b` block - while the overlay's other tables in this
/// region belong at 0x1F4. The manifest pairs a `0x13C` rodata cut with the
/// `0x3538..0x39EC` unit cut, so the table starts the object's rodata.
void Actor04400_Fn03538(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    GpEnemy*         enemy = arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable5   sp    = Actor04400_D00128;

    switch (D_801153F4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            Actor04400_Fn031B8(arg0);
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            Actor04400_Fn02B8C(arg0);
            Actor04400_Fn00874(arg0);
            Actor04400_UpdateRotation(arg0);
            Actor04400_Fn022A8(arg0, 0);
            if (work->field_438 == 0 && enemy->field_40 <= 0) {
                Actor04400_SetState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                Actor04400_SetState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                Actor04400_SetState(arg0, 9);
            } else if (Actor04400_TakeRequest(arg0)) {
                work->field_438 = 0;
                Actor04400_SetState(arg0, 3);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}
