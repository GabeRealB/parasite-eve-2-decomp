#include "common.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_80135b58.h"
#include "main/task.h"
#include "main/tmd.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->field_264`, scaled along Y by `work->field_2A0` (its own
/// angle field, decaying by 0x50 a frame while it sits above 0x200). The 0x30
/// bytes that hold the scaling matrix and its `VECTOR` are borrowed from the
/// scratchpad and released again; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
///
/// Carried by three enemy slots - `actor_104600`, `actor_204600` and
/// `actor_207200` - which is why it takes the `Task` rather than either
/// overlay's own context type; the shared span is in `configs/USA/overlays.toml`.
void ActorsShared8014b128(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor207200Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->idMap;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->field_8;
    if (work->field_2A0 >= 0x201) {
        work->field_2A0 = (u16)work->field_2A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_264;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}
