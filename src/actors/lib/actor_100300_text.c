#include "common.h"

#include "actors/actor_100300.h"
#include "actors/actors_shared_80132074.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include <psyq/inline_c.h>

s32 SndEvt_EnqueueType6(s32 sound, s32 pan, s32 depth);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8        D_801153F4;
extern s32       D_80115720;
extern s32       D_80115728;
extern s32       D_80115744;
extern s32       D_8011573C;
extern s32       Gp_LcgState;
extern GpU16Pair Actor00300_D15FD8;

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
void Actor00300_Fn0505C(Actor100300* arg0, MATRIX* arg1, s16 arg2);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00078);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn005D0);

extern GpPairSrcE           Actor00300_D15FE8;
extern Actor00300AreaConfig Actor00300_D16020[];
extern s32                  Actor00300_D16278[][2];
extern TaskDesc             Actor00300_D162F0;
extern u32                  Actor00300_D16314;
extern u32                  Actor00300_D1633C;

void Actor00300_Fn00970(GpEnemy* enemy, Task* task)
{
    GpAreaKey           key;
    GpEnemy*            child;
    GpRec18*            rec4B8;
    GpRec18*            rec4F0;
    GpRec18*            rec558;
    GpRec18*            rec5D8;
    Actor00300MainWork* work;
    TmdObject*          model;
    s32                 areaIndex;
    Task*               childTask;
    s32                 slot;
    u32                 index;
    u32                 rawId;
    u8                  areaByte0;
    GpAreaKey*          sessionKey;
    GpCdRec10*          entry;
    Actor100300Obj2C*   obj;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      parts;

    obj   = task->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(sizeof(Actor00300MainWork), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_648 = 0;
    work->field_68C = 0U;
    for (areaIndex = 0; Actor00300_D16020[areaIndex].id != 0; areaIndex++) {
        if ((Game_Session->field_7 == Actor00300_D16020[areaIndex].area) &&
            (Game_Session->field_6 == Actor00300_D16020[areaIndex].room)) {
            work->field_648 =
                Actor00300_D16278[Actor00300_D16020[areaIndex].id]
                                 [((Actor00300SpawnArgs*)enemy->field_3C)->field_2];
            work->field_68C = (u16)Actor00300_D16020[areaIndex].value;
        }
    }
    work->field_698 = (s16)((Actor00300SpawnArgs*)enemy->field_3C)->field_1;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = work->field_460;
    obj->field_20   = work->field_440;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    slot               = 1;
    parts              = ((Actor100300Obj2C*)task->extra)->field_8;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_50    = &Actor00300_D15FE8;
    enemy->field_54    = (s32)(work->rec4F0);
    enemy->field_18    = parts + 3;
    enemy->field_40    = (s16)Actor00300_D15FE8.field_4;
    work->field_5F0    = (void*)(((Actor100300Obj2C*)task->extra)->field_8 + 3);
    work->field_5F4    = 0x300;
    work->field_5F6    = 2;
    func_800B3F84((GpAnimCtx*)work, &Actor00300_D1633C, (GpAnimObj*)obj,
                  work->field_30C, work->field_14);
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, slot, 1);
        slot += 1;
    } while (slot < 0x13);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->matrix608 = coord->coord;
    work->field_688 = 0xA;
    work->field_666 = 0x28;
    child           = Gp_SpawnEnemyFromTable(&Actor00300_D162F0, 1, 0, enemy);
    rawId           = (u16)enemy->field_8;
    model           = child->task->extra;
    sessionKey      = (GpAreaKey*)&Game_Session->field_4;
    key.field_3     = sessionKey->field_3;
    key.field_2     = sessionKey->field_2;
    key.field_1     = sessionKey->field_1;
    areaByte0       = Game_Session->field_4;
    index           = rawId >> 12;
    key.field_0     = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    entry =
        (GpCdRec10*)((index * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    childTask              = child->task;
    work->pose4A0.field_4  = 0x2328;
    work->pose4A0.field_10 = 0xFA0;
    rec4B8                 = &work->rec4B8;
    work->pose4A0.field_0  = 0;
    work->pose4A0.field_2  = 0;
    work->pose4A0.field_8  = 0;
    work->pose4A0.field_A  = 0;
    work->pose4A0.field_C  = 0;
    work->pose4A0.field_12 = 0x3E8;
    work->pose4A0.field_14 = rec4B8;
    work->field_43C        = childTask;
    work->obj480.field_8 =
        (void*)(((Actor100300Obj2C*)task->extra)->field_8 + 2);
    work->obj480.field_C  = &work->pose4A0;
    work->obj480.field_10 = 0;
    work->obj480.field_12 = 0;
    work->obj480.field_14 = 0;
    work->obj480.field_18 = 0;
    work->obj480.field_1C = 0;
    work->obj480.flags    = 3U;
    Gp_LinkObj(3, &work->obj480);
    Gp_InitRec18Table(rec4B8, 1, 0);
    rec4F0             = work->rec4F0;
    work->obj480.flags = (u16)(work->obj480.flags | 0xCC00);
    work->obj4D0.field_8 =
        (void*)(((Actor100300Obj2C*)task->extra)->field_8 + 3);
    work->obj4D0.field_C  = rec4F0;
    work->obj4D0.field_10 = 0;
    work->obj4D0.field_12 = 0;
    work->obj4D0.field_14 = 0;
    work->obj4D0.field_18 = 0x30003;
    work->obj4D0.field_1C = 0x15E;
    work->obj4D0.flags    = 1U;
    Gp_LinkObj(2, &work->obj4D0);
    Gp_InitRec18Table(rec4F0, 3, 0);
    work->obj4D0.flags    = (u16)(work->obj4D0.flags | 0x8000);
    work->obj538.field_8  = (void*)((Actor100300Obj2C*)task->extra)->field_8;
    rec558                = work->rec558;
    work->obj538.field_18 = 0x30003;
    work->obj538.field_C  = rec558;
    work->obj538.field_10 = 0;
    work->obj538.field_12 = -0x1F4;
    work->obj538.field_14 = 0;
    work->obj538.field_1C = 0x1F4;
    work->obj538.flags    = 1U;
    Gp_LinkObj(2, &work->obj538);
    Gp_InitRec18Table(rec558, 4, 0);
    work->obj538.flags    = (u16)(work->obj538.flags | 0x4200);
    work->obj5B8.field_8  = ((Actor100300Obj2C*)child->task->extra)->field_8;
    rec5D8                = &work->rec5D8;
    work->obj5B8.field_C  = rec5D8;
    work->obj5B8.field_10 = -0x1F4;
    work->obj5B8.field_12 = 0x1F4;
    work->obj5B8.field_14 = 0;
    work->obj5B8.field_18 = Gp_PackPair(&Actor00300_D15FD8, 0);
    work->obj5B8.field_1C = 0x2BC;
    work->obj5B8.flags    = 1U;
    Gp_LinkObj(3, &work->obj5B8);
    Gp_InitRec18Table(rec5D8, 1, 0);
    D_80062735         = 0xA;
    work->obj5B8.flags = (u16)(work->obj5B8.flags & 0x7FFF);
    task->field_24     = &Actor00300_D16314;
    task->state        = 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn00E54);

extern s8 D_80115418;

void Actor00300_Fn01678(Actor100300* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    s16              timer;
    s16              nextPoint;
    s32              state;
    s16              nextState;
    s32              dx;
    s32              dz;
    u16              angle0;
    u16              angle1;
    VECTOR*          vec;
    VECTOR*          scratchEnd;

    scratchEnd          = *(void**)0x1F8003FC;
    vec                 = scratchEnd - 1;
    *(void**)0x1F8003FC = vec;
    work                = arg0->field_1C;
    state               = work->field_686;
    coord               = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_67C = 0;
            work->field_67A = 0;
            work->field_66E = 1;
            timer           = (u16)work->field_688 - 1;
            work->field_688 = timer;
            if ((timer << 0x10) <= 0) {
                scratchEnd[-1].vx = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vx - coord->coord.t[0]);
                vec->vy           = 0;
                vec->vz           = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vz - coord->coord.t[2]);
                angle0            = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
                nextState         = 1;
                work->field_680   = angle0;
                if (work->field_67E == angle0) {
                    nextState = 2;
                }
                work->field_686 = nextState;
                work->field_688 = 0;
            }
            break;
        case 1:
            work->field_67C   = 0x28;
            work->field_66E   = 3;
            work->field_67A   = 0;
            scratchEnd[-1].vx = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vx - coord->coord.t[0]);
            vec->vy           = 0;
            vec->vz           = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vz - coord->coord.t[2]);
            angle1            = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
            work->field_680   = angle1;
            if (work->field_67E == angle1) {
                work->field_686 = 2;
            }
            break;
        case 2:
            work->field_67C   = 0x28;
            work->field_67A   = 0x19;
            work->field_66E   = state;
            scratchEnd[-1].vx = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vx - coord->coord.t[0]);
            vec->vy           = 0;
            vec->vz           = (s32)(((SVECTOR*)work->field_648)[work->field_68E].vz - coord->coord.t[2]);
            work->field_680   = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
            dx                = scratchEnd[-1].vx;
            dz                = vec->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) <= work->field_67A) {
                coord->coord.t[0] = (s32)((SVECTOR*)work->field_648)[work->field_68E].vx;
                coord->coord.t[2] = (s32)((SVECTOR*)work->field_648)[work->field_68E].vz;
                work->field_67A   = 0;
                nextPoint         = (u16)work->field_68E + 1;
                work->field_68E   = nextPoint;
                if (nextPoint >= work->field_68C) {
                    work->field_68E = 0;
                }
                work->field_686 = 1;
            }
            break;
    }
    if ((work->field_6A0 != 0) || (D_80115418 != 0) || (work->field_690 != 0)) {
        work->field_684 = 1;
        work->field_686 = 0;
        work->field_6A0 = 0x1C2;
        Gp_ArmStateF0(1);
        D_80115418 = 0;
    }
    *(void**)0x1F8003FC = (void*)(*(void**)0x1F8003FC + 0x10);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn019C0);

void Actor00300_Fn01D60(Actor100300* arg0)
{
    Actor100300Work*       work;
    GsCOORDINATE2*         coord;
    Actor100300RotScratch* sc;
    s32                    random;

    sc    = (Actor100300RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (arg0->field_20->field_40 * 100 / (s32)Actor00300_D15FEC < 50 &&
        (s16)work->field_666 >= 20) {
        work->field_666 -= 20;
        work->field_66E  = 4;
        work->field_684  = 4;
        work->field_686  = 0;
    } else if ((s16)work->field_666 >= 5) {
        work->field_666 -= 5;
        work->field_66A  = Actor00300_D16000[((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 15];
        if (work->field_66A == 0) {
            work->field_684 = 1;
            work->field_686 = 0;
            work->field_688 = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 31);
        } else {
            work->field_684 = 2;
            work->field_686 = 0;
            work->field_688 = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 31) + 60;
        }
    } else {
        sc->vec.vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
        sc->vec.vy = 0;
        sc->vec.vz = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
        if (SquareRoot0(sc->vec.vx * sc->vec.vx + sc->vec.vz * sc->vec.vz) < 3000) {
            work->field_684 = 3;
            work->field_686 = 0;
            random          = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = random;
            work->field_688 = (((u32)random >> 16) & 31) + 60;
        } else {
            work->field_684 = 7;
            work->field_686 = 0;
        }
    }
    *(u32*)0x1F8003FC += 0x18;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100300_text", Actor00300_Fn01F9C);

void Actor00300_Fn02620(Actor100300* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    s16              delta;
    s16              angle;
    s16              timer;
    s32              magnitude;
    s32              sound;
    s32              pan;
    s32              random;

    scratchEnd            = *(VECTOR**)0x1F8003FC;
    vec                   = scratchEnd - 1;
    *(VECTOR**)0x1F8003FC = vec;
    work                  = arg0->field_1C;
    coord                 = arg0->field_2C->field_8;
    switch (work->field_686) {
        case 0:
            work->field_67C   = 0x3C;
            work->field_67A   = 0;
            work->field_66E   = 3;
            scratchEnd[-1].vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            vec->vy           = 0;
            vec->vz           = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
            work->field_680   = angle;
            delta             = (u16)angle - (u16)work->field_67E;
            magnitude         = abs(delta);
            angle             = magnitude >= 0x800 ? (delta > 0 ? 0x1000 - delta : delta + 0x1000) : magnitude;
            if (angle < 0x80) {
                work->field_686 = 1;
                work->field_67C = 0;
                work->field_66E = 7;
            } else {
                timer           = (u16)work->field_688 - 1;
                work->field_688 = timer;
                if (timer <= 0) {
                    work->field_684 = 1;
                    work->field_686 = 0;
                    random          = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState     = random;
                    work->field_688 = ((u32)random >> 16) & 15;
                }
            }
            break;
        case 1:
            if ((u32)((u16)work->field_672 - 0x10) < 0x14U) {
                work->field_676 += 0x100;
            } else if ((s16)work->field_672 >= 0x33) {
                work->field_676 -= 0x100;
            }
            if ((s16)work->field_672 == 0x20) {
                work->obj5B8.flags |= 0x8000;
                sound               = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x4003000A;
                pan                 = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((s16)work->field_672 >= 0x37) {
                work->field_676     = 0;
                work->field_684     = 1;
                work->field_686     = 0;
                random              = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState         = random;
                work->field_688     = ((u32)random >> 16) & 31;
                work->obj5B8.flags &= 0x7FFF;
            }
            break;
    }
    *(u32*)0x1F8003FC += 0x10;
}

void Actor00300_Fn028D0(Actor100300* arg0)
{
    SVECTOR           sp10;
    SVECTOR           sp18;
    SVECTOR           sp20;
    Actor100300Work*  work;
    GpEffWork*        effect;
    GpEffWork*        burst;
    GpEnemy*          enemy;
    Actor100300Obj2C* obj;
    GpEnemy*          currentEnemy;
    GsCOORDINATE2*    coord;
    s16               timer;
    s16               state;
    s32               random0;
    s32               angle0;
    s32               random1;
    s32               angle1;
    s32               sound;
    s32               random2;
    s32               pan0;
    s32               pan1;
    u32               effectRandom0;
    u32               effectRandom1;

    obj   = arg0->field_2C;
    work  = arg0->field_1C;
    enemy = arg0->field_20;
    state = work->field_686;
    coord = obj->field_8;
    switch (state) {
        case 0:
            if (Gp_State1C->field_4 == 0) {
                effectRandom0 = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState   = (s32)effectRandom0;
                if (!((effectRandom0 >> 0x10) & 3)) {
                    random0     = (effectRandom0 * 5) + 0x71357911;
                    Gp_LcgState = random0;
                    angle0      = ((u32)random0 >> 0x10) & 0xF80;
                    memset(&sp20, 0, sizeof(sp20));
                    sp20.vx = (s16)((u32)(rcos(angle0) * 5) >> 5);
                    sp20.vz = (s16)((u32)(rsin(angle0) * 5) >> 5);
                    sp18    = sp20;
                    Gp_SpawnEff(D_80115728, coord, 0x20100200, &sp18);
                }
            }
            work->field_67C = 0;
            work->field_67A = 0;
            if ((s16)work->field_672 >= 0x33) {
                work->field_686 = 1;
                work->field_66E = 5;
                sp10.vy         = -0x5DC;
                sp10.vx         = 0;
                sp10.vz         = 0x320;
                effect          = Gp_SpawnEff(D_80115744, coord, 0x10014, &sp10);
                work->field_654 = effect;
                if (effect != NULL) {
                    Task_Reparent((Task*)arg0, effect->field_0);
                    work->field_69C = 0x13;
                }
                work->field_658 = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40030009;
                pan0            = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(work->field_658, pan0, (s8)Gp_GetObjDepth((GpObj38*)coord));
                return;
            }
            return;
        case 1:
            if (work->field_69C > 0) {
                timer           = (u16)work->field_69C - 1;
                work->field_69C = timer;
                if (timer <= 0) {
                    work->field_654 = NULL;
                }
            }
            if (Gp_State1C->field_4 == 0) {
                effectRandom1 = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState   = (s32)effectRandom1;
                if (!((effectRandom1 >> 0x10) & 3)) {
                    random1     = (effectRandom1 * 5) + 0x71357911;
                    Gp_LcgState = random1;
                    angle1      = ((u32)random1 >> 0x10) & 0xF80;
                    memset(&sp20, 0, sizeof(sp20));
                    sp20.vx = (s16)((u32)(rcos(angle1) * 5) >> 5);
                    sp20.vz = (s16)((u32)(rsin(angle1) * 5) >> 5);
                    sp18    = sp20;
                    Gp_SpawnEff(D_80115728, coord, 0x20100200, &sp18);
                }
            }
            if ((s16)work->field_672 >= 0x13) {
                work->field_686        = 2;
                work->field_654        = NULL;
                work->field_66E        = 6;
                currentEnemy           = arg0->field_20;
                currentEnemy->field_40 = (u16)currentEnemy->field_40 + 0x64;
                func_800DA6E8(&enemy->node, -0x64, 0);
                burst = Gp_SpawnEff(D_80115720, coord, 0, NULL);
                if (burst != NULL) {
                    Task_Reparent((Task*)arg0, burst->field_0);
                }
                sound = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4003000B;
                pan1  = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, pan1, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 2:
            if ((s16)work->field_672 >= 0xF) {
                work->field_684 = 1;
                work->field_686 = 0;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random2;
                work->field_688 = ((u32)random2 >> 0x10) & 0x1F;
            }
            break;
    }
}

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

void Actor00300_Fn0340C(Actor100300* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                     = (MATRIX*)(((Actor100300ScratchStack*)0x1F8003FC)->sp - 0x20);
    ((Actor100300ScratchStack*)0x1F8003FC)->sp = (u32)matrix;
    active                                     = 0;
    work                                       = arg0->field_1C;
    coord                                      = arg0->field_2C->field_8;
    RotMatrix(&work->field_65C, matrix);
    USE_REG(matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    __asm__ volatile("nop; nop; .word 0x4A49E012");
    gte_stclmv(&coord[3].coord);
    gte_ldclmv((char*)matrix + 2);
    __asm__ volatile("nop; nop; .word 0x4A49E012");
    gte_stclmv((char*)&coord[3].coord + 2);
    gte_ldclmv((char*)matrix + 4);
    __asm__ volatile("nop; nop; .word 0x4A49E012");
    gte_stclmv((char*)&coord[3].coord + 4);
    angleX = work->field_65C.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_65C.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_65C.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_65C.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_65C.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_65C.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_664 = 0;
    }
    (*(u32*)0x1F8003FC) += 0x20;
}

extern u8    Actor00300_D0AA18[];
extern u8    Actor00300_D0AECC[];
extern u8    Actor00300_D0B640[];
extern u8    Actor00300_D0BE44[];
extern u8    Actor00300_D0C2C4[];
extern void* D_80067704[1];

void Actor00300_Fn03618(Actor100300* arg0)
{
    GpAreaKey  key;
    u8         areaByte0;
    u32        raw1, index1;
    GpEffWork* effect1;
    TmdObject* model1;
    GpCdRec10* entry1;
    GpAreaKey* sessionKey1;
    u32        raw2, index2;
    GpEffWork* effect2;
    TmdObject* model2;
    GpCdRec10* entry2;
    GpAreaKey* sessionKey2;
    u32        raw3, index3;
    GpEffWork* effect3;
    TmdObject* model3;
    GpCdRec10* entry3;
    GpAreaKey* sessionKey3;
    u32        raw4, index4;
    GpEffWork* effect4;
    TmdObject* model4;
    GpCdRec10* entry4;
    GpAreaKey* sessionKey4;
    u32        raw5, index5;
    GpEffWork* effect5;
    TmdObject* model5;
    GpCdRec10* entry5;
    GpAreaKey* sessionKey5;

    D_80067704[0] = Actor00300_D0AA18;
    effect1       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x200, NULL);
    if (effect1 != NULL) {
        sessionKey1 = (GpAreaKey*)&Game_Session->field_4;
        raw1        = (u16)arg0->field_20->field_8;
        model1      = (TmdObject*)effect1->field_0->extra;
        key.field_3 = sessionKey1->field_3;
        key.field_2 = sessionKey1->field_2;
        key.field_1 = sessionKey1->field_1;
        areaByte0   = Game_Session->field_4;
        index1      = raw1 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1           = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->field_24 = entry1->field_D;
        model1->field_25 = entry1->field_E;
        if (model1->field_18 != NULL) {
            Tmd_ProcessStream(model1);
            Tmd_ProcessStream(model1);
        }
    }

    D_80067704[0] = Actor00300_D0AECC;
    effect2       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x200, NULL);
    if (effect2 != NULL) {
        sessionKey2 = (GpAreaKey*)&Game_Session->field_4;
        raw2        = (u16)arg0->field_20->field_8;
        model2      = (TmdObject*)effect2->field_0->extra;
        key.field_3 = sessionKey2->field_3;
        key.field_2 = sessionKey2->field_2;
        key.field_1 = sessionKey2->field_1;
        areaByte0   = Game_Session->field_4;
        index2      = raw2 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->field_24 = entry2->field_D;
        model2->field_25 = entry2->field_E;
        if (model2->field_18 != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    D_80067704[0] = Actor00300_D0B640;
    effect3       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x200, NULL);
    if (effect3 != NULL) {
        sessionKey3 = (GpAreaKey*)&Game_Session->field_4;
        raw3        = (u16)arg0->field_20->field_8;
        model3      = (TmdObject*)effect3->field_0->extra;
        key.field_3 = sessionKey3->field_3;
        key.field_2 = sessionKey3->field_2;
        key.field_1 = sessionKey3->field_1;
        areaByte0   = Game_Session->field_4;
        index3      = raw3 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3           = (GpCdRec10*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->field_24 = entry3->field_D;
        model3->field_25 = entry3->field_E;
        if (model3->field_18 != NULL) {
            Tmd_ProcessStream(model3);
            Tmd_ProcessStream(model3);
        }
    }

    D_80067704[0] = Actor00300_D0BE44;
    effect4       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x200, NULL);
    if (effect4 != NULL) {
        sessionKey4 = (GpAreaKey*)&Game_Session->field_4;
        raw4        = (u16)arg0->field_20->field_8;
        model4      = (TmdObject*)effect4->field_0->extra;
        key.field_3 = sessionKey4->field_3;
        key.field_2 = sessionKey4->field_2;
        key.field_1 = sessionKey4->field_1;
        areaByte0   = Game_Session->field_4;
        index4      = raw4 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry4           = (GpCdRec10*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model4->field_24 = entry4->field_D;
        model4->field_25 = entry4->field_E;
        if (model4->field_18 != NULL) {
            Tmd_ProcessStream(model4);
            Tmd_ProcessStream(model4);
        }
    }

    D_80067704[0] = Actor00300_D0C2C4;
    effect5       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x200, NULL);
    if (effect5 != NULL) {
        sessionKey5 = (GpAreaKey*)&Game_Session->field_4;
        raw5        = (u16)arg0->field_20->field_8;
        model5      = (TmdObject*)effect5->field_0->extra;
        key.field_3 = sessionKey5->field_3;
        key.field_2 = sessionKey5->field_2;
        key.field_1 = sessionKey5->field_1;
        areaByte0   = Game_Session->field_4;
        index5      = raw5 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry5           = (GpCdRec10*)((index5 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model5->field_24 = entry5->field_D;
        model5->field_25 = entry5->field_E;
        if (model5->field_18 != NULL) {
            Tmd_ProcessStream(model5);
            Tmd_ProcessStream(model5);
        }
    }
}

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

void Actor00300_Fn03B70(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300Work*  work;
    Actor100300Obj2C* obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    c;
    VECTOR            vec;
    s32               mode;
    s32               sound;
    s32               pan;
    s16               phase;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    mode  = D_801153F4;
    coord = obj->field_8;
    if (mode == 1)
        goto case1;
    if (mode < 2)
        goto common;
    if (mode == 2)
        goto case2;
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
case2:
    obj->field_C                       = 0x80;
    work->field_43C->field_2C->field_C = 0x80;
    return;
common:
    switch (work->field_686) {
        case 0:
            work->field_674 = 0x1000;
            work->field_608 = coord->coord;
            arg0->field_54  = NULL;
            Gp_UnlinkNode(&arg0->next);
            Gp_UnlinkObj(&work->obj480);
            Gp_UnlinkObj(&work->obj538);
            Gp_UnlinkObj(&work->obj4D0);
            Gp_UnlinkObj(&work->obj5B8);
            Gp_SetLightMode(arg0, 1);
            Gp_ReleaseStateF0Add(arg1, 3);
            work->field_688 = 0;
            work->field_686 = 1;
            if (work->field_682 != 0) {
                obj->field_C    = 0x80;
                work->field_686 = 3;
            }
            c      = arg1->field_2C->field_8;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
            if (work->field_654 != NULL) {
                work->field_654->field_0->state = 3;
                work->field_654                 = NULL;
                work->field_69C                 = 0;
                SndEvt_EnqueueType7(work->field_658, 1);
            }
            sound = (((u16)arg1->field_20->field_8 >> 12) << 8) | 0x40030008;
            pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            return;
        case 1:
            if (work->field_674 >= 0x201)
                work->field_674 -= 0x50;
            Actor00300_Fn0505C(arg1, &work->field_608, work->field_674);
            phase           = work->field_688 + 1;
            work->field_688 = phase;
            if (phase == 10)
                obj->field_C |= 2;
            if (work->field_688 == 15)
                Gp_SpawnEff(0x600A5, &arg1->field_2C->field_8[3], 3, NULL);
            if (work->field_688 >= 0x3C)
                work->field_686 = 2;
            c      = arg1->field_2C->field_8;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
            return;
        case 2:
            Gp_DestroyEnemy(arg0, arg1);
            return;
        case 3:
            if (work->field_682 != 0) {
                if (work->field_682 >= 2) {
                    work->field_682 = 0;
                    Tmd_FreeBuffers((TmdObject*)obj);
                    obj->field_C |= 4;
                    Actor00300_Fn03618(arg1);
                } else
                    work->field_682++;
            }
            phase           = work->field_688 + 1;
            work->field_688 = phase;
            if (phase >= 0x3C)
                work->field_686 = 2;
            return;
    }
}

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

void Actor00300_Fn040A4(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work*       parentWork;
    Task*                  parent;
    Actor00300InitWork*    work;
    Actor00300InitScratch* scratch;
    void*                  head;
    SVECTOR*               offset;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoord;
    GsCOORDINATE2*         objCoord;
    GsCOORDINATE2*         objCoord2;

    head                = *(void**)0x1F8003FC;
    scratch             = (Actor00300InitScratch*)((u8*)head - 0x18);
    *(void**)0x1F8003FC = scratch;
    offset              = &scratch->offset;
    parent              = arg1->parent;
    coord               = ((Actor100300Obj2C*)arg1->extra)->field_8;
    parentCoord         = ((Actor100300Obj2C*)parent->extra)->field_8;
    parentWork          = (Actor100300Work*)parent->idMap;
    work                = Mem_Calloc(0x8C, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap        = (TaskIdMap*)work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x5DC;
    scratch->offset.vz = 0x320;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stlvnl(&scratch->result);
    coord->sub          = &Gfx_ViewCoord;
    coord->coord        = parentCoord->coord;
    coord->coord.t[0]   = parentCoord->coord.t[0] + scratch->result.vx;
    coord->coord.t[1]   = parentCoord->coord.t[1] + scratch->result.vy;
    coord->coord.t[2]   = parentCoord->coord.t[2] + scratch->result.vz;
    coord->flg          = 0;
    objCoord            = ((Actor100300Obj2C*)arg1->extra)->field_8;
    work->obj0.field_C  = &work->rec20;
    work->obj0.field_10 = 0;
    work->obj0.field_12 = 0;
    work->obj0.field_14 = 0;
    work->obj0.field_8  = objCoord;
    work->obj0.field_18 = Gp_PackPair(&Actor00300_D15FD8, parentWork->field_66A);
    work->obj0.field_1C = 0x1C2;
    work->obj0.flags    = 1;
    Gp_LinkObj(3, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->pose.field_C   = -0x1A4;
    work->pose.field_10  = 1;
    work->pose.field_12  = 1;
    work->pose.field_0   = 0;
    work->pose.field_2   = 0;
    work->pose.field_4   = 0;
    work->pose.field_8   = 0;
    work->pose.field_A   = 0;
    work->pose.field_14  = &work->rec70;
    work->obj0.flags    |= 0x8000;
    objCoord2            = ((Actor100300Obj2C*)arg1->extra)->field_8;
    work->obj38.field_C  = (GpRec18*)&work->pose;
    work->obj38.field_10 = 0;
    work->obj38.field_12 = 0;
    work->obj38.field_14 = 0;
    work->obj38.field_18 = 0;
    work->obj38.field_1C = 0;
    work->obj38.flags    = 3;
    work->obj38.field_8  = objCoord2;
    Gp_LinkObj(3, &work->obj38);
    Gp_InitRec18Table(&work->rec70, 1, 0);
    work->timer        = 0x1E;
    work->obj38.flags |= 0x4400;
    Task_DetachFromParent(arg1);
    arg1->state        = 1;
    *(u8**)0x1F8003FC += 0x18;
}

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
