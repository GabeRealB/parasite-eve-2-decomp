#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/3A34.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;

extern void       ActorsShared80135ae4AnimData;
extern TaskDesc   ActorsShared80135ae4SpawnTable[];
extern u16*       ActorsShared80135ae4CueTable[];
extern GpPairSrcE ActorsShared80135ae4Params[];

void ActorsShared80135ae4(GpEnemy* ctx, Task* actor)
{
    Actor105700Work* work;
    TmdObject*       obj;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parts;
    GsCOORDINATE2*   partsA;
    GsCOORDINATE2*   partsB;
    GsCOORDINATE2*   partsC;
    GsCOORDINATE2*   partsD;
    GsCOORDINATE2*   effParts;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     entry;
    GpEnemy*         eff;
    GpEnemy*         spawned;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    GpAreaKey        key;
    s32              i;
    s32              one;
    s32              kind;
    u32              idx;
    u8               areaByte3;
    GameSession*     session;
    s32              param;
    u32              lcg;

    obj   = actor->extra;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = (TaskIdMap*)work;
    obj->flags                 = 0;
    coord->flg                 = 0;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x39;
    work->field_66C            = ActorsShared80135ae4SpawnTable;
    work->field_670.coord      = &((TmdObject*)actor->extra)->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->ctx, &ActorsShared80135ae4AnimData, obj, work->field_30C, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->ctx, i, 1);
    }

    spawned    = Gp_SpawnEnemyFromTable(ActorsShared80135ae4SpawnTable, 3, 0, ctx);
    session    = gGameSession;
    spawned    = (GpEnemy*)spawned->task;
    sessionKey = (GpAreaKey*)&session->at4.loc.view;
    idx        = ctx->placeKey;
    areaByte3  = sessionKey->stage;
    model      = ((Task*)spawned)->extra;
    SOFT_USE_REG(spawned);
    key.stage = areaByte3;
    key.area  = sessionKey->area;
    key.room  = sessionKey->room;
    areaByte0 = session->at4.loc.view;
    idx       = idx >> 12;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    keyPtr       = &key;
    rec          = Gp_GetNestedAreaRec(keyPtr);
    idx        <<= 4;
    idx         += (s32)rec->field_0;
    model->tpage = ((GpAreaPlace*)idx)->tpage;
    model->clut  = ((GpAreaPlace*)idx)->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }

    eff     = Gp_SpawnEnemyFromTable(ActorsShared80135ae4SpawnTable, 1, 0, ctx);
    session = gGameSession;
    spawned = (GpEnemy*)eff->task;

    sessionKey = (GpAreaKey*)&session->at4.loc.view;
    idx        = ctx->placeKey;
    areaByte3  = sessionKey->stage;
    model      = ((Task*)spawned)->extra;
    SOFT_USE_REG(spawned);
    key.stage = areaByte3;
    key.area  = sessionKey->area;
    key.room  = sessionKey->room;
    areaByte0 = session->at4.loc.view;
    idx       = idx >> 12;
    SOFT_BARRIER();
    DEF_REG(keyPtr);
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    idx        <<= 4;
    idx         += (s32)rec->field_0;
    model->tpage = ((GpAreaPlace*)idx)->tpage;
    model->clut  = ((GpAreaPlace*)idx)->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }

    one  = 1;
    kind = ctx->spawnState;
    if (kind == one) {
        goto case1;
    }
    if (kind >= 2) {
        goto ge2;
    }
    if (kind == 0) {
        goto case0;
    }
    return;
ge2:
    if (kind == 2) {
        goto case2;
    }
    return;

case0:
    ctx->field_4  = &coord->coord;
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    parts           = ((TmdObject*)actor->extra)->coords;
    ctx->bodyPos.vx = 0;
    ctx->bodyPos.vy = 0;
    ctx->bodyPos.vz = 0;
    ctx->param      = ActorsShared80135ae4Params;
    ctx->recs       = work->field_4EC;
    ctx->coord      = &parts[3];
    ctx->hp         = ActorsShared80135ae4Params->hpMax;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_6AC = ctx->place->mode & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = ctx->place->variant;
        work->field_6DA = param * 1000;
    }

    tbl = ActorsShared80135ae4CueTable[gGameSession->at4.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->at4.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x39;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_6D0            = 0xFA;
    work->field_49C.end0.vz    = 0x1F40;
    work->field_49C.end0Radius = 0x3E8;
    work->field_49C.end1Radius = 0x5DC;
    work->field_49C.end0.vx    = 0;
    work->field_49C.end0.vy    = 0;
    work->field_49C.end1.vx    = 0;
    work->field_49C.end1.vy    = 0;
    work->field_49C.end1.vz    = 0;
    work->field_49C.recs       = work->field_4B4;
    lcg                        = Gp_LcgState * 5 + 0x71357911;
    work->field_6C4            = ((lcg >> 16) & 1) + 1;
    Gp_LcgState                = lcg;
    partsA                     = ((TmdObject*)actor->extra)->coords;
    work->field_47C.ctx.d4rec  = &work->field_49C;
    work->field_47C.pos.vx     = 0;
    work->field_47C.pos.vy     = 0;
    work->field_47C.pos.vz     = 0;
    work->field_47C.key        = 0;
    work->field_47C.radius     = 0;
    work->field_47C.flags      = 3;
    work->field_47C.coord      = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= 0xCC00;

    partsB                   = ((TmdObject*)actor->extra)->coords;
    work->field_4CC.ctx.recs = work->field_4EC;
    work->field_4CC.pos.vx   = 0;
    work->field_4CC.pos.vy   = 0;
    work->field_4CC.pos.vz   = 0;
    work->field_4CC.key      = 0x30039;
    work->field_4CC.radius   = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.coord    = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = ((TmdObject*)actor->extra)->coords;
    work->field_564.pos.vy   = -0x226;
    work->field_564.ctx.recs = work->field_584;
    work->field_564.pos.vx   = 0;
    work->field_564.pos.vz   = 0;
    work->field_564.key      = 0;
    work->field_564.radius   = 0x226;
    work->field_564.flags    = 1;
    work->field_564.coord    = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= 0x4200;

    effParts                 = ((TmdObject*)eff->task->extra)->coords;
    work->field_5E4.ctx.recs = work->field_604;
    work->field_5E4.pos.vx   = 0;
    work->field_5E4.pos.vy   = 0x1F4;
    work->field_5E4.pos.vz   = 0;
    work->field_5E4.key      = 0;
    work->field_5E4.radius   = 0x1F4;
    work->field_5E4.flags    = 1;
    work->field_5E4.coord    = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);

    work->field_63C.end0.vx    = 0;
    work->field_63C.end0.vy    = 0;
    work->field_63C.end0.vz    = 0;
    work->field_63C.end1.vx    = 0;
    work->field_63C.end1.vy    = 0;
    work->field_63C.end1.vz    = 0;
    work->field_63C.end0Radius = 1;
    work->field_63C.end1Radius = 1;
    work->field_63C.recs       = work->field_654;
    work->field_5E4.flags     &= 0x7FFF;
    partsD                     = ((TmdObject*)actor->extra)->coords;
    work->field_61C.ctx.d4rec  = &work->field_63C;
    work->field_61C.pos.vx     = 0;
    work->field_61C.pos.vy     = 0;
    work->field_61C.pos.vz     = 0;
    work->field_61C.key        = 0;
    work->field_61C.radius     = 0;
    work->field_61C.flags      = 3;
    work->field_61C.coord      = partsD;
    Gp_LinkObj(3, &work->field_61C);
    Gp_InitRec18Table(work->field_654, 1, 0);
    work->field_61C.flags = (work->field_61C.flags & 0x3FFF) | 0xC00;
    actor->state          = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->state    = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->state    = kind;
}
