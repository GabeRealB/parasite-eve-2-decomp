#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void func_actor_204000_8014AC8C(Actor104000* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014AED8);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014B4AC);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014BC3C);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor204000_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor104000RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor104000RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor104000RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor104000RangeScratch*)(head - 0xC))->dx *= ((Actor104000RangeScratch*)(head - 0xC))->dx;
    *(Actor104000RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor104000RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Restarts the actor when `field_4` is set; otherwise steps it, occasionally
/// switches to state 3 on a random roll, and arms the player state when the
/// camera target comes within 2000 units.
void func_actor_204000_8014C51C(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    GsCOORDINATE2*    coord;
    SVECTOR           delta;
    SVECTOR*          d;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 5;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_36E |= 0x8000;
        work->field_3A6 &= 0x7FFF;
        work->field_3DE &= 0x7FFF;
        work->field_28E |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        return;
    }
    func_actor_204000_8014AC8C(arg1);
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
    if (!Actor204000_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014C710);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014CD68);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014D5B8);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014DB50);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014DDD4);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014E14C);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014E7E0);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014EDC4);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014F04C);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014F3E8);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014F908);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014FD2C);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_801501A0);

INCLUDE_RODATA("actors/nonmatchings/actor_204000/actor_204000_2", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014A06C);
