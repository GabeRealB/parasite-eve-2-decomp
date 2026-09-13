#include "common.h"

#include "actors/actor_100300.h"
#include "actors/actors_shared_80132074.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/wipsys.h"

s32 SndEvt_EnqueueType6(s32 sound, s32 pan, s32 depth);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8  D_801153F4;
extern s32 D_80115728;
extern s32 D_8011573C;
extern s32 Gp_LcgState;

void Actor00300_Fn00078(GsCOORDINATE2* arg0, s32 arg1);
void Actor00300_Fn04528(Actor100300* arg0);
void Actor00300_Fn00E54(Actor100300* arg0);
void Actor00300_Fn01678(Actor100300* arg0);
void Actor00300_Fn019C0(Actor100300* arg0);
void Actor00300_Fn01F9C(Actor100300* arg0);
void Actor00300_Fn02620(Actor100300* arg0);
void Actor00300_Fn028D0(Actor100300* arg0);
void Actor00300_Fn02CE8(Actor100300* arg0);
void Actor00300_Fn030B8(Actor100300* arg0);
void Actor00300_Fn032BC(Actor100300* arg0);
void Actor00300_Fn0340C(Actor100300* arg0);
void Actor00300_Fn03A1C(Actor100300* arg0);
void Actor00300_Fn04A2C(Actor100300* arg0);
void Actor00300_Fn04C20(Actor100300* arg0);
void Actor00300_Fn04D28(Actor100300* arg0);
void Actor00300_Fn04E30(Actor100300* arg0);
void Actor00300_Fn04ED4(Actor100300* arg0);
void Actor00300_Fn04FB0(Actor100300* arg0);
void Actor00300_Fn05008(Actor100300* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00078);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn005D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00970);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00E54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01678);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn019C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01D60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01F9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn02620);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn028D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn02CE8);

void Actor00300_Fn030B8(Actor100300* arg0)
{
    SVECTOR          sp10;
    SVECTOR          sp18;
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    s16              timer;
    s16              state;
    s32              random;
    s32              angle;
    s32              sound;
    s32              pan;
    u32              effectRandom;
    u32              nextRandom;

    work  = arg0->field_1C;
    state = work->field_686;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_686 = 1;
            work->field_67C = 0;
            work->field_67A = 0;
            work->field_688 = 0;
            work->field_66E = 3;
            return;
        case 1:
            if (Gp_State1C->field_4 == 0) {
                effectRandom = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState  = (s32)effectRandom;
                if (!((effectRandom >> 0x10) & 3)) {
                    random      = (effectRandom * 5) + 0x71357911;
                    Gp_LcgState = random;
                    angle       = ((u32)random >> 0x10) & 0xF80;
                    memset(&sp18, 0, sizeof(sp18));
                    sp18.vx = (u32)(rcos(angle) * 5) >> 5;
                    sp18.vz = (u32)(rsin(angle) * 5) >> 5;
                    sp10    = sp18;
                    Gp_SpawnEff(D_80115728, coord, 0x20103200, &sp10);
                }
            }
            timer           = (u16)work->field_688 + 1;
            work->field_688 = timer;
            if (timer >= 0x5B) {
                work->field_684 = 1;
                work->field_688 = 0;
                work->field_686 = 0;
                nextRandom      = (Gp_LcgState * 5) + 0x71357911;
                work->field_666 = (u16)(work->field_666 + 5);
                work->field_688 = (nextRandom >> 0x10) & 0x1F;
                Gp_LcgState     = (s32)nextRandom;
                sound           = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4003000B;
                pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            return;
    }
}

void Actor00300_Fn032BC(Actor100300* arg0)
{
    Actor100300Work*       work;
    GsCOORDINATE2*         coord;
    Actor100300RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor100300RotScratch*)((*(u32*)0x1F8003FC) -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_680;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_67E = ang;
    if (adiff < 0x800) {
        step = work->field_67C;
        if (step >= adiff) {
            work->field_67E = want;
        } else {
            next = work->field_67E;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_67E = next;
        }
    } else {
        step = work->field_67C;
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
        work->field_67E = work->field_680;
        goto done;
    turn:
        wrapStep = work->field_67C;
        cur      = work->field_67E;
        if (diff > 0) {
            work->field_67E = cur - wrapStep;
        } else {
            work->field_67E = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_67E;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    (*(u32*)0x1F8003FC) += 0x18;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn0340C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn03618);

void Actor00300_Fn03A1C(Actor100300* arg0)
{
    Actor100300Work* work;
    GpAnimRec*       rec;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    s32              pan2;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_696 & 0x20)) {
            sound = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40030001;
            pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        if (!(rec->field_3 & 0x10) && (work->field_696 & 0x10)) {
            sound = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40030002;
            pan2  = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(sound, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        work->field_696 = (u16)(rec->field_3 & 0x30);
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn03B70);

static __inline__ void Actor00300_UpdateTransform(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Obj2C*           obj;
    Actor100300Obj2C*           original;
    GsCOORDINATE2*              saved;
    Actor100300Work*            work;
    s32                         disabled;
    s16                         flags;
    s16                         scale;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    GsCOORDINATE2*              coord;

    original = arg1->field_2C;
    disabled = D_801153F4;
    USE_REG2(original, original);
    __asm__ volatile("" : "=r"(obj) : "0"(original));
    saved = obj->field_8;
    work  = arg1->field_8->field_1C;
    if (disabled == 0) {
        if (Game_Session->field_1 != 0) {
            flags        = ((work->field_678 & 1) == 0) << 7;
            obj->field_C = flags;
            if (work->field_678 & 2) {
                obj->field_C = flags | 4;
            }
        }
        scale = work->field_676;
        if (scale <= 0) {
            obj->field_C = 0x80;
            return;
        }
        head                       = *(MATRIX**)0x1F8003FC;
        scratch                    = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
        coord                      = arg1->field_2C->field_8;
        *(void**)0x1F8003FC        = scratch;
        scratch->scale.vx          = 0x1000;
        scratch->scale.vy          = scale;
        scratch->scale.vz          = 0x1000;
        coord->coord               = work->field_628;
        scratch->mat.ident.m00_m01 = 0x1000;
        scratch->mat.ident.m02_m10 = 0;
        scratch->mat.ident.m11_m12 = 0x1000;
        scratch->mat.ident.m20_m21 = 0;
        scratch->mat.ident.m22     = 0x1000;
        ScaleMatrix(&scratch->mat.mat, &scratch->scale);
        MulMatrix(&coord->coord, &scratch->mat.mat);
        coord->flg         = 0;
        saved->flg         = 0;
        *(u8**)0x1F8003FC += 0x30;
    }
}

void Actor00300_Fn03F40(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor00300_UpdateTransform(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn040A4);

void Actor00300_Fn04370(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    s32              id;
    s32              expired;
    s16              timer;

    coord   = arg1->field_2C->field_8;
    work    = arg1->field_1C;
    expired = 0;
    switch (D_801153F4) {
        case 1:
            Actor00300_Fn00078(coord, 0x200);
            return;
        case 0:
        default:
            coord->flg         = 0;
            coord->coord.t[0] += (coord->coord.m[0][2] * 0x19) >> 8;
            coord->coord.t[2] += (coord->coord.m[2][2] * 0x19) >> 8;
            Gp_UpdateCoord(coord);
            Actor00300_Fn00078(coord, 0x200);
            id = work->field_70.field_4;
            if (id != 0 && Gp_RoomParamTables[Game_Session->field_7 - 1]
                                             [Game_Session->field_6 - 1][func_800E1B24(id)]
                                                 ->field_1 == 0) {
                expired = 1;
            }
            Gp_ClearRec18Occupied(&work->field_70);
            Actor00300_Fn04528(arg1);
            timer          = work->field_88 - 1;
            work->field_88 = timer;
            if (timer <= 0 || (work->field_20 & 1) || expired != 0) {
                Gp_SpawnEff(D_8011573C, coord, 0, NULL);
                arg1->field_30 = 2;
                work->field_8A = 0;
            }
        case 2:
            return;
    }
}

void Actor00300_Fn04528(Actor100300* arg0)
{
    s32                    want;
    GsCOORDINATE2*         coord;
    Actor100300RotScratch* sc;
    s16                    cur;
    s32                    ang;
    s32                    current;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;

    coord      = arg0->field_2C->field_8;
    sc         = (Actor100300RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->vec.vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
    sc->vec.vy = 0;
    sc->vec.vz = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
    want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
    ang        = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    cur        = ang;
    diff       = want - ang;
    adiff      = diff >= 0 ? diff : -diff;
    turn       = diff;
    if (adiff < 0xD) {
        cur = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = cur;
        cur     = current + 0xC;
        if (turn <= 0) {
            cur = current - 0xC;
        }
    }
    sc->rot.vx = 0;
    sc->rot.vy = cur;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    *(u32*)0x1F8003FC += 0x18;
}

void Actor00300_Fn04664(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->field_4 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

void Actor00300_Fn04770(Actor100300* arg0)
{
    Actor100300StateFuncTable3 sp;

    sp = Actor00300_D00004;
    sp.funcs[arg0->field_30]((Actor100300Ctx*)arg0->field_20, arg0);
}

void Actor00300_Fn047CC(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Work* work;

    work = arg1->field_1C;
    switch (D_801153F4) {
        case 0:
            arg1->field_2C->field_C            = 0;
            work->field_43C->field_2C->field_C = 0;
            arg0->field_14                     = work->field_698 != 0;
            break;
        case 1:
            Actor00300_Fn04FB0(arg1);
            Actor00300_Fn05008(arg1);
            return;
        case 2:
            arg1->field_2C->field_C            = 0x80;
            work->field_43C->field_2C->field_C = 0x80;
            arg0->field_14                     = 1;
            return;
    }
    if (Game_Session->field_1 != 0) {
        Actor00300_Fn048D4(arg0, arg1);
        return;
    }
    Actor00300_Fn04958(arg0, arg1);
}

void Actor00300_Fn048D4(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Obj2C* obj;
    Actor100300Work*  work;
    s16               flags;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (Game_Session->field_1 != 0) {
        flags        = ((work->field_678 & 1) == 0) << 7;
        obj->field_C = flags;
        if (work->field_678 & 2) {
            obj->field_C = flags | 4;
        }
    }
    Actor00300_Fn04ED4(arg1);
    Actor00300_Fn04FB0(arg1);
    Actor00300_Fn05008(arg1);
}

void Actor00300_Fn04958(Actor100300Ctx* arg0, Actor100300* arg1)
{
    GsCOORDINATE2*   coord;
    Actor100300Work* work;

    work  = arg1->field_1C;
    coord = arg1->field_2C->field_8;
    if (work->field_648 != 0) {
        if (arg0->field_4C != 0) {
            Actor00300_Fn04A2C(arg1);
        }
        Actor00300_Fn00E54(arg1);
        Actor00300_Fn04C20(arg1);
        if (work->field_67C != 0) {
            Actor00300_Fn032BC(arg1);
        }
        Actor00300_Fn04E30(arg1);
        Actor00300_Fn04ED4(arg1);
        if (work->field_664 != 0) {
            Actor00300_Fn0340C(arg1);
        }
        Actor00300_Fn03A1C(arg1);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        Actor00300_Fn04FB0(arg1);
        Actor00300_Fn05008(arg1);
    }
}

void Actor00300_Fn04A2C(Actor100300* arg0)
{
    Actor100300Work* work;
    GpEnemy*         enemy;
    s16              damage;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
    }
    if ((enemy->field_4C & 2) && (work->field_684 != 5)) {
        work->field_684 = 6;
        work->field_686 = 0;
    }
    if (enemy->field_4C & 0xC) {
        damage          = Gp_TickObjFlag4((GpObj5C*)enemy);
        work->field_690 = damage;
        if (damage != 0) {
            func_800DA6E8(&enemy->node, (s32)damage, 0);
            enemy->field_40 = (u16)enemy->field_40 - (u16)work->field_690;
            work->field_684 = 5;
            work->field_686 = 0;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }
}
