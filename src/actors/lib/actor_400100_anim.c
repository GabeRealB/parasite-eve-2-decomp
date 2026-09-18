#include "common.h"
#include "actors/actor_400100.h"
#include "actors/actor_400100_facing.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/1BC.h"
#include "main/mem.h"
#include "psyq/abs.h"

/// Same body as `ActorsShared80132808`: re-aim one joint by `yaw` about Y in
/// world space and write the result back in its parent's frame.
void Actor00100_Fn001FC(GsCOORDINATE2* coord, s16 yaw)
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

/// Bearing of `p` from `eye` in the XZ plane, staged in a scratch block of its
/// own that is released before `ratan2` runs.
static __inline__ s16 Actor00100_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                   head;
    Actor00100AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor00100AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` in the XY plane; used when the facing column is
/// close to vertical.
static __inline__ s16 Actor00100_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                   head;
    Actor00100AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor00100AvoidDelta*)(head - 0x10);
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
/// Each survivor becomes a 10-unit step added to `pos` and to the translation.
s32 Actor00100_Fn00508(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                     head;
    Actor00100AvoidScratch* s;
    s16                     diff;
    s16                     t;
    s32                     mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor00100AvoidScratch);
    s                     = (Actor00100AvoidScratch*)*(u8**)G_SCRATCH_HEAD;
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
            s->angle[s->count] = Actor00100_BearingXZ((SVECTOR3*)&recs[s->i].field_8, &s->eye);
        } else {
            s->angle[s->count] = Actor00100_BearingXY((SVECTOR3*)&recs[s->i].field_8, &s->eye);
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
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor00100AvoidScratch);
    return s->blocked != 0;
}

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `Actor00100_D1BA90`. Returns the "moved" flag: set when the X or Z delta is
/// nonzero, and also when its fractional half is, in which case the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 Actor00100_Fn00A54(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**               scratch;
    u8*                  head;
    Actor00100DeltaFlag* s;
    register void*       p asm("v1");
    s32                  val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]    = coord->coord.t[0] + ((Actor00100DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]    = coord->coord.t[2] + s->delta.vz.h.hi;
        Actor00100_D1BA90.vx = ((Actor00100DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor00100_D1BA90.vy = s->delta.vy.w >> 16;
        Actor00100_D1BA90.vz = s->delta.vz.w >> 16;
        val                  = ((Actor00100DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor00100_D1BA90.vx++;
            } else {
                coord->coord.t[0]--;
                Actor00100_D1BA90.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor00100_D1BA90.vz++;
            } else {
                coord->coord.t[2]--;
                Actor00100_D1BA90.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

/// Rotates the slot-3 player's and this actor's raised root positions into
/// world space and returns `func_800E0308` on the pair.
s32 Actor00100_Fn00BF8(Actor00100* arg0)
{
    Task*                   player;
    u8*                     head;
    Actor00100SightScratch* s;
    SVECTOR*                local;
    SVECTOR*                v;
    SVECTOR*                out;

    player                = Game_GetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (Actor00100SightScratch*)(head - 0x1C);
    s->local.vx           = ((Actor00100*)player)->field_2C->field_8->coord.t[0];
    s->local.vy           = ((Actor00100*)player)->field_2C->field_8->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((Actor00100*)player)->field_2C->field_8->coord.t[2];
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

/// Message handler for the walking animation. `field_0` is the opcode:
/// 0x109 drives the aim state machine, 0x104 picks one of the four poses in
/// `Actor00100_D00004` with the global LCG, 0x1602 plays the step sound with
/// the pose latched out of `Actor00100_D0BDB4`, and 0x202 writes the fixed
/// crouch pose. Every opcode except 0x104/0x1602/0x202 returns 0.
///
/// Each LCG arm keeps its own `value` local: they are separate variables
/// because the arms are separate blocks and one local shared between them
/// changes which register the allocator picks in every arm.
s32 Actor00100_Fn00E58(Actor00100* arg0, s32 arg1, Actor00100Msg* arg2)
{
    Actor00100PoseTable table;
    Actor00100PoseRow*  row;
    Actor00100Work*     work;
    Actor00100Ctx*      ctx;
    s8                  rnd;
    s32                 view;
    u32                 value2;
    u32                 value4;
    u32                 value5;
    u32                 value38;
    u32                 valueDefault;
    s32                 kind;
    s32                 cmd;
    s32                 sub;
    s32                 req;
    s32                 sound;
    s32                 pan;

    work = arg0->field_1C;
    ctx  = arg0->field_20;

    if (arg2->field_0.word == 0x109) {
        kind = arg2->field_2.word;
        switch (kind) {
            case 1:
                work->field_C26 = 0x5A;
                break;
            case 2:
                if (work->field_0 == 0x26) {
                    work->field_0 = 0x26;
                }
                break;
            case 3:
                work->field_C26 = work->field_C24;
                break;
            case 4:
                if (work->field_0 == 0x21) {
                    work->field_0 = 0x22;
                }
                if (work->field_0 == 0x18) {
                    work->field_0 = 0x26;
                }
                break;
        }
        return 1;
    } else {
        work->field_C0C = arg2->field_0.bytes[0];
        work->field_C0D = arg2->field_0.bytes[1];
        work->field_C0E = arg2->field_2.bytes[0];
        if (arg2->field_0.word == 0x104) {
            table = Actor00100_D00004;
            cmd   = arg2->field_2.word;
            switch (cmd) {
                case 0:
                    work->field_0 = 0;
                    break;
                case 1:
                    view = Gp_GetViewIndex() & 0xFF;
                    switch (view) {
                        case 2:
                            value2      = (Gp_LcgState * 5) + 0x71357911;
                            Gp_LcgState = value2;
                            rnd         = ((value2 >> 0x10) % 3) + 1;
                            break;
                        case 4:
                            value4      = (Gp_LcgState * 5) + 0x71357911;
                            Gp_LcgState = value4;
                            rnd         = 1;
                            if (((value4 >> 0x10) & 1) == 0) {
                                rnd = 3;
                            }
                            break;
                        case 5:
                            value5      = (Gp_LcgState * 5) + 0x71357911;
                            rnd         = (value5 >> 0x10) & 1;
                            Gp_LcgState = value5;
                            break;
                        case 3:
                        case 8:
                            value38     = (Gp_LcgState * 5) + 0x71357911;
                            rnd         = ((value38 >> 0x10) & 1) | 2;
                            Gp_LcgState = value38;
                            break;
                        default:
                            valueDefault = (Gp_LcgState * 5) + 0x71357911;
                            rnd          = (valueDefault >> 0x10) & 3;
                            Gp_LcgState  = valueDefault;
                    }
                    row                                 = &table.rows[rnd];
                    arg0->field_2C->field_8->coord.t[0] = row->vx;
                    arg0->field_2C->field_8->coord.t[1] = (s16)row->vy;
                    arg0->field_2C->field_8->coord.t[2] = (s16)row->vz;
                    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s16)row->yaw, 1);
                    arg0->field_2C->field_8->flg = 0;
                    work->field_0                = 5;
                    break;
            }
        }
        if (arg2->field_0.word == 0x1602) {
            sub = arg2->field_2.word;
            switch (sub) {
                case 0:
                    work->field_0 = 0;
                    break;
                case 2:
                    sound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x52160009;
                    pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                    SndEvt_EnqueueType6(sound, pan,
                                        (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
                    work->field_82E = sub;
                    work->field_828 = sub;
                    Actor00100_Fn02788(arg0);
                    Actor00100_Fn02788(arg0);
                    work->field_0   = 0x1C;
                    work->field_C1E = Actor00100_D0BDB4.rows[3].vy;
                    work->field_C20 = Actor00100_D0BDB4.rows[3].vx;
                    work->field_C22 = Actor00100_D0BDB4.rows[3].vz;
                    work->field_C24 = Actor00100_D0BDB4.rows[3].yaw;
                    break;
            }
        }
        if (arg2->field_0.word == 0x202) {
            req = arg2->field_2.word;
            switch (req) {
                case 0:
                    work->field_0 = 0;
                block_46:
                    return 0;
                case 2:
                    work->field_0                       = 0x26;
                    arg0->field_2C->field_8->coord.t[0] = -0x896;
                    arg0->field_2C->field_8->coord.t[1] = 0;
                    arg0->field_2C->field_8->coord.t[2] = 0x5AF;
                    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x3F4, 1);
                    goto block_46;
                default:
                    return 0;
            }
        } else {
            return 0;
        }
    }
}

/// Collects bearings from the obstacles in `recs` into a 16-slot scratch and
/// steps `coord` along each survivor. Same walk as `Actor00100_Fn00508`, but
/// `blocked` is raised only for a kind 0x10000 record whose `field_4` bit 0x80
/// is clear. The scratch is carved before the early-out, so that path leaks it.
s32 Actor00100_Fn01388(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                       head;
    Actor00100AvoidScratch16* s;
    s16                       diff;
    s16                       t;
    s32                       mag;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor00100AvoidScratch16);
    s                     = (Actor00100AvoidScratch16*)*(u8**)G_SCRATCH_HEAD;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x50));
    VectorNormalSS((SVECTOR*)(head - 0x50), (SVECTOR*)(head - 0x50));

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
        s->kind  = recs[s->i].field_4 & 0xFFFF0000;
        s->flags = recs[s->i].field_4 & 0x80;
        switch (s->kind) {
            case 0x10000:
                if (s->flags == 0) {
                    s->blocked = 1;
                }
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = Actor00100_BearingXZ((SVECTOR3*)&recs[s->i].field_8, &s->eye);
        } else {
            s->angle[s->count] = Actor00100_BearingXY((SVECTOR3*)&recs[s->i].field_8, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 16) {
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
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor00100AvoidScratch16);
    return s->blocked != 0;
}

/// Draw the beam between parts `firstJoint` and `secondJoint` of the actor's
/// model: project both ends into view space, widen them into a `width`-half
/// quad, and emit it as a `POLY_FT4` tinted `shade` at height `height`.
void Actor00100_Fn01900(Actor00100* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    Actor00100BeamScratch* s;
    s16                    angle;
    GsCOORDINATE2*         secondCoord;
    GsCOORDINATE2*         firstCoord;
    s32                    offset0;
    s32                    offset1;
    s32                    offset2;
    s32                    offset3;
    s32                    halfX;
    s32                    halfZ;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         view;
    POLY_FT4*              poly;

    coords      = actor->field_2C->field_8;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (Actor00100BeamScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor00100BeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &secondCoord->workm, &s->secondMatrix);
        s->first.vy   = height;
        s->second.vy  = height;
        s->first.vx   = s->firstMatrix.t[0];
        s->first.vz   = s->firstMatrix.t[2];
        s->second.vx  = s->secondMatrix.t[0];
        s->second.vz  = s->secondMatrix.t[2];
        angle         = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX         = (s->first.vx - s->second.vx) / 2;
        halfZ         = (s->first.vz - s->second.vz) / 2;
        offset0       = rcos(angle) * width;
        s->corner0.vy = height;
        s->corner0.vx = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1       = rcos(angle) * width;
        s->corner1.vy = height;
        s->corner1.vx = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2       = rcos(angle) * width;
        s->corner2.vy = height;
        s->corner2.vx = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3       = rcos(angle) * width;
        s->corner3.vy = height;
        s->corner3.vx = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        view          = (GsCOORDINATE2*)((u8*)&Gfx_ViewWorldMtx - OFFSET_OF(GsCOORDINATE2, workm));
        view->flg     = 0;
        Gp_UpdateCoord(view);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)((u8*)poly + 0x28);
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(Actor00100BeamScratch);
    }
}

void Actor00100_Fn01D74(Actor00100* arg0)
{
    GpAnimPose pose;
    GpAnimPose otherPose;
    GpAnimCtx* anim;
    s16        part;
    s16        index;
    s16        next;
    s32        blend;
    s32        invBlend;
    s32        offset;
    u8*        work;
    u8*        slotBase;

    index = 1;
    work  = (u8*)arg0->field_1C;
    anim  = (GpAnimCtx*)(work + 0x1C);
    do {
        part = index - 1;
        switch (part) {
            case 0:
                blend = 0xC00;
                break;
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0xC00;
                break;
            case 3:
                blend = 0x5DE;
                break;
            case 4:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            slotBase                 = work + (index * 0x28);
            *(u8*)(slotBase + 0x43D) = (u8)((Actor00100Work*)work)->field_83A;
            *(s8*)(slotBase + 0x39)  = (s8)(((Actor00100Work*)work)->field_832 - 3);
            func_800B3448(anim, (s32)index, (s32)&pose, 0);
            func_800B3448((GpAnimCtx*)(work + 0x420), (s32)index, (s32)&otherPose, 0);
            Gp_AnimWritePoseCopy(anim, (s32)index, &pose, &otherPose, blend, invBlend);
            next = index + 1;
        } else {
            offset                       = index * 0x28;
            *(s8*)(work + offset + 0x39) = (s8)(((Actor00100Work*)work)->field_832 - 3);
            Gp_AnimTickIndex((GpAnimCtx*)(work + 0x1C), (s32)index);
            next = index + 1;
        }
        index = next;
        /* Keep the next-index value separate from the copied loop index. */
        SOFT_TOUCH_REG(next);
    } while (next < 0x12);
}
