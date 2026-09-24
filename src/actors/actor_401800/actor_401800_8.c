#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_401800.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"

/// Push-out of contact record `rec`: how far `pos` sits inside the record's
/// radius (`depth`) along the direction from the record's centre, carried
/// into grid space. Only the X/Z components are written.
static __inline__ void Actor401800_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
{
    VECTOR d;
    VECTOR n;
    s32    t;
    s32    pen;

    d.vx = pos->vx - rec->point.vx;
    d.vy = 0;
    d.vz = pos->vz - rec->point.vz;
    pen  = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen  = rec->depth - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = pos->vx - rec->point.vx;
    d.vy = pos->vy - rec->point.vy;
    d.vz = pos->vz - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Refreshes `coord` and computes the push-out of the last kind 0x10000 /
/// 0x30000 record in `recs`, walking until `count` records or a zero `key`;
/// returns nonzero when any record hit. The push is left in the scratch block,
/// clamped to length 0x100. Nothing is done while movement is frozen or the
/// view is not ready.
s32 func_actor_401800_801323D4(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    Actor401800RepelScratch* head;
    Actor401800RepelScratch* s;
    Actor401800RepelScratch* blk;
    SVECTOR*                 offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                                 = 0;
    head                                       = *(Actor401800RepelScratch**)G_SCRATCH_HEAD;
    blk                                        = head - 1;
    *(Actor401800RepelScratch**)G_SCRATCH_HEAD = blk;
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
            Actor401800_CalcPush(&s->pos, &recs[s->i], &s->offset);
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
    *(Actor401800RepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}
