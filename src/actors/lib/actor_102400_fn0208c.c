#include "common.h"
#include "actors/actor_102400_fn0208c.h"

/// Rebuilds the model coordinate from the work block's saved rotation
/// `field_100`, scaled per axis by `field_128..field_12C`, keeping the
/// coordinate's translation. Also derives `field_52` and `field_D4` from the
/// Y and Z scale.
///
/// Shared by `actor_102400` and `actor_202400`.
void Actor02400_Fn0208C(Actor02400Scale* arg0)
{
    GsCOORDINATE2*          coord;
    Actor02400ScaleWork*    work;
    Actor02400ScaleScratch* scratch;
    MATRIX*                 head;

    coord                      = arg0->field_2C->field_8;
    work                       = arg0->field_1C;
    work->field_100            = coord->coord;
    head                       = *(MATRIX**)0x1F8003FC;
    scratch                    = (Actor02400ScaleScratch*)((u8*)head - 0x40);
    *(void**)0x1F8003FC        = scratch;
    work->field_52             = -0xC8000 / work->field_12A;
    work->field_D4             = (work->field_12C * 250) / 4096;
    scratch->scale.vx          = work->field_128;
    scratch->scale.vy          = work->field_12A;
    scratch->scale.vz          = work->field_12C;
    scratch->t.vx              = coord->coord.t[0];
    scratch->t.vy              = coord->coord.t[1];
    scratch->t.vz              = coord->coord.t[2];
    coord->coord               = work->field_100;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->coord.t[0]  = scratch->t.vx;
    coord->coord.t[1]  = scratch->t.vy;
    *(u8**)0x1F8003FC += 0x40;
    coord->coord.t[2]  = scratch->t.vz;
    coord->flg         = 0;
}
