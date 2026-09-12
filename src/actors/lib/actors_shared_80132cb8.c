#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actors_shared_80132cb8.h"

#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/tmd.h"

/// `gpf 12`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void ActorsShared80132cb8(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR     dir;
    GpGridFace  face;
    SVECTOR*    normal;
    SVECTOR*    verts;
    GpGridFace* faces;
    SVECTOR*    d;

    normal = &Gp_GridParams->field_4[index];
    verts  = Gp_GridParams->field_8;
    faces  = Gp_GridParams->field_C;

    face.verts[0] = index * 4;
    face.verts[1] = index * 4 + 1;
    face.verts[2] = index * 4 + 2;
    face.verts[3] = index * 4 + 3;
    face.field_8  = index;
    face.field_A  = 3;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->field_8->coord, normal);
    Gfx_MatrixCol0(&((TmdObject*)task->extra)->field_8->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(normal, normal);
    gte_lddp(scale);
    gte_ldsv(normal);
    gte_gpf12_real();
    gte_stsv(normal);
    gte_lddp(0x1388);
    gte_ldsv(d);
    gte_gpf12_real();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        ((TmdObject*)task->extra)->field_8->coord.t[0] + dir.vx + normal->vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy = dir.vy + normal->vy;
    verts[index * 4].vz                           = verts[index * 4 + 2].vz =
        ((TmdObject*)task->extra)->field_8->coord.t[2] + dir.vz + normal->vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        ((TmdObject*)task->extra)->field_8->coord.t[0] - dir.vx + normal->vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy = -dir.vy + normal->vy;
    verts[index * 4 + 1].vz                           = verts[index * 4 + 3].vz =
        ((TmdObject*)task->extra)->field_8->coord.t[2] - dir.vz + normal->vz;

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->field_8->coord, normal);
    VectorNormalSS(normal, normal);
    gte_lddp(0x1000);
    gte_ldsv(normal);
    gte_gpf12_real();
    gte_stsv(normal);

    if (Game_Session->field_6 == 0x27) {
        face.field_A = 3;
    } else {
        face.field_A = 2;
    }

    faces[index] = face;
}
