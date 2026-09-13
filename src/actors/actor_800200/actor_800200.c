#include "common.h"

#include "actors/actor_800200.h"
#include "actors/actors_shared_801625a8.h"
#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/tmd.h"

extern void         D_actor_800200_80169EF0;
extern GpActorWork* D_80115764;

void func_actor_800200_80162088(GpActorWork* arg0)
{
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  next;
    GsCOORDINATE2** addr;
    GpObj*          obj;
    GpRec18*        recs;
    McSaveData*     save;
    SVECTOR3*       scratch;
    void*           head;
    s32             packed;

    actor               = arg0->actor;
    head                = *(void**)0x1F8003FC;
    *(void**)0x1F8003FC = head - 8;
    scratch             = (SVECTOR3*)(head - 8);
    extra               = arg0->extra;
    addr                = &extra->field_8;
    coord               = *addr;
    arg0->state++;
    arg0->field_24   = &D_actor_800200_80169EF0;
    arg0->field_18   = &ActorsShared801625a8;
    actor->field_938 = 0x13;
    D_80115764       = arg0;
    coord->sub       = &Gfx_ViewCoord;
    coord->flg       = 0;
    extra->field_C   = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    Gp_AnimTickChildSlots(arg0);
    recs            = actor->field_17C;
    obj             = (GpObj*)actor->field_AC;
    actor->field_10 = coord->coord.t[0];
    actor->field_14 = coord->coord.t[1];
    actor->field_18 = coord->coord.t[2];
    obj->field_C    = (GpRec18*)actor->field_88;
    obj->field_8    = coord;
    actor->field_90 = (s32)recs;
    save            = &Mc_SaveData;
    obj->field_10   = 0;
    obj->field_12   = -0xFA;
    obj->field_14   = 0;
    {
        s32 temp;

        temp          = save->field_22;
        obj->field_1C = 0xFA;
        obj->flags    = 4;
        packed        = 0x10000;
        obj->field_18 = temp | packed;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table((GpRec18*)actor->field_90, 0x12, 0);
    obj->flags     |= 0xC200;
    obj             = (GpObj*)actor->field_CC;
    next            = arg0->extra->field_8;
    obj->field_C    = (GpRec18*)actor->field_94;
    obj->field_8    = next + 4;
    actor->field_9C = (s32)recs;
    obj->field_10   = 0;
    obj->field_12   = 0;
    obj->field_14   = 0;
    {
        s32 temp;

        temp          = save->field_22;
        obj->field_1C = 0xC8;
        obj->flags    = 4;
        obj->field_18 = temp | packed;
        Gp_LinkObj(0, obj);
    }
    obj->flags                 |= 0x8000;
    actor->field_984            = 7;
    ((SVECTOR3*)(head - 8))->vx = 0;
    scratch->vy                 = -0x100;
    scratch->vz                 = 0x200;
    Gp_BindActorD4(arg0, scratch, 0x600);
    *(u32*)0x1F8003FC += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200", func_actor_800200_801622B0);

void func_actor_800200_80162694(Task* arg0)
{
    arg0->state = 3;
}
