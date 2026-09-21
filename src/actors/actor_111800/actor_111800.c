#include "common.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/gameplay.h"

#include "actors/actor_111800.h"
#include "actors/actors_shared_80132808.h"
#include "psyq/inline_c.h"

/// Declared locally with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Main-executable globals with no module header yet: `D_80114C12` is the
/// cutscene-mode flag and `D_80071075` is a live cutscene. `func_80182360` is
/// the room overlay's handler the view-matrix test calls with `t[0]`.
extern u8 D_80071075;
extern s8 D_80114C12;
void      func_80182360(s32);

/// Per-frame handler: ticks animation slots 1..0x12, latches `slots[1].curRec`
/// into `field_492`, then runs the seven-step sequence in `field_484` (reseed,
/// ramp the two angles, wait, reverse the first angle, reseed again, wait,
/// then drop the model coordinate's Z and clear its flag).
void func_actor_111800_8013214C(Task* task)
{
    Actor111800Work* work;
    Actor111800Work* ctx;
    Actor111800Work* work0;
    Actor111800Work* work4;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              i;
    s32              flag1;
    s32              flag2;
    s32              z;

    i     = 1;
    work  = (Actor111800Work*)task->work;
    obj   = (TmdObject*)task->extra;
    ctx   = work;
    coord = obj->coords;
    do {
        Gp_AnimTickIndex(&ctx->anim, i & 0xFFFF);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    SCHED_BARRIER();
    SCHED_BARRIER();
    SCHED_BARRIER();
    SCHED_BARRIER();
    ctx->field_492 = ctx->slots[1].curRec;
    switch (work->field_484) {
        case 0:
            work0            = (Actor111800Work*)task->work;
            work0->field_492 = 0;
            SCHED_BARRIER();
            i = 1;
            do {
                func_800B4114(&work0->anim, i & 0xFFFF, 0, 0, 0xF);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            goto advance;
        case 1:
            flag1 = 0;
            flag2 = 0;
            if (work->field_48C >= -0x154) {
                work->field_48C -= 0x10;
            } else {
                flag1 = 1;
            }
            if (work->field_494 >= -0x154) {
                work->field_494 -= 0x10;
            } else {
                flag2 = 1;
            }
            if (flag2 & flag1) {
                goto advance;
            }
            break;
        case 2:
            work->field_488 += 1;
            if ((u32)work->field_488 < 0x1FU) {
                break;
            }
            work->field_484 += 1;
            break;
        case 3:
            if (work->field_48C < 0x2AA) {
                work->field_48C += 0x80;
                break;
            }
            goto advance;
        case 4:
            work->field_488 += 1;
            if ((u32)work->field_488 < 0x10U) {
                break;
            }
            i                = 1;
            work4            = (Actor111800Work*)task->work;
            work4->field_492 = 0;
            SCHED_BARRIER();
            do {
                func_800B4114(&work4->anim, i & 0xFFFF, 2, 0, 0xA);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            goto advance;
        case 5:
            work->field_488 += 1;
            if ((u32)work->field_488 < 2U) {
                break;
            }
        advance:
            work->field_488  = 0;
            work->field_484 += 1;
            break;
        case 6:
            z                 = coord->coord.t[2];
            coord->flg        = 0;
            coord->coord.t[2] = z - 0x96;
            break;
    }
}

/// Spawn/setup handler for the actor's model: allocates the 0x498-byte work
/// block into `Task::work`, hands the model object the view coordinate and the
/// block's two matrices, builds the animation context over the nineteen slots
/// and applies the nested area record matching id 0x13 through `Gp_SetTmdBytes`.
///
/// The allocation is parked in `Task::work` and read back before it is used, so
/// the first thing the block is named by is a reload: the `memCalloc` result is
/// stored straight from `$v0` and the failing branch tests that register, which
/// is what leaves the surviving copy of it to be emitted *after* the branch --
/// one declaration earlier and the copy lands before the test.
void func_actor_111800_80132390(Task* task)
{
    Actor111800Work* work;
    Actor111800Work* work2;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    s32              i;

    coord      = ((TmdObject*)task->extra)->coords;
    obj        = (TmdObject*)task->extra;
    task->work = (TaskIdMap*)memCalloc(0x498, false);
    if (task->work == NULL) {
        taskKill(task);
        return;
    }
    work = (Actor111800Work*)task->work;
    Mem_Set(work, 0U, 0x498U);
    coord->sub = &gGfxViewCoord;
    Tmd_AllocBuffers(obj);
    obj->lightMtx = &work->field_43C;
    obj->colorMtx = &work->field_45C;
    obj->flags    = 0;
    func_800B3F84(&work->anim, D_actor_111800_8013A448, obj, work->field_30C,
                  &work->slots[0]);
    work->field_47C  = gameGetPtrSlot(3);
    work->field_480  = D_80073B8C[0];
    i                = 1;
    work2            = (Actor111800Work*)task->work;
    work2->field_492 = 0;
    do {
        work2->slots[i & 0xFFFF].rate = 0x10;
        Gp_AnimResetSlot(&work2->anim, i & 0xFFFF, 5);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    work->field_494 = 0x155;
    place           = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    while (place->entryId != 0xFF && place->entryId != 0x13) {
        place++;
    }
    Gp_SetTmdBytes(obj, (s8)place->tpage, (s8)place->clut);
}

/// `ActorsShared80132808_Accumulate` with the seed copy taken from `src`
/// (`coords[5].coord`) rather than `arg0->coord`, so the 8-word load is
/// `0x194(coords)` while `arg0` stays the part pointer.
static __inline__ s32 Actor111800_Accumulate(GsCOORDINATE2* arg0, MATRIX* arg1, MATRIX* src)
{
    MATRIX         matrix;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = arg0->sub;
    view  = &gGfxViewCoord;
    *arg1 = *src;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->sub;
    }
}

/// Per-frame state machine. State 0 waits until no cutscene is up, then runs
/// the spawn handler and advances. State 1 ticks slots 1..0x12, latches
/// `field_492`, and advances after `func_80182360` when the view matrix is in
/// range. State 2 runs the sequence handler and kills the task once the
/// session is idle. Every path but the state-0 wait then pitches part 5 by
/// `field_494`, writes it back, yaws it through `ActorsShared80132808`, and
/// rebuilds the colour matrix around part 1's translation.
void func_actor_111800_8013251C(Task* task)
{
    MATRIX           mtx;
    Actor111800Work* work;
    Actor111800Work* ctx;
    Actor111800Work* work2;
    TmdObject*       extra;
    TmdObject*       obj;
    GsCOORDINATE2*   coords;
    GsCOORDINATE2*   part;
    MATRIX*          viewMtx;
    s32              state;
    s32              i;
    s32              x;
    u16              angle;

    state = task->state;
    work  = (Actor111800Work*)task->work;
    switch (state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                func_actor_111800_80132390(task);
                task->state += 1;
                break;
            }
            return;
        case 1:
            ctx = work;
            i   = 1;
            do {
                Gp_AnimTickIndex(&ctx->anim, i & 0xFFFF);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            ctx->field_492 = ctx->slots[1].curRec;
            viewMtx        = work->field_480;
            x              = viewMtx->t[0];
            if ((x >= 0x5DD && viewMtx->t[2] >= -0x513) || (x >= 0xC81 && viewMtx->t[2] < -0x514)) {
                work->field_484 = 0;
                func_80182360(x);
                task->state += 1;
            }
            break;
        case 2:
            func_actor_111800_8013214C(task);
            if (gGameSession->eventState == 0) {
                taskKill(task);
            }
            break;
    }
    extra  = (TmdObject*)task->extra;
    angle  = *(u16*)&work->field_494;
    coords = extra->coords;
    part   = coords + 5;
    Actor111800_Accumulate(part, &mtx, &coords[5].coord);
    RotMatrixX((s32)(s16)angle, &mtx);
    ActorsShared80132808_Localize(part, &mtx);
    Mem_CopyUnaligned(&mtx, &part->coord, 0x12U);
    part->flg = 0;
    Gp_UpdateCoord(part);
    ActorsShared80132808(((TmdObject*)task->extra)->coords + 5, work->field_48C);
    obj                 = (TmdObject*)task->extra;
    work2               = (Actor111800Work*)task->work;
    ((VECTOR*)&mtx)->vx = obj->coords[1].workm.t[0];
    ((VECTOR*)&mtx)->vy = ((TmdObject*)task->extra)->coords[1].workm.t[1];
    ((VECTOR*)&mtx)->vz = ((TmdObject*)task->extra)->coords[1].workm.t[2];
    func_800D7A9C(obj, (VECTOR*)&mtx, 0, 3);
    ((VECTOR*)&mtx)->vz = 0x555;
    ((VECTOR*)&mtx)->vy = 0x555;
    ((VECTOR*)&mtx)->vx = 0x555;
    ScaleMatrix(&work2->field_45C, (VECTOR*)&mtx);
}
