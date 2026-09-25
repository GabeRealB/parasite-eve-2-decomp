#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actors_shared_80135b58.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "gameplay/areaplace.h"
#include "gameplay/gameplay.h"
#include "gameplay/pairsrc.h"
#include "gameplay/3FB8.h"

typedef union Actor00700HitRecord {
    GpRec18 rec;
    struct {
        u32       header;
        GpFixed16 id;
    } hit;
} Actor00700HitRecord;

typedef union Actor00700ContactStorage {
    MATRIX matrix;
    struct {
        byte    pad_0[8];
        GpRec18 recs[3];
    } contacts;
    struct {
        /* 0x00 */ byte   pad_0[0x20];
        /* 0x20 */ MATRIX rotation;
    } quad;
} Actor00700ContactStorage;
STATIC_ASSERT_SIZEOF(Actor00700ContactStorage, 0x50);

typedef struct Actor00700Work {
    /* 0x000 */ byte                     pad_0[0x154];
    /* 0x154 */ Actor00700HitRecord      field_154;
    /* 0x16C */ byte                     pad_16C[0x20];
    /* 0x18C */ GpRec18                  field_18C;
    /* 0x1A4 */ byte                     pad_1A4[0x38];
    /* 0x1DC */ byte                     field_1DC[0x1E];
    /* 0x1FA */ u16                      field_1FA;
    /* 0x1FC */ byte                     field_1FC[0x18];
    /* 0x214 */ byte                     field_214[0x10];
    /* 0x224 */ GpEffArg                 field_224;
    /* 0x22C */ Actor00700ContactStorage field_22C;
    /* 0x27C */ byte                     field_27C[0x20];
    /* 0x29C */ byte                     pad_29C[0x10];
    /* 0x2AC */ s32                      field_2AC;
    /* 0x2B0 */ s32                      field_2B0;
    /* 0x2B4 */ s32                      field_2B4;
    /* 0x2B8 */ byte                     pad_2B8[4];
    /* 0x2BC */ s32                      field_2BC;
    /* 0x2C0 */ s32                      field_2C0;
    /* 0x2C4 */ s32                      field_2C4;
    /* 0x2C8 */ byte                     pad_2C8[0xC];
    /* 0x2D4 */ s16                      field_2D4;
    /* 0x2D6 */ s16                      field_2D6;
    /* 0x2D8 */ s16                      field_2D8;
    /* 0x2DA */ s16                      field_2DA;
    /* 0x2DC */ s16                      field_2DC;
    /* 0x2DE */ s16                      field_2DE;
    /* 0x2E0 */ s16                      field_2E0;
    /* 0x2E2 */ s16                      field_2E2;
    /* 0x2E4 */ s16                      field_2E4;
    /* 0x2E6 */ s16                      field_2E6;
    /* 0x2E8 */ byte                     pad_2E8[0x14];
    /* 0x2FC */ byte                     field_2FC[0x1E];
    /* 0x31A */ u16                      field_31A;
    /* 0x31C */ byte                     pad_31C[0x18];
    /* 0x334 */ GpEffArg                 field_334; // record the hit's effect is spawned with
    /* 0x33C */ GsCOORDINATE2*           field_33C;
    /* 0x340 */ MATRIX                   field_340;
    /* 0x360 */ s32                      field_360;
    /* 0x364 */ s32                      field_364;
    /* 0x368 */ s32                      field_368;
    /* 0x36C */ byte                     pad_36C[4];
    /* 0x370 */ SVECTOR                  field_370;
    /* 0x378 */ s16                      field_378;
    /* 0x37A */ s16                      field_37A;
    /* 0x37C */ s16                      field_37C;
    /* 0x37E */ u16                      field_37E;
    /* 0x380 */ s16                      field_380;
    /* 0x382 */ u16                      field_382;
    /* 0x384 */ s16                      field_384;
    /* 0x386 */ s16                      field_386;
    /* 0x388 */ s16                      field_388;
    /* 0x38A */ u16                      field_38A;
    /* 0x38C */ u16                      field_38C;
    /* 0x38E */ u16                      field_38E;
    /* 0x390 */ s16                      field_390;
    /* 0x392 */ u16                      field_392;
    /* 0x394 */ s16                      field_394;
    /* 0x396 */ s16                      field_396;
    /* 0x398 */ s16                      field_398;
} Actor00700Work;

/// 0x18-byte frame this overlay allocates on the scratchpad stack; only the
/// `SVECTOR` at +0x10 is used by `Actor00700_Fn012E4`; the vector holds
/// the player displacement in `Actor00700_Fn02820`.
typedef struct Actor00700RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor00700RotScratch;
STATIC_ASSERT_SIZEOF(Actor00700RotScratch, 0x18);

/// Four rotated corners and the projected center/depth on the scratchpad.
typedef struct Actor00700QuadScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     sxy;
    /* 0x24 */ s32     otz;
} Actor00700QuadScratch;
STATIC_ASSERT_SIZEOF(Actor00700QuadScratch, 0x28);

typedef struct Actor00700TexEntry {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 pad_1;
    /* 0x2 */ u8 v;
    /* 0x3 */ u8 pad_3;
} Actor00700TexEntry;
STATIC_ASSERT_SIZEOF(Actor00700TexEntry, 4);

/// The 0x2F4-byte allocation used by Actor00700_Fn01FE0.
typedef struct Actor00700SpawnWork {
    /* 0x000 */ u8      field_0[0x14];
    /* 0x014 */ u8      field_14[0xA0];
    /* 0x0B4 */ u8      field_B4[0x40];
    /* 0x0F4 */ MATRIX  field_F4;
    /* 0x114 */ MATRIX  field_114;
    /* 0x134 */ u8      field_134[8];
    /* 0x13C */ void*   field_13C;
    /* 0x140 */ void*   field_140;
    /* 0x144 */ u16     field_144;
    /* 0x146 */ u16     field_146;
    /* 0x148 */ u16     field_148;
    /* 0x14A */ u8      pad_14A[0x2];
    /* 0x14C */ s32     field_14C;
    /* 0x150 */ u16     field_150;
    /* 0x152 */ u16     field_152;
    /* 0x154 */ GpRec18 field_154;
    /* 0x16C */ u8      field_16C[8];
    /* 0x174 */ void*   field_174;
    /* 0x178 */ void*   field_178;
    /* 0x17C */ u16     field_17C;
    /* 0x17E */ u16     field_17E;
    /* 0x180 */ u16     field_180;
    /* 0x182 */ u8      pad_182[0x2];
    /* 0x184 */ s32     field_184;
    /* 0x188 */ u16     field_188;
    /* 0x18A */ u16     field_18A;
    /* 0x18C */ GpRec18 field_18C[4];
    /* 0x1EC */ u8      field_1EC[8];
    /* 0x1F4 */ void*   field_1F4;
    /* 0x1F8 */ void*   field_1F8;
    /* 0x1FC */ u16     field_1FC;
    /* 0x1FE */ u16     field_1FE;
    /* 0x200 */ u16     field_200;
    /* 0x202 */ u8      pad_202[0x2];
    /* 0x204 */ s32     field_204;
    /* 0x208 */ u16     field_208;
    /* 0x20A */ u16     field_20A;
    /* 0x20C */ GpRec18 field_20C;
    /* 0x224 */ void*   field_224;
    /* 0x228 */ u16     field_228;
    /* 0x22A */ u16     field_22A;
    /* 0x22C */ u8      pad_22C[0x80];
    /* 0x2AC */ s32     field_2AC;
    /* 0x2B0 */ s32     field_2B0;
    /* 0x2B4 */ s32     field_2B4;
    /* 0x2B8 */ u8      pad_2B8[0x1E];
    /* 0x2D6 */ u16     field_2D6;
    /* 0x2D8 */ u8      pad_2D8[0x4];
    /* 0x2DC */ u16     field_2DC;
    /* 0x2DE */ u8      pad_2DE[0x16];
} Actor00700SpawnWork;
STATIC_ASSERT_SIZEOF(Actor00700SpawnWork, 0x2F4);

/// The 0x39C-byte block `Actor00700_Fn00060` allocates. Larger than
/// `Actor00700SpawnWork` and laid out differently: the pose buffer
/// `func_800B3F84` fills sits at +0x12C instead of +0xB4, and the four
/// `GpObj` render nodes it links (`Gp_LinkObj` shapes 3/2/2/3, each with its
/// own `GpRec18` table) start at +0x1DC rather than +0x134.
typedef struct Actor00700InitWork {
    /* 0x000 */ byte           pad_0[0x14];
    /* 0x014 */ byte           field_14[0x118];
    /* 0x12C */ byte           field_12C[0x70];
    /* 0x19C */ MATRIX         field_19C;
    /* 0x1BC */ MATRIX         field_1BC;
    /* 0x1DC */ GpObj          obj1;
    /* 0x1FC */ GpRec18        rec1;
    /* 0x214 */ GpObj          obj2;
    /* 0x234 */ GpRec18        rec2;
    /* 0x24C */ byte           pad_24C[0x30];
    /* 0x27C */ GpObj          obj3;
    /* 0x29C */ GpRec18        rec3;
    /* 0x2B4 */ byte           pad_2B4[0x48];
    /* 0x2FC */ GpObj          obj4;
    /* 0x31C */ GpRec18        rec4;
    /* 0x334 */ GsCOORDINATE2* field_334;
    /* 0x338 */ u16            field_338;
    /* 0x33A */ u16            field_33A;
    /* 0x33C */ byte           pad_33C[0x42];
    /* 0x37E */ u16            field_37E;
    /* 0x380 */ s16            field_380;
    /* 0x382 */ byte           pad_382[0x1A];
} Actor00700InitWork;
STATIC_ASSERT_SIZEOF(Actor00700InitWork, 0x39C);

#define SCRATCH_SP (*(u32*)0x1F8003FC)

void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern MATRIX* D_80073B8C;
extern u8      D_801153F4;
extern s8      D_80115408;

extern Actor00700TexEntry Actor00700_D075BC[];

extern s16 Actor00700_D06DF0[];
extern u16 Actor00700_D06E00[];
extern s16 Actor00700_D06E20[];
extern u16 Actor00700_D06E30[];
extern s16 Actor00700_D06E50[];
extern s16 Actor00700_D06E98[];

/// Per-`field_F` drift speed for the state-1 wander in `Actor00700_Fn02A28`,
/// summed with a 5-bit `Gp_LcgState` draw.
extern s16 Actor00700_D07598[];

/// The records `Actor00700_Fn00060` binds the first body to: the pair it packs
/// into the fourth collision node's key, the context's parameter source (whose
/// `hpMax` seeds the health), and the second argument of `func_800B3F84`.
extern struct GpU16Pair Actor00700_D06DDC;
extern GpPairSrcE       Actor00700_D06DE0;
extern u32              Actor00700_D06E6C;

/// The same three for the second body, used by `Actor00700_Fn01FE0`.
extern GpPairSrcE       Actor00700_D07588;
extern struct GpU16Pair Actor00700_D07584;
extern u32              Actor00700_D075B4;

void Actor00700_Fn00060(GpEnemy* ctx, Task* actor);
void Actor00700_Fn01434(GpEnemy* arg0, Task* arg1);
void Actor00700_Fn0188C(GpEnemy* arg0, Task* arg1);
void Actor00700_Fn01988(Task* arg0, TmdObject* arg1, s32 arg2);
void Actor00700_Fn01AB8(Task* arg0);
void Actor00700_Fn01B50(Task* arg0);
void Actor00700_Fn01C10(Task* arg0);
void Actor00700_Fn01CF0(Task* arg0);
void Actor00700_Fn01D80(Task* arg0);
void Actor00700_Fn01E44(Task* arg0);
void Actor00700_Fn01E9C(Task* arg0);
void Actor00700_Fn01EEC(Task* arg0);
void Actor00700_Fn01FE0(GpEnemy* ctx, Task* actor);
void Actor00700_Fn02290(GpEnemy* arg0, Task* arg1);
void Actor00700_Fn02414(Task* arg0);
void Actor00700_Fn0268C(Task* arg0);
void Actor00700_Fn02820(Task* arg0);
void Actor00700_Fn02A28(Task* arg0);
void Actor00700_Fn02D28(GpEnemy* arg0, Task* arg1);
void Actor00700_Fn0305C(Task* arg0);
void Actor00700_Fn03518(Task* arg0);
void Actor00700_Fn03570(Task* arg0);

/// The state handlers `Actor00700_Fn01830` dispatches on `Task::state`:
/// set-up, per-frame update, and the one entered once the health runs out.
const GpEnemyTaskFuncTable3 Actor00700_D00004 = {
    { Actor00700_Fn00060, Actor00700_Fn0188C, Actor00700_Fn01434 },
};

/// The first body's set-up handler: allocate the work block, rebind the
/// model's light and colour matrices into it, then link the four collision
/// nodes and their tables. `&obj->coords[4]` -- the model's fifth
/// coordinate -- is what both the context and the second node hang off.
/// A failed allocation tears the enemy down and leaves the task here.
void Actor00700_Fn00060(GpEnemy* ctx, Task* actor)
{
    Actor00700InitWork* work;
    TmdObject*          obj;
    GsCOORDINATE2*      coord;
    s32                 i;

    obj   = actor->extra;
    coord = obj->coords;
    work  = memCalloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work   = work;
    obj->flags    = 0;
    coord->flg    = 0;
    obj->lightMtx = &work->field_1BC;
    obj->colorMtx = &work->field_19C;
    ctx->field_4  = (void*)(&coord->coord);
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord      = &((TmdObject*)actor->extra)->coords[4];
    ctx->node.flags = 0;
    ctx->bodyPos.vx = 0;
    ctx->bodyPos.vy = 0;
    ctx->bodyPos.vz = 0;
    ctx->param      = &Actor00700_D06DE0;
    ctx->recs       = &work->rec2;
    ctx->hp         = (u16)Actor00700_D06DE0.hpMax;
    work->field_338 = 0x100;
    work->field_33A = 1;
    work->field_334 = coord;
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
    work->obj2.coord    = &((TmdObject*)actor->extra)->coords[4];
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
    actor->state     = 1;
}

void Actor00700_Fn00334(Task* actor)
{
    GpEnemy*        ctx;
    u32             lastId;
    Actor00700Work* work;
    Actor00700Work* contactWork;
    GpDeltaScratch* allocated;
    GpDeltaScratch* oldScratch;
    GpDeltaScratch* scratch;
    GpDeltaScratch* normal;
    GsCOORDINATE2*  coord;
    s16             cooldown;
    s16             health;
    s32             cooldownParam;
    s32             wallDx;
    s32             wallDy;
    s32             wallDz;
    s32             result;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             depth;
    s32             push;
    s32             z;
    s32             boundedDepth;
    s8*             effectRec;
    s8*             contactRec;
    u32             id;
    u32             kind;
    u32             hitId;
    u32             effect;
    u32             damage;
    GsCOORDINATE2*  sourceCoord;

    push       = 0;
    lastId     = 0;
    oldScratch = *(GpDeltaScratch**)0x1F8003FC;
    work       = actor->work;
    allocated  = oldScratch - 3;
    SOFT_TOUCH_REG(allocated);
    scratch                       = allocated;
    *(GpDeltaScratch**)0x1F8003FC = scratch;
    coord                         = ((TmdObject*)actor->extra)->coords;
    ctx                           = actor->spawnArg2;
    result                        = func_800E0C10((GpRec18*)&work->field_27C[0x20], scratch, 4, NULL);
    USE_REG(oldScratch);
    if (result == 1)
        goto move_delta;
    if (result < 2)
        goto move_done;
    if (result == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += oldScratch[-3].vx.h.hi;
    coord->coord.t[1] += scratch->vy.h.hi;
    z                  = coord->coord.t[2] + scratch->vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_360;
    coord->coord.t[1] = work->field_364;
    z                 = work->field_368;
move_z:
    coord->coord.t[2] = z;
move_done:
    Gp_ClearRec18Occupied(&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        cooldown        = (u16)work->field_378 - 1;
        work->field_378 = cooldown;
        normal          = scratch + 1;
        if ((cooldown << 0x10) <= 0) {
            work->field_378 = 0;
            goto cooldown_done;
        }
    } else {
    cooldown_done:
        normal = scratch + 1;
    }
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG2(scratch, scratch);
    contactWork = work;
contact_loop: {
    USE_REG2(contactWork, contactWork);
    id   = contactWork->field_22C.contacts.recs[0].key;
    kind = id >> 0x10;
    if (kind == 1)
        goto physical_contact;
    if (kind == 0)
        goto next_contact;
    if (kind == 2)
        goto damage_contact;
    if (kind == 3)
        goto physical_contact;
    contactWork = (Actor00700Work*)((u8*)contactWork + 0x18);
    goto contact_test;
damage_contact:
    if (work->field_378 == 0) {
        sourceCoord   = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        dx            = sourceCoord->coord.t[0] - coord->coord.t[0];
        scratch->vx.w = dx;
        dy            = sourceCoord->coord.t[1] - coord->coord.t[1];
        scratch->vy.w = dy;
        dz            = sourceCoord->coord.t[2] - coord->coord.t[2];
        scratch->vz.w = dz;
        damage        = Gp_ComputeDamage(contactWork->field_22C.contacts.recs[0].key, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
        USE_REG(damage);
        if (Gp_RollEnemyChance(actor->spawnArg2, contactWork->field_22C.contacts.recs[0].key, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(0x6009C, ((TmdObject*)actor->extra)->coords, 0, NULL);
        }
        func_800DA6E8(&((GpEnemy*)actor->spawnArg2)->node, (s32)damage, 0);
        func_800E2C78(actor->spawnArg2, (s32)contactWork->field_22C.contacts.recs[0].key, (s32)damage, 0);
        health  = (u16)ctx->hp - damage;
        ctx->hp = health;
        if ((health << 0x10) <= 0) {
            work->field_37A = 5;
            work->field_37C = 0;
            actor->state    = (s32)kind;
        } else if (work->field_398 == 0) {
            work->field_37A = 4;
            work->field_37C = 0;
        }
        work->field_31A &= 0x7FFF;
        effect           = Gp_GetIdParam0(contactWork->field_22C.contacts.recs[0].key) & 0xFFFF;
        switch (effect) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                break;
            case 2:
                Gp_SetObjFlag2(actor->spawnArg2, contactWork->field_22C.contacts.recs[0].key, 0);
                break;
            case 3:
                Gp_SetObjFlag4(actor->spawnArg2, contactWork->field_22C.contacts.recs[0].key, 0);
                break;
            case 1:
            case 9:
                Gp_SetObjFlag1(actor->spawnArg2);
                break;
        }
        hitId = contactWork->field_22C.contacts.recs[0].key;
        if (lastId != hitId) {
            lastId = hitId;
            func_800FDB18(Gp_GetIdParam1((s32)hitId) & 0xFFFF, coord, NULL, &work->field_334);
        }
        cooldownParam = Gp_GetIdParam2(contactWork->field_22C.contacts.recs[0].key);
        if (cooldownParam > 0) {
            work->field_378 = cooldownParam;
        }
    }
    goto next_contact;
physical_contact:
    wallDx        = coord->workm.t[0] - contactWork->field_22C.contacts.recs[0].point.vx;
    scratch->vx.w = wallDx;
    wallDy        = coord->workm.t[1] - contactWork->field_22C.contacts.recs[0].point.vy;
    scratch->vy.w = wallDy;
    wallDz        = coord->workm.t[2] - contactWork->field_22C.contacts.recs[0].point.vz;
    scratch->vz.w = wallDz;
    depth         = contactWork->field_22C.contacts.recs[0].depth - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
    boundedDepth  = depth;
    if (depth <= 0) {
        boundedDepth = 0;
    }
    SOFT_TOUCH_REG_USE(boundedDepth, depth);
    depth = boundedDepth;
    if (push < depth) {
        push = depth;
        VectorNormal((VECTOR*)scratch, (VECTOR*)normal);
        ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)normal, (VECTOR*)(scratch + 2));
    }
next_contact:
    contactWork = (Actor00700Work*)((u8*)contactWork + 0x18);
}
contact_test:
    if ((s32)contactWork < (s32)&work->pad_0[0x48])
        goto contact_loop;
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * scratch[2].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(push * scratch[2].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied((s8*)work->field_22C.contacts.recs);
    effectRec = work->pad_31C;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_31A &= 0x7FFF;
        Gp_ClearRec18Occupied(effectRec);
    }
    contactRec = work->field_1FC;
    if (Gp_CountRec18Hi(contactRec, 0x10000) != 0) {
        sourceCoord      = ((TmdObject*)Gp_ActorSlots[(u8)work->field_1FC[4] >> 7]->extra)->coords;
        work->field_394  = 1;
        work->field_1FA &= 0x7FFF;
        work->field_33C  = sourceCoord;
    }
    Gp_ClearRec18Occupied(contactRec);
    *(GpDeltaScratch**)0x1F8003FC += 3;
}

void Actor00700_Fn008B4(Task* arg0)
{
    Actor00700Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    s32             state;
    s32             one;
    s32             rng0;
    s32             rng1;
    s32             rng2;
    s32             rng3;
    s32             rng4;
    s32             rng5;
    s32             rng6;
    s32             timer;
    s32             next;
    s32             flags;
    s32             ang;
    s32             snd;
    s32             pan;

    one   = 1;
    work  = arg0->work;
    obj   = arg0->extra;
    state = work->field_37C;
    coord = obj->coords;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto tail;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto tail;
case0:
    flags           = work->field_1FA;
    work->field_384 = 0;
    work->field_1FA = flags | 0x8000;
    timer           = work->field_38C + 1;
    work->field_38C = timer;
    if ((s16)timer < 0x1E) {
        goto tail;
    }
    rng0        = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState = rng0;
    if ((s32)(((u32)rng0 >> 16) & 0xF) <
        Actor00700_D06DF0[((GpEnemy*)arg0->spawnArg2)->place->rowIndex]) {
        work->field_37E = 7;
        next            = Actor00700_D06E00[((u32)(rng1 = rng0 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng1;
        work->field_37C = one;
        work->field_38C = next;
        goto tail;
    }
    rng2        = rng0 * 5 + 0x71357911;
    Gp_LcgState = rng2;
    if ((s32)(((u32)rng2 >> 16) & 0xF) <
        Actor00700_D06E20[((GpEnemy*)arg0->spawnArg2)->place->rowIndex]) {
        work->field_37E = 2;
        next            = Actor00700_D06E30[((u32)(rng3 = rng2 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng3;
        work->field_37C = 2;
        work->field_38C = next;
        goto tail;
    }
    work->field_38C = 0;
    goto tail;
case1:
    work->field_384 = 0x14;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
    goto tail;
case2:
    work->field_384 = 0x32;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
tail:
    work->field_38E = work->field_38E - 1;
    if ((s16)work->field_38E > 0) {
        goto post;
    }
    work->field_386 = 0x19;
    rng4            = Gp_LcgState * 5 + 0x71357911;
    rng5            = rng4 * 5 + 0x71357911;
    ang             = ((u32)rng5 >> 16) & 0x3FF;
    Gp_LcgState     = rng4;
    work->field_38E = ((u32)rng4 >> 16) & 0x1F;
    Gp_LcgState     = rng5;
    if ((((u32)rng5 >> 16) & 0x400) == 0) {
        ang = -ang;
    }
    work->field_38A = ((u16)work->field_388 + ang) & 0xFFF;
post:
    if (work->field_394 != 0) {
        work->field_37A = 1;
        work->field_394 = 0;
        work->field_37C = 0;
        work->field_37E = 2;
        rng6            = Gp_LcgState * 5 + 0x71357911;
        work->field_38C = (((u32)rng6 >> 16) & 0x1F) + 0x3C;
        snd             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40070003;
        Gp_LcgState     = rng6;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    }
    Actor00700_Fn01B50(arg0);
}

void Actor00700_Fn00BC0(Task* arg0)
{
    VECTOR*         vec;
    Actor00700Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  target;
    s32             state;
    s32             one;
    s32             dist;
    s32             raw;
    s16             diff;
    s32             adiff;
    s32             ang;
    s32             vel;
    s32             pan;
    s32             snd;

    one   = 1;
    vec   = (VECTOR*)(SCRATCH_SP -= 0x10);
    work  = arg0->work;
    obj   = arg0->extra;
    state = work->field_37C;
    coord = obj->coords;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    Gp_ArmStateF0(1);
    if (work->field_33C == 0) {
        work->field_33C = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
    }
    target          = work->field_33C;
    vec->vx         = target->coord.t[0] - coord->coord.t[0];
    vec->vy         = 0;
    vec->vz         = target->coord.t[2] - coord->coord.t[2];
    work->field_38A = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
    work->field_386 = 0x19;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto dist;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
dist:
    dist = SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz);
    if (dist < 0x2BC) {
        raw   = work->field_38A - (u16)work->field_388;
        diff  = raw;
        adiff = diff >= 0 ? diff : -diff;
        if (adiff < 0x800) {
            ang = adiff;
            goto wrap_done;
        }
        if (diff > 0) {
            ang = 0x1000 - raw;
            goto wrap_done;
        }
        ang = raw + 0x1000;
    wrap_done:
        if ((s16)ang < 0x32) {
            work->field_37E = 4;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_37C = 1;
            goto pop;
        }
        work->field_384 = 0;
        goto pop;
    }
    work->field_384 = 0x32;
    goto pop;
case1:
    if ((s16)work->field_382 == 0x14) {
        work->field_31A |= 0x8000;
    }
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37E  = 3;
    work->field_37C  = 2;
    work->field_31A &= 0x7FFF;
    goto pop;
case2:
    vel = 0;
    if ((s16)work->field_382 < 0xB) {
        vel = -0x78;
    }
    work->field_384 = vel;
    if ((s16)work->field_382 < 0x1F) {
        goto pop;
    }
    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40070004;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((s32)(((u32)Gp_LcgState >> 16) & 0xF) < Actor00700_D06E50[((GpEnemy*)arg0->spawnArg2)->place->rowIndex]) {
        work->field_37C = 0;
        work->field_37E = state;
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_38E = 0;
    work->field_394 = 0;
pop:
    SCRATCH_SP += 0x10;
}

void Actor00700_Fn00F20(Task* arg0)
{
    VECTOR          vec;
    Actor00700Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    s32             state;
    s32             one;
    s32             rng;
    s32             posX;

    one   = 1;
    work  = arg0->work;
    obj   = arg0->extra;
    state = work->field_37C;
    coord = obj->coords;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    work->field_37E = 0xA;
    work->field_380 = one;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_396 = one;
    work->field_37C = one;
    rng             = Gp_LcgState * 5 + 0x71357911;
    work->field_38C = (((u32)rng >> 16) & 0x1F) + 0xF;
    Gp_LcgState     = rng;
    posX            = coord->coord.t[0];
    vec.vx          = D_80073B8C->t[0] - posX;
    vec.vy          = D_80073B8C->t[1] - coord->coord.t[1];
    vec.vz          = D_80073B8C->t[2] - coord->coord.t[2];
    VectorNormalS(&vec, &work->field_370);
    goto pop;
case1:
    if ((s16)work->field_382 >= 0xF) {
        goto tick;
    }
    coord->coord.t[0] += -(work->field_370.vx * 50) >> 12;
    coord->coord.t[2] += -(work->field_370.vz * 50) >> 12;
tick:
    if ((u32)(work->field_382 - 6) < 9) {
        work->field_386 = 0x93;
        work->field_38A = (work->field_38A + 0x5C7) & 0xFFF;
    } else {
        work->field_386 = 0;
    }
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto pop;
    }
    if ((((GpEnemy*)arg0->spawnArg2)->reactionFlags & 2) != 0) {
        work->field_37E = 8;
        work->field_37A = 3;
        work->field_37C = 3;
        goto pop;
    }
    work->field_37E = 9;
    work->field_37C = 2;
    goto pop;
case2:
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_394 = one;
    work->field_396 = 0;
pop:;
}

void Actor00700_Fn01148(Task* arg0)
{
    GpEnemy*        ctx;
    Actor00700Work* work;
    s16             state;
    s32             rng;
    s32             rng2;
    u16             timer;

    work  = arg0->work;
    state = work->field_37C;
    switch (state) {
        case 0:
            work->field_384 = 0;
            work->field_386 = 0;
            if (work->field_396 == 0) {
                work->field_37C = 1;
                work->field_37E = 6;
            } else {
                work->field_37C = 2;
                rng             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rng;
                work->field_38C = ((u32)rng >> 0x10) & 0xF;
            }
            work->field_396 = 1;
            work->field_380 = 1;
            return;
        case 1:
            if ((s16)work->field_382 >= 0x1D) {
                work->field_37C = 2;
                rng2            = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rng2;
                work->field_38C = ((u32)rng2 >> 0x10) & 0xF;
                return;
            }
            return;
        case 2:
            timer           = work->field_38C - 1;
            work->field_38C = timer;
            if ((timer << 0x10) <= 0) {
                work->field_37E = 8;
                work->field_37C = 3;
                return;
            }
            break;
        case 3:
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                ctx                 = arg0->spawnArg2;
                ctx->reactionFlags &= 0xFD;
                work->field_37A     = 0;
                work->field_37C     = 0;
                work->field_37E     = 1;
                work->field_38C     = 0;
                work->field_394     = 1;
                work->field_396     = 0;
                work->field_398     = 0;
            }
            break;
    }
}

void Actor00700_Fn012E4(Task* arg0)
{
    Actor00700Work*       work;
    GsCOORDINATE2*        coord;
    Actor00700RotScratch* sc;
    s32                   ang;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s32                   step;
    s32                   cur;
    s32                   next;
    s32                   wrapStep;

    sc    = (Actor00700RotScratch*)(SCRATCH_SP -= 0x18);
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_38A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_388 = ang;
    if (adiff < 0x800) {
        step = work->field_386;
        if (step >= adiff) {
            work->field_388 = want;
        } else {
            next = work->field_388;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_388 = next;
        }
    } else {
        step = work->field_386;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_388 = work->field_38A;
        goto done;
    turn:
        wrapStep = work->field_386;
        cur      = work->field_388;
        if (diff > 0) {
            work->field_388 = cur - wrapStep;
        } else {
            work->field_388 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_388;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void Actor00700_Fn01434(GpEnemy* arg0, Task* arg1)
{
    Actor00700Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    Actor00700Work* work2;
    GsCOORDINATE2*  c;
    VECTOR          vec;
    s32             state;
    s32             i;
    s16             st;
    s16             phase;
    s16             val;
    s32             snd;
    s32             pan;

    obj   = arg1->extra;
    work  = arg1->work;
    state = D_801153F4;
    coord = obj->coords;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
    return;
case2:
    obj->flags = 0x80;
    return;
default_body:
    st = work->field_37C;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    return;
death:
    work->field_37E = 6;
    work->field_38C = 0;
    work->field_390 = 0x1000;
    work->field_340 = coord->coord;
    arg0->recs      = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_214);
    Gp_UnlinkObj(&work->field_27C);
    Gp_UnlinkObj(&work->field_2FC);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 7);
    work->field_37C = 1;
    work2           = arg1->work;
    i               = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
    c      = ((TmdObject*)arg1->extra)->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
    snd = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x40070005;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    return;
dying:
    Actor00700_Fn01EEC(arg1);
    phase           = work->field_38C + 1;
    work->field_38C = phase;
    if (phase == 10) {
        obj->flags = 2;
    }
    if ((s16)work->field_38C == 15) {
        Gp_SpawnEff(0x600A5, coord, 1, NULL);
    }
    if ((s16)work->field_38C >= 0x3C) {
        work->field_37C = 2;
    }
    work2 = arg1->work;
    i     = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
    c      = ((TmdObject*)arg1->extra)->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
}

void Actor00700_Fn01830(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor00700_D00004;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2), arg0);
}

void Actor00700_Fn0188C(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*  coord;
    TmdObject*      obj;
    Actor00700Work* work;
    s32             state;
    s32             one;

    obj   = arg1->extra;
    state = D_801153F4;
    work  = arg1->work;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags       = 0;
    arg0->node.flags = 0;
    goto default_body;
case2:
    obj->flags       = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor00700_Fn01988(arg1, obj, one);
    }
    Actor00700_Fn00334(arg1);
    Actor00700_Fn01AB8(arg1);
    SOFT_USE_REG(work);
    if (work->field_386 != 0) {
        Actor00700_Fn012E4(arg1);
    }
    Actor00700_Fn01CF0(arg1);
    Actor00700_Fn01D80(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor00700_Fn01E44(arg1);
    Actor00700_Fn01E9C(arg1);
}

void Actor00700_Fn01988(Task* arg0, TmdObject* arg1, s32 arg2)
{
    GpEnemy*        ctx;
    Actor00700Work* work;
    s32             damage;
    u16             remaining;
    u8              flags;

    ctx   = arg0->spawnArg2;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & 1) {
        ctx->reactionFlags = flags & 0xFE;
        work->field_37A    = 2;
        work->field_37C    = 0;
    }
    if ((ctx->reactionFlags & 2) && ((u32)((u16)work->field_37A - 2) >= 2U)) {
        work->field_37A = 3;
        work->field_37C = 0;
        work->field_398 = 1;
    }
    if (ctx->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->field_37A = 5;
                work->field_37C = 0;
                arg0->state     = 2;
            } else {
                work->field_37A = 4;
                work->field_37C = 0;
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= 0xF3;
        }
    }
}

void Actor00700_Fn01AB8(Task* arg0)
{
    switch (((Actor00700Work*)arg0->work)->field_37A) {
        case 0:
            Actor00700_Fn008B4(arg0);
            break;
        case 1:
            Actor00700_Fn00BC0(arg0);
            break;
        case 2:
            Actor00700_Fn00F20(arg0);
            break;
        case 3:
            Actor00700_Fn01148(arg0);
            break;
        case 4:
            Actor00700_Fn01C10(arg0);
            break;
        case 5:
            break;
    }
}

void Actor00700_Fn01B50(Task* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    s32             snd;
    s32             pan;
    u16             timer;
    u32             random;

    work            = arg0->work;
    coord           = ((TmdObject*)arg0->extra)->coords;
    timer           = work->field_392 - 1;
    work->field_392 = timer;
    if ((s16)timer <= 0) {
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_392 = (u16)(((random >> 0x10) & 0x7F) + 0x96);
        Gp_LcgState     = (s32)random;
        snd             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40070001;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, (s32)pan, (s8)gpGetObjDepth(coord));
    }
}

void Actor00700_Fn01C10(Task* arg0)
{
    Actor00700Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    s32             state;
    s32             snd;
    s32             pan;

    work  = arg0->work;
    obj   = arg0->extra;
    state = work->field_37C;
    coord = obj->coords;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    return;
case0:
    work->field_37E = 5;
    work->field_380 = 1;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_37C = 1;
    snd             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40070002;
    pan             = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    return;
case1:
    if ((s16)work->field_382 < 0x18) {
        return;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = state;
    work->field_38C = 0;
    work->field_394 = state;
}

/// Records the model's current root position in the work block, then displaces
/// the root coordinate by the work's step along the rotation's third column
/// (X and Z only) and by 0x80 on Y.
void Actor00700_Fn01CF0(Task* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = arg0->work;
    work->field_360    = coord->coord.t[0];
    work->field_364    = coord->coord.t[1];
    work->field_368    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_384) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_384) >> 0xC;
}

void Actor00700_Fn01D80(Task* arg0)
{
    Actor00700Work* work2;
    s32             i;
    s32             val;

    work2 = arg0->work;
    i     = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
}

void Actor00700_Fn01E44(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

void Actor00700_Fn01E9C(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

void Actor00700_Fn01EEC(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor00700Work*             work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = ((TmdObject*)arg0->extra)->coords;
    if (work->field_390 >= 0x201) {
        work->field_390 = (u16)work->field_390 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_390;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_340;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// The state handlers `Actor00700_Fn034BC` dispatches on `Task::state`,
/// for the second body this package carries: set-up, per-frame update, and the
/// one a resolved hit switches it to.
const GpEnemyTaskFuncTable3 Actor00700_D00054 = {
    { Actor00700_Fn01FE0, Actor00700_Fn02290, Actor00700_Fn02D28 },
};

void Actor00700_Fn01FE0(GpEnemy* ctx, Task* actor)
{
    GsCOORDINATE2*       coord;
    TmdObject*           obj;
    s32                  i;
    void*                rec1;
    void*                rec2;
    void*                rec3;
    Actor00700SpawnWork* work;

    obj   = actor->extra;
    coord = obj->coords;
    work  = memCalloc(0x2F4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work = work;
    obj->flags  = 0;
    coord->flg  = 0;
    obj->tpage += 1;
    obj->clut  += 1;
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    obj->lightMtx = &work->field_114;
    obj->colorMtx = &work->field_F4;
    ctx->field_4  = (void*)(&coord->coord);
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord      = coord;
    ctx->node.flags = 0;
    ctx->bodyPos.vx = 0;
    ctx->bodyPos.vy = 0;
    ctx->bodyPos.vz = 0;
    ctx->param      = &Actor00700_D07588;
    ctx->recs       = &work->field_154;
    ctx->hp         = (u16)Actor00700_D07588.hpMax;
    work->field_228 = 0x100;
    work->field_22A = 1;
    work->field_224 = coord;
    func_800B3F84(work, &Actor00700_D075B4, obj, &work->field_B4, &work->field_14);
    for (i = 1; i < 4; i++) {
        Gp_AnimResetSlot(work, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_2D6 = 1;
    work->field_2AC = (s32)coord->coord.t[0];
    work->field_2B0 = (s32)coord->coord.t[1];
    work->field_2B4 = (s32)coord->coord.t[2];
    work->field_2DC = (u16)((GpEnemy*)actor->spawnArg2)->place->yaw;
    rec1            = &work->field_154;
    work->field_13C = coord;
    work->field_140 = rec1;
    work->field_144 = 0;
    work->field_146 = 0;
    work->field_148 = 0;
    work->field_14C = 0x30008;
    work->field_150 = 0xFA;
    work->field_152 = 1U;
    Gp_LinkObj(2, &work->field_134);
    Gp_InitRec18Table(rec1, 1, 0);
    rec2            = &work->field_18C;
    work->field_174 = coord;
    work->field_178 = rec2;
    work->field_17C = 0;
    work->field_17E = 0;
    work->field_180 = 0;
    work->field_184 = 0x30008;
    work->field_188 = 0xFA;
    work->field_18A = 1U;
    work->field_152 = (u16)(work->field_152 | 0x8000);
    Gp_LinkObj(2, &work->field_16C);
    Gp_InitRec18Table(rec2, 4, 0);
    rec3            = &work->field_20C;
    work->field_1F4 = coord;
    work->field_1F8 = rec3;
    work->field_1FC = 0;
    work->field_1FE = 0;
    work->field_200 = 0;
    work->field_18A = (u16)(work->field_18A | 0x4000);
    work->field_204 = Gp_PackPair(&Actor00700_D07584, 0);
    work->field_208 = 0x190;
    work->field_20A = 1U;
    Gp_LinkObj(3, &work->field_1EC);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_20A = (u16)(work->field_20A & 0x7FFF);
    actor->state    = 1;
}

void Actor00700_Fn02290(GpEnemy* arg0, Task* arg1)
{
    TmdObject*      obj;
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    s32             state;
    s32             one;

    work  = arg1->work;
    obj   = arg1->extra;
    state = D_801153F4;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags       = 0;
    arg0->node.flags = 0;
    goto default_body;
case1:
    Actor00700_Fn03518(arg1);
    return;
case2:
    obj->flags       = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    Actor00700_Fn02414(arg1);
    Actor00700_Fn0268C(arg1);
    if (work->field_2E6 == 0 && D_80115408 != 0) {
        work->field_2E6 = 1;
        Gp_ArmStateF0(1);
    }
    Actor00700_Fn02820(arg1);
    Actor00700_Fn02A28(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Actor00700_Fn03518(arg1);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->placeKey >> 12) << 8) | 0x40070008;
        temp = (s8)Gp_GetObjPan(((TmdObject*)arg1->extra)->coords);
        SndEvt_EnqueueType6(id, temp, (s8)gpGetObjDepth(((TmdObject*)arg1->extra)->coords));
    }
}

void Actor00700_Fn02414(Task* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    s32             movement;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             amount;
    s32             damage;
    s32             z;
    u16             state;
    GsCOORDINATE2*  target;
    GpDeltaScratch* head;
    GpDeltaScratch* delta;

    work     = arg0->work;
    head     = *(void**)0x1F8003FC;
    delta    = (*(void**)0x1F8003FC = head - 1);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_18C, delta, 4, 0);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].vx.h.hi;
            coord->coord.t[1] += delta->vy.h.hi;
            z                  = coord->coord.t[2] + delta->vz.h.hi;
            coord->coord.t[2]  = z;
            break;
        case 2:
            coord->coord.t[0] = work->field_2BC;
            coord->coord.t[1] = work->field_2C0;
            coord->coord.t[2] = work->field_2C4;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_18C);
    state = (u16)work->field_154.hit.id.h.hi;
    switch ((u32)state) {
        case 0:
            break;
        case 1:
            arg0->state                     = 2;
            ((GpEnemy*)arg0->spawnArg2)->hp = 0;
            Gp_ArmStateF0(1);
            break;
        case 2:
            arg0->state = (s32)state;
            target      = ((TmdObject*)Gp_ActorSlots[(u8)work->field_154.hit.id.h.lo >> 7]->extra)->coords;
            dx          = target->coord.t[0] - coord->coord.t[0];
            delta->vx.w = dx;
            dy          = target->coord.t[1] - coord->coord.t[1];
            delta->vy.w = dy;
            dz          = target->coord.t[2] - coord->coord.t[2];
            delta->vz.w = dz;
            damage      = Gp_ComputeDamage((s32)work->field_154.hit.id.w, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            amount      = damage;
            if (damage == 0) {
                damage = 1;
                amount = 1;
            }
            func_800DA6E8(&((GpEnemy*)arg0->spawnArg2)->node, amount, 0);
            func_800E2C78(arg0->spawnArg2, (s32)work->field_154.hit.id.w, damage, 0);
            ((GpEnemy*)arg0->spawnArg2)->hp = 0;
            func_800FDB18(Gp_GetIdParam1((s32)work->field_154.hit.id.w) & 0xFFFF, ((TmdObject*)arg0->extra)->coords, 0, &work->field_224);
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154.rec);
    SCRATCH_SP += 0x10;
}

void Actor00700_Fn0268C(Task* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  coord2;
    SVECTOR*        sc;
    s32             direction;
    s32             direction2;
    s32             product;
    sc   = (SVECTOR*)(SCRATCH_SP -= 8);
    work = arg0->work;
    if (++work->field_2E0 >= 16) {
        work->field_2E0 = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_2D4 = !(((u32)Gp_LcgState >> 16) & 1);
    }
    switch (work->field_2D4) {
        case 0:
            work->field_2D8 += 0x100;
            if (work->field_2D8 >= 0x200) {
                work->field_2D8 = -0x100;
                direction       = work->field_2D6;
                work->field_2D6 = -direction;
            }
            break;
        case 1:
            work->field_2D8 = 0x100;
            direction2      = work->field_2D6;
            work->field_2D6 = -direction2;
            break;
    }
    sc->vx = 0;
    sc->vy = 0;
    sc->vz = work->field_2D8 * work->field_2D6;
    coord  = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(sc, &coord[2].coord);
    coord[2].flg = 0;
    sc->vx       = 0;
    sc->vy       = 0;
    product      = work->field_2D8 * work->field_2D6;
    sc->vz       = -product;
    coord2       = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(sc, &coord2[3].coord);
    coord2[3].flg = 0;
    SCRATCH_SP   += 8;
}

void Actor00700_Fn02820(Task* arg0)
{
    Actor00700Work*       work;
    GsCOORDINATE2*        coord;
    Actor00700RotScratch* sc;
    s32                   random;
    s32                   amount;
    s32                   cur;
    s32                   cur2;
    s32                   cur3;
    s32                   random2;
    s32                   amount2;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s16                   turn;
    s16                   wrap;

    sc    = (Actor00700RotScratch*)(SCRATCH_SP -= 0x18);
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_2E6) {
        case 0:
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            random          = (u32)Gp_LcgState >> 16;
            amount          = random & 0x1F;
            cur             = work->field_2DC;
            work->field_2DC = !(random & 0x20) ? cur - amount : cur + amount;
            break;
        case 1:
            sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->vec.vy = 0;
            sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
            diff       = want - (work->field_2DC & 0xFFF);
            adiff      = diff >= 0 ? diff : -diff;
            turn       = diff;
            if (adiff < 0x11) {
                work->field_2DC = want;
            } else {
                if (adiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0)
                        wrap = 0x1000 - diff;
                    turn = wrap;
                }
                cur2 = work->field_2DC;
                if (turn > 0) {
                    work->field_2DC = cur2 + 0x10;
                } else {
                    work->field_2DC = cur2 - 0x10;
                }
            }
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    random2         = (u32)Gp_LcgState >> 16;
    amount2         = random2 & 0x3F;
    cur3            = work->field_2DA;
    work->field_2DA = !(random2 & 0x40) ? cur3 - amount2 : cur3 + amount2;
    if (work->field_2DA > 0x100) {
        work->field_2DA = 0x100;
    } else if (work->field_2DA < -0x100) {
        work->field_2DA = -0x100;
    }
    sc->rot.vx = work->field_2DA;
    sc->rot.vy = work->field_2DC;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void Actor00700_Fn02A28(Task* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    u32             random;
    u32             random2;
    u32             random3;
    s32             amount;
    s32             amountB;
    s16             delta;
    s16             speed;
    s32             y;
    s32             newY;
    s16             base;

    work            = arg0->work;
    coord           = ((TmdObject*)arg0->extra)->coords;
    work->field_2BC = coord->coord.t[0];
    work->field_2C0 = coord->coord.t[1];
    work->field_2C4 = coord->coord.t[2];
    switch (work->field_2E6) {
        case 0:
            random = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount = random & 0x1F;
            if (!(random & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)(coord->coord.t[0] + (s16)delta) < work->field_2AC + 200 &&
                work->field_2AC - 200 < (s16)(coord->coord.t[0] + (s16)delta)) {
                coord->coord.t[0] += (s16)delta;
            } else {
                coord->coord.t[0] -= (s16)delta;
            }
            amountB = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F;
            if (work->field_2D4 != 0) {
                amountB = -amountB;
            }
            delta = amountB;
            if ((s16)(coord->coord.t[1] + (s16)delta) < work->field_2B0 + 500 &&
                work->field_2B0 - 500 < (s16)(coord->coord.t[1] + (s16)delta)) {
                coord->coord.t[1] += (s16)delta;
            } else {
                coord->coord.t[1] -= (s16)delta;
            }
            random3 = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount  = random3 & 0x1F;
            if (!(random3 & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)random3 < work->field_2B4 + 200 && work->field_2B4 - 200 < (s16)random3) {
                coord->coord.t[2] += (s16)delta;
            } else {
                coord->coord.t[2] -= (s16)delta;
            }
            break;
        case 1:
            speed = Actor00700_D07598[((GpEnemy*)arg0->spawnArg2)->place->rowIndex] +
                    (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F);
            coord->coord.t[0] += (coord->coord.m[0][2] * speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * speed) >> 12;
            base               = D_80073B8C->t[1] - 0x4B0;
            random2            = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            y                  = coord->coord.t[1];
            if (y >= base + 400) {
                coord->coord.t[1] = y - (random2 & 0xF);
            } else {
                if (base - 400 >= y) {
                    newY = y + (random2 & 0xF);
                } else {
                    amountB = random2 & 0x1F;
                    if (work->field_2D4 != 0) {
                        newY = y - amountB;
                    } else {
                        newY = y + amountB;
                    }
                }
                coord->coord.t[1] = newY;
            }
            break;
    }
}

void Actor00700_Fn02D28(GpEnemy* arg0, Task* arg1)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    SVECTOR*        head;
    SVECTOR*        rot;
    s32             angle;
    u32             rnd;
    u32             seed;
    s32             id;
    s32             pan;

    coord = ((TmdObject*)arg1->extra)->coords;
    work  = arg1->work;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            break;
        case 0:
        default:
            head                   = *(SVECTOR**)0x1F8003FC;
            rot                    = head - 1;
            *(SVECTOR**)0x1F8003FC = rot;
            switch (work->field_2DE) {
                case 0:
                    D_80115408                       = 1;
                    seed                             = Gp_LcgState * 5 + 0x71357911;
                    rnd                              = seed >> 16;
                    angle                            = rnd & 0xFF;
                    ((TmdObject*)arg1->extra)->flags = 2;
                    Gp_LcgState                      = seed;
                    work->field_2E2                  = 0x1000;
                    work->field_22C.matrix           = coord->coord;
                    if (!(rnd & 0x100)) {
                        angle = -angle;
                    }
                    work->field_2E4                         = angle;
                    arg0->recs                              = 0;
                    ((Actor00700SpawnWork*)work)->field_152 = ((Actor00700SpawnWork*)work)->field_152 & 0x7FFF;
                    ((Actor00700SpawnWork*)work)->field_18A = ((Actor00700SpawnWork*)work)->field_18A & 0xBFFF;
                    ((Actor00700SpawnWork*)work)->field_20A = ((Actor00700SpawnWork*)work)->field_20A | 0x8000;
                    id                                      = ((arg0->placeKey >> 12) << 8) | 0x40070006;
                    pan                                     = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(coord));
                    Gp_UnlinkNode(&arg0->node);
                    Gp_ReleaseStateF0Add(arg1, 8);
                    work->field_2E0 = 1;
                    work->field_2DE = 1;
                    break;
                case 1:
                    Actor00700_Fn03570(arg1);
                    work->field_2DA = (work->field_2DA + work->field_2E4) & 0xFFF;
                    work->field_2DC = (work->field_2DC + work->field_2E4) & 0xFFF;
                    rot->vx         = work->field_2DA;
                    rot->vy         = work->field_2DC;
                    rot->vz         = 0;
                    RotMatrix(rot, &coord->coord);
                    work->field_22C.matrix.t[1] += 0x18;
                    if ((s16)(work->field_2E0 / 3) < 8) {
                        Actor00700_Fn0305C(arg1);
                    } else {
                        ((TmdObject*)arg1->extra)->flags = 0x80;
                    }
                    work->field_2E0++;
                    if (work->field_2E0 >= 0x1E) {
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_134);
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_16C);
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_1EC);
                        work->field_2DE = 2;
                    }
                    break;
                case 2:
                    work->field_2E0--;
                    if (work->field_2E0 <= 0) {
                        Gp_DestroyEnemy(arg0, arg1);
                    }
                    break;
            }
            *(SVECTOR**)0x1F8003FC += 1;
            break;
    }
}

void Actor00700_Fn0305C(Task* arg0)
{
    Actor00700QuadScratch* sc;
    Actor00700Work*        work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    s32                    size, x, y;
    s16                    i;
    SVECTOR*               v;
    POLY_FT4*              prim;
    Actor00700TexEntry*    uv;
    obj         = arg0->extra;
    sc          = (Actor00700QuadScratch*)(SCRATCH_SP -= 0x28);
    coord       = obj->coords;
    work        = arg0->work;
    sc->v[0].vx = coord->workm.t[0];
    sc->v[0].vy = coord->workm.t[1];
    sc->v[0].vz = coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->v[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy);
    gte_stszotz(&sc->otz);
    if (sc->otz < 20) {
        SCRATCH_SP += 0x28;
        return;
    }
    if (work->field_2E0 == 1) {
        sc->v[0].vx = 0;
        sc->v[0].vy = 0;
        sc->v[0].vz = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
        RotMatrix(&sc->v[0], &work->field_22C.quad.rotation);
    }
    size        = 0x7800 / sc->otz;
    x           = sc->sxy & 0xFFFF;
    y           = sc->sxy >> 16;
    sc->v[0].vx = -size;
    sc->v[0].vy = -size;
    sc->v[0].vz = 0;
    sc->v[1].vx = size;
    sc->v[1].vy = -size;
    sc->v[1].vz = 0;
    sc->v[2].vx = -size;
    sc->v[2].vy = size;
    sc->v[2].vz = 0;
    sc->v[3].vx = size;
    sc->v[3].vy = size;
    sc->v[3].vz = 0;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&work->field_22C.quad.rotation);
        v = &sc->v[i];
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += x;
        v->vy += y;
    }
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2E);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 1);
    prim->tpage = (((obj->tpage * 64 + 0x180) & 0x3FF) >> 6) | 0xD0;
    prim->clut  = (obj->clut << 6) + 0x3D40;
    uv          = &Actor00700_D075BC[(s16)(work->field_2E0 / 3)];
    prim->u0    = uv->u;
    prim->v0    = uv->v;
    prim->u1    = uv->u + 31;
    prim->v1    = uv->v;
    prim->u2    = uv->u;
    prim->v2    = uv->v + 31;
    prim->u3    = uv->u + 31;
    prim->v3    = uv->v + 31;
    prim->x0    = sc->v[0].vx;
    prim->y0    = sc->v[0].vy;
    prim->x1    = sc->v[1].vx;
    prim->y1    = sc->v[1].vy;
    prim->x2    = sc->v[2].vx;
    prim->y2    = sc->v[2].vy;
    prim->x3    = sc->v[3].vx;
    prim->y3    = sc->v[3].vy;
    addPrim((u_long*)(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), prim);
    SCRATCH_SP += 0x28;
}

void Actor00700_Fn034BC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor00700_D00054;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2), arg0);
}

void Actor00700_Fn03518(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

void Actor00700_Fn03570(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor00700Work*             work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = ((TmdObject*)arg0->extra)->coords;
    if (work->field_2E2 >= 0x201) {
        work->field_2E2 = (u16)work->field_2E2 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2E2;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_22C.matrix;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    *(u8**)0x1F8003FC += 0x30;
}
