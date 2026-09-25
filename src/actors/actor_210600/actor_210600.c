#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor_210600.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"

/// Integer part of the last movement step `func_actor_210600_8014A9D0`
/// applied.
extern SVECTOR D_actor_210600_8015D310;

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_210600_80149E30(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor210600_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor210600_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// Walks the first `count` records of `recs`, up to an empty key, and for
/// every kind 0x10000 or 0x30000 record computes the XZ push-out of the
/// coordinate's world position from it; the last such push is kept in the
/// scratch block, and its length is scaled down to 0x100 when longer. Returns
/// whether any record of those kinds was met. Does nothing, returning 0, while
/// `D_80072729` or the session's `viewReady` is 1.
s32 func_actor_210600_8014A13C(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    Actor210600RepelScratch* head;
    Actor210600RepelScratch* s;
    Actor210600RepelScratch* blk;
    SVECTOR*                 offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                                 = 0;
    head                                       = *(Actor210600RepelScratch**)G_SCRATCH_HEAD;
    blk                                        = head - 1;
    *(Actor210600RepelScratch**)G_SCRATCH_HEAD = blk;
    s                                          = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Actor210600_CalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->flg                                  = 0;
    *(Actor210600RepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Collects the bearings of up to eight kind 0x10000 / 0x30000 records among
/// the first `count` of `recs`, taken in the XZ plane unless the coordinate's
/// facing is near vertical. Any two bearings more than a quarter turn (0x400)
/// apart cancel each other; each bearing left steps `coord` 10 units away from
/// it, the total XZ step accumulating in `pos`. Returns whether a kind 0x10000
/// record was among them; returns 0 at once while the session's `viewReady`
/// or `D_80072729` is 1.
s32 func_actor_210600_8014A484(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                      head;
    Actor210600AvoidScratch* s;
    s16                      diff;
    s16                      t;
    s32                      mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor210600AvoidScratch);
    s                     = (Actor210600AvoidScratch*)*(u8**)G_SCRATCH_HEAD;
    s->blocked            = 0;
    pos->vz               = 0;
    pos->vy               = 0;
    pos->vx               = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = Actor210600_BearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = Actor210600_BearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor210600AvoidScratch);
    return s->blocked != 0;
}

/// Steps `coord` by the movement the first `arg2` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `D_actor_210600_8015D310`. Returns the "moved" flag: set when the X or Z
/// delta is nonzero; where a delta also has a fractional part, the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 func_actor_210600_8014A9D0(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor210600DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor210600DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_210600_8015D310.vx = ((Actor210600DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_210600_8015D310.vy = s->delta.vy.w >> 16;
        D_actor_210600_8015D310.vz = s->delta.vz.w >> 16;
        val                        = ((Actor210600DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_210600_8015D310.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_210600_8015D310.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_210600_8015D310.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_210600_8015D310.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
s32 func_actor_210600_8014AB74(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    Actor210600PushScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(Actor210600PushScratch);
        st  = (Actor210600PushScratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    Actor210600_ToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    Actor210600_ToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].point.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].point.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].point.vz - *(u16*)&st->eye.vz;
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
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail  = (void**)G_SCRATCH_HEAD;
    hit   = st->hit;
    *tail = (u8*)*tail + sizeof(Actor210600PushScratch);
    return hit;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation request handler: step 1 of the work block's `field_87C` seeks
/// every slot 1..18 to the clip in `field_882` through `func_800B4114`,
/// passing `field_886`'s rate byte into the slot and the step `field_880`'s row
/// of `D_actor_210600_8015A498` as the request's fifth argument, then latches
/// the clip into `field_880`; step 2 does the same through
/// `Gp_AnimResetSlot`. Both settle on step 3 and clear the frame counter at
/// `field_884`, which is counted from here on while every slot is ticked.
void func_actor_210600_8014B2C0(Task* task)
{
    Actor210600Work* work;
    Actor210600Work* start;
    Actor210600Work* reset;
    Actor210600Work* tick;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor210600Work*)task->work;
    if (work->field_87C == 1) {
        start = (Actor210600Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->slots[i].rate = start->field_886.byte;
            func_800B4114(&start->anim, i, (s16)start->field_882, 0,
                          D_actor_210600_8015A498[start->field_880][(s16)start->field_882]);
        }
        start->field_880 = start->field_882;
        goto advance;
    }
    if (work->field_87C == 2) {
        reset = (Actor210600Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->slots[j].rate = reset->field_886.byte;
            Gp_AnimResetSlot(&reset->anim, j, (s16)reset->field_882);
        }
        reset->field_880 = reset->field_882;
    advance:
        work->field_87C = 3;
        work->field_884 = 0;
    }
    if (work->field_88A == 2) {
        work->field_88A = 3;
    }
    work->field_884++;
    tick = (Actor210600Work*)task->work;
    for (k = 1; k < 0x13; k++) {
        tick->slots[k].rate = tick->field_886.byte;
        Gp_AnimTickIndex(&tick->anim, k);
    }
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds the model's root part rotation around the yaw it already faces and
/// rescales it uniformly through a 0x34-byte block borrowed from
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. The same code as `func_actor_210600_8014B7B0`,
/// expanded in place where the update body calls it.
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale)
{
    Actor210600Scratch* blk;
    GsCOORDINATE2*      coord;
    u8*                 head;
    s16                 ang;
    u16                 m22;

    head                                  = *(u8**)G_SCRATCH_HEAD;
    coord                                 = ((TmdObject*)task->extra)->coords;
    blk                                   = (Actor210600Scratch*)(head - 0x34);
    *(Actor210600Scratch**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]  = *(u16*)&((Actor210600Scratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]  = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]  = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]  = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]  = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]  = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]  = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]  = *(u16*)&blk->m.m[2][1];
    m22                   = *(u16*)&blk->m.m[2][2];
    coord->flg            = 0;
    coord->coord.m[2][2]  = m22;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x34;
}

/// Update body of the actor's state machine. While the work block's 0x890 flag
/// is clear it runs the animation pass, rebuilds the model root's Y rotation at
/// 0.75 scale, and -- the first time animation slot 1 holds clip 7 -- spawns the
/// effect `Gp_GetIdParam1(0x1001)` on part 1 of the model through
/// `Actor210600Work::field_896`, which remembers the clip slot 0 holds so the
/// spawn is not repeated.
void func_actor_210600_8014B434(void* spawnArg2, Task* task)
{
    Actor210600Work* work;
    SVECTOR          vec;
    GpEffArg         eff;
    s32              id;

    work = (Actor210600Work*)task->work;
    if (work->field_890 == 0) {
        func_actor_210600_8014B2C0(task);
        Actor210600_ScaleRotation(task, 0xC00);

        id = work->slots[1].curRec & 0x3FF;
        if (id == 7 && work->field_896 != id) {
            memset(&vec, 0, 8);
            eff.coord      = ((TmdObject*)task->extra)->coords;
            eff.spawnArgLo = 0x100;
            eff.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, ((TmdObject*)task->extra)->coords + 1, &vec, &eff);
        }
        work->field_896 = work->slots[0].curRec & 0x3FF;
    }
}

/// Display-object mode handler. `arg2` selects the mode: 0 hides the display
/// object by setting bit 0x80 of `TmdObject.flags`, 1 clears `field_C` and so
/// shows it, 2 sets bit 0x4, and any other value clears the field and then sets
/// bit 0x4. Modes 0 and 1 reinstate the object's buffers through
/// `Tmd_AllocBuffers`; modes 0 and 2 arm the work block's 0x890 flag where the
/// other two clear it. `arg1` is unused; it exists because the dispatch passes
/// three arguments.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor210600Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor210600Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_890 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_890 = 0;
            break;
        case 2:
            obj->flags     |= 4;
            work->field_890 = 1;
            break;
        default:
            obj->flags      = 4;
            work->field_890 = 0;
            break;
    }
    return 0;
}
INCLUDE_RODATA("actors/nonmatchings/actor_210600/actor_210600", D_actor_210600_80149E24);
