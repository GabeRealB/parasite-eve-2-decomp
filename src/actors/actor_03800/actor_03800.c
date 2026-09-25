#include "common.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

typedef struct Actor103800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[6];
    /* 0x104 */ byte       field_104[0x60];
    /* 0x164 */ MATRIX     field_164;
    /* 0x184 */ MATRIX     field_184;
    /* 0x1A4 */ byte       field_1A4[8];
    /* 0x1AC */ void*      field_1AC;
    /* 0x1B0 */ GpRec18*   field_1B0;
    /* 0x1B4 */ u16        field_1B4;
    /* 0x1B6 */ s16        field_1B6;
    /* 0x1B8 */ u16        field_1B8;
    /* 0x1BA */ byte       pad_1BA[2];
    /* 0x1BC */ u32        field_1BC;
    /* 0x1C0 */ u16        field_1C0;
    /* 0x1C2 */ u16        field_1C2;
    /* 0x1C4 */ GpRec18    field_1C4[3];
    /* 0x20C */ byte       field_20C[8];
    /* 0x214 */ void*      field_214;
    /* 0x218 */ GpRec18*   field_218;
    /* 0x21C */ u16        field_21C;
    /* 0x21E */ s16        field_21E;
    /* 0x220 */ u16        field_220;
    /* 0x222 */ byte       pad_222[2];
    /* 0x224 */ u32        field_224;
    /* 0x228 */ s16        field_228;
    /* 0x22A */ u16        field_22A;
    /* 0x22C */ GpRec18    field_22C[4];
    /* 0x28C */ byte       field_28C[8];
    /* 0x294 */ void*      field_294;
    /* 0x298 */ GpRec18*   field_298;
    /* 0x29C */ u16        field_29C;
    /* 0x29E */ s16        field_29E;
    /* 0x2A0 */ u16        field_2A0;
    /* 0x2A2 */ byte       pad_2A2[2];
    /* 0x2A4 */ u32        field_2A4;
    /* 0x2A8 */ u16        field_2A8;
    /* 0x2AA */ u16        field_2AA;
    /* 0x2AC */ GpRec18    field_2AC[1];
    /* 0x2C4 */ GpEffArg   field_2C4; // record the death effect is spawned with
    /* 0x2CC */ MATRIX     field_2CC;
    /* 0x2EC */ s16        field_2EC;
    /* 0x2EE */ s16        field_2EE;
    /* 0x2F0 */ s16        field_2F0;
    /* 0x2F2 */ byte       pad_2F2[2];
    /// Coordinate node `Actor03800_Fn003B8` publishes on `field_344` for the
    /// detached modes (spawn kinds 1 and 2): it is seeded from the model's own
    /// `TmdObject::coords`, parented to `gGfxViewCoord` and then turned
    /// by 0x400 / 0x800 about X. `coord.coord.t` is the saved world translation
    /// the idle and detach ticks restore after rebuilding the rotation.
    /* 0x2F4 */ GsCOORDINATE2  coord;
    /* 0x344 */ GsCOORDINATE2* field_344;
    /* 0x348 */ u16            field_348;
    /* 0x34A */ s16            field_34A;
    /* 0x34C */ u16            field_34C;
    /* 0x34E */ s16            field_34E;
    /* 0x350 */ s16            field_350;
    /* 0x352 */ s16            field_352;
    /* 0x354 */ s16            field_354;
    /* 0x356 */ s16            field_356;
    /* 0x358 */ s16            field_358;
    /* 0x35A */ s16            field_35A;
    /* 0x35C */ s16            field_35C;
    /* 0x35E */ s16            field_35E;
    /* 0x360 */ s16            field_360;
    /* 0x362 */ s16            field_362;
    /* 0x364 */ s16            field_364;
    /* 0x366 */ s16            field_366;
    /* 0x368 */ s16            field_368;
    /* 0x36A */ s16            field_36A;
    /* 0x36C */ s16            field_36C;
    /* 0x36E */ s16            field_36E;
    /* 0x370 */ s16            field_370;
    /* 0x372 */ s16            field_372;
    /* 0x374 */ s16            field_374;
    /* 0x376 */ byte           pad_376[2];
    /* 0x378 */ s16            field_378;
    /* 0x37A */ s16            field_37A;
    /* 0x37C */ s16            field_37C;
    /* 0x37E */ s16            field_37E;
} Actor103800Work;
STATIC_ASSERT_SIZEOF(Actor103800Work, 0x380);

typedef struct Actor03800TurnScratch {
    /* 0x00 */ SVECTOR rotation;
    /* 0x08 */ MATRIX  matrix;
} Actor03800TurnScratch;
STATIC_ASSERT_SIZEOF(Actor03800TurnScratch, 0x28);

typedef struct Actor03800MoveScratch {
    VECTOR  delta;
    SVECTOR normal;
} Actor03800MoveScratch;
STATIC_ASSERT_SIZEOF(Actor03800MoveScratch, 0x18);

extern void* D_80067704[1];
extern u8    Actor03800_D0459C[];
extern u8    Actor03800_D046A0[];
extern u8    Actor03800_D047A4[];
extern u8    Actor03800_D04868[];
extern u8    Actor03800_D0492C[];

extern s16        Actor03800_D05F90[];
extern s16        Actor03800_D05FA8[];
extern u16        Actor03800_D05F40;
extern u16        Actor03800_D05F42;
extern GpPairSrcE Actor03800_D05F44;
extern u16        Actor03800_D05F48;
extern u8         Actor03800_D05F60[];

s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

void Actor03800_Fn000B8(GpEnemy* arg0, Task* arg1);
void Actor03800_Fn003B8(Task* arg0);
void Actor03800_Fn00974(Task* arg0);
void Actor03800_Fn00A98(Task* arg0);
void Actor03800_Fn026F8(Task* arg0);
void Actor03800_Fn02848(Task* arg0);
void Actor03800_Fn02998(GpEnemy* arg0, Task* arg1);
void Actor03800_Fn02E50(Task* arg0);
void Actor03800_Fn03008(Task* actor, u32 variant);
void Actor03800_Fn031B8(GpEnemy* arg0, Task* arg1);
void Actor03800_Fn032D8(Task* arg0);
void Actor03800_Fn03420(Task* arg0);
void Actor03800_Fn034B0(Task* arg0);
void Actor03800_Fn03594(Task* arg0);
void Actor03800_Fn03628(Task* arg0);
void Actor03800_Fn036EC(Task* arg0);
void Actor03800_Fn03744(Task* arg0);
void Actor03800_Fn037E0(Task* arg0);

/// State handlers `Actor03800_Fn0315C` dispatches, indexed by the task's state
/// (`Task::state`): the spawn state that allocates the work block and
/// moves to state 1, the per-frame tick, and the state-2 handler the tick hands
/// over to, which carries the death sequence.
const GpEnemyTaskFuncTable3 Actor03800_D00004 = {
    {
        Actor03800_Fn000B8,
        Actor03800_Fn031B8,
        Actor03800_Fn02998,
    },
};

void Actor03800_Fn000B8(GpEnemy* arg0, Task* arg1)
{
    GpObj*           obj;
    GpRec18*         records1;
    GpRec18*         records2;
    GpRec18*         records3;
    Actor103800Work* work;
    s32              i;
    TmdObject*       extra;

    extra = arg1->extra;
    work  = memCalloc(0x384, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    extra->lightMtx = &work->field_184;
    extra->flags    = 0;
    extra->colorMtx = &work->field_164;
    ((void (*)())Gp_IncStateF0Ref)(0);
    Actor03800_Fn003B8(arg1);
    arg0->field_4  = &work->field_344->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &((TmdObject*)arg1->extra)->coords[3];
    arg0->node.state.b.flags   = 0;
    arg0->bodyPos.vx           = 0;
    arg0->recs                 = work->field_1C4;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &Actor03800_D05F44;
    arg0->hp                   = (s16)Actor03800_D05F44.hpMax;
    work->field_2C4.coord      = &((TmdObject*)arg1->extra)->coords[3];
    work->field_2C4.spawnArgLo = 0x200;
    work->field_2C4.spawnArgHi = 1;
    func_800B3F84(&work->anim, Actor03800_D05F60, (TmdObject*)extra, work->field_104, work->slots);
    for (i = 1; i < 6; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    work->field_1AC = ((TmdObject*)arg1->extra)->coords;
    records1        = work->field_1C4;
    work->field_1B6 = -0xFA;
    work->field_1B0 = records1;
    work->field_1B4 = 0;
    work->field_1B8 = 0;
    work->field_1BC = 0x30026;
    work->field_1C0 = 0xFA;
    work->field_1C2 = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_1A4);
    Gp_InitRec18Table(records1, 3, 0);
    work->field_214 = ((TmdObject*)arg1->extra)->coords;
    records2        = work->field_22C;
    work->field_21E = -0x12C;
    work->field_218 = records2;
    work->field_21C = 0;
    work->field_220 = 0;
    work->field_224 = 0x30026;
    work->field_228 = 0x12C;
    work->field_22A = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_20C);
    Gp_InitRec18Table(records2, 4, 0);
    switch (work->field_350) {
        case 0:
            work->field_1C2 |= 0x8000;
            work->field_22A |= 0x4200;
            break;
        case 1:
            work->field_1C2 |= 0x8000;
            work->field_22A &= ~0x4200;
            break;
        case 2:
            work->field_1C2 |= 0x8000;
            work->field_22A &= ~0x4200;
            break;
        case 3:
            work->field_1C2 &= ~0x8000;
            work->field_22A &= ~0x4200;
            break;
    }
    obj             = (GpObj*)work->field_28C;
    work->field_294 = ((TmdObject*)arg1->extra)->coords;
    records3        = work->field_2AC;
    work->field_29E = -0xFA;
    work->field_2A0 = 0xFA;
    work->field_2A8 = 0xC8;
    work->field_298 = records3;
    work->field_29C = 0;
    work->field_2A4 = 0;
    work->field_2AA = 1U;
    Gp_LinkObj(3, obj);
    Gp_InitRec18Table(records3, 1, 0);
    work->field_2AA = (u16)(work->field_2AA | 0x8000);
    arg1->state     = 1;
}

/// Applies the spawn variant (`GpAreaPlace::mode`) to the freshly allocated
/// work block: the tens digit picks the mode (`field_350`) and the units digit
/// of mode 0 the idle pose, seeding the look-around countdown from the LCG.
/// Modes 1 and 2 instead detach the model: the work block's own coordinate is
/// seeded from the model's, parented to `gGfxViewCoord` and published on
/// `field_344`, while the model's coordinate is reset to an identity rotation
/// at the origin and re-parented under it. `Gp_MulMatrix0`-style GTE column
/// products then turn the detached coordinate by 0x400 / 0x800 about X.
void Actor03800_Fn003B8(Task* arg0)
{
    Actor103800Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   src;
    OverlayMat*      mtx;
    OverlayMat*      srcmtx;
    OverlayMat*      mtx2;
    OverlayMat*      srcmtx2;
    SVECTOR          rot;
    MATRIX           mat;
    s16              mode;
    s16              kind;

    ctx  = (GpEnemy*)arg0->spawnArg2;
    work = (Actor103800Work*)arg0->work;
    src  = ((TmdObject*)arg0->extra)->coords;
    mode = ctx->place->mode / 10;

    work->field_350 = mode;
    switch (mode) {
        case 0:
            kind            = ctx->place->mode % 10;
            work->field_352 = kind;
            switch (kind) {
                case 0:
                    work->field_348 = 1;
                    work->field_37A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 0x10) & 0xFF) + 0x5A;
                    break;
                case 1:
                    work->field_348 = 2;
                    work->field_356 = 0;
                    work->field_37A = 1;
                    break;
            }
            work->field_366 = 0x80;
            work->field_372 = 0x80;
            work->field_344 = ((TmdObject*)arg0->extra)->coords;
            break;
        case 1:
            work->field_352 = 8;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx                = (OverlayMat*)&work->coord.coord;
            mtx->ident.m00_m01 = 0x1000;
            mtx->ident.m02_m10 = 0;
            mtx->ident.m11_m12 = 0x1000;
            mtx->ident.m20_m21 = 0;
            mtx->ident.m22     = 0x1000;

            work->coord.sub        = &gGfxViewCoord;
            work->coord.coord      = src->coord;
            work->coord.coord.t[0] = src->coord.t[0];
            work->coord.coord.t[1] = src->coord.t[1];
            work->coord.coord.t[2] = src->coord.t[2];

            srcmtx                = (OverlayMat*)&src->coord;
            srcmtx->ident.m00_m01 = 0x1000;
            srcmtx->ident.m02_m10 = 0;
            srcmtx->ident.m11_m12 = 0x1000;
            srcmtx->ident.m20_m21 = 0;
            srcmtx->ident.m22     = 0x1000;

            src->sub        = &work->coord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x400;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->coord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 2:
            work->field_352 = 9;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx2                = (OverlayMat*)&work->coord.coord;
            mtx2->ident.m00_m01 = 0x1000;
            mtx2->ident.m02_m10 = 0;
            mtx2->ident.m11_m12 = 0x1000;
            mtx2->ident.m20_m21 = 0;
            mtx2->ident.m22     = 0x1000;

            work->coord.sub        = &gGfxViewCoord;
            work->coord.coord      = src->coord;
            work->coord.coord.t[0] = src->coord.t[0];
            work->coord.coord.t[1] = src->coord.t[1];
            work->coord.coord.t[2] = src->coord.t[2];

            srcmtx2                = (OverlayMat*)&src->coord;
            srcmtx2->ident.m00_m01 = 0x1000;
            srcmtx2->ident.m02_m10 = 0;
            srcmtx2->ident.m11_m12 = 0x1000;
            srcmtx2->ident.m20_m21 = 0;
            srcmtx2->ident.m22     = 0x1000;

            src->sub        = &work->coord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x800;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->coord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 3:
            work->field_352 = 0xB;
            work->field_366 = 0;
            work->field_372 = -1;
            work->field_344 = ((TmdObject*)arg0->extra)->coords;
            break;
    }
}

void Actor03800_Fn00974(Task* arg0)
{
    GpEnemy*         ctx;
    Actor103800Work* work;
    s16              damage;
    u16              remaining;
    u8               flags;

    ctx   = arg0->spawnArg2;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & 2) {
        if (work->field_350 == 0) {
            ctx->reactionFlags = flags & 0xFD;
            work->field_352    = 6;
            work->field_354    = 0;
            work->field_356    = 0;
            work->field_37E    = 1;
        } else if ((work->field_352 != 0xA) || (work->field_354 >= 4)) {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
    if (ctx->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, (s32)damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->field_352 = 7;
            } else {
                work->field_352 = 5;
            }
            work->field_354 = 0;
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= 0xF3;
        }
    }
}

void Actor03800_Fn00A98(Task* arg0)
{
    s32              one;
    u32              lastId;
    GpEnemy*         ctx;
    Actor103800Work* work;
    Actor103800Work* contactWork;
    Actor103800Work* wallWork;
    GpDeltaScratch*  scratch;
    GpDeltaScratch*  normal;
    GsCOORDINATE2*   coord;
    s16              health;
    s16              state;
    s32              id;

    s32            dx2;
    s32            dy2;
    s32            dz2;
    s32            result;
    s32            dx;
    s32            dy;
    s32            dz;
    s32            hitId;
    s32            depth;
    s32            i;
    s32            reaction;
    s32            push;
    s32            boundedDepth;
    u32            kind;
    u32            effect;
    u32            damage;
    GsCOORDINATE2* sourceCoord;

    push     = 0;
    reaction = 0;
    lastId   = 0;
    work     = arg0->work;
    scratch  = (SCRATCH_HEAD(GpDeltaScratch) -= 3);
    coord    = work->field_344;
    ctx      = arg0->spawnArg2;
    result   = func_800E0C10(work->field_22C, scratch, 4, NULL);
    i        = 0;
    if (result != 0) {
        for (i = 0; i < 4; i++) {
            wallWork = (Actor103800Work*)((u8*)work + i * 0x18);
            if ((wallWork->field_22C[0].key & 0xFFFF0000) == 0x100000) {
                if (wallWork->field_22C[0].at10.normal.vy >= -0xDDA) {
                    if (work->field_36E == 0) {
                        work->field_370 = 1;
                        break;
                    }
                } else {
                    work->field_374 = 1;
                }
            }
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += scratch->vx.h.hi;
                coord->coord.t[1] += scratch->vy.h.hi;
                coord->coord.t[2] += scratch->vz.h.hi;
                break;
            case 2:
                coord->coord.t[0] = work->field_2EC;
                coord->coord.t[1] = work->field_2EE;
                coord->coord.t[2] = work->field_2F0;
                break;
            default:
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_22C);
    USE_REG4(scratch, scratch, scratch, scratch);
    if (work->field_34E != 0) {
        if (--work->field_34E <= 0) {
            work->field_34E = 0;
            goto cooldown_done;
        }
        normal = scratch + 1;
    } else {
    cooldown_done:
        normal = scratch + 1;
    }
    one         = 1;
    contactWork = work;
contact_loop: {
    id   = contactWork->field_1C4[0].key;
    kind = (u32)id >> 0x10;
    if (kind == one)
        goto physical_contact;
    if (kind == 0)
        goto next_contact;
    if (kind == 2)
        goto damage_contact;
    if (kind == 3)
        goto physical_contact;
    contactWork = (Actor103800Work*)((u8*)contactWork + 0x18);
    goto contact_test;
damage_contact:
    if (work->field_34E == 0) {
        sourceCoord   = ((TmdObject*)Gp_ActorSlots[((u32)id >> 7) & 1]->extra)->coords;
        dx            = sourceCoord->coord.t[0] - coord->coord.t[0];
        scratch->vx.w = dx;
        dy            = sourceCoord->coord.t[1] - coord->coord.t[1];
        scratch->vy.w = dy;
        dz            = sourceCoord->coord.t[2] - coord->coord.t[2];
        scratch->vz.w = dz;
        damage        = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
        damage        = Gp_ComputeDamage(contactWork->field_1C4[0].key, damage, 0, 0);
        if (work->field_36E == 0) {
            if (Gp_RollEnemyChance((GpEnemy*)ctx, (u32)contactWork->field_1C4[0].key, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
        } else if (!(contactWork->field_1C4[0].key & 0x8000) && (damage != 0)) {
            damage *= 3;
            Gp_SpawnEff(0x6009C, coord, 4, NULL);
        }
        func_800DA6E8(&ctx->node, (s32)damage, 0);
        func_800E2C78(ctx, contactWork->field_1C4[0].key, (s32)damage, 0);
        health  = (u16)ctx->hp - damage;
        ctx->hp = health;
        if ((health << 0x10) <= 0) {
            reaction = 2;
        }
        effect = Gp_GetIdParam0(contactWork->field_1C4[0].key) & 0xFFFF;
        switch (effect) {
            case 0:
                break;
            default:
                break;
            case 3:
                Gp_SetObjFlag4(ctx, contactWork->field_1C4[0].key, 0);
                break;
            case 4:
                if (ctx->hp > 0) {
                    if (work->field_36E == 0) {
                        reaction = 1;
                    }
                } else {
                    work->field_368 = one;
                }
                break;
            case 6:
                if (ctx->hp <= 0) {
                    work->field_368 = one;
                } else if (work->field_36E == 0) {
                    reaction = 1;
                }
                break;
            case 8:
                if ((work->field_36E == 0) && (reaction == 0)) {
                    Gp_SetObjFlag2(ctx, contactWork->field_1C4[0].key, 0);
                }
                break;
            case 1:
            case 2:
            case 5:
            case 9:
                if ((work->field_36E == 0) && (reaction == 0)) {
                    reaction = 1;
                }
                break;
        }
        if (reaction == one)
            goto state_one;
        if (reaction < 2) {
            if (reaction == 0)
                goto state_zero;
        } else {
            if (reaction == 2)
                goto state_two;
        }
        goto state_done;
    state_zero:
        if (work->field_352 != 0xA && damage != 0) {
            work->field_352 = 5;
            goto state_reset;
        }
        goto state_done;
    state_one:
        state = 3;
        goto state_assign;
    state_two:
        state = 7;
    state_assign:
        work->field_352 = state;
    state_reset:
        work->field_354 = 0;
    state_done:
        hitId = contactWork->field_1C4[0].key;
        if (lastId != hitId) {
            lastId = hitId;
            func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 3, NULL, &work->field_2C4);
        }
        result = Gp_GetIdParam2(contactWork->field_1C4[0].key);
        if (result > 0) {
            work->field_34E = (s16)result;
        }
    }
    goto next_contact;
physical_contact:
    dx2           = coord->workm.t[0] - contactWork->field_1C4[0].point.vx;
    scratch->vx.w = dx2;
    dy2           = coord->workm.t[1] - contactWork->field_1C4[0].point.vy;
    scratch->vy.w = dy2;
    dz2           = coord->workm.t[2] - contactWork->field_1C4[0].point.vz;
    scratch->vz.w = dz2;
    depth         = contactWork->field_1C4[0].depth - SquareRoot0((dx2 * dx2) + (dy2 * dy2) + (dz2 * dz2));
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
    contactWork = (Actor103800Work*)((u8*)contactWork + 0x18);
}
contact_test:
    if ((s32)contactWork < (s32)&work->slots[1].timeLeft)
        goto contact_loop;
    if ((push > 0) && (work->field_350 == 0)) {
        coord->coord.t[0] += (s32)(push * scratch[2].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(push * scratch[2].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1C4);
    work->field_36C = 0;
    result          = Gp_CountRec18Hi(work->field_2AC, 0x10000);
    if (result != 0) {
        Gp_ArmStateF0(1);
        work->field_36C = 1;
        work->field_37A = 1;
        if ((work->field_350 == 0) && (work->field_36E == 0) && (work->field_352 != 0xC)) {
            work->field_352 = 0xC;
            work->field_354 = 0;
        }
    }
    Gp_ClearRec18Occupied(work->field_2AC);
    SCRATCH_HEAD(GpDeltaScratch) += 3;
}

void Actor03800_Fn01150(Task* arg0)
{
    Actor103800Work* work;
    s32              turn;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                turn            = ((u32)Gp_LcgState >> 16) & 0x3FF;
                if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                    turn = -turn;
                }
                work->field_348 = 2;
                work->field_36A = 1;
                work->field_364 = (work->field_362 + turn) & 0xFFF;
            }
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                if (work->field_37A == 0) {
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x5A;
                } else {
                    work->field_352 = 1;
                    work->field_354 = 0;
                    if (work->field_36A == 0) {
                        work->field_36A = 1;
                    }
                }
            }
            break;
    }

    if (Gp_StateF0.field_2 & 5) {
        work->field_352 = 2;
        work->field_354 = 0;
        if (work->field_36A == 0) {
            work->field_36A = 1;
        }
        work->field_37A = 1;
    }
}

void Actor03800_Fn012B4(Task* arg0)
{
    Actor103800Work* work;
    s32              turn;
    s32              delta;
    s32              turn2;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35E = 2;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            turn            = ((u32)Gp_LcgState >> 16) & 0x1FF;
            if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                turn = -turn;
            }
            delta = turn;
            if (work->field_370 != 0) {
                delta           = turn + 0x800;
                work->field_370 = 0;
            }
            work->field_348 = 2;
            work->field_354 = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            turn            = 0; /* dead store: keeps `turn` cse-canonical over `delta` */
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_356 = (((u32)Gp_LcgState >> 16) & 0xF) + 0x19;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_358 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0xF) + 0x19;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                turn2           = ((u32)Gp_LcgState >> 16) & 0x1FF;
                if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                    turn2 = -turn2;
                }
                work->field_364 = (work->field_362 + turn2) & 0xFFF;
            }
            if (work->field_370 != 0) {
                if (work->field_35C < 0x1E) {
                    work->field_352 = 0;
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
                } else {
                    work->field_352 = 0xC;
                    work->field_354 = 0;
                }
            }
            work->field_358--;
            if (work->field_358 <= 0) {
                work->field_352 = 0;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }

    if (Gp_StateF0.field_2 & 5) {
        work->field_352 = 2;
        work->field_354 = 0;
    }
}

void Actor03800_Fn01520(Task* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            vec.vx          = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_364 = ratan2((s16)vec.vx, (s16)vec.vz) & 0xFFF;
            work->field_348 = 9;
            if (work->field_36A == 0) {
                work->field_36A = 1;
            }
            work->field_354 = 1;
            break;
        case 1:
            work->field_360 = 0x28;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                work->field_354 = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x78;
            }
            break;
        case 2:
            work->field_360 = 0;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            break;
    }
}

void Actor03800_Fn0166C(Task* arg0)
{
    Actor03800MoveScratch* scratch;
    Actor103800Work*       work;
    GpEnemy*               ctx;
    GsCOORDINATE2*         coord;
    s16                    state;
    s32                    snd;
    s32                    pan;
    s32                    pan2;

    scratch = (Actor03800MoveScratch*)SCRATCH_PUSH_BYTES(0x18);
    work    = arg0->work;
    ctx     = arg0->spawnArg2;
    state   = work->field_354;
    coord   = work->field_344;
    switch (state) {
        case 0:
            work->field_348  = 3;
            work->field_354  = 1;
            work->field_36A  = 0;
            work->field_36E  = 1;
            work->field_2AA &= 0x7FFF;
            snd              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40260003;
            pan              = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            break;
        case 1:
            if ((u32)(work->field_34C - 2) < 12) {
                scratch->delta.vx = coord->coord.t[0] - Player_Status.coordMtx->t[0];
                scratch->delta.vy = coord->coord.t[1] - Player_Status.coordMtx->t[1];
                scratch->delta.vz = coord->coord.t[2] - Player_Status.coordMtx->t[2];
                VectorNormalS(&scratch->delta, &scratch->normal);
                coord->coord.t[0] += (scratch->normal.vx * 17) >> 9;
                coord->coord.t[2] += (scratch->normal.vz * 17) >> 9;
            } else {
                work->field_35C = 0;
                work->field_35E = 0;
            }
            if ((s16)work->field_34C == 12) {
                snd  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40260002;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_34C >= 29) {
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F48 - ctx->hp) * 100 / Actor03800_D05F48) * 10 + 240;
            }
            break;
        case 2:
            work->field_37C--;
            if (work->field_37C <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(0x18);
}

void Actor03800_Fn01948(Task* arg0)
{
    Actor103800Work* work = arg0->work;
    GsCOORDINATE2*   coord;
    s16              state;
    s32              snd;
    s32              pan;

    state = work->field_354;
    coord = work->field_344;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                if (work->field_36E == 0) {
                    work->field_348 = 0xB;
                    work->field_356 = 0xC;
                } else {
                    work->field_348 = 6;
                    work->field_356 = 0x13;
                }
            } else {
                work->field_348 = 7;
                work->field_356 = 0;
                work->field_352 = 0xA;
            }
            work->field_354 = 1;
            work->field_34A = 1;
            work->field_36A = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_360 = 0;
            snd             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40260003;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            break;
        case 1:
            work->field_356 -= 1;
            if (work->field_356 > 0) {
                break;
            }
            if (work->field_36E == 0) {
                if (work->field_37E == 0) {
                    work->field_352 = 2;
                    work->field_354 = 0;
                } else {
                    work->field_352 = 6;
                    work->field_354 = 0;
                    work->field_356 = 0;
                }
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
            }
            break;
    }
}

void Actor03800_Fn01AD0(Task* arg0)
{
    GpEnemy*         ctx;
    Actor103800Work* work;

    work            = arg0->work;
    ctx             = arg0->spawnArg2;
    work->field_360 = 0;
    work->field_35C = 0;
    work->field_35E = 0;
    work->field_356--;
    if (work->field_356 <= 0) {
        if (work->field_36E == 0) {
            work->field_348 = 0xB;
        } else {
            work->field_348 = 6;
        }
        work->field_34A = 1;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_356 = (((u32)Gp_LcgState >> 16) & 7) + 3;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
        work->field_37E = 0;
        if (work->field_350 == 0) {
            if (work->field_36E == 0) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F48 - ctx->hp) * 100 / Actor03800_D05F48) * 10 + 240;
            }
        } else {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
}

void Actor03800_Fn01C50(Task* arg0)
{
    SVECTOR          rotation;
    MATRIX           matrix;
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    s16              state;

    work  = arg0->work;
    state = work->field_354;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                arg0->state     = 2;
                work->field_354 = 0;
                return;
            }
            work->field_366  = 0x80;
            work->field_372  = 0x80;
            work->field_354  = 1;
            work->field_22A |= 0x4200;
            return;
        case 1:
            if (work->field_374 != 0) {
                work->field_354 = 2;
                return;
            }
        default:
            return;
        case 2:
            work->field_348 = 5;
            work->field_354 = 3;
            return;
        case 3:
            rotation.vx = 0;
            rotation.vy = (u16)work->field_362;
            rotation.vz = 0;
            RotMatrix(&rotation, &matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->sub        = &gGfxViewCoord;
            coord->coord      = work->field_2CC;
            coord->coord.t[0] = work->coord.coord.t[0];
            coord->coord.t[1] = work->coord.coord.t[1];
            coord->coord.t[2] = work->coord.coord.t[2];
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            work->field_356 = 0xF;
            work->field_344 = coord;
            work->field_354 = 4;
            return;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_378 = 1;
                work->field_354 = 0;
                arg0->state     = 2;
            }
            break;
    }
}

/// Second copy of the idle "look around" tick; identical body to
/// `Actor03800_Fn02068`, which the overlay carries twice.
void Actor03800_Fn01EEC(Task* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_354 = 1;
            rand            = (u32)Gp_LcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = ((u32)Gp_LcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((Gp_StateF0.field_2 & 5) || work->field_36C != 0) {
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = ((u32)Gp_LcgState >> 16 & 0xF) + 0xF;
    }
}

/// Idle "look around" tick. State 0 counts `field_356` down and, on expiry,
/// picks a new facing `field_364` within +/-0x3FF of the current one; state 1
/// waits for the turn to finish and re-arms the countdown. Either way, an
/// active `Gp_StateF0.field_2` bit (1 or 4) or a non-zero `field_36C` aborts
/// back to state 0 with a short delay.
void Actor03800_Fn02068(Task* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_354 = 1;
            rand            = (u32)Gp_LcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = ((u32)Gp_LcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((Gp_StateF0.field_2 & 5) || work->field_36C != 0) {
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = ((u32)Gp_LcgState >> 16 & 0xF) + 0xF;
    }
}

void Actor03800_Fn021E4(Task* arg0)
{
    GpEnemy*               ctx;
    Actor103800Work*       work;
    GsCOORDINATE2*         coord;
    Actor03800TurnScratch* scratch;
    s32                    sound;
    s32                    pan;

    scratch = (Actor03800TurnScratch*)SCRATCH_PUSH_BYTES(sizeof(*scratch));
    SCHED_BARRIER();
    work  = arg0->work;
    ctx   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_354) {
        case 0:
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
            }
            break;
        case 1:
            work->field_366  = 0x100;
            work->field_372  = 0x80;
            work->field_22A |= 0x4200;
            if (work->field_374 != 0) {
                work->field_374  = 0;
                work->field_354  = 2;
                work->field_366  = 0x80;
                work->field_2AA &= 0x7FFF;
                sound            = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40260002;
                pan              = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(coord));
            }
            break;
        case 2:
            work->field_348 = 8;
            work->field_354 = 3;
            break;
        case 3:
            scratch->rotation.vx = 0;
            scratch->rotation.vy = (u16)work->field_362;
            scratch->rotation.vz = 0;
            RotMatrix(&scratch->rotation, &scratch->matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&scratch->matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&scratch->matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&scratch->matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->coord      = work->field_2CC;
            coord->coord.t[0] = work->coord.coord.t[0];
            coord->coord.t[1] = work->coord.coord.t[1];
            coord->coord.t[2] = work->coord.coord.t[2];
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            work->field_354 = 4;
            work->field_344 = coord;
            work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            break;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(0x28);
}

/// State 12 of `Actor03800_Fn032D8`: pick and hold a turn direction while the
/// actor is aiming at the player. State 0 chooses the side to turn towards -
/// clockwise (1) when the player is in front of the actor's local +Z axis,
/// anticlockwise (2) otherwise - and a pending `field_370` request forces the
/// clockwise side. States 1 and 2 drive `field_35C` by -/+0x7D while the yaw
/// error `field_34C` is inside 8..0x10 and hand over to state 3 once it grows
/// past 0x10; state 3 finishes the move and hands back to `field_352` 1.
void Actor03800_Fn02584(Task* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            vec.vx          = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_348 = 0xA;
            if (work->field_370 != 0) {
                work->field_370 = 0;
                work->field_354 = 1;
            } else {
                work->field_354 =
                    ((vec.vx * coord->coord.m[0][2]) + (vec.vz * coord->coord.m[2][2]) > 0) ? 1 : 2;
            }
            work->field_2AA &= 0x7FFF;
            break;

        case 1:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = -0x7D;
                break;
            }
            goto turn_done;

        case 2:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = 0x7D;
                break;
            }
        turn_done:
            work->field_35C = 0;
            if ((s16)work->field_34C >= 0x11) {
                work->field_354 = 3;
            }
            break;

        case 3:
            if ((s16)work->field_34C >= 0x14) {
                work->field_352  = 1;
                work->field_354  = 0;
                work->field_372  = 0x80;
                work->field_2AA |= 0x8000;
            }
            break;
    }
}

void Actor03800_Fn026F8(Task* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_364;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_362 = ang;
    if (adiff < 0x800) {
        step = work->field_360;
        if (step >= adiff) {
            work->field_362 = want;
        } else {
            next = work->field_362;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_362 = next;
        }
    } else {
        step = work->field_360;
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
        work->field_362 = work->field_364;
        goto done;
    turn:
        wrapStep = work->field_360;
        cur      = work->field_362;
        if (diff > 0) {
            work->field_362 = cur - wrapStep;
        } else {
            work->field_362 = cur + wrapStep;
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_362;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_POP_BYTES(8);
}

void Actor03800_Fn02848(Task* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    s16              next;
    s16              speed;
    u16              value;
    s32              scale;
    Actor103800Work* work2;

    work  = arg0->work;
    coord = work->field_344;
    work2 = work;
    if (work->field_35E != 0) {
        next            = (u16)work->field_35C + (u16)work->field_35E;
        work->field_35C = next;
        if (next >= 0x33) {
            work->field_35C = 0x32;
        }
    }
    speed = work2->field_35C;
    if (speed > 0) {
        scale = speed * 0x190;
        value = Actor03800_D05F40 + ((Actor03800_D05F40 * scale) / 10000);
    } else {
        value = Actor03800_D05F40;
    }
    work2->field_2A4   = (((s16)value | (Actor03800_D05F42 << 0xC)) & 0xFFFF) | 0x40000;
    work->field_2EC    = (s16)coord->coord.t[0];
    work->field_2EE    = (s16)coord->coord.t[1];
    work->field_2F0    = (s16)coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_35C) >> 0xC;
    coord->coord.t[1] += work->field_366;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_35C) >> 0xC;
}

void Actor03800_Fn02998(GpEnemy* arg0, Task* arg1)
{
    Actor103800Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor103800Work* work2;
    GsCOORDINATE2*   c;
    VECTOR           vec;
    s32              state;
    s32              i;
    s16              st;
    s16              phase;
    s16              anim;
    s16              val;
    s32              snd;
    s32              pan;

    obj   = arg1->extra;
    work  = arg1->work;
    state = Gp_StateF0.field_4;
    coord = work->field_344;
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
    st = work->field_354;
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
    if (st == 3) {
        goto case3;
    }
    return;
death:
    if (work->field_378 == 0) {
        anim = 1;
        if (work->field_36E != 0) {
            anim = 5;
        }
        work->field_348 = anim;
    }
    work->field_356 = 0;
    work->field_35A = 0x1000;
    work->field_2CC = coord->coord;
    arg0->recs      = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj((GpObj*)work->field_1A4);
    Gp_UnlinkObj((GpObj*)work->field_20C);
    Gp_UnlinkObj((GpObj*)work->field_28C);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 0x26);
    work->field_354 = 1;
    if (work->field_368 != 0) {
        obj->flags      = 0x80;
        work->field_354 = 3;
    }
    work2 = arg1->work;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 6);
    }
    c      = ((Actor103800Work*)arg1->work)->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
    snd = ((arg0->placeKey >> 12) << 8) | 0x40260004;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    return;
dying:
    Actor03800_Fn037E0(arg1);
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase == 10) {
        obj->flags = 2;
    }
    if (work->field_356 == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
    }
    if (work->field_356 >= 0x3C) {
        work->field_354 = 2;
        obj->flags      = 0x80;
    }
    work2 = arg1->work;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 6);
    }
    c      = ((Actor103800Work*)arg1->work)->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy((GpEnemy*)arg0, arg1);
    return;
case3:
    if (work->field_368 == 0) {
        goto timer;
    }
    if (work->field_368 < 2) {
        goto inc368;
    }
    work->field_368 = 0;
    Tmd_FreeBuffers((TmdObject*)obj);
    obj->flags |= 4;
    Actor03800_Fn02E50(arg1);
    goto timer;
inc368:
    work->field_368++;
timer:
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_354 = 2;
}

void Actor03800_Fn02E50(Task* actor)
{
    s16  variants[4];
    s32  selected;
    s16* choice;
    s16* variantBase;
    s16* firstOut;
    s16* nextOut;
    s32  i;
    s32  thirdProduct;
    s32  secondIndex;
    s32  remaining;
    s32  firstProduct;
    u32  thirdRandom;
    u32  firstVariant;
    u32  firstRandom;
    u32  secondRandom;

    firstProduct = Gp_LcgState * 5;
    firstRandom  = firstProduct + 0x71357911;
    Gp_LcgState  = (s32)firstRandom;
    firstVariant = ((firstRandom >> 16) % 5) & 0xFFFF;
    Actor03800_Fn03008(actor, firstVariant);
    firstOut = &variants[0];
    if ((u16)Gp_StateF0.field_6 < 2U) {
        remaining = Actor03800_D05FA8[(u16)Gp_StateF0.field_6];
        i         = 0;
        do {
            if (i != firstVariant) {
                *firstOut = i;
                firstOut++;
            }
            i += 1;
        } while (i < 5);
        secondRandom = (Gp_LcgState * 5) + 0x71357911;
        secondIndex  = (secondRandom >> 0x10) & 3;
        SOFT_TOUCH_REG(secondIndex);
        variantBase = variants;
        choice      = variantBase;
        choice     += secondIndex;
        selected    = *choice;
        Gp_LcgState = (s32)secondRandom;
        Actor03800_Fn03008(actor, (u32)selected);
        remaining--;
        if (remaining > 0) {
            nextOut = variantBase;
            i       = 0;
            do {
                if ((i != firstVariant) && (i != secondIndex)) {
                    *nextOut = i;
                    nextOut++;
                }
                i += 1;
            } while (i < 5);
            thirdProduct = Gp_LcgState * 5;
            thirdRandom  = thirdProduct + 0x71357911;
            Gp_LcgState  = (s32)thirdRandom;
            Actor03800_Fn03008(actor, (u32)variants[((thirdRandom >> 16) % 3) & 0xFFFF]);
        }
    }
}

void Actor03800_Fn03008(Task* actor, u32 variant)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    GpEffWork*   eff;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    switch (variant) {
        case 0:
            D_80067704[0] = Actor03800_D0459C;
            break;
        case 1:
            D_80067704[0] = Actor03800_D046A0;
            break;
        case 2:
            D_80067704[0] = Actor03800_D047A4;
            break;
        case 3:
            D_80067704[0] = Actor03800_D04868;
            break;
        case 4:
            D_80067704[0] = Actor03800_D0492C;
            break;
    }
    eff = Gp_SpawnEff(0x40007, ((TmdObject*)actor->extra)->coords + 3, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    raw        = ((GpEnemy*)actor->spawnArg2)->placeKey;
    model      = (TmdObject*)eff->task->extra;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);

    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

void Actor03800_Fn0315C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor03800_D00004;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2), arg0);
}

void Actor03800_Fn031B8(GpEnemy* arg0, Task* arg1)
{
    Actor103800Work* work;
    s32              state;
    s32              one;

    state = Gp_StateF0.field_4;
    one   = 1;
    work  = arg1->work;
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
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->node.state.b.flags         = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.state.b.flags         = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->field_360 != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->field_36A != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->field_344->flg = 0;
    Gp_UpdateCoord(work->field_344);
case1:
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

void Actor03800_Fn032D8(Task* arg0)
{
    Actor103800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->work;
    state = work->field_352;
    switch (state) {
        case 0:
            Actor03800_Fn01150(arg0);
            break;
        case 1:
            Actor03800_Fn012B4(arg0);
            break;
        case 2:
            Actor03800_Fn01520(arg0);
            break;
        case 3:
            Actor03800_Fn0166C(arg0);
            break;
        case 4:
            Actor03800_Fn03420(arg0);
            break;
        case 5:
            Actor03800_Fn01948(arg0);
            break;
        case 6:
            Actor03800_Fn01AD0(arg0);
            break;
        case 7:
            Actor03800_Fn01C50(arg0);
            break;
        case 8:
            Actor03800_Fn01EEC(arg0);
            break;
        case 9:
            Actor03800_Fn02068(arg0);
            break;
        case 10:
            Actor03800_Fn021E4(arg0);
            break;
        case 11:
            Actor03800_Fn034B0(arg0);
            break;
        case 12:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->field_36E == 0) {
        work->field_21E = -0xFA;
        mag             = 0xFA;
    } else {
        work->field_21E = -0x15E;
        mag             = 0x15E;
    }
    work->field_228 = mag;
}

void Actor03800_Fn03420(Task* arg0)
{
    Actor103800Work* work = arg0->work;
    s32              state;

    state = work->field_354;
    switch (state) {
        case 0:
            work->field_348 = 4;
            work->field_354 = 1;
            break;
        case 1:
            if ((s16)work->field_34C == 0x1E) {
                work->field_36E = 0;
            }
            if ((s16)work->field_34C >= 0x3A) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = state;
                }
                work->field_2AA |= 0x8000;
            }
            break;
    }
}

void Actor03800_Fn034B0(Task* arg0)
{
    TmdObject*       obj;
    GpEnemy*         ctx;
    Actor103800Work* work;

    work = arg0->work;
    obj  = arg0->extra;
    ctx  = arg0->spawnArg2;
    switch (Gp_StateF0.field_20) {
        case 0:
            obj->flags              = 0x84;
            ctx->node.state.b.flags = 1;
            return;
        case 1:
            obj->flags       = 0;
            work->field_1C2 |= 0x8000;
            work->field_22A |= 0x4200;
            work->field_2AA |= 0x8000;
            Gp_ArmStateF0(1);
            work->field_366 = 0x80;
            work->field_356 = 0x5A;
            work->field_350 = 0;
            work->field_372 = 0x80;
            return;
        case 2:
            if (--work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            return;
    }
}

void Actor03800_Fn03594(Task* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;

    work  = arg0->work;
    coord = work->field_344;
    if (--work->field_36A <= 0) {
        work->field_36A = 0xC;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40260001;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(coord));
    }
}

void Actor03800_Fn03628(Task* arg0)
{
    Actor103800Work* work2;
    s32              i;
    s32              val;

    work2 = arg0->work;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 6);
    }
}

void Actor03800_Fn036EC(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((Actor103800Work*)arg0->work)->field_344;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

void Actor03800_Fn03744(Task* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->work;
    coord = work->field_344;
    if (work->field_350 == 0) {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x1F4, work->field_372);
        return;
    }
    hit = func_800EA1A8((VECTOR3*)coord->workm.t, &vec);
    if (hit != 0) {
        Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
    }
}

void Actor03800_Fn037E0(Task* arg0)
{
    Actor103800Work*   work;
    GsCOORDINATE2*     coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;

    work                            = arg0->work;
    head                            = SCRATCH_HEAD(ActorScaleScratch);
    scratch                         = head - 1;
    SCRATCH_HEAD(ActorScaleScratch) = scratch;
    coord                           = work->field_344;
    if (work->field_35A >= 0x201) {
        work->field_35A = (u16)work->field_35A - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_35A;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_2CC;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    SCRATCH_POP(ActorScaleScratch);
}
