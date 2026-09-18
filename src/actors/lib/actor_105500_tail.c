#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

extern void* D_80067704[1];
extern u8    Actor05500_D05F18[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);

void Actor05500_Fn03C54(Actor105500* actor)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    GpEffWork* eff;
    TmdObject* model;
    s32        idx;
    u32        raw;

    D_80067704[0] = Actor05500_D05F18;
    eff           = Gp_SpawnEff(0x40007, actor->field_2C->field_8 + 4, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey  = (GpAreaKey*)&gGameSession->field_4;
    raw         = ((Actor105500Ctx*)actor->field_20)->field_8;
    model       = (TmdObject*)eff->field_0->extra;
    key.field_3 = sessionKey->field_3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = sessionKey->field_0;
    idx         = raw >> 12;
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec             = Gp_GetNestedAreaRec(&key);
    entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
}

/// Folds a uniform 1/16 scale into the model's third coordinate node, through a
/// 0x30-byte block borrowed from the scratchpad and released again: an identity
/// rotation is splatted word-wise, `ScaleMatrix` shrinks its diagonal to 0x100,
/// and `MulMatrix` multiplies the result into `field_8[2].coord`. This is the
/// body shared as `ActorsShared80135b58`, which this whole-overlay unit is too
/// coarse to link against.
void Actor05500_Fn03D40(Actor105500* actor)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135b58Scratch* blk;
    GsCOORDINATE2*              coord;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *scratch = blk;
    coord    = actor->field_2C->field_8;

    blk->scale.vx          = 0x100;
    blk->scale.vy          = 0x100;
    blk->scale.vz          = 0x100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->mat.mat);
    *scratch = (u8*)*scratch + 0x30;
}

void Actor05500_Fn03DD8(Actor105500* arg0)
{
    Actor105500StateFuncTable3 sp;

    sp = Actor05500_D0002C;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Actor05500_Fn03E34(GpEnemy* enemy, Task* task)
{
    Task*                           parent;
    TmdObject*                      parentObj;
    GsCOORDINATE2*                  coord;
    ActorsShared80135c4cParentWork* parentWork;
    GsCOORDINATE2*                  parentCoord;
    ActorsShared80135c4cObjWork*    work;
    u16                             pair;

    parent      = task->parent;
    parentObj   = parent->extra;
    coord       = ((TmdObject*)task->extra)->field_8;
    parentWork  = (ActorsShared80135c4cParentWork*)parent->work;
    parentCoord = &parentObj->field_8[4];
    work        = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    coord->sub = &Gfx_ViewCoord;
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->flg         = 0;
    work->field_3A     = 0xC0;
    pair               = parentWork->field_3AC;
    work->obj.field_8  = coord;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.field_C  = &work->rec;
    work->field_3C     = pair;
    work->obj.field_18 = Gp_PackPair(&Actor05500_D08958, 2);
    work->obj.field_1C = 0x100;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}

void Actor05500_Fn03F88(Actor105500* arg0)
{
    Actor105500StateFuncTable3 sp;

    sp = Actor05500_D00038;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}
