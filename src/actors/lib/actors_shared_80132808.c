#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"

#include "actors/actors_shared_80132808.h"

void ActorsShared80132808(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    ActorsShared80132808_Accumulate(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = ActorsShared80132808_Localize(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}
