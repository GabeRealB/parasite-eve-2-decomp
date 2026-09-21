#include "common.h"

#include "actors/actor_800300.h"
#include "actors/actors_shared_801625a8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

#include <psyq/abs.h>

extern void         D_actor_800300_80168880;
extern GpActorWork* D_80115764;

extern GpImgRec** D_actor_800300_80168950[];
extern GpImgRec** D_actor_800300_80168960[];

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

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

        temp        = save->characterId;
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

        temp        = save->characterId;
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

void func_actor_800300_80162064(GpActorWork* arg0)
{
    void**                 scratch;
    u8*                    head;
    Actor800300VecScratch* sc;
    GameActor*             actor;
    TmdObject*             obj;
    TmdObject*             extra;
    GsCOORDINATE2*         coord;
    GpObj*                 objs[2];
    s32                    dy;
    s32                    i;
    s8                     bits;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    obj      = arg0->extra;
    *scratch = head - 0x18;
    extra    = obj;
    sc       = (Actor800300VecScratch*)(head - 0x18);
    actor    = arg0->actor;
    coord    = extra->coords;
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
    objs[0] = (GpObj*)actor->field_AC;
    objs[1] = (GpObj*)actor->field_CC;
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
    if (D_80115768 == 0) {
        func_actor_800300_80162C2C(arg0);
    }
    func_actor_800300_801623F8(arg0);
    Gp_ClearRec18Occupied(actor->field_17C);
    if (actor->field_984 & 1) {
        coord->coord.t[1] = actor->field_14 + 0x10;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    if ((s8)actor->field_986 != 0) {
        sc->vec.vx = (u16)actor->field_30.vx;
        sc->vec.vy = (u16)actor->field_30.vy;
        sc->vec.vz = (u16)actor->field_30.vz;
    } else {
        sc->vec.vx = (u16)coord->workm.m[0][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
        sc->vec.vy = (u16)coord->workm.m[1][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
        sc->vec.vz = (u16)coord->workm.m[2][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
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
        if (func_800EA1A8((VECTOR3*)coord->workm.t, (VECTOR3*)sc) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)sc, 0x200, Gp_State1C->groundShade);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

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
