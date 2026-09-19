#include "common.h"

#include "actors/actor_100700.h"
#include "actors/actor_100700_spawn.h"
#include "main/mem.h"
#include "main/tmd.h"

void Gp_DestroyEnemy(void* enemy, void* task);
void Gp_AnimResetSlot(void*, s32, s32);
void Gp_IncStateF0Ref(s32);
void Gp_InitRec18Table(void*, s32, s32);
void Gp_LinkNode(void*);
void Gp_LinkObj(s32, void*);
s32  Gp_PackPair(void*, s32);
void func_800B3F84(void*, void*, TmdObject*, void*, void*);

/// The actor's second spawn handler: allocate the work block, rebind the
/// model's light and colour matrices into it, then link the four collision
/// nodes and their tables. `&obj->field_8[4]` -- the model's fifth
/// coordinate -- is what both the context and the second node hang off.
/// A failed allocation tears the enemy down and leaves the task here.
void Actor00700_Fn00060(Actor00700Ctx* ctx, Actor00700* actor)
{
    Actor00700InitWork* work;
    TmdObject*          obj;
    GsCOORDINATE2*      coord;
    s32                 i;

    obj   = (TmdObject*)actor->field_2C;
    coord = obj->coords;
    work  = Mem_Calloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C = (Actor00700Work*)work;
    obj->flags      = 0;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_1BC;
    obj->colorMtx   = &work->field_19C;
    ctx->field_4    = (void*)(&coord->coord);
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_18     = &actor->field_2C->field_8[4];
    ctx->node.field_4 = 0;
    ctx->field_1C     = 0;
    ctx->field_20     = 0;
    ctx->field_24     = 0;
    ctx->field_50     = &Actor00700_D06DE0;
    ctx->field_54     = (s32)&work->rec2;
    ctx->field_40     = (u16)Actor00700_D06DE0.field_4;
    work->field_338   = 0x100;
    work->field_33A   = 1;
    work->field_334   = coord;
    func_800B3F84(work, &Actor00700_D06E6C, obj, &work->field_12C, &work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot(work, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_37E     = 1;
    work->field_380     = 1;
    work->obj1.coord    = coord;
    work->obj1.ctx.recs = &work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0x2EE;
    work->obj1.key      = 0;
    work->obj1.radius   = 0x12C;
    work->obj1.flags    = 1U;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(&work->rec1, 1, 0);
    work->obj1.flags    = (u16)(work->obj1.flags | 0x8000);
    work->obj2.coord    = &actor->field_2C->field_8[4];
    work->obj2.ctx.recs = &work->rec2;
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = 0;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x30007;
    work->obj2.radius   = 0x96;
    work->obj2.flags    = 1U;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(&work->rec2, 3, 0);
    work->obj3.coord    = coord;
    work->obj3.ctx.recs = &work->rec3;
    work->obj3.pos.vx   = 0;
    work->obj3.pos.vy   = -0xFA;
    work->obj3.pos.vz   = 0;
    work->obj2.flags    = (u16)(work->obj2.flags | 0x8000);
    work->obj3.key      = 0x30007;
    work->obj3.radius   = 0xFA;
    work->obj3.flags    = 1U;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(&work->rec3, 4, 0);
    work->obj4.coord    = coord;
    work->obj4.ctx.recs = &work->rec4;
    work->obj4.pos.vx   = 0;
    work->obj4.pos.vy   = 0;
    work->obj4.pos.vz   = 0x1F4;
    work->obj3.flags    = (u16)(work->obj3.flags | 0x4200);
    work->obj4.key      = Gp_PackPair(&Actor00700_D06DDC, 0);
    work->obj4.radius   = 0xC8;
    work->obj4.flags    = 1U;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(&work->rec4, 1, 0);
    work->obj4.flags = (u16)(work->obj4.flags & 0x7FFF);
    actor->field_30  = 1;
}
