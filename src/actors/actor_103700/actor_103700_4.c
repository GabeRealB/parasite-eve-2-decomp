#include "common.h"

#include "actors/actor_103700.h"
#include "actors/actors_shared_80133d68.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/wipsys.h"

extern s8 D_8011540A;

/// Inlined copy of `func_actor_103700_801350DC`, which lives in a later unit.
static inline void Actor103700_BobInline(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;

    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += D_actor_103700_80139DB8[(arg1 * 15) + (s16)work->field_25E];
}

/// Inlined copy of `func_actor_103700_80135140`.
static inline void Actor103700_SwayInline(Task* task, s32 arg1)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;
    s32              amp;

    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    frame           = work->field_25C + 1;
    work->field_25C = frame;
    if ((s16)frame >= 15) {
        work->field_25C = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_25A = arg1 + ((Gp_LcgState >> 16) & 0x3F);
    }
    amp                = (work->field_25A * D_actor_103700_80139DF4[(s16)work->field_25C] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

void func_actor_103700_801347E0(Task* task)
{
    Actor103700Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    Actor103700SteerScratch* scratch;
    s32                      mode;
    Actor103700Ctx*          ctx;

    scratch                                    = *(Actor103700SteerScratch**)G_SCRATCH_HEAD - 1;
    *(Actor103700SteerScratch**)G_SCRATCH_HEAD = scratch;
    obj                                        = (TmdObject*)task->extra;
    coord                                      = obj->coords;
    work                                       = (Actor103700Work*)task->work;
    mode                                       = work->field_250;
    ctx                                        = (Actor103700Ctx*)task->spawnArg2;

    switch (mode) {
        case 0:
            work->obj.flags &= 0x3FFF;
            obj->flags      |= 0x84;
            ctx->field_14    = 1;
            if (D_8011540A == 0) {
                work->field_250    = 1;
                work->obj.flags   |= 0xC000;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_23C.vx = coord->coord.t[0] - (((Gp_LcgState >> 16) & 0x1FF) + 500);
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_23C.vy = coord->coord.t[1] + (((Gp_LcgState >> 16) & 0x1FF) + 3500);
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_23C.vz = coord->coord.t[2] + (((Gp_LcgState >> 16) & 0x1FF) + 2000);
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_256    = (Gp_LcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~4;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
            }
            break;
        case 2:
            scratch->delta.vx = work->field_23C.vx - coord->coord.t[0];
            scratch->delta.vy = work->field_23C.vy - coord->coord.t[1];
            scratch->delta.vz = work->field_23C.vz - coord->coord.t[2];
            VectorNormalS(&scratch->delta, &scratch->normal);
            coord->coord.t[0] += (scratch->normal.vx * 5) >> 9;
            coord->coord.t[1] += (scratch->normal.vy * 5) >> 9;
            coord->coord.t[2] += (scratch->normal.vz * 5) >> 9;
            work->field_254    = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->field_F];
            ActorsShared80133d68((ActorShared80133d68*)task);

            Actor103700_BobInline(task, 0, 21);
            Actor103700_SwayInline(task, 80);

            if (abs(work->field_23C.vy - coord->coord.t[1]) < 40) {
                work->field_250 = 3;
                work->field_256 = 30;
                Gp_ArmStateF0(1);
            }
            break;
        case 3:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_254    = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->field_F];
            ActorsShared80133d68((ActorShared80133d68*)task);

            Actor103700_BobInline(task, 0, 21);
            Actor103700_SwayInline(task, 80);

            if ((s16)work->field_256 == 30) {
                D_8011540A = 2;
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_24E = mode;
                work->field_250 = 0;
                work->field_256 = 0;
            }
            break;
    }
}
