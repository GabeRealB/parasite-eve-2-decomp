#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "main/session.h"

/// `mvmva` rotating V0 by the rotation matrix with no translation, spelled as
/// a word since the `inline_c.h` macro of this name assembles to another one.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Twelve opaque bytes the mesh copy carries across unchanged.
typedef struct _Shelter1fHeliportBlk12 {
    u8 data[12];
} _Shelter1fHeliportBlk12;

/// A small mesh as a pointer table: `field_4` is rotated without translation,
/// `field_8` rotated and translated, and `field_C` copied through. The room
/// keeps a pristine source and a working copy that is rebuilt from it.
typedef struct _Shelter1fHeliportMesh {
    s32                      field_0;
    SVECTOR*                 field_4;
    SVECTOR*                 field_8;
    _Shelter1fHeliportBlk12* field_C;
} _Shelter1fHeliportMesh;

extern u8 D_8007216C;
extern u8 D_shelter_1f_heliport_801811D4[][4];

/// The mesh's pristine source and the working copy rebuilt from it.
extern _Shelter1fHeliportMesh D_shelter_1f_heliport_801812AC;
extern _Shelter1fHeliportMesh D_shelter_1f_heliport_80181974;

void func_shelter_1f_heliport_801807C0(void)
{
    s32 i;
    s32 idx = D_8007216C;

    if (gGameSession->at4.loc.place < 3 && idx < 12) {
        if (D_shelter_1f_heliport_801811D4[idx][0] != 0) {
            for (i = 0; i < 4; i++) {
                Gp_MsgSlot4Chain(i, D_shelter_1f_heliport_801811D4[idx][i]);
            }
        }
    }
}

/// Rebuilds the working mesh from its source under `coord`: the first four
/// vectors are rotated only, the eight after them rotated and translated and,
/// when `offset` is non-NULL, shifted by it afterwards.
void func_shelter_1f_heliport_8018085C(GsCOORDINATE2* coord, SVECTOR* offset)
{
    MATRIX                  m;
    long                    flag;
    s32                     i;
    SVECTOR*                d;
    SVECTOR*                s;
    _Shelter1fHeliportMesh* dst = &D_shelter_1f_heliport_80181974;
    _Shelter1fHeliportMesh* src = &D_shelter_1f_heliport_801812AC;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    m = coord->coord;

    d = dst->field_4;
    s = src->field_4;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0_real();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    if (offset != NULL) {
        for (i = 0; i < 8; i++) {
            RotTransSV(s, d, &flag);
            s++;
            d->vx += offset->vx;
            d->vy += offset->vy;
            d->vz += offset->vz;
            d++;
        }
    } else {
        for (i = 0; i < 8; i++) {
            RotTransSV(s++, d++, &flag);
        }
    }
}

void func_shelter_1f_heliport_80180B4C(void)
{
}
