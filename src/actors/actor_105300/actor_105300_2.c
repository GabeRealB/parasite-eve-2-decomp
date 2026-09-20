#include "common.h"

#include "actors/actor_105300.h"
#include "actors/actors_shared_8013246c.h"

#include "actors/actors_shared_80133610.h"
#include "actors/actors_shared_80136574.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "main/gfx.h"
#include "main/mem.h"

extern GpPairSrcE         D_actor_105300_8013D3A0;
extern Actor05300SpawnPos D_actor_105300_80133A20[2];
extern Actor05300Clip     D_actor_105300_8013D3E0[];
extern Actor05300SndRow   D_actor_105300_8013D3C4[];
extern u32                D_actor_105300_8013D3BC;
extern u8                 D_801153F4;
extern s32                D_actor_105300_8013D3B0[];
extern SVECTOR            D_actor_105300_80133A40[];
extern s32                D_actor_105300_8013D3B4;
extern GpMsgEntry         D_actor_105300_80133A00[];
extern SVECTOR            D_actor_105300_80133A30[2];
extern GpPairSrcE         D_actor_105300_8013D390;
extern u16                D_actor_105300_8013D394[];
extern u32                D_actor_105300_8013D3C0;
extern GpAnimSet*         D_actor_105300_8013D414[];
extern TaskDesc           D_actor_105300_8013D3FC[2];

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

/// Spawn state of the enemy: allocates the 0x48-byte part object, seeds its
/// coordinate's translation from the sub-state's entry in
/// `D_actor_105300_80133A20`, links it into `Gp_ObjLists[2]`, and raises one of
/// the two per-enemy death flags. A failed allocation tears the enemy down
/// instead and leaves the task on this handler; otherwise the task moves to the
/// tick handler (`state` 1).
void func_actor_105300_80132BAC(GpEnemy* arg0, Task* arg1)
{
    Actor05300Obj2C* obj;
    Actor05300Work*  work;
    Actor05300Part*  part;
    GsCOORDINATE2*   coord;
    GpRec18*         rec18;
    s32              flag;
    u16              type;

    obj   = arg1->extra;
    coord = obj->field_8;
    work  = (Actor05300Work*)arg1->parent->work;
    part  = memCalloc(0x48, 0);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work        = (TaskIdMap*)part;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = D_actor_105300_80133A20[work->field_334].x;
    coord->coord.t[1] = D_actor_105300_80133A20[work->field_334].y;
    coord->coord.t[2] = D_actor_105300_80133A20[work->field_334].z;
    coord->flg        = 0;
    arg0->field_4     = &coord->coord;
    arg0->field_48    = 0;
    Gp_LinkNode(&arg0->node);
    rec18                     = part->rec18;
    arg0->coord               = coord;
    arg0->bodyPos.vx          = 0;
    arg0->bodyPos.vy          = 0;
    arg0->bodyPos.vz          = 0;
    arg0->param               = &D_actor_105300_8013D3A0;
    arg0->recs                = rec18;
    arg0->hp                  = D_actor_105300_8013D3A0.hpMax;
    part->field_38.spawnArgLo = 0x500;
    part->field_38.coord      = coord;
    part->field_38.spawnArgHi = 2;
    part->obj.coord           = coord;
    part->obj.ctx.recs        = rec18;
    part->obj.pos.vx          = 0;
    part->obj.pos.vy          = 0;
    part->obj.pos.vz          = 0;
    part->obj.key             = ((Actor05300Work*)arg1->parent->work)->node0.key;
    part->obj.radius          = 0xC8;
    part->obj.flags           = 1;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(rec18, 1, 0);
    part->obj.flags |= 0x8000;
    type             = (u16)work->field_334;
    part->field_46   = type;
    if ((type << 0x10) == 0) {
        func_8017FD88(1);
        flag = 0x147;
    } else {
        func_8017E524(1);
        flag = 0x148;
    }
    GameFlag_SetNibble(flag, 0);
    arg1->state = 1;
}

void func_actor_105300_80132DAC(GpEnemy* arg0, Task* arg1)
{
    VECTOR*         vec;
    Actor05300Part* part;
    GsCOORDINATE2*  coord;
    s32             damage;
    s32             snd;
    s32             hitTime;

    coord = ((Actor05300Obj2C*)arg1->extra)->field_8;
    part  = (Actor05300Part*)arg1->work;
    switch (D_801153F4) {
        case 1:
            return;
        case 0:
            arg0->node.flags = 8;
            break;
        case 2:
            arg0->node.flags = 1;
            return;
    }
    vec = --*(VECTOR**)0x1F8003FC;
    if (part->field_40 != 0) {
        part->field_40--;
        if (part->field_40 <= 0) {
            part->field_40 = 0;
        }
    }
    if (part->field_44 != 0) {
        part->field_44--;
    }
    if (part->field_40 == 0 && (part->rec18[0].key & 0xFFFF0000) == 0x20000) {
        if (part->rec18[0].key & 0x8000) {
            func_800DA6E8(&arg0->node, 0, 0);
        } else {
            vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->rec18[0].key, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(arg0, part->rec18[0].key, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
            func_800DA6E8(&arg0->node, damage, 0);
            arg0->hp -= damage;
            if (arg0->hp <= 0) {
                arg1->state                                      = 2;
                part->field_42                                   = 0;
                ((Actor05300Work*)arg1->parent->work)->field_336 = 1;
                Gp_SpawnEff(0x6005C, coord, 0x10002400, NULL);
                Gp_SpawnEff(0x60070, coord, 0x32FF1400, NULL);
                snd  = D_actor_105300_8013D3B4;
                snd |= (arg0->placeKey >> 12) << 8;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else if (damage > 0) {
                if (part->field_44 == 0) {
                    if ((Gp_GetIdParam0(part->rec18[0].key) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, &part->field_38);
                    }
                    func_800FDB18(7, coord, NULL, &part->field_38);
                    part->field_44 = 10;
                }
                hitTime = Gp_GetIdParam2(part->rec18[0].key);
                if (hitTime > 0) {
                    part->field_40 = hitTime;
                }
                snd  = D_actor_105300_8013D3B0[0];
                snd |= (arg0->placeKey >> 12) << 8;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->rec18);
    *(VECTOR**)0x1F8003FC += 1;
}

void func_actor_105300_8013310C(GpEnemy* arg0, Task* arg1)
{
    Actor05300Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    GpAreaKey*      sessionKey;
    GpAreaRec*      rec;
    GpAreaPlace*    place;
    TmdObject*      model;
    GpEnemy*        spawned;
    GpAreaKey       key;
    u16             idx;
    s32             sound;
    s32             i;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x340, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_264;
    obj->colorMtx  = &work->field_244;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = coord;
    arg0->bodyPos.vx           = D_actor_105300_80133A30[0].vx;
    arg0->bodyPos.vy           = D_actor_105300_80133A30[0].vy;
    arg0->bodyPos.vz           = D_actor_105300_80133A30[0].vz;
    arg0->param                = &D_actor_105300_8013D390;
    arg0->recs                 = work->rec18;
    arg0->hp                   = D_actor_105300_8013D390.hpMax;
    work->field_2F4.coord      = coord;
    work->field_2F4.spawnArgLo = 0x500;
    work->field_2F4.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_105300_8013D414, obj,
                  work->poses, work->slots);
    for (i = 1; i < 0xA; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_326      = 0x1000;
    work->field_334      = 0;
    work->field_2FC      = coord->coord;
    work->field_338      = 1;
    work->field_33C      = D_actor_105300_8013D394[0];
    work->node0.coord    = coord;
    work->node0.ctx.recs = work->rec18;
    work->node0.pos.vx   = 0;
    work->node0.pos.vy   = 0;
    work->node0.pos.vz   = 0;
    work->node0.key      = 0x30035;
    work->node0.radius   = 0x5DC;
    work->node0.flags    = 1;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->rec18, 2, 0);
    work->node0.flags   |= 0x8000;
    work->node1.coord    = coord;
    work->node1.ctx.recs = work->rec18;
    work->node1.pos.vx   = D_actor_105300_80133A30[0].vx;
    work->node1.pos.vy   = D_actor_105300_80133A30[0].vy;
    work->node1.pos.vz   = D_actor_105300_80133A30[0].vz;
    work->node1.key      = 0x30035;
    work->node1.radius   = 0x12C;
    work->node1.flags    = 1;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags |= 0x8000;
    spawned            = Gp_SpawnEnemyFromTable(D_actor_105300_8013D3FC, 1, 0, arg0);
    model              = spawned->task->extra;
    idx                = arg0->placeKey >> 12;
    sessionKey         = &gGameSession->at4.loc;
    key.stage          = sessionKey->stage;
    key.area           = sessionKey->area;
    key.room           = sessionKey->room;
    key.view           = sessionKey->view;
    Gp_SyncAreaKeyIndex(&key);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    sound           = D_actor_105300_8013D3C0 | ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_0,
                        D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_2);
    arg1->msgTable = D_actor_105300_80133A00;
    arg1->state    = 1;
}
