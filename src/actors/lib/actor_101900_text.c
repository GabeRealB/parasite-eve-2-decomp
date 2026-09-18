#include "common.h"

#include "actors/actor_101900.h"
#include "actors/actor_101900_facing.h"
#include "actors/actors_shared_80132808.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "psyq/abs.h"
#include "rooms/rooms_shared_80182078.h"

/// Same body as `ActorsShared80132808`: re-aim one joint by `yaw` about Y in
/// world space and write the result back in its parent's frame.
void Actor01900_Fn00260(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    ActorsShared80132808_Accumulate(coord, rotation, &Gfx_ViewCoord);
    func_8004BFF8(yaw, rotation);
    out = ActorsShared80132808_Localize(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// Same body as `Actor04400_CalcPush`: push-out of `pos` from contact record
/// `rec`, how far it sits inside the record's radius (`field_2`) along the
/// direction from the record's centre, carried into grid space.
static __inline__ void Actor01900_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
{
    VECTOR d;
    VECTOR n;
    s32    t;
    s32    pen;

    d.vx = pos->vx - rec->field_8;
    d.vy = 0;
    d.vz = pos->vz - rec->field_C;
    pen  = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen  = rec->field_2 - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = pos->vx - rec->field_8;
    d.vy = pos->vy - rec->field_A;
    d.vz = pos->vz - rec->field_C;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Updates `coord` and computes the push-out of the last kind 0x10000 / 0x30000 record
/// in `recs`, walking until `count` or a zero `field_4`; nonzero when any hit.
s32 Actor01900_Fn0056C(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    Actor01900RepelScratch* head;
    Actor01900RepelScratch* s;
    Actor01900RepelScratch* blk;
    SVECTOR*                offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                                = 0;
    head                                      = *(Actor01900RepelScratch**)G_SCRATCH_HEAD;
    blk                                       = head - 1;
    *(Actor01900RepelScratch**)G_SCRATCH_HEAD = blk;
    s                                         = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].field_4 == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].field_4 & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Actor01900_CalcPush(&s->pos, &recs[s->i], &s->offset);
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
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(offset);
    }
    coord->flg                                 = 0;
    *(Actor01900RepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Bearing of `p` from `eye` in the XZ plane, staged in a scratch block of its
/// own that is released before `ratan2` runs.
static __inline__ s16 Actor01900_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                   head;
    Actor01900AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor01900AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` in the XY plane; used when the facing column is
/// close to vertical.
static __inline__ s16 Actor01900_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                   head;
    Actor01900AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor01900AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

/// Pushes `coord` away from the obstacles in `recs`. Records of kind 0x10000
/// (which also raises the returned `blocked` flag) or 0x30000 each give a
/// bearing, at most eight; bearings more than 0x400 apart cancel each other.
/// Each survivor becomes a 10-unit step added to `push` and to the translation.
s32 Actor01900_Fn008B4(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* push)
{
    u8*                     head;
    Actor01900AvoidScratch* s;
    s16                     diff;
    s16                     t;
    s32                     mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor01900AvoidScratch);
    s                     = (Actor01900AvoidScratch*)*(u8**)G_SCRATCH_HEAD;
    s->blocked            = 0;
    push->vz              = 0;
    push->vy              = 0;
    push->vx              = 0;

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
        if (recs[s->i].field_4 == 0) {
            break;
        }
        s->kind = recs[s->i].field_4 & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = Actor01900_BearingXZ((SVECTOR3*)&recs[s->i].field_8, &s->eye);
        } else {
            s->angle[s->count] = Actor01900_BearingXY((SVECTOR3*)&recs[s->i].field_8, &s->eye);
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
            __asm__ volatile("nop; nop; .word 0x4B98003D");
            gte_stsv(&s->dir);
            push->vx          += s->dir.vx;
            push->vz          += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor01900AvoidScratch);
    return s->blocked != 0;
}

extern SVECTOR Actor01900_D1730C;

s32 Actor01900_Fn00E00(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2)
{
    void**               scratch;
    u8*                  head;
    Actor01900DeltaFlag* s;
    register void*       p asm("v1");
    s32                  val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]   += ((Actor01900DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]   += s->delta.vz.h.hi;
        Actor01900_D1730C.vx = ((Actor01900DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor01900_D1730C.vy = s->delta.vy.w >> 16;
        Actor01900_D1730C.vz = s->delta.vz.w >> 16;
        val                  = ((Actor01900DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor01900_D1730C.vx++;
            } else {
                coord->coord.t[0]--;
                Actor01900_D1730C.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor01900_D1730C.vz++;
            } else {
                coord->coord.t[2]--;
                Actor01900_D1730C.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

/// Same body as `RoomsShared80182078`.
s32 Actor01900_Fn00FA4(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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

    if (gGameSession->viewReady == 1) {
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

/// Rotates the slot-3 player's and this actor's raised root positions into
/// world space and returns `func_800E0308` on the pair.
s32 Actor01900_Fn016F0(Actor01900* arg0)
{
    Task*                   player;
    u8*                     head;
    Actor01900SightScratch* s;
    SVECTOR*                local;
    SVECTOR*                v;
    SVECTOR*                out;

    player                = Game_GetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (Actor01900SightScratch*)(head - 0x1C);
    s->local.vx           = ((Actor01900*)player)->field_2C->field_8->coord.t[0];
    s->local.vy           = ((Actor01900*)player)->field_2C->field_8->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((Actor01900*)player)->field_2C->field_8->coord.t[2];
    Gp_UpdateCoord(&Gfx_ViewCoord);
    v = local;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0_real();
    gte_stsv(&s->out);
    s->out.vx += Gfx_ViewCoord.workm.t[0];
    s->out.vy += Gfx_ViewCoord.workm.t[1];
    s->out.vz += Gfx_ViewCoord.workm.t[2];

    s->local.vx = arg0->field_2C->field_8->coord.t[0];
    s->local.vy = arg0->field_2C->field_8->coord.t[1] - 1000;
    s->local.vz = arg0->field_2C->field_8->coord.t[2];
    Gp_UpdateCoord(&Gfx_ViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0_real();
    gte_stsv(out);
    s->from.vx           += Gfx_ViewCoord.workm.t[0];
    s->from.vy           += Gfx_ViewCoord.workm.t[1];
    s->from.vz           += Gfx_ViewCoord.workm.t[2];
    s->hit                = func_800E0308(&s->out, out);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
    return s->hit;
}

void Actor01900_Fn01950(Actor01900* arg0)
{
    GpAnimPose          pose;
    GpAnimPose          blendPose;
    GpAnimCtx*          anim;
    s16                 weight;
    s16                 i;
    Actor01900AnimWork* work;

    work   = (Actor01900AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_8AA;
            work->slots[i].field_9      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

s32 Actor01900_Fn01A7C(Actor01900Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->field_5A & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 7:
            id = work->field_5A & 0x3FF;
            if (id == 0xF) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x14) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
        case 3:
            id = work->field_5A & 0x3FF;
            if (id == 0x24) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x2C) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        case 4:
            id = work->field_5A & 0x3FF;
            if (id == 0xC && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000C;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        case 11:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        default:
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Cross-fade lengths in frames, indexed by the clip being left and the clip
/// being entered. `Actor01900_Fn01C94` reads one entry per animation change.
extern s8 Actor01900_D16988[][0x2D];

/// Per-frame animation driver: services a pending clip change, advances the
/// body and blend animations, eases the head toward its target yaw and emits
/// whatever sound event the current clip has reached.
///
/// `field_898` is the pending-change request: 1 cross-fades into `field_89E`
/// over the table's frame count, 2 restarts it outright, and both settle to 3.
/// `field_8A6` does the same for the blend animation and `field_8A8`.
void Actor01900_Fn01C94(Actor01900* arg0)
{
    Actor01900AnimWork* work;
    Actor01900AnimWork* w1;
    Actor01900AnimWork* w2;
    Actor01900AnimWork* w3;
    GpEnemy*            enemy;
    s16                 cur;
    s16                 dst;
    s16                 raw;
    s32                 clamped;
    s32                 i;
    s32                 i2;
    s32                 i3;
    s32                 i4;
    s32                 snd;
    s32                 id;
    s32                 pan;
    u16                 cur_u;
    u16                 dst_u;

    work  = (Actor01900AnimWork*)arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_898 == 1) {
        w1 = work;
        if (work->field_89C != work->field_89E) {
            for (i = 1; i < 0x13; i++) {
                w1->slots[i].field_9 = (u8)w1->field_8A2;
                func_800B4114(&w1->anim, i, w1->field_89E, 0,
                              (s32)Actor01900_D16988[w1->field_89C][w1->field_89E]);
            }
            /* Keeps this store from being merged with the identical one the
               `field_898 == 2` path makes just below. */
            SCHED_BARRIER();
            w1->field_89C = (s16)(u16)w1->field_89E;
        }
        goto block_9;
    }
    if (work->field_898 == 2) {
        w2 = work;
        i2 = 1;
        do {
            w2->slots[i2].field_9 = (u8)w2->field_8A2;
            Gp_AnimResetSlot(&w2->anim, i2, w2->field_89E);
            i2++;
        } while (i2 < 0x13);
        w2->field_89C = (s16)(u16)w2->field_89E;
    block_9:
        work->field_898 = 3;
        work->field_8A0 = 0;
        work->field_8B4 = 0;
    }
    if (work->field_8A6 == 2) {
        i3            = 1;
        w1            = (Actor01900AnimWork*)arg0->field_1C;
        w1->field_8AA = 0x30;
        w1->field_8AC = 0x800;
        do {
            w1->slots[i3].field_9 = (u8)w1->field_8AA;
            Gp_AnimResetSlot(&w1->blendAnim, i3, w1->field_8A8);
            i3++;
        } while (i3 < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if (work->field_89A == 0) {
        w3 = (Actor01900AnimWork*)arg0->field_1C;
        i4 = 1;
        do {
            w3->slots[i4].field_9 = (u8)w3->field_8A2;
            Gp_AnimTickIndex(&w3->anim, i4);
            i4++;
        } while (i4 < 0x13);
    } else {
        Actor01900_Fn01950(arg0);
        if (work->blendSlots[1].field_10 & 0x100) {
            work->field_89A = 0;
        }
    }
    dst   = work->field_8AE;
    cur   = work->field_8B0;
    dst_u = (u16)work->field_8AE;
    cur_u = (u16)work->field_8B0;
    if (dst > cur) {
        if ((dst - cur) >= 0x101) {
            work->field_8B0 = cur_u + 0x100;
        } else {
            goto block_25;
        }
    } else if ((cur - dst) >= 0x101) {
        work->field_8B0 = cur_u - 0x100;
    } else {
    block_25:
        work->field_8B0 = (s16)dst_u;
    }
    raw     = work->field_8B0;
    clamped = (u16)work->field_8B0;
    if (raw != 0) {
        if (raw >= 0x401) {
            clamped = 0x400;
        }
        if (raw < -0x400) {
            clamped = -0x400;
        }
        Actor01900_Fn00260(arg0->field_2C->field_8 + 5, (s16)(((s16)clamped * 2) / 3));
        Actor01900_Fn00260(arg0->field_2C->field_8 + 2,
                           (s16)((s32)((s16)clamped + ((u32)(clamped << 0x10) >> 0x1F)) >> 1));
        arg0->field_2C->field_8[5].flg = 0;
        arg0->field_2C->field_8[4].flg = 0;
        arg0->field_2C->field_8[3].flg = 0;
        arg0->field_2C->field_8[2].flg = 0;
    }
    snd = Actor01900_Fn01A7C((Actor01900Work*)work);
    if (snd != 0) {
        id  = snd | (((u16)enemy->field_8 >> 0xC) << 8);
        pan = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
}

/// Binds the actor model's light and colour matrices to the pair kept in its
/// work block.
static __inline__ void Actor01900_BindMatrices(Actor01900* actor)
{
    Actor01900Work* work;
    TmdObject*      obj;

    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->field_1C = &work->field_BB0;
    obj->field_20 = &work->field_BD0;
}

/// Enemy init: allocates the work block, sets up both animation contexts,
/// the three hit/body `GpObj` nodes and the patrol points, then picks the
/// starting state from the spawn flags and rescales the model.
void Actor01900_Fn02018(GpEnemy* enemy, Actor01900* actor)
{
    SVECTOR         dir;
    VECTOR          pos;
    SVECTOR*        v;
    TmdObject*      obj;
    GsCOORDINATE2*  root;
    Actor01900Work* work;
    GpObj*          body;
    GpObj*          head;
    s32             kind;

    root            = actor->field_2C->field_8;
    obj             = actor->field_2C;
    work            = Mem_Calloc(0xC9C, 0);
    actor->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    actor->field_18 = Actor01900_Fn0A6CC;
    Actor01900_BindMatrices(actor);
    enemy->field_4     = &actor->field_2C->field_8->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &actor->field_2C->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_4C     = 0;
    enemy->field_40     = (s16)Actor01900_D0AC54.field_4;
    enemy->field_50     = &Actor01900_D0AC54;
    enemy->field_54     = (s32)&work->field_8E8;
    func_800B3F84(&((Actor01900AnimWork*)work)->anim, Actor01900_D17174, (GpAnimObj*)obj,
                  ((Actor01900AnimWork*)work)->pad_328, ((Actor01900AnimWork*)work)->slots);
    func_800B3F84(&((Actor01900AnimWork*)work)->blendAnim, Actor01900_D17174, (GpAnimObj*)obj,
                  ((Actor01900AnimWork*)work)->pad_764, ((Actor01900AnimWork*)work)->blendSlots);
    work->field_898 = 2;
    work->field_89A = 0;
    work->field_89E = 2;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    Actor01900_Fn01C94(actor);

    work->field_A08.field_C  = &work->field_A28;
    work->field_A08.field_8  = root;
    work->field_A08.field_10 = 0;
    work->field_A08.field_12 = -0x100;
    work->field_A08.field_14 = 0;
    work->field_A08.field_18 = 0x30013;
    work->field_A08.field_1C = 0x180;
    work->field_A08.flags    = 1;
    Gp_LinkObj(2, &work->field_A08);
    work->field_C10       = 0;
    work->field_A08.flags = (work->field_A08.flags | 0x4000) & 0x7FFF;
    Gp_InitRec18Table(work->field_A08.field_C, 0xC, 0);

    body           = &work->field_8C8;
    body->field_8  = &actor->field_2C->field_8[2];
    body->field_C  = &work->field_8E8;
    body->field_10 = 0;
    body->field_12 = 0;
    body->field_14 = 0;
    body->field_18 = 0x30000;
    body->field_1C = 0x180;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->field_C, 0xC, 0);

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_B48;
    head->field_8  = &actor->field_2C->field_8[4];
    head->field_C  = &work->field_B68;
    v              = &dir;
    head->field_10 = v->vx;
    head->field_12 = v->vy;
    head->field_14 = v->vz;
    head->field_1C = 0x180;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->field_C, 1, 0);
    work->field_B48.field_18 = Gp_PackObjPair((GpObj50*)enemy, 0);

    work->field_14     = 0;
    work->field_C[0].x = actor->field_2C->field_8->coord.t[0];
    work->field_C[0].z = actor->field_2C->field_8->coord.t[2];
    Gfx_MatrixCol2(&actor->field_2C->field_8->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    __asm__ volatile("nop; nop; .word 0x4B98003D");
    gte_stsv(v);
    work->field_C[1].x = actor->field_2C->field_8->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->field_2C->field_8->coord.t[2] + dir.vz;

    actor->field_24 = &Actor01900_D1728C;
    root->sub       = &Gfx_ViewCoord;
    root->flg       = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.field_0 = &actor->field_2C->field_8[1];
    work->field_8B8.field_4 = 0x300;
    work->field_8B8.field_6 = 2;
    kind                    = actor->field_36;
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x17;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (((Task*)actor)->spawnArg1 & 0xF) {
        case 2:
            work->field_C2C = Actor01900_D0AC64[0].field_0;
            work->field_C2E = Actor01900_D0AC64[0].field_2;
            work->field_C30 = Actor01900_D0AC64[0].field_4;
            work->field_C32 = Actor01900_D0AC64[0].field_6;
            break;
        case 1:
            work->field_C2C = Actor01900_D0AC64[2].field_0;
            work->field_C2E = Actor01900_D0AC64[2].field_2;
            work->field_C30 = Actor01900_D0AC64[2].field_4;
            work->field_C32 = Actor01900_D0AC64[2].field_6;
            break;
        case 0:
        default:
            work->field_C2C = Actor01900_D0AC64[1].field_0;
            work->field_C2E = Actor01900_D0AC64[1].field_2;
            work->field_C30 = Actor01900_D0AC64[1].field_4;
            work->field_C32 = Actor01900_D0AC64[1].field_6;
            break;
    }

    Actor01900_RescaleYaw(actor->field_2C->field_8, 0x1194);
    work->field_C98 = 0;
    actor->field_30++;
}

/// Spawns the hit-reaction effect for a blow arriving at `yaw`: carves one
/// `SVECTOR` off the scratch head, fills it with one of the twelve presets in
/// `Actor01900_D1722C` picked from the magnitude and sign of `yaw` plus a
/// random draw, hands it to `func_800FDB18` together with the parameter of
/// `id`, and releases the scratch again.
void Actor01900_Fn02664(Actor01900* arg0, s16 yaw, s32 id)
{
    SVECTOR*        head;
    SVECTOR*        carved;
    SVECTOR*        dir;
    Actor01900Work* work;
    GsCOORDINATE2*  coord;
    s32             ang;
    s32             absAng;
    s32             headOn;
    s32             pick;
    u32             rnd;

    ang    = yaw;
    head   = *(SVECTOR**)G_SCRATCH_HEAD;
    carved = head - 1;
    TOUCH_REG(carved);
    *(SVECTOR**)G_SCRATCH_HEAD = carved;
    dir                        = carved;
    SOFT_BARRIER();
    absAng = ang;
    if (yaw < 0) {
        SOFT_TOUCH_REG(absAng);
        absAng = -absAng;
    }
    headOn = absAng < 0x200;
    work   = arg0->field_1C;
    if (headOn) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        SOFT_BARRIER();
        pick = (rnd >> 0x10) & 3;
        switch (pick) {
            case 0:
                head[-1] = Actor01900_D1722C[0];
                break;
            case 1:
                head[-1] = Actor01900_D1722C[1];
                break;
            case 2:
                head[-1] = Actor01900_D1722C[2];
                break;
            case 3:
                head[-1] = Actor01900_D1722C[3];
                break;
            default:
                *dir = Actor01900_D1722C[4];
                break;
        }
    } else if (absAng >= 0x601) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        SOFT_BARRIER();
        pick = (rnd >> 0x10) & 2;
        switch (pick) {
            case 0:
                head[-1] = Actor01900_D1722C[5];
                break;
            case 1:
                head[-1] = Actor01900_D1722C[6];
                break;
            default:
                *dir = Actor01900_D1722C[7];
                break;
        }
    } else if (yaw > 0) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        SOFT_BARRIER();
        if ((rnd >> 0x10) & 1) {
            head[-1] = Actor01900_D1722C[8];
        } else {
            head[-1] = Actor01900_D1722C[9];
        }
    } else {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        SOFT_BARRIER();
        if ((rnd >> 0x10) & 1) {
            head[-1] = Actor01900_D1722C[10];
        } else {
            head[-1] = Actor01900_D1722C[11];
        }
    }
    coord                   = arg0->field_2C->field_8;
    work->field_8B8.field_4 = 0x300;
    work->field_8B8.field_6 = 2;
    work->field_8B8.field_0 = coord + 1;
    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, arg0->field_2C->field_8 + dir->pad, dir, &work->field_8B8);
    *(SVECTOR**)G_SCRATCH_HEAD = *(SVECTOR**)G_SCRATCH_HEAD + 1;
}

/// First `GpRec18` among the twelve at `records` whose id has high word 2,
/// copying its position to `pos`; 0 at the first empty record.
static __inline__ s32 Actor01900_FindHit(GpRec18* records, SVECTOR* pos)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].field_4)
            break;
        if ((records[i].field_4 & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].field_8;
            pos->vy = records[i].field_A;
            pos->vz = records[i].field_C;
            return records[i].field_4;
        }
    }
    return 0;
}

void Actor01900_Fn02A50(Actor01900* arg0)
{
    PlayerStatus*         config = &Player_Status;
    Actor01900Work*       work;
    GpEnemy*              enemy;
    Actor01900HitScratch* head;
    Actor01900HitScratch* s;
    GsCOORDINATE2*        coord;
    Task*                 player;
    SVECTOR*              dir;
    s16                   z;
    s32                   yaw;
    s32                   dx;
    s32                   dy;
    s32                   dz;
    s32                   deathSound;
    s32                   deathPan;
    s32                   hitSound;
    s32                   hitPan;
    s32                   mag;
    s16                   state;
    s16                   effect;
    u32                   damage;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    if (enemy->field_40 > 0) {
        head  = *(Actor01900HitScratch**)G_SCRATCH_HEAD;
        s     = (*(Actor01900HitScratch**)G_SCRATCH_HEAD = head - 1);
        s->id = Actor01900_FindHit(&work->field_8E8, &head[-1].hitPos);
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = Game_GetPtrSlot(3);
                s->hitPos.vx = ((TmdObject*)player->extra)->field_8->workm.t[0];
                s->hitPos.vy = ((TmdObject*)player->extra)->field_8->workm.t[1];
                s->hitPos.vz = ((TmdObject*)player->extra)->field_8->workm.t[2];
            }
            work->field_C40              = 0;
            work->field_C42              = 0;
            arg0->field_2C->field_8->flg = 0;
            Gp_UpdateCoord(arg0->field_2C->field_8);
            s->dir.vx = arg0->field_2C->field_8->workm.t[0];
            s->dir.vy = arg0->field_2C->field_8->workm.t[1];
            s->dir.vz = arg0->field_2C->field_8->workm.t[2];
            s->dir.vx = s->hitPos.vx - arg0->field_2C->field_8->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->field_2C->field_8->workm.t[1];
            z         = s->hitPos.vz - arg0->field_2C->field_8->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->field_2C->field_8;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = Actor01900_NormalizeYaw(s->yaw);
            Actor01900_Fn02664(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x11 && state != 0x1F && state != 0xF && state != 4) {
                s->m = arg0->field_2C->field_8->coord;
                Gfx_RotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_C14 > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    __asm__ volatile("nop; nop; .word 0x4B98003D");
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    __asm__ volatile("nop; nop; .word 0x4B98003D");
                    gte_stsv(dir);
                }
                arg0->field_2C->field_8->coord.t[0] += s->dir.vx;
                arg0->field_2C->field_8->coord.t[1] += s->dir.vy;
                arg0->field_2C->field_8->coord.t[2] += s->dir.vz;
                arg0->field_2C->field_8->flg         = 0;
            }
            dx        = config->coordMtx->t[0] - arg0->field_2C->field_8->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->field_2C->field_8->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->field_2C->field_8->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13 && state != 0x11 && state != 0x1F && state != 0xF && state != 4) {
                    damage    = s->damage * 2;
                    s->damage = damage;
                    if (damage != 0) {
                        s->effect = 4;
                    }
                }
            }
            func_800E2C78((GpObj40*)enemy, s->id, s->damage, 0);
            enemy->field_40 -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_C12 += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[2], effect, NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD) && config->hp > 0 && work->field_C44 == 1) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            }
            if (enemy->field_40 <= 0) {
                deathSound = ((enemy->field_8 >> 0xC) << 8) | 0x400A0008;
                deathPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
            } else {
                hitSound = ((enemy->field_8 >> 0xC) << 8) | 0x400A0007;
                hitPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
            }
            work->field_C10 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    if (work->field_0 == 0x17 || work->field_0 == 0x18) {
                        work->field_0 = 6;
                    }
                    state = work->field_0;
                    if (state == 0x13 || state == 0xF || state == 4 || state == 0x11) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                        work->field_8A6 = 2;
                    } else if (work->field_C12 >= 0x4C || s->crit == 1) {
                        work->field_B48.flags &= 0x7FFF;
                        state                  = work->field_0;
                        if (state != 0x13 && state != 0x1F && state != 0x11) {
                            if (state == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                work->field_0 = 0x13;
                            }
                        }
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    work->field_B48.flags &= 0x7FFF;
                    Gp_SetObjFlag2((GpObj5D*)enemy, s->id, 0);
                    state = work->field_0;
                    if (state != 0x11 && state != 4) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    } else {
                        work->field_0 = 4;
                    }
                    break;
                case 3:
                    work->field_B48.flags &= 0x7FFF;
                    if (work->field_0 == 0x17 || work->field_0 == 0x18) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4((GpObj5C*)enemy, s->id, 0);
                    break;
                case 1:
                    enemy->field_4C       &= 0xFE;
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 4 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
                case 8:
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag <= 0x500) {
                            if (state == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                work->field_0 = 0x13;
                            }
                        }
                    }
                    break;
                case 9:
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
            }
            work->field_C14 = 5;
        } else if (work->field_C14 <= 0) {
            work->field_C12 = 0;
        } else {
            work->field_C14--;
        }
        if (enemy->field_4C & 0xC) {
            s->damage = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->field_4C &= 0xF3;
            }
            if (s->damage != 0) {
                work->field_B48.flags &= 0x7FFF;
                enemy->field_40       -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                state = work->field_0;
                if (state == 7 || state == 0xB || state == 0x12 || state == 0x1B) {
                    work->field_0 = 5;
                } else if (state == 4) {
                    work->field_2 = -1;
                } else if (state != 0xF) {
                    if (state == 0x13 || state == 0x11) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                        work->field_8A6 = 2;
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                        work->field_8A6 = 2;
                    }
                }
            }
        }
        if (enemy->field_40 <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    if (work->field_89E == 2 || work->field_89E == 3) {
                        work->field_0 = 0x1E;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                    work->field_0 = 0x1F;
                } else if (work->field_0 == 4) {
                    work->field_0 = 0x1F;
                } else if (work->field_0 != 0x13 && work->field_0 != 0x1F && work->field_0 != 0x11) {
                    work->field_0 = 0x13;
                }
            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                work->field_0 = 0x1F;
            } else if (work->field_0 != 0x13 && work->field_0 != 0x1F && work->field_0 != 0x11 && work->field_0 != 0x15 && work->field_0 != 0 && work->field_0 != 0x1D && work->field_0 != 0x1E) {
                work->field_0 = 0x13;
            }
        }
        *(Actor01900HitScratch**)G_SCRATCH_HEAD += 1;
    }
}

void Actor01900_Fn03710(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    TmdObject*      obj;
    s32             step;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                 = arg0->field_2C;
        enemy->node.field_4 = 0;
        obj->field_C        = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x17;
        work->field_A08.flags |= 0x4000;
        do {
            Actor01900_Fn01C94(arg0);
        } while ((u32)(work->field_5A & 0x3FF) < 6U);
        work->field_8A2 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    step                         = (s16)work->field_8A2 / 2;
    work->field_8A2              = (u16)step;
    if (step == 1) {
        work->field_8A2 = -0x10;
    }
    if ((s16)work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    Actor01900_Fn01C94(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->field_4C &= 0xFD;
        work->field_0    = 0x11;
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x11;
    }
}

/// Raises the player's weapon when the player is not already in state 2 and
/// stands within 0x1F4 of the actor in Y. Nonzero when it armed.
static __inline__ s32 Actor01900_ArmIfPlayerLevel(Actor01900* arg0)
{
    GpActorWork* player;
    s32          dy;

    player = Game_GetPtrSlot(3);
    if (player->actor->field_954 != 2) {
        dy = arg0->field_2C->field_8->coord.t[1] - player->extra->field_8->coord.t[1];
        if (ABS(dy) < 0x1F4) {
            Gp_ArmStateF0(1);
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers, arms the player if
/// they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x10 and rescales it by 0x1194.
void Actor01900_Fn03854(Actor01900* arg0)
{
    Actor01900Work*       work;
    Actor01900AimScratch* yaw;
    GsCOORDINATE2*        coord;
    TmdObject*            obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 1;
        work->field_8A2       = 0x10;
        work->field_89E       = 9;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
        work->field_8C8.field_1C = 0x180;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
    } else {
        *(Actor01900AimScratch**)G_SCRATCH_HEAD -= 1;
        yaw                                      = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
        arg0->field_2C->field_8->flg             = 0;
        if (work->field_68 & 0x100) {
            work->field_0 = 7;
        }
        yaw->angle      = Actor01900_PositionYaw(arg0, &yaw->delta, &Wip_SysConfig);
        work->field_8AE = yaw->angle;
        if (yaw->angle >= 0x11) {
            yaw->angle = 0x10;
        }
        if (yaw->angle < -0x10) {
            yaw->angle = -0x10;
        }
        coord       = arg0->field_2C->field_8;
        yaw->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, yaw->angle, 1);
        Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
        Actor01900_Fn01C94(arg0);
        *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
    }
}

void Actor01900_Fn03C04(GpAreaKey* session, GsCOORDINATE2* coord)
{
    Actor01900HeightClamp* row;
    s32                    offset;
    s32                    lo;
    s16                    i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

/// `Actor01900_Fn03C04`'s row scan without the clamp: nonzero when the
/// current room has an `Actor01900_D172CC` row.
static __inline__ s32 Actor01900_HasHeightClamp(GpAreaKey* session)
{
    Actor01900HeightClamp* row;
    s16                    i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

s32 Actor01900_Fn03C98(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    Actor01900Delta* head;
    Actor01900Delta* s;
    Actor01900Delta* blk;
    s16              vy;
    SVECTOR*         step;

    if (D_80072729 == 1) {
        return 0;
    }
    head                               = *(Actor01900Delta**)G_SCRATCH_HEAD;
    blk                                = head - 1;
    *(Actor01900Delta**)G_SCRATCH_HEAD = blk;
    s                                  = blk;
    s->moved                           = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor01900_HasHeightClamp(&gGameSession->at4.loc.view)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x180) {
                s->step.vy = (vy <= 0) ? -0x180 : 0x180;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xC0) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xC0);
            gte_ldsv(step);
            __asm__ volatile("nop; nop; .word 0x4B98003D");
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor01900_HasHeightClamp(&gGameSession->at4.loc.view)) {
        Actor01900_Fn03C04(&gGameSession->at4.loc.view, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(Actor01900Delta**)G_SCRATCH_HEAD += 1;
    return s->moved;
}

/// Pushes the actor's root coordinate by half of each nearby kind 0x10000 /
/// 0x30000 record's offset, walking `recs` until `count` or a zero `field_4`.
/// The duplicated coordinate update is load-bearing: loop.c counts both copies
/// before cross-jumping merges them, which keeps `count`'s sign extension in the loop.
s32 Actor01900_Fn03FF8(Actor01900* arg0, GpRec18* recs, s16 count)
{
    Actor01900PushScratch* head;
    Actor01900PushScratch* s;
    Actor01900PushScratch* blk;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->field_2C->field_8[1].flg           = 0;
    head                                     = *(Actor01900PushScratch**)G_SCRATCH_HEAD;
    blk                                      = head - 1;
    *(Actor01900PushScratch**)G_SCRATCH_HEAD = blk;
    s                                        = blk;
    Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
    s->pos.vx = arg0->field_2C->field_8[1].workm.t[0];
    s->pos.vy = arg0->field_2C->field_8[1].workm.t[1];
    s->pos.vz = arg0->field_2C->field_8[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].field_4 == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].field_4 & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0xC0) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0xC0);
                gte_ldsv(&s->offset);
                __asm__ volatile("nop; nop; .word 0x4B98003D");
                gte_stsv(&s->offset);
                arg0->field_2C->field_8->coord.t[0] += s->offset.vx / 2;
                arg0->field_2C->field_8->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->field_2C->field_8->coord.t[0] += s->offset.vx / 2;
                arg0->field_2C->field_8->coord.t[2] += s->offset.vz / 2;
            }
            arg0->field_2C->field_8->flg = 0;
        }
    }
    *(Actor01900PushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor01900_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                     head;
    Actor01900RangeScratch* blk;
    s32                     ret;

    head                                         = *(u8**)G_SCRATCH_HEAD;
    ((Actor01900RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                          = (Actor01900RangeScratch*)(head - 0xC);
    blk->dz                                      = d->vz;
    blk->r                                       = r;
    ((Actor01900RangeScratch*)(head - 0xC))->dx *= ((Actor01900RangeScratch*)(head - 0xC))->dx;
    *(Actor01900RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                     *= blk->dz;
    blk->r                                      *= blk->r;
    *(u8**)G_SCRATCH_HEAD                        = head;
    ret                                          = ((Actor01900RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Circling state: turns toward the player at most 0x30 per step while walking,
/// switching to state 0xA when lined up and far enough, 0xB when close and in
/// front, or 0x1B after 0x5B steps.
void Actor01900_Fn042BC(Actor01900* arg0)
{
    Actor01900Work*         work;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    Actor01900ChaseScratch* s;
    s32                     diff;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x42;
        work->field_89E          = 3;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->field_6 = 0;
        work->field_8 = 0;
        if (arg0->field_36 == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
        return;
    }
    work->field_6++;
    work->field_8++;
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD -= 1;
    s                                          = *(Actor01900ChaseScratch**)G_SCRATCH_HEAD;
    if (Actor01900_Fn03C98(arg0->field_2C->field_8, &work->field_A28, 0xC, 0x60) != 1) {
        if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_8E8, 0xC) != 1) {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    }
    Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
    arg0->field_2C->field_8->flg = 0;
    Actor01900_Fn01C94(arg0);
    s->playerYaw = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0],
                          ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
    Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
    s->yaw          = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw          = Actor01900_NormalizeYaw(s->yaw);
    coord           = arg0->field_2C->field_8;
    s->turn         = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->turn;
    diff            = s->yaw - s->playerYaw;
    if (ABS(diff) < 0x44 && work->field_C30 + work->field_C42 / 2 < work->field_6 && ABS(s->turn) < 0x80) {
        if (Actor01900_OutOfRange(&s->delta, 0x708)) {
            work->field_0 = 0xA;
        }
    }
    if (Actor01900_Fn016F0(arg0) != 1) {
        work->field_6++;
        coord           = arg0->field_2C->field_8;
        s->turn         = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!Actor01900_OutOfRange(&s->delta, 0x2BC)) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_6   = 0;
        work->field_8   = 0;
        coord           = arg0->field_2C->field_8;
        s->turn         = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C28 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C28 = -1;
            } else {
                work->field_C28 = 1;
            }
        }
        if (work->field_C28 == 1) {
            s->turn += 0x300;
        } else {
            s->turn -= 0x300;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C28 = -work->field_C28;
        }
    }
    if (s->turn > 0x30) {
        s->turn = 0x30;
    }
    if (s->turn < -0x30) {
        s->turn = -0x30;
    }
    facing   = arg0->field_2C->field_8;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, s->turn, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            Actor01900_StepForward(arg0->field_2C->field_8, 0x28);
        } else {
            Actor01900_StepForward(arg0->field_2C->field_8, 0xA);
        }
    } else if (work->field_68 & 0x100) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (work->field_C37 != 0) {
        work->field_C37--;
    }
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn04D14(Actor01900* arg0)
{
    Actor01900Work*         work;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    Actor01900ChaseScratch* s;
    s32                     turn;
    s32                     diffPos;
    s32                     diffNeg;
    s32                     yaw;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0xC0;
        work->field_898          = 1;
        work->field_89E          = 3;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C26   = 8;
        work->field_6     = 0;
        work->field_8     = 0;
        Actor01900_D172FC = 0;
        work->field_C40++;
        return;
    }
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD -= 1;
    s                                          = *(Actor01900ChaseScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg               = 0;
    Actor01900_Fn01C94(arg0);
    if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 0) {
        work->field_8++;
    } else {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0],
                               ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = Actor01900_NormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = arg0->field_2C->field_8;
    s->turn = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = arg0->field_2C->field_8;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, s->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    coord                        = arg0->field_2C->field_8;
    work->field_8AE              = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->field_2C->field_8->flg = 0;
    work->field_C24              = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C24 = work->field_C24 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C24 = 2;
    }
    Actor01900_MoveForward(arg0->field_2C->field_8, work->field_C24);
    Actor01900_D172FC += work->field_C24;
    if (work->field_C26 == 8 && work->field_8A2 >= 0x18) {
        work->field_C26 = -1;
    }
    if (work->field_C26 == -1 && work->field_8A2 == 0x12) {
        work->field_C26 = 0;
        work->field_6   = 0;
    }
    if (work->field_C26 == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0],
                                  ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
            Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = Actor01900_NormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw <= 0x400) {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2                           += work->field_C26;
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn0551C(Actor01900* arg0)
{
    Actor01900Work*         work;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    Actor01900ChaseScratch* head;
    Actor01900ChaseScratch* s;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        head                                      = *(Actor01900ChaseScratch**)G_SCRATCH_HEAD;
        obj                                       = arg0->field_2C;
        *(Actor01900ChaseScratch**)G_SCRATCH_HEAD = head - 1;
        s                                         = head - 1;
        arg0->field_20->node.field_4              = 0;
        obj->field_C                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        work->field_89E          = 3;
        work->field_89A          = 0;
        work->field_8AE          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
        coord                                      = arg0->field_2C->field_8;
        s->turn                                    = Actor01900_NormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                     = arg0->field_2C->field_8;
        s->angle                                   = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C20                            = s->angle;
        work->field_C22                            = s->angle + (u16)s->turn * 2;
        *(Actor01900ChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                      = *(Actor01900ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    s                                         = head - 1;
    Actor01900_Fn01C94(arg0);
    Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
    if (work->field_C20 == work->field_C22) {
        if (work->field_C40 < 2 || Actor01900_OutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        }
    }
    if (work->field_C20 > work->field_C22) {
        work->field_C20 -= 0x89;
        if (work->field_C20 < work->field_C22) {
            work->field_C20 = work->field_C22;
        }
    }
    if (work->field_C20 < work->field_C22) {
        work->field_C20 += 0x89;
        if (work->field_C20 > work->field_C22) {
            work->field_C20 = work->field_C22;
        }
    }
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, work->field_C20, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->field_2C->field_8, 0x28);
    } else {
        Actor01900_StepForward(arg0->field_2C->field_8, 0x14);
    }
    if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    *(Actor01900ChaseScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn05B4C(Actor01900* arg0)
{
    Actor01900Work*       work;
    Actor01900AimScratch* head;
    Actor01900AimScratch* aim;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    SVECTOR*              dir;
    MATRIX                mat;
    u16                   angle;

    head                                    = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
    work                                    = arg0->field_1C;
    *(Actor01900AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                     = head - 1;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0xC0;
        work->field_6            = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C28 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C28 = 1;
            } else {
                work->field_C28 = -1;
            }
        }
        if (work->field_C28 == 1) {
            work->field_89E = 0x15;
            if (work->field_C42 == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_C2E + angle;
            } else {
                aim->angle += work->field_C2E;
            }
            work->field_C28 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C42 == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_C2E;
            } else {
                aim->angle -= work->field_C2E;
            }
            work->field_C28 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        Actor01900_Fn01C94(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_C18;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C2A = 0xDE;
        work->field_C42++;
    }
    arg0->field_2C->field_8->flg = 0;
    Actor01900_Fn01C94(arg0);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C2A);
        gte_ldsv(&work->field_C18);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C2A >> 1);
        gte_ldsv(&work->field_C18);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->field_2C->field_8;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->field_2C->field_8;
        coord->coord.t[2] += aim->delta.vz;
        if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 0) {
            work->field_C2A >>= 1;
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
        work->field_2 = -1;
    }
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn05F38(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    GsCOORDINATE2*  coord;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        work->field_6   = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        coord = arg0->field_2C->field_8;
        Actor01900_StepForward(coord, -0x78);
        Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC);
        arg0->field_2C->field_8->flg = 0;
    }
    if (work->field_68 & 0x100) {
        if (enemy->node.field_5 == 1) {
            work->field_0 = 10;
        } else {
            work->field_0 = 6;
        }
    }
}

void Actor01900_Fn06100(Actor01900* arg0)
{
    Actor01900Work*       work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        facing;
    Actor01900AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 8;
        work->field_89E          = 3;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        return;
    }
    *(Actor01900AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                      = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
    if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &aim->delta);
    arg0->field_2C->field_8->flg = 0;
    Actor01900_Fn01C94(arg0);
    coord           = arg0->field_2C->field_8;
    aim->angle      = Actor01900_NormalizeYaw(ratan2(aim->delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = aim->angle;
    if (aim->angle < 0x200) {
        Actor01900_OutOfRange(&aim->delta, 0x384);
    }
    if (aim->angle > 0x40) {
        aim->angle = 0x40;
    }
    if (aim->angle < -0x40) {
        aim->angle = -0x40;
    }
    facing      = arg0->field_2C->field_8;
    aim->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->field_2C->field_8, 0x28);
    } else {
        Actor01900_StepForward(arg0->field_2C->field_8, 0x14);
    }
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn06634(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 1;
        work->field_89E          = 0xA;
        work->field_89A          = 0;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0 && work->field_C34[0] != 1 && work->field_C34[1] != 3 && work->field_C34[2] != 2) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    if (work->field_89E == 0xA) {
        Actor01900_StepForwardHead(arg0->field_2C->field_8, -0x57);
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            Actor01900_Fn01C94(arg0);
        }
        if ((work->field_68 & 0x100) && work->field_89E == 0xB) {
            work->field_8C8.flags &= 0xBFFF;
            if (enemy->field_40 > 0) {
                if (enemy->field_4C & 2) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

void Actor01900_Fn06904(Actor01900* arg0)
{
    Actor01900Work*       work;
    GpEnemy*              enemy;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Actor01900RotScratch* blk;
    u8*                   head;
    u8*                   tail;
    void*                 scratch_base;
    s16                   temp_v0;
    s16                   ang;
    s16                   cur;
    s32                   k;
    s32                   sy;
    u16                   temp_v1;
    u16                   m22;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.field_4   = 1;
        work->field_6         = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0x13);
                break;
            case 5:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->field_2C->field_8 + 2, 3, NULL);
                break;
            case 23:
                arg0->field_2C->field_C = 2;
                break;
            case 17:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 39:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                       = 0x1194;
            head                                    = scratch_base;
            head                                    = *(u8**)(head + 0x3FC);
            coord                                   = arg0->field_2C->field_8;
            blk                                     = (Actor01900RotScratch*)(head - 0x34);
            sy                                      = k - (cur - 0x14) * 0xB;
            *(Actor01900RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                     = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                              = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor01900RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor01900RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Arms `Gp_StateF0` and returns 1 when the player is within 500 units of the
/// actor's height (and not in `field_954` state 2).
void Actor01900_Fn06B4C(Actor01900* arg0)
{
    SVECTOR         delta;
    SVECTOR*        d;
    Actor01900Work* work;
    GpEnemy*        enemy;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    s32             sound;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        Actor01900_D171B4 = &Actor01900_D16960;
        work->field_89E   = 0x10;
        work->field_898   = 2;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x10;
        work->field_8AE          = 0;
        work->field_6            = 0;
        work->field_894          = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((work->field_5A & 0x3FF) == 0xF && work->field_894 != (work->field_5A & 0x3FF) &&
        (*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x01090000) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sound       = 0x51090009;
        if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
            sound = 0x51090008;
        }
        switch ((u8)Gp_GetViewIndex()) {
            case 2:
                SndEvt_EnqueueType6(sound, 0x64, 0);
                break;
            case 3:
                SndEvt_EnqueueType6(sound, 0x50, 0x1F);
                break;
            case 4:
            default:
                SndEvt_EnqueueType6(sound, 0x40, 0x4C);
                break;
        }
    }
    if ((work->field_5A & 0x3FF) == 5 && work->field_894 != (work->field_5A & 0x3FF)) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 1;
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) != 0x01030000 || (u8)Gp_GetViewIndex() != 0x10) {
            func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
        }
    }
    work->field_894 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor01900_OutOfRange(d, work->field_C32)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
            work->field_0 = 6;
        }
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
}

/// Patrol state: walks toward the waypoint `field_14` selects, turning at most
/// 0x20 per step and swapping waypoints on arrival or after 0x15 steps; switches
/// to state 6 when the player comes within `field_C32`, or within 0xFA0 and in
/// front.
void Actor01900_Fn06F40(Actor01900* arg0)
{
    Actor01900Work*        work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor01900TurnScratch* s;
    GsCOORDINATE2*         facing;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        work->field_89E          = 2;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        if (arg0->field_36 == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
        return;
    }
    *(Actor01900TurnScratch**)G_SCRATCH_HEAD -= 1;
    s                                         = *(Actor01900TurnScratch**)G_SCRATCH_HEAD;
    s->delta.vx                               = work->field_C[work->field_14].x - arg0->field_2C->field_8->coord.t[0];
    s->delta.vy                               = 0;
    s->delta.vz                               = work->field_C[work->field_14].z - arg0->field_2C->field_8->coord.t[2];
    if (!Actor01900_OutOfRange(&s->delta, 0xA0) || work->field_6 >= 0x15) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
        work->field_6 = 0;
    }
    Actor01900_Fn01C94(arg0);
    coord           = arg0->field_2C->field_8;
    s->angle        = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = arg0->field_2C->field_8;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, s->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->field_2C->field_8, 0xA);
    }
    if (arg0->field_36 != 0x10) {
        if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) == 1 && ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    } else {
        if ((Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) == 1 ||
             Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_8E8, 0xC) == 1) &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    }
    arg0->field_2C->field_8->flg = 0;
    if (Actor01900_Fn016F0(arg0) != 1) {
        Actor01900_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, &s->delta);
        if (!Actor01900_OutOfRange(&s->delta, work->field_C32)) {
            if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                work->field_0 = 6;
            }
        } else if (!Actor01900_OutOfRange(&s->delta, 0xFA0)) {
            coord    = arg0->field_2C->field_8;
            s->angle = Actor01900_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(s->angle) < 0x300) {
                if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                    work->field_0 = 6;
                }
            }
        }
    }
    if (*(u32*)&Gp_StateF0 & 0xD0000) {
        work->field_0 = 6;
    }
    *(Actor01900TurnScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn07810(Actor01900* arg0)
{
    Actor01900Work*        work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor01900TurnScratch* turn;
    u16                    next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x1E;
    }
    *(Actor01900TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                      = *(Actor01900TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                               = Actor01900_PositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8AE                           = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->field_8;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, turn->angle, 1);
    if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_MoveForward(arg0->field_2C->field_8, work->field_C24);
    if (work->field_C24 > 0) {
        next            = work->field_C24 - 0xA;
        work->field_C24 = next;
        if ((s16)next < 0) {
            work->field_C24 = 0;
        }
    }
    Actor01900_Fn01C94(arg0);
    if ((work->field_68 & 0x100) || work->field_C24 == 0) {
        work->field_0 = 9;
    }
    *(Actor01900TurnScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn07BA8(Actor01900* arg0)
{
    Actor01900Work*       work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Actor01900AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x16;
        work->field_89E          = 2;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        return;
    }
    Actor01900_Fn01C94(arg0);
    *(Actor01900AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                      = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
    aim->angle                               = Actor01900_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE                          = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        Actor01900_Fn01C94(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_89E == 0x11) {
        work->field_6++;
        Actor01900_StepForward(arg0->field_2C->field_8, -0x10);
        if (Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC) != 1) {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
        arg0->field_2C->field_8->flg = 0;
        if (work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn080A8(Actor01900* arg0)
{
    Actor01900Work*       work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Actor01900AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        work->field_89E          = 9;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   &= 0xBFFF;
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(Actor01900AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                      = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg             = 0;
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
    aim->angle      = Actor01900_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0) {
        aim->angle = 0;
    }
    if (aim->angle < 0) {
        aim->angle = 0;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    Actor01900_Fn01C94(arg0);
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn the actor toward the player at up to 0x28 per call. Takes a 0x10-byte
/// scratch block from `G_SCRATCH_HEAD` for the offset to the player and the
/// yaw, steps `field_8AE` toward that yaw, then rebuilds the root coordinate's
/// Y rotation from its own facing. The `field_4` branch is the state's entry.
void Actor01900_Fn083E8(Actor01900* arg0)
{
    Actor01900Work*        work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         facing;
    GsCOORDINATE2*         yawCoord;
    WipSysConfig*          cfg;
    Actor01900AimScratch*  head;
    Actor01900AimScratch*  aim;
    Actor01900AimScratch*  next;
    Actor01900AimScratch** slot;
    s16                    z;
    s16                    ang;
    s16                    delta;
    s16                    wrapped;
    s16                    yaw16;
    s32                    angle;
    s32                    yaw;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 2;
        work->field_8A2          = 0x10;
        work->field_89E          = 0x13;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   &= 0xBFFF;
        Actor01900_Fn01C94(arg0);
        Actor01900_Fn01C94(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }

    cfg  = &Wip_SysConfig;
    slot = (Actor01900AimScratch**)G_SCRATCH_HEAD;
    head = *slot;

    coord             = arg0->field_2C->field_8;
    next              = head - 1;
    head[-1].delta.vx = *(u16*)&cfg->field_4->t[0] - *(u16*)&coord->coord.t[0];
    SOFT_USE_REG(next);
    aim           = next;
    aim->delta.vy = *(u16*)&cfg->field_4->t[1] - *(u16*)&coord->coord.t[1];
    z             = *(u16*)&cfg->field_4->t[2] - *(u16*)&coord->coord.t[2];
    *slot         = aim;
    aim->delta.vz = z;

    facing  = arg0->field_2C->field_8;
    angle   = ratan2((s32)head[-1].delta.vx, (s32)z);
    delta   = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped = delta;
    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    yaw        = wrapped;
    yaw16      = yaw;
    aim->angle = yaw;
    if (work->field_8AE < yaw16) {
        if ((yaw16 - work->field_8AE) >= 0x29) {
            work->field_8AE = (u16)work->field_8AE + 0x28;
        } else {
            work->field_8AE = yaw;
        }
    } else if ((work->field_8AE - yaw16) >= 0x29) {
        work->field_8AE = (u16)work->field_8AE - 0x28;
    } else {
        work->field_8AE = yaw;
    }

    yawCoord   = arg0->field_2C->field_8;
    ang        = ratan2((s32)-yawCoord->coord.m[2][0], (s32)yawCoord->coord.m[2][2]);
    aim->angle = ang;
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s32)ang, 1);
    Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);

    work->field_898 = 2;
    Actor01900_Fn01C94(arg0);
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn08724(Actor01900* arg0)
{
    SVECTOR         vec;
    GpAreaKey       key;
    GpAreaKey*      sessionKey;
    GpAreaKey*      keyPtr;
    u8              areaByte0;
    GpAreaRec*      rec;
    GpCdRec10*      entry;
    GpEffWork*      eff;
    TmdObject*      model;
    s32             idx;
    u32             raw;
    u16             next;
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0x80;
        work->field_8C8.field_1C = 0x180;
        work->field_A08.flags    = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.field_4      = 1;
        work->field_8AE          = 0;
        work->field_6            = 0U;
        vec.vx                   = 0x64;
        vec.vz                   = 0;
        vec.vy                   = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0x13);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch ((s16)next) {
        case 3:
            D_80114B78[0] = &Actor01900_D10B68;
            vec.vz        = 0x64;
            vec.vy        = 0;
            vec.vx        = 0;
            eff           = Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 9, 0x200, &vec);
            goto body;
        case 4:
            D_80114B78[0] = &Actor01900_D10B68;
            vec.vy        = 0;
            vec.vx        = 0;
            eff           = Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 12, 0x200, &vec);
        body:
            if (eff != NULL) {
                sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                raw        = enemy->field_8;
                model      = (TmdObject*)eff->field_0->extra;
                key.stage  = sessionKey->stage;
                key.area   = sessionKey->area;
                key.room   = sessionKey->room;
                areaByte0  = gGameSession->at4.loc.view;
                idx        = raw >> 12;
                /* Both calls take `&key`. CSE of that address across the first
                   jal costs a callee-saved register; the ROM rematerializes
                   `addiu a0, sp, key` for each call. Same shape as
                   Actor02000_Fn0251C. */
                SOFT_BARRIER();
                keyPtr = &key;
                TOUCH_REG(keyPtr);
                key.view = areaByte0;
                Gp_SyncAreaKeyIndex(keyPtr);
                rec             = Gp_GetNestedAreaRec(&key);
                entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
                model->field_24 = entry->field_D;
                model->field_25 = entry->field_E;
                if (model->field_18 != NULL) {
                    Tmd_ProcessStream(model);
                    Tmd_ProcessStream(model);
                }
            }
            break;
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

/// Tint a freshly spawned effect model from the enemy's area record.
static __inline__ void Actor01900_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    GpAreaKey* keyPtr;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    TmdObject* model;
    s32        idx;
    u32        raw;

    if (eff != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = enemy->field_8;
        model      = (TmdObject*)eff->field_0->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = gGameSession->at4.loc.view;
        idx        = raw >> 12;
        SOFT_BARRIER();
        keyPtr = &key;
        TOUCH_REG(keyPtr);
        key.view = areaByte0;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec             = Gp_GetNestedAreaRec(&key);
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
}

void Actor01900_Fn0892C(Actor01900* arg0)
{
    SVECTOR         vec;
    GpEffWork*      eff;
    Actor01900Work* work;
    GpEnemy*        enemy;
    s16             cur;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8C8.field_1C = 0x180;
        work->field_A08.flags    = (u16)(work->field_A08.flags | 0x4000);
        enemy->node.field_4      = 1;
        work->field_8AE          = 0;
        work->field_6            = 0;
        vec.vx                   = 0x64;
        vec.vz                   = 0;
        vec.vy                   = 0;
        work->field_89E          = 2;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x10300, &vec);
        work->field_6 = 0;
    }
    work->field_6++;
    switch (work->field_89E) {
        case 2:
            if (work->field_6 >= 0x10 && (work->field_68 & 2)) {
                work->field_89E = 0x18;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            Actor01900_StepForwardHead(arg0->field_2C->field_8, 0xA);
            Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC);
            if (work->field_6 == 3) {
                D_80114B78[0] = &Actor01900_D10B68;
                vec.vz        = 0x64;
                vec.vy        = 0;
                vec.vx        = 0;
                eff           = Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 9, 0x200, &vec);
                Actor01900_TintEffect(eff, enemy);
            }
            if (work->field_6 == 5) {
                D_80114B78[0] = &Actor01900_D10B68;
                eff           = Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 1, 0x200, NULL);
                Actor01900_TintEffect(eff, enemy);
            }
            break;
        case 0x18:
            if (!(work->field_68 & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0x13);
                    break;
                case 5:
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    Gp_SpawnEff(0x600A5, arg0->field_2C->field_8 + 2, 2, NULL);
                    break;
                case 23:
                    arg0->field_2C->field_C = 2;
                    break;
                case 17:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 39:
                    arg0->field_2C->field_C = 0x80;
                    work->field_0           = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                Actor01900_RescaleYawY(arg0->field_2C->field_8, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 2);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 3);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 4);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 5);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 6);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 7);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 8);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 9);
    Actor01900_ResetYaw(arg0->field_2C->field_8 + 10);
}

/// Whether any of the three `GpRec18` at `records` carries an id with high
/// word 1, stopping at the first empty record.
static __inline__ s32 Actor01900_HasHit(GpRec18* records)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (!records[i].field_4)
            break;
        if ((records[i].field_4 & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers and arms the player
/// if they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x30, rescales it by 0x1194, and once the
/// actor is out of range of the player hands the work state on.
void Actor01900_Fn09694(Actor01900* arg0)
{
    Actor01900Work*       work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        facing;
    GsCOORDINATE2*        src;
    Actor01900AimScratch* aim;
    Actor01900AimScratch* head;
    Actor01900AimScratch* next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x180;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        work->field_89E          = 4;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->field_6          = 0;
        work->field_8          = 0;
        work->field_B48.flags &= 0x7FFF;
        return;
    }
    work->field_6++;
    if (work->field_6 == 0x16) {
        work->field_B48.flags |= 0x8000;
    }
    if (work->field_6 == 0x1D) {
        work->field_B48.flags &= 0x7FFF;
    }
    if (Actor01900_HasHit(&work->field_B68) == 1) {
        work->field_B48.flags &= 0x7FFF;
    }

    head              = *(Actor01900AimScratch**)G_SCRATCH_HEAD;
    next              = head - 1;
    src               = arg0->field_2C->field_8;
    head[-1].delta.vx = Wip_SysConfig.field_4->t[0] - src->coord.t[0];
    SOFT_USE_REG(next);
    aim            = next;
    next->delta.vy = Wip_SysConfig.field_4->t[1] - src->coord.t[1];
    next->delta.vz = Wip_SysConfig.field_4->t[2] - src->coord.t[2];

    *(Actor01900AimScratch**)G_SCRATCH_HEAD = next;
    arg0->field_2C->field_8->flg            = 0;
    Actor01900_Fn01C94(arg0);
    if (work->field_6 < 0xE) {
        coord = arg0->field_2C->field_8;
        aim->angle =
            Actor01900_NormalizeYaw(ratan2(next->delta.vx, next->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = aim->angle;
        if (aim->angle > 0x30) {
            aim->angle = 0x30;
        }
        if (aim->angle < -0x30) {
            aim->angle = -0x30;
        }
        facing      = arg0->field_2C->field_8;
        aim->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
        Actor01900_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    }
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        if (Actor01900_OutOfRange(&aim->delta, 0x2BC)) {
            work->field_0 = 6;
        } else {
            work->field_0 = 0xE;
        }
    }
    *(Actor01900AimScratch**)G_SCRATCH_HEAD += 1;
}

void Actor01900_Fn09BE8(Actor01900* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x180;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0xB;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    Actor01900_Fn00E00(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

void Actor01900_Fn09D3C(GpEnemy* enemy, Actor01900* actor)
{
    VECTOR                 pos;
    Actor01900StateTable   states;
    Actor01900Work*        work;
    Actor01900ViewScratch* scratch;
    Actor01900ViewScratch* head;
    s32                    state;

    work   = actor->field_1C;
    states = Actor01900_D001BC;

    actor->field_2C->field_8->flg = 0;
    Gp_UpdateCoord(actor->field_2C->field_8);
    pos.vx = actor->field_2C->field_8->workm.t[0];
    pos.vy = actor->field_2C->field_8->workm.t[1];
    pos.vz = actor->field_2C->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (D_801153F4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
        case 2:
            actor->field_2C->field_C = 0x80;
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
    }

    head                                     = *(Actor01900ViewScratch**)G_SCRATCH_HEAD;
    *(Actor01900ViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                                  = head - 1;

    if (work->field_C10 > 0) {
        work->field_C10 = (s16)((u16)work->field_C10 - 1);
    } else {
        Actor01900_Fn02A50(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    state         = work->field_0;
    if ((state == 0x1C) || (state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x1E)) {
        work->field_8C8.flags &= 0x7FFF;
        work->field_A08.flags &= 0x7FFF;
    } else {
        work->field_8C8.flags |= 0x8000;
    }
    states.fn[work->field_0](actor);
    Gp_ClearRec18Occupied(&work->field_A28);
    Gp_ClearRec18Occupied(&work->field_8E8);
    Gp_ClearRec18Occupied(&work->field_B68);
    if ((D_801153F2[1] == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    Actor01900_TransformToView(actor->field_2C->field_8 + 2, &scratch->pos);

    work->field_C48[work->field_C98].vx = scratch->pos.vx;
    work->field_C48[work->field_C98].vy = scratch->pos.vy;
    work->field_C48[work->field_C98].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_C98        = (u16)work->field_C98 + 1;
    if (work->field_C98 == 7) {
        work->field_C98 = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->field_1C.vx = work->field_C48[work->field_C98].vx;
        enemy->field_1C.vy = work->field_C48[work->field_C98].vy;
        enemy->field_1C.vz = work->field_C48[work->field_C98].vz;
    } else {
        enemy->field_1C.vx = scratch->pos.vx;
        enemy->field_1C.vy = scratch->pos.vy;
        enemy->field_1C.vz = scratch->pos.vz;
    }
    enemy->field_18 = &Gfx_ViewCoord;
}

void Actor01900_Fn0A314(void)
{
}
