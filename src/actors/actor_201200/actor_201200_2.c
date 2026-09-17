#include "common.h"
#include "actors/actor_201200.h"
#include "actors/actors_shared_8014a7b0.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/sound.h"

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
void func_actor_201200_8014A640(Actor201200* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_actor_201200_8014C738(Actor201200Ctx* arg0, Actor201200* arg1);

extern u8                    D_801153F4;
extern Actor201200StateTable D_actor_201200_80149F04;

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

/// Per-frame tick: refreshes the coordinate and color, handles the render
/// mode in `D_801153F4`, dispatches the substate handler and plays its sound.
void func_actor_201200_8014D4D0(Actor201200Ctx* arg0, Actor201200* arg1)
{
    VECTOR                pos;
    Actor201200StateTable table;
    Actor201200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                         = arg1->field_1C;
    table                        = D_actor_201200_80149F04;
    arg1->field_2C->field_8->flg = 0;
    Gp_UpdateCoord(arg1->field_2C->field_8);
    pos.vx = arg1->field_2C->field_8->workm.t[0];
    pos.vy = arg1->field_2C->field_8->workm.t[1];
    pos.vz = arg1->field_2C->field_8->workm.t[2];
    Gp_UpdateActorColor((struct _GpEnemy*)arg0, &pos, 0, 0);
    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
        case 2:
            arg1->field_2C->field_C = 0x80;
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    if (arg0->field_40 > 0) {
        func_actor_201200_8014C738(arg0, arg1);
        if (arg0->field_40 <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(&work->rec1B8);
    Gp_ClearRec18Occupied(&work->rec250);
    Gp_ClearRec18Occupied(&work->rec2E8);
    id = ActorsShared8014a7b0((ActorsShared8014a7b0Work*)work);
    if (id != 0) {
        snd = id | ((arg0->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
    }
    if (work->field_3D8 != 0) {
        func_800D7A9C(arg1->field_2C, (VECTOR*)arg1->field_2C->field_8->workm.t, 0, 3);
    }
    if (Game_Session->field_4D != 0) {
        arg1->field_2C->field_8->flg = 0;
    }
}
