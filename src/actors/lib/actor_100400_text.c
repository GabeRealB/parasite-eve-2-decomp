#include "common.h"

#include "actors/actor_100400.h"
#include "actors/coord_to_view.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "psyq/inline_c.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u32 Gp_LcgState;
extern u8  D_801153F2[2];
extern u8  D_801153F4;
void       Gp_ArmStateF0(s32 active);
void       Actor00400_Fn005DC(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, s32 arg3);
extern s32 D_80115738;

/* This overlay calls the gameplay helpers through its own (wider) prototypes:
   the extra trailing arguments are set up at every call site but ignored by
   the definitions in src/gameplay/3A34.c. */

void       Gp_SetObjFlag1(Actor100400Obj* arg0);
void       Gp_SetObjFlag2(Actor100400Obj* arg0, s32 arg1, s32 arg2);
void       Gp_SetObjFlag4(Actor100400Obj* arg0, s32 arg1, s32 arg2);
s32        Gp_TickObjFlag4(Actor100400Obj* arg0);
s32        Gp_ObjFlag4Expired(Actor100400Obj* arg0);
u32        Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3);
s32        Gp_RollEnemyChance(Actor100400Obj* arg0, u32 arg1, s32 arg2);
s32        Gp_GetIdParam0(s32 arg0);
s32        Gp_GetIdParam1(s32 arg0);
s32        Gp_GetIdParam2(s32 arg0);
GpEffWork* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);
void       func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void       func_800E2C78(Actor100400Obj* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800DA6E8(void* arg0, s32 arg1, s32 arg2);
s32        func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
void       Gp_ClearRec18Occupied(GpRec18* arg0);
void       Gp_SetLightMode(Actor100400Obj* arg0, s32 arg1);
void       Actor00400_Fn0875C(Actor100400* arg0, Actor100400Entry8* arg1, s32 arg2, s32 arg3);
void       Actor00400_Fn088EC(Actor100400* arg0, s16 arg1, s16 arg2, s16 arg3);
void       Actor00400_Fn02648(Actor100400* arg0, s32 arg1);
void       Actor00400_Fn0237C(Actor100400* arg0);
void       Actor00400_Fn02FF8(Actor100400* arg0);
void       Gp_IncStateF0Ref(s32 arg0);
s32        Gp_GetObjPan(GsCOORDINATE2* arg0);
s32        Gp_GetObjDepth(GsCOORDINATE2* arg0);
void       Gp_UpdateActorColor(Actor100400Obj* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void       Gp_SetObjTrans(Actor100400Ctx* arg0, s32 arg1, s32 arg2, s32 arg3);
s32        SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void       Gp_AnimTickIndex(Actor100400Work* arg0, s32 arg1);
void       Actor00400_Fn085B8(Actor100400* arg0);
void       Actor00400_Fn08624(Actor100400* arg0);
s16        Actor00400_Fn086FC(Actor100400* arg0, s16 arg1);
void       Actor00400_Fn08814(Actor100400* arg0);
s16        Actor00400_Fn08908(Actor100400* arg0);
void       Actor00400_Fn0824C(Actor100400* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
void       Actor00400_Fn08464(Actor100400* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
void       Gp_UnlinkObj(void* node);
void       Gp_LinkObj(s32 arg0, GpObj* arg1);
void       Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2);
s32        Gp_PackObjPair(Actor100400Obj* arg0, s32 arg1);
void       Gp_UnlinkNode(void* node);
void       Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void       Actor00400_Fn060CC(Actor100400* arg0);
void       Actor00400_Fn09714(Actor100400* arg0);
void       Actor00400_Fn097C8(Actor100400* arg0);
void       Actor00400_Fn06EA4(Actor100400* arg0);
void       Actor00400_Fn08ADC(Actor100400* arg0);
void       Actor00400_Fn08A88(Actor100400* arg0);
void       Actor00400_Fn08B40(Actor100400* arg0);
void       Actor00400_Fn08B94(Actor100400* arg0);
void       Gp_AnimResetSlot(Actor100400Work* anim, s32 slot, s32 active);
void       Actor00400_Fn06F64(Actor100400* arg0);
void       Actor00400_Fn0A880(Actor100400* arg0);
void       Actor00400_Fn04900(Actor100400* arg0);
void       Actor00400_Fn0A940(Actor100400* arg0);
void       Actor00400_Fn04A1C(Actor100400* arg0);
void       Actor00400_Fn0A9F4(Actor100400* arg0);
void       Actor00400_Fn0AA40(Actor100400* arg0);
void       Actor00400_Fn0A3D4(Actor100400* arg0);
void       Actor00400_Fn0A414(Actor100400* arg0);
void       Actor00400_Fn098A8(Actor100400* arg0);
void       Actor00400_Fn09924(Actor100400* arg0);
s16        Actor00400_Fn02154(Actor100400* arg0);
void       Actor00400_Fn0A5B8(Actor100400* arg0);
void       Gp_UpdateCoord(GsCOORDINATE2* arg0);
void       Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
void       Actor00400_Fn00E3C(Actor100400* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       Actor00400_Fn019B4(Actor100400* arg0);
void       Gp_LinkNode(byte* node, s32 slot);
void       func_800B3F84(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4);
void       Actor00400_Fn0814C(Actor100400* arg0, s16 arg1, SVECTOR* arg2, s16 arg3);
void       Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst);
void       Actor00400_Fn03570(GsCOORDINATE2* arg0, s16 arg1);
void       Gp_MtxToEuler(MATRIX* arg0, SVECTOR* arg1);
void       func_8004BFF8(s32 angle, MATRIX* matrix);

extern GsCOORDINATE2    Gfx_ViewCoord;
extern MATRIX           Gfx_ViewWorldMtx;
extern TaskFuncTable3   Actor00400_D0002C;
extern TaskFuncTable11  Actor00400_D0007C;
extern TaskFuncTable10  Actor00400_D000A8;
extern TaskFuncTable10  Actor00400_D000D0;
extern TaskFuncTable4   Actor00400_D00134;
extern TaskFuncTable3   Actor00400_D00144;
extern TaskFuncTable3   Actor00400_D00150;
extern TaskFuncTable3   Actor00400_D0015C;
extern TaskFuncTable4   Actor00400_D00168;
extern Actor100400Stats Actor00400_D0FDC8;
extern TaskDesc         Actor00400_D16028;
extern u16              Actor00400_D1609C[8];
extern byte             Actor00400_D1604C[];
extern u8               Actor00400_D0E5B8[];
extern u8               Actor00400_D0E970[];
extern u8               Actor00400_D0ED28[];
extern u8               Actor00400_D0F25C[];
extern u8               Actor00400_D0F790[];
extern u8               Actor00400_D0FD9C[];
extern void*            D_800678F0[1];

/// Spawns the hit/impact effect burst for `coord`. `arg3` packs an effect
/// parameter in its low 12 bits and a 4-bit variant index at bits 12..15;
/// `kind` selects between a single spark (0), a spark plus an optional
/// directional puff (1), and a four-shot burst (2). `phase` drives the
/// sub-effect `Actor00400_Fn005DC` plays and gates the puff on its low bits.
void Actor00400_Fn001AC(GsCOORDINATE2* coord, u16 phase, u16 kind, u32 arg3)
{
    SVECTOR vec;
    s32     i;
    u16     variant;
    u16     param;

    if (Gp_State1C->field_4 != 0) {
        Actor00400_Fn005DC(coord, ((u32)phase >> 1) % 6, 0x400, 0);
        if (Gp_State1C->field_4 >= 4) {
            return;
        }
    }

    variant = (arg3 >> 12) & 0xF;
    param   = arg3 & 0xFFF;

    switch (kind) {
        case 0:
            Gp_SpawnEff(D_80115738, coord, 0x14001000 + param + variant, NULL);
            break;

        case 1:
            Actor00400_Fn005DC(coord, ((u32)phase >> 1) % 6, param, 0);
            if (!(phase & 1)) {
                Gp_SpawnEff(D_80115738, coord, 0x01000000 + param + variant, NULL);
            }
            if (!(phase & 7)) {
                SVECTOR* dir;

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;

        case 2:
            Actor00400_Fn005DC(coord, ((u32)phase >> 1) % 6, param, 0);
            Gp_SpawnEff(D_80115738, coord, 0x10001000 + param + variant, NULL);
            for (i = 0; i < 4; i++) {
                SVECTOR* dir;

                Gp_SpawnEff(D_80115738, coord, 0x02001000 + param + variant, NULL);

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn005DC);

void Actor00400_Fn00A14(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_646 != 0) {
        if (!(work->field_646 & 7)) {
            s32 id  = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x4004000B;
            s32 pan = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        }
        if (work->field_646 == 0x18 || work->field_646 == 0x30) {
            func_800FDB18(7, &arg0->field_2C->field_8[1], NULL, &work->field_5DC);
        }
        if (work->field_646 == 0x16 && work->field_666 == 0) {
            work->obj_4DC.flags |= 0x8000;
        }
        if (--work->field_646 == 0) {
            work->obj_4DC.flags &= 0x7FFF;
        }
    }
}

void Actor00400_Fn00B48(Actor100400* arg0)
{
    Actor100400Ctx*  ctx;
    Actor100400Work* work;
    Actor100400Obj*  obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coords;
    u16              hp;
    u8               slot;

    ctx                     = arg0->field_2C;
    work                    = arg0->field_1C;
    obj                     = arg0->field_20;
    ctx->field_1C           = &work->field_59C;
    ctx->field_C            = 0;
    ctx->field_20           = &work->field_57C;
    coords                  = arg0->field_2C->field_8;
    coord                   = ctx->field_8;
    work->field_5DC.field_4 = 0x600;
    work->field_5DC.field_6 = 3;
    work->field_664         = 4;
    work->field_5DC.field_0 = &coords[1];
    obj->field_4            = &coord->coord;
    obj->field_48           = 0;
    obj->field_1C           = 0;
    obj->field_20           = 0;
    obj->field_24           = 0;
    slot                    = work->field_664;
    obj->field_18           = &arg0->field_2C->field_8[slot];
    Gp_LinkNode(obj->field_10, slot);
    obj->field_14 = 1;
    obj->field_54 = work->field_39C;
    obj->field_50 = &Actor00400_D0FDC8;
    hp            = Actor00400_D0FDC8.field_4;
    obj->field_42 = hp;
    obj->field_40 = hp;
    coord->sub    = &Gfx_ViewCoord;
    func_800B3F84(work, Actor00400_D1604C, ctx, work->field_26C, work->field_14);
    Actor00400_Fn019B4(arg0);
    work->field_556 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

void Actor00400_Fn00C84(Actor100400* arg0)
{
    Actor100400*      actor;
    GpActorWork*      player;
    Actor100400Work*  work;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    pc;
    Actor100400Entry8 pos;
    u8                frame;

    actor  = arg0;
    player = Gp_ActorSlots[0];
    work   = actor->field_1C;
    coord  = actor->field_2C->field_8;
    if (work->field_628 != 4) {
        Actor00400_Fn088EC(actor, 4, 0x20, 0xA);
        Actor00400_Fn08814(actor);
    }
    frame = Actor00400_Fn086FC(actor, 0x39);
    if (Actor00400_Fn08908(actor)) {
        work->field_62A = 0;
    }
    if (work->field_62A == 0) {
        Actor00400_Fn0824C(actor, 0xB, 0xE, (SVECTOR*)&work->field_564);
        if (work->pad_65C[0] & 1) {
            s32 id  = (((u16)actor->field_20->field_8 >> 12) << 8) | 0x40040002;
            s32 pan = (s8)Gp_GetObjPan(actor->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(actor->field_2C->field_8));
        } else {
            s32 id  = (((u16)actor->field_20->field_8 >> 12) << 8) | 0x40040003;
            s32 pan = (s8)Gp_GetObjPan(actor->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(actor->field_2C->field_8));
        }
        work->pad_65C[0]++;
    }
    if (work->field_62A >= 0 && frame >= work->field_62A) {
        pc          = player->extra->field_8;
        pos.field_0 = pc->coord.t[0];
        pos.field_2 = pc->coord.t[1];
        pos.field_4 = pc->coord.t[2];
        Actor00400_Fn0875C(actor, &pos, 8, 0x100);
        Actor00400_Fn08464(actor, 0xB, 0xE, (SVECTOR*)&work->field_564);
    }
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00E3C);

void Actor00400_Fn012B0(Actor100400* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    Actor00400_Fn00E3C(arg0, 1, 2, 0x258, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 2, 3, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 3, 4, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 4, 5, 0x1F4, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 1, 6, 0x320, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 6, 7, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 7, 8, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 1, 0xC, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 0xC, 0xD, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 0xD, 0xE, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 1, 9, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 9, 0xA, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 0xA, 0xB, 0x12C, arg1, temp_s2);
}

/* Tracks the nearer of the two party members and stores the result in the
   actor's work block.

   `field_54C`..`field_550` snapshot the actor's own root translation. The
   second coordinate of the model (`coord[1]`) is taken into view space and
   each slot's root translation measured against it; the closer of the two
   lands in `field_5E4` with its XZ distance in `field_640`. The chosen
   offset is then normalised and turned into a yaw relative to the actor's
   own heading (`field_556`) in `field_634`. */
void Actor00400_Fn01454(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c0;
    GsCOORDINATE2*   c1;
    GpActorWork*     player;
    GsCOORDINATE2*   joint;
    SVECTOR          delta0;
    SVECTOR          delta1;
    SVECTOR          view;
    s32              dist0;
    s32              dist1;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    player          = Gp_ActorSlots[0];
    joint           = &coord[1];
    work->field_54C = coord->coord.t[0];
    work->field_54E = coord->coord.t[1];
    work->field_550 = coord->coord.t[2];
    if (player != NULL) {
        c0      = player->extra->field_8;
        view.vx = 0;
        view.vy = 0;
        view.vz = 0;
        ActorCoordToView(joint, &view);
        delta0.vx = c0->coord.t[0] - view.vx;
        delta0.vy = c0->coord.t[1] - view.vy;
        delta0.vz = c0->coord.t[2] - view.vz;
        dist0     = SquareRoot0(delta0.vx * delta0.vx + delta0.vz * delta0.vz);
        if (Gp_ActorSlots[1] == NULL) {
            work->field_5E4.vx = c0->coord.t[0];
            work->field_5E4.vy = c0->coord.t[1];
            work->field_5E4.vz = c0->coord.t[2];
            work->field_640    = dist0;
        } else {
            c1        = Gp_ActorSlots[1]->extra->field_8;
            delta1.vx = c1->coord.t[0] - view.vx;
            delta1.vy = c1->coord.t[1] - view.vy;
            delta1.vz = c1->coord.t[2] - view.vz;
            dist1     = SquareRoot0(delta1.vx * delta1.vx + delta1.vz * delta1.vz);
            if (dist1 < dist0) {
                work->field_5E4.vx = c1->coord.t[0];
                work->field_5E4.vy = c1->coord.t[1];
                work->field_5E4.vz = c1->coord.t[2];
                delta0             = delta1;
                dist0              = dist1;
            } else {
                work->field_5E4.vx = c0->coord.t[0];
                work->field_5E4.vy = c0->coord.t[1];
                work->field_5E4.vz = c0->coord.t[2];
            }
            work->field_640 = dist0;
        }
        VectorNormalSS(&delta0, &delta0);
        work->field_634 = (ratan2(delta0.vx, delta0.vz) - work->field_556) & 0xFFF;
    }
}

/* Re-aims the two upper body coordinates at the target yaw held in
   `field_546` and folds the result back into the model root.

   With `arg1 == 0` the yaw chases the heading `Actor00400_Fn0814C` reports:
   it steps 0x18 per frame while the error is more than 0x20, and decays
   toward zero once the heading leaves +/-0x5FF. A non-zero `arg1` only
   decays, twice as fast. Each of the two coordinates then gets its pitch
   re-applied about X and a third of the yaw about Y, and the composed
   inverse of all three lands in `c4`. */
void Actor00400_Fn016A4(Actor100400* arg0, s32 arg1)
{
    SVECTOR              euler;
    SVECTOR              rot1;
    SVECTOR              rot2;
    MATRIX               t1;
    MATRIX               t2;
    MATRIX               t3;
    Actor100400Mat       ma;
    Actor100400Mat       mb;
    Actor100400Mat       mc;
    Actor100400MatWords* ia;
    Actor100400MatWords* ib;
    Actor100400MatWords* ic;
    GsCOORDINATE2*       base;
    GsCOORDINATE2*       c1;
    GsCOORDINATE2*       c2;
    GsCOORDINATE2*       c3;
    GsCOORDINATE2*       c4;
    Actor100400Work*     work;
    MATRIX*              m2;
    MATRIX*              m3;

    base = arg0->field_2C->field_8;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    work = arg0->field_1C;
    Actor00400_Fn0814C(arg0, 4, &euler, 0x600);
    if ((arg1 & 0xFF) == 0) {
        if ((u16)(euler.vy + 0x5FF) < 0xBFF) {
            if ((u32)((euler.vy - (s16)work->field_546) + 0x20) >= 0x41) {
                if ((s16)work->field_546 < euler.vy) {
                    work->field_546 = work->field_546 + 0x18;
                } else {
                    work->field_546 = work->field_546 - 0x18;
                }
            }
        } else {
            work->field_546 = work->field_546 + ((s32) - (s16)(work->field_546 * 0x10) >> 8);
        }
    } else {
        work->field_546 = work->field_546 + ((s32) - (s16)(work->field_546 * 0x10) >> 7);
    }

    m2 = &c2->coord;
    ia = &ma.ident;
    ib = &mb.ident;
    ic = &mc.ident;

    ma.ident.m00_m01 = 0x1000;
    ma.ident.m02_m10 = 0;
    ia->m11_m12      = 0x1000;
    ma.ident.m20_m21 = 0;
    ia->m22          = 0x1000;
    mb.ident.m00_m01 = 0x1000;
    mb.ident.m02_m10 = 0;
    ib->m11_m12      = 0x1000;
    mb.ident.m20_m21 = 0;
    ib->m22          = 0x1000;
    mc.ident.m00_m01 = 0x1000;
    mc.ident.m02_m10 = 0;
    ic->m11_m12      = 0x1000;
    mc.ident.m20_m21 = 0;
    ic->m22          = 0x1000;

    Gp_MtxToEuler(m2, &rot1);
    m3 = &c3->coord;
    Gp_MtxToEuler(m3, &rot2);
    RotMatrixX(rot1.vx, &ma.mat);
    RotMatrixX(rot2.vx, &mb.mat);
    Actor00400_Fn08A1C(&ma.mat, m2);
    Actor00400_Fn08A1C(&mb.mat, m3);
    Gp_UpdateCoord(c1);
    Gp_UpdateCoord(c2);
    Gp_UpdateCoord(c3);
    Actor00400_Fn03570(c2, (s16)work->field_546 / 3);
    Actor00400_Fn03570(c3, (s16)work->field_546 / 3);

    mc.ident.m00_m01 = 0x1000;
    mc.ident.m02_m10 = 0;
    ic->m11_m12      = 0x1000;
    mc.ident.m20_m21 = 0;
    ic->m22          = 0x1000;

    func_8004BFF8((s16)work->field_546 / 3, &mc.mat);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(m2, &t2);
    TransposeMatrix(m3, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc.mat);
    Actor00400_Fn08A1C(&t1, &c4->coord);
}

/* Links the actor's four collision objects and clears their record tables;
   `obj_42C` takes hit flag 0x4000 from `field_661`. */
void Actor00400_Fn019B4(Actor100400* arg0)
{
    Actor100400Work* work = arg0->field_1C;

    work->obj_35C.field_8  = &arg0->field_2C->field_8[1];
    work->obj_35C.field_C  = work->field_39C;
    work->obj_35C.field_10 = 0;
    work->obj_35C.field_12 = 0;
    work->obj_35C.field_14 = 0;
    work->obj_35C.field_18 = 0x30004;
    work->obj_35C.field_1C = 0x300;
    work->obj_35C.flags    = 1;
    Gp_LinkObj(2, &work->obj_35C);
    Gp_InitRec18Table(work->field_39C, 6, 0);
    work->obj_35C.flags |= 0x8000;

    work->obj_37C.field_8  = &arg0->field_2C->field_8[4];
    work->obj_37C.field_C  = work->field_39C;
    work->obj_37C.field_10 = 0;
    work->obj_37C.field_12 = 0;
    work->obj_37C.field_14 = 0;
    work->obj_37C.field_18 = 0x30004;
    work->obj_37C.field_1C = 0xC0;
    work->obj_37C.flags    = 1;
    Gp_LinkObj(2, &work->obj_37C);
    work->obj_37C.flags |= 0x8000;

    work->obj_4DC.field_8  = &arg0->field_2C->field_8[1];
    work->obj_4DC.field_C  = work->rec_4FC;
    work->obj_4DC.field_10 = 0;
    work->obj_4DC.field_12 = 0;
    work->obj_4DC.field_14 = 0;
    work->obj_4DC.field_18 = Gp_PackObjPair(arg0->field_20, 0);
    work->obj_4DC.field_1C = 0x480;
    work->obj_4DC.flags    = 1;
    Gp_LinkObj(3, &work->obj_4DC);
    Gp_InitRec18Table(work->rec_4FC, 3, 0);
    work->obj_4DC.flags &= 0x7FFF;

    work->obj_42C.field_8  = arg0->field_2C->field_8;
    work->obj_42C.field_C  = work->field_44C;
    work->obj_42C.field_10 = 0;
    work->obj_42C.field_12 = 0;
    work->obj_42C.field_14 = 0;
    work->obj_42C.field_18 = 0x30004;
    work->obj_42C.field_1C = 0x380;
    work->obj_42C.flags    = 1;
    Gp_LinkObj(2, &work->obj_42C);
    Gp_InitRec18Table(work->field_44C, 6, 0);
    if (work->field_661 != 0) {
        work->obj_42C.flags |= 0x4000;
    } else {
        work->obj_42C.flags &= 0xBFFF;
    }
}

/* Damage / knock-back tick: walks the six contact records, applies the hit
   the first one carries, then folds the accumulated push-back into the work
   position and the actor's coordinate. */
void Actor00400_Fn01B90(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Obj*  obj;
    GsCOORDINATE2*   coord;
    GpDeltaScratch   delta;
    s32              kind;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s32              tick;
    s32              i;

    kind            = 0;
    coord           = arg0->field_2C->field_8;
    work            = arg0->field_1C;
    obj             = arg0->field_20;
    work->field_642 = 0;
    for (i = 0; i < 6; i++) {
        if ((work->field_39C[i].field_4 & 0xFFFF0000) == 0x20000) {
            if (work->field_61C == 0) {
                work->field_642 = 1;
                work->field_65D = 1;
                dmg             = Gp_ComputeDamage(work->field_39C[i].field_4, work->field_640, 0, 0);
                amount          = dmg;
                work->field_61C = Gp_GetIdParam2(work->field_39C[i].field_4);
                if (Gp_RollEnemyChance(obj, work->field_39C[i].field_4, work->field_610) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->field_39C[i].field_4) & 0xFFFF,
                              &arg0->field_2C->field_8[work->field_664], 0, &work->field_5DC);
                work->field_644 = (amount < 0x3C) ? 5 : 2;
                switch (Gp_GetIdParam0(work->field_39C[i].field_4) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(obj);
                        break;
                    case 2:
                        Gp_SetObjFlag2(obj, work->field_39C[i].field_4, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(obj, work->field_39C[i].field_4, 0);
                        break;
                    case 4:
                        work->field_644 = 4;
                        break;
                    case 5:
                        work->field_644 = 2;
                        break;
                    case 6:
                        work->field_644 = 4;
                        break;
                    case 7:
                        kind            = 2;
                        work->field_644 = 2;
                        amount         += amount;
                        break;
                    case 8:
                        work->field_644 = 0;
                        work->field_642 = 0;
                        break;
                    case 9:
                        work->field_644 = 1;
                        break;
                }
                if ((work->field_39C[i].field_4 & 0x7F) == 0x1C && (work->field_39C[i].field_4 & 0x8000) == 0) {
                    obj->field_4C  &= 0xFE;
                    work->field_644 = 5;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[work->field_664], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[work->field_664], 2, 0);
                        break;
                }
                func_800E2C78(obj, work->field_39C[i].field_4, amount, 0);
                func_800DA6E8(&obj->field_10, amount, 0);
                obj->field_40 -= amount;
                if ((s16)obj->field_40 < 0) {
                    obj->field_40 = 0;
                }
            } else if ((Gp_GetIdParam1(work->field_39C[i].field_4) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->field_2C->field_8[1], 0, &work->field_5DC);
            }
        }
        if (work->field_642 != 0) {
            break;
        }
    }

    if (obj->field_4C & 1) {
        obj->field_4C  &= 0xFE;
        work->field_644 = 2;
    }
    if (obj->field_4C & 2) {
        obj->field_4C  &= 0xFD;
        work->field_644 = 3;
    }
    if (obj->field_4C & 0xC) {
        tmp  = Gp_TickObjFlag4(obj);
        tick = (s16)tmp;
        if (tick != 0) {
            obj->field_40 -= tmp;
            if ((s16)obj->field_40 < 0) {
                obj->field_40 = 0;
            }
            func_800DA6E8(&obj->field_10, tick, 0);
            if ((s16)obj->field_40 < 0) {
                obj->field_40 = 0;
            }
            work->field_642 = 1;
            work->field_644 = 0;
        }
        if (Gp_ObjFlag4Expired(obj) != 0) {
            obj->field_4C &= 0xF3;
        }
    }

    switch (func_800E0C10(work->field_44C, &delta, 6, 0)) {
        case 0:
            break;
        case 1:
            tmp              = delta.vx.h.hi;
            work->field_564 += tmp;
            tmp              = delta.vz.h.hi;
            work->field_568 += tmp;
            if ((delta.vx.w & 0xFFFF) != 0) {
                if (delta.vx.w > 0) {
                    work->field_564++;
                } else {
                    work->field_564--;
                }
            }
            if ((delta.vz.w & 0xFFFF) != 0) {
                if (delta.vz.w > 0) {
                    work->field_568++;
                } else {
                    work->field_568--;
                }
            }
            tmp                = delta.vx.h.hi;
            coord->coord.t[0] += tmp;
            tmp                = delta.vz.h.hi;
            coord->coord.t[2] += tmp;
            if ((delta.vx.w & 0xFFFF) != 0) {
                if (delta.vx.w > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            if ((delta.vz.w & 0xFFFF) != 0) {
                if (delta.vz.w > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
            coord->flg = 0;
            break;
        case 2:
            coord->coord.t[0] = work->field_54C;
            coord->coord.t[2] = work->field_550;
            break;
    }

    Gp_ClearRec18Occupied(work->field_39C);
    Gp_ClearRec18Occupied(work->field_44C);
    if (work->field_61C > 0) {
        work->field_61C--;
    } else {
        work->field_61C = 0;
    }
}

s16 Actor00400_Fn02154(Actor100400* arg0)
{
    Actor100400Work* work;
    s16              state;
    s16              req;

    work = arg0->field_1C;
    if (work->field_642 != 1) {
        goto fail;
    }
    req = work->field_644;
    if (req == 1) {
        state = 7;
    } else if (req == 2) {
        state = 8;
    } else if (req == 3) {
        state = 9;
    } else if (req == 4) {
        state = 8;
    } else {
        goto other;
    }
    work->field_638 = state;
    work->field_63A = 0;
    work->field_644 = 0;
    goto ok;
other:
    if (req == 5) {
        D_801153F2[1] = 1;
        Gp_ArmStateF0(1);
        work->field_650 = 10;
        work->field_644 = 0;
        return 0;
    }
    work->field_644 = 0;
    goto fail;
ok:
    return 1;
fail:
    return 0;
}

s32 Actor00400_Fn02208(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;

    work   = arg0->field_1C;
    coord  = arg0->field_2C->field_8;
    vec.vx = work->field_60C[work->field_65B].field_0 - coord->coord.t[0];
    vec.vy = work->field_60C[work->field_65B].field_2 - coord->coord.t[1];
    vec.vz = work->field_60C[work->field_65B].field_4 - coord->coord.t[2];
    if (work->field_628 != 3) {
        Actor00400_Fn088EC(arg0, 3, 0x10, 0xE);
        Gp_SetLightMode(arg0->field_20, 2);
    }
    work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 400) {
        work->field_65B = (work->field_65B + 1) & 7;
        return 1;
    } else {
        Actor00400_Fn0875C(arg0, &work->field_60C[work->field_65B], 0x2C, 0x100);
        Actor00400_Fn0762C(arg0, 0x60, work->field_556);
        return 0;
    }
}

/* The random pick spawns in both arms rather than after the `if`: jump2
   cross-jumps the identical tails, which is what leaves the 0x20010 argument
   load ahead of the `D_800678F0` store in each arm. */
void Actor00400_Fn0237C(Actor100400* arg0)
{
    GpEffWork* eff1;
    TmdObject* src1;
    TmdObject* dst1;
    GpEffWork* eff2;
    TmdObject* src2;
    TmdObject* dst2;
    GpEffWork* eff3;
    TmdObject* src3;
    TmdObject* dst3;
    GpEffWork* eff4;
    TmdObject* src4;
    TmdObject* dst4;
    GpEffWork* eff5;
    TmdObject* src5;
    TmdObject* dst5;

    D_800678F0[0] = Actor00400_D0E5B8;
    eff1          = Gp_SpawnEff(0x20010, &((TmdObject*)arg0->field_2C)->field_8[4], 0x200, NULL);
    if (eff1 != NULL) {
        src1           = (TmdObject*)arg0->field_2C;
        dst1           = (TmdObject*)eff1->field_0->extra;
        dst1->field_24 = src1->field_24;
        dst1->field_25 = src1->field_25;
        if (dst1->field_18 != NULL) {
            Tmd_ProcessStream(dst1);
            Tmd_ProcessStream(dst1);
        }
    }
    D_800678F0[0] = Actor00400_D0E970;
    eff2          = Gp_SpawnEff(0x20010, &((TmdObject*)arg0->field_2C)->field_8[11], 0x200, NULL);
    if (eff2 != NULL) {
        src2           = (TmdObject*)arg0->field_2C;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    D_800678F0[0] = Actor00400_D0ED28;
    eff3          = Gp_SpawnEff(0x20010, &((TmdObject*)arg0->field_2C)->field_8[14], 0x200, NULL);
    if (eff3 != NULL) {
        src3           = (TmdObject*)arg0->field_2C;
        dst3           = (TmdObject*)eff3->field_0->extra;
        dst3->field_24 = src3->field_24;
        dst3->field_25 = src3->field_25;
        if (dst3->field_18 != NULL) {
            Tmd_ProcessStream(dst3);
            Tmd_ProcessStream(dst3);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = Actor00400_D0F25C;
        eff4          = Gp_SpawnEff(0x20010, &((TmdObject*)arg0->field_2C)->field_8[8], 0x200, NULL);
    } else {
        D_800678F0[0] = Actor00400_D0F790;
        eff4          = Gp_SpawnEff(0x20010, &((TmdObject*)arg0->field_2C)->field_8[8], 0x200, NULL);
    }
    if (eff4 != NULL) {
        src4           = (TmdObject*)arg0->field_2C;
        dst4           = (TmdObject*)eff4->field_0->extra;
        dst4->field_24 = src4->field_24;
        dst4->field_25 = src4->field_25;
        if (dst4->field_18 != NULL) {
            Tmd_ProcessStream(dst4);
            Tmd_ProcessStream(dst4);
        }
    }
    D_800678F0[0] = Actor00400_D0FD9C;
    eff5          = Gp_SpawnEff(0x20010, &((TmdObject*)arg0->field_2C)->field_8[1], 0x200, NULL);
    if (eff5 != NULL) {
        src5           = (TmdObject*)arg0->field_2C;
        dst5           = (TmdObject*)eff5->field_0->extra;
        dst5->field_24 = src5->field_24;
        dst5->field_25 = src5->field_25;
        if (dst5->field_18 != NULL) {
            Tmd_ProcessStream(dst5);
            Tmd_ProcessStream(dst5);
        }
    }
    Gp_SpawnEff(0x60030, &((TmdObject*)arg0->field_2C)->field_8[1], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02648);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02D48);

/// Same nearest-waypoint search as `Actor00400_Fn031A4`, but the winner is
/// stored into `field_56C` and then made current: the record `field_64A` used
/// to point at is cleared to kind 0 and the new one is marked kind 1.
void Actor00400_Fn02FF8(Actor100400* arg0)
{
    Actor100400NearestScratch* scratch;
    Actor100400Work*           work;
    Actor100400Record*         record;
    u8*                        head;
    s16                        index;
    s16                        kind;
    s32                        dx;
    s32                        dz;
    s32                        distance;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x1C;
    scratch               = (Actor100400NearestScratch*)*(u8**)G_SCRATCH_HEAD;
    work                  = arg0->field_1C;
    scratch->index        = 1;
    scratch->bestIndex    = 0;
    scratch->best         = 0x7FFFFFFF;
    for (;;) {
        index  = scratch->index;
        record = (Actor100400Record*)(index * sizeof(Actor100400Record) + (u32)work->field_608);
        kind   = record->field_6;
        if (kind == -1) {
            goto done;
        }
        if ((kind != 1) || (index == work->field_64A)) {
            scratch->delta.vx = dx = work->field_5E4.vx - record->field_0;
            scratch->delta.vz = dz = work->field_5E4.vz - work->field_608[scratch->index].field_4;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->dist          = distance;
            if (distance < scratch->best) {
                work->field_56C.vx = work->field_608[scratch->index].field_0;
                work->field_56C.vz = work->field_608[scratch->index].field_4;
                scratch->best      = scratch->dist;
                scratch->bestIndex = scratch->index;
            }
        }
        scratch->index = scratch->index + 1;
    }
done:
    if (work->field_64A != scratch->bestIndex) {
        work->field_608[work->field_64A].field_6 = 0;
        work->field_64A                          = scratch->bestIndex;
        work->field_608[work->field_64A].field_6 = 1;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
}

/// Finds the nearest eligible waypoint record in `field_608` and returns its
/// XZ in `arg1`. Records with `field_6 == 1` are only considered when they are
/// the one `field_64A` points at, and the walk ends at the `-1` terminator.
void Actor00400_Fn031A4(Actor100400* arg0, SVECTOR* arg1)
{
    Actor100400NearestScratch* scratch;
    Actor100400Work*           work;
    Actor100400Record*         record;
    u8*                        head;
    s16                        index;
    s16                        kind;
    s32                        dx;
    s32                        dz;
    s32                        distance;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x1C;
    scratch               = (Actor100400NearestScratch*)*(u8**)G_SCRATCH_HEAD;
    work                  = arg0->field_1C;
    scratch->index        = 1;
    scratch->bestIndex    = 0;
    scratch->best         = 0x7FFFFFFF;
loop:
    index  = scratch->index;
    record = (Actor100400Record*)(index * sizeof(Actor100400Record) + (u32)work->field_608);
    kind   = record->field_6;
    if (kind != -1) {
        if ((kind != 1) || (index == work->field_64A)) {
            scratch->delta.vx = dx = work->field_5E4.vx - record->field_0;
            scratch->delta.vz = dz = work->field_5E4.vz - work->field_608[scratch->index].field_4;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->dist          = distance;
            if (distance < scratch->best) {
                arg1->vx           = work->field_608[scratch->index].field_0;
                arg1->vz           = work->field_608[scratch->index].field_4;
                scratch->best      = scratch->dist;
                scratch->bestIndex = scratch->index;
            }
        }
        scratch->index = scratch->index + 1;
        goto loop;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
}

void Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade)
{
    Actor100400TextQuadScratch* s;
    POLY_FT4*                   poly;

    s                 = (Actor100400TextQuadScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor100400TextQuadScratch));
    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    s->depth = RotTransPers4(corner0, corner1, corner2, corner3, &s->screen0, &s->screen1, &s->screen2, &s->screen3,
                             &s->perspective, &s->flags);
    if (s->flags >= 0) {
        poly           = Gpu_PrimCursor;
        Gpu_PrimCursor = (u8*)poly + 0x28;
        setlen(poly, 9);
        poly->code       = 0x2E;
        *(s32*)&poly->x0 = s->screen0;
        *(s32*)&poly->x1 = s->screen1;
        *(s32*)&poly->x2 = s->screen2;
        *(s32*)&poly->x3 = s->screen3;
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade >> 1, shade, shade);
        addPrim((u32*)((((u32)(s->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor100400TextQuadScratch);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn03570);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn03920);

/// Colours the actor from the second attach coordinate of its model through a
/// 0x10-byte `VECTOR` taken off `G_SCRATCH_HEAD`, then hides the root
/// coordinate while `field_65F` is set.
static __inline__ void Actor00400_UpdateColor(Actor100400* arg0, GsCOORDINATE2* coord,
                                              Actor100400Work* work, Actor100400Ctx* ctx)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    block->vz                 = coord->workm.t[2];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    Gp_UpdateActorColor(arg0->field_20, block, 0, 0);
    if (work->field_65F != 0) {
        Gp_SetObjTrans(ctx, 0, 0, 0);
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Per-frame callback for the text actor's second task. Same frame gate as
/// `Actor00400_Fn04B48`: `D_801153F4` 2 only flags the model hidden, 0 runs
/// this frame's state handler before falling through to the draw half, and 1
/// is the draw half on its own.
void Actor00400_Fn040DC(Actor100400* arg0)
{
    TaskFuncTable11  fns;
    Actor100400Work* work;
    Actor100400Ctx*  ctx;
    Actor100400Ctx*  ctx2;
    Actor100400Work* work2;
    GsCOORDINATE2*   coord;
    s32              y;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ctx   = arg0->field_2C;
    fns   = Actor00400_D0007C;
    switch (D_801153F4) {
        case 2:
            ctx->field_C |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                break;
            }
            fns.funcs[work->field_638]((Task*)arg0);
            work->flags_62C.half = work->field_4C;
            if (work->field_644 != 4) {
                work->field_660 = 1;
                Actor00400_Fn02648(arg0, 1);
            }
            y                 = coord->coord.t[1];
            coord->coord.t[1] = y + ((work->field_63E + (s16)work->field_658 - y) >> 4);
            /* fallthrough */
        case 1:
            ctx2  = arg0->field_2C;
            work2 = arg0->field_1C;
            Actor00400_UpdateColor(arg0, &ctx2->field_8[1], work2, ctx2);
            break;
    }
}

void Actor00400_Fn042C0(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Obj*  obj;
    s32              id;

    work = arg0->field_1C;
    obj  = arg0->field_20;
    if (arg0->field_2C->field_8->coord.t[1] - work->field_64E < 0x320) {
        work->field_63E = work->field_64E;
    }
    Gp_UnlinkNode(&obj->field_10);
    Gp_ReleaseStateF0Add(arg0, 0);
    obj->field_54 = NULL;
    Gp_UnlinkObj(&work->obj_35C);
    Gp_UnlinkObj(&work->obj_37C);
    Gp_UnlinkObj(&work->obj_4DC);
    Gp_UnlinkObj(&work->obj_42C);
    work->field_636 = 0;
    if (work->field_644 == 4) {
        Actor100400Work* w = arg0->field_1C;
        w->field_638       = 7;
        w->field_63A       = 0;
        return;
    }
    if (arg0->field_34 != 7) {
        Actor100400Work* w;
        id = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->field_2C->field_8),
                            (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        w            = arg0->field_1C;
        w->field_632 = 0x30;
        w->field_63C = 4;
        w->field_628 = 1;
        w->field_624 = 1;
    }
    work->field_638++;
}

void Actor00400_Fn04414(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    Actor100400Work* w2;
    s32              i;
    s32              cond;

    work = arg0->field_1C;
    w    = arg0->field_1C;
    if (w->field_624 == 1) {
        if (w->field_626 != w->field_628) {
            w->field_62A = 0;
        } else {
            w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
        }
        Actor00400_Fn08624(arg0);
        w->field_624 = 3;
    } else if (w->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        w->field_624 = 3;
        w->field_62A = 0;
    } else if (w->field_624 == 3) {
        w->field_62A++;
    }
    i = 1;
    do {
        Gp_AnimTickIndex(w, i);
        i++;
    } while (i < 0xF);
    if (arg0->field_34 != 7) {
        w2 = arg0->field_1C;
        if ((w2->flags_62C.half & 1) || (w2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond == 0) {
            return;
        }
        w2            = arg0->field_1C;
        w2->field_63C = 4;
        w2->field_632 = 0x10;
        w2->field_628 = 0xE;
        w2->field_624 = 1;
    }
    work->field_638++;
}

void Actor00400_Fn04580(Actor100400* arg0)
{
    Actor100400Work*     work = arg0->field_1C;
    Actor100400Obj*      obj  = arg0->field_20;
    Actor100400Ctx*      ctx  = arg0->field_2C;
    TaskFuncTable10      fns;
    Actor100400Mat       m;
    Actor100400MatWords* ia;
    Actor100400Work*     w;
    Actor100400Work*     w2;
    Actor100400Work*     w3;
    Actor100400Work*     work2;
    Actor100400Ctx*      ctx2;
    GsCOORDINATE2*       coord;
    MATRIX*              dst;
    s32                  i;

    fns = Actor00400_D000A8;
    switch (D_801153F4) {
        case 2:
            ctx->field_C |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                break;
            }
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->field_638]((Task*)arg0);
            Actor00400_Fn00A14(arg0);
            w = arg0->field_1C;
            if (w->field_624 == 1) {
                if (w->field_626 != w->field_628) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->field_624 = 3;
            } else if (w->field_624 == 2) {
                Actor00400_Fn085B8(arg0);
                w->field_624 = 3;
                w->field_62A = 0;
            } else if (w->field_624 == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                Gp_AnimTickIndex(w, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->field_4C;
            Actor00400_Fn016A4(arg0, (u8)work->field_665);
            w2              = arg0->field_1C;
            coord           = arg0->field_2C->field_8;
            ia              = &m.ident;
            m.ident.m00_m01 = 0x1000;
            m.ident.m02_m10 = 0;
            ia->m11_m12     = 0x1000;
            m.ident.m20_m21 = 0;
            ia->m22         = 0x1000;
            RotMatrixZ(w2->field_558, &m.mat);
            func_8004BFF8(w2->field_556, &m.mat);
            dst          = &coord->coord;
            dst->m[0][0] = m.mat.m[0][0];
            dst->m[0][1] = m.mat.m[0][1];
            dst->m[0][2] = m.mat.m[0][2];
            dst->m[1][0] = m.mat.m[1][0];
            dst->m[1][1] = m.mat.m[1][1];
            dst->m[1][2] = m.mat.m[1][2];
            dst->m[2][0] = m.mat.m[2][0];
            dst->m[2][1] = m.mat.m[2][1];
            dst->m[2][2] = m.mat.m[2][2];
            coord->flg   = 0;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->field_40 <= 0) {
                w3             = arg0->field_1C;
                arg0->field_30 = 2;
                w3->field_638  = 0;
                w3->field_63A  = 0;
            }
            /* fallthrough */
        case 1:
            ctx2  = arg0->field_2C;
            work2 = arg0->field_1C;
            Actor00400_UpdateColor(arg0, &ctx2->field_8[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->field_2C->field_8->coord.t[1], 0x80);
            ctx->field_C &= ~0x80;
            break;
    }
}

void Actor00400_Fn04900(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->field_1C;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_632 = 0x10;
        work->field_628 = 0xC;
        work->field_624 = 2;
        id              = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->field_2C->field_8),
                            (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->field_1C;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->field_1C;
            work2->field_638 = 2;
            work2->field_63A = 0;
        }
    }
}

void Actor00400_Fn04A1C(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->field_1C;
    if (work->field_642 != 0 && work->field_644 == 2) {
        id = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->field_2C->field_8),
                            (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        work2            = arg0->field_1C;
        work2->field_63C = 6;
        work2->field_632 = 0x10;
        work2->field_628 = 0xD;
        work2->field_624 = 1;
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->field_1C;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->field_1C;
            work2->field_638 = 2;
            work2->field_63A = 0;
        }
    }
}

/// Per-frame callback for the main actor task. `D_801153F4` gates the frame:
/// 2 only flags the model hidden, 0 runs this frame's state handler before
/// falling through to the draw half, and 1 is the draw half on its own.
void Actor00400_Fn04B48(Actor100400* arg0)
{
    TaskFuncTable10  fns;
    Actor100400Work* work;
    Actor100400Ctx*  ctx;
    Actor100400Ctx*  ctx2;
    Actor100400Work* work2;
    GsCOORDINATE2*   coord;

    work = arg0->field_1C;
    ctx  = arg0->field_2C;
    fns  = Actor00400_D000D0;
    switch (D_801153F4) {
        case 2:
            ctx->field_C |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                break;
            }
            fns.funcs[work->field_638]((Task*)arg0);
            work->flags_62C.half = work->field_4C;
            /* fallthrough */
        case 1:
            ctx2  = arg0->field_2C;
            work2 = arg0->field_1C;
            coord = &ctx2->field_8[1];
            Actor00400_UpdateColor(arg0, coord, work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->field_2C->field_8->coord.t[1], (u8)work->field_648);
            break;
    }
}

void Actor00400_Fn04CF8(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Obj*  obj;
    s32              id;
    Actor100400Work* w;

    work          = arg0->field_1C;
    obj           = arg0->field_20;
    obj->field_54 = NULL;
    Gp_UnlinkObj(&work->obj_42C);
    Gp_UnlinkObj(&work->obj_35C);
    Gp_UnlinkObj(&work->obj_37C);
    Gp_UnlinkObj(&work->obj_4DC);
    Gp_UnlinkNode(&obj->field_10);
    Gp_ReleaseStateF0Add(arg0, 0);
    work->field_648 = 0x80;
    if (work->field_644 == 4) {
        w            = arg0->field_1C;
        w->field_638 = 6;
        w->field_63A = 0;
        return;
    }
    w               = arg0->field_1C;
    w->field_63C    = 8;
    w->field_632    = 0x10;
    w->field_628    = 0xF;
    w->field_624    = 1;
    work->field_636 = 0;
    id              = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
    SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->field_2C->field_8),
                        (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    work->field_638++;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04E18);

static inline void Actor00400_TurnToward(Actor100400* arg0, SVECTOR* target, s32 step, s32 range)
{
    Actor100400Work* work = arg0->field_1C;
    GsCOORDINATE2*   coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords      = arg0->field_2C->field_8;
    coords->flg = 0;
    vec.vx      = target->vx - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_556;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > range) {
        work->field_556 = angle - step;
    } else if (diff < -range) {
        work->field_556 = angle + step;
    }
}

/// Spawns the 16-way ring of `0x01202148` effects the boss uses when it lands
/// and when it is knocked down: one per 1/16 turn, at the height `field_64E`
/// gives above the root coordinate.
static inline void Actor00400_SpawnRing(Actor100400* arg0, Actor100400Work* work, GsCOORDINATE2* coord)
{
    GsCOORDINATE2* coord2;
    SVECTOR        vec;
    s32            i;
    s16            y;

    i      = 0;
    y      = work->field_64E - coord->coord.t[1] + 0xFA;
    coord2 = arg0->field_2C->field_8;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
}

void Actor00400_Fn05320(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w1;
    Actor100400Work* w2;
    Actor100400Work* w4;
    Actor100400Work* w5;
    GsCOORDINATE2*   coord;
    s8               armed;
    s32              cond;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              sound3;
    s32              pan3;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    work->field_636++;
    if (work->field_636 >= 0x14) {
        w1    = arg0->field_1C;
        armed = 0;
        if (w1->field_640 < 0xDAC && (u32)(w1->field_634 - 0x600) >= 0x400U) {
            D_801153F2[1] = 1;
            Gp_ArmStateF0(1);
            armed         = 1;
            w2            = arg0->field_1C;
            w2->field_638 = 4;
            w2->field_63A = 0;
        }
        if (armed) {
            return;
        }
        Actor00400_TurnToward(arg0, (SVECTOR*)&work->field_60C[work->field_65B & 7], 0x20, 0x30);
    }
    if (work->field_636 == 8) {
        work->field_660 = 0;
        sound           = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040007;
        pan             = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
    if (work->field_636 == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->field_636 == 0x14) {
        sound2 = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040004;
        pan2   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
    w4 = arg0->field_1C;
    if ((w4->flags_62C.half & 1) || (w4->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        Actor00400_SpawnRing(arg0, work, coord);
        sound3 = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040008;
        pan3   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound3, pan3, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
        w5              = arg0->field_1C;
        w5->field_63C   = 4;
        w5->field_632   = 0x10;
        w5->field_628   = 1;
        w5->field_624   = 1;
        work->field_63A++;
    }
}

void Actor00400_Fn05728(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* state;
    Actor100400Work* state2;
    Actor100400Work* state3;
    u32              random;
    s16              next;
    u8               idx;
    u8               idx2;

    work = arg0->field_1C;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (work->field_640 < 0x2710 && (u32)(work->field_634 - 0xC0) >= 0xE81U) {
            random      = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = random;
            if ((random >> 16) & 1) {
                work2                              = arg0->field_1C;
                work2->field_638                   = 0xA;
                work2->field_63A                   = 0;
                work2->field_614[work2->field_65A] = work2->field_638;
                next                               = 4;
                if (work2->field_614[0] == work2->field_614[1] &&
                    work2->field_614[0] == work2->field_614[2] && work2->field_614[0] == 0xA) {
                    state                              = arg0->field_1C;
                    state->field_638                   = next;
                    state->field_63A                   = 0;
                    work2->field_614[work2->field_65A] = next;
                    work2->field_64C                   = 0x5A;
                }
                idx              = work2->field_65A + 1;
                work2->field_65A = idx;
                if (idx >= 3U) {
                    work2->field_65A = 0;
                }
            } else {
                work->field_64C   = 0x5A;
                state2            = arg0->field_1C;
                state2->field_638 = 4;
                state2->field_63A = 0;
            }
        } else {
            state3                           = arg0->field_1C;
            state3->field_638                = 4;
            state3->field_63A                = 0;
            work->field_614[work->field_65A] = work->field_638;
            idx2                             = work->field_65A + 1;
            work->field_65A                  = idx2;
            if (idx2 >= 3U) {
                work->field_65A = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn058C4);

void Actor00400_Fn05D00(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              i;
    s16              y;

    coord           = arg0->field_2C->field_8;
    work            = arg0->field_1C;
    work->field_660 = 1;
    work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
    Gp_SetLightMode(arg0->field_20, 2);
    if (work->field_628 != 3) {
        i      = 0;
        y      = work->field_64E - coord->coord.t[1] + 0xFA;
        coord2 = arg0->field_2C->field_8;
        do {
            vec.vx = (u32)rsin(i << 8) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 8) >> 3;
            Gp_SpawnEff(D_80115738, coord2, 0x01202148, &vec);
            i++;
        } while (i < 16);
        sound = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x40040008;
        pan   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        w            = arg0->field_1C;
        w->field_63C = 4;
        w->field_632 = 0x10;
        w->field_628 = 1;
        w->field_624 = 1;
    } else {
        work->field_63A++;
    }
    work->field_63A++;
}

void Actor00400_Fn05EA4(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              id;
    s32              pan;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    Actor00400_Fn02FF8(arg0);
    vec.vx          = work->field_56C.vx - coord->coord.t[0];
    vec.vy          = 0;
    vec.vz          = work->field_56C.vz - coord->coord.t[2];
    work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 800 && work->field_64C == 0) {
        Actor100400Work* w;
        Gp_SetLightMode(arg0->field_20, 0);
        work->field_636 = 0;
        w               = arg0->field_1C;
        w->field_638    = 3;
        w->field_63A    = 0;
        return;
    }
    if (work->field_628 != 3) {
        Actor100400Work* w;
        w            = arg0->field_1C;
        w->field_63C = 10;
        w->field_632 = 0x10;
        w->field_628 = 3;
        w->field_624 = 1;
    }
    Actor00400_TurnToward(arg0, &work->field_56C, 0x30, 0x100);
    Actor00400_Fn0762C(arg0, 0x60, work->field_556);
    if (!(work->field_630 & 0xF)) {
        id  = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x40040001;
        pan = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
}

void Actor00400_Fn060CC(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->field_1C;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_632 = 0x20;
        work->field_628 = 0xA;
        work->field_624 = 2;
        id              = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->field_2C->field_8),
                            (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->field_1C;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->field_1C;
            work2->field_638 = 2;
            work2->field_63A = 0;
        }
    }
}

void Actor00400_Fn061E8(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              i;
    s16              y;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_63C = 3;
    work->field_632 = 0x10;
    work->field_628 = 0xB;
    work->field_624 = 1;
    sound           = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
    i               = 0;
    pan             = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    y      = work->field_64E - coord->coord.t[1] + 0xFA;
    coord2 = arg0->field_2C->field_8;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
    sound2 = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040008;
    pan2   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
    SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    work->field_63A++;
}

void Actor00400_Fn06380(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;

    work = arg0->field_1C;
    work->field_636++;
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x10, 0x20);
    if (work->field_636 == 1) {
        work->field_646 = 0x18;
    }
    if (work->field_636 >= 0x24) {
        work->field_636 = 0;
        w               = arg0->field_1C;
        w->field_63C    = 8;
        w->field_632    = 0x10;
        w->field_628    = 7;
        w->field_624    = 1;
        work->field_63A++;
    }
}

/// Spawns the marker task from `Actor00400_D16028[1]` and hands it a 0x64-byte
/// work block: coordinate 5 gives the task's root translation, and the view
/// space span from coordinate 4's base to the same point raised by `height` -
/// the per-enemy value `Actor00400_D1609C` selects - is stored in the work.
static inline void Actor00400_SpawnMarker(Actor100400* arg0)
{
    Actor100400Params*     params;
    Actor100400MarkerWork* marker;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         origin;
    GsCOORDINATE2*         span;
    GsCOORDINATE2*         dst;
    Task*                  task;
    SVECTOR                pos;
    SVECTOR                base;
    SVECTOR                tip;
    u16                    height;

    params = arg0->field_20->field_3C;
    if (params != NULL) {
        height = Actor00400_D1609C[params->field_F & 7];
    } else {
        height = 0xBE;
    }
    coords = arg0->field_2C->field_8;
    origin = &coords[5];
    span   = &coords[4];
    task   = Task_SpawnFromTable(&Actor00400_D16028, 1, 0, 0);
    if (task != NULL) {
        marker = Mem_Calloc(sizeof(Actor100400MarkerWork), false);
        if (marker == NULL) {
            Task_Kill(task);
        } else {
            base.vx = 0;
            base.vy = 0;
            base.vz = 0;
            tip.vx  = 0;
            tip.vy  = 0;
            tip.vz  = height;
            ActorCoordToView(span, &base);
            ActorCoordToView(span, &tip);
            task->idMap = (TaskIdMap*)marker;
            dst         = ((Actor100400Ctx*)task->extra)->field_8;
            pos.vx      = 0;
            pos.vy      = 0;
            pos.vz      = 0;
            ActorCoordToView(origin, &pos);
            dst->coord.t[0]  = pos.vx;
            dst->coord.t[1]  = pos.vy;
            dst->coord.t[2]  = pos.vz;
            marker->field_58 = tip.vx - base.vx;
            marker->field_5A = tip.vy - base.vy;
            marker->field_5C = tip.vz - base.vz;
        }
    }
}

void Actor00400_Fn064B0(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              pan;
    s32              cond;

    work = arg0->field_1C;
    work->field_636++;
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x10, 0x20);
    if (work->field_636 == 0x29) {
        id  = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x4004000A;
        pan = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
    if (work->field_636 == 0x2B) {
        Actor00400_SpawnMarker(arg0);
    }
    work2 = arg0->field_1C;
    if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = arg0->field_1C;
        work2->field_638 = 2;
        work2->field_63A = 0;
    }
}

void Actor00400_Fn06798(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;

    work   = arg0->field_1C;
    coord  = arg0->field_2C->field_8;
    vec.vx = work->field_60C[work->field_65B].field_0 - coord->coord.t[0];
    vec.vy = work->field_60C[work->field_65B].field_2 - coord->coord.t[1];
    vec.vz = work->field_60C[work->field_65B].field_4 - coord->coord.t[2];

    work->field_63E = work->field_60C[work->field_65B].field_2;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 1000) {
        work->field_65B = (work->field_65B + 1) & 7;
        return;
    }
    if (work->field_628 != 3) {
        Actor100400Work* w;
        Actor100400Work* a;
        s32              i;

        w            = arg0->field_1C;
        w->field_63C = 10;
        w->field_632 = 0x10;
        w->field_628 = 3;
        w->field_624 = 1;

        a = arg0->field_1C;
        if (a->field_624 == 1) {
            if (a->field_626 != a->field_628) {
                a->field_62A = 0;
            } else {
                a->field_62A = Actor00400_Fn086FC(arg0, a->field_62A);
            }
            Actor00400_Fn08624(arg0);
            a->field_624 = 3;
        } else if (a->field_624 == 2) {
            Actor00400_Fn085B8(arg0);
            a->field_624 = 3;
            a->field_62A = 0;
        } else if (a->field_624 == 3) {
            a->field_62A++;
        }
        i = 1;
        do {
            Gp_AnimTickIndex(a, i);
            i++;
        } while (i < 0xF);
    }
    Actor00400_TurnToward(arg0, (SVECTOR*)&work->field_60C[work->field_65B], 0x2C, 0x100);
    Actor00400_Fn0762C(arg0, 0x60, work->field_556);
    Gp_SetLightMode(arg0->field_20, 2);
}

void Actor00400_Fn06A44(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    work->field_636++;
    if (work->field_636 == 1) {
        SndEvt_EnqueueType6((((u16)arg0->field_20->field_8 >> 12) << 8) | 0x54220005, 0, 0);
    }
    if (work->field_636 == 8) {
        work->field_660 = 0;
        id              = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x40040007;
        pan             = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
    if (work->field_636 == 0x14) {
        work->field_646 = 0x18;
        w               = arg0->field_1C;
        w->field_63C    = 4;
        w->field_632    = 0x10;
        w->field_628    = 7;
        w->field_624    = 1;
        work->field_636 = 0;
        work->field_63A++;
    }
}

/// Per-frame callback for the text actor's third task, with the same
/// `D_801153F4` frame gate as `Actor00400_Fn04B48`: 2 only flags the model
/// hidden, 0 runs this frame's state handler and rebuilds the root rotation
/// before falling through to the draw half, and 1 is the draw half on its own.
void Actor00400_Fn06B7C(Actor100400* arg0)
{
    Actor100400Work*     work                    = arg0->field_1C;
    Actor100400Obj*      obj                     = arg0->field_20;
    Actor100400Ctx*      ctx                     = arg0->field_2C;
    void                 (*fns[2])(Actor100400*) = { Actor00400_Fn08A88, Actor00400_Fn08B40 };
    Actor100400Mat       m;
    Actor100400MatWords* ia;
    Actor100400Work*     w;
    Actor100400Work*     w2;
    Actor100400Work*     w3;
    Actor100400Work*     work2;
    Actor100400Ctx*      ctx2;
    GsCOORDINATE2*       coord;
    MATRIX*              dst;
    s32                  i;

    switch (D_801153F4) {
        case 2:
            ctx->field_C |= 0x80;
            break;
        case 0:
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns[work->field_638](arg0);
            w = arg0->field_1C;
            if (w->field_624 == 1) {
                if (w->field_626 != w->field_628) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->field_624 = 3;
            } else if (w->field_624 == 2) {
                Actor00400_Fn085B8(arg0);
                w->field_624 = 3;
                w->field_62A = 0;
            } else if (w->field_624 == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                Gp_AnimTickIndex(w, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->field_4C;
            w2                   = arg0->field_1C;
            coord                = arg0->field_2C->field_8;
            ia                   = &m.ident;
            m.ident.m00_m01      = 0x1000;
            m.ident.m02_m10      = 0;
            ia->m11_m12          = 0x1000;
            m.ident.m20_m21      = 0;
            ia->m22              = 0x1000;
            RotMatrixZ(w2->field_558, &m.mat);
            func_8004BFF8(w2->field_556, &m.mat);
            dst          = &coord->coord;
            dst->m[0][0] = m.mat.m[0][0];
            dst->m[0][1] = m.mat.m[0][1];
            dst->m[0][2] = m.mat.m[0][2];
            dst->m[1][0] = m.mat.m[1][0];
            dst->m[1][1] = m.mat.m[1][1];
            dst->m[1][2] = m.mat.m[1][2];
            dst->m[2][0] = m.mat.m[2][0];
            dst->m[2][1] = m.mat.m[2][1];
            dst->m[2][2] = m.mat.m[2][2];
            coord->flg   = 0;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->field_40 <= 0) {
                w3             = arg0->field_1C;
                arg0->field_30 = 2;
                w3->field_638  = 0;
                w3->field_63A  = 0;
            }
            /* fallthrough */
        case 1:
            ctx2  = arg0->field_2C;
            work2 = arg0->field_1C;
            Actor00400_UpdateColor(arg0, &ctx2->field_8[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->field_2C->field_8->coord.t[1], 0x80);
            ctx->field_C &= ~0x80;
            break;
    }
}

static inline s32 Actor00400_ConsumeStateRequest(Actor100400Work* work)
{
    s16 req;
    s32 state;

    state = work->field_642;
    if (state != 1) {
        return 0;
    }
    req = work->field_644;
    if (req == 1)
        goto set;
    if (req == 2)
        goto set;
    if (req == 3)
        goto set;
    if (req != 4)
        goto other;
set:
    /* The do/while(0) is load-bearing: flow.c weights REG_N_REFS by loop
       depth, and the two extra references it buys `work` are what let the
       pointer outrank `req` in global.c's allocation order. */
    do {
        work->field_638 = state;
        work->field_63A = 0;
    } while (0);
other:
    work->field_644 = 0;
    return 1;
}

void Actor00400_Fn06EA4(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              cond;

    work = arg0->field_1C;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        work = arg0->field_1C;
        if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->field_1C;
            work2->field_63C = 8;
            work2->field_632 = 4;
            work2->field_628 = 0xF;
            work2->field_624 = 1;
        }
    }
}

void Actor00400_Fn06F64(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->field_1C;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_63C = 2;
        work->field_632 = 0x10;
        work->field_628 = 0x13;
        work->field_624 = 1;
        id              = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->field_2C->field_8),
                            (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        return;
    }
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        work = arg0->field_1C;
        if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->field_1C;
            work2->field_638 = 0;
            work2->field_63A = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn070C0);

void Actor00400_Fn07400(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* work3;
    s32              phase;
    s32              cond;

    work = arg0->field_1C;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        phase           = (u16)work->field_636 + 1;
        work->field_636 = phase;
        work->field_63E = work->field_658 + ((u16)work->field_64E + ((rsin(phase << 16 >> 10) * 0x10) >> 10));
        work2           = arg0->field_1C;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work3            = arg0->field_1C;
            work3->field_63C = 8;
            work3->field_632 = 2;
            work3->field_628 = 0x10;
            work3->field_624 = 1;
        }
    }
}

void Actor00400_Fn07518(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              phase;

    work = arg0->field_1C;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_63C = 2;
        work->field_632 = 0x10;
        work->field_628 = 0x12;
        work->field_624 = 1;
    }
    if (Actor00400_ConsumeStateRequest(arg0->field_1C) == 0) {
        phase           = (u16)work->field_636 + 1;
        work->field_636 = phase;
        work->field_63E = work->field_658 + ((u16)work->field_64E + ((rsin(phase << 16 >> 9) * 0x10) >> 9));
        if (work->field_636 >= 0x79) {
            work2            = arg0->field_1C;
            work2->field_638 = 0;
            work2->field_63A = 0;
        }
    }
}

/* Steps the actor's root coordinate along its heading in the XZ plane and
   marks it dirty. Same body as func_actor_206100_8014EA8C in
   src/actors/actor_206100/actor_206100.c. */
void Actor00400_Fn0762C(Actor100400* arg0, s16 arg1, s16 arg2)
{
    arg0->field_2C->field_8->coord.t[0] += ((rsin(arg2) << 4) * arg1) >> 16;
    arg0->field_2C->field_8->coord.t[2] += ((rcos(arg2) << 4) * arg1) >> 16;
    arg0->field_2C->field_8->flg         = 0;
}

void Actor00400_Fn0A2F4(Task* arg0);
void Actor00400_Fn0A364(Task* arg0);

/// Two-state dispatcher over a handler table built on the stack.
void Actor00400_Fn076E8(Task* task)
{
    TaskFunc funcs[2] = {
        Actor00400_Fn0A2F4,
        Actor00400_Fn0A364,
    };

    funcs[task->state](task);
}

/// Draws two LCG values into the work's `field_62E`/`field_630`, resets the
/// state counters and copies the root coordinate's `t[1]` into `field_63E`.
void Actor00400_Fn07738(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;
    GsCOORDINATE2*   coord;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    Gp_IncStateF0Ref(0);
    Gp_LcgState                  = Gp_LcgState * 5 + 0x71357911;
    work->flags_62C.hi.field_62E = Gp_LcgState >> 16;
    Gp_LcgState                  = Gp_LcgState * 5 + 0x71357911;
    work->field_630              = Gp_LcgState >> 16;
    state                        = arg0->field_1C;
    state->field_632             = 0x10;
    state->field_628             = 1;
    state->field_624             = 2;
    state2                       = arg0->field_1C;
    state2->field_638            = 1;
    state2->field_63A            = 0;
    work->field_63E              = coord->coord.t[1];
}

void Actor00400_Fn077F4(Actor100400* arg0)
{
    Actor100400Work* work;
    TaskFuncTable4   fns;
    Actor100400Work* work2;

    work = arg0->field_1C;
    fns  = Actor00400_D00134;
    if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
        D_801153F2[1] = 1;
        Gp_ArmStateF0(1);
    } else if (D_801153F2[1] != 0) {
        work2            = arg0->field_1C;
        work2->field_638 = 4;
        work2->field_63A = 0;
    } else {
        fns.funcs[(s16)work->field_63A]((Task*)arg0);
    }
}

void Actor00400_Fn0962C(Actor100400* arg0);
void Actor00400_Fn058C4(Actor100400* arg0);

void Actor00400_Fn078C8(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0962C,
        Actor00400_Fn058C4,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn0793C(Actor100400* arg0)
{
    Actor100400Work* work;
    TaskFuncTable3   fns;

    work = arg0->field_1C;
    fns  = Actor00400_D00144;
    fns.funcs[(s16)work->field_63A]((Task*)arg0);
}

void Actor00400_Fn07998(void)
{
}

void Actor00400_Fn079A0(void)
{
}

void Actor00400_Fn079A8(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn09714,
        Actor00400_Fn060CC,
    };

    states[(s16)work->field_63A](arg0);
}

static inline s32 Actor00400_TakeStateRequest(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_642 == 1) {
        switch (work->field_644) {
            case 2:
                work->field_638 = 8;
                work->field_63A = 0;
                work->field_644 = 0;
                return 1;
            case 3:
                work->field_638 = 9;
                work->field_63A = 0;
                work->field_644 = 0;
                return 1;
        }
    }
    work->field_644 = 0;
    return 0;
}

void Actor00400_Fn079FC(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn061E8,
        Actor00400_Fn097C8,
    };
    s16 taken;

    taken = Actor00400_TakeStateRequest(arg0);
    if (taken == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn07ABC(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn098A8,
        Actor00400_Fn09924,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn07B10(Actor100400* arg0)
{
    Actor100400Work* work;
    TaskFuncTable3   fns;

    work = arg0->field_1C;
    fns  = Actor00400_D00150;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        fns.funcs[(s16)work->field_63A]((Task*)arg0);
    }
}

void Actor00400_Fn07B98(Actor100400* arg0)
{
    Actor100400Obj*  obj;
    Actor100400Work* work;
    TaskFuncTable3   fns;

    obj             = arg0->field_20;
    work            = arg0->field_1C;
    fns             = Actor00400_D0015C;
    work->field_660 = 1;
    obj->field_14   = 1;
    fns.funcs[(s16)work->field_63A]((Task*)arg0);
}

void Actor00400_Fn07C04(Actor100400* arg0)
{
    Actor100400Obj*  obj;
    Actor100400Work* work;
    TaskFuncTable4   fns;
    Actor100400Work* work2;

    obj  = arg0->field_20;
    work = arg0->field_1C;
    fns  = Actor00400_D00168;
    if (GameFlag_GetNibble(0xBC) != 0) {
        work2            = arg0->field_1C;
        work2->field_638 = 0xB;
        work2->field_63A = 0;
    } else {
        work->field_660 = 1;
        obj->field_14   = 1;
        fns.funcs[(s16)work->field_63A]((Task*)arg0);
    }
}

void Actor00400_Fn07CC4(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    s32              i;

    work = arg0->field_1C;
    work->field_636++;
    w = arg0->field_1C;
    if (w->field_624 == 1) {
        if (w->field_626 != w->field_628) {
            w->field_62A = 0;
        } else {
            w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
        }
        Actor00400_Fn08624(arg0);
        w->field_624 = 3;
    } else if (w->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        w->field_624 = 3;
        w->field_62A = 0;
    } else if (w->field_624 == 3) {
        w->field_62A++;
    }
    i = 1;
    do {
        Gp_AnimTickIndex(w, i);
        i++;
    } while (i < 0xF);
    if (work->field_636 >= 0x3C) {
        work->field_638++;
    }
}

void Actor00400_Fn07DE0(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    Gp_SetLightMode(arg0->field_20, 1);
    work->field_636 = 0;
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn07E20(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Ctx*  ctx;

    work = arg0->field_1C;
    ctx  = arg0->field_2C;
    if (++work->field_636 >= 0x18) {
        ctx->field_C   |= 2;
        work->field_636 = 0;
        work->field_638++;
    }
}

void Actor00400_Fn07E74(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (++work->field_636 == 0x10) {
        Gp_SetLightMode(arg0->field_20, 2);
    }
    if (work->field_636 > 0x20) {
        work->field_638++;
    }
}

void Actor00400_Fn07EE8(Actor100400* arg0)
{
    Actor100400Ctx*  ctx;
    Actor100400Work* work;

    ctx             = arg0->field_2C;
    ctx->field_C   |= 0x80;
    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn07F18(Actor100400* arg0)
{
    Actor100400Ctx*  ctx;
    Actor100400Work* work;

    ctx             = arg0->field_2C;
    work            = arg0->field_1C;
    ctx->field_C   |= 0x80;
    work->field_636 = 0;
    work->field_638++;
}

void Actor00400_Fn07F44(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (++work->field_636 >= 2) {
        work->field_638++;
    }
}

void Actor00400_Fn07F88(Actor100400* arg0)
{
    TmdObject*       model;
    Actor100400Work* work;

    model = (TmdObject*)arg0->field_2C;
    work  = arg0->field_1C;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    Actor00400_Fn0237C(arg0);
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn07FEC(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn08004(Actor100400* arg0)
{
    TaskFuncTable3 sp;

    sp = Actor00400_D0002C;
    sp.funcs[arg0->field_30]((Task*)arg0);
}
