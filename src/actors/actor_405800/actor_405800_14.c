#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_405800.h"

/// Wraps the actor's pitch, yaw and roll (`field_80`, `field_82`, `field_84`)
/// to 12 bits and rebuilds the model root's rotation from them: an identity
/// matrix taken off `G_SCRATCH_HEAD` is turned by roll, pitch and then yaw,
/// and its 3x3 copied into the root coordinate, whose translation is left
/// alone.
void func_actor_405800_80139FC4(Task* arg0)
{
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    MATRIX*          m;
    MATRIX*          dst;

    work->field_80           &= 0xFFF;
    work->field_82           &= 0xFFF;
    work->field_84           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->field_84, m);
    RotMatrixX((s16)work->field_80, m);
    func_8004BFF8((s16)work->field_82, m);
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
