#include "common.h"

#include "actors/actors_shared_80136938.h"
#include "actors/actors_shared_80136c80.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void ActorsShared80136938(Task* arg0)
{
    ActorsShared80136938Work* work;
    GsCOORDINATE2*            coord;
    GpEffWork*                eff;
    GpObj*                    obj;
    GpActorD4Rec*             rec;
    GpMtxWords*               rot;
    SVECTOR*                  head;
    SVECTOR*                  vec;
    SVECTOR                   local;
    MATRIX*                   matrix;
    u32                       rng;
    s32                       angle;
    s32                       pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(0x58, false);
    if (work == NULL) {
        Task_CallExit(arg0);
        return;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    obj                        = &work->obj;
    arg0->work                 = work;
    eff                        = Gp_SpawnEff(0x60081, coord, 0, NULL);
    arg0->spawnArg2            = eff->task;
    Task_Reparent(arg0, eff->task);
    angle       = arg0->spawnArg1;
    vec->vy     = -rcos(angle);
    vec->vx     = rsin(angle);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    vec->vz     = 0xE000 - ((Gp_LcgState >> 16) & 0x1FFF);
    matrix      = &coord->coord;
    local       = *vec;
    SOFT_BARRIER();
    rec = &work->rec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)" : : "m"(local) : "$2");
    gte_rtv0_real();
    gte_stsv(vec);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    gte_lddp(((Gp_LcgState >> 16) & 0x1F) + 0x1E);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(&work->vec);
    rot                = (GpMtxWords*)&coord->coord;
    rot->w0            = 0x1000;
    rot->w1            = 0;
    rot->w2            = 0x1000;
    rot->w3            = 0;
    rot->h4            = 0x1000;
    coord->coord.t[0] += work->vec.vx;
    rng                = Gp_LcgState * 5 + 0x71357911;
    coord->coord.t[1] += (rng >> 16) & 0x7F;
    coord->coord.t[2] += work->vec.vz;
    Gp_LcgState        = rng;
    coord->flg         = 0;
    obj->coord         = coord;
    obj->ctx.d4rec     = rec;
    obj->pos.vx        = 0;
    obj->pos.vy        = 0;
    obj->pos.vz        = 0;
    obj->radius        = 0;
    obj->key           = Gp_PackPair(&ActorsShared80136938Pair, 1);
    obj->flags         = 3;
    rec->recs          = &work->rec2;
    rec->end1.vx       = 0;
    rec->end1.vy       = 0;
    rec->end1.vz       = 0;
    rec->end0.vx       = 0;
    rec->end0.vy       = 0;
    rec->end0.vz       = 0;
    rec->end0Radius    = 0x96;
    rec->end1Radius    = 0x96;
    Gp_InitRec18Table(&work->rec2, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags                 |= 0xC000;
    arg0->exitCallback          = ActorsShared801511c8;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
    arg0->state                += 1;
    __asm__("" : "=r"(rec), "+r"(coord));
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x40460002, pan, (s8)gpGetObjDepth(coord));
    ActorsShared80136c80(arg0);
}
