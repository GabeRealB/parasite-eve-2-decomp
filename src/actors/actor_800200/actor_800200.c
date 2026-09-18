#include "common.h"

#include "actors/actor_800200.h"
#include "actors/actors_shared_801625a8.h"
#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/tmd.h"

#include <psyq/abs.h>

extern void         D_actor_800200_80169EF0;
extern GpActorWork* D_80115764;
extern u8           D_801153F4;

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

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
    addr                = &extra->coords;
    coord               = *addr;
    arg0->state++;
    arg0->field_24   = &D_actor_800200_80169EF0;
    arg0->field_18   = &ActorsShared801625a8;
    actor->field_938 = 0x13;
    D_80115764       = arg0;
    coord->sub       = &Gfx_ViewCoord;
    coord->flg       = 0;
    extra->flags     = 0;
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
    next            = arg0->extra->coords;
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

void func_actor_800200_801622B0(GpActorWork* arg0)
{
    void**                 scratch;
    u8*                    head;
    Actor800200VecScratch* sc;
    GameActor*             actor;
    TmdObject*             obj;
    TmdObject*             extra;
    GsCOORDINATE2*         coord;
    GpActorD4*             d4;
    GpObj*                 objs[2];
    s32                    dy;
    s32                    i;
    s8                     bits;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    obj      = arg0->extra;
    *scratch = head - 0x18;
    extra    = obj;
    sc       = (Actor800200VecScratch*)(head - 0x18);
    coord    = extra->coords;
    actor    = arg0->actor;
    d4       = actor->field_910;
    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_90, 0x12, &actor->field_930);
            if ((s8)actor->field_992 == 2) {
                coord->coord.t[0] = actor->field_10;
                coord->coord.t[1] = actor->field_14;
                coord->coord.t[2] = actor->field_18;
            }
        } else {
            actor->field_992 = 0;
        }
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
    }
    *(GsCOORDINATE2*)d4->field_18 = *arg0->extra->coords;
    objs[0]                       = (GpObj*)actor->field_AC;
    objs[1]                       = (GpObj*)actor->field_CC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= 0x4000;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~0x4000;
        }
    }
    actor->field_983 = 0;
    if (D_80115768 == 0 && D_801153F4 == 0) {
        func_actor_800200_801652EC(arg0);
    }
    Gp_ClearRec18Occupied(actor->field_17C);
    Gp_ClearRec18Occupied(&actor->field_910->field_A0);
    if (actor->field_984 & 1) {
        coord->coord.t[1] = actor->field_14 + 8;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    if ((s8)actor->field_986 != 0) {
        sc->vec.vx = (u16)actor->field_30.vx;
        sc->vec.vy = (u16)actor->field_30.vy;
        sc->vec.vz = (u16)actor->field_30.vz;
    } else {
        sc->vec.vx = (u16)coord->workm.m[0][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
        sc->vec.vy = (u16)coord->workm.m[1][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
        sc->vec.vz = (u16)coord->workm.m[2][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
    }
    ((SVECTOR*)actor->field_88)->vx = sc->vec.vx;
    ((SVECTOR*)actor->field_88)->vy = sc->vec.vy;
    ((SVECTOR*)actor->field_88)->vz = sc->vec.vz;
    ((SVECTOR*)actor->field_94)->vx = sc->vec.vx;
    ((SVECTOR*)actor->field_94)->vy = sc->vec.vy;
    ((SVECTOR*)actor->field_94)->vz = sc->vec.vz;
    ((SVECTOR*)actor->field_A0)->vx = sc->vec.vx;
    ((SVECTOR*)actor->field_A0)->vy = sc->vec.vy;
    ((SVECTOR*)actor->field_A0)->vz = sc->vec.vz;
    if (!(extra->flags & 0x80)) {
        Gp_DrawEffGroundQuad((VECTOR3*)coord->workm.t, 0x200, Gp_State1C->field_8);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void func_actor_800200_80162694(Task* arg0)
{
    arg0->state = 3;
}
