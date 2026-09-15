#include "common.h"

#include "actors/actor_100400.h"
#include "main/gameflag.h"
#include "main/task.h"

extern u32 Gp_LcgState;
extern u8  D_801153F2[2];
void       Gp_ArmStateF0(s32 active);
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
void       Actor00400_Fn0237C(Actor100400* arg0);
void       Actor00400_Fn02FF8(Actor100400* arg0);
void       Gp_IncStateF0Ref(s32 arg0);
s32        Gp_GetObjPan(GsCOORDINATE2* arg0);
s32        Gp_GetObjDepth(GsCOORDINATE2* arg0);
s32        SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void       Gp_UnlinkObj(void* node);
void       Gp_LinkObj(s32 arg0, GpObj* arg1);
void       Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2);
s32        Gp_PackObjPair(Actor100400Obj* arg0, s32 arg1);
void       Gp_UnlinkNode(void* node);
void       Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void       Actor00400_Fn060CC(Actor100400* arg0);
void       Actor00400_Fn09714(Actor100400* arg0);
void       Actor00400_Fn06EA4(Actor100400* arg0);
void       Actor00400_Fn08ADC(Actor100400* arg0);
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

extern GsCOORDINATE2    Gfx_ViewCoord;
extern MATRIX           Gfx_ViewWorldMtx;
extern TaskFuncTable3   Actor00400_D0002C;
extern TaskFuncTable4   Actor00400_D00134;
extern TaskFuncTable3   Actor00400_D00144;
extern TaskFuncTable3   Actor00400_D00150;
extern TaskFuncTable3   Actor00400_D0015C;
extern TaskFuncTable4   Actor00400_D00168;
extern Actor100400Stats Actor00400_D0FDC8;
extern byte             Actor00400_D1604C[];

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn001AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn005DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00A14);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00C84);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn01454);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn016A4);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02208);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0237C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02648);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02D48);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02FF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn031A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn03318);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn03570);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn03920);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn040DC);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04414);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04580);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04B48);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05320);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05728);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05EA4);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06380);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn064B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06798);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06B7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06EA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06F64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn070C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07400);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07518);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn079FC);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07CC4);

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
