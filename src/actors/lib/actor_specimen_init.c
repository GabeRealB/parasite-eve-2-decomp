#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actor_specimen_init.h"
#include "actors/actors_shared_80136938.h"
#include "actors/actors_shared_80138570.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorSpecimenInit(GpEnemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    GpRec18*               table;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord6;
    u32                    rng;
    u32                    rng2;
    s32                    i;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x39C, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    coord6         = &coord[6];
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_1BC;
    obj->colorMtx  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->bodyPos.vy = -0x64;
    arg0->coord      = coord;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &ActorSpecimenInitParams;
    arg0->hp         = ActorSpecimenInitParams.hpMax;
    arg0->recs       = work->field_24C;
    work->field_35C  = &((TmdObject*)arg1->extra)->coords[1];
    work->field_360  = 0x280;
    work->field_362  = 2;
    func_800B3F84((GpAnimCtx*)work, ActorSpecimenInitAnimBank, obj,
                  work->field_12C, work->slots);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_370            = 1;
    work->field_372            = 1;
    work->field_388            = 0;
    work->field_384            = 0;
    work->field_386            = 0;
    work->field_38E            = 0;
    work->field_38A            = 0;
    work->field_1FC.end0.vz    = 0xBB8;
    work->field_1FC.end0Radius = 0xFA0;
    work->field_1FC.end1Radius = 0x7D0;
    table                      = work->field_214;
    work->field_1FC.recs       = table;
    work->obj1.ctx.d4rec       = &work->field_1FC;
    work->obj1.coord           = coord;
    work->obj1.pos.vx          = 0;
    work->obj1.pos.vy          = 0;
    work->obj1.pos.vz          = 0;
    work->obj1.key             = 0;
    work->obj1.radius          = 0;
    work->obj1.flags           = 3;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(table, 1, 0);
    work->obj2.coord    = coord;
    work->obj2.ctx.recs = work->field_24C;
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = -0x15E;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x3002A;
    work->obj2.radius   = 0x15E;
    work->obj2.flags    = 1;
    work->obj1.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->field_24C, 4, 0);
    work->obj2.flags   |= 0xC200;
    work->obj3.coord    = coord6;
    work->obj3.ctx.recs = work->field_2CC;
    work->obj3.pos.vx   = -0x154;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj3.key      = Gp_PackPair(&ActorsShared80136938Pair, 0);
    work->obj3.radius   = 0x1F4;
    work->obj3.flags    = 1;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->field_2CC, 1, 0);
    work->field_394    = 0;
    work->obj3.flags  &= 0x7FFF;
    rng                = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState        = rng;
    work->field_390    = (u16)((rng >> 16) % 20U + 0x50);
    rng2               = rng * 5 + 0x71357911;
    Gp_LcgState        = rng2;
    work->field_392    = (u16)((rng2 >> 16) % 50U + 0x32);
    arg1->exitCallback = ActorsShared80138570;
    arg1->state       += 1;
}
