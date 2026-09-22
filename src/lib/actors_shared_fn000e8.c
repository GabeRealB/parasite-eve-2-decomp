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

static __inline__ void Actor01900_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
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

s32 ActorsSharedFn000e8(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
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
