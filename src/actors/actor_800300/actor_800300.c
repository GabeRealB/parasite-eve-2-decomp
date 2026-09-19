#include "common.h"

#include "actors/actors_shared_801625a8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

extern void         D_actor_800300_80168880;
extern GpActorWork* D_80115764;

extern GpImgRec** D_actor_800300_80168950[];
extern GpImgRec** D_actor_800300_80168960[];

void func_actor_800300_80161E80(GpActorWork* arg0)
{
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  next;
    GsCOORDINATE2** addr;
    GpActorD4*      d4;
    GpObj*          obj;
    GpRec18*        recs;
    McSaveData*     save;
    s32             packed;
    s8              fcc;

    actor = arg0->actor;
    extra = arg0->extra;
    d4    = actor->field_910;
    addr  = &extra->coords;
    coord = *addr;
    arg0->state++;
    arg0->field_24   = &D_actor_800300_80168880;
    arg0->field_18   = &ActorsShared801625a8;
    actor->field_938 = 0x13;
    D_80115764       = arg0;
    coord->sub       = &gGfxViewCoord;
    coord->flg       = 0;
    extra->flags     = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    recs            = actor->field_17C;
    obj             = (GpObj*)actor->field_AC;
    actor->field_10 = coord->coord.t[0];
    actor->field_14 = coord->coord.t[1];
    actor->field_18 = coord->coord.t[2];
    obj->ctx.dir    = (GpObjDirRec*)actor->field_88;
    obj->coord      = coord;
    actor->field_90 = (s32)recs;
    save            = &Mc_SaveData;
    obj->pos.vx     = 0;
    obj->pos.vy     = -0x12C;
    obj->pos.vz     = 0;
    {
        s32 temp;

        temp        = save->field_22;
        obj->radius = 0x12C;
        obj->flags  = 4;
        packed      = 0x10000;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table((GpRec18*)actor->field_90, 0x12, 0);
    obj->flags     |= 0xC200;
    obj             = (GpObj*)actor->field_CC;
    next            = arg0->extra->coords;
    obj->ctx.dir    = (GpObjDirRec*)actor->field_94;
    obj->coord      = next + 4;
    actor->field_9C = (s32)recs;
    obj->pos.vx     = 0;
    obj->pos.vy     = 0;
    obj->pos.vz     = 0;
    {
        s32 temp;

        temp        = save->field_22;
        obj->radius = 0xC8;
        obj->flags  = 4;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    obj->flags      |= 0xC000;
    actor->field_984 = 7;
    func_8010BF7C(arg0, 0x3C, 0x7F);
    fcc             = -0x6A;
    d4->repeatCount = fcc;
}

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300", func_actor_800300_80162064);

void func_actor_800300_801623F8(GpActorWork* arg0)
{
    void**      scratch;
    u8*         head;
    s32         temp;
    RECT*       rect;
    GameActor*  actor;
    GpImgRec*** table;
    s32         idx;
    u32         row;
    GpImgRec*   img;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    temp     = (s32)(head - 8);
    *scratch = (void*)temp;
    rect     = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_actor_800300_80168950;
            idx   = (s8)actor->field_987 - 1;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x40;
                rect->w             = 0x19;
                rect->h             = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_actor_800300_80168960;
            idx   = (row = (s8)actor->field_98A - 1);
            img   = table[row][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 0xC;
                rect->y = 0x60;
                rect->w = 0xE;
                rect->h = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_actor_800300_8016259C(Task* arg0)
{
    arg0->state = 3;
}
