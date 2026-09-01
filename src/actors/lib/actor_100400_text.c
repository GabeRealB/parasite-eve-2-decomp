#include "common.h"

#include "actors/actor_100400.h"

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn001AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn005DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00A14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00B48);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00C84);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn00E3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn012B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn01454);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn016A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn019B4);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn02154);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn042C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04414);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04580);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04900);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04A1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04B48);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04CF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn04E18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05320);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05728);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn058C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05D00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn05EA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn060CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn061E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06380);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn064B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06798);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06A44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06B7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06EA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn06F64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn070C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07400);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07518);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0762C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn076E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07738);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn077F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn078C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0793C);

void Actor00400_Fn07998(void)
{
}

void Actor00400_Fn079A0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn079A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn079FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07ABC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07B10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07B98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07C04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07CC4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07DE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07E20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07E74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07EE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07F18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07F44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn07F88);

void Actor00400_Fn07FEC(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08004);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0805C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0814C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0824C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08354);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08464);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn085B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08624);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn086FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0875C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08814);

void Actor00400_Fn088EC(Actor100400* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = arg3;
    work->field_632 = arg2;
    work->field_628 = arg1;
    work->field_624 = 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08908);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08948);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn089C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08A1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08A88);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08ADC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08B40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08B94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08C54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08D70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08DFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08E50);

void Actor00400_Fn08FB0(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08FC8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn08FF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09038);

void Actor00400_Fn0909C(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn090B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09124);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn091F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn092D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09348);

void Actor00400_Fn093BC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn093C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09418);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0946C);

void Actor00400_Fn094C0(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_65B = 0;
    work->field_63A = work->field_63A + 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn094DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn095D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0962C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn096C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09714);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn097C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn098A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09924);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09A1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09A48);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09A8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09AE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09B44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09B74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09BDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09C04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09C84);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09CCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09D3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09D98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09E70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09F18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn09FDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A034);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A08C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A190);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A28C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A2F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A364);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A3D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A414);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A468);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A4BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A510);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A57C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A5B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A680);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A6B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A704);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A760);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A7F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A82C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A880);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A940);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0A9F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text", Actor00400_Fn0AA40);
