#include "common.h"

#include "actors/actor_103800.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/gfx.h"
#include <psyq/inline_c.h>

void Actor03800_Fn00974(Actor103800* arg0);
void Actor03800_Fn00A98(Actor103800* arg0);
void Actor03800_Fn026F8(Actor103800* arg0);
void Actor03800_Fn02848(Actor103800* arg0);
void Actor03800_Fn02E50(Actor103800* arg0);
void Actor03800_Fn03594(Actor103800* arg0);
void Actor03800_Fn03628(Actor103800* arg0);
void Actor03800_Fn036EC(Actor103800* arg0);
void Actor03800_Fn03744(Actor103800* arg0);
void Actor03800_Fn037E0(Actor103800* arg0);
void Gp_ArmStateF0(s32 arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

extern u8  D_801153F2;
extern u8  D_801153F4;
extern s32 Gp_LcgState;

void              Actor03800_Fn003B8(Task* arg0);
extern GpPairSrcE Actor03800_D05F44;
extern u8         Actor03800_D05F60[];

void Actor03800_Fn000B8(GpEnemy* arg0, Task* arg1)
{
    GpObj*            obj;
    GpRec18*          records1;
    GpRec18*          records2;
    GpRec18*          records3;
    Actor103800Work*  work;
    s32               i;
    Actor103800Obj2C* extra;

    extra = arg1->extra;
    work  = memCalloc(0x384, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    extra->field_1C = &work->field_184;
    extra->field_C  = 0;
    extra->field_20 = &work->field_164;
    ((void (*)())Gp_IncStateF0Ref)(0);
    Actor03800_Fn003B8(arg1);
    arg0->field_4  = &work->field_344->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &((Actor103800Obj2C*)arg1->extra)->field_8[3];
    arg0->node.flags           = 0;
    arg0->bodyPos.vx           = 0;
    arg0->recs                 = work->field_1C4;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &Actor03800_D05F44;
    arg0->hp                   = (s16)Actor03800_D05F44.hpMax;
    work->field_2C4.coord      = &((Actor103800Obj2C*)arg1->extra)->field_8[3];
    work->field_2C4.spawnArgLo = 0x200;
    work->field_2C4.spawnArgHi = 1;
    func_800B3F84(&work->anim, Actor03800_D05F60, (TmdObject*)extra, work->field_104, work->slots);
    for (i = 1; i < 6; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    work->field_1AC = ((Actor103800Obj2C*)arg1->extra)->field_8;
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
    work->field_214 = ((Actor103800Obj2C*)arg1->extra)->field_8;
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
    work->field_294 = ((Actor103800Obj2C*)arg1->extra)->field_8;
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
    Actor103800Work*     work;
    Actor103800Ctx*      ctx;
    GsCOORDINATE2*       src;
    Actor103800MatWords* mtx;
    Actor103800MatWords* srcmtx;
    Actor103800MatWords* mtx2;
    Actor103800MatWords* srcmtx2;
    SVECTOR              rot;
    MATRIX               mat;
    s16                  mode;
    s16                  kind;

    ctx  = (Actor103800Ctx*)arg0->spawnArg2;
    work = (Actor103800Work*)arg0->work;
    src  = ((Actor103800Obj2C*)arg0->extra)->field_8;
    mode = ctx->field_3C->mode / 10;

    work->field_350 = mode;
    switch (mode) {
        case 0:
            kind            = ctx->field_3C->mode % 10;
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
            work->field_344 = ((Actor103800Obj2C*)arg0->extra)->field_8;
            break;
        case 1:
            work->field_352 = 8;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx                = (Actor103800MatWords*)&work->coord.coord;
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

            srcmtx                = (Actor103800MatWords*)&src->coord;
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
            gte_rtir_real();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir_real();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir_real();
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

            mtx2                = (Actor103800MatWords*)&work->coord.coord;
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

            srcmtx2                = (Actor103800MatWords*)&src->coord;
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
            gte_rtir_real();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir_real();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir_real();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 3:
            work->field_352 = 0xB;
            work->field_366 = 0;
            work->field_372 = -1;
            work->field_344 = ((Actor103800Obj2C*)arg0->extra)->field_8;
            break;
    }
}

void Actor03800_Fn00974(Actor103800* arg0)
{
    Actor103800Ctx*  ctx;
    Actor103800Work* work;
    s16              damage;
    u16              remaining;
    u8               flags;

    ctx   = arg0->field_20;
    flags = ctx->field_4C;
    work  = arg0->field_1C;
    if (flags & 2) {
        if (work->field_350 == 0) {
            ctx->field_4C   = flags & 0xFD;
            work->field_352 = 6;
            work->field_354 = 0;
            work->field_356 = 0;
            work->field_37E = 1;
        } else if ((work->field_352 != 0xA) || (work->field_354 >= 4)) {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
    if (ctx->field_4C & 0xC) {
        damage = Gp_TickObjFlag4((GpObj5C*)ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, (s32)damage, 0);
            remaining     = ctx->field_40 - damage;
            ctx->field_40 = remaining;
            if ((s16)remaining <= 0) {
                work->field_352 = 7;
            } else {
                work->field_352 = 5;
            }
            work->field_354 = 0;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}
