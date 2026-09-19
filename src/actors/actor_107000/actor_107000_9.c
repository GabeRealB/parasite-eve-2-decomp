#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Node 3's pair table, packed by `Gp_PackPair` into `obj3`, and the enemy
/// record whose `pairTable` points at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern GpU16Pair  D_actor_107000_80139E98;
extern GpPairSrcE D_actor_107000_80139EA0;

/// Animation bank `func_800B3F84` seeds the work block's seven slots from.
extern u8 D_actor_107000_8013F59C[];

/// Message dispatch table this spawn parks in `Task::msgTable`.
extern u8 D_actor_107000_8013F5E0[];

// actor_207000 (func_actor_207000_8014EE88) carries the same body, refused
// promotion for the reason the sibling above is: the pair table, the animation
// bank and the node-3 record it names - D_actor_107000_80139E98,
// D_actor_107000_80139EA0, D_actor_107000_8013F59C and D_actor_107000_8013F5E0
// - are this overlay's own data (the twin's are D_actor_207000_80151E98,
// D_actor_207000_80151EA0, D_actor_207000_8015759C and D_actor_207000_801575E0),
// so one shared object could not link into both.

/// The variant's spawn handler: allocate the `Actor107000Spawn2Work` block,
/// rebind the model's light and colour matrices into it, link its three `GpObj`
/// render nodes and their collision tables, and hand the task over to the state
/// table in `Task::msgTable`.
///
/// Node 1 is the odd one: it points its context at the `GpActorD4Rec` at 0x1FC
/// rather than at a record table, and the record's own `recs` names the one
/// `GpRec18` beside it - the pair `GpActorD4` keeps, and the three constants it
/// carries are that record's fields rather than an object's. Node 3's `field_8`
/// is the model's seventh coordinate (`&coord[6]`), which is the value the
/// sibling `func_actor_107000_80131F0C` computes for its `part`.
///
/// The spawn arg seeds `field_364`/`field_366` the same way it does there, and
/// a high halfword of 1 kills the specimen instead. The tail draws two numbers
/// off `Gp_LcgState` for `field_390`/`field_392`, the spawn countdown and the
/// running total the spawn cue fires on at 5.
void func_actor_107000_80136E88(GpEnemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         part;
    u32                    draw;
    s32                    one;
    s32                    i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[6];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    work->field_366 = (u16)arg1->spawnArg1;
    work->field_364 = (s16)(arg1->spawnArg1 >> 16);
    obj->flags     |= 0x80;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_1BC;
    obj->colorMtx   = &work->field_19C;
    arg0->field_4   = &coord->coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = coord;
    arg0->node.flags = one;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_107000_80139EA0;
    arg0->hp         = D_actor_107000_80139EA0.hpMax;
    arg0->recs       = &work->field_24C[0];
    work->field_35C  = &((TmdObject*)arg1->extra)->coords[1];
    work->field_360  = 0x100;
    work->field_362  = one;
    func_800B3F84((GpAnimCtx*)work, D_actor_107000_8013F59C, obj, work->field_12C,
                  (GpAnimSlot*)&work->slots[0]);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 7);
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
    work->field_1FC.recs       = work->field_214;
    work->obj1.ctx.d4rec       = &work->field_1FC;
    work->obj1.coord           = coord;
    work->obj1.pos.vx          = 0;
    work->obj1.pos.vy          = 0;
    work->obj1.pos.vz          = 0;
    work->obj1.key             = 0;
    work->obj1.radius          = 0;
    work->obj1.flags           = 3U;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->field_214, 1, 0);
    work->obj2.coord    = coord;
    work->obj2.ctx.recs = &work->field_24C[0];
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = -0x258;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x3002A;
    work->obj2.radius   = 0x258;
    work->obj2.flags    = 1U;
    work->obj1.flags    = (u16)(work->obj1.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(&work->field_24C[0], 4, 0);
    work->obj3.coord    = part;
    work->obj3.ctx.recs = &work->field_2CC[0];
    work->obj3.pos.vx   = -0x154;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj2.flags    = (u16)(work->obj2.flags & 0x3DFF);
    work->obj3.key      = Gp_PackPair(&D_actor_107000_80139E98, 0);
    work->obj3.radius   = 0x12C;
    work->obj3.flags    = 1U;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(&work->field_2CC[0], 1, 0);
    work->field_394  = 0;
    work->field_396  = 0;
    work->obj3.flags = (u16)(work->obj3.flags & 0x7FFF);
    draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    work->field_390    = (u16)(((u32)draw >> 16) % 20U + 0x50);
    draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    work->field_392    = (u16)(((u32)draw >> 16) % 50U + 0x32);
    arg1->msgTable     = D_actor_107000_8013F5E0;
    arg1->state        = 4;
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_9", func_actor_107000_80137220);
