#include "common.h"

#include "actors/actor_102500.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;

void* Mem_Calloc(s32 size, s32 arg1);
void  Gp_DestroyEnemy(Actor02500Ctx* ctx, Actor02500* actor);
void  Gp_LinkNode(Actor02500Node* node);
void  func_800B3F84(Actor02500Work* arg0, void* arg1, Actor02500Obj2C* arg2, void* arg3,
                    Actor02500AnimSlots* arg4);
void  Gp_AnimResetSlot(Actor02500Work* arg0, s32 arg1, s32 arg2);
void  Gp_IncStateF0Ref(s32 arg0);
void  Gp_SetLightMode(Actor02500Ctx* arg0, s32 arg1);
void  Gp_LinkObj(s32 arg0, Actor02500Obj* arg1);
void  Gp_InitRec18Table(Actor02500Rec18* arg0, s32 arg1, s32 arg2);
s32   Gp_PackPair(void* arg0, s32 arg1);

void Actor02500_Fn00078(Actor02500Ctx* ctx, Actor02500* actor)
{
    Actor02500Work*  work;
    Actor02500Obj2C* obj;
    GsCOORDINATE2*   coord;
    s32              i;

    obj   = actor->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x348, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C = work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = work->field_14C;
    obj->field_20   = work->field_12C;
    ctx->field_4    = &coord->coord;
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_20     = -0x96;
    ctx->field_18     = coord;
    ctx->node.field_4 = 0;
    ctx->field_1C     = 0;
    ctx->field_24     = 0;
    ctx->field_50     = Actor02500_D05B38;
    ctx->field_40     = Actor02500_D05B38->field_4;
    work->field_2E0   = 0x200;
    work->field_2DC   = coord;
    work->field_2E2   = 1;
    func_800B3F84(work, &Actor02500_D05BA0, obj, work->field_DC, &work->field_14);
    work->field_31C = 1;
    work->field_31E = 1;
    for (i = 1; i < 5; i++) {
        Gp_AnimResetSlot(work, i, work->field_31C);
    }
    Gp_IncStateF0Ref(0);
    switch (ctx->field_3C->field_2) {
        case 0:
            work->field_322 = 0;
            work->field_324 = 0;
            ctx->field_54   = work->field_1C4;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_32E = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x1E;
            break;
        case 1:
            work->field_322 = 5;
            work->field_324 = 0;
            ctx->field_54   = NULL;
            Gp_SetLightMode(ctx, 2);
            break;
        case 2:
            work->field_322 = 5;
            work->field_324 = 1;
            ctx->field_54   = NULL;
            Gp_SetLightMode(ctx, 2);
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_338 = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x1E;
    work->field_314 = coord->coord.t[0];
    work->field_316 = coord->coord.t[1];
    work->field_318 = coord->coord.t[2];
    work->field_32A = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;

    work->field_16C.field_8  = coord;
    work->field_16C.field_C  = work->field_18C;
    work->field_16C.field_10 = 0;
    work->field_16C.field_12 = -0x190;
    work->field_16C.field_14 = 0x258;
    work->field_16C.field_18 = 0;
    work->field_16C.field_1C = 0x258;
    work->field_16C.flags    = 1;
    Gp_LinkObj(3, &work->field_16C);
    Gp_InitRec18Table(work->field_18C, 1, 0);

    work->field_1A4.field_8  = coord;
    work->field_1A4.field_C  = work->field_1C4;
    work->field_1A4.field_10 = 0;
    work->field_1A4.field_12 = -0x12C;
    work->field_1A4.field_14 = 0;
    work->field_1A4.field_18 = 0x30019;
    work->field_1A4.field_1C = 0x12C;
    work->field_1A4.flags    = 1;
    work->field_16C.flags   |= 0x8000;
    Gp_LinkObj(2, &work->field_1A4);
    Gp_InitRec18Table(work->field_1C4, 3, 0);

    if (ctx->field_3C->field_2 == 0) {
        work->field_1A4.flags |= 0x8000;
    } else {
        work->field_1A4.flags &= 0x7FFF;
    }

    work->field_20C.field_8  = coord;
    work->field_20C.field_C  = work->field_22C;
    work->field_20C.field_10 = 0;
    work->field_20C.field_12 = -0x12C;
    work->field_20C.field_14 = 0;
    work->field_20C.field_18 = 0x30019;
    work->field_20C.field_1C = 0x12C;
    work->field_20C.flags    = 1;
    Gp_LinkObj(2, &work->field_20C);
    Gp_InitRec18Table(work->field_22C, 5, 0);
    work->field_20C.flags |= 0x4200;

    work->field_2A4.field_8  = actor->field_2C->field_8 + 4;
    work->field_2A4.field_C  = work->field_2C4;
    work->field_2A4.field_10 = 0;
    work->field_2A4.field_12 = -0x3B6;
    work->field_2A4.field_14 = 0x1CC;
    work->field_2A4.field_18 = Gp_PackPair(&Actor02500_D05B30, 0);
    work->field_2A4.field_1C = 0x12C;
    work->field_2A4.flags    = 1;
    Gp_LinkObj(3, &work->field_2A4);
    Gp_InitRec18Table(work->field_2C4, 1, 0);
    work->field_2A4.flags &= 0x7FFF;
    actor->field_30        = 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn00494);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0053C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00574);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00590);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L005B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L005BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L005E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L005EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L005F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00628);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00710);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00744);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L007C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L007D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00800);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00858);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00878);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00888);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0089C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L008B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L008E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00900);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00978);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L009B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L009B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00A10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00A8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00AC8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn00B18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00B78);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00B90);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00BEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00C34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00CD8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00D08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00D30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00D3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00D88);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00D8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn00DD8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00E44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00E5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00E74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00F4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00F60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00F70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00F74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L00F98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01028);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01050);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L010AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L010C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L010DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L010E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L010EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn01144);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L011FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01280);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L012CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L012D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn012F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0136C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01408);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0140C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01418);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01454);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01458);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01484);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01518);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0153C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0158C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L015C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L015E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01650);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L016BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L016C0);

void Actor02500_Fn016FC(Actor02500* arg0)
{
    Actor02500Work*       work;
    GsCOORDINATE2*        coord;
    Actor02500RotScratch* sc;
    s32                   ang;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s32                   step;
    s32                   cur;
    s32                   next;
    s32                   wrapStep;

    sc    = (Actor02500RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_32A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_32C = ang;
    if (adiff < 0x800) {
        step = work->field_328;
        if (step >= adiff) {
            work->field_32C = want;
        } else {
            next = work->field_32C;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_32C = next;
        }
    } else {
        step = work->field_328;
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
        work->field_32C = work->field_32A;
        goto done;
    turn:
        wrapStep = work->field_328;
        cur      = work->field_32C;
        if (diff > 0) {
            work->field_32C = cur - wrapStep;
        } else {
            work->field_32C = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_32C;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn0184C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01928);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L019EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01AB0);

void  Gp_UnlinkNode(void* node);
void  Gp_UnlinkObj(void* node);
void  Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void  Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
void* Gp_SpawnEnemyFromTable(void* table, s32 idx, s32 arg2, void* parent);
void  Actor02500_Fn0184C(Actor02500* arg0);
void  Actor02500_Fn02480(Actor02500* arg0);

extern u8 D_801153F4;

void Actor02500_Fn01AC8(Actor02500Ctx* arg0, Actor02500* arg1)
{
    Actor02500Work*  work;
    Actor02500Obj2C* obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           vec;
    s32              mode;
    s32              one;
    s16              st;
    s16              phase;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    mode  = D_801153F4;
    coord = obj->field_8;
    if (mode == 1) {
        goto case1;
    }
    if (mode < 2) {
        goto common;
    }
    if (mode == 2) {
        goto case2;
    }
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
case2:
    obj->field_C = 0x80;
    return;
common:
    one = 1;
    st  = work->field_324;
    if (st == one) {
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
    work->field_31C = 8;
    work->field_32E = 0;
    work->field_332 = 0x1000;
    work->field_2E4 = coord->coord;
    arg0->field_54  = NULL;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_16C);
    Gp_UnlinkObj(&work->field_1A4);
    Gp_UnlinkObj(&work->field_20C);
    Gp_UnlinkObj(&work->field_2A4);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 0x19);
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    if (work->field_33C == 0) {
        work->field_324 = one;
        return;
    }
    obj->field_C    = 0x80;
    work->field_324 = 3;
    return;
dying:
    Actor02500_Fn02480(arg1);
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase == 10) {
        obj->field_C = 2;
    }
    if (work->field_32E == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
        Gp_SpawnEnemyFromTable(Actor02500_D05B88, 1, 0, arg0);
    }
    if (work->field_32E >= 0x3C) {
        obj->field_C    = 0x80;
        work->field_324 = 2;
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
case3:
    if (work->field_33C == 0) {
        goto timer;
    }
    if (work->field_33C < 2) {
        goto inc;
    }
    work->field_33C = 0;
    Tmd_FreeBuffers((TmdObject*)obj);
    obj->field_C |= 4;
    Actor02500_Fn0184C(arg1);
    goto timer;
inc:
    work->field_33C++;
timer:
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_324 = 2;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn01E04);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor02500_Fn00494(Actor02500* arg0);
void Actor02500_Fn016FC(Actor02500* arg0);
void Actor02500_Fn02008(Actor02500* arg0);
void Actor02500_Fn02288(Actor02500* arg0);
void Actor02500_Fn02318(Actor02500* arg0);
void Actor02500_Fn023D8(Actor02500* arg0);
void Actor02500_Fn02430(Actor02500* arg0);

void Actor02500_Fn01E60(Actor02500Ctx* arg0, Actor02500* arg1)
{
    Actor02500Work*  work;
    Actor02500Obj2C* temp_a1;
    GsCOORDINATE2*   temp_s2;
    s32              state;
    s32              one;

    temp_a1 = arg1->field_2C;
    state   = D_801153F4;
    work    = arg1->field_1C;
    temp_s2 = temp_a1->field_8;
    one     = 1;
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
    temp_a1->field_C   = 0;
    arg0->node.field_4 = 0;
    goto default_body;
case1:
    if (work->field_322 == 5) {
        return;
    }
    Actor02500_Fn023D8(arg1);
    goto tail;
case2:
    temp_a1->field_C   = 0x80;
    arg0->node.field_4 = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor02500_Fn01F8C(arg1);
    }
    Actor02500_Fn00494(arg1);
    Actor02500_Fn02008(arg1);
    if (work->field_328 != 0) {
        Actor02500_Fn016FC(arg1);
    }
    Actor02500_Fn02288(arg1);
    Actor02500_Fn02318(arg1);
    temp_s2->flg = 0;
    Gp_UpdateCoord(temp_s2);
    Actor02500_Fn023D8(arg1);
    if (work->field_322 == 5) {
        return;
    }
tail:
    Actor02500_Fn02430(arg1);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn01F8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01FBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L01FE8);

void Actor02500_L02000(void)
{
}

void Actor02500_Fn00B18(Actor02500* arg0);
void Actor02500_Fn00DD8(Actor02500* arg0);
void Actor02500_Fn01144(Actor02500* arg0);
void Actor02500_Fn012F0(Actor02500* arg0);
void Actor02500_Fn020D0(Actor02500* arg0);
void Actor02500_Fn02178(Actor02500* arg0);
void Actor02500_Fn021F8(Actor02500* arg0);

void Actor02500_Fn02008(Actor02500* arg0)
{
    switch (arg0->field_1C->field_322) {
        case 0:
            Actor02500_Fn00B18(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case 1:
            Actor02500_Fn00DD8(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case 2:
            Actor02500_Fn020D0(arg0);
            break;
        case 3:
            Actor02500_Fn02178(arg0);
            break;
        case 4:
            Actor02500_Fn021F8(arg0);
            break;
        case 5:
            Actor02500_Fn012F0(arg0);
            break;
        case 6:
            arg0->field_30 = 2;
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn020D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L020F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02114);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02144);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02168);

void Actor02500_L02170(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02178);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L021A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L021C8);

void Actor02500_L021F0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn021F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02230);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02254);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02274);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02288);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02318);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02368);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02394);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L023A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L023C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn023D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02430);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02480);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L024D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02574);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn025D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02634);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02730);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02750);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L027F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02828);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0283C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L0284C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn02874);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L028A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L028CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L028E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_L02904);
