#include "common.h"
#include "actors/actor_201200.h"

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
void func_actor_201200_8014A640(Actor201200* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014A88C);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor201200_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor201200RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor201200RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor201200RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor201200RangeScratch*)(head - 0xC))->dx *= ((Actor201200RangeScratch*)(head - 0xC))->dx;
    *(Actor201200RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor201200RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_201200_8014AE60(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 5;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        func_actor_201200_8014A640(arg1);
        return;
    }
    func_actor_201200_8014A640(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = arg1->field_2C->field_8;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor201200_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014B054);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014B5FC);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014BDFC);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014C4C0);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014C738);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014CA08);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014D0B4);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014D4D0);
