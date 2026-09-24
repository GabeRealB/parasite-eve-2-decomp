#include "common.h"

#include "actors/actor_107000.h"
#include "main/tmd.h"
#include <psyq/inline_c.h>
#include "gte.h"

/// Applies the second form's reaction twist: the rotation `rotation` is
/// turned into a matrix on the scratch stack and multiplied into the rotations
/// of coordinates 3 and 5 on the GTE. The twist's X angle then decays by 0x20
/// a frame; once it would drop to 0x20 or below it is cleared together with
/// `field_38C`.
void Actor07000_Fn06390(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                  = (Actor107000Spawn2Work*)arg0->work;
    head                  = *(u8**)0x1F8003FC;
    *(MATRIX**)0x1F8003FC = (MATRIX*)(head - 0x20);
    scratch               = (MATRIX*)(head - 0x20);
    coord                 = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    *(MATRIX**)0x1F8003FC = (MATRIX*)((u8*)*(MATRIX**)0x1F8003FC + 0x20);
}
