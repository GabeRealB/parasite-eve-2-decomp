#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"
#include "actors/actors_shared_80132808.h"

/// Same body as `ActorsShared80132808`: re-aim one joint by `yaw` about Y in
/// world space and write the result back in its parent's frame.
void func_actor_341700_8016AC64(GsCOORDINATE2* coord, s16 yaw)
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
