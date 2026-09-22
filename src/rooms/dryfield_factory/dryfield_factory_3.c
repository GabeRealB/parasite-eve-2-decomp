#include "common.h"

#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern GpGridParams D_dryfield_factory_80186D38;
extern GpGridParams D_dryfield_factory_80186E04;
extern GpGridParams D_dryfield_factory_80187BF0;
extern GpGridParams D_dryfield_factory_80187BF8;

void func_dryfield_factory_8017DF80(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_3", func_dryfield_factory_8017DF88);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_3", func_dryfield_factory_8017DFE0);

/// Rebuilds four faces of the room's collision grid from a template, moved
/// into the frame of the task's model: the template normals are rotated into
/// grid normals 2..5 and its corners rotated and translated into corners 8..15.
/// When `remapFaces` is set, the template's four face records are copied into
/// faces 2..5 with their corner and normal indices rebased onto those slots.
void func_dryfield_factory_8017E140(Task* task, s32 remapFaces, s32 useAltTemplate)
{
    long           flag;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    GpGridParams*  geom;
    GpGridParams*  src;
    SVECTOR*       s;
    SVECTOR*       d;
    GpGridFace*    sf;
    GpGridFace*    df;
    u16*           sv;
    u16*           dv;
    s32            i;
    s32            j;

    coord = ((TmdObject*)task->extra)->coords;
    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_factory_80187BF0;
    }
    if (useAltTemplate != 0) {
        src = &D_dryfield_factory_80186E04;
    } else {
        src = &D_dryfield_factory_80186D38;
    }

    m = &coord->coord;
    s = src->field_4;
    d = geom->field_4 + 2;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(m);
        gte_ldv0(s);
        s++;
        gte_mvmva_real();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    s = src->field_8;
    d = geom->field_8 + 8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }

    if (remapFaces != 0) {
        sf = src->field_C;
        df = geom->field_C + 2;
        for (i = 0; i < 4; i++) {
            j  = 0;
            dv = df->verts;
            sv = sf->verts;
            do {
                *dv++ = *sv++ + 8;
            } while (++j < 4);
            df->field_8 = sf->field_8 + 2;
            df->field_A = sf->field_A;
            df++;
            sf++;
        }
    }
}
