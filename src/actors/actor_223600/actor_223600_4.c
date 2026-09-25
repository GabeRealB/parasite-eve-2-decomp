#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_223600.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"

/// 0x88-byte block taken from the scratchpad head by
/// `func_actor_223600_8014A170`: `pos` is the coordinate's world translation,
/// `offset` the latest push-out (scaled down to length 0x100 at the end),
/// `last` its XZ copy, `i` the record cursor and `hit` the return value;
/// `dist` takes 0x7FFE at the record that ends the table.
typedef struct Actor223600RepelScratch {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ SVECTOR offset;
    /* 0x28 */ SVECTOR last;
    /* 0x30 */ SVECTOR pos;
    /* 0x38 */ s32     kind;
    /* 0x3C */ u32     len;
    /* 0x40 */ s16     dist[32];
    /* 0x80 */ s16     i;
    /* 0x82 */ byte    pad_82[4];
    /* 0x86 */ s16     hit;
} Actor223600RepelScratch;
STATIC_ASSERT_SIZEOF(Actor223600RepelScratch, 0x88);

/// XZ push-out of `pos` from one obstacle record: the record's radius minus
/// the horizontal distance to its point, floored at zero, applied along the
/// direction from the point to `pos` taken into grid space.
static __inline__ void Actor223600_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
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

/// Walks the first `count` records of `recs`, up to an empty key, and for
/// every kind 0x10000 or 0x30000 record computes the XZ push-out of the
/// coordinate's world position from it; the last such push is kept in the
/// scratch block, and its length is scaled down to 0x100 when longer. Returns
/// whether any record of those kinds was met. Does nothing, returning 0, while
/// `D_80072729` or the session's `viewReady` is 1.
s32 func_actor_223600_8014A170(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    Actor223600RepelScratch* head;
    Actor223600RepelScratch* s;
    Actor223600RepelScratch* blk;
    SVECTOR*                 offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                                 = 0;
    head                                       = *(Actor223600RepelScratch**)G_SCRATCH_HEAD;
    blk                                        = head - 1;
    *(Actor223600RepelScratch**)G_SCRATCH_HEAD = blk;
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
            Actor223600_CalcPush(&s->pos, &recs[s->i], &s->offset);
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
    *(Actor223600RepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}
