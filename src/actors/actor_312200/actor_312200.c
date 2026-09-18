#include "common.h"

#include "actors/actor_312200.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"

#include "rooms/rooms_shared_80182078.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8 D_actor_312200_80169F44[];
extern u8 D_actor_312200_80169F5C[];

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80161E30);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162178);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_801626C4);

/// Same body as `RoomsShared80182078` / `Actor01900_Fn00FA4` /
/// `func_actor_323000_80162BD0` / `func_actor_356100_80162C90`.
s32 func_actor_312200_80162868(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                      scratch;
    void**                      tail;
    u8*                         head;
    RoomsShared80182078Scratch* st;
    u16                         vz;
    s16                         d;
    s16                         dz;
    s32                         t;
    s32                         hit;

    if (Game_Session->field_4D == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(RoomsShared80182078Scratch);
        st  = (RoomsShared80182078Scratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    RoomsShared80182078ToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    RoomsShared80182078ToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].field_4 == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].field_4 & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].field_8 - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].field_A - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].field_C - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = *(u16*)&st->aim.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&st->aim.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&st->aim.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = *(u16*)&st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12_real();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail  = (void**)G_SCRATCH_HEAD;
    hit   = st->hit;
    *tail = (u8*)*tail + sizeof(RoomsShared80182078Scratch);
    return hit;
}

void func_actor_312200_80162FB4(Task* task)
{
    Actor312200Work* work;
    Actor312200Work* start;
    Actor312200Work* reset;
    Actor312200Work* second;
    Actor312200Work* tick;
    s32              i;
    s32              j;
    s32              k;
    s32              m;

    work = (Actor312200Work*)task->work;
    if (work->field_88C == 1) {
        start = (Actor312200Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->slots[i].field_9 = start->field_896.byte;
            func_800B4114(&start->anim, i, (s16)start->field_892, 0,
                          D_actor_312200_80169F28[start->field_890][(s16)start->field_892]);
        }
        start->field_890 = start->field_892;
        goto advance;
    }
    if (work->field_88C == 2) {
        reset = (Actor312200Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->slots[j].field_9 = reset->field_896.byte;
            Gp_AnimResetSlot(&reset->anim, j, (s16)reset->field_892);
        }
        reset->field_890 = reset->field_892;
    advance:
        work->field_88C = 3;
        work->field_894 = 0;
        work->field_8A8 = 0;
    }
    if (work->field_89A == 2) {
        second                 = (Actor312200Work*)task->work;
        second->field_89E.half = 0x30;
        second->field_8A0      = 0x500;
        for (k = 1; k < 0x13; k++) {
            second->slots[k].field_9 = second->field_89E.byte;
            Gp_AnimResetSlot(&second->anim2, k, (s16)second->field_89C);
        }
        work->field_89A = 3;
    }
    work->field_894++;
    tick = (Actor312200Work*)task->work;
    for (m = 1; m < 0x13; m++) {
        tick->slots[m].field_9 = tick->field_896.byte;
        Gp_AnimTickIndex(&tick->anim, m);
    }
}

/// Spawn body: allocates the actor's 0x984-byte `Actor312200Work`, stores it in
/// `Task::work` and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. The enemy takes the
/// root coordinate's matrix as `field_4` and the model's third part coordinate
/// as `field_18`; the display node the body builds in place points its `field_C`
/// at the block's three-entry `GpRec18` table and its `field_8` at the model's
/// fourth part coordinate. The model coordinate is parented to `Gfx_ViewCoord`
/// and rebuilt once before the three matrix translations are copied to
/// `func_800D7A9C` (start 0, count 3).
void func_actor_312200_80163178(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    TmdObject*       tmd;
    Actor312200Work* mem;
    Actor312200Work* work;
    GpObj*           node;

    obj        = (TmdObject*)task->extra;
    coord      = obj->field_8;
    mem        = (Actor312200Work*)Mem_Calloc(sizeof(Actor312200Work), 0);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    tmd                = (TmdObject*)task->extra;
    tmd->field_1C      = &work->light;
    tmd->field_20      = &work->color;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_4D     = 0;
    enemy->field_4C     = 0;
    enemy->field_4D     = 0;
    func_800B3F84(&work->anim, D_actor_312200_80169F44, (GpAnimObj*)obj, work->poses, work->slots);
    work->field_88C      = 2;
    work->field_892      = 1;
    work->field_896.half = 0x10;
    func_actor_312200_80162FB4(task);
    node           = &work->field_8BC;
    node->field_8  = &((TmdObject*)task->extra)->field_8[3];
    node->field_C  = work->recs;
    node->field_10 = 0;
    node->field_12 = 0;
    node->field_14 = 0;
    node->field_18 = 0x3000A;
    node->field_1C = 0x180;
    node->flags    = 1;
    Gp_LinkObj(2, node);
    node->flags |= 0x8000;
    Gp_InitRec18Table(node->field_C, 3, 0);
    task->field_24  = D_actor_312200_80169F5C;
    work->field_8AC = 0;
    work->field_8AD = 1;
    coord->sub      = &Gfx_ViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, &vec, 0, 3);
    work->field_0 = 0;
    work->field_2 = -1;
    task->state++;
}

/// Per-tick handler and state dispatcher, called with the task second. The
/// handler table is built in place - the show handler at index 0, the tick
/// handler at index 1 - and `field_0` selects from it, unless the global
/// `D_801153F4` holds the actor. `field_4` records whether the state moved
/// before it is re-latched into `field_2`. The tail clears the display node's
/// `GpRec18` record while occupied, re-propagates the root coordinate's
/// translation over the model's three part coordinates while `field_8AD` is
/// set, and then refreshes `field_8AD` from that coordinate's `flg` - so the
/// propagation runs on the frame after the coordinate is dirtied. That same
/// coordinate is rebuilt (`flg` dropped) and the 0x51030008 loop queued while
/// the room is live, from view 0x10 with the 0x7DB action `field_8B8` at 1.
///
/// The `SOFT_BARRIER` is a matching aid, not the original's: without it the
/// scheduler pulls the handler table's first `lui` in front of the `work`
/// load. The trailing `vec` is the original's own - three dead stores, but the
/// frame and the rest of the schedule are built around them.
void func_actor_312200_80163370(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor312200Work* work;
    VECTOR           vec;
    void             (*states[2])(Task*);
    s32              pan;

    work = (Actor312200Work*)task->work;
    SOFT_BARRIER();
    states[0] = func_actor_312200_80163778;
    states[1] = func_actor_312200_801637CC;
    if (D_801153F4 == 0) {
        if (work->field_2 != work->field_0) {
            work->field_4 = 1;
        } else {
            work->field_4 = 0;
        }
        work->field_2 = work->field_0;
        states[work->field_0](task);
        if (work->recs[0].field_4 != 0) {
            Gp_ClearRec18Occupied(work->recs);
        }
        if (work->field_8AD != 0) {
            obj = (TmdObject*)task->extra;
            func_800D7A9C(obj, (VECTOR*)obj->field_8->workm.t, 0, 3);
        }
        if (Game_Session->field_4D != 0) {
            ((TmdObject*)task->extra)->field_8->flg = 0;
            if ((Gp_GetViewIndex() == 0x10) && (work->field_8B8 == 1)) {
                pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
                SndEvt_EnqueueType6(0x51030008, pan,
                                    (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
            }
        }
        if (((TmdObject*)task->extra)->field_8->flg == 0) {
            work->field_8AD = 1;
        } else {
            work->field_8AD = 0;
        }
        vec.vz = 0;
        vec.vy = 0;
        vec.vx = 0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_312200/actor_312200", D_actor_312200_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_312200/actor_312200", ActorsShared80135df4Table);
